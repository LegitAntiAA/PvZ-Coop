#include "FusionNativeSourceLifecycle313.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeNightZombie291Bridge.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeBatchActions.h"
#include "FusionDayZombie273Rules.h"
#include "NativeSpecialZombie269.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace LawnFusion
{
// Root's Water bridge supplies the real state hooks, not a second controller.
const NativeWaterZombie313Hooks& GetNativeWaterZombie313Hooks();
namespace
{
bool Reject(std::string* e,const char* why){if(e)*e=why;return false;}
bool Authority(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&!b->mApp->NetIsClient()&&IsNativeFusionBoard(b)&&GetNativeContext(b);}
struct Ref
{
 Board* board=nullptr;uint32_t uid=0,epoch=0,token=0;uint16_t kind=0;
 Zombie* Get()const
 {auto* c=GetNativeContext(board);if(!c||c->epoch!=epoch)return nullptr;auto* z=board->mZombies.DataArrayTryToGet(uid);const auto* d=GetNativeSourceZombieData(z);return z&&!z->mDead&&d&&d->kind==kind&&d->instanceToken==token?z:nullptr;}
};
bool Capture(Zombie* z,Ref& r)
{if(!z||!Authority(z->mBoard)||z->mNetGhost||z->mDead)return false;const auto* d=GetNativeSourceZombieData(z);if(!d||!d->instanceToken)return false;r={z->mBoard,z->mBoard->mZombies.DataArrayGetID(z),GetNativeContext(z->mBoard)->epoch,d->instanceToken,d->kind};return r.uid&&r.Get()==z;}
thread_local std::vector<Ref> active;
struct Scope{explicit Scope(const Ref& r){active.push_back(r);}~Scope(){active.pop_back();}};
bool Active(const Ref& r)
{for(const auto& a:active)if(a.board==r.board&&a.uid==r.uid&&a.epoch==r.epoch&&a.token==r.token)return true;return false;}
int Reason(uint16_t kind,int reason){return kind==45?(reason==1?1:2):reason;}
bool Definition(ReanimationType type,std::string* error)
{
 ReanimatorEnsureDefinitionLoaded(type,false);
 if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return Reject(error,"Source death/birth native definition unavailable.");
 const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
 if(!tracks.tracks||tracks.count<=0)return Reject(error,"Source death/birth native definition has no tracks.");
 for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&_strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)
  return Reject(error,"Source death/birth definition has an unbudgeted dynamic attacher.");
 return true;
}
template<class T>bool Pool(const DataArray<T>& pool,uint64_t& used,uint64_t& capacity)
{if(!pool.mBlock||pool.mSize>pool.mMaxSize)return false;used=pool.mSize;capacity=pool.mMaxSize;return true;}
bool Leaves(Zombie* z,int reason,FusionSourceDeathBudget319& out,std::string* error)
{
 Ref r;if(!Capture(z,r))return Reject(error,"Source retirement requires an authoritative full local generation.");
 auto* c=GetNativeContext(r.board);FusionSourceDeathPending319 p;p.kind=r.kind;p.reason=Reason(r.kind,reason);p.controlled=z->mMindControlled;
 if(c->sourceDeathPoints313.contains(r.uid)){out={};return true;} // Already in genuine BaseDie: no callback/resource replay.
 const double maximum=double(z->mBodyMaxHealth)+z->mHelmMaxHealth+z->mShieldMaxHealth;
 if(maximum<0||!std::isfinite(c->sourcePoints+float(maximum/12000.0)))return Reject(error,"Source death points invalid before any callback/status mutation.");
 if(FindFusionExtendedZombie313Profile(r.kind))
 {
  const auto* s=GetNativeExtendedZombie313(z);if(!s)return Reject(error,"Source extended retirement state missing.");
  if(!s->initialized||s->failed)return Reject(error,"Source extended retirement controller invalid.");
  p.consumed=s->deathConsumed;p.machineType=s->machineType;
  const auto& h=GetNativeExtendedZombie313Hooks();NativeExtendedZombie313Input input;
  if(!h.input(z,input))return Reject(error,"Source extended retirement input unavailable.");p.changingRow=input.changingRow;
  if(!p.consumed&&!h.preflight(z,r.kind==40?NativeExtendedZombie313Operation::MachineDeath:r.kind==26?NativeExtendedZombie313Operation::KirovDeath:NativeExtendedZombie313Operation::IronDeath,error))return false;
 }
 else if(NativeHeavyZombie313BKind(r.kind))
 {
  const auto* s=GetNativeHeavyZombie313B(z);if(!s||!s->initialized||s->failed||!CanRetireNativeHeavyZombie313B(z,p.reason,error))return false;
  p.awardLevel=s->gold.awardLevel;p.pointsCommitted=s->gold.pointsCommitted;p.deathCommitted=s->gold.deathVisualCommitted;p.rewardCommitted=s->gold.rewardCommitted;
 }
 else if(FusionWaterZombie313Kind(r.kind))
 {
  const auto it=c->waterZombies313.find(r.uid);const auto& h=GetNativeWaterZombie313Hooks();
  if(it==c->waterZombies313.end()||!it->second.initialized||it->second.failed||!h.prepare||
   (!it->second.deathCallbackHandled&&!h.prepare(z,NativeWaterZombie313Operation::Death,error)))return false;
 }
 else if(FusionJackbox300Kind(r.kind))
 {
  const auto* s=GetNativeJackbox309(z);if(!s||!s->initialized||s->failed)return false;
  p.consumed=s->deathEventHandled;p.boxPresent=s->boxPresent;p.countdownStarted=s->countdown<s->originalCountdown;
  const auto& h=GetNativeJackbox309Hooks();NativeJackbox300Explosion e;float x=0,y=0;
  if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
  e.ownerId=z->mNetId;e.ownerToken=r.token;e.sourceKind=r.kind;e.row=z->mRow;e.fromWave=z->mFromWave;e.targetRow=-1;e.renderOrder=z->mRenderOrder;
  e.shadowX=e.centerX=x;e.shadowY=y;e.centerY=float(y-.7*GetNativeFusionPixelsY(r.board));e.controlled=z->mMindControlled;
  NativeJackbox300Input input;if(!h.input(z,input))return false;e.targetRow=input.targetRow;
  if(!p.consumed&&!h.preflight(z,NativeJackbox300Operation::Death,&e,p.kind==30&&p.reason==0&&p.boxPresent&&p.countdownStarted,error))return false;
 }
 // Source base Die1 creates genuine11 and one of source sounds0..2. Sound
 // mappings are an explicit integration dependency, never an invented groan.
 if(p.reason==1)
 {
  if(!EnsureNativeSourceEffect269(r.board,11,error))return false;
  for(uint16_t sound=0;sound<3;++sound)if(!LawnNet::NetSourceSound284Ready(z->mApp,sound))return Reject(error,"Source Die1 sound0/1/2 mapping unavailable.");
 }
 if(!PlanFusionSourceDeathLeaves319(p,out))return Reject(error,"Source mandatory death leaf plan invalid.");
 return true;
}
bool Points(const Ref& r)
{
 auto* z=r.Get();auto* c=GetNativeContext(r.board);if(!z||!c)return true;
 if(c->sourceDeathPoints313.contains(r.uid))return true;
 if(r.kind==203||r.kind==223){c->sourceDeathPoints313.insert(r.uid);return true;} // Gold's pointsCommitted owns its source override.
 const double sum=double(z->mBodyMaxHealth)+z->mHelmMaxHealth+z->mShieldMaxHealth;
 const float next=c->sourcePoints+float(sum/12000.0);if(sum<0||!std::isfinite(next))return false;
 c->sourceDeathPoints313.insert(r.uid);c->sourcePoints=next;return true;
}
// Source BaseDie381A20 before its virtual DieEvent. The native counters are
// the existing projection of freeze/butter speed, not newly advanced clocks.
bool ClearDeathStatus(const Ref& r)
{
 auto* z=r.Get();if(!z)return true;const bool frozen=z->mIceTrapCounter>0;
 if(frozen){z->RemoveIceTrap();if(!(z=r.Get()))return true;}
 if(auto* cold=GetNativeZombieColdData(z))
 {if(frozen)cold->level=0;cold->frozenTicks=0;cold->chilledTicks=z->mChilledCounter;}
 // Native RemoveButter resets presentation; unlike Unity's butterSpeed
 // scalar, this engine derives stopped speed from the real counter itself.
 const bool buttered=z->mButteredCounter>0;z->mButteredCounter=0;
 if(buttered){z->RemoveButter();if(!(z=r.Get()))return true;}
 if(auto* butter=GetNativeZombieButterData(z)){butter->ticks=0;butter->visible=false;}
 // Poison's source level is cleared by BaseDie before DieEvent. Preserve
 // Doom/Jala tags for their real consume-before-recursion death callbacks.
 if(auto* action=GetNativeZombieActionData(z))action->poisonLevel=0;
 return true;
}
bool AfterDeathEvent(const Ref& r)
{
 if(!Points(r))return false;
 if(auto* z=r.Get())NativeBatchZombieDying(z); // Source points BEFORE JalaedExplode/drop callbacks.
 return true;
}
bool Finish(const Ref& r,int reason)
{
 if(!AfterDeathEvent(r))return false;auto* z=r.Get();if(!z)return true;
 if(reason==1)
 {
  float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
  if(!LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(RandRangeInt(0,2)),.5f))return false;
  SpawnNativeSourceEffect269(r.board,11,x,float(y-GetNativeFusionPixelsY(r.board)),z->mRenderOrder+1,true);
  if(!(z=r.Get()))return true;
 }
 // Existing bounded source DropItem/Garden policy remains owned centrally;
 // a conversion must not invoke native random loot a second time here.
 z->DieNoLoot();return !r.Get();
}
}
bool IsNativeSourceRetiring313(const Zombie* z){Ref r;return Capture(const_cast<Zombie*>(z),r)&&Active(r);}
bool NativeSourceDeathIgnoresGarlic313(const Zombie* z)
{Ref r;if(!Capture(const_cast<Zombie*>(z),r))return false;const auto* c=GetNativeContext(r.board);return Active(r)||c->sourceDeathPoints313.contains(r.uid);}
bool GetNativeSourceZombieBirthBudget319(Board* b,std::span<const uint16_t> kinds,FusionSourceDeathBudget319& out,std::string* error)
{
 if(error)error->clear();if(!Authority(b))return Reject(error,"Source birth peak requires authoritative Board.");FusionSourceDeathBudget319 sum;
 for(auto kind:kinds)
 {
  const int carrier=GetNativeSourceZombieCarrier(kind);
  if(carrier!=ZOMBIE_NORMAL&&carrier!=ZOMBIE_TRAFFIC_CONE&&carrier!=ZOMBIE_PAIL&&carrier!=ZOMBIE_DOOR&&carrier!=ZOMBIE_POLEVAULTER&&carrier!=ZOMBIE_NEWSPAPER&&carrier!=ZOMBIE_FOOTBALL&&
     !(carrier==ZOMBIE_ZAMBONI&&(kind==14||kind==16||kind==18)))
   return Reject(error,"Source birth peak has an uncovered native initializer.");
  if(!Definition(GetZombieDefinition(ZombieType(carrier)).mReanimationType,error))return false;
  FusionSourceDeathBudget319 one;one.zombies=one.reanimations=1;
  if(const auto* graph=FindFusionSourceGraph313(kind))
  {
   if(graph->layerCount<1||graph->layerCount>3||carrier!=ZOMBIE_NORMAL)return false;
   const auto& tracks=gReanimatorDefArray[int(REANIM_ZOMBIE)].mTracks;
   const auto span=[&](const char* name){for(int n=0;n<tracks.count;++n)if(tracks.tracks[n].mName&&!_stricmp(tracks.tracks[n].mName,name)&&tracks.tracks[n].mTransforms.count>0)return true;return false;};
   for(unsigned n=0;n<graph->stateCount;++n)if(!span(graph->states[n].nativeTrack))return Reject(error,"Source graph actual native track span unavailable.");
   if(graph->layerCount>1&&!span("anim_head1"))return Reject(error,"Source auxiliary actual native head track span unavailable.");
   one.reanimations=graph->layerCount;one.attachments=graph->layerCount>1?1:0;
  }
  else if(FusionWaterZombie313Kind(kind))one.reanimations=kind==20?1:2;
  else if(FusionDayZombie273Shooter(kind)||kind==104){if(!Definition(REANIM_PEASHOOTER,error))return false;one.reanimations++;one.attachments++;}
  if(!AddFusionSourceDeathBudget319(sum,one,sum))return Reject(error,"Source birth peak overflow.");
 }
 out=sum;return true;
}
bool CanAllocateNativeSourceBudget319(Board* b,const FusionSourceDeathBudget319& peak,std::string* error)
{
 if(!Authority(b))return Reject(error,"Source whole peak requires authoritative Board.");
 auto* effects=b->mApp->mEffectSystem;if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder||!effects->mParticleHolder)return false;
 FusionSourceDeathBudget319 used,cap;
 if(!Pool(b->mZombies,used.zombies,cap.zombies)||!Pool(b->mProjectiles,used.projectiles,cap.projectiles)||!Pool(b->mCoins,used.coins,cap.coins)||
  !Pool(effects->mReanimationHolder->mReanimations,used.reanimations,cap.reanimations)||!Pool(effects->mAttachmentHolder->mAttachments,used.attachments,cap.attachments)||
  !Pool(effects->mParticleHolder->mParticleSystems,used.particleSystems,cap.particleSystems)||!Pool(effects->mParticleHolder->mEmitters,used.emitters,cap.emitters))return Reject(error,"Source whole peak native pool invalid.");
 // Board.AddZombieInRow reserves one spare slot, even for a single birth.
 if(peak.zombies){if(cap.zombies)cap.zombies--;else return false;}
 Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead&&GetNativeDroppedCard269Data(coin))++used.droppedCards;cap.droppedCards=128;
 return FusionSourceDeathBudgetFits319(used,peak,cap)||Reject(error,"Source whole mandatory peak exceeds actual native capacity.");
}
bool PlanNativeSourceRetireBatch319(Board* b,std::span<const uint32_t> ids,int reason,FusionSourceDeathBudget319& out,std::string* error)
{
 if(error)error->clear();if(!Authority(b)||reason<0||reason>2)return Reject(error,"Source retirement reason/Board invalid.");
 FusionSourceDeathBudget319 sum;std::unordered_set<uint32_t> seen;
 for(auto id:ids)
 {
  if(!id||!seen.insert(id).second)return Reject(error,"Source retirement batch has zero/duplicate generation.");
  auto* z=b->mZombies.DataArrayTryToGet(id);FusionSourceDeathBudget319 own;
  if(!Leaves(z,reason,own,error)||!AddFusionSourceDeathBudget319(sum,own,sum))return false;
 }
 out=sum;return true;
}
bool CanRetireNativeSourceZombieBatch313(Board* b,std::span<const uint32_t> ids,int reason,const FusionSourceDeathBudget319& births,std::string* error)
{FusionSourceDeathBudget319 peak;if(!PlanNativeSourceRetireBatch319(b,ids,reason,peak,error)||!AddFusionSourceDeathBudget319(peak,births,peak))return false;return CanAllocateNativeSourceBudget319(b,peak,error);}
bool CanRetireNativeSourceZombieBatch313(Board* b,std::span<const uint32_t> ids,int reason,std::string* error)
{return CanRetireNativeSourceZombieBatch313(b,ids,reason,FusionSourceDeathBudget319{},error);}
bool CanRetireNativeSourceZombie313(Zombie* z,int reason,std::string* error)
{Ref r;if(!Capture(z,r))return false;const std::array<uint32_t,1> ids={r.uid};return CanRetireNativeSourceZombieBatch313(r.board,ids,reason,error);}
bool RetireNativeSourceZombie313(Zombie* z,int reason)
{
 Ref r;if(!Capture(z,r)||reason<0||reason>2)return false;reason=Reason(r.kind,reason);
 if(Active(r))return Finish(r,reason); // A real source virtual Die leaf reentered after its own callback.
 if(!CanRetireNativeSourceZombie313(z,reason))return false;Scope scope(r);
 auto* c=GetNativeContext(r.board);const bool already=c->sourceDeathPoints313.contains(r.uid);
 if(already&&reason==0)return true; // Do not restart an actual source death clock on each HP update.
 if(already){z->DieNoLoot();return !r.Get();} // BaseDie status1: changed reason destroys without replaying DieEvent/FX/points.
 if(!ClearDeathStatus(r))return false;if(!(z=r.Get()))return true;
 if(auto* s=EditNativeExtendedZombie313(z))
 {
  const auto result=BeginNativeExtendedZombie313Death(z,*s,reason,GetNativeExtendedZombie313Hooks());
  if(result==NativeExtendedZombie313Death::Refused||result==NativeExtendedZombie313Death::NotHandled)return false;
  if(result==NativeExtendedZombie313Death::OwnerRetired)return true;
 }
 else if(NativeHeavyZombie313BKind(r.kind))
 {
  if(!already&&!BeforeRetireNativeHeavyZombie313B(z,reason))return false;if(!(z=r.Get()))return true;
  if(reason==0&&!BeginNativeHeavyZombie313BActorDeath(z))return false;
 }
 else if(FusionWaterZombie313Kind(r.kind))
 {
  auto it=c->waterZombies313.find(r.uid);if(it==c->waterZombies313.end()||!BeginNativeWaterZombie313Death(z,it->second,reason,GetNativeWaterZombie313Hooks()))return false;
  if(!r.Get())return true;
 }
 else if(FusionJackbox300Kind(r.kind))
 {
  const auto result=BeginNativeJackbox309Death(z,reason);if(result==NativeJackbox300DeathResult::Refused||result==NativeJackbox300DeathResult::NotHandled)return false;
  if(result==NativeJackbox300DeathResult::OwnerRetired)return true;
 }
 else if(r.kind==10&&reason==0&&!BeginNativeJackson291Death(z))return false;
 if(reason!=0)return Finish(r,reason);
 if(!AfterDeathEvent(r))return false;if(!(z=r.Get()))return true;
 RetireNativeSourceAdventureEnemy273(z);if(!(z=r.Get()))return true;
 if(FindFusionSourceGraph313(r.kind)||FusionWaterZombie313Kind(r.kind)||FusionJackbox300Kind(r.kind)||r.kind==10)
 {z->mVelX=0;z->mIsEating=false;z->mZombiePhase=PHASE_ZOMBIE_DYING;return true;}
 z->PlayDeathAnim(0);return true; // Existing admitted legacy native death path, under the reentry scope.
}
}
