#include "FusionNativeRandom360.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeZombies.h"
#include "FusionNativeEffects269.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
struct Ref {Board* board=nullptr;uint32_t epoch=0,uid=0;bool operator==(const Ref&)const=default;};
thread_local std::vector<Ref> active;
bool Reject(std::string* error,const char* why){if(error)*error=why;return false;}
bool World(Board* b)
{
 const auto* c=GetNativeContext(b);
 return b&&b->mApp&&c&&IsNativeSourceWorld313(b)&&b->mApp->mGameMode==GAMEMODE_ADVENTURE&&
  (c->adventure.enabled||c->validation)&&!b->IsPvPMode()&&!b->mApp->IsCoopEndlessMode(b->mApp->mGameMode);
}
bool Authority(const Zombie* z)
{return IsNativeRandom360(z)&&World(z->mBoard)&&!z->mNetGhost&&!z->mApp->NetIsClient();}
bool Playing(const Zombie* z)
{return Authority(z)&&!z->mDead&&!z->mBoard->mPaused&&z->mApp->mGameScene==SCENE_PLAYING;}
Ref Identity(const Zombie* z)
{
 const auto* c=z?GetNativeContext(z->mBoard):nullptr;
 return c?Ref{z->mBoard,c->epoch,z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z))}:Ref{};
}
struct Scope {explicit Scope(Ref r){active.push_back(r);}~Scope(){active.pop_back();}};
void MaxPeak(FusionSourceDeathBudget319& a,const FusionSourceDeathBudget319& b)
{
 a.zombies=(std::max)(a.zombies,b.zombies);a.projectiles=(std::max)(a.projectiles,b.projectiles);
 a.reanimations=(std::max)(a.reanimations,b.reanimations);a.attachments=(std::max)(a.attachments,b.attachments);
 a.coins=(std::max)(a.coins,b.coins);a.droppedCards=(std::max)(a.droppedCards,b.droppedCards);
 a.particleSystems=(std::max)(a.particleSystems,b.particleSystems);a.emitters=(std::max)(a.emitters,b.emitters);
}
struct Plan {std::vector<uint16_t> pool;float shadowX=0,shadowY=0,rootX=0;bool buffCooldown=false;};
bool Prepare(Zombie* z,const NativeRandom360Hooks& h,Plan& out,std::string* error)
{
 if(!Playing(z))return Reject(error,"Random360 requires current authoritative ordinary source Board");
 const auto* d=GetNativeSourceZombieData(z);Plan p;
 if(!d||!BuildFusionRandom360Pool(d->kind,z->mMindControlled,p.pool,error)||
    !EnsureNativeRandom360Resources(z->mBoard,d->kind,error))return false;
 float rootY=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",p.shadowX,p.shadowY)||
    !GetNativeSourceZombie269NodePoint(z,"",p.rootX,rootY))return Reject(error,"Random360 actual root/Shadow sample unavailable");
 // Common closure validates EVERY source outcome, including self-recursive
 // random roots. The central DFS must add this family's dependencies once.
 if(!EnsureNativeSourceZombieClosure280(z->mBoard,p.pool,error))return false;
 const bool water=z->mBoard->mPlantRow[z->mRow]==PLANTROW_POOL;FusionSourceDeathBudget319 peak;bool possible=false;
 for(uint16_t selected:p.pool)
 {
  uint16_t emitted=0;if(!ResolveFusionRandom360DirectKind(selected,water,emitted))continue; // Genuine source-null; stays in RNG pool.
  possible=true;
  if(h.prepareOutcome)
  {if(!h.prepareOutcome(z->mBoard,emitted,z->mRow,z->mMindControlled,error))return false;}
  else
  {
   const auto* profile=FindFusionZombieProfile(emitted);
   if(!profile||(profile->waterOnly&&!water))return Reject(error,"Random360 source-legal direct dry/water outcome not admitted by native factory");
  }
  const uint16_t one[]={emitted};FusionSourceDeathBudget319 birth;
  if(!GetNativeSourceZombieBirthBudget319(z->mBoard,one,birth,error))return false;MaxPeak(peak,birth);
  // Ordinary native Source factories use the imported source stat envelope;
  // coopEndless/PvP are excluded above. Prove every possible scalar before RNG.
  const auto* profile=FindFusionZombieProfile(emitted);FusionRandom360Health scaled;
  if(!profile||!ScaleFusionRandom360Health({float(profile->maxHealth),profile->maxHealth,
      profile->firstArmor,profile->firstArmor,profile->secondArmor,profile->secondArmor},1.2f,scaled))
      return Reject(error,"Random360 possible child health scaling exceeds native bounds");
 }
 if(!possible)return Reject(error,"Random360 source pool has no nonnull outcome on this row");
 if(!z->mMindControlled)
 {
  if(!h.travelBuffCooldown||!h.travelBuffCooldown(z->mBoard,p.buffCooldown))return Reject(error,"Random360 canonical lastRandomBuffTime unavailable");
  if(!p.buffCooldown&&(!h.prepareTravelBuff||!h.unlockTravelBuff||!h.prepareTravelBuff(z->mBoard,error)))
      return Reject(error,"Random360 full source rare Travel buff outcome closure is not prepared");
 }
 const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);const uint32_t ids[]={uid};
 if(!CanRetireNativeSourceZombieBatch313(z->mBoard,ids,2,peak,error))return false;
 out=std::move(p);if(error)error->clear();return true;
}
Zombie* Spawn(Zombie* parent,uint16_t kind,float x,const NativeRandom360Hooks& h)
{
 if(h.spawnOutcome)return h.spawnOutcome(parent,kind,x);
 NativeSourceZombieSpawnRequest request;request.kind=kind;request.row=parent->mRow;request.fromWave=parent->mFromWave;
 request.positioned=true;request.shadowX=x;request.friendly=parent->mMindControlled;request.withEffect=false;
 return SpawnNativeSourceZombie(parent->mBoard,request);
}
bool Fall(Zombie* z,const NativeRandom360Hooks& h,std::string* error)
{
 if(!Authority(z))return Reject(error,"Random360 fall is authority-only");
 if(z->mDead||z->IsDeadOrDying()||NativeRandom360FallInProgress(z))return true;
 Plan p;if(!Prepare(z,h,p,error))return false;
 const auto* d=GetNativeSourceZombieData(z);const uint16_t parentKind=d->kind;
 const Ref ref=Identity(z);if(!ref.uid)return false;Scope scope(ref);
 // Source ordinary FirstArmorFall always consumes the chance draw, even when
 // cooldown/control makes the rare branch ineligible. No invented RNG filter.
 const int chance=RandRangeInt(0,199);bool awarded=false;
 if(chance==0&&!p.buffCooldown&&!z->mMindControlled)
 {if(!h.unlockTravelBuff||!h.unlockTravelBuff(z,awarded,error))return false;}
 Zombie* child=nullptr;
 if(!awarded)
 {
  // Source retries ONLY null results. Native bound prevents an infinite stall;
  // hitting it is a refusal, never a changed weighted pool or fallback kind.
  for(unsigned attempt=0;attempt<65536&&!child;++attempt)
  {
   const auto selected=p.pool[size_t(RandRangeInt(0,int(p.pool.size())-1))];uint16_t emitted=0;
   if(!ResolveFusionRandom360DirectKind(selected,z->mBoard->mPlantRow[z->mRow]==PLANTROW_POOL,emitted))continue;
   child=Spawn(z,emitted,p.shadowX,h);
   if(!child)return Reject(error,"Random360 prepared nonnull native child factory unexpectedly failed");
  }
  if(!child)return Reject(error,"Random360 source-null retry bound reached without substitution");
  auto* state=GetNativeSourceZombieData(child);float factor=1;
  const int draw=parentKind==105?RandRangeInt(1,6):parentKind==215?RandRangeInt(4,6):0;
  FusionRandom360Health health;
  if(!state||!FusionRandom360ScaleFromDraw(parentKind,draw,factor)||
    !ScaleFusionRandom360Health({state->health,child->mBodyMaxHealth,child->mHelmHealth,child->mHelmMaxHealth,
        child->mShieldHealth,child->mShieldMaxHealth},factor,health))return Reject(error,"Random360 child post-factory stat invariant failed");
  state->health=health.health;child->mBodyHealth=NativeSourceZombieLivenessHealth(health.health);child->mBodyMaxHealth=health.maximum;
  child->mHelmHealth=health.first;child->mHelmMaxHealth=health.firstMaximum;child->mShieldHealth=health.second;child->mShieldMaxHealth=health.secondMaximum;
 }
 // Source marks summoned before cosmetic11 and synchronous virtual Die2.
 // This generation-scoped synchronous lease prevents the head hook reentry.
 SpawnNativeSourceEffect269(z->mBoard,11,p.rootX,p.shadowY-float(GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder+1,true);
 LawnNet::NetTrace("Fusion Random360 replacement parent=%u token=%u kind=%u child=%u buff=%d",z->mNetId,d->instanceToken,parentKind,child?child->mNetId:0,awarded?1:0);
 return RetireNativeSourceZombie313(z,2);
}
}
bool IsNativeRandom360(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return z&&d&&FusionRandomZombie360Kind(d->kind)&&z->mBoard&&IsNativeFusionBoard(z->mBoard);}
bool NativeRandom360FallInProgress(const Zombie* z)
{const auto ref=Identity(z);return ref.uid&&std::find(active.begin(),active.end(),ref)!=active.end();}
bool EnsureNativeRandom360Resources(Board* b,uint16_t k,std::string* error)
{return World(b)&&FusionRandomZombie360Kind(k)&&EnsureNativeSourceZombieVisuals(b,k,error)&&CanQueryNativeSourceZombieRoot273(b,k)&&EnsureNativeSourceEffect269(b,11,error);}
bool PreflightNativeRandom360Fall(Zombie* z,const NativeRandom360Hooks& h,std::string* error)
{if(!Authority(z))return false;if(z->mDead||z->IsDeadOrDying()||NativeRandom360FallInProgress(z))return true;Plan p;return Prepare(z,h,p,error);}
bool ApplyNativeRandom360FirstArmorDamage(Zombie* z,int damage,int& overflow,const NativeRandom360Hooks& h,std::string* error)
{
 if(!IsNativeRandom360(z)||damage<0)return false;if(z->mNetGhost||z->mApp->NetIsClient()){overflow=0;return true;}
 if(!Playing(z))return false;auto* d=GetNativeSourceZombieData(z);FusionRandom360ArmorDamage plan;
 if(!d||!PlanFusionRandom360ArmorDamage(d->kind,z->mHelmHealth,z->mHelmMaxHealth,d->firstArmorStage,damage,z->IsDeadOrDying(),plan))return false;
 if(plan.fall){if(!Fall(z,h,error))return false;if(z->mDead){overflow=0;return true;}}
 z->mHelmHealth=plan.armor;d->firstArmorStage=plan.stage;overflow=0;
 if(!plan.armor){z->mHelmType=HELMTYPE_NONE;z->mHasObject=false;}return true;
}
bool ApplyNativeRandom360HeadLoss(Zombie* z,const NativeRandom360Hooks& h,std::string* error)
{
 if(!IsNativeRandom360(z))return false;if(z->mNetGhost||z->mApp->NetIsClient())return true;
 return Fall(z,h,error);
}
}
