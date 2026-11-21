#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Ref { uint16_t id=0;uint32_t token=0; };
struct Packet
{
    NativePacketData meta{};int type=0,time=0,counter=0;bool refreshing=false,active=false;float remaining=0;
};
struct State
{
    int stage=0,slot=-1,waiting=0,selected=0,introChecks=0,playingFrame=-1,newOwner=-1,resumeTick=-1;
    uint32_t epoch=0,since=0,chat=0,wheel=0,diagnostic=0,introFirst=0,introLast=0,pickSample=0,baselineSample=0,postSample=0,finalSample=0;
    bool configured=false,firstPicked=false,firstPlaced=false,firstObserved=false,pauseRequested=false,before=false,after=false,resumeRequested=false;
    bool newPicked=false,newPlaced=false,newObserved=false,finalPause=false,postRecorded=false,readyRecorded=false;uint16_t packet=0;Ref original{},fresh{};
    std::map<uint16_t,Packet> bank;std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool value,const char* why){if(!value)a->CoopSmokeFail(why);return value;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_ROSTER240_STAGE role=%s slot=%d epoch=%u stage=%d",Role(a),s.slot,s.epoch,stage);}
std::string Tag(const char* name){return "R240_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,s.slot,text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,s.slot,text);}
bool Barrier(LawnApp* a,const char* value,unsigned mask)
{const auto tag=Tag(value);Send(a,tag);for(int i=0;i<3;++i)if((mask&(1u<<i))&&!Has(a,i,tag))return false;return true;}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Roster owned SDL Escape failed");}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Roster owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Choice(LawnApp* a,SeedChooserScreen* c,int dense)
{
    int x=0,y=0;c->GetSeedPositionInChooser(dense,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Roster chooser owned SDL wheel failed");return false;}
    if(std::abs(c->mScrollAmount)>.05f)return false;return Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_roster_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Roster actual native BMP failed"))return false;s.frames.insert(name);NetTrace("SMOKE_ROSTER240_RENDER role=%s slot=%d frame=%s freshDraws=%d",Role(a),s.slot,name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* Actor(Board* b,Ref ref)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&p->mNetId==ref.id&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==27&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
SeedPacket* Card(Board* b,uint16_t id)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i)if(b->mSeedBank->mSeedPackets[i].mNetId==id)return &b->mSeedBank->mSeedPackets[i];return nullptr;}
bool ReadBank(LawnApp* a,std::map<uint16_t,Packet>& result)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);const auto& state=ctx->selection.State();std::array<int,4> owners{};
    if(!Require(a,b->mSeedBank->mNumPackets==10&&state.targetCount==10&&state.packets.size()==10,"Roster bank must retain exactly ten actual packets"))return false;
    for(int i=0;i<10;++i)
    {
        const auto& card=b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(&card);const FusionBankPacket* selected=nullptr;
        if(d)for(const auto& p:state.packets)if(p.id==d->selectionId){selected=&p;break;}
        if(!Require(a,d&&selected&&card.mNetId&&d->owner<4&&card.mNetOwnerSlot==d->owner&&selected->owner==d->owner&&selected->kind==d->kind&&selected->isExtra==d->isExtra&&selected->randomOrigin==d->randomOrigin&&selected->price==d->price&&selected->rechargeSeconds==d->rechargeSeconds&&!selected->mysteryPending&&std::isfinite(selected->remainingRechargeSeconds)&&!result.contains(card.mNetId),"Roster native physical/metadata/selection packet mismatch"))return false;
        ++owners[d->owner];result.emplace(card.mNetId,Packet{*d,int(card.mPacketType),card.mRefreshTime,card.mRefreshCounter,card.mRefreshing,card.mActive,selected->remainingRechargeSeconds});
    }
    for(int i=0;i<4;++i)if(!Require(a,owners[i]==state.quotas[i],"Roster actual bank owner counts differ from canonical quotas"))return false;return true;
}
void LogBank(LawnApp* a,const char* phase,const std::map<uint16_t,Packet>& bank)
{
    for(const auto& [id,p]:bank)NetTrace("SMOKE_ROSTER240_BANK role=%s slot=%d phase=%s epoch=%u packet=%u selection=%u kind=%u extra=%d owner=%u origin=%u price=%d recharge=%.9g type=%d time=%d counter=%d refreshing=%d active=%d remaining=%.9g nativeOwner=%u",Role(a),s.slot,phase,s.epoch,id,p.meta.selectionId,p.meta.kind,p.meta.isExtra?1:0,p.meta.owner,p.meta.randomOrigin,p.meta.price,p.meta.rechargeSeconds,p.type,p.time,p.counter,p.refreshing?1:0,p.active?1:0,p.remaining,p.meta.owner);
}
bool Original(LawnApp* a,const char* phase,bool log)
{
    auto* p=Actor(a->mBoard,s.original);const auto* d=GetNativePlantData(p);
    if(!Require(a,p&&d&&d->kind==27&&d->owner==2&&d->packetId==s.packet&&d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&!d->randomOrigin&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&p->mPlantCol==2&&p->mRow==1,"Roster altered or deleted the departed player's birth-provenance plant"))return false;
    if(log)NetTrace("SMOKE_ROSTER240_OLD role=%s slot=%d phase=%s epoch=%u id=%u token=%u packet=%u owner=2 kind=27 HP=300 column=2 row=1",Role(a),s.slot,phase,s.epoch,p->mNetId,d->instanceToken,d->packetId);return true;
}
bool Purchase(LawnApp* a,bool second)
{
    auto* b=a->mBoard;SeedPacket* card=Card(b,s.packet);
    if(!second&&!card)for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* c=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(c);if(d&&d->kind==27&&d->owner==2&&!d->isExtra){card=c;s.packet=c->mNetId;break;}}
    const auto* definition=FusionFindPlant(27);const auto* meta=GetNativePacketData(card);const int owner=second?s.newOwner:2,prior=second?475:500;
    if(!Require(a,card&&meta&&definition&&meta->kind==27&&meta->owner==owner&&!meta->isExtra&&meta->price==25&&meta->price==definition->sunCost&&meta->rechargeSeconds==7.5f,"Roster paid source27 card/owner/cost/CD missing"))return false;
    auto& picked=second?s.newPicked:s.firstPicked;auto& placed=second?s.newPlaced:s.firstPlaced;auto& observed=second?s.newObserved:s.firstObserved;auto& ref=second?s.fresh:s.original;
    if(s.slot==owner&&!picked)
    {if(!card->mActive||card->mRefreshing)return false;auto* bank=b->mSeedBank;s.pickSample=Samples(a);picked=BoardClick(a,float(bank->mX+card->mX+card->mOffsetX+25),float(bank->mY+card->mY+35));return false;}
    if(s.slot==owner&&!placed)
    {const auto* c=b->mCursorObject;const bool held=c&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==card->mNetId;if(!held||(a->NetIsClient()&&Samples(a)<=s.pickSample))return false;if(!Require(a,b->mSunMoney==prior,"Roster bank pickup charged before real placement"))return false;NetTrace("SMOKE_ROSTER240_HELD role=%s slot=%d phase=%s packet=%u owner=%d sun=%d heldEcho=1",Role(a),s.slot,second?"reused":"initial",s.packet,owner,b->mSunMoney);const int col=second?6:2,row=second?3:1;placed=BoardClick(a,float(b->GetCellCenterX(col,row)),float(b->GridToPixelY(col,row)+50));return false;}
    if(!observed)
    {
        auto* p=At(b,second?6:2,second?3:1);const auto* d=GetNativePlantData(p);if(!d)return false;
        if(!Require(a,d->instanceToken&&d->owner==owner&&d->packetId==s.packet&&d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&!d->randomOrigin&&p->mPlantHealth==definition->maxHealth&&p->mPlantMaxHealth==300&&b->mSunMoney==prior-25&&card->mRefreshTime==750&&card->mRefreshing&&!card->mActive,"Roster real paid actor/ledger/CD identity differs"))return false;
        if(second&&!Require(a,p->mNetId!=s.original.id&&d->instanceToken!=s.original.token,"Roster acquired packet did not create a fresh actor"))return false;
        ref={p->mNetId,d->instanceToken};observed=true;NetTrace("SMOKE_ROSTER240_PURCHASE role=%s slot=%d phase=%s epoch=%u id=%u token=%u packet=%u owner=%u kind=27 HP=300 cost=25 sun=%d cooldown=750 nativeSDL=1",Role(a),s.slot,second?"reused":"initial",s.epoch,p->mNetId,d->instanceToken,s.packet,d->owner,b->mSunMoney);
    }
    return Barrier(a,second?"REUSED":"PAID",second?3:7);
}
bool After(LawnApp* a,bool frozen)
{
    const auto& state=GetNativeContext(a->mBoard)->selection.State();std::map<uint16_t,Packet> current;
    if(!Require(a,state.activeMask==3&&state.quotas[0]==5&&state.quotas[1]==5&&!state.quotas[2]&&!state.quotas[3]&&state.phase==FusionBankPhase::Playing,"Roster post-drop canonical mask/quotas/phase differs")||!ReadBank(a,current)||!Original(a,"after",false))return false;
    for(const auto& [id,before]:s.bank)
    {
        const auto found=current.find(id);if(!Require(a,found!=current.end(),"Roster dropped/replaced a live packet ID"))return false;const auto& p=found->second;
        if(!Require(a,p.meta.owner<2&&p.meta.selectionId==before.meta.selectionId&&p.meta.kind==before.meta.kind&&p.meta.isExtra==before.meta.isExtra&&p.meta.randomOrigin==before.meta.randomOrigin&&p.meta.price==before.meta.price&&p.meta.rechargeSeconds==before.meta.rechargeSeconds&&p.type==before.type&&(!frozen||(p.time==before.time&&p.counter==before.counter&&p.refreshing==before.refreshing&&p.active==before.active&&p.remaining==before.remaining)),"Roster mutated a preserved packet identity/price/origin/live cooldown"))return false;
    }
    s.newOwner=current.at(s.packet).meta.owner;
    if(!s.after){s.after=true;s.postSample=Samples(a);LogBank(a,"after",current);Original(a,"after",true);NetTrace("SMOKE_ROSTER240_DROP role=%s slot=%d epoch=%u mask=3 quotas=5,5,0,0 packet=%u newOwner=%d sun=%d samples=%u pausedCDUnchanged=1 birthOwner=2",Role(a),s.slot,s.epoch,s.packet,s.newOwner,a->mBoard->mSunMoney,s.postSample);}
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionRoster(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_ROSTER"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(GetTickCount()-s.since>90000){char why[100];sprintf_s(why,"Roster deadline slot=%d stage=%d before=%d after=%d packet=%u",s.slot,s.stage,s.before?1:0,s.after?1:0,s.packet);a->CoopSmokeFail(why);return true;}
    if(s.stage==0)
    {if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=3||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
     if(s.slot<0){s.slot=Slot(a);NetTrace("SMOKE_ROSTER240_IDENTITY role=%s slot=%d",Role(a),s.slot);}if(!Require(a,s.slot>=0&&s.slot<3,"Roster requires actual slots0,1,2"))return true;if(a->NetIsClient()){Send(a,"R240_BEGIN");Advance(a,1);}else if(Has(a,1,"R240_BEGIN")&&Has(a,2,"R240_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
    if(!Require(a,Slot(a)==s.slot,"Roster changed a remaining peer's actual slot identity"))return true;
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"R240_BEGIN");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(!s.epoch){s.epoch=ctx->epoch;NetTrace("SMOKE_ROSTER240_EPOCH role=%s slot=%d epoch=%u",Role(a),s.slot,s.epoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Roster changed its actual native activity epoch"))return true;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000){s.diagnostic=GetTickCount();auto* p=Card(b,s.packet);NetTrace("SMOKE_ROSTER240_GATE role=%s slot=%d stage=%d players=%d mask=%u selected=%d waiting=%d packet=%u owner=%d counter=%d time=%d paused=%d snapshots=%u",Role(a),s.slot,s.stage,a->mNetManager->GetPlayerCount(),bank.activeMask,s.selected,s.waiting,s.packet,p?p->mNetOwnerSlot:-1,p?p->mRefreshCounter:-1,p?p->mRefreshTime:-1,b->mPaused?1:0,Samples(a));}
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==7&&bank.targetCount==10&&bank.quotas[0]==4&&bank.quotas[1]==3&&bank.quotas[2]==3&&!bank.quotas[3],"Roster initial three-player quotas differ"))return true;
        constexpr std::array<std::array<uint16_t,4>,3> wanted={{{0,1,2,3},{4,5,6,0xffff},{27,29,21,0xffff}}};const int quota=bank.quotas[s.slot];int own=0;
        for(const auto& p:bank.packets)if(p.owner==s.slot){if(!Require(a,own<quota&&p.kind==wanted[s.slot][own]&&!p.isExtra,"Roster actual chooser identity differs"))return true;++own;}
        if(own>s.selected){s.selected=own;NetTrace("SMOKE_ROSTER240_CHOICE_ACK role=%s slot=%d own=%d quota=%d",Role(a),s.slot,own,quota);}
        if(own<s.waiting)return true;if(own<quota){int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted[s.slot][own]&&!ctx->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Roster requested card is not eligible"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()==10&&Capture(a,"chooser")){NetTrace("SMOKE_ROSTER240_POOL role=%s slot=%d mask=7 quotas=4,3,3,0 cards=10 actualPlayers=3",Role(a),s.slot);Advance(a,2);}
    }
    else if(s.stage==2)
    {if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* p=chooser?chooser->mStartButton:nullptr;if(!p||p->mDisabled||p->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=p->mX+p->mWidth*.5f,y=p->mY+p->mHeight*.5f;if(!p->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Roster Ready hover failed");}else if(Click(a,x,y))Advance(a,3);}
    else if(s.stage==3)
    {if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Roster skipped full native intro"))return true;if(s.playingFrame<0){s.playingFrame=a->mAppCounter;return true;}if(a->mAppCounter<=s.playingFrame)return true;if(a->NetIsHost()&&!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize,"Roster fresh world not empty after natural intro deletion"))return true;if(!Require(a,b->mSunMoney==500,"Roster initial wallet must be actual500"))return true;Advance(a,4);}
    else if(s.stage==4){if(Purchase(a,false))Advance(a,5);}
    else if(s.stage==5)
    {if(a->NetIsHost()&&!s.pauseRequested){s.pauseRequested=Key(a);return true;}if(b->mPaused){s.baselineSample=Samples(a);Advance(a,6);}}
    else if(s.stage==6)
    {
        if(!b->mPaused||Samples(a)-s.baselineSample<2)return true;
        if(!s.before){if(!ReadBank(a,s.bank)||!Original(a,"before",true))return true;const auto& used=s.bank.at(s.packet);if(!Require(a,used.meta.owner==2&&used.time==750&&used.counter>0&&used.counter<750&&used.refreshing&&!used.active&&b->mSunMoney==475&&bank.activeMask==7,"Roster initial live cooldown/balance missing"))return true;s.before=true;LogBank(a,"before",s.bank);}
        if(!Capture(a,"before")||!Barrier(a,"BEFORE",7))return true;
        if(s.slot==2){NetTrace("SMOKE_ROSTER240_DROP_SAFE role=client slot=2 epoch=%u packet=%u id=%u token=%u sun=475 mask=7 resultScope=departing-peer-only",s.epoch,s.packet,s.original.id,s.original.token);a->CoopSmokeWriteResult(true,nullptr);return true;}Advance(a,7);
    }
    else if(s.stage==7)
    {if(a->mNetManager->GetPlayerCount()!=2||bank.activeMask!=3)return true;if(!Require(a,s.slot<2&&b->mPaused&&b->mSunMoney==475,"Roster intermediate drop lost remaining peer or changed wallet/pause"))return true;if(After(a,true))Advance(a,8);}
    else if(s.stage==8)
    {
        if(!b->mPaused){if(s.after&&Has(a,0,Tag("POST"))&&Has(a,1,Tag("POST"))){s.resumeTick=b->mMainCounter;Advance(a,9);}return true;}
        if(!After(a,true)||Samples(a)-s.postSample<8||!Capture(a,"after_drop")||!Barrier(a,"POST",3))return true;
        if(!s.postRecorded){s.postRecorded=true;NetTrace("SMOKE_ROSTER240_POST_SNAPSHOTS role=%s slot=%d epoch=%u start=%u now=%u fresh=%u",Role(a),s.slot,s.epoch,s.postSample,Samples(a),Samples(a)-s.postSample);}
        if(a->NetIsHost()&&!s.resumeRequested){s.resumeRequested=Key(a);return true;}
    }
    else if(s.stage==9)
    {if(!Original(a,"reused",false))return true;auto* card=Card(b,s.packet);if(card&&card->mActive&&!card->mRefreshing&&!s.readyRecorded){const int remaining=s.bank.at(s.packet).time-s.bank.at(s.packet).counter;if(a->NetIsHost()&&!Require(a,b->mMainCounter-s.resumeTick>=remaining,"Roster acquired card bypassed its preserved natural cooldown"))return true;s.readyRecorded=true;NetTrace("SMOKE_ROSTER240_CD_READY role=%s slot=%d packet=%u owner=%d preservedRemaining=%d hostElapsed=%d natural=1",Role(a),s.slot,s.packet,s.newOwner,remaining,a->NetIsHost()?b->mMainCounter-s.resumeTick:-1);}if(Purchase(a,true)){s.finalSample=Samples(a);Advance(a,10);}}
    else if(s.stage==10)
    {
        if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}if(!b->mPaused||Samples(a)-s.finalSample<2)return true;
        auto* fresh=Actor(b,s.fresh);const auto* d=GetNativePlantData(fresh);if(!After(a,false)||!Require(a,fresh&&d&&d->owner==s.newOwner&&d->packetId==s.packet&&fresh->mPlantHealth==300&&fresh->mPlantMaxHealth==300&&b->mSunMoney==450&&ctx->money==3000,"Roster final acquired-card actor/ledger differs")||!Capture(a,"complete")||!Barrier(a,"FINAL",3))return true;
        Original(a,"complete",true);NetTrace("SMOKE_ROSTER240_COMPLETE role=%s slot=%d epoch=%u oldId=%u oldToken=%u oldOwner=2 packet=%u newOwner=%d newId=%u newToken=%u mask=3 quotas=5,5,0,0 packets=10 sun=450 postDropSnapshots=8 naturalCD=1 pausedCDPreserved=1 paidCharges=2 birthPreserved=1 actorCounterRNGWrites=0 phase=Playing Choosing=NotExecuted Intro=NotExecuted",Role(a),s.slot,s.epoch,s.original.id,s.original.token,s.packet,s.newOwner,s.fresh.id,s.fresh.token);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
