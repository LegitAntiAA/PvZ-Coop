#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionCherryRules.h"
#include "FusionPotatoRules.h"
#include "FusionNativeCherryShooter.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeRollingProjectile.h"
#include "FusionNativeProducer.h"
#include "FusionShooterTiming.h"
#include "FusionNativeCold.h"
#include "FusionNativeDeath.h"
#include "FusionNativeLobber.h"
#include "FusionNativeDefender.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeRecipientShooters.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeEconomy.h"
#include "FusionNativeAction266.h"
#include "FusionNativeAction267.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeWaterPlants267.h"
#include "FusionNativeSeaLantern267.h"
#include "FusionNativeCannon268.h"
#include "FusionNativeWater268.h"
#include "FusionNativeChomper268.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeLastPlants269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeMorePlants269.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeStar269.h"
#include "FusionNativeDoom269.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSolar269.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"

namespace LawnFusion
{
namespace
{
bool HasNativeContext(const GameObject* object)
{
    return object && object->mApp && IsNativeFusionBoard(object->mBoard) && GetNativeContext(object->mBoard);
}
}

void NativePlantInitialized(Plant* plant)
{
    if (!HasNativeContext(plant)) return;
    NativePlantData* data = GetNativePlantData(plant);
    if (!data) return;
    // Fresh authoritative source attributes. Ghost binding must retain the
    // received current HP; moving a real plant does not call this initializer.
    if (!plant->mNetGhost && !plant->mApp->NetIsClient())
    {
        const auto* definition = FusionFindPlant(data->kind);
        if (definition && definition->hasStats && definition->maxHealth > 0)
            plant->mPlantHealth = plant->mPlantMaxHealth = definition->maxHealth;
    }
    data->idleSeconds = 0.0f;
    data->shootSeconds = -1.0f;
    data->animationSpeed = 1.0f;
    if(data->kind==255){InitializeNativeRollingActor269(plant,255);return;}
    if(IsNativeLastPlants269(plant))
    {
        const bool emperor=data->kind==900;
        if(!InitializeNativeLastPlants269(plant)&&emperor&&!plant->mNetGhost&&!plant->mApp->NetIsClient())
        {
            LawnNet::NetTrace("Fusion Emperor269 initialization failed parent=%u tick=%d",plant->mNetId,plant->mBoard->mMainCounter);
            plant->Die(255);
        }
        return;
    }
    if(IsNativeWorldRemainder269Plant(plant)){InitializeNativeWorldRemainder269Plant(plant);return;}
    if(IsNativeMorePlants269(plant)){InitializeNativeMorePlants269(plant);return;}
    if(IsNativeRemainingLeaves269Plant(plant)){InitializeNativeRemainingLeaves269Plant(plant);return;}
    if(IsNativeRemaining269Plant(plant)){InitializeNativeRemaining269Plant(plant);return;}
    if(IsNativeSummonPlants269(plant))
    {InitializeNativeSummonPlants269(plant);if(data->kind==1166)GetNativeEquipmentPlantState(plant);return;}
    if(IsNativeCompanions269Plant(plant)){InitializeNativeCompanions269Plant(plant);return;}
    if(IsNativeWorld269Plant(plant)){InitializeNativeWorld269Plant(plant);return;}
    if (IsNativeStar269Plant(plant)) { InitializeNativeStar269Plant(plant); return; }
    if (IsNativeDoom269Plant(plant)) { InitializeNativeDoom269Plant(plant); return; }
    if (IsNativeHypno269Plant(plant)) { InitializeNativeHypno269Plant(plant); return; }
    if (IsNativeSolar269(plant)) { InitializeNativeLobber(plant); return; }
    if (InitializeNativeCannon268(plant) || InitializeNativeWater268Plant(plant) || InitializeNativeChomper268Plant(plant)) return;
    if (IsNativeEquipmentPlant(plant)) { InitializeNativeEquipmentPlant(plant); return; }
    InitializeNativeExtras268Plant(plant);
    if ((data->kind==1129 || data->kind==1162) && !plant->mNetGhost && !plant->mApp->NetIsClient())
        data->animationSpeed=RandRangeFloat(.9f,1.1f);
    if (IsNativeMachine268(plant)) return;
    if (InitializeNativeSeaLantern267Plant(plant) || InitializeNativeWater267(plant) ||
        InitializeNativeAction267(plant) || InitializeNativeUmbrellaBatch(plant) ||
        InitializeNativeEconomyPlant(plant) || InitializeNativeAction266(plant)) return;
    if (IsNativeFireAction(plant))
    {
        InitializeNativeFireAction(plant);
        return;
    }
    if (IsNativeShooterBatch(plant))
    {
        InitializeNativeShooterBatch(plant);
        return;
    }
    if (IsNativeRecipientShooter(plant))
    {
        InitializeNativeRecipientShooter(plant);
        return;
    }
    if (IsNativeDefender(plant))
    {
        InitializeNativeDefender(plant);
        return;
    }
    if (IsNativeBatchAction(plant))
    {
        InitializeNativeBatchAction(plant);
        return;
    }
    if (IsNativeLobber(plant))
    {
        InitializeNativeLobber(plant);
        return;
    }
    if (IsNativeIceShroom(plant))
    {
        InitializeNativeIceShroom(plant);
        return;
    }
    if (IsNativeChomper(plant) || IsNativeBaseMushroom(plant))
    {
        InitializeNativeBasePlant(plant);
        return;
    }
    if (IsNativeTimedPeaShooter(plant))
    {
        InitializeNativeCherryShooter(plant);
        return;
    }
    if (data->kind == 2)
    {
        plant->mPlantHealth = plant->mPlantMaxHealth = 300;
        if (plant->IsInPlay()) plant->mDoSpecialCountdown = kFusionCherryFuseTicks;
        return;
    }
    if (data->kind == 4 || data->kind == 1009)
    {
        plant->mPlantHealth = plant->mPlantMaxHealth = 300;
        if (plant->IsInPlay())
        {
            plant->mState = STATE_NOTREADY;
            TryGetFusionPotatoPreparationTicks(RandRangeFloat(14.0f, 16.0f), plant->mStateCountdown);
            LawnNet::NetTrace("Fusion native Potato prepared id=%u ticks=%d sourceSecondsMin=14 sourceSecondsMax=16 checkBeforeDecrement=1",
                plant->mNetId, plant->mStateCountdown);
        }
        if (data->kind == 1009) InitializeNativeProducer(plant);
        return;
    }
    if (data->kind == 1000 && !plant->mNetGhost && !plant->mApp->NetIsClient())
    {
        // Preserve the existing native shooter adaptation; the common producer
        // initializes its independent counter after this attack draw.
        plant->mPlantHealth = plant->mPlantMaxHealth = 300;
        plant->mLaunchRate = 500;
        plant->mLaunchCounter = RandRangeInt(100, 400);
    }
    if (IsNativeProducer(plant)) InitializeNativeProducer(plant);
}

void NativePlantHealthChanged(Plant* plant)
{
    NativeSummonPlants269HealthChanged(plant);
    NativeRemaining269HealthChanged(plant);
    NativeChomper268HealthChanged(plant);
    NativeExtras268HealthChanged(plant);
    NativeFireActionHealthChanged(plant);
    NativeGarlicHealthChanged(plant);
    NativeDefenderHealthChanged(plant);
    NativeChomperHealthChanged(plant);
    NativeProducerHealthChanged(plant);
    NativePlantHealthChangedForDeath(plant);
    NativeAction267HealthChanged(plant);
}

void UpdateNativePlant(Plant* plant)
{
    if (!HasNativeContext(plant) || plant->mDead || plant->mNetGhost) return;
    NativePlantData* data = GetNativePlantData(plant);
    if (!data) return;
    if(IsNativeRollingActor269(plant))return;
    if(IsNativeLastPlants269(plant)){UpdateNativeLastPlants269Phase(plant);return;}
    if(IsNativeWorldRemainder269Plant(plant)){UpdateNativeWorldRemainder269PlantPhase(plant);return;}
    if(IsNativeMorePlants269(plant)){UpdateNativeMorePlants269Phase(plant);return;}
    if(IsNativeRemainingLeaves269Plant(plant)){UpdateNativeRemainingLeaves269Phase(plant);return;}
    if(IsNativeRemaining269Plant(plant)){UpdateNativeRemaining269Phase(plant);return;}
    if(IsNativeSummonPlants269(plant)){NativeDefenderHealthChanged(plant);UpdateNativeSummonPlants269Phase(plant);return;}
    if(IsNativeCompanions269Plant(plant)){NativeDefenderHealthChanged(plant);UpdateNativeCompanions269PlantPhase(plant);return;}
    if(IsNativeWorld269Plant(plant)){UpdateNativeWorld269PlantPhase(plant);return;}
    if (IsNativeStar269Plant(plant))
    {
        NativeDefenderHealthChanged(plant); // Includes the real StarPumpkin damage stages.
        UpdateNativeStar269PlantPhase(plant);
        if(data->kind==1104)data->pendingProductionTicks=plant->mDoSpecialCountdown;
        return;
    }
    if (IsNativeDoom269Plant(plant)) { UpdateNativeDoom269Phase(plant); return; }
    if (IsNativeHypno269Plant(plant)) { UpdateNativeHypno269Phase(plant); return; }
    if (IsNativeSolar269(plant)) { UpdateNativeLobberPhase(plant); return; }
    if (IsNativeCannon268(plant)) { UpdateNativeCannon268Phase(plant); return; }
    if (UpdateNativeWater268Phase(plant) || UpdateNativeChomper268Phase(plant)) return;
    if (IsNativeEquipmentPlant(plant)) { UpdateNativeEquipmentPlantPhase(plant); return; }
    NativeDefenderHealthChanged(plant);
    if (UpdateNativeSeaLantern267Phase(plant) || UpdateNativeWater267Phase(plant) ||
        UpdateNativeAction267Phase(plant) || UpdateNativeUmbrellaBatchPhase(plant) ||
        UpdateNativeEconomyPhase(plant) || UpdateNativeAction266Phase(plant)) return;
    if (IsNativeFireAction(plant))
    {
        if (IsNativeBatchAction(plant)) UpdateNativeBatchActionPhase(plant);
        UpdateNativeFireActionPhase(plant);
        return;
    }
    if (IsNativeShooterBatch(plant))
    {
        UpdateNativeShooterBatchPhase(plant);
        return;
    }
    if (IsNativeRecipientShooter(plant))
    {
        UpdateNativeRecipientShooterPhase(plant);
        return;
    }
    if (IsNativeBatchAction(plant))
    {
        UpdateNativeBatchActionPhase(plant);
        return;
    }
    if (IsNativeLobber(plant))
    {
        UpdateNativeLobberPhase(plant);
        UpdateNativeCannon268Money(plant);
        return;
    }
    if (IsNativeProducer(plant) && plant->mApp->NetIsClient()) return;
    if (IsNativeChomper(plant) || IsNativeBaseMushroom(plant))
    {
        UpdateNativeBasePlantPhase(plant);
        return;
    }
    if (IsNativeTimedPeaShooter(plant))
    {
        UpdateNativeCherryShooterPhase(plant);
        return;
    }
    if (IsNativeProducer(plant) && data->kind != 1000)
    {
        data->idleSeconds += 0.01f;
        return;
    }
    if (data->kind != 1000)
    {
        data->idleSeconds += 0.01f;
        return;
    }
    data->idleSeconds += 0.01f;
    if (data->shootSeconds < 0.0f)
    {
        if (plant->mShootingCounter > 0) data->shootSeconds = 0.0f;
    }
    else
    {
        data->shootSeconds += 0.01f;
        if (data->shootSeconds >= 0.5f) data->shootSeconds = -1.0f;
    }
}

void NativeProjectileCreated(Plant* source, Projectile* projectile, const FusionSplitEmissionSpec* emission)
{
    if (!HasNativeContext(source) || !projectile || source->mNetGhost || projectile->mNetGhost ||
        projectile->mBoard != source->mBoard) return;
    const NativePlantData* plant = GetNativePlantData(source);
    if(plant&&plant->kind==0&&projectile->mProjectileType==PROJECTILE_PEA&&
        projectile->mMotionType==MOTION_STRAIGHT&&!GetNativeProjectileData(projectile))
    {
        // Bind the existing native body at its factory boundary so all Fusion
        // recipients can observe an ordinary pea. Keep PvZ's effective speed.
        projectile->mVelX=3.33f;
        if(!EnsureNativeProjectileVisuals(source->mBoard,0)||
            !AttachNativeProjectile(source->mBoard,projectile,NativeProjectileData{}))projectile->Die();
        return;
    }
    if (plant && (plant->kind == 6 || plant->kind == 19))
    {
        CreateNativePuffProjectile(source, projectile);
        return;
    }
    if (plant && IsNativeTimedPeaShooter(source))
    {
        const auto* profile = FindFusionShooterTimingProfile(plant->kind);
        if (profile && profile->projectileKind == 2) CreateNativeRollingProjectile(source, projectile);
        else CreateNativeTimedPeaProjectile(source, projectile, emission);
        return;
    }
    if (!plant || plant->kind != 1000) return;
    NativeProjectileData data;
    data.kind = 8;
    data.damage = 100;
    data.sunOnHit = 5;
    if (!AttachNativeProjectile(source->mBoard, projectile, data)) projectile->Die();
}

int GetNativeProjectileDamage(const Projectile* projectile, int fallback)
{
    if (!HasNativeContext(projectile)) return fallback;
    const NativeProjectileData* data = GetNativeProjectileData(projectile);
    return data ? data->damage : fallback;
}

void NativeProjectileHit(Projectile* projectile, bool hitZombie)
{
    if (!HasNativeContext(projectile) || !hitZombie || projectile->mDead || projectile->mNetGhost) return;
    NativeProjectileData* data = GetNativeProjectileData(projectile);
    if (!data || data->sunOnHit <= 0) return;
    const int value = data->sunOnHit;
    // Consume the impact award before spawning, so an impact animation or a
    // repeated collision callback cannot create a second five-sun item.
    data->sunOnHit = 0;
    if (projectile->mBoard->mCoins.mSize >= projectile->mBoard->mCoins.mMaxSize) return;
    projectile->mBoard->AddCoin(static_cast<int>(projectile->mPosX),
        static_cast<int>(projectile->mPosY + projectile->mPosZ), value == 25 ? CoinType::COIN_SUN : CoinType::COIN_SMALLSUN,
        CoinMotion::COIN_MOTION_FROM_PLANT, SeedType::SEED_NONE, value);
}

bool ApplyNativeFusionCherryDamage(Zombie* zombie)
{
    if (!HasNativeContext(zombie) || zombie->mApp->NetIsClient() || zombie->mNetGhost ||
        zombie->mDead || zombie->IsDeadOrDying() || zombie->mMindControlled ||
        !IsNativeFusionGroundZombie(zombie)) return false;
    FusionCherryDamage result;
    if (!TryResolveFusionCherryDamage(kFusionSourceNormalDifficulty,
        static_cast<float>(zombie->mBodyHealth), zombie->mHelmHealth, result)) return false;
    const int body = zombie->mBodyHealth, first = zombie->mHelmHealth, second = zombie->mShieldHealth;
    if (result.instantBurn) zombie->ApplyBurn(true);
    else zombie->TakeDamage(result.damage, 18U);
    LawnNet::NetTrace("Fusion native Cherry impact id=%u body=%d first=%d second=%d afterBody=%d afterFirst=%d afterSecond=%d instantBurn=%d dead=%d sourceDifficulty=%d",
        zombie->mNetId, body, first, second, zombie->mBodyHealth, zombie->mHelmHealth, zombie->mShieldHealth,
        result.instantBurn ? 1 : 0, zombie->mDead ? 1 : 0, kFusionSourceNormalDifficulty);
    return true;
}

bool IsNativeFusionPotato(const Plant* plant)
{
    if (!HasNativeContext(plant)) return false;
    const auto* data = GetNativePlantData(plant);
    return data && (data->kind == 4 || data->kind == 1009 || data->kind == 1007);
}

bool NativeFusionPotatoHasTrigger(Plant* plant)
{
    if (!IsNativeFusionPotato(plant) || plant->mNetGhost || plant->mApp->NetIsClient() || plant->mDead) return false;
    FusionPotatoArea area;
    if (!TryMakeFusionPotatoArea(plant->mBoard->GetCellCenterX(plant->mPlantCol, plant->mRow),
        plant->mBoard->GridToPixelY(plant->mPlantCol, plant->mRow) + 50, area)) return false;
    Zombie* zombie = nullptr;
    while (plant->mBoard->IterateZombies(zombie))
    {
        // Only the existing native basic family is ported. Source status rules
        // for future families are independently preserved by the pure policy.
        if (!IsNativeFusionGroundZombie(zombie) || zombie->mDead) continue;
        const int status = zombie->IsDeadOrDying() ? 1 : 0;
        if (!FusionPotatoCanTrigger(status, zombie->mRow == plant->mRow, zombie->mMindControlled,
            zombie->IsDeadOrDying(), plant->mState == STATE_POTATO_ARMED, plant->mState == STATE_DOINGSPECIAL)) continue;
        const auto rect = zombie->GetZombieRect();
        if (FusionPotatoTriggerOverlaps(area, rect.mX, rect.mY, rect.mWidth, rect.mHeight)) return true;
    }
    return false;
}

bool ApplyNativeFusionPotatoDamage(Zombie* zombie, int damage)
{
    if (damage != 300 && damage != 500 && damage != 1800) return false;
    return ApplyNativeSourceExplosionDamage(zombie,damage);
}

bool ApplyNativeSourceExplosionDamage(Zombie* zombie,int damage)
{
    if (damage < 0 || !HasNativeContext(zombie) || zombie->mApp->NetIsClient() || zombie->mNetGhost ||
        zombie->mDead || zombie->mMindControlled || !IsNativeFusionGroundZombie(zombie)) return false;
    const int body = zombie->mBodyHealth, first = zombie->mHelmHealth, second = zombie->mShieldHealth;
    // Source Explode10 damages the shield independently, then helmet/body.
    // Source normal2 does not multiply1800; it never uses Cherry's instant burn.
    zombie->TakeDamage(damage, 18U);
    LawnNet::NetTrace("Fusion native Explosion impact id=%u body=%d first=%d second=%d afterBody=%d afterFirst=%d afterSecond=%d dead=%d damage=%d sourceDifficulty=%d",
        zombie->mNetId, body, first, second, zombie->mBodyHealth, zombie->mHelmHealth, zombie->mShieldHealth,
        zombie->IsDeadOrDying() ? 1 : 0, damage, kFusionSourceNormalDifficulty);
    return true;
}
}
