#include "FusionNativeBossBungee366.h"
#include "FusionLateZombie363Rules.h"
#include "FusionNativeHeavyZombie313B.h"
#include "FusionNextZombie343Rules.h"
#include "FusionNativeNextZombie343.h"
#include "FusionSourceZombieTypes313.h"
#include "FusionBasePlantRules.h"
#include "FusionNative.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombies.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
#include "FusionNativePlantDamage.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../GridItem.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace LawnFusion
{
namespace
{
using Param=FusionSourceGraph313Param;using Event=FusionSourceGraph313Event;
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool Authority(const Zombie* z){return z&&NativeHeavyZombie313BKind(Kind(z))&&z->mBoard&&z->mApp&&z->mApp->mBoard==z->mBoard&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeFusionBoard(z->mBoard);}
bool Playing(const Zombie* z){return Authority(z)&&!z->mDead&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
bool Gold(uint16_t k){return k==203||k==223;}
bool Hooks(const NativeHeavyZombie313BHooks& h){return h.graphStateFor&&h.plantTraits&&h.node&&h.sourcePoint&&h.setAxis&&h.bounds&&h.landY&&h.contact&&h.bite&&h.crash&&h.prepareEvent&&h.prepareShot&&h.prepareCapture&&h.capture&&h.umbrellaBlocks&&h.magnetContents&&h.sound&&h.fallSound&&h.sourceDie&&h.sourceEffect&&h.ladderPlaced&&h.goldEvent&&h.goldUpdate;}
void Trigger(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h,Param p){auto* g=h.graphStateFor(z);if(!g){s.failed=true;return;}g->parameters.triggers|=FusionSourceGraph313Trigger(p);}
Plant* Resolve(Zombie* z,uint32_t uid){auto* p=uid?z->mBoard->mPlants.DataArrayTryToGet(uid):nullptr;return p&&!p->mDead&&p->mIsOnBoard?p:nullptr;}
FusionPogo313BView Pogo(const NativeHeavyZombie313BState& s){return {s.status,s.pogoWait,s.pogoPaused,s.pogoStarted,s.jumped,s.shadowEnabled,s.lostJumper};}
void Pogo(NativeHeavyZombie313BState& s,const FusionPogo313BView& p){s.status=p.status;s.pogoWait=p.wait;s.pogoPaused=p.paused;s.pogoStarted=p.started;s.jumped=p.jumped;s.shadowEnabled=p.shadowEnabled;s.lostJumper=p.lostJumper;}
bool Speed(Zombie* z,float& speed)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* g=GetNativeZombieGarlicData(z);
 if(!d||!TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,g&&FusionGarlicStopsWalking(*g)&&!NativeSourceDeathIgnoresGarlic313(z),speed))return false;
 speed*=d->originSpeed*GetNativeWater268GrapSpeedFactor(z);return std::isfinite(speed);
}
bool Touches(const Plant* p,float x,float y,float w,float height)
{const auto r=const_cast<Plant*>(p)->GetPlantRect();return x<=r.mX+r.mWidth&&x+w>=r.mX&&y<=r.mY+r.mHeight&&y+height>=r.mY;}
bool GroundBox(Zombie* z,Plant* p,const NativeHeavyZombie313BHooks& h,bool raised)
{
 float x=0,y=0;if(!NativeSourcePlantColliderEnabled(p)||!h.node(z,"Shadow",x,y))return false;
 const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(z->mBoard));
 return Touches(p,x-.5f*px,y-(raised?2.f:1.f)*py,px,2*py);
}
bool SelectTarget(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{
 std::vector<FusionHeavy313BPlant> actors;Plant* p=nullptr;
 while(z->mBoard->mPlants.IterateNext(p)){FusionHeavy313BPlant t;if(!h.plantTraits(p,t))return false;actors.push_back(t);}
 std::vector<uint32_t> candidates;if(!BuildFusionBungee313BCandidates(actors,candidates))return false;
 if(candidates.empty()){h.sourceDie(z,0);return true;} // SetTarget3506E0 virtual Die(0), not Destroy/Die(2).
 std::string error;if(!h.prepareCapture(z,candidates,&error))return false; // All possible outcomes BEFORE target RNG.
 const auto uid=candidates[size_t(RandRangeInt(0,int(candidates.size())-1))];p=Resolve(z,uid);
 FusionHeavy313BPlant traits;if(!p||!h.plantTraits(p,traits))return false;
 // Source targets the actual selected actor Shadow/cell; sourcePoint conversion
 // of plants belongs to the same tag callback, not a native alias rectangle.
 s.bungee.targetColumn=traits.column;s.bungee.targetRow=traits.row;s.bungee.targetX=traits.shadowX;
 s.bungee.targetY=traits.shadowY;s.bungee.markerY=FusionBungee313BInitialMarkerY(traits.row);
 z->SetRow(traits.row);float min=0,max=0,top=0;if(!h.bounds(z->mBoard,min,max,top))return false;
 return h.setAxis(z,s.bungee.targetX,14.f-traits.row*1.5f);
}
bool Catch(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{
 if(!z->mMindControlled)
 {
  std::vector<FusionHeavy313BPlant> cell;Plant* p=nullptr;
  while(z->mBoard->mPlants.IterateNext(p))if(p->mPlantCol==s.bungee.targetColumn&&p->mRow==s.bungee.targetRow)
  {FusionHeavy313BPlant t;if(!h.plantTraits(p,t))return false;cell.push_back(t);}
  std::vector<uint32_t> candidates;for(const auto& t:cell)if(FusionBungee313BStealable(t,cell))candidates.push_back(t.uid);
  if(!candidates.empty())
  {
   std::string error;if(!h.prepareCapture(z,candidates,&error))return false;
   const auto uid=candidates[size_t(RandRangeInt(0,int(candidates.size())-1))];p=Resolve(z,uid);
   if(!p||!h.capture(z,p))return false;s.capturedPlantUID=uid;h.sound(z,62);
  }
 }
 s.bungee.markerVisible=false;s.bungee.status=24;s.status=24;return true;
}
bool SetLadder(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{
 auto* p=Resolve(z,s.targetPlantUID);
 if(!p||z->mBoard->GetLadderAt(p->mPlantCol,p->mRow)){Trigger(z,s,h,Param::Back);return true;}
 auto& pool=z->mBoard->mGridItems;if(!pool.mBlock||pool.mSize>=pool.mMaxSize)return false;
 auto* ladder=z->mBoard->AddALadder(p->mPlantCol,p->mRow);if(!ladder)return false;
 s.status=0;s.ladderVisible=false;Trigger(z,s,h,Param::LadderOver);h.ladderPlaced(z,p);return true;
}
bool Shot(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{
 auto* target=Resolve(z,s.targetPlantUID);if(z->mMindControlled||!target)return true;
 float x=0,y=0;std::string error;
 if(!h.node(z,"shoot",x,y)||!h.prepareShot(z,target,&error))return false;
 if(Kind(z)==112)return ShootNativeCherryCatapult343(z,s,target,x,y,h);
 if(!TryMakeNativeHostileBasketball(z->mBoard,target,z->mRow,x,y))return false;
 if(s.ammo)--s.ammo;s.targetPlantUID=0;h.sound(z,101);if(!s.ammo)Trigger(z,s,h,Param::Walk2);return true;
}
}
bool NativeHeavyZombie313BOwnsPhaseCounter(const Zombie* z){return NativeHeavyZombie313BKind(Kind(z));}
bool InitializeNativeHeavyZombie313B(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h,const NativeSourceGraph313Hooks& graph)
{
 if(!Authority(z)||!Hooks(h)||!NativeSourceGraph313ClockTracksReady(z,graph,nullptr))return false;
 auto* gs=h.graphStateFor(z);if(!gs)return false;s={};s.initialized=true;
 const auto kind=Kind(z);if(kind==28)s.status=12;if(FusionCatapult343Kind(kind))s.ammo=kind==216?2:25;
 if(kind==32||FusionSuperPogo363Kind(kind)){s.status=14;s.jumpSpeed=RandRangeFloat(1.1f,1.4f);z->mPhaseCounter=0;}
 if(kind==33){s.status=18;s.ladderVisible=true;}
 if(kind==45){s.status=22;s.bungee.status=22;z->mPhaseCounter=250;z->mSummonCounter=200;}
 if(Gold(kind)){s.gold={};z->mSummonCounter=kind==223?13:0;}
 if(!InitializeNativeSourceGraph313(z,*h.graphStateFor(z),graph))return false;
 const char* initial=kind==28?"mine":kind==45?"idle":"walk";
 if(!PlayNativeSourceGraph313(z,*h.graphStateFor(z),0,initial,0,graph))return false;
 // Unity Start runs after the caller's synchronous SetZombie delivery option.
 // Source45 target RNG is consequently deferred until its first actual Update.
 s.worldTick=-1;return true;
}
bool GetNativeHeavyZombie313BParameters(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h,FusionSourceGraph313Parameters& out)
{
 if(!Authority(z)||!Hooks(h))return false;
 auto* gs=h.graphStateFor(z);if(!gs)return false;auto p=gs->parameters;if(!Speed(z,p.speed))return false;
 p.jumpSpeed=s.jumpSpeed;p.shooting=s.shooting;p.wait=Kind(z)==112?s.ammo<=20:s.status==19;
 p.attacking=FusionCatapult343Kind(Kind(z))?false:h.contact(z,s.status==0||Gold(Kind(z)));
 out=p;return true;
}
bool UpdateNativeHeavyZombie313B(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h,const NativeSourceGraph313Hooks& graph)
{
 if(!NativeHeavyZombie313BKind(Kind(z)))return false;if(!Playing(z)||!s.initialized||s.failed)return true;
 if(s.worldTick==z->mBoard->mMainCounter)return true;
 const bool first=s.worldTick<0;s.worldTick=z->mBoard->mMainCounter;
 if(!Hooks(h)){s.failed=true;return true;}
 const auto k=Kind(z);
 if(k==45&&first&&!s.bungee.setZombie)
 {const auto start=StartNativeBossBungee366(z,s,h);if(start==NativeBossBungee366Start::Failed||(start==NativeBossBungee366Start::NotLinked&&!SelectTarget(z,s,h))){s.failed=true;return true;}}
 if(z->mDead)return true;
 float axisX=0,axisY=0,shadowX=0,shadowY=0,min=0,max=0,top=0;
 if(!h.sourcePoint(z,"axis",axisX,axisY)||!h.sourcePoint(z,"Shadow",shadowX,shadowY)||!h.bounds(z->mBoard,min,max,top)){s.failed=true;return true;}
 if(k==28&&s.status==12)
 {
  const auto rise=FusionMiner313BCheckRise(s.status,shadowX,s.axePresent,false);
  if(rise!=FusionMiner313BRise::None)NativeMiner313BAxeLost(z,s,rise==FusionMiner313BRise::Natural,h);
 }
 if(FusionCatapult343Kind(k))
 {
  if(s.caltropDying){if(z->mPhaseCounter>0)--z->mPhaseCounter;if(!z->mPhaseCounter){h.sourceDie(z,0);return true;}}
  else if(!z->IsDeadOrDying())
  {
   s.shooting=false;Plant* selected=nullptr;float farthest=-1;
   if(FusionCatapult313BCanSearch(z->mMindControlled,shadowX,max,s.ammo))
   {Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p)){FusionHeavy313BPlant t;if(!h.plantTraits(p,t)){s.failed=true;return true;}const float d=std::abs(t.shadowX-shadowX);if(FusionCatapult313BPlantAllowed(t,z->mRow,axisX)&&d>farthest){farthest=d;selected=p;}}}
   s.shooting=selected!=nullptr;if(!s.shooting)s.targetPlantUID=0;else if(!Resolve(z,s.targetPlantUID))s.targetPlantUID=z->mBoard->mPlants.DataArrayGetID(selected);
   if(!h.crash(z)){s.failed=true;return true;}
  }
 }
 if((k==32||FusionSuperPogo363Kind(k))&&!z->IsDeadOrDying())
 {
  if(s.status==14&&!z->mMindControlled&&!s.pogoWait&&!(z->mBoard->mMainCounter&1))
  {Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p)){FusionHeavy313BPlant t;if(!h.plantTraits(p,t)){s.failed=true;return true;}if(FusionPogo313BCanPause(Pogo(s),false,z->mRow,shadowX,t)&&GroundBox(z,p,h,true)){s.pogoPaused=true;s.pogoWait=2;z->mVelX=0;break;}}}
  if(FusionSuperPogo363Kind(k))
  {
   FusionSuperPogo313BStep step;
   if(!StepFusionSuperPogo313B(Pogo(s),z->mPhaseCounter*.01f,.01f,axisX,max-2.5f,max,z->mMindControlled,step)){s.failed=true;return true;}
   Pogo(s,step.after);z->mPhaseCounter=int(std::lround(step.waitSeconds*100));
   if(step.startJump){Trigger(z,s,h,Param::Jump);z->mVelX=float(-4.25*kFusionBasePixelsX*.01);z->mVelZ=float(6*GetNativeFusionPixelsY(z->mBoard)*.01);}
   if(s.status==20)
   {
    float land=0;if(!h.landY(z->mBoard,axisX,z->mRow,land)){s.failed=true;return true;}
    const float py=float(GetNativeFusionPixelsY(z->mBoard));
    FusionSuperPogo313BMotion move;
    if(!StepFusionSuperPogo313BMotion(axisX,axisY,z->mVelX/float(kFusionBasePixelsX*.01),z->mVelZ/(py*.01f),.01f,land,move)){s.failed=true;return true;}
    if(move.landed){s.status=14;s.shadowEnabled=true;Trigger(z,s,h,Param::JumpOver);z->mVelX=z->mVelZ=0;if(!h.setAxis(z,axisX,land))s.failed=true;}
    else {if(!h.setAxis(z,axisX+z->mVelX/float(kFusionBasePixelsX),axisY+z->mVelZ/py))s.failed=true;z->mVelZ=move.velocityY*py*.01f;}
   }
  }
  if(s.status==14&&!s.pogoPaused)
  {float speed=0;if(!Speed(z,speed)){s.failed=true;return true;}const float vx=FusionPogo313BHorizontalSpeed(s.pogoWait,speed,z->mMindControlled);z->mVelX=vx*float(kFusionBasePixelsX*.01);if(!h.setAxis(z,axisX+vx*.01f,axisY))s.failed=true;}
 }
 if(k==45)
 {
  float marker=0;bool landed=false;
  if(!s.bungee.targetLanded&&s.bungee.markerVisible)
  {if(!StepFusionBungee313BMarker(s.bungee.markerY,s.bungee.targetY,.01f,marker,landed)){s.failed=true;return true;}s.bungee.markerY=marker;if(landed){s.bungee.targetLanded=true;h.sound(z,50);}}
  if(z->mSummonCounter>0&&!--z->mSummonCounter)h.sound(z,uint16_t(RandRangeInt(122,124)));
  if(s.status==22&&z->mPhaseCounter>0&&!--z->mPhaseCounter)s.status=s.bungee.status=23;
  if(s.status==0&&z->mPhaseCounter>0&&!--z->mPhaseCounter)Trigger(z,s,h,Param::Steal);
  FusionBungee313BMotion move;
  if(!StepFusionBungee313BMotion(s.bungee,axisY,.01f,top,move)){s.failed=true;return true;}
  s.bungee=move.after;s.status=s.bungee.status;
  if(!h.setAxis(z,axisX,move.rootY)){s.failed=true;return true;}
  if(move.land)z->mPhaseCounter=300;
  if(move.putZombie){s.failed=true;return true;} // Delivery resource/factory continuation is an explicit remaining seam.
  if(move.checkUmbrella){Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p))if(!p->mDead&&std::abs(p->mPlantCol-s.bungee.targetColumn)<=1&&std::abs(p->mRow-s.bungee.targetRow)<=1&&h.umbrellaBlocks(p,z)){s.status=s.bungee.status=24;break;}}
  if(move.retire){h.sourceDie(z,2);return true;}
 }
 if(Gold(k)&&!h.goldUpdate(z,s)){s.failed=true;return true;}
 if(!s.failed&&!UpdateNativeSourceGraph313(z,*h.graphStateFor(z),graph))s.failed=true;
 return true;
}
bool PreflightNativeHeavyZombie313BEvent(Zombie* z,const NativeHeavyZombie313BState&,Event e,const NativeHeavyZombie313BHooks& h,std::string* error)
{if(!Authority(z)||!Hooks(h))return false;if(e==Event::SetLadder&&(!z->mBoard->mGridItems.mBlock||z->mBoard->mGridItems.mSize>=z->mBoard->mGridItems.mMaxSize))return false;return h.prepareEvent(z,e,error);}
bool DispatchNativeHeavyZombie313BEvent(Zombie* z,NativeHeavyZombie313BState& s,Event e,int32_t,const NativeHeavyZombie313BHooks& h)
{
 if(!Authority(z)||z->mDead||!Hooks(h))return false;
 const auto k=Kind(z);
 if(Gold(k))return h.goldEvent(z,s,e);
 switch(e)
 {
 case Event::Destroy:h.sourceDie(z,2);return true;
 case Event::FallSound:h.fallSound(z);return true;
 case Event::Bite:h.bite(z);return true;
 case Event::Shoot:return k==216?ShootNativeDrownCatapult343(z,s,h):FusionCatapult343Kind(k)&&Shot(z,s,h);
 case Event::ReplaceBomb:if(k!=216)return false;s.catapultFlags343|=2;return true; // Exact AnimRelodeOver.
 case Event::SetLadder:return k==33&&SetLadder(z,s,h);
 case Event::LandSound:if(k==32||FusionSuperPogo363Kind(k)){auto p=Pogo(s);if(!FusionPogo313BLandSound(p))return false;Pogo(s,p);h.sound(z,109);return true;}return false;
 case Event::JumpFail:
  if(k==32||FusionSuperPogo363Kind(k))
  {Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p)){FusionHeavy313BPlant t;float x=0,y=0;if(!h.plantTraits(p,t)||!h.sourcePoint(z,"Shadow",x,y))return false;if(FusionPogo313BJumpBlocked(Pogo(s),z->IsDeadOrDying(),z->mRow,x,t)&&GroundBox(z,p,h,false)){if(!h.setAxis(z,t.shadowX+.5f,y))return false;h.sound(z,64);return h.sourceEffect(z,23,1.75f);}}return true;}return false;
 case Event::BigJump:if(!FusionSuperPogo363Kind(k))return false;if(!z->IsDeadOrDying())s.status=20;s.shadowEnabled=false;return true;
 case Event::ChangeStatus:if(k!=28)return false;s.status=0;return true;
 case Event::Catch:return k==45&&Catch(z,s,h);
 default:return false;
 }
}
bool RootDeltaNativeHeavyZombie313B(Zombie* z,const NativeHeavyZombie313BState& s,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out)
{
 const auto* graph=FindFusionSourceGraph313(Kind(z));if(!graph||state>=graph->stateCount)return false;
 const char* name=graph->states[state].name;bool moving=false;
 const auto k=Kind(z);if(FusionCatapult343Kind(k))moving=(!std::strcmp(name,"walk")||!std::strcmp(name,"walk2")||(k==112&&!std::strcmp(name,"walk1")))&&!s.caltropDying;
 else if(k==33)moving=!std::strcmp(name,"walk")||!std::strcmp(name,"walk2");
 else if(k==32||FusionSuperPogo363Kind(k))moving=s.status==0&&!std::strcmp(name,"walk2");
 else if(k==28)moving=!std::strcmp(name,"mine")||!std::strcmp(name,"walk1")||!std::strcmp(name,"landing");
 else if(Gold(k))moving=!std::strcmp(name,"walk")||!std::strcmp(name,"walk2");
 if(!moving||z->mIsEating||z->IsDeadOrDying()){out=0;return true;}
 if(k==112||k==216){double delta=0;if(!SourceRootDelta343(k,state,a,b,delta))return false;out=z->mMindControlled!=s.lookBack?-delta:delta;return true;}
 if(k==209){double d=0;if(!SourceRootDeltaLate363(k,state,a,b,d))return false;out=z->mMindControlled!=s.lookBack?-d:d;return true;}
 return TryGetFusionHeavy313BRootDelta(k,name,{a.loop,a.seconds},{b.loop,b.seconds},z->mMindControlled!=s.lookBack,out);
}
bool NativeHeavyZombie313BContact(Zombie* z,NativeHeavyZombie313BState& s,Plant* p,const NativeHeavyZombie313BHooks& h)
{
 if(!NativeHeavyZombie313BKind(Kind(z)))return false;if(!Playing(z)||!p||!Hooks(h))return true;
 const auto k=Kind(z);
 if(FusionCatapult343Kind(k)||Gold(k)){h.crash(z);return true;}
 FusionHeavy313BPlant t;if(!h.plantTraits(p,t))return true;
 if(k==33&&FusionLadder313BCanSet(s.status,z->mMindControlled,z->IsDeadOrDying(),z->mRow,t))
 {s.targetPlantUID=z->mBoard->mPlants.DataArrayGetID(p);Trigger(z,s,h,Param::Ladder);return true;}
 if(k==28&&s.status==12&&t.row==z->mRow)
 {std::vector<uint16_t> contents;if((t.kind==911||t.kind==922)&&!h.magnetContents(p,contents)){s.failed=true;return true;}if(FusionMiner313BPlantSurprises(t.kind,contents))NativeMiner313BAxeLost(z,s,false,h);return true;}
 if(s.status==0)h.contact(z,true);return true;
}
bool NativeHeavyZombie313BLoseLadder(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{if(Kind(z)!=33)return false;if(!Authority(z))return true;s.status=0;s.ladderVisible=false;z->mShieldType=SHIELDTYPE_NONE;Trigger(z,s,h,Param::LoseLadder);return true;}
bool NativeHeavyZombie313BLoseJumper(Zombie* z,NativeHeavyZombie313BState& s,int reason,const NativeHeavyZombie313BHooks& h)
{
 const auto k=Kind(z);if(k!=32&&!FusionSuperPogo363Kind(k))return false;if(!Authority(z))return true;
 if(FusionSuperPogo363Kind(k)&&s.status==20){float x=0,y=0,land=0;if(!h.sourcePoint(z,"axis",x,y)||!h.landY(z->mBoard,x,z->mRow,land)||!h.setAxis(z,x,land)){s.failed=true;return true;}}
 auto p=Pogo(s);FusionPogo313BLoseJumper(p,k,z->IsDeadOrDying(),reason);Pogo(s,p);z->mVelX=z->mVelZ=0;z->mHasObject=false;Trigger(z,s,h,Param::Walk);return true;
}
bool NativeMiner313BAxeLost(Zombie* z,NativeHeavyZombie313BState& s,bool natural,const NativeHeavyZombie313BHooks& h)
{if(Kind(z)!=28)return false;if(!Authority(z))return true;s.status=13;if(natural){s.axePresent=false;s.lookBack=!s.lookBack;z->mHasObject=false;Trigger(z,s,h,Param::Rise);h.sound(z,94);}else Trigger(z,s,h,Param::Surprise);return true;}
bool BeginNativeHeavyZombie313BDeath(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{if(!NativeHeavyZombie313BKind(Kind(z)))return false;if(!Authority(z))return true;if(Gold(Kind(z))){s.status=0;s.gold.status=1;}else s.status=1;Trigger(z,s,h,Param::Die);return true;}
bool KillNativeCatapult313BByCaltrop(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{if(!FusionCatapult343Kind(Kind(z)))return false;if(!Authority(z))return true;s.caltropDying=true;if(auto* d=GetNativeSourceZombieData(z))d->beforeDying=true;z->mPhaseCounter=200;Trigger(z,s,h,Param::Shake);Trigger(z,s,h,Param::Die);return true;}
bool ApplyNativeCatapult313BBodyDamage(Zombie* z,int damage,const NativeHeavyZombie313BHooks& h)
{if(!FusionCatapult343Kind(Kind(z)))return false;if(!Playing(z)||damage<0||!Hooks(h))return true;auto* d=GetNativeSourceZombieData(z);if(!d||!std::isfinite(d->health-damage))return true;d->health-=damage;z->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);
 if(FusionCatapult313BBroken(d->health,z->mBodyMaxHealth))if(auto* c=GetNativeContext(z->mBoard)){const auto uid=z->mBoard->mZombies.DataArrayGetID(z);const auto it=c->heavyZombies313B.find(uid);if(it!=c->heavyZombies313B.end())it->second.armorStage=1;}
 if(d->health<=0)h.sourceDie(z,2);return true;}
bool ConfigureNativeBungee313BDelivery(Zombie* z,NativeHeavyZombie313BState& s,uint16_t child)
{(void)z;(void)s;(void)child;return false;} // No delivery/preview renderer admission until its exact source closure is implemented.
bool MoveNativeHeavyZombie313B(Zombie* z){return NativeHeavyZombie313BKind(Kind(z));}
}
