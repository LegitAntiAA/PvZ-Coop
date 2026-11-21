#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeShooterBatch.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativePoison.h"
#include "../Fusion/FusionNativeDefender.h"
#include "../Fusion/FusionNativePlacement.h"
#include "../Fusion/FusionBatchActionRules.h"
#include "../CursorObject.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
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
constexpr uint16_t kinds[4][6]={{1172,1158,904,1103,0,0},{1007,919,927,1047,0,0},{902,932,1024,1025,0,0},{1022,1058,1073,1112,0,0}};
constexpr int counts[]={4,4,4,4};
constexpr int targetHP=20000000;
constexpr int armorHP=10000;
struct Ref { uint16_t id=0;uint32_t token=0; };
struct TargetOutcome
{
    bool observed=false,retirementExpected=false,retired=false;
    int body=-1,helm=-1,shield=-1,phase=-1;
    float x=0,velocity=0;bool head=false,eating=false;
};
struct State
{
    int group=0,stage=0,waiting=0,playingFrame=-1,baselineTick=0,introChecks=0;
    uint32_t epoch=0,previousEpoch=0,since=0,chat=0,wheel=0,diagnostic=0,pauseSample=0,introFirst=0,introLast=0;
    bool configured=false,setup=false,pauseRequested=false,resumed=false,finalPause=false,finalLogged=false;
    std::array<Ref,6> actors{};
    std::array<int,6> lastActorHP{};
    std::array<float,6> lastActorX{},lastActorY{};
    std::array<bool,6> phaseSeen{};
    Ref replacementActor;
    std::vector<uint16_t> targets;
    std::map<uint16_t,TargetOutcome> targetOutcomes;
    std::set<uint16_t> payloads,projectiles;
    std::set<std::string> frames;
    std::map<std::string,int> draws;
    bool tier101=false,tier301=false;
    bool crafted=false,picked=false,placed=false,mineDead=false,threeDead=false,hypno=false,fear=false,adaptiveFar=false,adaptiveNear=false,colors=false;
    bool poison=false,beamCold=false,beamControl=false,heat=false,landing=false,pot=false,bite=false,entry=false,replacement=false;
    bool healPicked=false,healPlaced=false,healPaid=false,fireHeal=false,potTiming=false;
    Ref healActor;
    Ref poolDying,poolEdge;
    bool poolDeathDamage=false,poolDeathEater=false,poolDeathObserved=false,poolDeathHeal=false,poolAdjacentShot=false,poolEdgeShot=false;
    int poolDyingHP=300,paidHealTick=-1,poolDeathTick=-1;
    bool adaptiveDamage=false,adaptiveEaten=false,adaptiveRetired=false;
    int lastTallHealth=8000,potBirth=-1;
    int craftSun=10000;
    std::string ids;
} s;
int Count(){return counts[s.group];}
int Column(int i){return s.group==2&&i<2?4:2;}
int Row(int i){constexpr int poolRows[]={0,4,5,1};return s.group==3?poolRows[i]:i;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int next){s.stage=next;s.since=GetTickCount();NetTrace("SMOKE_SHOOTER267_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,next);}
std::string Tag(const char* name){return "A267_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(!Require(a,text.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Shooter267 proof exceeds native chat limit"))return false;
    if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const char* name){const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Shooter267 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Shooter267 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Shooter267 owned SDL chooser scroll failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_shooter267_"+std::to_string(s.group)+"_"+suffix;
    if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requestedDraw=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requestedDraw))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Shooter267 actual native renderer capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_SHOOTER267_RENDER role=%s group=%d epoch=%u frame=%s freshDraws=%d requestedDraw=%d capturedDraw=%d",Role(a),s.group,s.epoch,name.c_str(),a->mDrawCount-s.draws[name],requestedDraw,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* Actor(Board* b,int index)
{
    Plant* p=nullptr;while(b->mPlants.IterateNext(p))
    {
        const auto* d=GetNativePlantData(p);
        if(!p->mDead&&p->mIsOnBoard&&d&&(d->kind==kinds[s.group][index]||(s.group==1&&index==3&&d->kind==14))&&p->mPlantCol==Column(index)&&p->mRow==Row(index))return p;
    }
    return nullptr;
}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
void LogActorLoss(LawnApp* a,int index,const char* reason)
{
    Zombie* nearest=nullptr;Zombie* z=nullptr;float distance=1e30f;
    while(a->mBoard->IterateZombies(z))if(!z->mDead&&z->mRow==Row(index))
    {const float delta=std::abs(z->mPosX-s.lastActorX[index]);if(delta<distance){distance=delta;nearest=z;}}
    NetTrace("SMOKE_SHOOTER267_ACTOR_LOST role=%s group=%d index=%d kind=%u id=%u token=%u lastHP=%d lastX=%.9g lastY=%.9g reason=%s far=%d near=%d damage=%d eater=%d npc=%u npcPhase=%d npcX=%.9g npcY=%.9g npcEating=%d npcMC=%d",
        Role(a),s.group,index,kinds[s.group][index],s.actors[index].id,s.actors[index].token,s.lastActorHP[index],s.lastActorX[index],s.lastActorY[index],reason,
        s.adaptiveFar?1:0,s.adaptiveNear?1:0,s.adaptiveDamage?1:0,s.adaptiveEaten?1:0,nearest?nearest->mNetId:0,nearest?int(nearest->mZombiePhase):-1,
        nearest?nearest->mPosX:0,nearest?nearest->mPosY:0,nearest&&nearest->mIsEating?1:0,nearest&&nearest->mMindControlled?1:0);
}
int InitialTargetBody(size_t index)
{return s.group==0&&index>=4?index<7?1000000:8000000:targetHP;}
bool ObserveTargetOutcomes(LawnApp* a)
{
    for(size_t i=0;i<s.targets.size();++i)
    {
        const auto id=s.targets[i];auto& proof=s.targetOutcomes[id];auto* z=Target(a->mBoard,id);
        if(!z)
        {
            if(!Require(a,proof.observed&&proof.retirementExpected,"Shooter267 target retired without observed damage/status and natural retirement state"))return false;
            if(!proof.retired)
                NetTrace("SMOKE_SHOOTER267_TARGET_RETIRED role=%s group=%d id=%u observedOutcome=1 expectedRetirement=1 lastBody=%d lastHelm=%d lastShield=%d lastPhase=%d lastX=%.9g lastVx=%.9g lastHead=%d lastEating=%d canonicalPresence=0",
                    Role(a),s.group,id,proof.body,proof.helm,proof.shield,proof.phase,proof.x,proof.velocity,proof.head?1:0,proof.eating?1:0);
            proof.retired=true;continue;
        }
        const int initialArmor=s.group==0&&i==1?armorHP:0;
        const bool outcome=z->mBodyHealth<InitialTargetBody(i)||z->mHelmHealth<initialArmor||z->mShieldHealth<initialArmor||z->mMindControlled;
        if(outcome&&!proof.observed)
        {
            proof.observed=true;
            NetTrace("SMOKE_SHOOTER267_TARGET_OUTCOME role=%s group=%d id=%u initialBody=%d body=%d initialArmor=%d helm=%d shield=%d MC=%d observedOutcome=1",
                Role(a),s.group,id,InitialTargetBody(i),z->mBodyHealth,initialArmor,z->mHelmHealth,z->mShieldHealth,z->mMindControlled?1:0);
        }
        const bool naturalRetirement=z->IsDeadOrDying()||!z->mHasHead||z->mMindControlled||(!z->mIsEating&&z->mVelX>0);
        if(proof.observed&&naturalRetirement&&!proof.retirementExpected)
        {
            proof.retirementExpected=true;
            NetTrace("SMOKE_SHOOTER267_TARGET_RETIREMENT_EXPECTED role=%s group=%d id=%u phase=%d x=%.9g vx=%.9g head=%d eating=%d MC=%d observedOutcome=1",
                Role(a),s.group,id,int(z->mZombiePhase),z->mPosX,z->mVelX,z->mHasHead?1:0,z->mIsEating?1:0,z->mMindControlled?1:0);
        }
        proof.body=z->mBodyHealth;proof.helm=z->mHelmHealth;proof.shield=z->mShieldHealth;proof.phase=int(z->mZombiePhase);
        proof.x=z->mPosX;proof.velocity=z->mVelX;proof.head=z->mHasHead;proof.eating=z->mIsEating;
    }
    return true;
}
bool SpawnTarget(LawnApp* a,int row,float center,int health,bool armor=false,bool eater=false,bool walk=false,int maximum=0)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
    if(!Require(a,z&&z->mNetId,"Shooter267 native normal target factory failed"))return false;
    z->mBodyHealth=health;z->mBodyMaxHealth=maximum?maximum:health;z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;
    if(armor)
    {
        z->mHelmType=HELMTYPE_BUCKET;z->mShieldType=SHIELDTYPE_DOOR;
        z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=armorHP;
    }
    z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);const auto r=z->GetZombieRect();
    z->mPosX+=center-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);if(!walk)z->mVelX=0;if(!eater&&!walk)z->ApplyAnimRate(0);
    s.targets.push_back(z->mNetId);
    NetTrace("SMOKE_SHOOTER267_TARGET_SETUP group=%d epoch=%u index=%zu id=%u row=%d hp=%d max=%d helm=%d shield=%d x=%.6g y=%.6g initialOnly=1 nativeNormalControl=1 armorComponentFixture=%d sourceNPCParity=NotExecuted",s.group,s.epoch,s.targets.size()-1,z->mNetId,row,health,z->mBodyMaxHealth,z->mHelmHealth,z->mShieldHealth,z->mPosX,z->mPosY,armor?1:0);
    return true;
}

Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
Plant* Factory(LawnApp* a,int col,int row,uint16_t kind,uint8_t owner)
{
    NativePlantData d;d.kind=kind;d.owner=owner;
    for(size_t n=0;n<GetFusionRecipeCount();++n)if(GetFusionRecipes()[n].result==kind)
    {d.firstParent=GetFusionRecipes()[n].existing;d.secondParent=GetFusionRecipes()[n].used;break;}
    if(!IsNativeBasePlantKind(kind)&&d.firstParent==kFusionNoPlant&&d.secondParent==kFusionNoPlant)d.owner=LawnNet::NET_SHARED_OWNER;
    auto* p=kind==1058?TrySpawnNativeFusionPlant(a->mBoard,col,row,d):
        a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&d);
    Require(a,p&&GetNativePlantData(p)&&p->mNetId&&GetNativePlantData(p)->instanceToken,"Shooter267 typed initial factory failed");return p;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(a->NetIsHost()&&!s.setup)
    {
        if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Shooter267 fresh world required"))return false;
        if(s.group==3&&!Require(a,ConfigureNativeFusionBackground(b,BACKGROUND_3_POOL),"Shooter267 Pool6 configuration refused"))return false;
        constexpr uint16_t all[]={1172,1158,904,1103,1007,919,927,1047,902,932,1024,1025,1022,1058,1073,1112,14,13,0,26,9,1005,1054,16};
        std::string error;const bool loaded=EnsureNativeVisuals(b,all,{},&error);if(!Require(a,loaded,error.c_str()))return false;
        NetTrace("SMOKE_SHOOTER267_PREFLIGHT sourceModels=24 nativeLoad=1 groupedSourcePlants=16 auxiliaryFactoryActorsNotAdditionalAcceptance=1");
        b->mSunMoney=s.craftSun;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
        // All actor writes in this fixture end before the initial peer barrier.
        // Eaters keep their native animation; stationary receivers use the same
        // explicit native normal control as previous grouped smoke fixtures.
        for(int i=0;i<Count();++i)
        {
            const int row=Row(i);const float center=float(b->GetCellCenterX(s.group==2?(i<2?7:i==3?9:5):5,row));
            const bool eater=s.group==3&&(i==0||i==2);
            const float targetX=eater?float(b->GetCellCenterX(2,row)+35):s.group==1&&i==0?float(b->GetCellCenterX(2,row)+44):center;
            if(!SpawnTarget(a,row,targetX,targetHP,s.group==0&&i==1,eater,s.group==2&&i==3))return false;
        }
        if(s.group==3&&(!SpawnTarget(a,3,float(b->GetCellCenterX(5,3)),targetHP)||
            !SpawnTarget(a,5,float(b->GetCellCenterX(8,5)),targetHP,false,true,true)))return false;
        for(int i=0;i<Count();++i)
        {
            const uint16_t kind=s.group==1&&i==3?14:kinds[s.group][i];
            if(!Factory(a,Column(i),Row(i),kind,uint8_t(s.group==1&&i==3?1:i%2)))return false;
        }
        if(s.group==0)
        {
            // Retain the original low-health native controls. They naturally
            // become headless below max/3 and cannot establish904 mind-control.
            for(int c:{6,7,8})
            {
                if(!SpawnTarget(a,2,float(b->GetCellCenterX(c,2)),1000000,false,false,false,targetHP))return false;
            }
            // 40% stays above native headless1/3 while below source MC1/2.
            // The healthy row2 receiver independently retains the Cold branch.
            if(!SpawnTarget(a,2,float(b->GetCellCenterX(4,2)),8000000,false,false,false,targetHP))return false;
        }
        if(s.group==2)
        {
            if(!Factory(a,2,0,1005,0)||!Factory(a,1,0,919,0)||!Factory(a,2,1,0,1)||!Factory(a,0,4,1024,0))return false;
            if(!SpawnTarget(a,4,float(b->GetCellCenterX(3,4)),targetHP))return false;
            // These extra ordinary actors walk naturally into fear/Fume range.
            for(int row:{2})
            {
                auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
                if(!Require(a,z&&z->mNetId,"Shooter267 approach control factory failed"))return false;
                z->mBodyHealth=z->mBodyMaxHealth=targetHP;
                z->mPosX=float(b->GetCellCenterX(9,row));z->mX=int(z->mPosX);
            }
            // Native StartEating resets anim_eat to0; its first event is.14.
            // The next-tick pause therefore binds both actor UIDs before bite.
            if(!SpawnTarget(a,2,float(b->GetCellCenterX(2,2)+35),targetHP,false,true))return false;
            auto* eater=Target(b,s.targets.back());auto* food=Actor(b,2);
            const auto plantRect=food->GetPlantRect(),attackRect=eater->GetZombieAttackRect();
            eater->mPosX+=float(plantRect.mX+plantRect.mWidth-25-attackRect.mX);eater->mX=int(eater->mPosX);
            eater->StartEating();
            const auto* eatAnim=a->ReanimationTryToGet(eater->mBodyReanimID);
            if(!Require(a,eatAnim&&eatAnim->mAnimTime==0&&eatAnim->mAnimRate>0&&eater->mIsEating&&
                .14f*eatAnim->mFrameCount/(.01f*eatAnim->mAnimRate)>2&&eater->FindPlantTarget(ATTACKTYPE_CHEW)==food,
                "Shooter267 initial eater is not bound to1024 with a delayed native bite"))return false;
            NetTrace("SMOKE_SHOOTER267_EATER_SETUP id=%u plant=%u plantKind=1024 column=2 row=2 x=%.9g animationTime=0 firstEvent=.14 firstBiteAfterTicks=%.9g initialOnly=1",eater->mNetId,food->mNetId,eater->mPosX,.14f*eatAnim->mFrameCount/(.01f*eatAnim->mAnimRate));
        }
        if(s.group==3)
        {
            // Actual source callbacks: row4 enters rows3/5/4; row5 enters4/5.
            if(!Factory(a,2,Row(3),26,1)||!Factory(a,4,5,1058,0)||!Factory(a,7,5,919,0))return false;
        }
        std::ostringstream ids;ids<<Tag("IDS");for(auto id:s.targets)ids<<' '<<id;s.ids=ids.str();s.setup=true;s.pauseRequested=Key(a);
    }
    if(a->NetIsHost())Send(a,s.ids);
    else if(!s.setup)
    {
        const auto prefix=Tag("IDS")+" ";
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind(prefix,0)==0)
        {std::istringstream ids(line.mText.substr(prefix.size()));unsigned id=0;while(ids>>id)s.targets.push_back(uint16_t(id));s.setup=true;break;}
    }
    if(!s.setup||!b->mPaused||s.targets.size()!=size_t(s.group==0?8:s.group==2||s.group==3?6:4))return false;
    for(auto id:s.targets)if(!Target(b,id))return false;
    if(s.group==3)
    {
        s.entry=b->mBackground==BACKGROUND_3_POOL&&GetNativeFusionRowCount(b)==6&&
            Target(b,s.targets[0])->mBodyHealth==targetHP&&Target(b,s.targets[3])->mBodyHealth==targetHP&&
            Target(b,s.targets[4])->mBodyHealth==targetHP-1800;
        for(int i:{1,2,5})s.entry&=Target(b,s.targets[i])->mBodyHealth==targetHP-3600;
        if(!Require(a,s.entry,"Shooter267 Pool1058 adjacent/edge entry rows or exact1800/3600 damage differ"))return false;
        auto* dying=At(b,7,5,919);auto* edge=At(b,4,5,1058);const auto* dd=GetNativePlantData(dying);const auto* ed=GetNativePlantData(edge);
        if(!Require(a,dying&&edge&&dd&&ed&&dying->mPlantHealth==300&&dying->mPlantMaxHealth==300,"Shooter267 Pool source919/edge1058 baseline missing"))return false;
        if(!s.poolDying.id)
        {
            s.poolDying={dying->mNetId,dd->instanceToken};s.poolEdge={edge->mNetId,ed->instanceToken};
            NetTrace("SMOKE_SHOOTER267_POOL_ENTRY role=%s row3Damage=1800 row4Damage=3600 row5Damage=3600 outsideRowsDamage=0 main1058Row=4 edge1058=%u edgeToken=%u death919=%u deathToken=%u deathHP=300 initialOnly=1",Role(a),edge->mNetId,ed->instanceToken,dying->mNetId,dd->instanceToken);
        }
    }
    for(int i=0;i<Count();++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!s.actors[i].id)
        {s.actors[i]={p->mNetId,d->instanceToken};s.lastActorHP[i]=p->mPlantHealth;s.lastActorX[i]=float(p->mX);s.lastActorY[i]=float(p->mY);NetTrace("SMOKE_SHOOTER267_BOUND role=%s group=%d kind=%u id=%u token=%u initialOnly=1",Role(a),s.group,d->kind,p->mNetId,d->instanceToken);}
        if(s.group==2&&i==3&&!Require(a,p->mPlantHealth==300&&p->mPlantMaxHealth==300,"Shooter267 adaptive Fume sourceHP300 differs at baseline"))return false;
    }
    return true;
}
bool Craft(LawnApp* a)
{
    if(s.group!=1||s.crafted)return true;auto* b=a->mBoard;auto* p=Actor(b,3);const auto* d=GetNativePlantData(p);
    SeedPacket* card=nullptr;for(int i=0;i<b->mSeedBank->mNumPackets;++i)
    {auto* next=&b->mSeedBank->mSeedPackets[i];const auto* data=GetNativePacketData(next);if(data&&data->owner==1&&data->kind==13)card=next;}
    if(!Require(a,card&&p&&d,"Shooter267 paid recipe source/card missing"))return false;
    if(d->kind==1047)
    {
        const auto* price=FusionFindPlant(13);
        if(!Require(a,p->mNetId!=s.actors[3].id&&d->instanceToken!=s.actors[3].token&&d->firstParent==14&&d->secondParent==13&&d->owner==1&&d->packetId==card->mNetId&&price&&b->mSunMoney==s.craftSun-price->sunCost&&card->mRefreshing,"Shooter267 actual paid client recipe/provenance/cost/CD differs"))return false;
        s.crafted=true;s.actors[3]={p->mNetId,d->instanceToken};
        NetTrace("SMOKE_SHOOTER267_PAID role=%s existing=14 used=13 result=1047 newId=%u token=%u clientSDL=1 cost=%d sun=%d",Role(a),p->mNetId,d->instanceToken,price->sunCost,b->mSunMoney);return true;
    }
    if(a->NetIsClient())
    {
        if(!s.picked){if(!card->mActive||card->mRefreshing)return true;s.picked=Click(a,float(b->mSeedBank->mX+card->mX+card->mOffsetX+25),float(b->mSeedBank->mY+card->mY+35));}
        else if(!s.placed&&b->mCursorObject&&b->mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK)
            s.placed=Click(a,float(b->GetCellCenterX(2,3)),float(b->GridToPixelY(2,3)+50));
    }
    return true;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;if(!Craft(a))return false;
    if(s.group==3)
    {
        auto* dying=At(b,7,5,919);const auto* dd=GetNativePlantData(dying);
        if(dying)
        {
            if(!Require(a,dd&&dying->mNetId==s.poolDying.id&&dd->instanceToken==s.poolDying.token,"Shooter267 Pool source919 identity changed"))return false;
            if(dying->mPlantHealth<s.poolDyingHP)
            {
                auto* eater=Target(b,s.targets[5]);const bool eating=eater&&!eater->mMindControlled&&eater->mIsEating&&eater->FindPlantTarget(ATTACKTYPE_CHEW)==dying;
                s.poolDeathDamage=true;s.poolDeathEater|=eating;
                if(eating&&!s.poolDeathObserved)NetTrace("SMOKE_SHOOTER267_POOL_919_BITE role=%s id=%u token=%u hp=%d->%d eater=%u actualEating=1",Role(a),dying->mNetId,dd->instanceToken,s.poolDyingHP,dying->mPlantHealth,eater->mNetId);
            }
            s.poolDyingHP=dying->mPlantHealth;
        }
        else if(!s.poolDeathObserved)
        {
            if(!Require(a,s.poolDeathDamage&&s.poolDeathEater&&s.healPaid&&s.fireHeal&&b->mMainCounter>s.paidHealTick,
                "Shooter267 Pool919 lost before real bite and distinct earlier paid16 heal"))return false;
            s.poolDeathObserved=true;s.poolDeathTick=b->mMainCounter;
            NetTrace("SMOKE_SHOOTER267_POOL_919_DEATH role=%s id=%u token=%u lastHP=%d eater=%u paid16=%u paidHealTick=%d deathTick=%d naturalBite=1",Role(a),s.poolDying.id,s.poolDying.token,s.poolDyingHP,s.targets[5],s.healActor.id,s.paidHealTick,s.poolDeathTick);
        }
    }
    for(int i=0;i<Count();++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);
        if(!p||!d)
        {
            if(s.group==1&&i==0){s.mineDead=true;s.phaseSeen[i]=true;continue;}
            if(s.group==3&&i==0){s.phaseSeen[i]=true;continue;}
            if(s.group==2&&i==2)
            {
                auto* fresh=At(b,Column(i),Row(i),9);const auto* fd=GetNativePlantData(fresh);auto* eater=Target(b,s.targets.back());
                if(fresh&&fd&&fresh->mNetId!=s.actors[i].id&&fd->instanceToken&&fd->instanceToken!=s.actors[i].token&&eater&&eater->mMindControlled&&b->mMainCounter>s.baselineTick)
                {
                    if(!s.replacement)
                    {s.replacementActor={fresh->mNetId,fd->instanceToken};NetTrace("SMOKE_SHOOTER267_REPLACEMENT role=%s oldId=%u oldToken=%u newId=%u newToken=%u kind=9 column=%d row=%d eater=%u MC=1 tick=%d baselineTick=%d naturalBite=1",Role(a),s.actors[i].id,s.actors[i].token,fresh->mNetId,fd->instanceToken,Column(i),Row(i),eater->mNetId,b->mMainCounter,s.baselineTick);}
                    if(!Require(a,fresh->mNetId==s.replacementActor.id&&fd->instanceToken==s.replacementActor.token,"Shooter267 replacement identity changed"))return false;
                    s.phaseSeen[i]=true;s.replacement=true;continue;
                }
            }
            if(s.group==2&&i==3&&s.adaptiveFar&&s.adaptiveNear&&s.adaptiveDamage&&s.adaptiveEaten)
            {
                if(!s.adaptiveRetired)LogActorLoss(a,i,"observed native bite after Far+Near");
                s.adaptiveRetired=true;continue;
            }
            LogActorLoss(a,i,"unexpected disappearance without required outcome");
            char why[160];sprintf_s(why,"Shooter267 source actor lost index=%d kind=%u id=%u token=%u lastHP=%d",i,kinds[s.group][i],s.actors[i].id,s.actors[i].token,s.lastActorHP[i]);return Require(a,false,why);
        }
        if(!Require(a,p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token,"Shooter267 source actor identity changed"))return false;
        s.phaseSeen[i]=s.phaseSeen[i]||d->shootSeconds>=0||d->batchShotCount>0;
        if(d->kind==927){s.tier101|=d->batchBagCount>=1;s.tier301|=d->batchBagCount>=2;}
        if(d->kind==1024)s.fear|=p->mState==STATE_SCAREDYSHROOM_LOWERING||p->mState==STATE_SCAREDYSHROOM_SCARED;
        if(d->kind==1025)
        {
            if(d->shootSeconds>=0){s.adaptiveNear|=p->mState==STATE_KERNELPULT_BUTTER;s.adaptiveFar|=p->mState!=STATE_KERNELPULT_BUTTER;}
            if(p->mPlantHealth<s.lastActorHP[i])
            {
                s.adaptiveDamage=true;Zombie* eater=nullptr;Zombie* z=nullptr;
                while(b->IterateZombies(z))if(!z->mDead&&!z->mMindControlled&&z->mRow==p->mRow&&z->mIsEating&&z->FindPlantTarget(ATTACKTYPE_CHEW)==p){eater=z;break;}
                s.adaptiveEaten|=eater!=nullptr;
                NetTrace("SMOKE_SHOOTER267_ADAPTIVE_DAMAGE role=%s id=%u token=%u before=%d after=%d far=%d near=%d eater=%u npcPhase=%d npcX=%.9g npcEating=%d tick=%d",
                    Role(a),p->mNetId,d->instanceToken,s.lastActorHP[i],p->mPlantHealth,s.adaptiveFar?1:0,s.adaptiveNear?1:0,eater?eater->mNetId:0,eater?int(eater->mZombiePhase):-1,eater?eater->mPosX:0,eater?1:0,b->mMainCounter);
            }
        }
        s.lastActorHP[i]=p->mPlantHealth;s.lastActorX[i]=float(p->mX);s.lastActorY[i]=float(p->mY);
        if(d->kind==1112)
        {auto* upper=At(b,2,Row(3),26);const auto* u=GetNativePlantData(upper);s.pot|=upper&&u&&u->shootSeconds>=0&&GetNativeDefenderPotKind(upper)==1112&&GetNativeDefenderLobberInterval(upper,3)==2;s.phaseSeen[i]|=s.pot;}
        if(d->kind==1073)
        {auto* z=Target(b,s.targets[i]);s.bite|=z&&p->mPlantHealth<p->mPlantMaxHealth&&z->mBodyHealth<targetHP;s.phaseSeen[i]|=s.bite;
            if(p->mPlantHealth>s.lastTallHealth)
            {
                if(!s.fireHeal)
                {
                    if(!Require(a,s.healPaid&&!s.poolDeathObserved,"Shooter267 healing preceded paid16 or coincided with919 death"))return false;
                    s.fireHeal=true;s.paidHealTick=b->mMainCounter;
                    NetTrace("SMOKE_SHOOTER267_FIRE_HEAL role=%s plant=%u before=%d after=%d row=5 paid16=1 sourceActor=%u sourceToken=%u tick=%d nativePulse=1",Role(a),p->mNetId,s.lastTallHealth,p->mPlantHealth,s.healActor.id,s.healActor.token,s.paidHealTick);
                }
                else if(s.poolDeathObserved&&!s.poolDeathHeal)
                {
                    if(!Require(a,b->mMainCounter>s.paidHealTick,"Shooter267 paid/death heal witnesses share one snapshot"))return false;
                    s.poolDeathHeal=true;NetTrace("SMOKE_SHOOTER267_POOL_DEATH_HEAL role=%s plant=%u before=%d after=%d row=5 source919=%u sourceToken=%u paidHealTick=%d deathTick=%d distinctWitness=1",Role(a),p->mNetId,s.lastTallHealth,p->mPlantHealth,s.poolDying.id,s.poolDying.token,s.paidHealTick,s.poolDeathTick);
                }
            }
            s.lastTallHealth=p->mPlantHealth;}
    }
    for(auto id:s.targets)if(auto* z=Target(b,id))
    {
        if(s.group==0){s.poison|=GetNativePoisonLevel(z)>0;s.beamCold|=z->mChilledCounter>0||z->mIceTrapCounter>0;s.beamControl|=z->mMindControlled;}
        if(s.group==2)s.colors|=GetNativeZombieActionData(z)&&GetNativeZombieActionData(z)->colorMask==127&&z->mMindControlled;
        if(s.group==3&&id==s.targets[0])s.hypno|=z->mMindControlled&&z->mBodyMaxHealth==targetHP/2&&std::abs(z->mScaleZombie-.5f)<.001f;
    }
    if(!ObserveTargetOutcomes(a))return false;
    Projectile* q=nullptr;bool flight=false;
    while(b->mProjectiles.IterateNext(q))if(!q->mDead)
    {
        const auto* d=GetNativeProjectileData(q);if(!d)continue;flight=true;s.payloads.insert(d->kind);
        if(s.group==3&&d->kind==0)
        {
            const bool adjacent=q->mRow==5&&d->specialType==kFusionThreeLower,edge=q->mRow==4&&d->specialType==kFusionThreeUpper;
            if((adjacent&&!s.poolAdjacentShot)||(edge&&!s.poolEdgeShot))
                NetTrace("SMOKE_SHOOTER267_POOL_SIDE_SHOT role=%s source=%u sourceToken=%u sourceRow=%d projectile=%u row=%d way=%u x=%.9g y=%.9g age=%d actualNativeMotion=1",
                    Role(a),adjacent?s.actors[1].id:s.poolEdge.id,adjacent?s.actors[1].token:s.poolEdge.token,adjacent?4:5,q->mNetId,q->mRow,d->specialType,q->mPosX,q->mPosY+q->mPosZ,q->mProjectileAge);
            s.poolAdjacentShot|=adjacent;s.poolEdgeShot|=edge;
        }
        if(s.group==2&&(d->kind==98||d->kind==99)&&d->lastTorchId==s.actors[1].id&&d->lastTorchToken==s.actors[1].token)
            s.phaseSeen[1]=true; // Source932 is a passive conversion, with no animated shot counter.
        if(s.projectiles.insert(q->mNetId).second)
        {
            NetTrace("SMOKE_SHOOTER267_PROJECTILE role=%s group=%d id=%u payload=%u damage=%d way=%u",Role(a),s.group,q->mNetId,d->kind,d->damage,d->specialType);
            if(s.group==3&&d->kind==52&&q->mRow==Row(3)&&a->NetIsHost())
            {
                const int birth=b->mMainCounter-q->mProjectileAge;
                if(s.potBirth>=0)
                {const int delta=birth-s.potBirth;if(!Require(a,delta>=190&&delta<=211,"Shooter267 real CabbagePot callback interval is outside source2sec jitter"))return false;s.potTiming=true;}
                s.potBirth=birth;
            }
        }
        s.heat|=d->kind==87&&d->fireState&&d->damage==320;s.landing|=(d->kind==28||d->kind==36)&&d->specialType==255;
    }
    if(s.group==3)
    {
        if(a->NetIsHost()&&s.potTiming)Send(a,Tag("POT_TIMING"));
        if(a->NetIsClient()&&Has(a,0,Tag("POT_TIMING")))s.potTiming=true;
        if(s.bite&&!s.healPaid)
        {
            SeedPacket* card=nullptr;for(int i=0;i<b->mSeedBank->mNumPackets;++i)
            {auto* next=&b->mSeedBank->mSeedPackets[i];const auto* nd=GetNativePacketData(next);if(nd&&nd->owner==0&&nd->kind==16&&!nd->isExtra)card=next;}
            if(!Require(a,card,"Shooter267 native FireRow proof card16 missing"))return false;
            if(auto* jala=At(b,8,Row(2),16))
            {
                const auto* d=GetNativePlantData(jala);const auto* price=FusionFindPlant(16);
                if(!Require(a,d&&d->instanceToken&&jala->mNetId&&d->owner==0&&d->packetId==card->mNetId&&
                    d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&price&&GetNativePacketData(card)->price==price->sunCost&&
                    b->mSunMoney==s.craftSun-price->sunCost&&(!a->NetIsHost()||card->mTimesUsed==1)&&card->mRefreshing&&
                    card->mRefreshTime==int(std::lround(GetNativePacketData(card)->rechargeSeconds*100.f)),
                    "Shooter267 actual paid Jalapeno16 identity/cost/CD differs"))return false;
                s.healPaid=true;s.healActor={jala->mNetId,d->instanceToken};
                NetTrace("SMOKE_SHOOTER267_HEAL_PAID role=%s kind=16 id=%u token=%u packet=%u owner=0 cost=%d sun=%d CD=1 hostSDL=1",Role(a),jala->mNetId,d->instanceToken,card->mNetId,price->sunCost,b->mSunMoney);
            }
            else if(a->NetIsHost()&&!s.healPlaced)
            {
                if(!s.healPicked)
                {
                    if(card->mActive&&!card->mRefreshing)
                        s.healPicked=BoardClick(a,float(b->mSeedBank->mX+card->mX+card->mOffsetX+card->mWidth/2),float(b->mSeedBank->mY+card->mY+card->mHeight/2));
                }
                else
                {
                    const auto* c=b->mCursorObject;
                    if(c&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==card->mNetId)
                        s.healPlaced=BoardClick(a,float(b->GetCellCenterX(8,Row(2))),float(b->GridToPixelY(8,Row(2))+50));
                }
            }
        }
    }
    if(flight)Capture(a,"flight");return true;
}
uint32_t MissingProof(Board* b)
{
    uint32_t m=0;const auto need=[&](int bit,bool okay){if(!okay)m|=uint32_t(1)<<bit;};
    for(int i=0;i<Count();++i)need(i,s.phaseSeen[i]);
    if(s.group==0){need(8,s.poison);need(9,s.beamCold);need(10,s.beamControl);need(11,s.payloads.contains(47));}
    if(s.group==1){int bit=8;for(int k:{7,87,93,28})need(bit++,s.payloads.contains(uint16_t(k)));need(12,s.mineDead);need(13,s.tier101);need(14,s.tier301);need(15,s.crafted);need(16,s.landing);}
    if(s.group==2){int bit=8;for(int k:{36,98,99,13})need(bit++,s.payloads.contains(uint16_t(k)));need(12,s.heat);need(13,s.fear);need(14,s.adaptiveNear);need(15,s.adaptiveFar);need(16,s.colors);need(17,s.replacement);}
    if(s.group==3){need(8,s.hypno);need(9,s.bite);need(10,s.pot);need(11,s.payloads.contains(0));need(12,s.payloads.contains(52));need(13,s.entry);need(14,s.fireHeal);need(15,s.potTiming);need(16,s.poolAdjacentShot);need(17,s.poolEdgeShot);need(18,s.poolDeathObserved);need(19,s.poolDeathHeal);}
    for(size_t i=0;i<s.targets.size();++i)
    {
        const auto proof=s.targetOutcomes.find(s.targets[i]);const auto* z=Target(b,s.targets[i]);
        need(int(20+i),proof!=s.targetOutcomes.end()&&proof->second.observed&&(z||proof->second.retirementExpected));
    }
    return m;
}
void LogProof(LawnApp* a,const char* reason)
{
    NetTrace("SMOKE_SHOOTER267_PROOF role=%s group=%d stage=%d reason=%s missing=%08x phases=%d,%d,%d,%d poison=%d cold=%d control=%d mine=%d tiers=%d,%d crafted=%d heat=%d landing=%d fear=%d adaptive=%d,%d colors=%d hypno=%d bite=%d pot=%d",
        Role(a),s.group,s.stage,reason,MissingProof(a->mBoard),s.phaseSeen[0]?1:0,s.phaseSeen[1]?1:0,s.phaseSeen[2]?1:0,s.phaseSeen[3]?1:0,s.poison?1:0,s.beamCold?1:0,s.beamControl?1:0,s.mineDead?1:0,s.tier101?1:0,s.tier301?1:0,s.crafted?1:0,s.heat?1:0,s.landing?1:0,s.fear?1:0,s.adaptiveFar?1:0,s.adaptiveNear?1:0,s.colors?1:0,s.hypno?1:0,s.bite?1:0,s.pot?1:0);
    if(s.group==3)NetTrace("SMOKE_SHOOTER267_HEAL_GATE role=%s kind=16 picked=%d placed=%d paid=%d recovered=%d sourceId=%u sourceToken=%u lastTallHP=%d cursor=%d",
        Role(a),s.healPicked?1:0,s.healPlaced?1:0,s.healPaid?1:0,s.fireHeal?1:0,s.healActor.id,s.healActor.token,s.lastTallHealth,a->mBoard->mCursorObject?int(a->mBoard->mCursorObject->mCursorType):-1);
    if(s.group==3)NetTrace("SMOKE_SHOOTER267_POOL_GATE role=%s rows=6 entry=%d adjacentShot=%d edgeShot=%d deathDamage=%d deathEater=%d deathObserved=%d deathHeal=%d last919HP=%d paidHealTick=%d deathTick=%d",
        Role(a),s.entry?1:0,s.poolAdjacentShot?1:0,s.poolEdgeShot?1:0,s.poolDeathDamage?1:0,s.poolDeathEater?1:0,s.poolDeathObserved?1:0,s.poolDeathHeal?1:0,s.poolDyingHP,s.paidHealTick,s.poolDeathTick);
    for(auto id:s.targets){const auto* z=Target(a->mBoard,id);NetTrace("SMOKE_SHOOTER267_TARGET_GATE role=%s id=%u body=%d max=%d helm=%d shield=%d MC=%d",Role(a),id,z?z->mBodyHealth:-1,z?z->mBodyMaxHealth:-1,z?z->mHelmHealth:-1,z?z->mShieldHealth:-1,z&&z->mMindControlled?1:0);}
}
bool Complete(LawnApp* a,bool log)
{if(log)LogProof(a,"outcome");return MissingProof(a->mBoard)==0;}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionShooter267(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_SHOOTER267"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>180000)
    {
        const bool world=a->mBoard&&IsNativeFusionBoard(a->mBoard)&&s.setup;
        if(world)LogProof(a,"deadline");
        char why[120];sprintf_s(why,"Shooter267 deadline group=%d stage=%d missing=%08x",s.group,s.stage,world?MissingProof(a->mBoard):0);
        a->CoopSmokeFail(why);return true;
    }
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"A267_BEGIN");}
        else if(Has(a,1,"A267_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==10)
    {
        if(!Barrier(a,"DONE"))return true;
        if(s.group==3){NetTrace("SMOKE_SHOOTER267_COMPLETE role=%s activities=4 sourcePlants=16 initialFactory=1 nativeCombat=1 postSetupActorWrites=0 paidRecipe14plus13=clientSDL sourceNPCParity=NotExecuted",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.previousEpoch)Send(a,"A267_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.previousEpoch)return true;
    if(!s.epoch){s.epoch=ctx->epoch;NetTrace("SMOKE_SHOOTER267_EPOCH role=%s group=%d epoch=%u previous=%u",Role(a),s.group,s.epoch,s.previousEpoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Shooter267 actual native validation epoch differs"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000)
    {
        s.diagnostic=GetTickCount();NetTrace("SMOKE_SHOOTER267_GATE role=%s group=%d stage=%d epoch=%u tick=%d samples=%u paused=%d projectiles=%zu",Role(a),s.group,s.stage,s.epoch,b->mMainCounter,Samples(a),b->mPaused?1:0,s.projectiles.size());
        if(s.setup&&s.stage>=6)LogProof(a,"gate");
    }
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Shooter267 shared native chooser quota differs"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;
        if(own<5)
        {
            const int wanted=own==0?(Slot(a)==1?13:s.group==3?16:11):5*Slot(a)+own;int dense=-1;
            for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);
            if(!Require(a,dense>=0,"Shooter267 baseline chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;
        }
        if(bank.packets.size()!=10)return true;int rank=-1;
        for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Shooter267 own-first native bank order differs"))return true;rank=next;}
        if(Capture(a,"chooser")){NetTrace("SMOKE_SHOOTER267_POOL role=%s group=%d epoch=%u target=10 quotas=5,5 ownFirst=1 nativeChooser=1",Role(a),s.group,s.epoch);Advance(a,2);}
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Shooter267 owned Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro)
        {if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Shooter267 actual native full intro not observed"))return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;return true;}if(a->mAppCounter<=s.playingFrame)return true;
        if(a->NetIsHost()&&!Setup(a))return true;
        Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!Setup(a))return true;if(!s.pauseSample){s.pauseSample=Samples(a);return true;}
        if(Samples(a)-s.pauseSample<2||!Capture(a,"before")||!Barrier(a,"BEFORE"))return true;
        s.baselineTick=b->mMainCounter;Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(a->NetIsHost()&&!s.resumed){s.resumed=Key(a);return true;}
        if(!b->mPaused)Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!Observe(a))return true;
        if(Complete(a,false)&&s.frames.contains("fusion_shooter267_"+std::to_string(s.group)+"_flight")&&Barrier(a,"OUTCOME"))Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(!Observe(a))return true;
        if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}
        if(b->mPaused){s.pauseSample=0;Advance(a,8);}
    }
    else if(s.stage==8)
    {
        if(!s.pauseSample){s.pauseSample=Samples(a);return true;}if(Samples(a)-s.pauseSample<2)return true;
        if(!ObserveTargetOutcomes(a))return true;
        if(!Complete(a,false))return true;
        LawnNet::NetBuffer health;health.WriteU16(uint16_t(s.targets.size()));
        // Both peers use the host's exact UID order, independent of local slots.
        for(auto id:s.targets)
        {
            auto* z=Target(b,id);
            health.WriteU16(id);health.WriteU8(z?1:0);
            health.WriteI32(z?z->mBodyHealth:-1);health.WriteI32(z?z->mHelmHealth:-1);health.WriteI32(z?z->mShieldHealth:-1);
        }
        const uint32_t digest=NetFnv1a32(health.Data(),health.Size());
        char hp[32];sprintf_s(hp,"HP_%zu_%08x",s.targets.size(),digest);
        if(!s.finalLogged)
        {
            if(!Complete(a,true))return true;s.finalLogged=true;
            NetTrace("SMOKE_SHOOTER267_HP_DIGEST role=%s group=%d epoch=%u count=%zu hash=%08x bytes=%zu paused=1 canonicalHostUIDOrder=1",Role(a),s.group,s.epoch,s.targets.size(),digest,health.Size());
        }
        if(!Barrier(a,hp))return true;
        if(!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        if(s.group==3)NetTrace("SMOKE_SHOOTER267_POOL_REGRESSION role=%s group=3 rows=6 adjacentEntry3=1 edgeEntry2=1 adjacentRow5Shot=1 edgeRow4Shot=1 paid16Heal=1 natural919Death=1 deathRow5Heal=1 distinctSnapshots=1 auxiliaryActorsNotAdditionalAcceptance=1 postSetupActorWrites=0",Role(a));
        NetTrace("SMOKE_SHOOTER267_CASE role=%s group=%d epoch=%u plants=%d nativeCombat=1 snapshots=%u elapsedTicks=%d postSetupActorWrites=0 initialFactory=1 paidRecipe=%d finalSameHP=1 canonicalPresenceAndTombstones=1 sourceNPCParity=NotExecuted",Role(a),s.group,s.epoch,Count(),Samples(a),b->mMainCounter-s.baselineTick,s.crafted?1:0);Advance(a,10);
    }
    return true;
}
}
