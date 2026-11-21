#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../GridItem.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionBatchActionRules.h"
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
using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct PlantSpec {uint16_t kind;int column,row;};
struct TargetSpec {int row;float x;int hp,bite=-1,helm=0,shield=0,fear=-1,spike=-1;};
struct Shot {int age=0,row=0;float x=0,y=0;uint32_t generation=0;uint8_t phase=0;bool hot=false;};
struct State
{
    int group=0,stage=0,waitingOwn=0,introChecks=0,step=0,expectedSun=500,price=0;
    uint32_t since=0,lastChat=0,introFirst=0,introLast=0,epoch=0,oldEpoch=0,pickSamples=0,pauseProof=0;
    bool configured=false,setup=false,before=false,resume=false,ready=false,pause=false,after=false,picked=false,placed=false,entry=false,pauseSampled=false,diagnosed=false;
    std::vector<PlantSpec> plantSpecs;std::vector<TargetSpec> targetSpecs;std::vector<Ref> plants;std::vector<uint16_t> targets;
    std::array<bool,2> feared{},recovered{};bool close=false,closeDead=false;int doubleShots=0;uint8_t colorMask=0;
    unsigned originalPhases=0,hotPhases=0,childPhases=0;bool curve=false;std::array<int,3> entryBefore{};
    Ref child{};std::string ids;std::map<uint16_t,Shot> shots;
    std::set<uint16_t> preEntryShots,childShots;std::set<std::string> frames;std::map<std::string,int> firstDraw;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();if(stage==5||stage==9)s.pauseSampled=false;NetTrace("SMOKE_FUSION_ACTION_STAGE role=%s group=%d stage=%d epoch=%u",Role(a),s.group,stage,s.epoch);}
std::string Tag(const char* name){return "FA197_"+std::string(name)+"_"+std::to_string(s.group)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int who,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;s.lastChat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){const auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Action owned SDL Escape failed");}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Action owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_action_"+std::to_string(s.group)+"_"+suffix;
    if(s.frames.contains(name))return true;if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Action actual renderer capture failed"))return false;s.frames.insert(name);NetTrace("SMOKE_FUSION_ACTION_RENDER role=%s group=%d frame=%s freshDraws=%d",Role(a),s.group,name.c_str(),a->mDrawCount-s.firstDraw[name]);return true;
}
Plant* Actor(Board* b,Ref ref){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int column,int row,uint16_t kind)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==column&&p->mRow==row&&d&&d->kind==kind)return p;}return nullptr;}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id&&!z->mDead)return z;return nullptr;}
bool Retired(Board* b,uint16_t id){auto* z=Target(b,id);return !z||z->IsDeadOrDying()||z->mBodyHealth<=0;}
void DiagnoseWaitingAction(LawnApp* a)
{
    if(s.diagnosed||GetTickCount()-s.since<5000)return;s.diagnosed=true;auto* b=a->mBoard;
    NetTrace("SMOKE_FUSION_ACTION_WAIT role=%s group=%d epoch=%u originalPhases=%u hotPhases=%u childPhases=%u feared=%d,%d recovered=%d,%d close=%d closeDead=%d farDouble=%d colorMask=%u",Role(a),s.group,s.epoch,s.originalPhases,s.hotPhases,s.childPhases,s.feared[0]?1:0,s.feared[1]?1:0,s.recovered[0]?1:0,s.recovered[1]?1:0,s.close?1:0,s.closeDead?1:0,s.doubleShots,s.colorMask);
    for(size_t i=0;i<s.plants.size();++i)
    {
        auto* p=Actor(b,s.plants[i]);const auto* d=GetNativePlantData(p);float x=0,y=0;const bool ground=p&&p->GetShadowCenter(x,y);
        NetTrace("SMOKE_FUSION_ACTION_WAIT_PLANT role=%s group=%d index=%zu kind=%u id=%u alive=%d state=%d launch=%d shooting=%d shootSeconds=%.9g ground=%d groundX=%.6g groundY=%.6g",Role(a),s.group,i,s.plantSpecs[i].kind,s.plants[i].id,p?1:0,p?int(p->mState):-1,p?p->mLaunchCounter:0,p?p->mShootingCounter:0,d?d->shootSeconds:-1,ground?1:0,x,y);
    }
    for(size_t i=0;i<s.targets.size();++i)
    {auto* z=Target(b,s.targets[i]);NetTrace("SMOKE_FUSION_ACTION_WAIT_TARGET role=%s group=%d index=%zu id=%u alive=%d body=%d max=%d hasHead=%d",Role(a),s.group,i,s.targets[i],z?1:0,z?z->mBodyHealth:0,z?z->mBodyMaxHealth:0,z&&z->mHasHead?1:0);}
}
NativePlantData Provenance(uint16_t kind)
{
    NativePlantData data;data.kind=kind;if(IsNativeBasePlantKind(kind)||kind==1057||kind==1155)return data;
    for(size_t i=0;i<GetFusionRecipeCount();++i){const auto& r=GetFusionRecipes()[i];if(r.result==kind&&r.existing!=kind&&r.used!=kind){data.firstParent=r.existing;data.secondParent=r.used;break;}}
    return data;
}
void Specs()
{
    if(!s.plantSpecs.empty())return;
    if(s.group==0){s.plantSpecs={{8,2,0},{1022,2,2},{1024,2,4}};s.targetSpecs={{0,0,800,0},{2,0,800,1,400,600},{4,0,800,2}};}
    if(s.group==1){s.plantSpecs={{13,2,0},{1057,7,1},{17,2,3},{1155,7,4}};s.targetSpecs={{0,220,5000},{1,580,5000},{3,220,1000},{4,580,1000}};}
    if(s.group==2){s.plantSpecs={{11,2,1},{16,8,4}};s.targetSpecs={{1,220,3000},{0,220,3000},{4,724,3000},{3,724,3000}};}
    if(s.group==3){s.plantSpecs={{9,1,0},{1024,1,2},{1025,1,4},{17,2,0},{17,2,2}};s.targetSpecs={{0,220,60,-1,0,0,0,3},{0,560,3000},{2,220,60,-1,0,0,1,4},{2,560,3000},{4,300,20},{4,680,3000}};}
    if(s.group==4){s.plantSpecs={{14,1,2},{18,4,1},{18,4,2},{18,4,3}};s.targetSpecs={{1,520,5000},{2,520,5000},{3,520,5000}};}
    s.plants.resize(s.plantSpecs.size());s.targets.resize(s.targetSpecs.size());
}
void Center(Zombie* z,float x){const auto r=z->GetZombieRect();z->mPosX+=x-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(a->NetIsHost()&&!s.setup)
    {
        if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Action initial setup requires fresh unpaused native activity"))return false;
        b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
        for(size_t i=0;i<s.plantSpecs.size();++i){const auto p=s.plantSpecs[i];const auto data=Provenance(p.kind);auto* actor=b->AddPlant(p.column,p.row,GetNativeSeedForPlant(p.kind),SEED_NONE,&data);const auto* d=GetNativePlantData(actor);if(!Require(a,actor&&d&&d->instanceToken,"Action initial native plant factory failed"))return false;s.plants[i]={actor->mNetId,d->instanceToken};}
        for(size_t i=0;i<s.targetSpecs.size();++i)
        {
            const auto p=s.targetSpecs[i];auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,p.row,0);if(!Require(a,z&&z->mNetId,"Action initial native target factory failed"))return false;
            z->mPosY=z->GetPosYBasedOnRow(p.row);z->mY=int(z->mPosY);z->mBodyHealth=z->mBodyMaxHealth=p.hp;
            if(p.helm){z->mHelmType=HELMTYPE_BUCKET;z->AttachHelmet();z->mHelmHealth=z->mHelmMaxHealth=p.helm;}
            if(p.shield){z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();z->mShieldHealth=z->mShieldMaxHealth=p.shield;}
            if(p.bite>=0)
            {
                auto* actor=Actor(b,s.plants[p.bite]);bool found=false;
                for(int offset=80;offset>=-80;--offset){z->mPosX=float(actor->mX+offset);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==actor){found=true;break;}}
                if(!Require(a,found,"Action actual native bite rectangle could not target its source plant"))return false;
            }
            else
            {
                z->mVelX=0;z->ApplyAnimRate(0);Center(z,p.x);
                if(p.fear>=0)
                {
                    auto* fear=Actor(b,s.plants[p.fear]);auto* spike=Actor(b,s.plants[p.spike]);float fx=0,fy=0,sx=0,sy=0;FusionBatchArea fa,sa;bool found=false;
                    if(!Require(a,fear&&spike&&fear->GetShadowCenter(fx,fy)&&spike->GetShadowCenter(sx,sy)&&TryMakeFusionBatchArea(s.plantSpecs[p.fear].kind,fx,fy,false,fa)&&TryMakeFusionBatchArea(17,sx,sy,false,sa),"Action fear/spike setup geometry unavailable"))return false;
                    for(int dx=0;dx<=45;++dx){Center(z,sx+dx);const auto r=z->GetZombieRect();if(FusionBatchAreaTouchesRect(fa,r.mX,r.mY,r.mWidth,r.mHeight)&&FusionBatchAreaTouchesRect(sa,r.mX,r.mY,r.mWidth,r.mHeight)&&!z->FindPlantTarget(ATTACKTYPE_CHEW)){found=true;break;}}
                    if(!Require(a,found,"Action near control must touch fear/spike without biting its shooter"))return false;
                }
            }
            s.targets[i]=z->mNetId;
            NetTrace("SMOKE_FUSION_ACTION_TARGET_SETUP group=%d index=%zu id=%u row=%d HP=%d helmet=%d shield=%d nativeChew=%d stationary=%d initialOnly=1",s.group,i,z->mNetId,p.row,p.hp,p.helm,p.shield,p.bite>=0?1:0,p.bite<0?1:0);
        }
        std::ostringstream ids;ids<<Tag("IDS");for(auto id:s.targets)ids<<' '<<id;s.ids=ids.str();s.setup=true;
        NetTrace("SMOKE_FUSION_ACTION_SETUP group=%d epoch=%u plants=%zu targets=%zu initialFactoryOnly=1 HPAndVelocityInitialOnly=1 forcedRNG=0 postSetupActorWrites=0",s.group,s.epoch,s.plants.size(),s.targets.size());
        if(!Key(a))return false;
    }
    if(a->NetIsHost())return Send(a,s.ids);
    const auto prefix=Tag("IDS")+" ";for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind(prefix,0)==0)
    {std::istringstream ids(line.mText.substr(prefix.size()));for(auto& id:s.targets){unsigned value=0;if(!(ids>>value)||!value||value>65535)return Require(a,false,"Action source target ID payload invalid");id=uint16_t(value);}return true;}
    return false;
}
bool Before(LawnApp* a)
{
    auto* b=a->mBoard;
    for(size_t i=0;i<s.plantSpecs.size();++i)
    {
        const auto p=s.plantSpecs[i];auto* actor=At(b,p.column,p.row,p.kind);const auto* d=GetNativePlantData(actor);const auto expected=Provenance(p.kind);const auto* definition=FusionFindPlant(p.kind);
        if(!Require(a,actor&&d&&definition&&actor->mSeedType==GetNativeSeedForPlant(p.kind)&&actor->mPlantHealth==definition->maxHealth&&actor->mPlantMaxHealth==definition->maxHealth&&d->owner==255&&!d->packetId&&!d->randomOrigin&&d->firstParent==expected.firstParent&&d->secondParent==expected.secondParent&&d->instanceToken,"Action actual source plant identity/alias/HP/ancestry differs before combat"))return false;
        s.plants[i]={actor->mNetId,d->instanceToken};
        NetTrace("SMOKE_FUSION_ACTION_PLANT role=%s group=%d epoch=%u index=%zu kind=%u id=%u token=%u alias=%d HP=%d owner=255 packet=0 parents=%u,%u",Role(a),s.group,s.epoch,i,p.kind,actor->mNetId,d->instanceToken,int(actor->mSeedType),actor->mPlantHealth,d->firstParent,d->secondParent);
    }
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=Target(b,s.targets[i]);const auto p=s.targetSpecs[i];
        if(!Require(a,z&&z->mZombieType==ZOMBIE_NORMAL&&!z->mMindControlled&&z->mBodyHealth==p.hp&&z->mBodyMaxHealth==p.hp&&z->mHelmHealth==p.helm&&z->mHelmMaxHealth==p.helm&&z->mShieldHealth==p.shield&&z->mShieldMaxHealth==p.shield&&z->mScaleZombie==1,"Action actual native target before-state differs across peers"))return false;
        NetTrace("SMOKE_FUSION_ACTION_NPC role=%s group=%d epoch=%u when=before index=%zu id=%u body=%d max=%d helm=%d helmMax=%d shield=%d shieldMax=%d scale=%.6g controlled=0",Role(a),s.group,s.epoch,i,z->mNetId,z->mBodyHealth,z->mBodyMaxHealth,z->mHelmHealth,z->mHelmMaxHealth,z->mShieldHealth,z->mShieldMaxHealth,z->mScaleZombie);
    }
    return true;
}
void Projectiles(LawnApp* a)
{
    auto* b=a->mBoard;Projectile* p=nullptr;
    while(b->mProjectiles.IterateNext(p))
    {
        const auto* d=GetNativeProjectileData(p);if(p->mDead||!d)continue;
        if(s.group==3&&!Require(a,(d->kind==9||d->kind==13)&&d->damage==20&&!d->sunOnHit&&p->mProjectileType==PROJECTILE_PUFF&&p->mMotionType==MOTION_STRAIGHT&&d->effectVariant<=6,"Action ranged source projectile tuple differs"))return;
        auto old=s.shots.find(p->mNetId);
        if(old==s.shots.end())
        {
            s.shots[p->mNetId]={p->mProjectileAge,p->mRow,p->mPosX,p->mPosY,b->mProjectiles.DataArrayGetID(p),d->specialType,d->fireState==1};
            NetTrace("SMOKE_FUSION_ACTION_PROJECTILE role=%s group=%d epoch=%u id=%u payload=%u row=%d phase=%u variant=%u damage=%d type=%d motion=%d hostPhysics=%d",Role(a),s.group,s.epoch,p->mNetId,d->kind,p->mRow,d->specialType,d->effectVariant,d->damage,int(p->mProjectileType),int(p->mMotionType),a->NetIsHost()?1:0);
            if(s.group==3&&p->mRow==4&&d->kind==9)++s.doubleShots;
        }
        if(s.group==4&&d->kind==0)
        {
            if(!Require(a,d->specialType==0||d->specialType==3||d->specialType==4,"Action Three source lane phase differs"))return;
            const unsigned bit=1u<<d->specialType;if(!d->fireState)s.originalPhases|=bit;
            else
            {
                if(!Require(a,d->damage==40&&p->mProjectileType==PROJECTILE_FIREBALL&&d->lastTorchId&&d->lastTorchToken&&p->mRow>=1&&p->mRow<=3,"Action Three Torch source tuple differs"))return;
                const auto torch=s.plants[p->mRow];if(!Require(a,d->lastTorchId==torch.id&&d->lastTorchToken==torch.token,"Action heated Three lost the actual Torch identity"))return;
                s.hotPhases|=bit;
                if(s.child.id&&!s.preEntryShots.contains(p->mNetId))
                {
                    s.childPhases|=bit;
                    if(s.childShots.insert(p->mNetId).second)NetTrace("SMOKE_FUSION_ACTION_CHILD_SHOT role=%s epoch=%u child=%u projectile=%u phase=%u hot=1 firstObservedAfterEntry=1",Role(a),s.epoch,s.child.id,p->mNetId,d->specialType);
                }
                if(old!=s.shots.end()&&a->NetIsHost()&&!Require(a,old->second.generation==b->mProjectiles.DataArrayGetID(p),"Action yellow Torch conversion allocated a different projectile"))return;
                if(old==s.shots.end()||!old->second.hot)NetTrace("SMOKE_FUSION_ACTION_HOT role=%s epoch=%u id=%u phase=%u damage=40 motion=%d torch=%u token=%u sameNativeActor=%d",Role(a),s.epoch,p->mNetId,d->specialType,int(p->mMotionType),d->lastTorchId,d->lastTorchToken,a->NetIsHost()?1:0);
            }
            if(a->NetIsHost()&&old!=s.shots.end()&&(d->specialType==3||d->specialType==4))
            {
                const auto last=old->second;const int dt=p->mProjectileAge-last.age;double dy=0;
                for(int age=last.age+1;age<=p->mProjectileAge;++age){const double t=(age-1)*.01;if(t<.5)dy+=(d->specialType==3?-1:1)*(7-16*t)*(100.0/1.67)*.01;}
                if(!Require(a,dt>=0&&std::abs(p->mPosX-last.x-3.2*dt)<.08&&std::abs(p->mPosY-last.y-dy)<.12,"Action Three actual native diagonal displacement differs from source formula"))return;
                if(dt>0&&last.age<45)s.curve=true;
            }
            s.shots[p->mNetId]={p->mProjectileAge,p->mRow,p->mPosX,p->mPosY,b->mProjectiles.DataArrayGetID(p),d->specialType,d->fireState==1};
        }
        if((s.group==3&&(d->kind==9||d->kind==13))||(s.group==4&&d->fireState))Capture(a,"projectiles");
    }
}
SeedPacket* JalaCard(Board* b)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==0&&d->kind==16&&!d->isExtra)return p;}return nullptr;}
bool Three(LawnApp* a)
{
    auto* b=a->mBoard;auto* card=JalaCard(b);constexpr unsigned all=1u|(1u<<3)|(1u<<4);
    if(!Require(a,card!=nullptr,"Action paid Jalapeno source packet missing"))return false;
    if(s.step==0)
    {
        if((s.originalPhases&all)!=all||(s.hotPhases&all)!=all||(a->NetIsHost()&&!s.curve)||!Capture(a,"three14"))return false;
        if(!Barrier(a,"THREE14"))return false;s.step=1;
    }
    auto* child=At(b,1,2,1058);
    if(!child&&a->NetIsHost())
    {
        if(!s.picked){auto* bank=b->mSeedBank;if(!Require(a,card->mActive&&!card->mRefreshing,"Action real Jalapeno card not ready"))return false;s.pickSamples=Samples(a);s.picked=BoardClick(a,float(bank->mX+card->mX+card->mOffsetX+card->mWidth/2),float(bank->mY+card->mY+card->mHeight/2));return false;}
        const auto* cursor=b->mCursorObject;
        if(cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=card->mNetId)return false;
        if(!s.placed)
        {
            Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))if(!shot->mDead)return false;
            auto* original=Actor(b,s.plants[0]);if(!original||original->mShootingCounter<=20)return false;
            for(int i=0;i<3;++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth>1800,"Action strong entry control lost its baseline"))return false;s.entryBefore[i]=z->mBodyHealth;}
            const auto* data=GetNativePacketData(card);const auto* definition=FusionFindPlant(16);s.price=data->price;
            if(!Require(a,s.price==definition->sunCost&&b->mSunMoney==500,"Action entry paid price/source balance differs"))return false;
            NetTrace("SMOKE_FUSION_ACTION_ENTRY_DRAIN epoch=%u liveProjectiles=0 naturalShootingCounter=%d oldShooter=%u",s.epoch,original->mShootingCounter,original->mNetId);
            s.placed=BoardClick(a,float(b->GetCellCenterX(1,2)),float(b->GridToPixelY(1,2)+50));return false;
        }
        return false;
    }
    if(!child)return false;const auto* d=GetNativePlantData(child);
    if(!s.child.id)
    {
        if(!Require(a,d&&d->firstParent==14&&d->secondParent==16&&d->owner==0&&d->packetId==card->mNetId&&!d->randomOrigin&&d->instanceToken&&child->mPlantHealth==300&&!Actor(b,s.plants[0])&&child->mNetId!=s.plants[0].id,"Action real14+16 fusion identity/ancestry/owner differs"))return false;
        for(const auto& [id,shot]:s.shots)s.preEntryShots.insert(id);
        s.child={child->mNetId,d->instanceToken};s.expectedSun=500-GetNativePacketData(card)->price;
        if(!Require(a,b->mSunMoney==s.expectedSun&&card->mRefreshTime==int(std::lround(GetNativePacketData(card)->rechargeSeconds*100))&&card->mRefreshing&&!card->mActive,"Action real paid entry missed charge/cooldown"))return false;
        if(a->NetIsHost())for(int i=0;i<3;++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth==s.entryBefore[i]-1800,"Action paid1058 entry did not apply exactly1800 to all three rows"))return false;NetTrace("SMOKE_FUSION_ACTION_ENTRY_DAMAGE target=%u row=%d before=%d after=%d delta=1800",z->mNetId,z->mRow,s.entryBefore[i],z->mBodyHealth);}
        NetTrace("SMOKE_FUSION_ACTION_PAID_ENTRY role=%s epoch=%u kind=1058 id=%u token=%u parents=14,16 owner=0 packet=%u cost=%d sun=%d sourceCooldown=%d nativeSDL=1 hostDamageProof=%d",Role(a),s.epoch,child->mNetId,d->instanceToken,card->mNetId,GetNativePacketData(card)->price,b->mSunMoney,card->mRefreshTime,a->NetIsHost()?1:0);
        Capture(a,"entry1058");
    }
    return (s.childPhases&all)==all&&Capture(a,"entry1058")&&Capture(a,"three1058");
}
bool Battle(LawnApp* a)
{
    auto* b=a->mBoard;Projectiles(a);if(a->mCoopSmokeResultWritten)return false;
    if(s.group==0)
    {
        for(int i=0;i<3;++i)
        {
            auto* z=Target(b,s.targets[i]);if(!z||!z->mMindControlled)return false;
            if(i==1){const auto* d=GetNativeZombieActionData(z);if(!Require(a,z->mBodyHealth==400&&z->mBodyMaxHealth==400&&z->mHelmHealth==200&&z->mHelmMaxHealth==200&&z->mShieldHealth==300&&z->mShieldMaxHealth==300&&z->mScaleZombie==.5f&&d&&d->scale==.5f,"Action actual HypnoPuff half health/armor/max/scale mismatch"))return false;}
            else if(!Require(a,z->mBodyHealth==800&&z->mBodyMaxHealth==800,"Action ordinary hypno changed body health"))return false;
            if(Actor(b,s.plants[i]))return false;
        }
        auto* replacement=At(b,2,4,9);const auto* data=GetNativePlantData(replacement);
        return data&&data->owner==255&&!data->packetId&&!data->randomOrigin&&replacement->mPlantHealth==300&&Capture(a,"hypno");
    }
    if(s.group==1)
    {
        for(int i=0;i<2;++i)if(auto* p=Actor(b,s.plants[i]))if(p->mState!=STATE_NOTREADY)Capture(a,i?"stomp1057":"stomp13");
        for(int i=0;i<4;++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth>0,"Action melee control retired before its measurable damage"))return false;
            const int loss=s.targetSpecs[i].hp-z->mBodyHealth;if(i<2){if(!loss)return false;auto* squash=Actor(b,s.plants[i]);
                if(loss!=1800)NetTrace("SMOKE_FUSION_ACTION_MELEE_DELTA role=%s index=%d target=%u before=%d body=%d loss=%d hasHead=%d squashAlive=%d squashState=%d",Role(a),i,z->mNetId,s.targetSpecs[i].hp,z->mBodyHealth,loss,z->mHasHead?1:0,squash?1:0,squash?int(squash->mState):-1);
                if(!Require(a,loss==1800,"Action Squash actual stomp damage differs"))return false;if(squash)return false;}
            else {if(loss<40)return false;if(!Require(a,loss%20==0,"Action repeated Caltrop damage is not20 per impact"))return false;}}
        return s.frames.contains("fusion_action_1_stomp13")&&s.frames.contains("fusion_action_1_stomp1057")&&Capture(a,"melee");
    }
    if(s.group==2)
    {
        if(Actor(b,s.plants[0])||Actor(b,s.plants[1]))return false;
        for(int i=0;i<4;++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth==(i==3?3000:1200),"Action Doom/Jalapeno area/row1800 or outside control differs"))return false;}
        auto* crater=b->GetCraterAt(2,1);if(!Require(a,crater&&!crater->mDead&&crater->mGridItemCounter>0,"Action natural Doom lacks native crater"))return false;
        return Capture(a,"instant");
    }
    if(s.group==3)
    {
        for(int i=0;i<2;++i){auto* p=Actor(b,s.plants[i]);if(!Require(a,p!=nullptr,"Action scared shooter was bitten during its controlled fear probe"))return false;if(p->mState==STATE_SCAREDYSHROOM_SCARED){s.feared[i]=true;Capture(a,i?"fear1024":"fear9");}else if(s.feared[i]&&Retired(b,s.targets[i*2]))s.recovered[i]=true;}
        auto* adaptive=Actor(b,s.plants[2]);if(adaptive&&adaptive->mState==STATE_KERNELPULT_BUTTER){s.close=true;Capture(a,"close1025");}
        s.closeDead|=Retired(b,s.targets[4]);auto* colorful=Target(b,s.targets[3]);const auto* color=GetNativeZombieActionData(colorful);if(color)s.colorMask=color->colorMask;
        bool basic=false;for(const auto& [id,shot]:s.shots)basic|=shot.row==0;
        return s.feared[0]&&s.feared[1]&&s.recovered[0]&&s.recovered[1]&&basic&&s.colorMask&&s.close&&s.closeDead&&s.doubleShots>=2&&
            s.frames.contains("fusion_action_3_fear9")&&s.frames.contains("fusion_action_3_fear1024")&&s.frames.contains("fusion_action_3_close1025")&&Capture(a,"ranged");
    }
    return Three(a);
}
bool After(LawnApp* a)
{
    if(!Capture(a,"after"))return false;
    auto* b=a->mBoard;
    for(size_t i=0;i<s.targets.size();++i)
    {
        auto* z=Target(b,s.targets[i]);const auto* data=GetNativeZombieActionData(z);
        NetTrace("SMOKE_FUSION_ACTION_NPC role=%s group=%d epoch=%u when=after index=%zu id=%u alive=%d body=%d max=%d helm=%d helmMax=%d shield=%d shieldMax=%d scale=%.6g controlled=%d mask=%u",Role(a),s.group,s.epoch,i,s.targets[i],z?1:0,z?z->mBodyHealth:0,z?z->mBodyMaxHealth:0,z?z->mHelmHealth:0,z?z->mHelmMaxHealth:0,z?z->mShieldHealth:0,z?z->mShieldMaxHealth:0,z?z->mScaleZombie:0,z&&z->mMindControlled?1:0,data?data->colorMask:0);
    }
    if(s.group==3)NetTrace("SMOKE_FUSION_ACTION_RANGED role=%s fear9=%d recovered9=%d fear1024=%d recovered1024=%d colorMask=%u fullSeven=%d close1025=%d closeDead=%d farDoubleShots=%d rareSevenRequired=0",Role(a),s.feared[0]?1:0,s.recovered[0]?1:0,s.feared[1]?1:0,s.recovered[1]?1:0,s.colorMask,s.colorMask==127?1:0,s.close?1:0,s.closeDead?1:0,s.doubleShots);
    return true;
}
void NextGroup()
{
    s.oldEpoch=s.epoch;s.epoch=0;++s.group;s.waitingOwn=s.introChecks=s.step=0;s.expectedSun=500;s.introFirst=s.introLast=0;
    s.setup=s.before=s.resume=s.ready=s.pause=s.after=s.picked=s.placed=s.entry=s.pauseSampled=s.diagnosed=false;s.plantSpecs.clear();s.targetSpecs.clear();s.plants.clear();s.targets.clear();s.shots.clear();s.preEntryShots.clear();s.childShots.clear();s.ids.clear();
    s.feared={};s.recovered={};s.close=s.closeDead=s.curve=false;s.doubleShots=0;s.colorMask=0;s.originalPhases=s.hotPhases=s.childPhases=0;s.child={};
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionAction(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_ACTION"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>(s.stage==7?45000U:30000U)){char why[110];sprintf_s(why,"Action deadline group=%d stage=%d role=%s",s.group,s.stage,Role(a));a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FA197_BEGIN");}else if(Has(a,1,"FA197_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    // Finish the old activity without dereferencing its Board: START may already
    // have replaced it on the client before the next fixture update.
    if(s.stage==10)
    {
        if(!Barrier(a,"DONE"))return true;NetTrace("SMOKE_FUSION_ACTION_CASE role=%s group=%d epoch=%u nativeBattle=1 initialSetupOnly=1",Role(a),s.group,s.epoch);
        if(s.group==4){NetTrace("SMOKE_FUSION_ACTION_COMPLETE role=%s groups=5 sourceKinds=8,1022,11,16,13,1057,17,1155,14,1058,9,1024,1025 paidEntry1058=1 rareSevenColorControl=NotRequired sourceZombieAIUnported=1 postSetupActorWrites=0 nativeBoard=1 secondWorld=0",Role(a));Advance(a,11);a->CoopSmokeWriteResult(true,nullptr);return true;}
        NextGroup();Advance(a,1);if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.oldEpoch)Send(a,"FA197_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.oldEpoch)return true;
    if(!s.epoch){s.epoch=ctx->epoch;Specs();NetTrace("SMOKE_FUSION_ACTION_EPOCH role=%s group=%d epoch=%u previous=%u",Role(a),s.group,s.epoch,s.oldEpoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Action requires the same new canonical native epoch/models"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Action native fair chooser quota differs"))return true;
        std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].isExtra==(slot==1))choices.push_back(int(i));
        std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return (ctx->eligible[x].kind==16?0:1)<(ctx->eligible[y].kind==16?0:1);});
        if(!Require(a,choices.size()>=5&&ctx->eligible[choices[0]].kind==16,"Action source Jalapeno eligible identity missing"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;
        if(own<5){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()==10)Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Action actual Ready hover failed");}else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500,"Action skipped actual native full intro"))return true;Advance(a,4);
    }
    else if(s.stage==4){if(Setup(a))Advance(a,5);}
    else if(s.stage==5)
    {
        if(!b->mPaused)return true;if(!s.pauseSampled){s.pauseProof=Samples(a);s.pauseSampled=true;}if(Samples(a)-s.pauseProof<2)return true;
        if(!s.before){if(!Before(a))return true;s.before=true;}if(Capture(a,"before"))Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!Barrier(a,"MATRIX"))return true;if(a->NetIsHost()&&!s.resume){s.resume=Key(a);return true;}if(!b->mPaused&&!a->GetDialogCount())Advance(a,7);
    }
    else if(s.stage==7)
    {
        const bool done=Battle(a);if(a->mCoopSmokeResultWritten)return true;
        if(!done)DiagnoseWaitingAction(a);
        if(!Require(a,b->mSunMoney==s.expectedSun,"Action currency changed outside actual paid entry"))return true;
        if(done&&Capture(a,"battle")){s.ready=true;Advance(a,8);}
    }
    else if(s.stage==8)
    {if(!Barrier(a,"BATTLE"))return true;if(a->NetIsHost()&&!s.pause){s.pause=Key(a);return true;}if(b->mPaused)Advance(a,9);}
    else if(s.stage==9)
    {if(!s.pauseSampled){s.pauseProof=Samples(a);s.pauseSampled=true;}if(Samples(a)-s.pauseProof<2)return true;if(!s.after){if(!After(a))return true;s.after=true;}Advance(a,10);}
    return true;
}
}
