#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeRollingProjectile.h"
#include "../Fusion/FusionRollingProjectileRules.h"
#include "../Fusion/FusionCherryShooterRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
constexpr int kColumn=3,kRow=2,kHealth=5000;
constexpr std::array<int,6> kTargetX={400,400,400,650,400,750};
constexpr std::array<int,6> kTargetRows={kRow,kRow,kRow,kRow,kRow+1,kRow};
struct Shot { int tick=0,age=0;float x=0; };
struct State
{
    int stage=0,kind=1004,waitingOwn=0,previousCounter=0,previousTick=0,previousHealth=4000,previousHealthTick=0;
    int primaryLastTick=0,primaryLastAge=0,overlapTicks=0,overlapLastTick=-1,crackLoggedMask=0;float primaryLastX=0;
    uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,heldSamples=0,purchaseSamples=0;
    uint32_t primaryGeneration=0;
    uint16_t parent=0,finalPlant=0,parentPacket=0,nutPacket=0,primary=0,eater=0;
    bool configured=false,clicked=false,placed=false,purchaseObserved=false,ambient=false,setup=false,active=false;
    bool triple=false,boundary=false,clientProjectile=false,clientTriple=false,eaterParked=false,controls=false;
    std::array<uint16_t,6> targets{};std::array<int,3> stageShots{};
    std::array<std::set<uint32_t>,3> hittingGenerations;
    std::vector<int> fireTicks;
    std::map<uint16_t,Shot> projectiles;
    std::map<std::string,int> firstDraw;
    std::set<std::string> frames;
    std::set<int> physicsSamples;
} s;
const char* Role(LawnApp* app){return app->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* app){return app->NetIsHost()?app->mCoopSmokeSnapshotsSent:app->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* app,bool value,const char* error){if(!value)app->CoopSmokeFail(error);return value;}
void Advance(LawnApp* app,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.purchaseObserved=false;
    LawnNet::NetTrace("SMOKE_FUSION_NUT_STAGE role=%s kind=%d stage=%d",Role(app),s.kind,stage);
}
bool Has(LawnApp* app,int owner,const std::string& text)
{for(const auto& l:app->mNetManager->mChatLines)if(l.mSlot==owner&&l.mText==text)return true;return false;}
bool Send(LawnApp* app,const std::string& text)
{
    const int owner=app->mNetManager->GetMySlot();if(Has(app,owner,text))return true;
    if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;
    s.lastChat=GetTickCount();app->mNetManager->SendChat(text);return Has(app,owner,text);
}
bool Barrier(LawnApp* app,const std::string& text){Send(app,text);return Has(app,0,text)&&Has(app,1,text);}
bool Capture(LawnApp* app,const char* name)
{
    if(s.frames.contains(name))return true;
    if(!s.firstDraw.contains(name)){s.firstDraw[name]=app->mDrawCount;return false;}
    if(app->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(app,app->CoopSmokeCaptureFrame(name),"Nut actual renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_NUT_RENDER role=%s frame=%s freshDraws=%d",Role(app),name,app->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* app,float x,float y)
{
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.windowID=motion.motion.windowID;down.button.button=SDL_BUTTON_LEFT;
    down.button.clicks=1;down.button.down=true;down.button.x=x;down.button.y=y;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
    return Require(app,SDL_PushEvent(&motion)&&SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Nut scoped SDL queue failed");
}
bool BoardClick(LawnApp* app,float x,float y)
{return Click(app,x+app->mBoard->mX+app->mDDInterface->mWideScreenOffsetX,y+app->mBoard->mY+app->mDDInterface->mWideScreenOffsetY);}
bool Cell(LawnApp* app)
{return BoardClick(app,float(app->mBoard->GetCellCenterX(kColumn,kRow)),float(app->mBoard->GridToPixelY(kColumn,kRow)+50));}
bool Button(LawnApp* app,GameButton* button)
{
    if(!button||button->mDisabled||button->mBtnNoDraw)return false;
    const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
    if(!button->IsMouseOver())
    {
        SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
        Require(app,SDL_PushEvent(&motion),"Nut Ready hover failed");return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_NUT_READY role=%s nativeSDL=1 hoverObserved=1",Role(app));return Click(app,x,y);
}
SeedPacket* Packet(Board* board,int owner,int kind,bool extra)
{
    for(int i=0;i<board->mSeedBank->mNumPackets;++i)
    {auto* p=&board->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==owner&&d->kind==kind&&d->isExtra==extra)return p;}
    return nullptr;
}
bool Pick(LawnApp* app,SeedPacket* p)
{
    if(!Require(app,p&&p->mActive&&!p->mRefreshing,"Nut real card is not ready"))return false;
    const auto* bank=app->mBoard->mSeedBank;
    return BoardClick(app,float(bank->mX+p->mX+p->mOffsetX+p->mWidth/2),float(bank->mY+p->mY+p->mHeight/2));
}
bool Held(Board* board,uint16_t id)
{
    const auto* c=board->mCursorObject;if(!c||c->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||c->mSeedBankIndex<0||c->mSeedBankIndex>=board->mSeedBank->mNumPackets)return false;
    const auto& p=board->mSeedBank->mSeedPackets[c->mSeedBankIndex];return p.mNetId==id&&c->mType==p.mPacketType;
}
Plant* At(Board* board)
{Plant* p=nullptr;while(board->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==kColumn&&p->mRow==kRow)return p;return nullptr;}
Zombie* Target(Board* board,uint16_t id)
{Zombie* z=nullptr;while(board->mZombies.IterateNext(z))if(z->mNetId==id)return z;return nullptr;}
bool NoCoins(LawnApp* app)
{Coin* c=nullptr;while(app->mBoard->mCoins.IterateNext(c))if(!Require(app,c->mDead,"Nut projectile unexpectedly created sun/Coin"))return false;return true;}
bool Pool(LawnApp* app,const NativeContext& context)
{
    const auto& state=context.selection.State();const size_t count=(std::min)(context.eligible.size(),kNativeFusionBankCapacity);
    if(!Require(app,state.activeMask==3&&state.targetCount==count&&state.quotas[0]+state.quotas[1]==count&&state.quotas[0]>=2&&state.quotas[1]>=1,"Nut real chooser quota/capacity mismatch"))return false;
    std::set<std::pair<FusionPlantId,bool>> identities;
    for(const auto& e:context.eligible)
    {
        const auto* d=FusionFindPlant(e.kind);FusionBankEligibleCard sourceCard;
        if(!Require(app,d&&d->hasStats&&GetNativeSeedForPlant(e.kind)!=SEED_NONE&&identities.emplace(e.kind,e.isExtra).second&&
            TryMakeFusionEligibleCard(e.kind,e.isExtra,sourceCard)&&e==sourceCard&&e.rechargeSeconds==d->rechargeSeconds,"Nut eligible identity/price/CD changed"))return false;
    }
    for(FusionPlantId k:{FusionPlantId(0),FusionPlantId(1),FusionPlantId(2),FusionPlantId(3),FusionPlantId(4),FusionPlantId(5),FusionPlantId(6),FusionPlantId(7)})
        if(!Require(app,identities.contains({k,false})&&identities.contains({k,true}),"Nut production pool filtered an existing base identity"))return false;
    return true;
}
Projectile* ProjectileById(Board* b,uint16_t id)
{Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))if(!p->mDead&&p->mNetId==id)return p;return nullptr;}
bool Business(LawnApp* app)
{
    auto* b=app->mBoard;auto* p=At(b);const auto* d=GetNativePlantData(p);
    auto* a=Packet(b,0,0,false);auto* n=Packet(b,1,3,false);const auto* card=GetNativePacketData(n);
    if(!Require(app,p&&d&&a&&n&&card&&p->mNetId==s.finalPlant&&d->kind==1004&&d->firstParent==0&&d->secondParent==3&&
        d->owner==1&&d->packetId==s.nutPacket&&!d->randomOrigin&&p->mSeedType==SEED_PEASHOOTER&&p->mPlantHealth>0&&p->mPlantHealth<=4000&&
        p->mPlantMaxHealth==4000&&d->productionTicks==0&&d->pendingProductionTicks==0&&FusionNativeAnimationSpeedValid(1004,d->animationSpeed)&&b->mSunMoney==350&&
        a->mNetId==s.parentPacket&&n->mNetId==s.nutPacket&&a->mRefreshTime==750&&n->mRefreshTime==3000&&
        card->price==50&&!card->isExtra&&card->rechargeSeconds==30&&(!app->NetIsHost()||(a->mTimesUsed==1&&n->mTimesUsed==1)),
        "Nut actual ancestry/HP4000/normal-card price/CD/owner conservation failed"))return false;
    Plant* other=nullptr;while(b->mPlants.IterateNext(other))if(!other->mDead&&other->mIsOnBoard)
        if(!Require(app,other==p,"Nut fusion left an extra live plant"))return false;
    return NoCoins(app);
}
bool Setup(LawnApp* app)
{
    auto* b=app->mBoard;
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=b->AddZombieInRow(i==5?ZOMBIE_BALLOON:ZOMBIE_NORMAL,kTargetRows[i],0);
        if(!Require(app,z&&z->mNetId,"Nut native target allocation failed"))return false;
        auto* body=app->ReanimationTryToGet(z->mBodyReanimID);const float rate=body?body->mAnimRate:0;
        if(!Require(app,body&&std::isfinite(rate)&&rate>0,"Nut original native animation rate missing"))return false;
        // Native hypnotized zombies despawn beyond the opposite edge850.
        // Keep this rejected control at its safe final position while the
        // ordinary acquisition targets wait offscreen for both peers' IDs.
        z->mPosX=i==3?float(kTargetX[i]):1800.0f+100*float(i);z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(kTargetRows[i]);z->mY=int(z->mPosY);
        z->mBodyHealth=z->mBodyMaxHealth=kHealth;z->mHelmType=HELMTYPE_NONE;z->mShieldType=SHIELDTYPE_NONE;
        z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;z->mMindControlled=i==3;z->mVelX=0;
        z->SetAnimRate(rate);z->ApplyAnimRate(0);s.targets[i]=z->mNetId;
        if(!Require(app,body->mAnimRate==0&&z->mOriginalAnimRate==rate&&(i!=5||z->IsFlying()),"Nut real walking/flying control did not initialize coherently"))return false;
        LawnNet::NetTrace("SMOKE_FUSION_NUT_SETUP index=%zu id=%u row=%d type=%d HP=5000 mind=%d flying=%d originalRate=%.6f staged=1 fixtureControl=1 boundedNativeTarget=%d nativeBalloonControl=%d",
            i,z->mNetId,kTargetRows[i],int(z->mZombieType),z->mMindControlled?1:0,z->IsFlying()?1:0,rate,i==5?0:1,i==5?1:0);
    }
    s.setup=true;return true;
}
std::string Ids(){std::ostringstream out;out<<"FN67_IDS";for(auto id:s.targets)out<<' '<<id;return out.str();}
bool ReadIds(LawnApp* app)
{
    if(app->NetIsHost())return s.setup;
    for(const auto& l:app->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind("FN67_IDS ",0)==0)
    {
        std::istringstream input(l.mText.substr(9));std::set<unsigned> seen;
        for(auto& id:s.targets){unsigned v=0;if(!Require(app,bool(input>>v)&&v&&v<=65535&&seen.insert(v).second,"Nut target IDs invalid"))return false;id=uint16_t(v);}
        std::string trailing;return Require(app,!(input>>trailing),"Nut target IDs trailing data");
    }
    return false;
}
bool SetupEater(LawnApp* app)
{
    auto* b=app->mBoard;auto* p=At(b);auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,kRow,0);
    if(!Require(app,p&&z&&z->mNetId,"Nut real biter allocation failed"))return false;
    auto* body=app->ReanimationTryToGet(z->mBodyReanimID);const float rate=body?body->mAnimRate:0;
    if(!Require(app,body&&rate>0&&std::isfinite(rate),"Nut biter animation missing"))return false;
    z->mBodyHealth=z->mBodyMaxHealth=kHealth;z->SetAnimRate(rate);
    z->mPosY=z->GetPosYBasedOnRow(kRow);z->mY=int(z->mPosY);
    bool found=false;for(int offset=80;offset>=-80;--offset)
    {z->mPosX=float(p->mX+offset);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){found=true;break;}}
    if(!Require(app,found,"Nut real chew rectangle cannot target native plant"))return false;
    s.eater=z->mNetId;s.previousHealth=p->mPlantHealth;s.previousHealthTick=b->mMainCounter;
    LawnNet::NetTrace("SMOKE_FUSION_NUT_EATER id=%u row=%d x=%d HP=5000 newActorInitialHPOnly=1 naturalEatPlant=1 plantHP=%d noPlantHealthWrites=1",z->mNetId,z->mRow,z->mX,p->mPlantHealth);
    return true;
}
bool ReadEater(LawnApp* app)
{
    if(app->NetIsHost())return s.eater!=0;
    for(const auto& l:app->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind("FN67_EATER ",0)==0)
    {
        std::istringstream in(l.mText.substr(11));unsigned id=0;std::string extra;
        if(!Require(app,bool(in>>id)&&id&&id<=65535&&!(in>>extra),"Nut biter ID invalid"))return false;s.eater=uint16_t(id);return true;
    }
    return false;
}
bool Observe(LawnApp* app)
{
    if(!Business(app))return false;auto* b=app->mBoard;auto* p=At(b);
    const auto* data=GetNativePlantData(p);const int stage=FusionNutShooterDamageStage(p->mPlantHealth);
    const int expectedStage=p->mPlantHealth<2666&&p->mPlantHealth>1333?1:p->mPlantHealth<1333?2:0;
    if(!Require(app,stage==expectedStage,"Nut crack stage differs from source strict health thresholds"))return false;
    if(app->NetIsHost())
    {
        if(p->mShootingCounter>s.previousCounter)
        {
            const int dt=b->mMainCounter-s.previousTick;
            if(!Require(app,p->mShootingCounter<=76&&76-p->mShootingCounter<=(std::max)(1,dt),"Nut source SpeedInactive75-event counter changed"))return false;
            const int trigger=b->mMainCounter-(76-p->mShootingCounter);s.fireTicks.push_back(trigger+75);
            LawnNet::NetTrace("SMOKE_FUSION_NUT_TRIGGER tick=%d expectedFire=%d counter=%d sourceDelay=75 speed=%.9g SpeedInactive=1 nativeClock=1",trigger,trigger+75,p->mShootingCounter,data->animationSpeed);
        }
        s.previousCounter=p->mShootingCounter;s.previousTick=b->mMainCounter;
    }
    Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
    {
        if(shot->mDead)continue;const auto* d=GetNativeProjectileData(shot);float cx=0,cy=0;
        if(!Require(app,d&&d->kind==2&&d->damage==20&&d->sunOnHit==0&&shot->mProjectileType==PROJECTILE_PEA&&GetNativeRollingProjectileCenter(shot,cx,cy),"Nut projectile metadata/native actor missing"))return false;
        if(!s.primary)s.primary=shot->mNetId;
        if(app->NetIsHost())
        {
            const int age=shot->mProjectileAge;float gx=0,gy=0;
            if(!Require(app,p->GetShadowCenter(gx,gy)&&std::abs(shot->mVelX-1.28f)<.0001f&&shot->mRotationSpeed==0,"Nut canonical anchor/rolling velocity changed"))return false;
            const double startZ=-35.86107635498047,initialV=.1173652708530426,acc=.0586826354265213;
            const double z=age>=33?0:startZ+age*initialV+acc*age*(age+1)/2;
            const double vz=age>=33?0:initialV+age*acc;
            if(!Require(app,std::abs(shot->mPosZ-z)<.025&&std::abs(shot->mVelZ-vz)<.003&&std::abs(cx-(gx+41.86666666666667+age*1.28))<.12&&
                std::abs(cy-(gy-5.988023952095808+shot->mPosZ))<.06&&std::abs(shot->mShadowY-(gy+5.988023952095808))<.04&&
                std::abs(shot->mRotation-(-.1884955592153876*age))<.01,"Nut actual native gravity/land/rotation sample differs"))return false;
            auto it=s.projectiles.find(shot->mNetId);
            if(it==s.projectiles.end())
            {
                const int born=b->mMainCounter-age+1;
                if(!Require(app,std::count(s.fireTicks.begin(),s.fireTicks.end(),born)==1,"Nut projectile missed the actual75-tick event"))return false;
                for(const auto& observed:s.projectiles)
                    if(!Require(app,observed.second.tick-observed.second.age+1!=born,"Nut duplicated a single source event projectile"))return false;
                s.projectiles.emplace(shot->mNetId,Shot{b->mMainCounter,age,cx});
                auto* eater=Target(b,s.eater);if(eater&&eater->mIsEating&&eater->FindPlantTarget(ATTACKTYPE_CHEW)==p)++s.stageShots[stage];
                LawnNet::NetTrace("SMOKE_FUSION_NUT_SHOT id=%u born=%d age=%d bullet=2 damage=20 sun=0 stage=%d biting=%d sourceDelay=75 nativeActor=1",shot->mNetId,born,age,stage,eater&&eater->mIsEating?1:0);
            }
            else
            {
                const int dt=b->mMainCounter-it->second.tick;
                if(!Require(app,age-it->second.age==dt&&std::abs(cx-it->second.x-1.28f*dt)<.06f,"Nut actual rolling displacement/age changed"))return false;
                it->second={b->mMainCounter,age,cx};
            }
            Sexy::Rect r=shot->GetProjectileRect();const double halfX=.358*(72.0/1.35)/2,halfY=.417*(100.0/1.67)/2;
            const int left=int(std::floor(cx-halfX)),top=int(std::floor(cy-halfY));
            if(!Require(app,r.mX==left&&r.mY==top&&r.mWidth==int(std::ceil(cx+halfX))-left&&r.mHeight==int(std::ceil(cy+halfY))-top,"Nut root collider rotated or lost source dimensions"))return false;
            const uint32_t generation=b->mProjectiles.DataArrayGetID(shot);const auto* context=GetNativeContext(b);
            const auto visited=context->rollingHits.find(generation);
            if(visited!=context->rollingHits.end())for(int i=0;i<3;++i)
                if(visited->second.contains(uint32_t(b->ZombieGetID(Target(b,s.targets[i])))))s.hittingGenerations[i].insert(generation);
            if(shot->mNetId==s.primary)
            {
                s.primaryGeneration=b->mProjectiles.DataArrayGetID(shot);s.primaryLastTick=b->mMainCounter;s.primaryLastAge=age;s.primaryLastX=cx;
                if((age==1||age==2||age==32||age==33||age==34)&&s.physicsSamples.insert(age).second)
                    LawnNet::NetTrace("SMOKE_FUSION_NUT_PHYSICS id=%u age=%d x=%.6f Z=%.9g Vz=%.9g rotation=%.9g vx=1.28 rootColliderUnrotated=1 sourceMetricAdapter=1",shot->mNetId,age,cx,shot->mPosZ,shot->mVelZ,shot->mRotation);
                if(!s.controls)
                {
                    if(!Require(app,!NativeRollingProjectileCanHit(shot,Target(b,s.targets[3]))&&!NativeRollingProjectileCanHit(shot,Target(b,s.targets[4]))&&
                        Target(b,s.targets[5])->IsFlying()&&!NativeRollingProjectileCanHit(shot,Target(b,s.targets[5])),"Nut actual query accepts MC/adjacent/flying control"))return false;
                    s.controls=true;LawnNet::NetTrace("SMOKE_FUSION_NUT_FILTER mindControlled=0 adjacentRow=0 flying=0 actualCanHit=1 nativeBalloonControlOnly=1");
                }
            }
        }
        else s.clientProjectile=true;
        Capture(app,"fusion_nut_projectile");
    }
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=Target(b,s.targets[i]);if(!z)return true;
        const int loss=kHealth-z->mBodyHealth;
        if(!Require(app,!z->IsDeadOrDying()&&z->mHelmHealth==0&&z->mShieldHealth==0&&loss>=0&&loss%20==0&&(i<3||loss==0),"Nut repeated/AoE/control impact changed expected health"))return false;
        if(app->NetIsHost()&&i<3&&!Require(app,loss==20*int(s.hittingGenerations[i].size()),"Nut damage did not conserve one20-hit per projectile/actor generation"))return false;
        if(app->NetIsHost()&&!Require(app,z->mRow==kTargetRows[i]&&std::abs(z->mPosX-kTargetX[i])<.01f,"Nut stationary target moved"))return false;
    }
    if(app->NetIsHost())
    {
        auto* primary=ProjectileById(b,s.primary);const auto* context=GetNativeContext(b);
        if(primary&&!s.triple)
        {
            const auto found=context->rollingHits.find(s.primaryGeneration);
            bool three=found!=context->rollingHits.end();
            for(int i=0;i<3;++i)three=three&&found->second.contains(uint32_t(b->ZombieGetID(Target(b,s.targets[i]))));
            if(three)
            {
                if(!Require(app,found->second.size()==3&&s.projectiles.size()==1&&Target(b,s.targets[0])->mBodyHealth==4980&&Target(b,s.targets[1])->mBodyHealth==4980&&Target(b,s.targets[2])->mBodyHealth==4980,"Nut first projectile did not independently pierce exactly three unique actors"))return false;
                s.triple=true;LawnNet::NetTrace("SMOKE_FUSION_NUT_TRIPLE projectile=%u targets=%u,%u,%u visited=3 births=1 HP=4980,4980,4980 alive=1 damage=20 perGeneration=1",s.primary,s.targets[0],s.targets[1],s.targets[2]);
            }
        }
        if(primary&&s.triple&&s.projectiles.size()==1)
        {
            const auto rect=primary->GetProjectileRect(),body=Target(b,s.targets[0])->GetZombieRect();
            if(rect.mX<=body.mX+body.mWidth&&rect.mX+rect.mWidth>=body.mX)
            {
                if(!Require(app,Target(b,s.targets[0])->mBodyHealth==4980&&Target(b,s.targets[1])->mBodyHealth==4980&&Target(b,s.targets[2])->mBodyHealth==4980,"Nut repeated contact hit the same collider again"))return false;
                if(s.overlapLastTick!=b->mMainCounter){s.overlapLastTick=b->mMainCounter;++s.overlapTicks;}
            }
        }
        if(s.primary&&!primary&&!s.boundary)
        {
            const double right=b->GetCellCenterX(0,0)+(11.2+4.8)*(72.0/1.35);
            if(!Require(app,s.triple&&s.overlapTicks>=3&&b->mMainCounter-s.primaryLastTick==1&&s.primaryLastX<=right+.02&&s.primaryLastX+1.28>right-.02&&
                !context->rollingHits.contains(s.primaryGeneration),"Nut primary retired early or failed source-bound/history cleanup"))return false;
            s.boundary=true;LawnNet::NetTrace("SMOKE_FUSION_NUT_BOUNDARY projectile=%u lastAge=%d lastX=%.6f right=%.6f nextOutside=1 visitedRetired=1 overlapTicks=%d nativeClock=1",s.primary,s.primaryLastAge,s.primaryLastX,right,s.overlapTicks);
        }
    }
    bool allHit=true;for(int i=0;i<3;++i)allHit=allHit&&Target(b,s.targets[i])->mBodyHealth<=4980;
    if(allHit)s.clientTriple=true;
    if(s.eater&&!s.eaterParked)
    {
        auto* eater=Target(b,s.eater);
        if(app->NetIsHost()&&p->mPlantHealth<s.previousHealth)
        {
            const int loss=s.previousHealth-p->mPlantHealth,dt=b->mMainCounter-s.previousHealthTick;
            if(!Require(app,eater&&eater->mIsEating&&eater->FindPlantTarget(ATTACKTYPE_CHEW)==p&&loss%4==0&&loss<=4*(dt/4+1),"Nut health loss was not real native chew"))return false;
        }
        s.previousHealth=p->mPlantHealth;s.previousHealthTick=b->mMainCounter;
    }
    if(stage==1||stage==2)
    {
        const char* name=stage==1?"fusion_nut_crack1":"fusion_nut_crack2";
        if(Capture(app,name)&&!(s.crackLoggedMask&(1<<stage)))
        {s.crackLoggedMask|=1<<stage;LawnNet::NetTrace("SMOKE_FUSION_NUT_CRACK role=%s stage=%d HP=%d sourceMax=4000 nativeHealth=1",Role(app),stage,p->mPlantHealth);}
        if(app->NetIsHost()&&stage==2&&s.stageShots[1]>0&&s.stageShots[2]>0&&!s.eaterParked)
        {
            auto* eater=Target(b,s.eater);if(!Require(app,eater&&eater->mIsEating,"Nut stage2 shot occurred without natural biter"))return false;
            eater->mPosX=1100;eater->mX=1100;s.eaterParked=true;
            LawnNet::NetTrace("SMOKE_FUSION_NUT_BITING_DONE HP=%d stage1Shots=%d stage2Shots=%d positionOnlyPark=1 healthWrites=0 timerWrites=0",p->mPlantHealth,s.stageShots[1],s.stageShots[2]);
        }
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionNut(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_NUT");
    if(!app||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!app->CoopSmokeActive()||app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(app->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>(s.stage<=3?60000U:s.stage==8?45000U:25000U)){char e[100];sprintf_s(e,"Nut stage deadline stage=%d role=%s",s.stage,Role(app));app->CoopSmokeFail(e);return true;}
    if(!app->mNetManager||!app->mNetManager->IsActive())return true;app->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(app->NetIsHost()&&!s.configured){if(!Require(app,!app->mBoard&&!app->mSeedChooserScreen,"Nut needs a clean lobby"))return true;app->mNetManager->SetDifficulty(1);s.configured=true;}
        if(app->mNetManager->GetPlayerCount()!=2||(app->NetIsClient()&&!app->mNetManager->IsConnectedToHost()))return true;
        if(app->NetIsClient()){Advance(app,1);Send(app,"FN67_START");}
        else if(Has(app,1,"FN67_START")){Advance(app,1);std::string e;if(!app->StartFusionValidation(&e))app->CoopSmokeFail(e.c_str());}return true;
    }
    if(!app->mBoard||!IsNativeFusionBoard(app->mBoard)){if(s.stage==1&&app->NetIsClient())Send(app,"FN67_START");return true;}
    auto* b=app->mBoard;const auto* context=GetNativeContext(b);auto* chooser=app->mSeedChooserScreen;const int owner=app->mNetManager->GetMySlot();
    if(!Require(app,context&&context->validation&&!app->mFusionRuntime&&b->GetColumnCount()==10&&b->GetCellPitchX()==72,"Nut lost actual native10x5 Board"))return true;
    const auto& bank=context->selection.State();
    if(s.stage>=7&&!Observe(app))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Pool(app,*context)||!Capture(app,"fusion_nut_chooser"))return true;
        std::vector<int> choices;for(size_t i=0;i<context->eligible.size();++i)
        {const auto& c=context->eligible[i];if(owner==0?!c.isExtra&&c.kind!=3:c.isExtra||c.kind==3)choices.push_back(int(i));}
        auto priority=[&](int i){const auto& c=context->eligible[i];return owner==0?(c.kind==0?0:10):(!c.isExtra&&c.kind==3?0:10);};
        std::stable_sort(choices.begin(),choices.end(),[&](int a,int c){return priority(a)<priority(c);});
        if(!Require(app,choices.size()>=bank.quotas[owner]&&!choices.empty()&&priority(choices[0])==0,"Nut real chooser priority unavailable"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==owner;if(own<s.waitingOwn)return true;
        if(own<bank.quotas[owner]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(app,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=bank.targetCount)return true;
        auto* a=Packet(b,0,0,false);auto* n=Packet(b,1,3,false);if(!Require(app,a&&n,"Nut normal0/normal3 real packets missing"))return true;
        s.parentPacket=a->mNetId;s.nutPacket=n->mNetId;if(!Capture(app,"fusion_nut_picked"))return true;
        NetTrace("SMOKE_FUSION_NUT_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u normal0Host=1 normal3Client=1 unfiltered=1",Role(app),context->eligible.size(),bank.targetCount,bank.quotas[0],bank.quotas[1]);Advance(app,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(app,3);else if(chooser&&!chooser->mSeedsInFlight&&Button(app,chooser->mStartButton))Advance(app,3);}
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(app,"fusion_nut_intro");return true;}
        if(bank.phase!=FusionBankPhase::Playing||app->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(app,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_nut_intro")&&b->mSunMoney==500,"Nut skipped Ready/full intro/fresh500"))return true;
        if(app->NetIsHost()&&!s.ambient){b->mSunCountDown=100000;b->mZombieCountDown=b->mZombieCountDownStart=100000;s.ambient=true;NetTrace("SMOKE_FUSION_NUT_AMBIENT wavesAndSkyOnly=deferred plantClocksUnchanged=1 fixtureControl=1");}
        if(!Capture(app,"fusion_nut_playing"))return true;NetTrace("SMOKE_FUSION_NUT_PLAYING role=%s sun=500 nativeBoard=1 secondWorld=0",Role(app));Advance(app,4);
    }
    else if(s.stage==4)
    {
        if(!Barrier(app,"FN67_PLAY"))return true;
        if(owner==0&&!s.clicked){if(Pick(app,Packet(b,0,0,false)))s.clicked=true;return true;}
        if(owner==0&&!s.placed){if(!Held(b,s.parentPacket))return true;s.placed=Cell(app);return true;}
        auto* p=At(b);const auto* d=GetNativePlantData(p);if(!d||d->kind!=0)return true;
        if(!Require(app,b->mSunMoney==400&&d->owner==0&&d->packetId==s.parentPacket,"Nut ordinary Pea parent purchase differs"))return true;
        s.parent=p->mNetId;if(Barrier(app,"FN67_PARENT"))Advance(app,5);
    }
    else if(s.stage==5)
    {
        if(owner==1&&!s.clicked){s.heldSamples=Samples(app);if(Pick(app,Packet(b,1,3,false)))s.clicked=true;return true;}
        if(owner==1&&!s.placed)
        {
            if(!Held(b,s.nutPacket)||Samples(app)<=s.heldSamples||!Capture(app,"fusion_nut_held"))return true;
            if(!Require(app,b->mSunMoney==400&&At(b)&&At(b)->mNetId==s.parent,"Nut held pickup mutated parent/balance"))return true;
            if(!Send(app,"FN67_HELD"))return true;s.placed=Cell(app);return true;
        }
        if(!s.purchaseObserved)
        {
            auto* p=At(b);const auto* d=GetNativePlantData(p);if(!d||d->kind!=1004)return true;s.finalPlant=p->mNetId;
            if(!Require(app,s.finalPlant!=s.parent&&p->mPlantHealth==4000,"Nut fusion did not replace parent with fresh4000HP"))return true;
            if(!Business(app)||!Capture(app,"fusion_nut_fused"))return true;s.purchaseObserved=true;s.purchaseSamples=Samples(app);
        }
        if(!Barrier(app,"FN67_FUSED"))return true;
        NetTrace("SMOKE_FUSION_NUT_PURCHASE role=%s old=%u id=%u packet=%u parents=0,3 owner=1 extra=0 usedPrice=50 sun=350 HP=4000 sourceCD=3000 nativeSDL=1",Role(app),s.parent,s.finalPlant,s.nutPacket);Advance(app,6);
    }
    else if(s.stage==6)
    {
        if(!Business(app))return true;
        if(!s.purchaseObserved){if(!Require(app,b->mZombies.mSize==0&&b->mProjectiles.mSize==0,"Nut fired before target activation"))return true;s.purchaseObserved=true;}
        if(!Barrier(app,"FN67_SETUP"))return true;if(app->NetIsHost()&&!s.setup&&!Setup(app))return true;
        if(app->NetIsHost()&&!Send(app,Ids()))return true;if(!ReadIds(app))return true;
        for(size_t i=0;i<s.targets.size();++i)
        {
            auto* z=Target(b,s.targets[i]);
            if(!z||z->IsDeadOrDying())
            {
                if(app->NetIsHost())
                {
                    NetTrace("SMOKE_FUSION_NUT_TARGET_LOST index=%zu id=%u found=%d dead=%d x=%.3f mind=%d setupX=%.3f",i,s.targets[i],z?1:0,z&&z->IsDeadOrDying()?1:0,z?z->mPosX:-1,z&&z->mMindControlled?1:0,i==3?float(kTargetX[i]):1800.0f+100*float(i));
                    Require(app,false,"Nut staged native target retired before the ID barrier");
                }
                return true;
            }
        }
        if(!Barrier(app,"FN67_TARGETS"))return true;
        if(app->NetIsHost()){for(size_t i=0;i<s.targets.size();++i){auto* z=Target(b,s.targets[i]);z->mPosX=float(kTargetX[i]);z->mX=kTargetX[i];}s.previousCounter=At(b)->mShootingCounter;s.previousTick=b->mMainCounter;s.active=true;}
        Advance(app,7);
    }
    else if(s.stage==7)
    {
        if(app->NetIsHost()&&(!s.triple||s.overlapTicks<3))return true;
        if(!s.clientTriple||!s.frames.contains("fusion_nut_projectile")||!Capture(app,"fusion_nut_triple")||!Barrier(app,"FN67_TRIPLE"))return true;
        if(app->NetIsHost()&&!s.eater&&!SetupEater(app))return true;
        if(app->NetIsHost()&&!Send(app,"FN67_EATER "+std::to_string(s.eater)))return true;
        if(!ReadEater(app))return true;Advance(app,8);
    }
    else if(s.stage==8)
    {
        if(app->NetIsHost()&&s.eaterParked&&s.boundary&&s.physicsSamples.contains(32)&&s.physicsSamples.contains(33))Send(app,"FN67_NATURAL_DONE");
        if(!Has(app,0,"FN67_NATURAL_DONE")||!s.frames.contains("fusion_nut_crack1")||!s.frames.contains("fusion_nut_crack2")||
            (app->NetIsClient()&&!s.clientProjectile))return true;
        if(!Barrier(app,"FN67_CRACKS"))return true;Advance(app,9);
    }
    else if(s.stage==9)
    {
        if(Samples(app)<8||Samples(app)-s.purchaseSamples<2||!Barrier(app,"FN67_DONE")||!Capture(app,"fusion_nut_complete"))return true;
        NetTrace("SMOKE_FUSION_NUT_COMPLETE role=%s id=%u primary=%u HP=%d maxHP=4000 stage=2 sun=350 sourceCD=3000 threePierce=1 MC=5000 adjacent=5000 flying=5000 shootingWhileBitten=1 nativeBoard=1 secondWorld=0 plantHealthWrites=0 plantTimerWrites=0 samples=%u",Role(app),s.finalPlant,s.primary,At(b)->mPlantHealth,Samples(app));
        Advance(app,10);app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
