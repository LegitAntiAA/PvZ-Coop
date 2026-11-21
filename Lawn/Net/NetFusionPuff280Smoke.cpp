#include "NetFusionPuff280Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeDayZombie273.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/Image.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0,localUID=0;};
struct State
{
    int stage=0,waiting=0,input=0,classicLevel=-1,classicFinished=-1;
    uint32_t started=0,since=0,chat=0,wheel=0,epoch=0,samples=0,introStart=0,introLast=0,diagnostic=0;
    unsigned introChecks=0;bool born=false,paused=false,logged=false;
    Ref actor;std::array<Ref,2> plants{};std::array<bool,2> ready{},cooldown{};
    std::map<uint16_t,uint32_t> shots;uint16_t proposedShot=0;
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* k){const auto* v=std::getenv(k);return v&&std::strcmp(v,"1")==0;}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool v,const char* why){if(!v)a->CoopSmokeFail(why);return v;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.input=0;s.since=GetTickCount();NetTrace("SMOKE_PUFF280_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);}
std::string Tag(const std::string& value){return "P280_"+std::to_string(s.epoch)+"_"+value;}
std::string Id(const Ref& ref){return std::to_string(ref.id)+"_"+std::to_string(ref.token);}
bool Has(LawnApp* a,int slot,const std::string& tag)
{for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==tag)return true;return false;}
bool Send(LawnApp* a,const std::string& tag)
{
    if(!Require(a,tag.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Puff280 proof exceeds chat limit"))return false;
    if(Has(a,Slot(a),tag))return true;if(s.chat&&GetTickCount()-s.chat<850)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(tag);return Has(a,Slot(a),tag);
}
bool Barrier(LawnApp* a,const std::string& value)
{const auto tag=Tag(value);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Puff280 SDL hover failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Puff280 SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Puff280 SDL pause failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_puff280_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;const int draw=s.draws[name]+2;
    if(!a->CoopSmokeEnsureFreshFrameAfterDraw(draw))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Puff280 fresh BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_PUFF280_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),draw,a->mCoopSmokeFrameDrawCount);return true;
}
bool Profile(const FusionAdventureProfile273& p){return p.completedLevels==511&&p.cycle==1&&p.frontierLevel==10;}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||!c->adventure.enabled)return false;const auto& v=c->adventure;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&v.run.level==9&&v.run.cycle==1&&v.run.replay&&
        !v.run.tutorial.active&&!v.run.tutorial.stage&&!v.run.over&&!v.schedulerFailed&&v.maximumWaves==30&&v.sourceDifficulty==2&&
        Profile(v.profile)&&Profile(a->mNetManager->mConfig.mFusionSourceProfile)&&c->money==3000&&
        b->mBackground==BACKGROUND_1_DAY&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&
        a->mPlayerInfo&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished,
        "Puff280 isolated replay9 native world/profile differs"))return false;
    if(!s.epoch)s.epoch=c->epoch;return Require(a,s.epoch==c->epoch,"Puff280 epoch changed");
}
SeedPacket* Packet(Board* b,int owner)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];if(const auto* d=GetNativePacketData(p);d&&d->owner==owner&&d->kind==6&&d->isExtra==(owner==1))return p;}return nullptr;}
bool PacketRules(LawnApp* a,int owner)
{
    auto* p=Packet(a->mBoard,owner);const auto* d=GetNativePacketData(p);
    return Require(a,p&&d&&d->price==(owner?175:75)&&std::abs(d->rechargeSeconds-7.5f)<.0001f,
        "Puff280 Day normal75/extra175/CD7.5 metadata differs");
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* q=a->mSeedChooserScreen;
    if(!q||!q->mNativeFusionChooser||!q->mMouseVisible||q->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();const int slot=Slot(a);const uint16_t wanted[]={6,0,1,2,3};
    if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Puff280 native chooser quota differs"))return false;
    int own=0;for(const auto& p:bank.packets)if(p.owner==slot)
    {if(!Require(a,own<5&&p.kind==wanted[own]&&p.isExtra==(slot==1),"Puff280 native chooser selection differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<5)
    {
        int dense=-1;for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==wanted[own]&&c->eligible[i].isExtra==(slot==1))dense=int(i);
        if(!Require(a,dense>=0,"Puff280 source-completed9 normal/extra Puff unavailable"))return false;
        int x=0,y=0;q->GetSeedPositionInChooser(dense,x,y);const float cy=float(y+35-int(q->mScrollbar->mScrollValue));
        if(cy<158||cy>493)
        {
            if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
            SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(q->mX+300);m.motion.y=float(q->mY+300);
            SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
            Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Puff280 chooser scroll failed");return false;
        }
        if(std::abs(q->mScrollAmount)<=.05f&&Click(a,float(q->mX+x+25),float(q->mY)+cy))s.waiting=own+1;return false;
    }
    if(bank.packets.size()!=10||!PacketRules(a,0)||!PacketRules(a,1))return false;
    int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i)
    {const int next=LawnNet::NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Puff280 native own-first bank differs"))return false;rank=next;}
    return Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Actor(LawnApp* a)
{
    auto* b=a->mBoard;
    if(a->NetIsHost()&&!s.born)
    {
        if(!Require(a,b->mPlants.mSize==0&&b->mZombies.mSize==0&&b->mSunMoney==500,"Puff280 actor fixture is not a fresh opening"))return false;
        NativeSourceZombieSpawnRequest request;request.kind=107;request.row=0;request.fromWave=0;request.positioned=true;request.shadowX=float(b->GetCellCenterX(9,0));
        auto* z=SpawnNativeSourceZombie(b,request);const auto* d=GetNativeSourceZombieData(z);
        if(!Require(a,z&&d&&d->kind==107,"Puff280 genuine initial107 factory rejected"))return false;s.born=true;
        NetTrace("SMOKE_PUFF280_INITIAL kind=107 row=0 fromWave=0 shadowBirthX=%.6f acquisition=NotClaimed postBirthWrites=0",request.shadowX);
    }
    Zombie* z=nullptr;Zombie* p=nullptr;while(b->IterateZombies(p))if(const auto* d=GetNativeSourceZombieData(p);d&&d->kind==107){z=p;break;}
    if(!z)return false;const auto* d=GetNativeSourceZombieData(z);const auto* day=GetNativeDayZombie273(z);
    if(!Require(a,d&&day&&z->mNetId&&d->instanceToken&&z->mRow==0&&z->mFromWave==0&&z->mNetGhost==a->NetIsClient()&&
        z->mBodyMaxHealth==270&&z->mHelmMaxHealth==1100&&!GetNativeContext(b)->dayZombies273.empty(),
        "Puff280 actual107/Day frame context differs"))return false;
    if(!s.actor.id)s.actor={z->mNetId,d->instanceToken,b->mZombies.DataArrayGetID(z)};
    if(!Require(a,z->mNetId==s.actor.id&&d->instanceToken==s.actor.token&&b->mZombies.DataArrayGetID(z)==s.actor.localUID,
        "Puff280 initial107 full generation changed"))return false;
    return Capture(a,"day_actor")&&Barrier(a,"ACTOR_"+Id(s.actor));
}
Plant* Puff(Board* b,int owner)
{
    Plant* p=nullptr;while(b->IteratePlants(p))if(p->mPlantCol==7+owner&&p->mRow==0)
        if(const auto* d=GetNativePlantData(p);d&&d->kind==6)return p;return nullptr;
}
bool PlantPuff(LawnApp* a,int owner)
{
    auto* b=a->mBoard;if(!PacketRules(a,owner))return false;auto* packet=Packet(b,owner);
    if(Slot(a)==owner&&s.input==0)
    {
        if(!packet->CanPickUp()||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
        s.ready[size_t(owner)]=true;
        if(BoardClick(a,float(b->mSeedBank->mX+packet->mX+packet->mOffsetX+25),float(b->mSeedBank->mY+packet->mY+35)))s.input=1;return false;
    }
    if(Slot(a)==owner&&s.input==1)
    {
        const auto* cursor=b->mCursorObject;
        if(cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||cursor->mSeedBankIndex>=b->mSeedBank->mNumPackets||
            b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=packet->mNetId)return false;
        if(BoardClick(a,float(b->GetCellCenterX(7+owner,0)),float(b->GridToPixelY(7+owner,0)+50)))s.input=2;return false;
    }
    auto* p=Puff(b,owner);if(!p)return false;const auto* d=GetNativePlantData(p);
    if(!Require(a,p->mNetId&&d->instanceToken&&p->mSeedType==SEED_PUFFSHROOM&&!p->mIsAsleep&&p->mWakeUpCounter==0&&p->mIsOnBoard&&
        p->mNetGhost==a->NetIsClient()&&p->mPlantMaxHealth==300&&p->mPlantHealth<=300&&d->owner==owner&&d->packetId==packet->mNetId&&
        d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&!d->randomOrigin&&
        b->mSunMoney==(owner?250:425)&&!GetNativeContext(b)->dayZombies273.empty(),"Puff280 real purchase/awake carrier/owner/HP/Sun differs"))return false;
    if(!s.plants[size_t(owner)].id)
    {
        s.plants[size_t(owner)]={p->mNetId,d->instanceToken,b->mPlants.DataArrayGetID(p)};
        NetTrace("SMOKE_PUFF280_PLANT role=%s owner=%d id=%u token=%u localUID=%u kind=6 extra=%d price=%d sun=%d hp=%d maxHP=%d awake=1 dayContextBeforeBirth=1",
            Role(a),owner,p->mNetId,d->instanceToken,s.plants[size_t(owner)].localUID,owner,owner?175:75,b->mSunMoney,p->mPlantHealth,p->mPlantMaxHealth);
    }
    const auto& ref=s.plants[size_t(owner)];
    if(!Require(a,p->mNetId==ref.id&&d->instanceToken==ref.token&&b->mPlants.DataArrayGetID(p)==ref.localUID,"Puff280 paid actor full generation changed"))return false;
    if(packet->mRefreshing&&packet->mRefreshTime==750&&packet->mRefreshCounter>=0&&packet->mRefreshCounter<750)s.cooldown[size_t(owner)]=true;
    if(!s.cooldown[size_t(owner)])return false;
    return Barrier(a,"PLANT_"+std::to_string(owner)+"_"+Id(ref));
}
void ObserveShots(LawnApp* a)
{
    auto* b=a->mBoard;Projectile* p=nullptr;while(b->IterateProjectiles(p))if(const auto* d=GetNativeProjectileData(p);d&&d->kind==9)
    {
        if(!Require(a,p->mNetId&&d->damage==20&&!d->sunOnHit&&!d->isZombieBullet&&p->mProjectileType==PROJECTILE_PUFF&&p->mMotionType==MOTION_PUFF&&
            p->mNetGhost==a->NetIsClient()&&std::isfinite(p->mPosX)&&std::isfinite(p->mPosY),"Puff280 natural projectile9 tuple differs"))return;
        if(!s.shots.contains(p->mNetId))
        {s.shots[p->mNetId]=b->mProjectiles.DataArrayGetID(p);NetTrace("SMOKE_PUFF280_SHOT role=%s epoch=%u id=%u localUID=%u kind=9 damage=20 motion=%d natural=1 wireHostToken=NotPresent",Role(a),s.epoch,p->mNetId,s.shots[p->mNetId],int(p->mMotionType));}
    }
}
bool VisibleShot(LawnApp* a)
{
    // Source9 intentionally reuses the real native PUFF artwork/Draw, not an
    // embedded source model. Its native sprite centre is pos +half a cel;
    // the source-model node query does not apply to this carrier.
    auto* image=Sexy::IMAGE_PUFFSHROOM_PUFF1;
    if(!Require(a,image&&image->GetCelWidth()>0&&image->GetCelHeight()>0,"Puff280 actual native Puff artwork unavailable"))return false;
    auto* b=a->mBoard;Projectile* p=nullptr;
    while(b->IterateProjectiles(p))if(const auto* d=GetNativeProjectileData(p);d&&d->kind==9)
    {
        const float x=p->mPosX+image->GetCelWidth()*.5f+b->mX+a->mDDInterface->mWideScreenOffsetX;
        const float y=p->mPosY+p->mPosZ+image->GetCelHeight()*.5f+b->mY+a->mDDInterface->mWideScreenOffsetY;
        if(p->mFrame>=0&&p->mFrame<image->mNumCols&&std::isfinite(x)&&std::isfinite(y)&&
            x>=8&&x<a->mWidth-16&&y>=0&&y<a->mHeight-16)return true;
    }
    return false;
}
bool ShotProof(LawnApp* a)
{
    const auto prefix=Tag("SHOT_");
    if(a->NetIsHost()&&!s.proposedShot&&!s.shots.empty())s.proposedShot=s.shots.begin()->first;
    if(a->NetIsHost()&&s.proposedShot)Send(a,prefix+std::to_string(s.proposedShot));
    for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
    {
        const auto value=line.mText.substr(prefix.size());unsigned id=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),id);
        if(!Require(a,parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&id&&id<=65535&&(!s.proposedShot||s.proposedShot==id),"Puff280 malformed/conflicting shot proof"))return false;
        s.proposedShot=uint16_t(id);
    }
    if(!s.proposedShot||!s.shots.contains(s.proposedShot))return false;
    return Barrier(a,"OBSERVED_"+std::to_string(s.proposedShot));
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionPuff280(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_PUFF280"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,"Puff280 requires owned native fusion_validation"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>180000){a->CoopSmokeFail("Puff280 native mechanism input/proof deadline");return true;}
    if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1&&a->mPlayerInfo&&Profile(a->mNetManager->mConfig.mFusionSourceProfile),"Puff280 private replay profile/peers differ"))return true;
        if(s.classicLevel<0){s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;}
        if(a->NetIsClient()){if(Send(a,"PUFF280_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"PUFF280_BEGIN"))return true;
        if(Require(a,a->StartFusionSourceAdventure(9,false),"Puff280 source replay9 production entry rejected"))Advance(a,1);return true;
    }
    if(!a->mBoard||!World(a))return true;auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto phase=c->selection.State().phase;
    if(s.stage>=4)ObserveShots(a);
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* q=a->mSeedChooserScreen;auto* button=q?q->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||q->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())Click(a,x,y,false);else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000,"Puff280 skipped native intro"))return true;
        if(Actor(a))Advance(a,4);
    }
    else if(s.stage==4){if(PlantPuff(a,0))Advance(a,5);}
    else if(s.stage==5){if(PlantPuff(a,1))Advance(a,6);}
    else if(s.stage==6)
    {
        // At least one genuine currently rendered source bullet must accompany
        // the fresh Puff BMP; its wire identity is agreed separately below.
        if(!s.frames.contains("fusion_puff280_puffs")&&!VisibleShot(a))return true;
        if(Capture(a,"puffs")&&ShotProof(a))Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(a->NetIsHost()&&!s.paused)s.paused=Escape(a);if(!b->mPaused)return true;
        s.samples=Samples(a);Advance(a,8);
    }
    else if(s.stage==8&&Samples(a)-s.samples>=2)
    {
        if(!Require(a,b->mPaused&&b->mSunMoney==250&&s.cooldown[0]&&s.cooldown[1]&&s.shots.contains(s.proposedShot)&&s.ready[size_t(Slot(a))],
            "Puff280 final paused economy/cooldown/local-ready/shot proof differs"))return true;
        int surviving=0;
        for(int owner=0;owner<2;++owner)
        {
            const auto& ref=s.plants[size_t(owner)];auto* p=b->mPlants.DataArrayTryToGet(ref.localUID);
            if(!p)continue; // Natural combat may retire a proved birth; no survival claim.
            const auto* d=GetNativePlantData(p);
            if(!Require(a,p->mNetId==ref.id&&(!d||d->instanceToken==ref.token)&&
                (p->mDead||(d&&d->kind==6&&!p->mIsAsleep&&d->owner==owner&&d->packetId==Packet(b,owner)->mNetId)),
                "Puff280 final surviving Puff generation/awake/provenance differs"))return true;
            if(!p->mDead)++surviving;
        }
        if(!Capture(a,"paused"))return true;
        if(!s.logged)
        {s.logged=true;NetTrace("SMOKE_PUFF280_COMPLETE role=%s epoch=%u actors=%s,%s npc107=%s shot=%u localShotUID=%u normal=75 extra=175 sun=250 CD=750 cooldownObserved=2 readyBeforeOwnUse=1 awakeAtBirth=2 surviving=%d dayActorBeforeRawBirth=1 nativeInputs=1 frames=4 canonicalShot=EpochNetIDTuple wireShotToken=NotPresent progressionEarned=0 Full9Victory=NotExecuted NightCampaign=NotExecuted HPWrites=0 clockWrites=0 RNGWrites=0",Role(a),s.epoch,Id(s.plants[0]).c_str(),Id(s.plants[1]).c_str(),Id(s.actor).c_str(),s.proposedShot,s.shots[s.proposedShot],surviving);}
        if(Barrier(a,"FINAL_"+Id(s.plants[0])+"_"+Id(s.plants[1])+"_"+std::to_string(s.proposedShot)))a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
