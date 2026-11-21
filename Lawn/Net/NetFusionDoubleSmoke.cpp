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
constexpr std::array<int,3> kTargetX={460,560,460};
constexpr std::array<int,3> kTargetRows={kRow,kRow,kRow+1};
// Independent source AnimationEvent float32 values, not a call to the timing implementation.
constexpr std::array<float,2> kEventSeconds={.53333336f,1.0666667f};
struct Volley { int trigger=0,initial=0;std::array<int,2> deadlines{};unsigned fired=0; };
struct ProjectileSample { int tick=0;float x=0; };
struct State
{
    int stage=0,kind=0,waitingOwn=0,previousCounter=0,previousTick=0,hits=0,tracedHits=0;
    uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,heldSamples=0,purchaseSamples=0;
    uint16_t parent=0,intermediate=0,finalPlant=0,parentPacket=0,extraPacket=0,cherryPacket=0;
    bool configured=false,clicked=false,placed=false,purchaseObserved=false,ambient=false,setup=false,active=false,parked=false;
    bool clientActive=false,clientProjectile=false;
    std::array<uint16_t,3> targets{};
    std::vector<Volley> volleys;
    std::map<uint16_t,ProjectileSample> projectiles;
    std::map<std::string,int> firstDraw;
    std::set<std::string> frames;
} s;
const char* Role(LawnApp* app){return app->NetIsHost()?"host":"client";}
int Damage(){return s.kind==1030?20:60;}
int BulletKind(){return s.kind==1030?0:1;}
int FinalSun(){return s.kind==1030?200:50;}
uint32_t Samples(LawnApp* app){return app->NetIsHost()?app->mCoopSmokeSnapshotsSent:app->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* app,bool value,const char* error){if(!value)app->CoopSmokeFail(error);return value;}
void Advance(LawnApp* app,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.purchaseObserved=false;
    LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_STAGE role=%s kind=%d stage=%d",Role(app),s.kind,stage);
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
    if(!Require(app,app->CoopSmokeCaptureFrame(name),"Double actual renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_RENDER role=%s frame=%s freshDraws=%d",Role(app),name,app->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* app,float x,float y)
{
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.windowID=motion.motion.windowID;down.button.button=SDL_BUTTON_LEFT;
    down.button.clicks=1;down.button.down=true;down.button.x=x;down.button.y=y;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
    return Require(app,SDL_PushEvent(&motion)&&SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Double scoped SDL queue failed");
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
        Require(app,SDL_PushEvent(&motion),"Double Ready hover failed");return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_READY role=%s nativeSDL=1 hoverObserved=1",Role(app));return Click(app,x,y);
}
SeedPacket* Packet(Board* board,int owner,int kind,bool extra)
{
    for(int i=0;i<board->mSeedBank->mNumPackets;++i)
    {auto* p=&board->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==owner&&d->kind==kind&&d->isExtra==extra)return p;}
    return nullptr;
}
bool Pick(LawnApp* app,SeedPacket* p)
{
    if(!Require(app,p&&p->mActive&&!p->mRefreshing,"Double real card is not ready"))return false;
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
{Coin* c=nullptr;while(app->mBoard->mCoins.IterateNext(c))if(!Require(app,c->mDead,"Double projectile unexpectedly created sun/Coin"))return false;return true;}
bool Pool(LawnApp* app,const NativeContext& context)
{
    const auto& state=context.selection.State();const size_t count=(std::min)(context.eligible.size(),kNativeFusionBankCapacity);
    if(!Require(app,state.activeMask==3&&state.targetCount==count&&state.quotas[0]+state.quotas[1]==count&&state.quotas[0]>=2&&state.quotas[1]>=1,"Double real chooser quota/capacity mismatch"))return false;
    std::set<std::pair<FusionPlantId,bool>> identities;
    for(const auto& e:context.eligible)
    {
        const auto* d=FusionFindPlant(e.kind);FusionBankEligibleCard sourceCard;
        if(!Require(app,d&&d->hasStats&&GetNativeSeedForPlant(e.kind)!=SEED_NONE&&identities.emplace(e.kind,e.isExtra).second&&
            TryMakeFusionEligibleCard(e.kind,e.isExtra,sourceCard)&&e==sourceCard&&e.rechargeSeconds==d->rechargeSeconds,"Double eligible identity/price/CD changed"))return false;
    }
    for(FusionPlantId k:{FusionPlantId(0),FusionPlantId(1),FusionPlantId(2),FusionPlantId(3),FusionPlantId(4),FusionPlantId(5),FusionPlantId(6),FusionPlantId(7)})
        if(!Require(app,identities.contains({k,false})&&identities.contains({k,true}),"Double production pool filtered an existing base identity"))return false;
    return true;
}
bool Business(LawnApp* app,bool final)
{
    auto* board=app->mBoard;auto* p=At(board);const auto* d=GetNativePlantData(p);
    auto* normal=Packet(board,0,0,false);auto* extra=Packet(board,1,0,true);auto* cherry=Packet(board,0,2,false);
    const bool last=final&&s.kind==1008;const int expected=last?1008:1030;
    // A source-ready, never-used card has no live refresh cycle yet. Its
    // configured50s becomes mRefreshTime only when native WasPlanted runs.
    const int cherryRefresh=last?5000:0;
    const bool valid=p&&d&&normal&&extra&&cherry&&p->mNetId==(last?s.finalPlant:s.intermediate)&&d->kind==expected&&
        d->firstParent==(last?1030:0)&&d->secondParent==(last?2:0)&&d->owner==(last?0:1)&&d->packetId==(last?s.cherryPacket:s.extraPacket)&&
        d->randomOrigin==0&&p->mSeedType==SEED_PEASHOOTER&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&p->mDoSpecialCountdown==0&&
        d->productionTicks==0&&d->pendingProductionTicks==0&&FusionNativeAnimationSpeedValid(expected,d->animationSpeed)&&
        board->mSunMoney==(last?50:200)&&normal->mNetId==s.parentPacket&&extra->mNetId==s.extraPacket&&cherry->mNetId==s.cherryPacket&&
        normal->mRefreshTime==750&&extra->mRefreshTime==750&&cherry->mRefreshTime==cherryRefresh&&
        (last||(cherry->mActive&&!cherry->mRefreshing&&cherry->mRefreshCounter==0))&&
        (!app->NetIsHost()||(normal->mTimesUsed==1&&extra->mTimesUsed==1&&cherry->mTimesUsed==(last?1:0)));
    if(!valid)
    {
        LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_BUSINESS_FAIL role=%s stage=%d final=%d expectedKind=%d actualKind=%d actor=%u expectedActor=%u owner=%d packet=%u parents=%d,%d HP=%d/%d fuse=%d speed=%.9g production=%d/%d sun=%d expectedSun=%d",
            Role(app),s.stage,final?1:0,expected,d?int(d->kind):-1,p?p->mNetId:0,last?s.finalPlant:s.intermediate,d?int(d->owner):-1,d?d->packetId:0,
            d?int(d->firstParent):-1,d?int(d->secondParent):-1,p?p->mPlantHealth:-1,p?p->mPlantMaxHealth:-1,p?p->mDoSpecialCountdown:-1,
            d?d->animationSpeed:0,d?d->productionTicks:-1,d?d->pendingProductionTicks:-1,board->mSunMoney,last?50:200);
        LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_PACKET_FAIL normal=%u/%u refresh=%d used=%d extra=%u/%u refresh=%d used=%d cherry=%u/%u refresh=%d expectedRefresh=%d counter=%d active=%d refreshing=%d used=%d expectedUsed=%d",
            normal?normal->mNetId:0,s.parentPacket,normal?normal->mRefreshTime:-1,normal?normal->mTimesUsed:-1,
            extra?extra->mNetId:0,s.extraPacket,extra?extra->mRefreshTime:-1,extra?extra->mTimesUsed:-1,
            cherry?cherry->mNetId:0,s.cherryPacket,cherry?cherry->mRefreshTime:-1,cherryRefresh,cherry?cherry->mRefreshCounter:-1,
            cherry&&cherry->mActive?1:0,cherry&&cherry->mRefreshing?1:0,cherry?cherry->mTimesUsed:-1,last?1:0);
        return Require(app,false,"Double actor ancestry/owner/packet/HP/balance/CD conservation failed");
    }
    const auto* a=GetNativePacketData(normal);const auto* b=GetNativePacketData(extra);const auto* c=GetNativePacketData(cherry);
    if(!Require(app,a&&b&&c&&a->price==100&&!a->isExtra&&b->price==200&&b->isExtra&&c->price==150&&!c->isExtra,"Double normal/extra packet provenance drifted"))return false;
    Plant* other=nullptr;while(board->mPlants.IterateNext(other))if(!other->mDead)
        if(!Require(app,other==p&&GetNativePlantData(other)&&GetNativePlantData(other)->kind!=2,"Double purchase spawned a temporary Cherrybomb/extra live plant"))return false;
    return NoCoins(app);
}
bool Setup(LawnApp* app)
{
    auto* board=app->mBoard;
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=board->AddZombieInRow(ZOMBIE_NORMAL,kTargetRows[i],0);
        if(!Require(app,z&&z->mNetId,"Double native target allocation failed"))return false;
        auto* body=app->ReanimationTryToGet(z->mBodyReanimID);const float walking=body?body->mAnimRate:0;
        if(!Require(app,body&&std::isfinite(walking)&&walking>0,"Double original native walking rate missing"))return false;
        z->mPosX=1000.0f+float(i)*100;z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(kTargetRows[i]);z->mY=int(z->mPosY);
        z->mVelX=0;z->mMindControlled=false;z->mBodyHealth=z->mBodyMaxHealth=kHealth;
        z->mHelmType=HELMTYPE_NONE;z->mShieldType=SHIELDTYPE_NONE;z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;
        z->SetAnimRate(walking);z->ApplyAnimRate(0.0f);s.targets[i]=z->mNetId;
        if(!Require(app,body->mAnimRate==0&&z->mOriginalAnimRate==walking,"Double native walking freeze failed"))return false;
        LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_SETUP index=%zu id=%u row=%d body=5000 helm=0 shield=0 staged=1 originalRate=%.6f fixtureControl=1",i,z->mNetId,kTargetRows[i],walking);
    }
    s.setup=true;return true;
}
std::string Ids(){std::ostringstream out;out<<"FD60_IDS";for(auto id:s.targets)out<<' '<<id;return out.str();}
bool ReadIds(LawnApp* app)
{
    if(app->NetIsHost())return s.setup;
    for(const auto& l:app->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind("FD60_IDS ",0)==0)
    {
        std::istringstream input(l.mText.substr(9));std::set<unsigned> seen;
        for(auto& id:s.targets){unsigned v=0;if(!Require(app,bool(input>>v)&&v&&v<=65535&&seen.insert(v).second,"Double target ID message invalid"))return false;id=uint16_t(v);}
        std::string extra;return Require(app,!(input>>extra),"Double target IDs trailing data");
    }
    return false;
}
void MoveX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
bool Observe(LawnApp* app)
{
    if(!Business(app,true))return false;
    auto* board=app->mBoard;auto* p=At(board);const auto* d=GetNativePlantData(p);
    if(app->NetIsHost())
    {
        if(p->mShootingCounter>s.previousCounter)
        {
            const std::array<int,2> deadlines={int(std::ceil(double(kEventSeconds[0])/(2*double(d->animationSpeed))*100)),int(std::ceil(double(kEventSeconds[1])/(2*double(d->animationSpeed))*100))};
            const int initial=deadlines[1]+1,dt=board->mMainCounter-s.previousTick;
            if(!Require(app,s.volleys.size()<2&&(s.volleys.empty()||s.volleys.back().fired==3)&&p->mShootingCounter<=initial&&initial-p->mShootingCounter<=(std::max)(1,dt),"Double source trigger counter/event count differs"))return false;
            const int trigger=board->mMainCounter-(initial-p->mShootingCounter);s.volleys.push_back({trigger,initial,deadlines,0});
            LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_TRIGGER role=host kind=%d volley=%zu tick=%d counter=%d initial=%d first=%d second=%d speed=%.9g nominalDeadlines=27,54 nativeClock=1 timerWrites=0",s.kind,s.volleys.size(),trigger,p->mShootingCounter,initial,deadlines[0],deadlines[1],d->animationSpeed);
        }
        s.previousCounter=p->mShootingCounter;s.previousTick=board->mMainCounter;
    }
    Projectile* shot=nullptr;while(board->mProjectiles.IterateNext(shot))
    {
        if(shot->mDead)continue;const auto* meta=GetNativeProjectileData(shot);
        if(!Require(app,meta&&meta->kind==BulletKind()&&meta->damage==Damage()&&meta->sunOnHit==0&&shot->mProjectileType==PROJECTILE_PEA,"Double projectile identity/damage/sun policy differs"))return false;
        if(app->NetIsHost())
        {
            if(!Require(app,std::abs(shot->mVelX-3.2f)<.0001f,"Double straight projectile speed differs"))return false;
            auto it=s.projectiles.find(shot->mNetId);
            if(it==s.projectiles.end())
            {
                const int fireTick=board->mMainCounter-shot->mProjectileAge+1;int volley=-1,event=-1;
                for(size_t v=0;v<s.volleys.size();++v)for(int e=0;e<2;++e)if(s.volleys[v].trigger+s.volleys[v].deadlines[e]==fireTick){volley=int(v);event=e;}
                if(!Require(app,volley>=0&&event>=0&&!(s.volleys[volley].fired&(1U<<event))&&s.projectiles.size()<4,"Double duplicate/early/late/missing source event projectile"))return false;
                s.volleys[volley].fired|=1U<<event;s.projectiles.emplace(shot->mNetId,ProjectileSample{board->mMainCounter,shot->mPosX});
                LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_PROJECTILE role=host kind=%d id=%u volley=%d event=%d fireTick=%d triggerTick=%d deadline=%d age=%d bullet=%d damage=%d sun=0 vx=3.2 nativeActor=1",s.kind,shot->mNetId,volley+1,event+1,fireTick,s.volleys[volley].trigger,s.volleys[volley].deadlines[event],shot->mProjectileAge,BulletKind(),Damage());
            }
            else
            {
                const int dt=board->mMainCounter-it->second.tick;
                if(!Require(app,std::abs(shot->mPosX-it->second.x-3.2f*dt)<.06f,"Double actual native projectile displacement differs"))return false;
                it->second={board->mMainCounter,shot->mPosX};
            }
        }
        else s.clientProjectile=true;
        Capture(app,"fusion_double_projectile");
    }
    auto* front=Target(board,s.targets[0]);auto* rear=Target(board,s.targets[1]);auto* adjacent=Target(board,s.targets[2]);
    if(!front||!rear||!adjacent)return true;
    const int loss=kHealth-front->mBodyHealth;
    if(!Require(app,loss>=0&&loss%Damage()==0&&loss/Damage()<=4&&!front->IsDeadOrDying()&&!rear->IsDeadOrDying()&&!adjacent->IsDeadOrDying()&&
        rear->mBodyHealth==kHealth&&adjacent->mBodyHealth==kHealth&&front->mHelmHealth==0&&front->mShieldHealth==0&&rear->mHelmHealth==0&&rear->mShieldHealth==0&&adjacent->mHelmHealth==0&&adjacent->mShieldHealth==0,
        "Double impact is not one normal payload/noAoE/noPierce"))return false;
    s.hits=loss/Damage();
    if(!app->NetIsHost()&&!s.clientActive)
    {if(std::abs(front->mPosX-kTargetX[0])>.01f)return true;s.clientActive=true;}
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=Target(board,s.targets[i]);const bool parked=s.hits==4&&i<2&&z->mPosX>=1000;
        if(!Require(app,z->mZombieType==ZOMBIE_NORMAL&&z->mRow==kTargetRows[i]&&(parked||std::abs(z->mPosX-kTargetX[i])<.01f)&&
            std::abs(z->mPosY-z->GetPosYBasedOnRow(kTargetRows[i]))<.01f&&z->mOriginalAnimRate>0,"Double frozen real target moved or changed family"))return false;
    }
    if(s.hits>s.tracedHits)
    {
        if(app->NetIsHost()&&!Require(app,s.hits==s.tracedHits+1,"Double host skipped an individual damage transition"))return false;
        s.tracedHits=s.hits;LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_DAMAGE role=%s kind=%d hits=%d front=%d rear=5000 adjacent=5000 damage=%d noAoE=1 noSun=1 noPierce=1 sun=%d healthWrites=0",Role(app),s.kind,s.hits,front->mBodyHealth,Damage(),FinalSun());
    }
    if(app->NetIsHost()&&s.hits==4&&!s.parked)
    {
        if(!Require(app,s.volleys.size()==2&&s.volleys[0].fired==3&&s.volleys[1].fired==3&&s.projectiles.size()==4,"Double did not produce exactly two two-event volleys"))return false;
        MoveX(front,1000);MoveX(rear,1100);s.parked=true;
        LawnNet::NetTrace("SMOKE_FUSION_DOUBLE_PARK role=host afterHits=4 positionOnly=1 countersUnchanged=1 preventFurtherAcquisition=1");
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionDouble(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_DOUBLE");
    if(!app||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!app->CoopSmokeActive()||app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(app->mCoopSmokeResultWritten)return true;
    if(!s.since)
    {
        s.since=GetTickCount();const char* kind=std::getenv("PVZ_COOP_SMOKE_FUSION_DOUBLE_KIND");
        if(!Require(app,kind&&(!std::strcmp(kind,"1030")||!std::strcmp(kind,"1008")),"Double strict source-kind flag missing"))return true;
        s.kind=!std::strcmp(kind,"1030")?1030:1008;
    }
    if(GetTickCount()-s.since>(s.stage<=3?60000U:25000U)){char e[100];sprintf_s(e,"Double stage deadline stage=%d role=%s",s.stage,Role(app));app->CoopSmokeFail(e);return true;}
    if(!app->mNetManager||!app->mNetManager->IsActive())return true;app->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(app->NetIsHost()&&!s.configured){if(!Require(app,!app->mBoard&&!app->mSeedChooserScreen,"Double lobby not clean"))return true;app->mNetManager->SetDifficulty(1);s.configured=true;}
        if(app->mNetManager->GetPlayerCount()!=2||(app->NetIsClient()&&!app->mNetManager->IsConnectedToHost()))return true;
        if(app->NetIsClient()){Advance(app,1);Send(app,"FD60_START");}
        else if(Has(app,1,"FD60_START")){Advance(app,1);std::string error;if(!app->StartFusionValidation(&error))app->CoopSmokeFail(error.c_str());}
        return true;
    }
    if(!app->mBoard||!IsNativeFusionBoard(app->mBoard)){if(s.stage==1&&app->NetIsClient())Send(app,"FD60_START");return true;}
    auto* board=app->mBoard;const auto* context=GetNativeContext(board);auto* chooser=app->mSeedChooserScreen;
    if(!Require(app,context&&context->validation&&!app->mFusionRuntime&&board->GetColumnCount()==10&&board->GetCellPitchX()==72,"Double is not the shared native10x5 Board"))return true;
    const auto& state=context->selection.State();const int owner=app->mNetManager->GetMySlot();
    if(s.stage>=9&&!Observe(app))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!board->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Pool(app,*context)||!Capture(app,"fusion_double_chooser"))return true;
        std::vector<int> choices;for(size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].isExtra==(owner==1))choices.push_back(int(i));
        auto priority=[&](int i){const auto k=context->eligible[i].kind;return k==0?0:owner==0&&k==2?1:10;};
        std::stable_sort(choices.begin(),choices.end(),[&](int a,int b){return priority(a)<priority(b);});
        if(!Require(app,choices.size()>=state.quotas[owner]&&!choices.empty()&&priority(choices[0])==0&&(owner==1||(choices.size()>1&&priority(choices[1])==1)),"Double chooser required priorities missing"))return true;
        int own=0;for(const auto& p:state.packets)own+=p.owner==owner;if(own<s.waitingOwn)return true;
        if(own<state.quotas[owner])
        {int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(app,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(state.packets.size()!=state.targetCount)return true;
        auto* a=Packet(board,0,0,false);auto* b=Packet(board,1,0,true);auto* c=Packet(board,0,2,false);
        if(!Require(app,a&&b&&c,"Double selected real packets missing"))return true;
        s.parentPacket=a->mNetId;s.extraPacket=b->mNetId;s.cherryPacket=c->mNetId;
        if(!Capture(app,"fusion_double_picked"))return true;
        NetTrace("SMOKE_FUSION_DOUBLE_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u priorities=0normal,2normal,0extra unfiltered=1 nativeChooser=1",Role(app),context->eligible.size(),state.targetCount,state.quotas[0],state.quotas[1]);Advance(app,2);
    }
    else if(s.stage==2)
    {if(state.phase!=FusionBankPhase::Choosing)Advance(app,3);else if(chooser&&!chooser->mSeedsInFlight&&Button(app,chooser->mStartButton))Advance(app,3);}
    else if(s.stage==3)
    {
        if(state.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(app,"fusion_double_intro");return true;}
        if(state.phase!=FusionBankPhase::Playing||app->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(app,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_double_intro")&&board->mSunMoney==500,"Double skipped Ready/full native intro/fresh500"))return true;
        if(app->NetIsHost()&&!s.ambient){board->mSunCountDown=100000;board->mZombieCountDown=board->mZombieCountDownStart=100000;s.ambient=true;NetTrace("SMOKE_FUSION_DOUBLE_AMBIENT wavesAndSkyOnly=deferred nativePlantClocksUnchanged=1 fixtureControl=1");}
        if(!Capture(app,"fusion_double_playing"))return true;
        NetTrace("SMOKE_FUSION_DOUBLE_PLAYING role=%s kind=%d epoch=%u sun=500 nativeBoard=1 secondWorld=0",Role(app),s.kind,context->epoch);Advance(app,4);
    }
    else if(s.stage==4)
    {
        if(!Barrier(app,"FD60_PLAY"))return true;
        auto* packet=Packet(board,0,0,false);
        if(owner==0&&!s.clicked){if(Pick(app,packet))s.clicked=true;return true;}
        if(owner==0&&!s.placed){if(!Held(board,s.parentPacket))return true;s.placed=Cell(app);return true;}
        auto* p=At(board);const auto* d=GetNativePlantData(p);if(!d||d->kind!=0)return true;
        if(!Require(app,p->mPlantHealth==300&&d->owner==0&&d->packetId==s.parentPacket&&board->mSunMoney==400&&packet->mRefreshTime==750,"Double ordinary parent purchase differs"))return true;
        s.parent=p->mNetId;if(!Barrier(app,"FD60_PARENT"))return true;
        NetTrace("SMOKE_FUSION_DOUBLE_PARENT role=%s id=%u packet=%u owner=0 price=100 sun=400 sourceCD=750 nativeSDL=1",Role(app),s.parent,s.parentPacket);Advance(app,5);
    }
    else if(s.stage==5)
    {
        if(owner==1&&!s.clicked){s.heldSamples=Samples(app);if(Pick(app,Packet(board,1,0,true)))s.clicked=true;return true;}
        if(owner==1&&!s.placed)
        {
            if(!Held(board,s.extraPacket)||Samples(app)<=s.heldSamples||!Capture(app,"fusion_double_held"))return true;
            if(!Require(app,At(board)&&At(board)->mNetId==s.parent&&board->mSunMoney==400,"Double held echo charged/mutated parent"))return true;
            NetTrace("SMOKE_FUSION_DOUBLE_HELD role=client packet=%u extra=1 price=200 sun=400 snapshotEcho=1 nativeSDL=1",s.extraPacket);
            if(!Send(app,"FD60_HELD"))return true;s.placed=Cell(app);return true;
        }
        if(owner==0&&!Has(app,1,"FD60_HELD"))return true;
        if(!s.purchaseObserved)
        {
            auto* p=At(board);const auto* d=GetNativePlantData(p);if(!d||d->kind!=1030)return true;
            s.intermediate=p->mNetId;if(!Require(app,s.parent&&s.intermediate!=s.parent,"Double intermediate did not replace observed parent"))return true;
            if(!Business(app,false)||!Capture(app,"fusion_double_intermediate"))return true;
            s.purchaseObserved=true;
        }
        // Once this peer has observed/captured1030, retain its ACK even if the
        // host's next authoritative1008 snapshot overtakes the chat echo.
        if(!Barrier(app,"FD60_1030"))return true;
        NetTrace("SMOKE_FUSION_DOUBLE_INTERMEDIATE role=%s old=%u id=%u packet=%u owner=1 extra=1 parents=0,0 price=200 sun=200 sourceCD=750 bothPeerAck=1",Role(app),s.parent,s.intermediate,s.extraPacket);Advance(app,6);
    }
    else if(s.stage==6)
    {
        if(s.kind==1008)
        {
            if(owner==0&&!s.clicked){if(Pick(app,Packet(board,0,2,false)))s.clicked=true;return true;}
            if(owner==0&&!s.placed){if(!Held(board,s.cherryPacket))return true;s.placed=Cell(app);return true;}
            auto* p=At(board);const auto* d=GetNativePlantData(p);if(!d||d->kind!=1008)return true;s.finalPlant=p->mNetId;
            if(!Require(app,s.finalPlant!=s.intermediate,"DoubleCherry did not replace actual1030"))return true;
        }
        else s.finalPlant=s.intermediate;
        if(!Business(app,true))return true;
        if(!s.purchaseObserved){s.purchaseSamples=Samples(app);s.purchaseObserved=true;}
        if(!Capture(app,"fusion_double_fused")||!Barrier(app,"FD60_FINAL"))return true;
        NetTrace("SMOKE_FUSION_DOUBLE_PURCHASE role=%s kind=%d parent=%u intermediate=%u final=%u packet=%u owner=%d parents=%d,%d sun=%d sourceCD=%d noTemporaryCherry=1 nativeSDL=1",Role(app),s.kind,s.parent,s.intermediate,s.finalPlant,s.kind==1008?s.cherryPacket:s.extraPacket,s.kind==1008?0:1,s.kind==1008?1030:0,s.kind==1008?2:0,FinalSun(),s.kind==1008?5000:750);Advance(app,7);
    }
    else if(s.stage==7)
    {
        if(!Business(app,true))return true;
        if(!s.purchaseObserved)
        {
            if(!Require(app,board->mProjectiles.mSize==0&&board->mZombies.mSize==0,"Double fired without an acquired target"))return true;
            s.purchaseObserved=true;
        }
        if(!Barrier(app,"FD60_SETUP"))return true;
        if(app->NetIsHost()&&!s.setup&&!Setup(app))return true;
        Advance(app,8);
    }
    else if(s.stage==8)
    {
        if(app->NetIsHost()&&!Send(app,Ids()))return true;
        if(!ReadIds(app))return true;
        for(auto id:s.targets){auto* z=Target(board,id);if(!z||z->IsDeadOrDying())return true;}
        if(!Barrier(app,"FD60_TARGETS"))return true;
        if(app->NetIsHost())
        {
            for(size_t i=0;i<s.targets.size();++i)MoveX(Target(board,s.targets[i]),float(kTargetX[i]));s.active=true;
            s.previousCounter=At(board)->mShootingCounter;s.previousTick=board->mMainCounter;
            NetTrace("SMOKE_FUSION_DOUBLE_ACTIVATE front=%u rear=%u adjacent=%u frontX=460 rearX=560 HP=5000 positionOnly=1 timerWrites=0",s.targets[0],s.targets[1],s.targets[2]);
        }
        Advance(app,9);
    }
    else if(s.stage==9)
    {
        if(s.hits<4||!s.frames.contains("fusion_double_projectile")||!Capture(app,"fusion_double_combat"))return true;
        if(!Require(app,app->NetIsHost()||(s.clientActive&&s.clientProjectile),"Double client missed authoritative projectile/combat presentation"))return true;
        if(!Barrier(app,"FD60_COMBAT"))return true;Advance(app,10);
    }
    else if(s.stage==10)
    {
        if(!Require(app,s.hits==4,"Double final hit count changed"))return true;
        if(Samples(app)<8||Samples(app)-s.purchaseSamples<2||!Barrier(app,"FD60_DONE")||!Capture(app,"fusion_double_complete"))return true;
        NetTrace("SMOKE_FUSION_DOUBLE_COMPLETE role=%s kind=%d final=%u intermediate=%u hits=4 damage=%d front=%d rear=5000 adjacent=5000 sun=%d sourceCD=%d twoEventsPerVolley=1 volleys=2 nativeBoard=1 secondWorld=0 healthWritesAfterSetup=0 timerWrites=0 samples=%u",Role(app),s.kind,s.finalPlant,s.intermediate,Damage(),kHealth-4*Damage(),FinalSun(),s.kind==1008?5000:750,Samples(app));
        Advance(app,11);app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
