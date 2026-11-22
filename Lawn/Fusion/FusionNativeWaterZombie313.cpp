#include "FusionNativeWaterZombie313.h"
#include "FusionNative.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeWater268.h"
#include "FusionNativeSourceClock313.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
using Phase=FusionWaterZombie313Phase;
bool Host(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return z&&d&&FusionWaterZombie313Kind(d->kind)&&z->mBoard&&z->mApp&&
 z->mApp->mBoard==z->mBoard&&!z->mNetGhost&&!z->mApp->NetIsClient()&&!z->mDead&&IsNativeFusionBoard(z->mBoard);}
bool Ready(const NativeWaterZombie313Hooks& h)
{return h.stateFor&&h.prepare&&h.contact&&h.drownContact&&h.bite&&h.dolphinContact&&h.placeShadow&&h.facingKeepShadow&&h.splash&&
 h.particle&&h.sound&&h.fallSound&&h.retire&&h.driverContacts&&h.driverTerrain&&h.throwWeapon&&h.randomInt;}
Reanimation* Body(const Zombie* z){return z->mApp->ReanimationTryToGet(z->mBodyReanimID);}
bool Matches(Reanimation* b,const FusionWaterZombie313Clip& c)
{if(!b||b->mDead||!b->TrackExists(c.nativeTrack))return false;int start=0,count=0;b->GetFramesForLayer(c.nativeTrack,start,count);
 return count>0&&b->mFrameStart==start&&b->mFrameCount==count;}
bool Read(Zombie* z,Phase phase,FusionZombieClipPosition& out)
{
 const auto* d=GetNativeSourceZombieData(z);FusionWaterZombie313Clip c;auto* b=Body(z);
 if(!d||!TryGetFusionWaterZombie313Clip(d->kind,phase,c)||!Matches(b,c)||b->mLoopCount<0||
    !std::isfinite(b->mAnimTime)||b->mAnimTime<0||b->mAnimTime>1)return false;
 if(c.loop)out={uint32_t(b->mLoopCount),b->mAnimTime*c.seconds};
 else out={0,float((double(b->mLoopCount)+b->mAnimTime)*c.seconds)};
 return std::isfinite(out.seconds);
}
bool Write(Zombie* z,Phase phase,FusionZombieClipPosition at)
{
 FusionWaterZombie313Clip c;auto* b=Body(z);const auto* d=GetNativeSourceZombieData(z);
 if(!d||!TryGetFusionWaterZombie313Clip(d->kind,phase,c)||!Matches(b,c)||!std::isfinite(at.seconds)||at.seconds<0)return false;
 const double full=c.loop?double(at.loop)+at.seconds/c.seconds:double(at.seconds)/c.seconds;
 const double loops=std::floor(full);if(loops>(std::numeric_limits<int>::max)())return false;
 b->mLoopCount=int(loops);b->mAnimTime=b->mLastFrameTime=float(full-loops);return true;
}
bool Factor(Zombie* z,float& out)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* g=GetNativeZombieGarlicData(z);
 if(!d)return false;
 if(d->kind==16)return TryGetFusionZombieStatusSpeedFactor(0,false,false,z->mButteredCounter>0,false,out);
 if(!TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,
    g&&FusionGarlicStopsWalking(*g)&&!NativeSourceDeathIgnoresGarlic313(z),out))return false;
 out*=GetNativeWater268GrapSpeedFactor(z);return std::isfinite(out);
}
struct Run {LawnApp* app;Board* board;uint32_t uid;uint16_t kind;Zombie* z;NativeWaterZombie313State* s;const NativeWaterZombie313Hooks& h;};
Run Capture(Zombie* z,NativeWaterZombie313State& s,const NativeWaterZombie313Hooks& h)
{return {z->mApp,z->mBoard,z->mBoard->mZombies.DataArrayGetID(z),GetNativeSourceZombieData(z)->kind,z,&s,h};}
bool Refresh(Run& r)
{
 if(r.app->mBoard!=r.board)return false;r.z=r.board->mZombies.DataArrayTryToGet(r.uid);
 const auto* d=GetNativeSourceZombieData(r.z);
 if(!r.z||r.z->mDead||!d||d->kind!=r.kind||d->instanceToken!=r.uid)return false;
 r.s=r.h.stateFor(r.z);return r.s&&r.s->initialized&&r.s->localUID==r.uid;
}
void ClearBlend(NativeWaterZombie313State& s)
{s.outgoing=s.phase;s.outgoingSourceSeconds=s.transitionElapsed=s.transitionTotal=0;s.previous={};}
bool Enter(Run& r,Phase phase,float blend)
{
 FusionWaterZombie313Clip c;if(!TryGetFusionWaterZombie313Clip(r.kind,phase,c)||!Body(r.z)||!Body(r.z)->TrackExists(c.nativeTrack))return false;
 const auto old=r.s->phase;const auto cursor=r.s->incoming;
 r.s->phase=phase;r.s->incoming={};
 if(blend>0){r.s->outgoing=old;r.s->previous=cursor;r.s->outgoingSourceSeconds=cursor.seconds;r.s->transitionElapsed=0;r.s->transitionTotal=blend;}
 else ClearBlend(*r.s);
 // All are CLOCK-ONLY raw carrier loops. Nonloop source poses clamp only in
 // the source sampler; actual native time may exceed1 for exit1.0034704.
 r.z->PlayZombieReanim(c.nativeTrack,REANIM_LOOP,0,0);
 if(r.kind==17&&phase==Phase::SnorkelSwim)r.s->sourceStatus=7;
 if(r.kind==17&&phase==Phase::SnorkelEat)r.s->sourceStatus=0;
 return Write(r.z,phase,{})&&UpdateNativeWaterZombie313Animation(r.z,*r.s);
}
bool StartAuxiliary(Zombie* z,NativeWaterZombie313State& s,bool playing)
{
 auto* a=z->mApp->ReanimationTryToGet(static_cast<ReanimationID>(s.auxiliaryID));
 if(!a)
 {
  a=z->mApp->AddReanimation(0,0,0,REANIM_ZOMBIE);if(!a)return false;
  s.auxiliaryID=uint32_t(z->mApp->ReanimationGetID(a));
 }
 // This clock is unattached: the holder must advance it on the host. Hide
 // its carrier tracks without pretending an Attachment owns its update.
 a->mIsAttachment=false;
 a->PlayReanim("anim_walk",REANIM_LOOP,0,0);a->mAnimTime=a->mLastFrameTime=0;a->mLoopCount=0;
 for(int track=0;track<a->mDefinition->mTracks.count;++track)a->mTrackInstances[track].mForceDontRender=true;
 s.auxiliaryActive=playing;return true;
}
bool BeginShake(Zombie* z,NativeWaterZombie313State& s)
{
 if(s.auxiliaryActive)return true;
 if(!StartAuxiliary(z,s,true))return false;
 s.auxiliaryTransitionElapsed=0;s.auxiliaryTransitionTotal=.25f;
 return UpdateNativeWaterZombie313Animation(z,s);
}
double Span(FusionZombieClipPosition a,FusionZombieClipPosition b,const FusionWaterZombie313Clip& c)
{return double(b.loop-a.loop)*c.seconds+double(b.seconds)-a.seconds;}
float WaitEvent(uint16_t kind,Phase phase,FusionZombieClipPosition at,float rate)
{
 FusionZombieClipPosition event;uint16_t bits;FusionWaterZombie313Clip c;
 if(rate<=0||!TryGetFusionWaterZombie313NextEvent(kind,phase,at,event,bits)||!TryGetFusionWaterZombie313Clip(kind,phase,c))return (std::numeric_limits<float>::infinity)();
 return float((std::max)(0.0,Span(at,event,c))/rate);
}
bool Motion(Run& r,double delta)
{
 if(!delta)return true;
 const double scale=(r.kind==17||r.kind==20?1.25:1.35)*kFusionBasePixelsX*r.z->mScaleZombie;
 const double dx=delta*scale*(r.s->facingRight?-1:1);
 if(!std::isfinite(r.z->mPosX+dx))return false;
 r.z->mPosX+=float(dx);r.z->mX=int(r.z->mPosX);return true;
}
bool Events(Run& r,Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b)
{
 uint16_t bits=0;if(!TryGetFusionWaterZombie313Events(r.kind,phase,a,b,bits))return false;
 if(bits&Water313Bite){r.h.bite(r.z);if(!Refresh(r))return true;}
 if((bits&Water313JumpFail)&&r.kind==19&&r.s->jumpFailed&&r.s->sourceStatus!=1)
 {
  r.h.sound(r.z,64,.5f);
  if(!r.h.placeShadow(r.z,r.s->failShadowX,r.s->failShadowY)||!Enter(r,Phase::DolphinSwim,0))return false;
  r.s->sourceStatus=0;r.h.particle(r.z,23,1.75f);if(!Refresh(r))return true;
 }
 if(bits&Water313Splash)
 {
  NativeWaterZombie313Splash e;std::string error;
  if(!r.h.prepare(r.z,NativeWaterZombie313Operation::Splash,&error)||!TryMakeNativeWaterZombie313Splash(r.z,e)||!r.h.splash(r.board,e))return false;
  if(!Refresh(r))return true;r.h.particle(r.z,32,0);r.h.sound(r.z,75,.5f);if(!Refresh(r))return true;
 }
 if(bits&Water313JumpOver)
 {
  if(r.s->sourceStatus!=1)r.s->sourceStatus=0;
  if(r.kind==27||FusionWaterShooter343Kind(r.kind)){if(!r.h.facingKeepShadow(r.z,true))return false;r.s->facingRight=true;}
 }
 if(bits&Water313Throw)
 {if(!r.z->mMindControlled&&!GetNativeSourceZombieData(r.z)->beforeDying&&!r.h.throwWeapon(r.z))return false;if(!Refresh(r))return true;}
 if(bits&Water313Fall){r.h.fallSound(r.z);if(!Refresh(r))return true;}
 if(bits&Water313Destroy){r.h.retire(r.z,2);return true;}
 return true;
}
bool Step(Run& r,FusionZombieClipPosition observed,float speed,bool contact)
{
 FusionWaterZombie313Clip clip;if(!TryGetFusionWaterZombie313Clip(r.kind,r.s->phase,clip)||observed.loop<r.s->incoming.loop)return false;
 const double span=Span(r.s->incoming,observed,clip);if(!std::isfinite(span)||span<0)return false;
 float rate=float(span/.01),remaining=.01f;
 for(int split=0;remaining>0;++split)
 {
  if(split>=32)return false;
  const Phase phase=r.s->phase;TryGetFusionWaterZombie313Clip(r.kind,phase,clip);
  FusionWaterZombie313Transition tr;bool transition=false;float untilTransition=(std::numeric_limits<float>::infinity)();
  if(!r.s->transitionTotal&&TrySelectFusionWaterZombie313Transition(r.kind,*r.s,contact,r.s->jumpRequested,r.s->throwRequested,speed,tr))
  {
   transition=true;untilTransition=!tr.needsExit||r.s->incoming.seconds>=tr.exitSeconds?0:
      rate>0?(tr.exitSeconds-r.s->incoming.seconds)/rate:(std::numeric_limits<float>::infinity)();
  }
  float dt=(std::min)({remaining,WaitEvent(r.kind,phase,r.s->incoming,rate),untilTransition});
  float oldRate=0;FusionWaterZombie313Clip old;
  if(r.s->transitionTotal>0)
  {
   if(!TryGetFusionWaterZombie313Clip(r.kind,r.s->outgoing,old))return false;
   oldRate=old.multiplier*(old.speedParameter?speed:1);
   dt=(std::min)({dt,r.s->transitionTotal-r.s->transitionElapsed,WaitEvent(r.kind,r.s->outgoing,r.s->previous,oldRate)});
  }
  if(!std::isfinite(dt)||dt<0)return false;
  const auto ia=r.s->incoming,oa=r.s->previous;FusionZombieClipPosition ib,ob=oa;
  const float effective=clip.speedParameter?rate/clip.multiplier:1;
  if(!TryAdvanceFusionWaterZombie313Clock(r.kind,phase,ia,effective,dt,ib))return false;
  if(dt==WaitEvent(r.kind,phase,ia,rate)){uint16_t ignored;if(!TryGetFusionWaterZombie313NextEvent(r.kind,phase,ia,ib,ignored))return false;}
  const bool blending=r.s->transitionTotal>0;const auto outgoing=r.s->outgoing;
  if(blending)
  {
   if(!TryAdvanceFusionWaterZombie313Clock(r.kind,outgoing,oa,speed,dt,ob))return false;
   if(dt==WaitEvent(r.kind,outgoing,oa,oldRate)){uint16_t ignored;if(!TryGetFusionWaterZombie313NextEvent(r.kind,outgoing,oa,ob,ignored))return false;}
  }
  double delta=0,previousDelta=0;if(!TryGetFusionWaterZombie313RootDelta(r.kind,phase,ia,ib,delta))return false;
  if(blending)
  {
   if(!TryGetFusionWaterZombie313RootDelta(r.kind,outgoing,oa,ob,previousDelta))return false;
   const double weight=(r.s->transitionElapsed+dt*.5)/r.s->transitionTotal;
   delta=previousDelta*(1-weight)+delta*weight;
  }
  if(!Write(r.z,phase,ib))return false;r.s->incoming=ib;
  if(blending){r.s->previous=ob;r.s->outgoingSourceSeconds=ob.seconds;
   r.s->transitionElapsed=(std::min)(r.s->transitionTotal,r.s->transitionElapsed+dt);}
  if(!Motion(r,delta))return false;
  if(blending&&!Events(r,outgoing,oa,ob))return false;if(!Refresh(r))return true;
  if(r.s->phase==phase&&!Events(r,phase,ia,ib))return false;if(!Refresh(r))return true;
  remaining=(std::max)(0.f,remaining-dt);
  if(r.s->phase!=phase){TryGetFusionWaterZombie313Clip(r.kind,r.s->phase,clip);rate=clip.multiplier*(clip.speedParameter?speed:1);continue;}
  if(transition&&dt==untilTransition)
  {
   if(!Enter(r,tr.to,tr.duration))return false;
   if(tr.to==Phase::DolphinJump)r.s->jumpRequested=false;
   if(tr.to==Phase::DrownThrow)r.s->throwRequested=false;
   TryGetFusionWaterZombie313Clip(r.kind,r.s->phase,clip);rate=clip.multiplier*(clip.speedParameter?speed:1);continue;
  }
  if(blending&&r.s->transitionElapsed>=r.s->transitionTotal)ClearBlend(*r.s);
  if(dt==0&&ib.loop==ia.loop&&ib.seconds==ia.seconds&&ob.loop==oa.loop&&ob.seconds==oa.seconds)return false;
 }
 return true;
}
}
bool NativeWaterZombie313WorldValid(const Board* b,uint16_t kind,int row,std::string* error,bool controlled)
{
 if(error)error->clear();const auto reject=[&](const char* why){if(error)*error=why;return false;};
 if(!IsNativeSourceWorld313(b))
  return reject("Water313 requires the actual current ordinary source world and supported grid callbacks.");
 const int rows=GetNativeFusionRowCount(b);if(rows!=5&&rows!=6)return false;uint8_t water[6]{};
 for(int r=0;r<rows;++r){if(b->mPlantRow[r]!=PLANTROW_NORMAL&&b->mPlantRow[r]!=PLANTROW_POOL)return false;water[r]=b->mPlantRow[r]==PLANTROW_POOL;}
 const bool pool=b->mBackground==BACKGROUND_3_POOL||b->mBackground==BACKGROUND_4_FOG;
 if(!pool&&b->mBackground!=BACKGROUND_1_DAY&&b->mBackground!=BACKGROUND_2_NIGHT)
  return reject("Water313 does not admit Roof or unsupported source terrain.");
 return FusionWaterZombie313WorldValid(kind,rows,pool,std::span<const uint8_t>(water,rows),row,controlled)||reject("Water313 actual terrain/row capability does not admit this source kind.");
}
bool InitializeNativeWaterZombie313(Zombie* z,NativeWaterZombie313State& s,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||!Ready(h)||!NativeWaterZombie313WorldValid(z->mBoard,GetNativeSourceZombieData(z)->kind,z->mRow,nullptr,z->mMindControlled))return false;
 const auto uid=z->mBoard->mZombies.DataArrayGetID(z);if(s.initialized)return s.localUID==uid&&!s.failed;
 std::string error;if(!uid||!h.prepare(z,NativeWaterZombie313Operation::Initialize,&error))return false;
 FusionWaterZombie313View v;const auto kind=GetNativeSourceZombieData(z)->kind;if(!InitializeFusionWaterZombie313View(kind,v))return false;
 s=NativeWaterZombie313State{};static_cast<FusionWaterZombie313View&>(s)=v;s.localUID=uid;s.initialized=true;s.facingRight=z->mMindControlled;
 if(kind!=20&&!StartAuxiliary(z,s,s.auxiliaryActive)){s.failed=true;return false;}
 if(kind==20){const int draw=h.randomInt(z->mBoard,5,10);if(draw<5||draw>=10){s.failed=true;return false;}s.throwCountdown=float(draw);}
 auto r=Capture(z,s,h);if(!Enter(r,s.phase,0)){s.failed=true;return false;}
 z->mInPool=z->mBoard->mPlantRow[z->mRow]==PLANTROW_POOL;z->mZombieHeight=HEIGHT_ZOMBIE_NORMAL;
 if(kind==16)h.sound(z,76,1);if(FusionDolphin343Kind(kind))h.sound(z,78,1);
 return true;
}
bool UpdateNativeWaterZombie313Animation(Zombie* z,NativeWaterZombie313State& s)
{
 if(!Host(z))return false;const auto* d=GetNativeSourceZombieData(z);FusionWaterZombie313Clip clip;float factor=0;
 auto* body=Body(z);if(!Factor(z,factor)||!TryGetFusionWaterZombie313Clip(d->kind,s.phase,clip)||!Matches(body,clip))return false;
 const float speed=d->originSpeed*factor;
 body->mAnimRate=body->mFrameCount*clip.multiplier*(clip.speedParameter?speed:1)/clip.seconds;z->mOriginalAnimRate=body->mAnimRate;
 if(d->kind!=20)
 {
  auto* a=z->mApp->ReanimationTryToGet(static_cast<ReanimationID>(s.auxiliaryID));if(!a||a->mFrameCount<=0)return false;
  const float seconds=d->kind==16?.15f:d->kind==17?.26666668f:.35000002f;
  a->mAnimRate=s.auxiliaryActive?a->mFrameCount*(d->kind==16?speed:1)/seconds:0;
 }
 return true;
}
bool UpdateNativeWaterZombie313(Zombie* z,NativeWaterZombie313State& s,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||!Ready(h))return false;if(s.failed||!s.initialized||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
 if(s.tick==z->mBoard->mMainCounter)return true;s.tick=z->mBoard->mMainCounter;auto r=Capture(z,s,h);
 if(!FusionWaterZombie313ViewValid(r.kind,s)){s.failed=true;return true;}
 if(s.auxiliaryTransitionTotal)s.auxiliaryTransitionElapsed=(std::min)(s.auxiliaryTransitionTotal,s.auxiliaryTransitionElapsed+.01f);
 if(r.kind==16)
 {
  if(s.driverStopAge>=0&&z->mZombieAge>=s.driverStopAge){BeginNativeWaterZombie313Death(z,s,2,h);return true;}
  float next=0,velocity=0;if(!TryStepFusionDriver313Speed(s.driverSpeed,!s.colliderEnabled,s.facingRight,.01f,next,velocity)){s.failed=true;return true;}
  s.driverSpeed=next;
  const double dx=velocity*kFusionBasePixelsX*.01;if(!std::isfinite(z->mPosX+dx)){s.failed=true;return true;}
  z->mPosX+=float(dx);z->mX=int(z->mPosX);
  if(s.colliderEnabled){h.driverTerrain(z);if(!Refresh(r))return true;h.driverContacts(z);if(!Refresh(r))return true;}
 }
 float factor=0;if(!Factor(r.z,factor)){r.s->failed=true;return true;}
 bool contact=false;
 if(r.kind!=16&&r.s->sourceStatus!=1)
 {
  // Dolphin's source override allows base contact only atstatus0. Drown
  // initial contact requests its one throw without binding an eating target.
  if(r.kind==17||(FusionDolphin343Kind(r.kind)&&r.s->sourceStatus==0)||
     (r.kind==20&&r.s->phase!=Phase::DrownWalk))contact=h.contact(r.z,true);
  if(r.kind==20&&r.s->phase==Phase::DrownWalk&&!r.z->mMindControlled&&h.drownContact(r.z))r.s->throwRequested=true;
  if(FusionDolphin313CanProbe(r.kind,r.s->sourceStatus,r.z->mMindControlled,r.z->mIceTrapCounter>0))
  {
   bool blocked=false;float x=0,y=0;
   if(h.dolphinContact(r.z,blocked,x,y))
   {
    std::string error;if(!h.prepare(r.z,NativeWaterZombie313Operation::Jump,&error)){r.s->failed=true;return true;}
    r.s->jumpFailed=blocked;r.s->failShadowX=x;r.s->failShadowY=y;r.s->sourceStatus=9;r.s->jumpRequested=true;h.sound(r.z,79,.5f);
   }
  }
  if(r.kind==20)
  {
   float after=0;bool due=false;if(!TryStepFusionDrown313Countdown(r.s->throwCountdown,r.s->phase==Phase::DrownThrow||r.s->phase==Phase::DrownWalk2||
        r.s->phase==Phase::DrownEat,r.z->mMindControlled,GetNativeSourceZombieData(r.z)->beforeDying,.01f,after,due)){r.s->failed=true;return true;}
   r.s->throwCountdown=after;if(due)r.s->throwRequested=true;
  }
 }
 FusionZombieClipPosition observed;
 if(!Read(r.z,r.s->phase,observed)||!Step(r,observed,GetNativeSourceZombieData(r.z)->originSpeed*factor,contact))
 {if(Refresh(r))r.s->failed=true;return true;}
 if(Refresh(r)&&!UpdateNativeWaterZombie313Animation(r.z,*r.s))r.s->failed=true;return true;
}
bool MoveNativeWaterZombie313(Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d&&FusionWaterZombie313Kind(d->kind);}
bool BeginNativeWaterZombie313Death(Zombie* z,NativeWaterZombie313State& s,int reason,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||!Ready(h)||!s.initialized||s.failed)return false;if(s.deathCallbackHandled)return true;std::string error;
 if(!h.prepare(z,NativeWaterZombie313Operation::Death,&error))return false;
 const auto k=GetNativeSourceZombieData(z)->kind;
 const bool dolphin=(FusionDolphin343Kind(k))&&(s.phase==Phase::DolphinRide||s.phase==Phase::DolphinJump||
    (s.transitionTotal>0&&(s.outgoing==Phase::DolphinRide||s.outgoing==Phase::DolphinJump)));
 s.deathCallbackHandled=true;
 if(k==16)
 {auto r=Capture(z,s,h);if(!NativeDriver313DeathEvent(z,s,h)){s.deathCallbackHandled=false;return false;}if(Refresh(r))h.retire(r.z,reason==1?1:2);return true;}
 if((k==17&&s.sourceStatus==7)||dolphin){h.retire(z,2);return true;}
 if(reason!=0){h.retire(z,reason);return true;}
 auto r=Capture(z,s,h);FusionZombieClipPosition at;if(!Read(z,s.phase,at))return false;s.incoming=at;
 const Phase death=k==17?Phase::SnorkelDeath:k==20?Phase::DrownDeath:Phase::DolphinDeath;
 s.sourceStatus=1;return Enter(r,death,.25f);
}
bool NativeWaterZombie313LoseHead(Zombie* z,NativeWaterZombie313State& s,const NativeWaterZombie313Hooks& h)
{if(!Host(z)||GetNativeSourceZombieData(z)->kind!=17)return false;if(s.sourceStatus==7)h.retire(z,2);return true;}
bool ApplyNativeDriver313BodyDamage(Zombie* z,NativeWaterZombie313State& s,int damage,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||GetNativeSourceZombieData(z)->kind!=16)return false;std::string error;
 if(!h.prepare(z,NativeWaterZombie313Operation::DriverDamage,&error))return true;
 auto* d=GetNativeSourceZombieData(z);float after=0;bool retire=false;uint8_t stage=s.driverStage;
 if(!TryStepFusionDriver313Damage(d->health,z->mBodyMaxHealth,damage,after,stage,retire))return true;
 if(stage==2&&!BeginShake(z,s))return true;
 s.driverStage=stage;d->health=after;z->mBodyHealth=NativeSourceZombieLivenessHealth(after);
 if(retire)BeginNativeWaterZombie313Death(z,s,2,h);return true; // Source override has no generic limb calls.
}
bool NativeDriver313Caltrop(Zombie* z,NativeWaterZombie313State& s,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||GetNativeSourceZombieData(z)->kind!=16)return false;if(!s.colliderEnabled)return true;
 std::string error;if(!h.prepare(z,NativeWaterZombie313Operation::DriverStop,&error)||z->mZombieAge>(std::numeric_limits<int>::max)()-200)return true;
 if(!BeginShake(z,s))return true;auto r=Capture(z,s,h);
 if(!Enter(r,Phase::DriverDamaged,.25f)){s.failed=true;return true;}
 s.driverStage=2;s.colliderEnabled=false;s.driverStopAge=z->mZombieAge+200;z->mVelX=0;return true;
}
bool NativeDriver313DeathEvent(Zombie* z,NativeWaterZombie313State& s,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||GetNativeSourceZombieData(z)->kind!=16||!Ready(h))return false;
 if(s.driverDeathEventDone)return true;std::string error;
 if(!h.prepare(z,NativeWaterZombie313Operation::Death,&error))return false;
 s.driverDeathEventDone=true;auto r=Capture(z,s,h);h.sound(z,43,.5f);
 if(Refresh(r))h.particle(r.z,34,.6f);return true;
}
bool NativeWaterZombie313SetFacing(Zombie* z,NativeWaterZombie313State& s,bool right,const NativeWaterZombie313Hooks& h)
{
 if(!Host(z)||!s.initialized||s.failed||!Ready(h))return false;
 if(s.facingRight==right)return true;if(!h.facingKeepShadow(z,right))return false;s.facingRight=right;return true;
}
bool NativeWaterZombie313ColliderEnabled(const Zombie* z,const NativeWaterZombie313State& s)
{return z&&!z->mDead&&s.colliderEnabled;}
int NativeWaterZombie313Status(const NativeWaterZombie313State& s){return s.sourceStatus;}
bool CaptureNativeWaterZombie313Auxiliary(const Zombie* z,const NativeWaterZombie313State& s,LawnNet::NetZombieAnimState& out)
{
 LawnNet::NetZombieAnimState v;const auto* d=GetNativeSourceZombieData(z);if(!d)return false;
 if(d->kind==20){out=v;return true;}
 const auto* a=z->mApp->ReanimationTryToGet(static_cast<ReanimationID>(s.auxiliaryID));if(!a||a->mDead)return false;
 v.mPresent=true;v.mDead=a->mDead;v.mFrameStart=a->mFrameStart;v.mFrameCount=a->mFrameCount;v.mAnimTime=a->mAnimTime;v.mAnimRate=a->mAnimRate;
 v.mLoopType=uint8_t(a->mLoopType);v.mLoopCount=uint8_t((std::clamp)(a->mLoopCount,0,255));out=v;return true;
}
bool AdoptNativeWaterZombie313Auxiliary(Zombie* z,NativeWaterZombie313State& s,const LawnNet::NetZombieAnimState& v)
{
 const auto* d=GetNativeSourceZombieData(z);if(!d||!FusionWaterZombie313Kind(d->kind)||(!z->mNetGhost&&!z->mApp->NetIsClient()))return false;
 if(d->kind==20)return !v.mPresent;
 FusionWaterZombie313View candidate=s;candidate.auxiliary=v;
 if(!v.mPresent||!FusionWaterZombie313ViewValid(d->kind,candidate))return false;
 ReanimatorEnsureDefinitionLoaded(REANIM_ZOMBIE,false);
 if(!gReanimatorDefArray||unsigned(REANIM_ZOMBIE)>=gReanimatorDefCount)return false;
 const auto& tracks=gReanimatorDefArray[int(REANIM_ZOMBIE)].mTracks;
 if(!tracks.tracks||tracks.count<=0)return false;uint32_t frames=0;
 for(int i=0;i<tracks.count;++i)frames=(std::max)(frames,uint32_t(tracks.tracks[i].mTransforms.count));
 if(!LawnNet::NetZombieAnimStateFitsFrames(v,frames)||!StartAuxiliary(z,s,s.auxiliaryActive))return false;
 auto* a=z->mApp->ReanimationTryToGet(static_cast<ReanimationID>(s.auxiliaryID));
 if(!a)return false;
 a->mFrameStart=v.mFrameStart;a->mFrameCount=v.mFrameCount;a->mAnimTime=a->mLastFrameTime=v.mAnimTime;a->mAnimRate=v.mAnimRate;
 a->mLoopType=ReanimLoopType(v.mLoopType);a->mLoopCount=v.mLoopCount;s.auxiliary=v;return true;
}
void RemoveNativeWaterZombie313Auxiliary(Zombie* z,NativeWaterZombie313State& s)
{if(z&&s.auxiliaryID)z->mApp->RemoveReanimation(static_cast<ReanimationID>(s.auxiliaryID));s.auxiliaryID=0;}
bool GetNativeWaterZombie313Presentation(const Zombie* z,const NativeWaterZombie313State& s,const char*& name,float& phase)
{
 const auto* d=GetNativeSourceZombieData(z);FusionWaterZombie313Clip c;FusionZombieClipPosition at;
 if(!d||!FusionWaterZombie313ViewValid(d->kind,s)||!TryGetFusionWaterZombie313Clip(d->kind,s.phase,c)||!Read(const_cast<Zombie*>(z),s.phase,at))return false;
 name=c.state;phase=c.loop?at.seconds/c.seconds:(std::min)(1.f,at.seconds/c.seconds);return true;
}
bool GetNativeWaterZombie313Blend(const Zombie* z,const NativeWaterZombie313State& s,const char*& name,float& seconds,float& weight)
{
 const char* incoming=nullptr;float phase=0;if(!GetNativeWaterZombie313Presentation(z,s,incoming,phase))return false;
 const char* n=nullptr;float time=0,w=1;
 if(s.transitionTotal){FusionWaterZombie313Clip c;const auto* d=GetNativeSourceZombieData(z);
  if(!TryGetFusionWaterZombie313Clip(d->kind,s.outgoing,c))return false;n=c.state;time=s.outgoingSourceSeconds/c.multiplier;w=s.transitionElapsed/s.transitionTotal;}
 name=n;seconds=time;weight=w;return true;
}
bool GetNativeWaterZombie313Auxiliary(const Zombie* z,const NativeWaterZombie313State& s,const char*& name,float& seconds,float& weight,int& layer)
{
 const auto* d=GetNativeSourceZombieData(z);if(!d||d->kind==20)return false;
 LawnNet::NetZombieAnimState clock;if(z->mNetGhost||z->mApp->NetIsClient())clock=s.auxiliary;
 else if(!CaptureNativeWaterZombie313Auxiliary(z,s,clock))return false;
 if(!clock.mPresent)return false;const float length=d->kind==16?.15f:d->kind==17?.26666668f:.35000002f;
 name=d->kind==16?"shake":"water";seconds=clock.mAnimTime*length;
 weight=s.auxiliaryActive?(s.auxiliaryTransitionTotal?s.auxiliaryTransitionElapsed/s.auxiliaryTransitionTotal:1):0;layer=1;return true;
}
bool TryMakeNativeWaterZombie313Splash(const Zombie* z,NativeWaterZombie313Splash& out)
{
 const auto* d=GetNativeSourceZombieData(z);float x=0,y=0;
 if(!d||(!FusionDolphin343Kind(d->kind))||!z->mNetId||!d->instanceToken||!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
 out={z->mNetId,d->instanceToken,z->mRow,x,y};return true;
}
}
