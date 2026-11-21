#include "NetFusion270Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeEffects269.h"
#include "../Fusion/FusionZombieBatch270Rules.h"
#include "../Fusion/NativeSpecialZombie269.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
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
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref{uint16_t id=0;uint32_t token=0;};
constexpr uint16_t kinds[]={15,104,14,18};constexpr int rows[]={0,1,2,4};
struct PlantSpec{uint16_t kind;int col,row;};
constexpr PlantSpec controls[]={{0,1,0},{0,1,1},{3,2,1},{12,5,2},{0,5,2},{0,6,4},{17,4,4}};
struct State
{
 int group=0,stage=0,waiting=0,entry=-1,start=-1,mask=0;uint32_t epoch=0,since=0,chat=0,wheel=0,samples=0,diagnostic=0;
 bool configured=false,setup=false,pauseRequested=false,resumeRequested=false;
 std::array<Ref,4> npcs{};std::array<Ref,7> plants{};std::array<float,4> initialX{};
 std::array<bool,4> moved{},retired{};std::array<bool,2> fell{},angry{};
 std::array<Sexy::Rect,7> plantRects{};std::array<int,7> initialPlantHP{};
 bool head=false,shot=false,queued=false,impact=false,subContact=false,subCrush=false,driverContact=false,driverCrush=false,driverStopped=false,driverRetired=false;
 int driverStopAge=-1,driverDeadline=-1,driverDueBoard=-1;std::set<uint16_t> projectileIds;
 std::set<std::string> frames;std::map<std::string,int> draws;
}s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_ZOMBIE270_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "Z270_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Zombie270 chat proof exceeded native limit"))return false;if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Zombie270 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Zombie270 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Zombie270 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_zombie270_g"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Zombie270 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_ZOMBIE270_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Zombie* Z(Board* b,int i){Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==s.npcs[i].id){const auto* d=GetNativeSourceZombieData(z);if(d&&d->instanceToken==s.npcs[i].token)return z;}return nullptr;}
Plant* P(Board* b,int i){Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==s.plants[i].id){const auto* d=GetNativePlantData(p);if(d&&d->instanceToken==s.plants[i].token)return p;}return nullptr;}
Plant* At(Board* b,const PlantSpec& spec){Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==spec.col&&p->mRow==spec.row){const auto* d=GetNativePlantData(p);if(d&&d->kind==spec.kind)return p;}return nullptr;}
bool Preflight(LawnApp* a)
{
 auto* b=a->mBoard;unsigned errors=0;std::string error;
 for(uint16_t kind:kinds){error.clear();const bool model=EnsureNativeSourceZombieVisuals(b,kind,&error);const auto* p=FindFusionZombieBatch270Profile(kind);const auto* live=FindFusionZombieProfile(kind);const bool profile=p&&live&&GetNativeSourceZombieCarrier(kind)==int(p->carrier);
  NetTrace("SMOKE_ZOMBIE270_PREFLIGHT npc=%u model=%d profile=%d error=%s actors=0",kind,model,profile,error.c_str());errors+=!model||!profile;}
 for(uint16_t kind:{uint16_t(0),uint16_t(3),uint16_t(12),uint16_t(17)}){error.clear();const bool ready=EnsureNativePlantVisuals(b,kind,&error);errors+=!ready;NetTrace("SMOKE_ZOMBIE270_PREFLIGHT plant=%u ready=%d error=%s actors=0",kind,ready,error.c_str());}
 error.clear();const bool projectile=EnsureNativeProjectileVisuals(b,3,&error);errors+=!projectile;NetTrace("SMOKE_ZOMBIE270_PREFLIGHT projectile=3 ready=%d error=%s actors=0",projectile,error.c_str());
 for(uint16_t kind:{uint16_t(35),uint16_t(36)}){error.clear();const bool effect=EnsureNativeSourceEffect269(b,kind,&error);errors+=!effect;NetTrace("SMOKE_ZOMBIE270_PREFLIGHT effect=%u ready=%d error=%s actors=0",kind,effect,error.c_str());}
 return Require(a,!errors,"Zombie270 grouped native resource/profile preflight failed; see all PREFLIGHT errors");
}
bool Setup(LawnApp* a)
{
 auto* b=a->mBoard;if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Zombie270 requires initial empty world")||!Require(a,ConfigureNativeFusionBackground(b,BACKGROUND_3_POOL),"Zombie270 Pool configuration refused")||!Preflight(a))return false;
 for(const auto& spec:controls){NativePlantData d;d.kind=spec.kind;d.owner=LawnNet::NET_SHARED_OWNER;auto* p=b->AddPlant(spec.col,spec.row,GetNativeSeedForPlant(spec.kind),SEED_NONE,&d);if(!Require(a,p&&!p->mDead&&GetNativePlantData(p),"Zombie270 actual initial plant factory failed"))return false;}
 for(int i=0;i<4;++i)
 {
  NativeSourceZombieSpawnRequest request;request.kind=kinds[i];request.row=rows[i];request.positioned=true;request.shadowX=float(b->GetCellCenterX(7,rows[i]));
  auto* z=SpawnNativeSourceZombie(b,request);auto* d=GetNativeSourceZombieData(z);const auto* expected=FindFusionZombieBatch270Profile(kinds[i]);
  if(!Require(a,z&&d&&expected&&z->mNetId&&d->instanceToken&&z->mBodyMaxHealth==expected->maxHealth,"Zombie270 canonical source factory/HP/profile failed"))return false;
  if(i<2)z->mShieldHealth=40; // Initial controlled low shield; maximum stays source1100/200.
  if(i==3){d->health=1000;z->mBodyHealth=1000;} // Initial source float+native projection, maximum2700.
  NetTrace("SMOKE_ZOMBIE270_INITIAL kind=%u id=%u token=%u carrier=%d row=%d health=%.9g maximum=%d shield=%d/%d sourceRNGUnchanged=1 controlledInitialProps=1 postSetupActorWrites=0",d->kind,z->mNetId,d->instanceToken,int(z->mZombieType),z->mRow,d->health,z->mBodyMaxHealth,z->mShieldHealth,z->mShieldMaxHealth);
 }
 s.setup=true;s.pauseRequested=Escape(a);return true;
}
bool Bind(LawnApp* a)
{
 auto* b=a->mBoard;if(!Require(a,b->mBackground==BACKGROUND_3_POOL&&GetNativeFusionRowCount(b)==6,"Zombie270 authoritative Pool6 terrain missing"))return false;for(int i=0;i<4;++i)if(!s.npcs[i].id){Zombie* z=nullptr;while(b->IterateZombies(z)){const auto* d=GetNativeSourceZombieData(z);if(!z->mDead&&d&&d->kind==kinds[i]&&z->mRow==rows[i]){s.npcs[i]={z->mNetId,d->instanceToken};s.initialX[i]=a->NetIsClient()?z->mNetTargetX:z->mPosX;break;}}if(!s.npcs[i].id)return false;}
 for(int i=0;i<7;++i)if(!s.plants[i].id){auto* p=At(b,controls[i]);const auto* d=GetNativePlantData(p);if(!p||!d)return false;s.plants[i]={p->mNetId,d->instanceToken};s.plantRects[i]=p->GetPlantRect();s.initialPlantHP[i]=p->mPlantHealth;}
 return true;
}
bool Identity(LawnApp* a)
{LawnNet::NetBuffer data;for(int i=0;i<4;++i){data.WriteU16(kinds[i]);data.WriteU16(s.npcs[i].id);data.WriteU32(s.npcs[i].token);}for(const auto& p:s.plants){data.WriteU16(p.id);data.WriteU32(p.token);}char name[32];sprintf_s(name,"IDS_%08x",LawnNet::NetFnv1a32(data.Data(),data.Size()));return Barrier(a,name);}
bool Observe(LawnApp* a)
{
 auto* b=a->mBoard;const int elapsed=s.start<0?0:b->mMainCounter-s.start;
 for(int i=0;i<4;++i)
 {
  auto* z=Z(b,i);const auto* d=GetNativeSourceZombieData(z);
  if(!z||!d){if(i==3&&s.driverStopped){if(a->NetIsHost()&&!Require(a,b->mMainCounter>=s.driverDueBoard,"Zombie270 driver retired before native2s deadline"))return false;s.driverRetired=s.retired[i]=true;}
   else if(s.mask&(1<<i))s.retired[i]=true;else {char why[128];sprintf_s(why,"Zombie270 source lost without prior outcome kind=%u id=%u token=%u",kinds[i],s.npcs[i].id,s.npcs[i].token);return Require(a,false,why);}continue;}
  const float x=a->NetIsClient()?z->mNetTargetX:z->mPosX;if(std::abs(x-s.initialX[i])>1)s.moved[i]=true;
  if(i<2){int status=0;if(GetNativeSpecialZombie269Status(z,status)){if(z->mShieldHealth==0&&(status==5||status==6))s.fell[i]=true;if(status==6)s.angry[i]=true;}}
  if(i==1)
  {if(a->NetIsHost()){const auto* head=a->ReanimationTryToGet(z->mSpecialHeadReanimID);if(head&&head->mFrameCount>0&&z->mBossHeadCounter>0&&head->mAnimRate>0)s.head=true;}
   else if(d->paperHead.mPresent&&d->paperHead.mFrameCount>0&&d->paperShootEvent>0&&d->paperHead.mAnimRate>0)s.head=true;}
  if(i>=2)
  {const int pindex=i==2?4:5;const auto& r=s.plantRects[pindex];const bool touches=NativeSpecialZombie269VehicleTouchesRect(z,GetNativeSourceZombie269NodePoint,float(r.mX),float(r.mY),float(r.mWidth),float(r.mHeight));
   if(i==2){if(!Require(a,z->mInPool&&NativeSpecialZombie269VehicleTerrainValid(z),"Zombie270 Submarine source water admission changed"))return false;s.subContact|=touches;if(s.subContact&&!P(b,pindex))s.subCrush=true;}
   else{s.driverContact|=touches;if(s.driverContact&&!P(b,pindex))s.driverCrush=true;}
   if(i==3&&(a->NetIsHost()?z->mFlatTires:d->vehicleStopped)){if(!s.driverStopped){s.driverStopAge=a->NetIsHost()?z->mZombieAge:-1;s.driverDeadline=a->NetIsHost()?z->mSummonCounter:-1;s.driverDueBoard=a->NetIsHost()?b->mMainCounter+(z->mSummonCounter-z->mZombieAge):-1;NetTrace("SMOKE_ZOMBIE270_DRIVER_STOP role=%s id=%u token=%u age=%d deadline=%d hostClock=%d realCaltrop17=1",Role(a),z->mNetId,d->instanceToken,s.driverStopAge,s.driverDeadline,a->NetIsHost());}
    if(a->NetIsHost()&&!z->IsDeadOrDying()&&!Require(a,z->mSummonCounter>=z->mZombieAge&&z->mSummonCounter-z->mZombieAge<=200,"Zombie270 invalid native driver pending deadline"))return false;s.driverStopped=true;}}
 }
 Projectile* p=nullptr;while(b->IterateProjectiles(p))if(!p->mDead)if(const auto* d=GetNativeProjectileData(p);d&&d->kind==3&&d->damage==300&&d->specialType==9&&d->isZombieBullet&&p->mRow==1){s.shot=true;s.projectileIds.insert(p->mNetId);}
 if(a->NetIsHost())for(const auto& entry:GetNativeContext(b)->superCherryStarts)if(entry.second.targetId==s.plants[2].id&&entry.second.targetGeneration==s.plants[2].token)s.queued=true;
 if(auto* target=P(b,2);target&&target->mPlantHealth<=s.initialPlantHP[2]-300)s.impact=true;
 if(s.moved[0]&&s.fell[0]&&s.angry[0])s.mask|=1;
 if(s.moved[1]&&s.fell[1]&&s.angry[1]&&s.head&&s.shot&&s.impact&&(!a->NetIsHost()||s.queued))s.mask|=2;
 if(s.moved[2]&&s.subCrush)s.mask|=4;
 if(s.moved[3]&&s.driverCrush&&s.driverStopped&&s.driverRetired)s.mask|=8;
 if(!s.diagnostic||GetTickCount()-s.diagnostic>5000){s.diagnostic=GetTickCount();NetTrace("SMOKE_ZOMBIE270_PROOF role=%s elapsed=%d mask=%02x moved=%d%d%d%d fall=%d%d angry=%d%d headRaw=%d shot3=%d queueHost=%d impact=%d subPolygon=%d subCrush=%d driverPolygon=%d driverCrush=%d driverStopped=%d driverRetired=%d sourceAIParity=NotClaimed",Role(a),elapsed,s.mask,s.moved[0],s.moved[1],s.moved[2],s.moved[3],s.fell[0],s.fell[1],s.angry[0],s.angry[1],s.head,s.shot,s.queued,s.impact,s.subContact,s.subCrush,s.driverContact,s.driverCrush,s.driverStopped,s.driverRetired);}
 if(s.start>=0&&!b->mPaused){if(elapsed>=200&&!Capture(a,"action"))return false;if(elapsed>=500&&!Capture(a,"late"))return false;}
 return true;
}
bool Final(LawnApp* a)
{LawnNet::NetBuffer data;for(int i=0;i<4;++i){auto* z=Z(a->mBoard,i);const auto* d=GetNativeSourceZombieData(z);data.WriteU16(s.npcs[i].id);data.WriteU32(s.npcs[i].token);data.WriteU8(z?1:0);data.WriteI32(z?z->mBodyHealth:-1);data.WriteI32(z?z->mShieldHealth:-1);data.WriteU8(z&&(a->NetIsHost()?z->mFlatTires:d&&d->vehicleStopped)?1:0);data.WriteU8(d&&d->kind==104?uint8_t(a->NetIsHost()?z->mSummonCounter:d->paperShootTicks):0);data.WriteU8(d&&d->kind==104?uint8_t(a->NetIsHost()?z->mBossHeadCounter:d->paperShootEvent):0);
  data.WriteF32(d?d->health:-1);if(d&&d->kind==104){const auto* head=a->NetIsHost()?a->ReanimationTryToGet(z->mSpecialHeadReanimID):nullptr;data.WriteU16(head?uint16_t(head->mFrameStart):d->paperHead.mFrameStart);data.WriteU16(head?uint16_t(head->mFrameCount):d->paperHead.mFrameCount);data.WriteF32(head?head->mAnimTime:d->paperHead.mAnimTime);}}
 for(int i=0;i<7;++i){auto* p=P(a->mBoard,i);data.WriteU16(s.plants[i].id);data.WriteU32(s.plants[i].token);data.WriteI32(p?p->mPlantHealth:-1);}char tag[32];sprintf_s(tag,"FINAL_%08x",LawnNet::NetFnv1a32(data.Data(),data.Size()));return Barrier(a,tag);}
}
namespace LawnNet
{
bool NetSmokeUpdateFusion270(LawnApp* a)
{
 const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_ZOMBIE270"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
 if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>200000){a->CoopSmokeFail("Zombie270 bounded grouped deadline; see source proof mask");return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2)return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"Z270_BEGIN");}else if(Has(a,1,"Z270_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
 if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Zombie270 requires matching native validation world"))return true;
 const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
 if(s.stage==1){if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
  int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
  if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(std::size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Zombie270 chooser source card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}if(bank.packets.size()==10)Advance(a,2);}
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
  if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
  if(!button->IsMouseOver()){SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(a->mSDLWindow);motion.motion.x=x;motion.motion.y=y;Require(a,SDL_PushEvent(&motion),"Zombie270 Ready hover failed");}
  else if(Click(a,x,y))Advance(a,3);}
 else if(s.stage==3){if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);}
 else if(s.stage==4){if(!b->mPaused||!Bind(a))return true;if(!s.samples){s.samples=Samples(a);return true;}if(Samples(a)-s.samples<2||!Identity(a)||!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;if(s.start<0)s.start=b->mMainCounter;
  if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);if(s.resumeRequested||a->NetIsClient())Advance(a,5);}
 else if(s.stage==5){if(b->mPaused||!Observe(a))return true;if(s.mask!=15||b->mMainCounter-s.start<800||!s.frames.contains("fusion_zombie270_g0_late")||!Barrier(a,"EFFECTS"))return true;s.pauseRequested=false;Advance(a,6);}
 else if(s.stage==6){if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);if(b->mPaused){s.samples=Samples(a);Advance(a,7);}}
 else if(s.stage==7){if(!b->mPaused||Samples(a)-s.samples<2||!Observe(a)||!Final(a)||!Capture(a,"complete")||!Barrier(a,"COMPLETE"))return true;
  NetTrace("SMOKE_ZOMBIE270_CASE role=%s group=0 epoch=%u kinds=4 mask=0f finalSameState=1 snapshots=%u initialProps=controlled postSetupActorWrites=0",Role(a),s.epoch,Samples(a));
  NetTrace("SMOKE_ZOMBIE270_COMPLETE role=%s groups=1 sourceZombies=4 selectedGroup=0 freshFrames=4 actualPause=1 sourceNPCParity=NotClaimed queueClient=NotReplicated acceptedAdded=0 postSetupActorWrites=0",Role(a));a->CoopSmokeWriteResult(true,nullptr);}
 return true;
}
}
