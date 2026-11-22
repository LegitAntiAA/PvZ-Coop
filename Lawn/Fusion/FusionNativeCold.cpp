#include "FusionNativeCold.h"
#include "FusionColdRules.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeDefender.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeCherryShooter.h"
#include "FusionRollingProjectileRules.h"
#include "FusionBasePlantRules.h"
#include "FusionBatchActionRules.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <cmath>
#include <limits>
#include <vector>

namespace LawnFusion
{
namespace
{
bool HostObject(const GameObject* object)
{
    return object && object->mApp && !object->mApp->NetIsClient() && !object->mNetGhost &&
        IsNativeFusionBoard(object->mBoard) && NativeFusionBackgroundValid(static_cast<uint8_t>(object->mBoard->mBackground));
}
bool HostTarget(Zombie* zombie)
{
    if(!HostObject(zombie)||zombie->IsDeadOrDying()||zombie->mBodyHealth<=0||
        zombie->mMindControlled||(GetNativeSourceZombieData(zombie)&&zombie->mOnHighGround))return false;
    if(IsNativeZombie313(zombie)){NativeZombie313Semantics v;return GetNativeZombie313Semantics(zombie,v)&&v.status!=1;}
    if(!NativeFusionGroundTerrainAllowed(static_cast<uint8_t>(zombie->mBoard->mBackground),zombie->mRow,zombie->mInPool))return false;
    const auto* source=GetNativeSourceZombieData(zombie);
    const bool paper=source&&(source->kind==15||source->kind==104)&&
        (zombie->mZombiePhase==PHASE_NEWSPAPER_READING||zombie->mZombiePhase==PHASE_NEWSPAPER_MADDENING||zombie->mZombiePhase==PHASE_NEWSPAPER_MAD);
    return (IsNativeFusionGroundZombie(zombie)&&!zombie->IsFlying()&&(!source||zombie->mZombiePhase==PHASE_ZOMBIE_NORMAL||paper))||
        zombie->mZombieType==ZOMBIE_BALLOON;
}
FusionColdState ReadState(Zombie* zombie, const NativeZombieColdData& data)
{
    return {data.level,data.threshold,zombie->mChilledCounter,zombie->mIceTrapCounter};
}
void Commit(Zombie* zombie, NativeZombieColdData& data, const FusionColdState& requested,int sourceOperation=0,int sourceTicks=0)
{
    auto state=requested;
    if(const auto* source=GetNativeSourceZombieData(zombie))
    {
        const int status=GetNativeFusionZombieStatus(zombie);
        if(SpecialZombie269RejectEffect(source->kind,status,SpecialZombie269StatusEffect::Cold))state.chilledTicks=zombie->mChilledCounter;
        if(SpecialZombie269RejectEffect(source->kind,status,SpecialZombie269StatusEffect::Freeze))state.frozenTicks=zombie->mIceTrapCounter;
        // Source AddfreezeLevel commits level/threshold before virtual SetFreeze.
    }
    data.level = state.level;
    data.threshold = state.threshold;
    if (zombie->mDead || zombie->IsDeadOrDying() || zombie->mBodyHealth <= 0)
    {
        // A committed post-damage callback may still own the retiring DataArray
        // generation. Retain its counters without recreating removed effects.
        zombie->mChilledCounter = state.chilledTicks;
        zombie->mIceTrapCounter = state.frozenTicks;
    }
    else if(IsNativeZombie313(zombie))
    {
        if(sourceOperation==1)NativeZombie313ApplyCold(zombie,false,sourceTicks);
        else if(sourceOperation==2)NativeZombie313ApplyCold(zombie,true,sourceTicks);
        else if(state.frozenTicks>zombie->mIceTrapCounter)NativeZombie313ApplyCold(zombie,true,state.frozenTicks);
        else zombie->SetColdCounters(state.chilledTicks,state.frozenTicks);
    }
    else zombie->SetColdCounters(state.chilledTicks,state.frozenTicks);
    // These are snapshot mirrors, never ticked here.
    data.chilledTicks = zombie->mChilledCounter;
    data.frozenTicks = zombie->mIceTrapCounter;
}
bool Piercing(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    const auto* profile = data ? FindFusionColdProjectileProfile(data->kind) : nullptr;
    return profile && profile->maximumHits > 1 && IsNativeColdProjectile(projectile);
}
bool Overlaps(const Sexy::Rect& a, const Sexy::Rect& b)
{
    return a.mX <= b.mX+b.mWidth && a.mX+a.mWidth >= b.mX &&
        a.mY <= b.mY+b.mHeight && a.mY+a.mHeight >= b.mY;
}
void IceDoomBurst(Projectile* projectile)
{
    float x=0,y=0;
    if (!GetNativeTimedPeaProjectileCenter(projectile,x,y)) return;
    // AoeDamage.MidBomb calls Bomb with rangeRow1 (3D3AF4), radius1.5,
    // damage10/Explode10. It includes the current and earlier direct victims.
    std::vector<ZombieID> victims; Zombie* zombie=nullptr;
    while (projectile->mBoard->IterateZombies(zombie))
    {
        if (!HostTarget(zombie) || !FusionColdBurstCanDamage(GetNativeFusionZombieStatus(zombie),
            zombie->mMindControlled,zombie->mRow-projectile->mRow,1)) continue;
        const auto r=zombie->GetZombieRect();
        if (FusionBatchCircleTouchesRect(x,y,1.5,r.mX,r.mY,r.mWidth,r.mHeight)) victims.push_back(projectile->mBoard->ZombieGetID(zombie));
    }
    for (auto id:victims)
        if ((zombie=projectile->mBoard->ZombieTryToGet(id)) && HostTarget(zombie)) zombie->TakeDamage(10,18U);
    projectile->mApp->AddTodParticle(x,y,projectile->mRenderOrder+1,PARTICLE_POWIE);
    LawnNet::NetBroadcastParticle(projectile->mApp,x,y,projectile->mRenderOrder+1,PARTICLE_POWIE);
    LawnNet::NetTrace("Fusion native IceDoomBullet burst id=%u targets=%zu damage=10 tick=%d",projectile->mNetId,victims.size(),projectile->mBoard->mMainCounter);
}
}

bool IsNativeIceShroom(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && data->kind == 10;
}
void InitializeNativeIceShroom(Plant* plant)
{
    if (!IsNativeIceShroom(plant) || !HostObject(plant) || plant->mDead) return;
    plant->SetSleeping(false);
    plant->mWakeUpCounter = 0;
    plant->mState = STATE_DOINGSPECIAL;
    plant->mDoSpecialCountdown = kFusionIceShroomFuseTicks;
    LawnNet::NetTrace("Fusion native IceShroom init id=%u token=%u fuse=%d tick=%d",
        plant->mNetId,GetNativePlantData(plant)->instanceToken,plant->mDoSpecialCountdown,plant->mBoard->mMainCounter);
}
bool ApplyNativeIceShroom(Plant* plant)
{
    if (!IsNativeIceShroom(plant)) return false;
    if (!HostObject(plant) || plant->mDead) return true;
    float x = static_cast<float>(plant->mBoard->GetCellCenterX(plant->mPlantCol,plant->mRow));
    float y = static_cast<float>(plant->mBoard->GridToPixelY(plant->mPlantCol,plant->mRow)+50);
    plant->GetShadowCenter(x,y);
    if (!ApplyNativeColdBoardFreeze(plant->mBoard,x,y)) return true;
    LawnNet::NetTrace("Fusion native IceShroom effect plant=%u tick=%d",plant->mNetId,plant->mBoard->mMainCounter);
    plant->Die();
    return true;
}
bool ApplyNativeColdBoardFreeze(Board* board, float x, float y)
{
    if (!board || !board->mApp || board->mApp->mBoard!=board || board->mApp->NetIsClient() ||
        !IsNativeFusionBoard(board) || !NativeFusionBackgroundValid(static_cast<uint8_t>(board->mBackground)) || !std::isfinite(x) || !std::isfinite(y)) return false;
    // Also a continuation of an already-admitted death effect. Do not reject
    // the remaining callbacks merely because a previous kill awarded the level.
    NativeDefenderElementPulse(board,FusionDefenderElement::Freeze);
    NativeMachine268Element(board,-1,1);
    std::vector<ZombieID> targets;
    Zombie* zombie = nullptr;
    while (board->IterateZombies(zombie))
        if (HostTarget(zombie)) targets.push_back(board->ZombieGetID(zombie));
    for (const auto id : targets)
    {
        zombie = board->ZombieTryToGet(id);
        if (!HostTarget(zombie)) continue;
        if (!ApplyNativeColdFreeze(zombie,kFusionColdFreezeTicks)) continue;
        zombie->TakeDamage(20,2U); // NormalAll1; no Ice2 SetCold overwrite.
        const auto* data=GetNativeZombieColdData(zombie);
        if (!data) continue;
        LawnNet::NetTrace("Fusion native board freeze target=%u level=%d threshold=%d slow=%d freeze=%d damage=20 tick=%d",
            zombie->mNetId,data->level,data->threshold,zombie->mChilledCounter,zombie->mIceTrapCounter,board->mMainCounter);
    }
    board->mApp->PlayFoley(FOLEY_FROZEN);
    board->mApp->AddTodParticle(x,y,static_cast<int>(RENDER_LAYER_TOP),PARTICLE_ICE_TRAP);
    LawnNet::NetBroadcastParticle(board->mApp,x,y,static_cast<int>(RENDER_LAYER_TOP),PARTICLE_ICE_TRAP);
    return true;
}
bool IsNativeColdProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    return data && FindFusionColdProjectileProfile(data->kind) && IsNativeTimedPeaProjectile(projectile);
}
bool PrepareNativeColdHit(Projectile* projectile, Zombie* target, int& damage, FusionColdDamageContext& context)
{
    if (!IsNativeColdProjectile(projectile) || !HostObject(projectile) || projectile->mDead ||
        !HostTarget(target) || target->mBoard != projectile->mBoard ||
        !NativeTimedPeaProjectileCanHit(projectile,target)) return false;
    const auto* payload = GetNativeProjectileData(projectile);
    if(NativeSummon269RejectCold(target))
    {
        // Money's source AddIceLevel/SetCold overrides are no-ops. Preserve
        // the projectile's ordinary damage multiplier without creating cold
        // state or accumulating a future frozen-damage bonus.
        const auto* profile=FindFusionColdProjectileProfile(payload->kind);
        if(!profile||damage<=0||damage>(std::numeric_limits<int>::max)()/profile->baseDamageMultiplier)return false;
        damage*=profile->baseDamageMultiplier;context.projectileKind=payload->kind;
        context.delivery=FusionColdDamageDelivery::ProjectileProfile;return true;
    }
    auto* data = GetOrCreateNativeZombieColdData(target);
    FusionColdState next;
    int resolved = 0;
    if (!data || !TryPrepareFusionColdHit(payload->kind,ReadState(target,*data),damage,next,resolved)) return false;
    Commit(target,*data,next);
    damage = resolved;
    context.projectileKind = payload->kind;
    context.delivery=FusionColdDamageDelivery::ProjectileProfile;
    if (payload->specialType==1 && !FindFusionColdProjectileProfile(payload->kind)->coldThroughShield)
        context.delivery=FusionColdDamageDelivery::IceShieldless; // Source way9 SnowPea/ExtremeSnowPea.
    LawnNet::NetTrace("Fusion native cold prepare projectile=%u kind=%u target=%u level=%d threshold=%d slow=%d freeze=%d damage=%d tick=%d",
        projectile->mNetId,payload->kind,target->mNetId,data->level,data->threshold,target->mChilledCounter,target->mIceTrapCounter,damage,projectile->mBoard->mMainCounter);
    return true;
}
void NativeColdDamageAfterShield(Zombie* target, const FusionColdDamageContext& context, bool shieldPassed)
{
    const bool applies=context.delivery == FusionColdDamageDelivery::ProjectileProfile ?
        FusionColdAppliesAfterShield(context.projectileKind,shieldPassed) :
        FusionColdDeliveryAppliesAfterShield(context.delivery,shieldPassed);
    if (!applies) return;
    ApplyNativeColdSlow(target);
}
void ApplyNativeColdSlow(Zombie* target)
{
    if (!HostTarget(target) || NativeSummon269RejectCold(target)) return;
    auto* data = GetOrCreateNativeZombieColdData(target);
    FusionColdState next;
    if (data && TrySetFusionColdSlow(ReadState(target,*data),next)) Commit(target,*data,next,1,kFusionColdSlowTicks);
}
bool ApplyNativeColdFreeze(Zombie* target, int ticks)
{
    if (!HostTarget(target) || NativeSummon269RejectCold(target)) return false;
    if(const auto* source=GetNativeSourceZombieData(target);source&&
       SpecialZombie269RejectEffect(source->kind,GetNativeFusionZombieStatus(target),SpecialZombie269StatusEffect::Freeze))return false;
    auto* data=GetOrCreateNativeZombieColdData(target);
    FusionColdState next;
    if (!data || !TrySetFusionColdFreeze(ReadState(target,*data),ticks,next)) return false;
    Commit(target,*data,next,2,ticks);
    return true;
}
bool UpdateNativeColdContact(Plant* plant)
{
    const auto* data=GetNativePlantData(plant);
    const auto* profile=data ? FindFusionColdContactProfile(data->kind) : nullptr;
    if (!profile) return false;
    if (!HostObject(plant) || plant->mDead) return true;
    float x=0,y=0;
    if (!plant->GetShadowCenter(x,y)) return true;
    const double width=profile->width*kFusionBasePixelsX,height=profile->height*kFusionBasePixelsY;
    const double left=x+profile->centerXFromShadow*kFusionBasePixelsX-width*.5;
    const double top=y-profile->centerYFromShadow*kFusionBasePixelsY-height*.5;
    Zombie* target=nullptr;
    while (plant->mBoard->IterateZombies(target))
    {
        if (!HostTarget(target) || !FusionColdContactCanTrigger(data->kind,GetNativeFusionZombieStatus(target),
            target->mRow==plant->mRow,target->mMindControlled,!target->mHasHead,target->mIceTrapCounter)) continue;
        const auto rect=target->GetZombieRect();
        if (rect.mX>left+width || rect.mX+rect.mWidth<left || rect.mY>top+height || rect.mY+rect.mHeight<top ||
            !ApplyNativeColdFreeze(target,profile->freezeTicks)) continue;
        plant->mApp->PlayFoley(FOLEY_FROZEN);
        plant->mApp->AddTodParticle(x,y,plant->mRenderOrder+1,PARTICLE_ICE_TRAP);
        LawnNet::NetBroadcastParticle(plant->mApp,x,y,plant->mRenderOrder+1,PARTICLE_ICE_TRAP);
        LawnNet::NetTrace("Fusion native contact freeze plant=%u kind=%u target=%u ticks=%d",plant->mNetId,data->kind,target->mNetId,profile->freezeTicks);
        plant->Die();
        break;
    }
    return true;
}
bool AddNativeColdLevel(Zombie* target, int amount)
{
    if (NativeSummon269RejectCold(target)) return false;
    // Continuation of an admitted collision, also after lethal damage. The
    // metadata factory checks the allocated full generation, not its old HP.
    if (!HostObject(target) || target->mApp->mBoard != target->mBoard ||
        (!IsNativeFusionGroundZombie(target)&&target->mZombieType!=ZOMBIE_BALLOON) || target->mMindControlled) return false;
    auto* data = GetOrCreateNativeZombieColdData(target,true);
    FusionColdState next;
    if (!data || !TryAddFusionColdLevels(ReadState(target,*data),amount,next)) return false;
    Commit(target,*data,next);
    LawnNet::NetTrace("Fusion native cold add target=%u amount=%d level=%d threshold=%d slow=%d freeze=%d tick=%d retiring=%d hp=%d",
        target->mNetId,amount,data->level,data->threshold,target->mChilledCounter,target->mIceTrapCounter,target->mBoard->mMainCounter,
        target->mDead || target->IsDeadOrDying() || target->mBodyHealth <= 0 ? 1 : 0,target->mBodyHealth);
    return true;
}
void NativeColdFreezeExpired(Zombie* target)
{
    if (!HostObject(target)) return;
    auto* data = GetNativeZombieColdData(target);
    FusionColdState next;
    if (!data || !TryEndFusionColdFreeze(ReadState(target,*data),next)) return;
    data->level = next.level;
    data->chilledTicks = target->mChilledCounter;
    data->frozenTicks = 0;
    LawnNet::NetTrace("Fusion native cold thaw target=%u threshold=%d slow=%d tick=%d",
        target->mNetId,data->threshold,target->mChilledCounter,target->mBoard->mMainCounter);
    LawnNet::NetBroadcastParticle(target->mApp,target->mPosX+75,target->mPosY+106,
        target->mRenderOrder+1,PARTICLE_ICE_TRAP_RELEASE);
}
bool HandleNativeColdPiercingCollision(Projectile* projectile)
{
    if (!Piercing(projectile)) return false;
    if (!HostObject(projectile) || projectile->mDead) return true;
    if (NativeTimedPeaProjectileExpired(projectile)) { projectile->Die(); return true; }
    const auto rect = projectile->GetProjectileRect();
    std::vector<ZombieID> targets;
    Zombie* target = nullptr;
    while (projectile->mBoard->IterateZombies(target))
        if (HostTarget(target) && NativeTimedPeaProjectileCanHit(projectile,target) && Overlaps(rect,target->GetZombieRect()))
            targets.push_back(projectile->mBoard->ZombieGetID(target));
    for (const auto id : targets)
    {
        if (projectile->mDead) break;
        if (auto* zombie = projectile->mBoard->ZombieTryToGet(id)) projectile->DoImpact(zombie);
    }
    return true;
}
bool ApplyNativeColdPiercingImpact(Projectile* projectile, Zombie* target)
{
    if (!Piercing(projectile)) return false;
    if (!HostObject(projectile) || projectile->mDead || !HostTarget(target) ||
        !NativeTimedPeaProjectileCanHit(projectile,target)) return true;
    auto* context = GetNativeContext(projectile->mBoard);
    const uint32_t projectileId = projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
    const uint32_t zombieId = static_cast<uint32_t>(projectile->mBoard->ZombieGetID(target));
    if (!context || !RememberFusionRollingHit(context->rollingHits,projectileId,zombieId)) return true;
    int damage = GetNativeProjectileData(projectile)->damage;
    FusionColdDamageContext cold;
    if (!PrepareNativeColdHit(projectile,target,damage,cold)) { projectile->Die(); return true; }
    projectile->PlayImpactSound(target);
    target->TakeDamage(damage,projectile->GetDamageFlags(target),&cold);
    float x = 0.0f, y = 0.0f;
    if (GetNativeTimedPeaProjectileCenter(projectile,x,y))
    {
        projectile->mApp->AddTodParticle(x,y,projectile->mRenderOrder+1,PARTICLE_SNOWPEA_SPLAT);
        LawnNet::NetBroadcastParticle(projectile->mApp,x,y,projectile->mRenderOrder+1,PARTICLE_SNOWPEA_SPLAT);
    }
    if (cold.projectileKind==24) IceDoomBurst(projectile);
    const auto count = FusionProjectileHitCount(context->rollingHits,projectileId);
    LawnNet::NetTrace("Fusion native cold pierce id=%u target=%u generation=%u visited=%zu damage=%d",
        projectile->mNetId,target->mNetId,zombieId,count,damage);
    if (count >= static_cast<std::size_t>(FindFusionColdProjectileProfile(cold.projectileKind)->maximumHits)) projectile->Die();
    return true;
}
bool ApplyNativeColdProjectileLand(Projectile* projectile)
{
    const auto* data=GetNativeProjectileData(projectile);
    if (!data || data->kind!=24 || !IsNativeColdProjectile(projectile)) return false;
    if (!HostObject(projectile) || projectile->mDead) return true;
    IceDoomBurst(projectile);
    projectile->PlayImpactSound(nullptr);
    projectile->Die();
    return true;
}
void RetireNativeColdProjectile(Projectile* projectile)
{
    if (!HostObject(projectile) || !IsNativeColdProjectile(projectile)) return;
    if (auto* context = GetNativeContext(projectile->mBoard))
        context->rollingHits.erase(projectile->mBoard->mProjectiles.DataArrayGetID(projectile));
}
}
