#include "FusionNativeNightZombie281.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
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
using Phase=FusionNightZombie281Phase;
bool Host(const Zombie* z,uint16_t kind)
{
 const auto* d=GetNativeSourceZombieData(z);
 return z&&d&&d->kind==kind&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&
  !z->mNetGhost&&!z->mApp->NetIsClient()&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);
}
Reanimation* Body(const Zombie* z)
{return z&&z->mApp?z->mApp->ReanimationTryToGet(z->mBodyReanimID):nullptr;}
bool Factor(Zombie* z,float& value)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* g=GetNativeZombieGarlicData(z);
 if(!d||!TryGetFusionZombieStatusSpeedFactor(d->kind,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,
   g&&FusionGarlicStopsWalking(*g),value))return false;
 value*=GetNativeWater268GrapSpeedFactor(z);return true;
}
bool Authority(const Zombie* z)
{
 const auto* d=GetNativeSourceZombieData(z);
 return z&&d&&d->kind==10&&z->mBoard&&z->mApp&&z->mApp->mBoard==z->mBoard&&
  !z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceAdventureGrassWorld281(z->mBoard);
}
bool Read(const Reanimation* r,const FusionNightZombie281Clip& clip,FusionZombieClipPosition& out)
{
 if(!r||r->mDead||r->mLoopCount<0||!std::isfinite(r->mAnimTime)||r->mAnimTime<0||r->mAnimTime>1)return false;
 FusionZombieClipPosition value{uint32_t(r->mLoopCount),r->mAnimTime*clip.seconds};
 if(!clip.loop)
 {if(value.loop==1&&r->mAnimTime==1)value.loop=0;if(value.loop)return false;}
 out=value;return true;
}
bool BodyMatches(Reanimation* r,const FusionNightZombie281Clip& clip)
{
 if(!r||!r->TrackExists(clip.nativeTrack))return false;int first=0,count=0;r->GetFramesForLayer(clip.nativeTrack,first,count);
 return count>0&&r->mFrameStart==first&&r->mFrameCount==count;
}
bool WriteClock(Zombie* z,Phase phase,FusionZombieClipPosition at)
{
 auto* body=Body(z);FusionNightZombie281Clip clip;
 if(!TryGetFusionNightZombie281Clip(phase,clip)||!BodyMatches(body,clip)||
    at.loop>uint32_t((std::numeric_limits<int>::max)())||!std::isfinite(at.seconds)||at.seconds<0||at.seconds>clip.seconds)return false;
 float normalizedTime=at.seconds/clip.seconds;
 // Keep an exact event cursor representable by the real F32 native clock.
 // A divide/multiply round trip must not appear to run backward on freeze.
 if(normalizedTime*clip.seconds<at.seconds)normalizedTime=std::nextafter(normalizedTime,1.f);
 body->mAnimTime=normalizedTime;body->mLoopCount=int(at.loop);body->mLastFrameTime=body->mAnimTime;return true;
}
void ClearTransition(NativeNightZombie281State& s)
{s.outgoing=Phase::Moonwalk;s.outgoingSourceSeconds=s.transitionElapsed=s.transitionTotal=0;s.outgoingPosition={};}
bool HookReady(const NativeNightZombie281Hooks& h)
{return h.eveBalanced&&h.travelDebuff13&&h.prepareChildren&&h.childEffect&&h.facingKeepShadow&&
 h.contact&&h.bite&&h.sound&&h.fallSound&&h.destroy&&h.outsideSourceBounds&&h.dieWithoutAnimation;}
bool Facing(Zombie* z,NativeNightZombie281State& s,bool back,const NativeNightZombie281Hooks& h)
{if(!h.facingKeepShadow(z,back)){s.failed=true;return false;}s.lookBack=back;return true;}
bool Enter(Zombie* z,NativeNightZombie281State& s,Phase phase)
{
 auto* body=Body(z);FusionNightZombie281Clip clip;
 if(!body||!TryGetFusionNightZombie281Clip(phase,clip)||!body->TrackExists(clip.nativeTrack))return false;
 s.phase=phase;s.entering=true;s.eventPosition={};z->mVelX=0;
 z->PlayZombieReanim(clip.nativeTrack,clip.loop?REANIM_LOOP:REANIM_PLAY_ONCE_AND_HOLD,0,0);
 return UpdateNativeJackson281Animation(z,s);
}
std::array<bool,4> Present(Zombie* z,const NativeNightZombie281State& s)
{
 std::array<bool,4> result{};
 // Source Unity-null means actual destruction, not dying/zero HP or wrong allegiance.
 for(size_t i=0;i<4;++i)result[i]=s.dancers[i]&&z->mBoard->mZombies.DataArrayTryToGet(s.dancers[i]);
 return result;
}
bool Coordinates(Zombie* z,std::array<uint8_t,6>& rows,int& count,float& sourceX)
{
 count=GetNativeFusionRowCount(z->mBoard);if(count<1||count>6)return false;
 float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
 sourceX=float((x-z->mBoard->GetCellCenterX(0,z->mRow))/kFusionBasePixelsX-4.8);
 for(int i=0;i<count;++i)rows[i]=z->mBoard->mPlantRow[i]==PLANTROW_POOL?1:0;
 return std::isfinite(sourceX);
}
bool Birth(Zombie* z,const FusionNightZombie281Children& plan,const NativeNightZombie281Hooks& h,
 bool dance,std::array<uint32_t,4>* refs,bool& failed,std::string* error)
{
 if(!plan.count)return true;std::array<uint16_t,4> kinds{};std::array<float,4> roots{};
 if(plan.count>plan.actors.size())return false;
 for(size_t i=0;i<plan.count;++i)
 {
  const auto& p=plan.actors[i];if(p.row<0||p.row>=GetNativeFusionRowCount(z->mBoard))return false;
  kinds[i]=p.kind;roots[i]=float(z->mBoard->GetCellCenterX(0,p.row)+(p.sourceX+4.8)*kFusionBasePixelsX);
  if(!std::isfinite(roots[i]))return false;
 }
 if(!h.prepareChildren(z,std::span<const uint16_t>(kinds.data(),plan.count),error))return false;
 for(size_t i=0;i<plan.count;++i)
 {
  const auto& p=plan.actors[i];NativeSourceZombieSpawnRequest request;
  request.kind=p.kind;request.row=p.row;request.fromWave=z->mFromWave;
  request.friendly=p.controlled;
  const float wantedRoot=roots[i];
  auto* child=SpawnNativeSourceZombie(z->mBoard,request);
  const auto* data=GetNativeSourceZombieData(child);float rootX=0,rootY=0;
  if(!child||!data||data->kind!=p.kind||!GetNativeSourceZombie269NodePoint(child,"",rootX,rootY)||
      !std::isfinite(wantedRoot)||!std::isfinite(child->mPosX+wantedRoot-rootX))
  {failed=true;if(error)*error="Night284 prepared typed child/root initialization failed; partial batch cannot retry.";return false;}
  // SetZombie positions the actual child ROOT at the requested source X.
  // Birth-only projection, before any child update or cosmetic publication.
  child->mPosX+=wantedRoot-rootX;child->mX=int(child->mPosX);
  if(!h.childEffect(z,child,dance))
  {failed=true;if(error)*error="Night281 prepared child birth/effect failed; partial batch cannot retry.";return false;}
  if(refs)(*refs)[p.slot]=z->mBoard->mZombies.DataArrayGetID(child);
 }
 return true;
}
bool Summon(Zombie* z,NativeNightZombie281State& s,const NativeNightZombie281Hooks& h)
{
 std::array<uint8_t,6> rows{};int count=0;float x=0;
 if(!Coordinates(z,rows,count,x))return false;
 // Source early balanced boundary precedes sound and status guards.
 if(h.eveBalanced(z->mBoard)&&x<1.5f){s.summonPending=false;return true;}
 if(!s.summonSoundPlayed){h.sound(z,69);s.summonSoundPlayed=true;}
 FusionNightZombie281Children plan;
 if(!TryPlanFusionJackson281(z->mRow,std::span<const uint8_t>(rows.data(),count),x,h.eveBalanced(z->mBoard),
   z->IsDeadOrDying()?1:0,!z->mHasHead,z->mMindControlled,Present(z,s),plan))return false;
 if(z->IsDeadOrDying()||!z->mHasHead){s.summonPending=false;return true;}
 std::string error;
 if(!Birth(z,plan,h,false,&s.dancers,s.failed,&error))return false;
 s.loseDancer=false;s.summonPending=false;return true;
}
bool Events(Zombie* z,NativeNightZombie281State& s,Phase phase,FusionZombieClipPosition a,
 FusionZombieClipPosition b,bool entering,const NativeNightZombie281Hooks& h,bool& destroyed)
{
 uint16_t events=0;if(!TryGetFusionJackson281Events(phase,a,b,entering,events))return false;
 // The stepper splits at EVERY callback boundary; two different facing events
 // cannot be collapsed into one mask. Loop-time0 Forward is not entry-only.
 if(events&Night281Forward)if(!Facing(z,s,false,h))return false;
 if(events&Night281Back)if(!Facing(z,s,true,h))return false;
 if(events&Night281Summon){s.summonPending=true;s.summonSoundPlayed=false;}
 if(events&Night281PointOver)s.canAttack=true;
 if(events&Night281Bite1)h.bite(z);if(events&Night281Bite2)h.bite(z);
 if(events&Night281Fall)h.fallSound(z);
 if(events&Night281Destroy){destroyed=true;h.destroy(z);}
 return true;
}
double Distance(FusionZombieClipPosition a,FusionZombieClipPosition b,const FusionNightZombie281Clip& c)
{return double(b.loop-a.loop)*c.seconds+double(b.seconds)-a.seconds;}
bool Advance(Phase phase,FusionZombieClipPosition a,float rate,float dt,FusionZombieClipPosition& b)
{
 FusionNightZombie281Clip clip;if(!TryGetFusionNightZombie281Clip(phase,clip))return false;
 return TryAdvanceFusionJackson281Clock(phase,a,rate/clip.multiplier,dt,b);
}
float EventWait(Phase phase,FusionZombieClipPosition at,float rate)
{
 if(rate<=0)return (std::numeric_limits<float>::infinity)();
 FusionNightZombie281Clip clip;FusionZombieClipPosition next;
 if(!TryGetFusionNightZombie281Clip(phase,clip)||!TryGetFusionJackson281NextEvent(phase,at,next))return (std::numeric_limits<float>::infinity)();
 return float((std::max)(0.0,Distance(at,next,clip))/rate);
}
bool BeginTransition(Zombie* z,NativeNightZombie281State& s,const FusionNightZombie281Transition& transition,
 FusionZombieClipPosition at,float animatorSpeed,const NativeNightZombie281Hooks& h,bool& destroyed)
{
 float total=0;if(!TryGetFusionJackson281TransitionSeconds(s.phase,transition,animatorSpeed,total))return false;
 // Approved native interruption projection: A->B interrupted by AnyDeath is
 // B->Death. Preserve B's ACTUAL cursor; A is no longer sampled/dispatched.
 s.outgoing=s.phase;s.outgoingPosition=at;s.outgoingSourceSeconds=at.seconds;
 s.transitionElapsed=0;s.transitionTotal=total;
 if(!Enter(z,s,transition.target))return false;
 if(!Events(z,s,s.phase,{},{},true,h,destroyed))return false;s.entering=false;return true;
}
bool TransitionDue(const NativeNightZombie281State& s,FusionZombieClipPosition at,
 FusionZombieClipPosition end,float animatorSpeed,bool contact,FusionNightZombie281Transition& t,float& wait,
 FusionZombieClipPosition& target)
{
 if(s.transitionTotal>0)return false;
 if(s.transitionDeferred)
 {if(animatorSpeed<=0)return false;t=s.deferredTransition;target=at;wait=0;return true;}
 if(!TryGetFusionJackson281Transition(s.phase,at,end,animatorSpeed>0,
  contact&&s.canAttack,s.loseDancer,s.moonWalkFinished,t))return false;
 FusionNightZombie281Clip c;TryGetFusionNightZombie281Clip(s.phase,c);
 float edge=at.seconds;
 if(s.phase==Phase::Point)edge=c.seconds*.8235294f;
 else if(s.phase==Phase::Walk&&t.target==Phase::ArmRaise)edge=c.seconds*.925f;
 else if(s.phase==Phase::Walk&&t.target==Phase::Point)edge=c.seconds*.9f;
 else if(s.phase==Phase::ArmRaise&&t.target==Phase::Walk)edge=c.seconds;
 target={at.loop,edge};
 const double span=Distance(at,end,c);
 wait=span>0?float((std::max)(0.0,double(edge)-at.seconds)/span):0;
 return true; // wait is the fraction of the forecast span, converted by caller.
}
bool ApplyMotion(Zombie* z,const NativeNightZombie281State& s,FusionZombieClipPosition ia,FusionZombieClipPosition ib,
 FusionZombieClipPosition oa,FusionZombieClipPosition ob,float elapsedAfter)
{
 const bool flipped=z->mMindControlled!=s.lookBack;const double scale=.31*kFusionBasePixelsX*z->mScaleZombie;
 if(!std::isfinite(scale)||scale<0)return false;
 double dx=0;
 const bool okay=s.transitionTotal>0?
  TryGetFusionJackson281BlendRootDelta(s.phase,ia,ib,s.outgoing,oa,ob,s.transitionElapsed/s.transitionTotal,
    elapsedAfter/s.transitionTotal,flipped,scale,dx):
  TryGetFusionNightZombie281RootDelta(s.phase,ia,ib,flipped,scale,dx);
 if(!okay||!std::isfinite(z->mPosX+dx))return false;
 z->mPosX+=float(dx);z->mX=int(z->mPosX);return true;
}
bool Step(Zombie* z,NativeNightZombie281State& s,FusionZombieClipPosition observed,float animatorSpeed,
 const NativeNightZombie281Hooks& h)
{
 // Existing native fixed gameplay update is .01s, not a new wall clock. Read
 // the actual body span first. Callback-boundary sampling temporarily projects
 // that same span onto the real body; a phase change rebases only its remainder.
 constexpr float nativeStep=.01f;
 FusionNightZombie281Clip clip;if(!TryGetFusionNightZombie281Clip(s.phase,clip))return false;
 const double observedSpan=Distance(s.eventPosition,observed,clip);
 if(observed.loop<s.eventPosition.loop||!std::isfinite(observedSpan)||observedSpan<0)return false;
 float rate=float(observedSpan/nativeStep),remaining=nativeStep;bool rebased=false,destroyed=false;
 if(!clip.loop&&observed.seconds==clip.seconds&&s.eventPosition.seconds<clip.seconds)
  rate=(std::max)(rate,clip.multiplier*animatorSpeed);
 if(s.entering)
 {if(!Events(z,s,s.phase,s.eventPosition,s.eventPosition,true,h,destroyed))return false;s.entering=false;if(destroyed)return true;}
 // A native end->next-loop0 representation can have zero arithmetic distance.
 // Dispatch ONLY its observed loop increment; frozen end samples never invent it.
 if(observed.loop>s.eventPosition.loop&&observedSpan==0)
 {
  const auto before=s.eventPosition;s.eventPosition=observed;
  if(!WriteClock(z,s.phase,observed)||!Events(z,s,s.phase,before,observed,false,h,destroyed))return false;
  if(destroyed)return true;
 }
 for(int splits=0;remaining>0;++splits)
 {
  if(splits>=64)return false; // No input/clock can silently consume an unbounded event backlog.
  TryGetFusionNightZombie281Clip(s.phase,clip);
  if(!WriteClock(z,s.phase,s.eventPosition))return false;
  const auto segmentPhase=s.phase;
  FusionZombieClipPosition forecast;
  if(!Advance(s.phase,s.eventPosition,rate,remaining,forecast))return false;
  const bool contact=s.phase==Phase::Death?false:h.contact(z,s.canAttack);
  FusionNightZombie281Transition transition;float fraction=0;FusionZombieClipPosition transitionAt{};
  const bool due=TransitionDue(s,s.eventPosition,forecast,animatorSpeed,contact,transition,fraction,transitionAt);
  const float transitionWait=due?remaining*fraction:(std::numeric_limits<float>::infinity)();
  const float incomingEventWait=EventWait(s.phase,s.eventPosition,rate);
  float outgoingEventWait=(std::numeric_limits<float>::infinity)();
  float dt=(std::min)(remaining,incomingEventWait);
  FusionNightZombie281Clip old;float oldRate=0;
  if(s.transitionTotal>0)
  {
   if(!TryGetFusionNightZombie281Clip(s.outgoing,old))return false;
   oldRate=old.multiplier*animatorSpeed;
   outgoingEventWait=EventWait(s.outgoing,s.outgoingPosition,oldRate);
   dt=(std::min)({dt,s.transitionTotal-s.transitionElapsed,outgoingEventWait});
  }
  dt=(std::min)(dt,transitionWait);
  if(!std::isfinite(dt)||dt<0)return false;
  const auto ia=s.eventPosition,oa=s.outgoingPosition;auto ib=ia,ob=oa;
  if(!Advance(s.phase,ia,rate,dt,ib)||(s.transitionTotal>0&&!Advance(s.outgoing,oa,oldRate,dt,ob)))return false;
  // Divide/multiply rounding must not leave a callback one ULP below its key.
  if(dt==incomingEventWait&&!TryGetFusionJackson281NextEvent(s.phase,ia,ib))return false;
  if(s.transitionTotal>0&&dt==outgoingEventWait&&!TryGetFusionJackson281NextEvent(s.outgoing,oa,ob))return false;
  if(due&&dt==transitionWait)ib=transitionAt;
  const float nextElapsed=s.transitionTotal>0?(std::min)(s.transitionTotal,s.transitionElapsed+dt):0;
  if(!WriteClock(z,s.phase,ib)||!ApplyMotion(z,s,ia,ib,oa,ob,nextElapsed))return false;
  s.eventPosition=ib;
  if(s.transitionTotal>0)
  {
   const auto outgoing=s.outgoing;s.outgoingPosition=ob;s.outgoingSourceSeconds=ob.seconds;s.transitionElapsed=nextElapsed;
   if(!Events(z,s,outgoing,oa,ob,false,h,destroyed))return false;
   if(destroyed)return true;
  }
  if(s.phase==segmentPhase)
   if(!Events(z,s,s.phase,ia,ib,false,h,destroyed))return false;
  if(destroyed)return true;
  if(s.phase!=segmentPhase)
  {
   // A real bite/reflect hook may synchronously enter Death. Preserve its new
   // cursor and projection; never replay the interrupted incoming event span.
   remaining=(std::max)(0.f,remaining-dt);rebased=true;
   TryGetFusionNightZombie281Clip(s.phase,clip);rate=clip.multiplier*animatorSpeed;continue;
  }
  // Event-space operations see the genuine moved/facing-adjusted Shadow.
  if(s.summonPending&&!Summon(z,s,h)&&s.failed)return false;
  remaining=(std::max)(0.f,remaining-dt);
  if(s.transitionTotal>0&&s.transitionElapsed>=s.transitionTotal)ClearTransition(s);
  if(due&&dt==transitionWait)
  {
   if(!transition.fixedSeconds&&animatorSpeed<=0)
   {
    // A normalized duration has no finite entry at Speed0. Retain this exact
    // source exit cursor and enter once speed is positive; never divide by0.
    s.transitionDeferred=true;s.deferredTransition=transition;rate=0;rebased=true;
   }
   else
   {
    s.transitionDeferred=false;
    if(!BeginTransition(z,s,transition,s.eventPosition,animatorSpeed,h,destroyed))return false;
    if(destroyed)return true;rebased=true;TryGetFusionNightZombie281Clip(s.phase,clip);rate=clip.multiplier*animatorSpeed;
   }
  }
  else if(dt==0&&ib.loop==ia.loop&&ib.seconds==ia.seconds&&ob.loop==oa.loop&&ob.seconds==oa.seconds)return false;
 }
 if(rebased)
 {
  if(!WriteClock(z,s.phase,s.eventPosition))return false;
 }
 return true;
}
}
bool NativeNightZombie281StateValid(const NativeNightZombie281State& s)
{return FusionNightZombie281ViewValid(s);}
bool InitializeNativeJackson281(Zombie* z,NativeNightZombie281State& s,const NativeNightZombie281Hooks& h)
{
 // Authority/resource initialization is legal during native intro or pause.
 // No gameplay timer/event advancement happens until the normal host update.
 if(!Authority(z)||!HookReady(h))return false;
 s=NativeNightZombie281State{};s.initialized=true;s.wasControlled=z->mMindControlled;
 return Enter(z,s,Phase::Moonwalk)&&Facing(z,s,true,h);
}
void NativeJackson281MindControlled(NativeNightZombie281State& s)
{s.dancers={};s.wasControlled=true;}
bool UpdateNativeJackson281Animation(Zombie* z,const NativeNightZombie281State& s)
{
 if(!Authority(z))return false;auto* body=Body(z);const auto* d=GetNativeSourceZombieData(z);
 FusionNightZombie281Clip c;float factor=1;
 if(!body||!d||!TryGetFusionNightZombie281Clip(s.phase,c)||!Factor(z,factor))return false;
 // ALL seven states, including Death, use source Speed.
 z->mOriginalAnimRate=body->mFrameCount*c.multiplier*d->originSpeed*factor/c.seconds;
 body->mAnimRate=z->mOriginalAnimRate;return true;
}
bool BeginNativeJackson281Death(Zombie* z,NativeNightZombie281State& s,const NativeNightZombie281Hooks& h)
{
 const auto* data=GetNativeSourceZombieData(z);if(!data||data->kind!=10)return false;
 if(!Authority(z)||s.phase==Phase::Death)return true;
 if(!s.initialized||s.failed||!HookReady(h)){s.failed=true;return true;}
 FusionNightZombie281Clip c;FusionZombieClipPosition now;float factor=1;
 if(!TryGetFusionNightZombie281Clip(s.phase,c)||!BodyMatches(Body(z),c)||!Read(Body(z),c,now)||!Factor(z,factor))
 {s.failed=true;return true;}
 bool destroyed=false;s.summonPending=false;s.transitionDeferred=false;
 if(!BeginTransition(z,s,{Phase::Death,.25f,true},now,data->originSpeed*factor,h,destroyed))s.failed=true;
 return true;
}
bool UpdateNativeJackson281(Zombie* z,NativeNightZombie281State& s,const NativeNightZombie281Hooks& h)
{
 const auto* d=GetNativeSourceZombieData(z);if(!d||d->kind!=10)return false;
 if(!Host(z,10)||z->mDead)return true;
 if(!s.initialized||s.failed||!HookReady(h))return true;
 // MainCounter is ONLY a private once-per-native-update guard, never a wire clock.
 if(s.tick==z->mBoard->mMainCounter)return true;s.tick=z->mBoard->mMainCounter;
 if(z->mMindControlled&&!s.wasControlled)NativeJackson281MindControlled(s);
 if(h.outsideSourceBounds(z)){h.dieWithoutAnimation(z);return true;}
 if(z->IsDeadOrDying()&&s.phase!=Phase::Death)
 {BeginNativeJackson281Death(z,s,h);return true;}
 float factor=1;FusionNightZombie281Clip clip;FusionZombieClipPosition now;
 if(!Factor(z,factor)||!TryGetFusionNightZombie281Clip(s.phase,clip)||!BodyMatches(Body(z),clip)||!Read(Body(z),clip,now))
 {s.failed=true;return true;}
 // Derived source Update/FixedUpdate still maintain these flags during death.
 s.moonWalkElapsed+=.01f;
 if(s.moonWalkElapsed>3&&!s.moonWalkFinished)s.moonWalkFinished=true;
 if(FusionJackson281MissingDancer(z->mRow,GetNativeFusionRowCount(z->mBoard),Present(z,s)))s.loseDancer=true;
 if(s.phase!=Phase::Death)
 {
  const bool contact=h.contact(z,s.canAttack);
  if(contact&&!s.canAttack&&!s.moonWalkFinished&&!h.travelDebuff13(z->mBoard))s.moonWalkFinished=true;
 }
 if(!Step(z,s,now,d->originSpeed*factor,h))s.failed=true;
 if(!z->mDead&&!s.failed)UpdateNativeJackson281Animation(z,s);return true;
}
bool MoveNativeJackson281(Zombie* z,NativeNightZombie281State&)
{
 // Controller Update already applies each source root segment before callbacks.
 // Consume generic native movement on both host and presentation-only ghosts.
 const auto* d=GetNativeSourceZombieData(z);return d&&d->kind==10;
}
bool GetNativeJackson281Presentation(const Zombie* z,const NativeNightZombie281State& s,const char*& name,float& phase)
{
 const auto* d=GetNativeSourceZombieData(z);auto* body=Body(z);FusionNightZombie281Clip c;FusionZombieClipPosition at;
 if(!d||d->kind!=10||!NativeNightZombie281StateValid(s)||!TryGetFusionNightZombie281Clip(s.phase,c)||
    !BodyMatches(body,c)||!Read(body,c,at))return false;
 const auto* state=FusionJackson281ControllerState(s.phase);if(!state)return false;
 name=state;phase=body->mAnimTime;return true;
}
bool GetNativeJackson281BlendPresentation(const Zombie* z,const NativeNightZombie281State& s,
 const char*& outgoingState,float& outgoingElapsedSeconds,float& incomingWeight)
{
 const char* incoming=nullptr;float phase=0;
 if(!GetNativeJackson281Presentation(z,s,incoming,phase))return false;
 const char* state=nullptr;float elapsed=0,weight=1;
 if(s.transitionTotal>0)
 {
  FusionNightZombie281Clip c;if(!TryGetFusionNightZombie281Clip(s.outgoing,c))return false;
  state=FusionJackson281ControllerState(s.outgoing);elapsed=s.outgoingSourceSeconds/c.multiplier;
  weight=s.transitionElapsed/s.transitionTotal;
 }
 outgoingState=state;outgoingElapsedSeconds=elapsed;incomingWeight=weight;return true;
}
bool SpawnNativeDancePole284Children(Zombie* z,int row,float shadowX,const NativeNightZombie281Hooks& h,bool& failed,std::string* error)
{
 if(!Host(z,6)||!IsNativeSourceAdventureGrassWorld281(z->mBoard)||z->IsDeadOrDying()||!z->mHasHead||
   failed||!h.eveBalanced||!h.prepareChildren||!h.childEffect||!std::isfinite(shadowX))return false;
 const int count=GetNativeFusionRowCount(z->mBoard);if(count<1||count>6||row<0||row>=count)return false;
 std::array<uint8_t,6> rows{};
 for(int i=0;i<count;++i)rows[i]=z->mBoard->mPlantRow[i]==PLANTROW_POOL?1:0;
 const float sourceX=float((shadowX-z->mBoard->GetCellCenterX(0,row))/kFusionBasePixelsX-4.8);
 FusionNightZombie281Children plan;
 if(!TryPlanFusionDancePole281(row,std::span<const uint8_t>(rows.data(),count),sourceX,h.eveBalanced(z->mBoard),0,false,plan))return false;
 return Birth(z,plan,h,true,nullptr,failed,error);
}
bool SpawnNativeDancePole281Children(Zombie* z,const NativeNightZombie281Hooks& h,bool& failed,std::string* error)
{
 if(!z||!z->mBoard)return false;float x=0,y=0;
 if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
 return SpawnNativeDancePole284Children(z,z->mRow,x,h,failed,error);
}
}
