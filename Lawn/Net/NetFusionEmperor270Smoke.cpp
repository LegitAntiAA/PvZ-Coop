#include "NetFusionEmperor270Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeLastPlants269.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <iterator>
#include <vector>
#include <set>
#include <string>
namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref{uint16_t id=0;uint32_t token=0;};
struct Child{Ref ref;uint16_t kind=0;float birthSeconds=0;int birthTick=-1;};
constexpr int parentRows[]={0,2,4};
struct State
{
 int group=0,stage=0,waiting=0,entry=-1,start=-1,mask=0,birthTick=-1;uint32_t epoch=0,since=0,chat=0,wheel=0,samples=0,diagnostic=0;
 bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,bagZero=false,biteRetired=false;
 std::array<Ref,3> parents{};std::array<Ref,5> biters{};
 std::array<bool,5> mindControlled{};std::array<bool,2> phaseSeen{};
 unsigned lastBag=5;
 std::array<std::map<uint16_t,Child>,2> children;
 std::set<std::string> frames;std::map<std::string,int> draws;
}s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_EMPEROR270_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "E270_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Emperor270 chat proof exceeded native limit"))return false;if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Emperor270 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Emperor270 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Emperor270 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_emperor270_g"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Emperor270 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_EMPEROR270_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* P(Board* b,int i)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==s.parents[i].id){const auto* d=GetNativePlantData(p);if(d&&d->instanceToken==s.parents[i].token)return p;}return nullptr;}
Zombie* Z(Board* b,Ref ref)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==ref.id){const auto* d=GetNativeSourceZombieData(z);if(d&&d->instanceToken==ref.token)return z;}return nullptr;}
bool Preflight(LawnApp* a)
{
 auto* b=a->mBoard;unsigned errors=0;std::string error;
 for(uint16_t kind:{uint16_t(0),uint16_t(14),uint16_t(15),uint16_t(109),uint16_t(18),uint16_t(104)})
 {error.clear();const bool model=EnsureNativeSourceZombieVisuals(b,kind,&error);const bool profile=FindFusionZombieProfile(kind)&&GetNativeSourceZombieCarrier(kind)>=0;
  errors+=!model||!profile;NetTrace("SMOKE_EMPEROR270_PREFLIGHT npc=%u model=%d profile=%d error=%s actors=0 RNGDraw=0",kind,model,profile,error.c_str());}
 for(uint16_t kind:{uint16_t(900),uint16_t(12)})
 {error.clear();const bool okay=EnsureNativePlantVisuals(b,kind,&error)&&GetNativeSeedForPlant(kind)!=SEED_NONE;errors+=!okay;NetTrace("SMOKE_EMPEROR270_PREFLIGHT plant=%u ready=%d error=%s actors=0",kind,okay,error.c_str());}
 for(int row:{0,2,4}){const bool ready=CanPlaceNativeEmperor269(b,row);errors+=!ready;NetTrace("SMOKE_EMPEROR270_PREFLIGHT fullFactoryRow=%d ready=%d RNGDraw=0 actors=0",row,ready);}
 return Require(a,!errors,"Emperor270 complete native RNG roster/resource preflight failed; see all errors");
}
bool Setup(LawnApp* a)
{
 auto* b=a->mBoard;if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Emperor270 requires initial empty world")||
  !Require(a,ConfigureNativeFusionBackground(b,BACKGROUND_3_POOL),"Emperor270 actual Pool configuration refused")||!Preflight(a))return false;
 NativePlantData lily;lily.kind=12;lily.owner=LawnNet::NET_SHARED_OWNER;
 if(!Require(a,b->AddPlant(2,2,GetNativeSeedForPlant(12),SEED_NONE,&lily)!=nullptr,"Emperor270 real initial Lily factory failed"))return false;
 for(int i=0;i<3;++i)
 {NativePlantData d;d.kind=900;d.owner=LawnNet::NET_SHARED_OWNER;auto* p=b->AddPlant(2,parentRows[i],GetNativeSeedForPlant(900),SEED_NONE,&d);const auto* live=GetNativePlantData(p);
  if(!Require(a,p&&!p->mDead&&live&&live->batchBagCount==5&&live->productionTicks==3000,"Emperor270 normalD2 initial source bag5/3000 clock failed; fastStart forbidden"))return false;
  s.parents[i]={p->mNetId,live->instanceToken};
  NetTrace("SMOKE_EMPEROR270_INITIAL_PARENT index=%d id=%u token=%u row=%d col=2 health=%d bag=%u production=%d fastStart=0",i,p->mNetId,live->instanceToken,p->mRow,p->mPlantHealth,live->batchBagCount,live->productionTicks);}
 s.birthTick=b->mMainCounter;const auto plantRect=P(b,2)->GetPlantRect();
 for(int i=0;i<5;++i)
 {NativeSourceZombieSpawnRequest r;r.kind=0;r.row=4;r.positioned=true;r.shadowX=float(b->GetCellCenterX(4,4));auto* z=SpawnNativeSourceZombie(b,r);auto* d=GetNativeSourceZombieData(z);
  if(!Require(a,z&&d&&!z->mMindControlled&&z->mNetId&&d->instanceToken,"Emperor270 actual initial source0 eater failed"))return false;
  // Initial contact lets all five acquire the plant before the first conversion;
  // distant biters otherwise choose the converted zombies as combat targets.
  // HP, AI, bite events, speed, RNG and clocks stay native.
  const auto attack=z->GetZombieAttackRect();z->mPosX+=float(plantRect.mX+plantRect.mWidth/2-attack.mX-attack.mWidth/2);z->mX=int(z->mPosX);
  if(!Require(a,z->FindPlantTarget(ATTACKTYPE_CHEW)==P(b,2),"Emperor270 initial native20px plant-target admission failed"))return false;
  s.biters[i]={z->mNetId,d->instanceToken};const auto placed=z->GetZombieAttackRect();
  NetTrace("SMOKE_EMPEROR270_INITIAL_BITER index=%d id=%u token=%u kind=0 body=%d/%d attackLeft=%d parentRight=%d gap=%d sourceOriginSpeed=%.9g initialHealthControlled=0 postSetupActorWrites=0",i,z->mNetId,d->instanceToken,z->mBodyHealth,z->mBodyMaxHealth,placed.mX,plantRect.mX+plantRect.mWidth,placed.mX-plantRect.mX-plantRect.mWidth,d->originSpeed);}
 s.setup=true;s.pauseRequested=Escape(a);return true;
}
bool Bind(LawnApp* a)
{
 auto* b=a->mBoard;if(!Require(a,b->mBackground==BACKGROUND_3_POOL&&GetNativeFusionRowCount(b)==6,"Emperor270 canonical Pool6 missing"))return false;
 for(int i=0;i<3;++i)if(!s.parents[i].id)
 {Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==2&&p->mRow==parentRows[i])if(const auto* d=GetNativePlantData(p);d&&d->kind==900){s.parents[i]={p->mNetId,d->instanceToken};break;}}
 std::vector<Ref> biters;Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mRow==4)if(const auto* d=GetNativeSourceZombieData(z);d&&d->kind==0&&!d->sourceParentId)biters.push_back({z->mNetId,d->instanceToken});
 std::sort(biters.begin(),biters.end(),[](Ref a,Ref b){return a.id<b.id;});if(biters.size()!=5)return false;
 for(int i=0;i<5;++i)s.biters[i]=biters[i];
 for(int i=0;i<3;++i){auto* p=P(b,i);const auto* d=GetNativePlantData(p);if(!p||!d)return false;if(!Require(a,d->batchBagCount==5&&d->productionTicks>2900&&d->productionTicks<=3000,"Emperor270 paused baseline lost five uses or first30s clock"))return false;}
 return true;
}
bool Identity(LawnApp* a)
{LawnNet::NetBuffer bytes;for(auto p:s.parents){bytes.WriteU16(p.id);bytes.WriteU32(p.token);}for(auto z:s.biters){bytes.WriteU16(z.id);bytes.WriteU32(z.token);}char name[32];sprintf_s(name,"IDS_%08x",LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));return Barrier(a,name);}
bool Observe(LawnApp* a)
{
 auto* b=a->mBoard;const int elapsed=s.start<0?0:b->mMainCounter-s.start;
 for(int i=0;i<5;++i)
 {auto* z=Z(b,s.biters[i]);if(!z){if(!Require(a,s.mindControlled[i],"Emperor270 original eater retired before its actual MC bite"))return false;continue;}
  if(z->mMindControlled&&!s.mindControlled[i]){s.mindControlled[i]=true;NetTrace("SMOKE_EMPEROR270_BITE_MC role=%s index=%d id=%u token=%u elapsed=%d",Role(a),i,z->mNetId,s.biters[i].token,elapsed);}}
 unsigned consumed=unsigned(std::count(s.mindControlled.begin(),s.mindControlled.end(),true));auto* bitten=P(b,2);const auto* bag=GetNativePlantData(bitten);
 if(bag){if(!Require(a,bag->batchBagCount<=s.lastBag&&bag->batchBagCount<=5,"Emperor270 source five-use bag increased"))return false;s.lastBag=bag->batchBagCount;s.bagZero|=!bag->batchBagCount;}
 else if(consumed==5){s.biteRetired=true;s.bagZero=true;} // Each of the five distinct baseline identities actually became MC; snapshots may skip the final one-tick bag0.
 else return Require(a,false,"Emperor270 parent lost before all five actual MC bites");
 for(int i=0;i<2;++i)
 {auto* p=P(b,i);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d&&d->kind==900&&d->instanceToken==s.parents[i].token,"Emperor270 automatic source parent unexpectedly retired"))return false;
  if(d->shootSeconds>=0)s.phaseSeen[i]=true;
  if(!Require(a,d->batchBagCount==5,"Emperor270 auto summons consumed source bite uses"))return false;
  Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead)if(const auto* child=GetNativeSourceZombieData(z);child&&child->sourceParentId==s.parents[i].id&&child->sourceParentToken==s.parents[i].token)
  {
   const bool kind=i==0?(child->kind==15||child->kind==109||child->kind==18||child->kind==104):child->kind==14;
   if(!Require(a,kind&&z->mMindControlled&&!child->dieWithParent&&z->mRow==parentRows[i]&&child->instanceToken&&z->mNetId,"Emperor270 real summon kind/friendly/parent generation mismatch"))return false;
   if(!s.children[i].contains(z->mNetId))
   {if(!Require(a,d->idleSeconds>=29.99f,"Emperor270 child appeared before natural first3000ticks"))return false;
    if(a->NetIsHost()&&!Require(a,b->mMainCounter-s.birthTick>=3000,"Emperor270 host child born before first3000ticks"))return false;
    s.children[i].emplace(z->mNetId,Child{{z->mNetId,child->instanceToken},child->kind,d->idleSeconds,a->NetIsHost()?b->mMainCounter:-1});
    NetTrace("SMOKE_EMPEROR270_CHILD role=%s parentIndex=%d parent=%u parentToken=%u id=%u token=%u kind=%u row=%d parentSeconds=%.9g production=%d friendly=1 fullRNGUnfiltered=1",Role(a),i,p->mNetId,d->instanceToken,z->mNetId,child->instanceToken,child->kind,z->mRow,d->idleSeconds,d->productionTicks);}
  }
  if(s.children[i].size()>=2)
  {const auto& first=s.children[i].begin()->second;const auto& second=std::next(s.children[i].begin())->second;const float interval=second.birthSeconds-first.birthSeconds;
   // The host proves exact source timing. Client observations may be delayed
   // by snapshot delivery or frame capture; their timestamps are not births.
   if(a->NetIsHost()&&!Require(a,second.birthTick-first.birthTick==3000,"Emperor270 actual host automatic repeat is not3000ticks"))return false;
   if(!Require(a,interval>0,"Emperor270 child observations reversed the authoritative source clock"))return false;
   if(s.phaseSeen[i])s.mask|=1<<i;}
 }
 if(consumed==5&&s.bagZero&&s.biteRetired)s.mask|=4;
 if(!s.diagnostic||GetTickCount()-s.diagnostic>5000){s.diagnostic=GetTickCount();NetTrace("SMOKE_EMPEROR270_PROOF role=%s elapsed=%d mask=%02x land=%u water=%u phases=%d%d MCbites=%u lastBag=%u bagZero=%d parentRetired=%d first3000=natural repeat30=natural RNGFilter=0 postSetupActorWrites=0",Role(a),elapsed,s.mask,unsigned(s.children[0].size()),unsigned(s.children[1].size()),s.phaseSeen[0],s.phaseSeen[1],consumed,s.lastBag,s.bagZero,s.biteRetired);}
 if(s.start>=0&&!b->mPaused){if(elapsed>=3200&&!Capture(a,"action"))return false;if(elapsed>=6100&&!Capture(a,"late"))return false;}
 return true;
}
bool Final(LawnApp* a)
{
 LawnNet::NetBuffer bytes;for(int i=0;i<3;++i){auto* p=P(a->mBoard,i);const auto* d=GetNativePlantData(p);bytes.WriteU16(s.parents[i].id);bytes.WriteU32(s.parents[i].token);bytes.WriteU8(p?1:0);bytes.WriteI32(p?p->mPlantHealth:-1);bytes.WriteI32(d?d->productionTicks:-1);bytes.WriteI32(d?d->batchBagCount:-1);bytes.WriteF32(d?d->shootSeconds:-1);}
 auto put=[&](Ref ref){auto* z=Z(a->mBoard,ref);const auto* d=GetNativeSourceZombieData(z);bytes.WriteU16(ref.id);bytes.WriteU32(ref.token);bytes.WriteU8(z?1:0);bytes.WriteI32(z?z->mBodyHealth:-1);bytes.WriteU8(z&&z->mMindControlled?1:0);bytes.WriteU16(d?d->sourceParentId:0);bytes.WriteU32(d?d->sourceParentToken:0);};
 for(auto ref:s.biters)put(ref);for(const auto& cohort:s.children)for(const auto& [id,child]:cohort)put(child.ref);
 char tag[32];sprintf_s(tag,"FINAL_%08x",LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));return Barrier(a,tag);
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionEmperor270(LawnApp* a)
{
 const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_EMPEROR270"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
 if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>200000){a->CoopSmokeFail("Emperor270 bounded grouped deadline; see source proof mask");return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2)return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"E270_BEGIN");}else if(Has(a,1,"E270_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
 if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Emperor270 requires matching native validation world"))return true;
 const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
 if(s.stage==1){if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
  int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
  if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(std::size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Emperor270 chooser source card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}if(bank.packets.size()==10)Advance(a,2);}
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
  if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
  if(!button->IsMouseOver()){SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(a->mSDLWindow);motion.motion.x=x;motion.motion.y=y;Require(a,SDL_PushEvent(&motion),"Emperor270 Ready hover failed");}
  else if(Click(a,x,y))Advance(a,3);}
 else if(s.stage==3){if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);}
 else if(s.stage==4){if(!b->mPaused||!Bind(a))return true;if(!s.samples){s.samples=Samples(a);return true;}if(Samples(a)-s.samples<2||!Identity(a)||!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;if(s.start<0)s.start=b->mMainCounter;
  if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);if(s.resumeRequested||a->NetIsClient())Advance(a,5);}
 else if(s.stage==5){if(b->mPaused||!Observe(a))return true;if(s.mask!=7||b->mMainCounter-s.start<6200||!s.frames.contains("fusion_emperor270_g0_late")||!Barrier(a,"EFFECTS"))return true;s.pauseRequested=false;Advance(a,6);}
 else if(s.stage==6){if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);if(b->mPaused){s.samples=Samples(a);Advance(a,7);}}
 else if(s.stage==7){if(!b->mPaused||Samples(a)-s.samples<2||!Observe(a)||!Final(a)||!Capture(a,"complete")||!Barrier(a,"COMPLETE"))return true;
  NetTrace("SMOKE_EMPEROR270_CASE role=%s group=0 epoch=%u kinds=1 mask=07 finalSameState=1 snapshots=%u initialProps=controlled postSetupActorWrites=0",Role(a),s.epoch,Samples(a));
  NetTrace("SMOKE_EMPEROR270_COMPLETE role=%s groups=1 sourcePlants=1 parentActors=3 selectedGroup=0 freshFrames=4 actualPause=1 sourceNPCParity=NotClaimed fullLandRNG=1 fastStart=0 fiveBites=1 repeat30s=1 acceptedAdded=0 postSetupActorWrites=0",Role(a));a->CoopSmokeWriteResult(true,nullptr);}
 return true;
}
}
