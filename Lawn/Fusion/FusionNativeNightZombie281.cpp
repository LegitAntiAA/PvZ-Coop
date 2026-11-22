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
bool Read(const Reanimation* r,float duration,FusionZombieClipPosition& out)
{
 if(!r||r->mDead||r->mLoopCount<0||!std::isfinite(r->mAnimTime)||r->mAnimTime<0||r->mAnimTime>1)return false;
 out={uint32_t(r->mLoopCount),r->mAnimTime*duration};return true;
}
bool HookReady(const NativeNightZombie281Hooks& h)
{return h.eveBalanced&&h.travelDebuff13&&h.prepareChildren&&h.childEffect&&h.facingKeepShadow&&
 h.contact&&h.bite&&h.sound&&h.fallSound&&h.destroy&&h.outsideSourceBounds&&h.dieWithoutAnimation;}
bool Facing(Zombie* z,NativeNightZombie281State& s,bool back,const NativeNightZombie281Hooks& h)
{if(!h.facingKeepShadow(z,back)){s.failed=true;return false;}s.lookBack=back;return true;}
bool Enter(Zombie* z,NativeNightZombie281State& s,Phase phase)
{
 auto* body=Body(z);FusionNightZombie281Clip clip;
 if(!body||!TryGetFusionNightZombie281Clip(phase,clip)||!body->TrackExists(clip.nativeTrack))return false;
 s.phase=phase;s.entering=true;s.eventPosition={};s.motionValid=false;z->mVelX=0;
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
 FusionZombieClipPosition b,bool entering,const NativeNightZombie281Hooks& h)
{
 uint16_t events=0;if(!TryGetFusionJackson281Events(phase,a,b,entering,events))return false;
 // Process each actual event in source chronological order, not bit order.
 if(entering&&(events&Night281Forward))if(!Facing(z,s,false,h))return false;
 if(phase==Phase::ArmRaise)
 {
  constexpr float times[]={.8333333f,1.6666666f,2.5f};
  for(int i=0;i<3;++i)if(a.seconds<times[i]&&b.seconds>=times[i])if(!Facing(z,s,i!=1,h))return false;
 }
 if(events&Night281Summon)
 {s.summonPending=true;s.summonSoundPlayed=false;}
 if(events&Night281PointOver)s.canAttack=true;
 if(events&Night281Bite1)h.bite(z);if(events&Night281Bite2)h.bite(z);
 if(events&Night281Fall)h.fallSound(z);if(events&Night281Destroy)h.destroy(z);
 return true;
}
}
bool NativeNightZombie281StateValid(const NativeNightZombie281State& s)
{
 FusionNightZombie281Clip clip;
 return TryGetFusionNightZombie281Clip(s.phase,clip)&&(!s.canAttack||s.moonWalkFinished);
}
bool InitializeNativeJackson281(Zombie* z,NativeNightZombie281State& s,const NativeNightZombie281Hooks& h)
{
 if(!Host(z,10)||!HookReady(h))return false;
 s=NativeNightZombie281State{};s.initialized=true;s.wasControlled=z->mMindControlled;
 return Enter(z,s,Phase::Moonwalk)&&Facing(z,s,true,h);
}
void NativeJackson281MindControlled(NativeNightZombie281State& s)
{s.dancers={};s.wasControlled=true;}
bool UpdateNativeJackson281Animation(Zombie* z,const NativeNightZombie281State& s)
{
 if(!Host(z,10))return false;auto* body=Body(z);const auto* d=GetNativeSourceZombieData(z);
 FusionNightZombie281Clip c;float factor=1;
 if(!body||!d||!TryGetFusionNightZombie281Clip(s.phase,c)||
   (s.phase!=Phase::Death&&!Factor(z,factor)))return false;
 z->mOriginalAnimRate=body->mFrameCount*c.multiplier*d->originSpeed*factor/c.seconds;
 body->mAnimRate=z->mOriginalAnimRate;return true;
}
bool UpdateNativeJackson281(Zombie* z,NativeNightZombie281State& s,const NativeNightZombie281Hooks& h)
{
 const auto* d=GetNativeSourceZombieData(z);if(!d||d->kind!=10)return false;
 if(!Host(z,10)||z->mDead)return true;
 if(!s.initialized||s.failed||!HookReady(h))return true;
 if(s.tick==z->mBoard->mMainCounter)return true;s.tick=z->mBoard->mMainCounter;
 if(z->mMindControlled&&!s.wasControlled)NativeJackson281MindControlled(s);
 if(h.outsideSourceBounds(z)){h.dieWithoutAnimation(z);return true;}
 if(z->IsDeadOrDying()&&s.phase!=Phase::Death)
 {s.transitionRemaining=0;s.summonPending=false;if(!Enter(z,s,Phase::Death)){s.failed=true;return true;}}
 float factor=1;FusionNightZombie281Clip clip;FusionZombieClipPosition now;
 if(!Factor(z,factor)||!TryGetFusionNightZombie281Clip(s.phase,clip)||!Read(Body(z),clip.seconds,now))return true;
 const auto before=s.eventPosition;const bool entering=s.entering;s.entering=false;
 if(!Events(z,s,s.phase,before,now,entering,h)){s.failed=true;return true;}
 s.eventPosition=now;if(z->mDead)return true;
 if(s.transitionRemaining>0)
 {
  FusionNightZombie281Clip old;TryGetFusionNightZombie281Clip(s.outgoing,old);
  const auto a=s.outgoingPosition;auto b=a;
  b.seconds=(std::min)(old.seconds,b.seconds+.01f*old.multiplier*d->originSpeed*factor);
  if(!Events(z,s,s.outgoing,a,b,false,h)){s.failed=true;return true;}
  s.outgoingPosition=b;s.transitionRemaining=(std::max)(0.f,s.transitionRemaining-.01f);
 }
 if(s.phase==Phase::Death){UpdateNativeJackson281Animation(z,s);return true;}
 s.moonWalkElapsed+=.01f;
 if(s.moonWalkElapsed>3&&!s.moonWalkFinished)s.moonWalkFinished=true;
 if(FusionJackson281MissingDancer(z->mRow,GetNativeFusionRowCount(z->mBoard),Present(z,s)))s.loseDancer=true;
 const bool contact=h.contact(z,s.canAttack);
 if(contact&&!s.canAttack&&!s.moonWalkFinished&&!h.travelDebuff13(z->mBoard))s.moonWalkFinished=true;
 if(s.summonPending)Summon(z,s,h);
 FusionNightZombie281Transition transition;
 if(s.transitionRemaining<=0&&TryGetFusionJackson281Transition(s.phase,before,now,factor>0,
   contact&&s.canAttack,s.loseDancer,s.moonWalkFinished,transition))
 {
  s.outgoing=s.phase;s.outgoingPosition=now;
  const float speed=clip.multiplier*d->originSpeed*factor;
  s.transitionRemaining=transition.fixedSeconds?transition.duration:transition.duration*clip.seconds/speed;
  if(!Enter(z,s,transition.target))s.failed=true;
  else
  {
   // Unity dispatches clip-time0 callbacks on entry, before movement/node queries.
   if(!Events(z,s,s.phase,{},{},true,h))s.failed=true;
   s.entering=false;
  }
 }
 UpdateNativeJackson281Animation(z,s);return true;
}
bool MoveNativeJackson281(Zombie* z,NativeNightZombie281State& s)
{
 const auto* d=GetNativeSourceZombieData(z);if(!d||d->kind!=10)return false;
 if(!Host(z,10)||z->mDead||s.failed||!s.initialized)return true;
 if(s.motionTick==z->mBoard->mMainCounter)return true;s.motionTick=z->mBoard->mMainCounter;
 FusionNightZombie281Clip c;FusionZombieClipPosition now;
 if(!TryGetFusionNightZombie281Clip(s.phase,c)||!Read(Body(z),c.seconds,now))return true;
 if(s.motionValid&&s.motionPhase==s.phase&&!z->IsDeadOrDying())
 {
  double dx=0;if(TryGetFusionNightZombie281RootDelta(s.phase,s.motionPosition,now,
   z->mMindControlled!=s.lookBack,1.2*kFusionBasePixelsX*z->mScaleZombie,dx)&&std::isfinite(z->mPosX+dx))z->mPosX+=float(dx);
 }
 s.motionPosition=now;s.motionPhase=s.phase;s.motionValid=true;return true;
}
bool GetNativeJackson281Presentation(const Zombie* z,const NativeNightZombie281State& s,const char*& name,float& phase)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* body=Body(z);FusionNightZombie281Clip c;
 if(!d||d->kind!=10||!NativeNightZombie281StateValid(s)||!body||!TryGetFusionNightZombie281Clip(s.phase,c)||
   !std::isfinite(body->mAnimTime)||body->mAnimTime<0||body->mAnimTime>1)return false;
 name=c.name;phase=body->mAnimTime;return true;
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
