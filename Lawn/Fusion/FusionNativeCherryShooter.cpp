#include "FusionNativeCherryShooter.h"
#include "FusionCherryShooterRules.h"
#include "FusionStraightProjectileRules.h"
#include "FusionSplitShooterRules.h"
#include "FusionColdRules.h"
#include "FusionFireRules.h"
#include "FusionNative.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeShooterTiming.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
bool HostPlant(Plant* plant)
{
    return plant && plant->mApp && !plant->mApp->NetIsClient() && !plant->mNetGhost &&
        !plant->mDead && IsNativeTimedPeaShooter(plant);
}

double SourceProjectileX(const Projectile* projectile, float centerX)
{
    return (centerX-projectile->mBoard->GetCellCenterX(0,0))/(72.0/1.35)-4.8;
}
}

bool IsNativeCherryShooter(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && data->kind == 1001;
}

bool IsNativeCherryProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    return data && data->kind == 1 && data->damage == kFusionCherryShooterDamage &&
        data->sunOnHit == 0 && projectile->mProjectileType == PROJECTILE_PEA;
}

bool IsNativeTimedPeaShooter(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    const auto* profile = data ? FindFusionShooterTimingProfile(data->kind) : nullptr;
    return profile && (profile->projectileKind == 2 || FindFusionStraightProjectileProfile(profile->projectileKind)) &&
        GetNativeSeedForPlant(data->kind) == SEED_PEASHOOTER;
}

bool IsNativeTimedPeaProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    return data && FindFusionStraightProjectileProfile(data->kind) &&
        (IsNativeMore269Projectile(projectile)||FusionFireProjectileMetadataValid(data->kind,data->damage,data->sunOnHit,data->fireState,
            data->lastTorchId,data->lastTorchToken,projectile->mProjectileType,data->kind == 0 && data->effectVariant == 1)) &&
        FusionSplitProjectilePhaseValid(data->kind,data->specialType,projectile->mMotionType);
}

bool IsNativeSplitShooter(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && IsNativeTimedPeaShooter(plant) && !GetFusionSplitEmissionSpecs(data->kind).empty();
}

Zombie* FindNativeSplitShooterTarget(Plant* plant, int row)
{
    if (!HostPlant(plant) || !IsNativeSplitShooter(plant) || row != plant->mRow) return nullptr;
    const double vision = 1.35*(plant->mBoard->GetColumnCount()-1)-4.8+1.85;
    Zombie* zombie = nullptr;
    while (plant->mBoard->IterateZombies(zombie))
    {
        if (!IsNativeFusionGroundZombie(zombie) || zombie->mDead || zombie->IsDeadOrDying() || zombie->IsFlying()) continue;
        float x = 0.0f, y = 0.0f;
        if (!zombie->GetShadowCenter(x,y)) continue;
        const double sourceX = (x-plant->mBoard->GetCellCenterX(0,0))/(72.0/1.35)-4.8;
        if (FusionSplitShooterCanTarget(GetNativeFusionZombieStatus(zombie), zombie->mRow == row, zombie->mMindControlled,
            static_cast<float>(zombie->mBodyHealth), sourceX, vision)) return zombie;
    }
    return nullptr;
}

void InitializeNativeCherryShooter(Plant* plant)
{
    if (!HostPlant(plant)) return;
    auto* data = GetNativePlantData(plant);
    const auto* definition = FusionFindPlant(data->kind);
    if (!definition || !definition->hasStats || definition->maxHealth <= 0) return;
    plant->mPlantHealth = plant->mPlantMaxHealth = definition->maxHealth;
    if (!InitializeNativeShooterTiming(plant, *FindFusionShooterTimingProfile(data->kind), false)) return;
    LawnNet::NetTrace("Fusion native CherryShooter init id=%u health=%d launch=%d animationSpeed=%.9g",
        plant->mNetId, plant->mPlantMaxHealth, plant->mLaunchCounter, data->animationSpeed);
}

bool ResetNativeCherryLaunchCounter(Plant* plant)
{
    if (!HostPlant(plant)) return false;
    return ResetNativeShooterLaunchCounter(plant, *FindFusionShooterTimingProfile(GetNativePlantData(plant)->kind));
}

void BeginNativeCherryShot(Plant* plant)
{
    if (!HostPlant(plant)) return;
    auto* data = GetNativePlantData(plant);
    if (!BeginNativeShooterShot(plant, *FindFusionShooterTimingProfile(data->kind))) return;
    LawnNet::NetTrace("Fusion native CherryShooter trigger id=%u shootCounter=%d animationSpeed=%.9g",
        plant->mNetId, plant->mShootingCounter, data->animationSpeed);
}

void UpdateNativeCherryShooterPhase(Plant* plant)
{
    if (!HostPlant(plant)) return;
    UpdateNativeShooterPhase(plant, *FindFusionShooterTimingProfile(GetNativePlantData(plant)->kind));
}

bool GetNativeCherryShooterGroundAnchor(Plant* plant, float& x, float& y)
{
    return plant && plant->mBoard && IsNativeTimedPeaShooter(plant) && plant->GetShadowCenter(x, y);
}

bool GetNativeCherryProjectileCenter(const Projectile* projectile, float& x, float& y)
{
    if (!IsNativeCherryProjectile(projectile)) return false;
    return GetNativeTimedPeaProjectileCenter(projectile, x, y);
}

bool GetNativeTimedPeaProjectileCenter(const Projectile* projectile, float& x, float& y)
{
    if (!IsNativeTimedPeaProjectile(projectile)) return false;
    x = projectile->mPosX+kFusionCherryProjectileCenterOffset;
    y = projectile->mPosY+projectile->mPosZ+kFusionCherryProjectileCenterOffset;
    return std::isfinite(x) && std::isfinite(y);
}

void CreateNativeCherryProjectile(Plant* source, Projectile* projectile)
{
    if (!source || !GetNativePlantData(source)) return;
    const auto* profile = FindFusionShooterTimingProfile(GetNativePlantData(source)->kind);
    if (profile && profile->projectileKind == 1) CreateNativeTimedPeaProjectile(source, projectile);
}

void CreateNativeTimedPeaProjectile(Plant* source, Projectile* projectile, const FusionSplitEmissionSpec* emission)
{
    if (!HostPlant(source) || !projectile || projectile->mNetGhost || projectile->mBoard != source->mBoard) return;
    const uint16_t plantKind = GetNativePlantData(source)->kind;
    const auto* profile = FindFusionShooterTimingProfile(plantKind);
    const bool split = !GetFusionSplitEmissionSpecs(plantKind).empty();
    if ((split && (!emission || !FusionSplitEmissionValid(plantKind,*emission))) || (!split && emission))
    { projectile->Die(); return; }
    const uint16_t payloadKind = emission ? emission->projectileKind : profile->projectileKind;
    const auto* payload = FindFusionStraightProjectileProfile(payloadKind);
    if (!payload) return;
    const auto* definition = FusionFindPlant(GetNativePlantData(source)->kind);
    if (!definition || !definition->hasStats || definition->damage != payload->damage)
    { projectile->Die(); return; }
    NativeProjectileData data;
    data.kind = payloadKind;
    data.damage = payload->damage;
    data.sunOnHit = 0;
    data.specialType = emission ? emission->specialType : 0;
    float groundX = 0.0f, groundY = 0.0f;
    projectile->mMotionType = data.specialType == 1 ? MOTION_BACKWARDS : MOTION_STRAIGHT;
    if (!GetNativeCherryShooterGroundAnchor(source, groundX, groundY) ||
        !AttachNativeProjectile(source->mBoard, projectile, data))
    {
        projectile->Die();
        return;
    }
    // Nominal source clip-key muzzle aligned to the real native ground anchor.
    // Additive/blended Unity live-transform parity is not implied by this art
    // adaptation.60 is art scale; physical velocity uses72/1.35.
    float muzzleX = .514f, muzzleY = .6003f;
    if (emission) { muzzleX = emission->muzzleX; muzzleY = emission->muzzleY; }
    else if (FindFusionColdProjectileProfile(data.kind) && !TryGetFusionColdMuzzle(plantKind,muzzleX,muzzleY))
    { projectile->Die(); return; }
    else if (data.kind == 10 && !TryGetFusionPuffPeaMuzzle(plantKind, muzzleX, muzzleY))
    { projectile->Die(); return; }
    const float centerX = groundX+muzzleX*60.0f;
    const float centerY = groundY-muzzleY*60.0f;
    projectile->mPosX = centerX-kFusionCherryProjectileCenterOffset;
    projectile->mPosY = centerY-kFusionCherryProjectileCenterOffset;
    projectile->mPosZ = 0.0f;
    projectile->mX = static_cast<int>(projectile->mPosX);
    projectile->mY = static_cast<int>(projectile->mPosY);
    projectile->mShadowY = groundY;
    projectile->mVelX = data.specialType == 1 ? -payload->pixelsPerTick : payload->pixelsPerTick;
    projectile->mVelY = projectile->mVelZ = projectile->mAccZ = 0.0f;
    LawnNet::NetTrace("Fusion native CherryShooter projectile plant=%u id=%u kind=%u damage=%d sun=0 center=%.3f,%.3f vx=%.3f shootCounter=%d special=%u event=%d",
        source->mNetId, projectile->mNetId, data.kind, data.damage, centerX, centerY, projectile->mVelX, source->mShootingCounter,
        data.specialType, emission ? static_cast<int>(emission->eventIndex) : -1);
}

bool UpdateNativeTimedPeaProjectileMotion(Projectile* projectile)
{
    if (!IsNativeTimedPeaProjectile(projectile)) return false;
    if (projectile->mNetGhost || !projectile->mApp || projectile->mApp->NetIsClient()) return true;
    auto* data = GetNativeProjectileData(projectile);
    const auto* profile = FindFusionStraightProjectileProfile(data->kind);
    if (data->fireState && data->specialType == 0)
    {
        // Heating keeps the existing trajectory, including the native base Pea.
        projectile->mPosX += projectile->mVelX;
        return true;
    }
    // Match source FixedUpdate order at native100Hz: old way sets velocity,
    // current position may turn the phase, then that old velocity integrates.
    // No clamp, age reset or immediate positive-velocity refresh on the turn.
    projectile->mVelX = projectile->mMotionType == MOTION_BACKWARDS ? -profile->pixelsPerTick : profile->pixelsPerTick;
    float x = 0.0f, y = 0.0f;
    uint8_t next = data->specialType;
    if (!GetNativeTimedPeaProjectileCenter(projectile,x,y) ||
        !TryAdvanceFusionSplitProjectilePhase(SourceProjectileX(projectile,x),-10.0,data->specialType,next))
    { projectile->Die(); return true; }
    if (next != data->specialType)
    {
        data->specialType = next;
        projectile->mMotionType = MOTION_STRAIGHT;
        data->lastTorchId = 0;
        data->lastTorchToken = 0;
        projectile->SyncFireballPresentationDirection();
        LawnNet::NetTrace("Fusion native Split projectile turn id=%u kind=%u x=%.3f age=%d vx=%.3f special=2",
            projectile->mNetId,data->kind,x,projectile->mProjectileAge,projectile->mVelX);
    }
    projectile->mPosX += projectile->mVelX;
    return true;
}

bool TryGetNativeTimedPeaDamageFlags(const Projectile* projectile, unsigned int& flags)
{
    const auto* data = GetNativeProjectileData(projectile);
    if (data && data->fireState && IsNativeTimedPeaProjectile(projectile))
    {
        flags = 0U; // FireZombie uses Normal even for a rear Split shot.
        return true;
    }
    return data && IsNativeTimedPeaProjectile(projectile) &&
        TryGetFusionSplitProjectileDamageFlags(data->kind,data->specialType,flags);
}

bool GetNativeCherryProjectileRect(const Projectile* projectile, Sexy::Rect& rect)
{
    if (!IsNativeCherryProjectile(projectile)) return false;
    return GetNativeTimedPeaProjectileRect(projectile, rect);
}

bool GetNativeTimedPeaProjectileRect(const Projectile* projectile, Sexy::Rect& rect)
{
    if (!IsNativeTimedPeaProjectile(projectile)) return false;
    const auto* data = GetNativeProjectileData(projectile);
    if (data->kind == 0 && data->fireState)
    {
        // The flame attachment changes appearance, not the original Pea collider.
        rect = Sexy::Rect(projectile->mX-15,projectile->mY,projectile->mWidth+15,projectile->mHeight);
        return true;
    }
    const auto* profile = FindFusionStraightProjectileProfile(data->kind);
    if (!profile->sourceCollider) return false;
    float x = 0.0f, y = 0.0f;
    FusionStraightProjectileRect bounds;
    if (!GetNativeTimedPeaProjectileCenter(projectile, x, y) || !TryMakeFusionStraightProjectileRect(*profile, x, y, bounds))
    {
        rect = Sexy::Rect(0,0,0,0);
        return true;
    }
    const int left = static_cast<int>(std::floor(bounds.x)), top = static_cast<int>(std::floor(bounds.y));
    rect = Sexy::Rect(left, top, static_cast<int>(std::ceil(bounds.x+bounds.width))-left,
        static_cast<int>(std::ceil(bounds.y+bounds.height))-top);
    return true;
}

bool NativeCherryProjectileCanHit(const Projectile* projectile, Zombie* zombie)
{
    return IsNativeCherryProjectile(projectile) && NativeTimedPeaProjectileCanHit(projectile, zombie);
}

bool NativeTimedPeaProjectileCanHit(const Projectile* projectile, Zombie* zombie)
{
    if (!IsNativeTimedPeaProjectile(projectile) || projectile->mNetGhost || !projectile->mApp || projectile->mApp->NetIsClient() ||
        !zombie || zombie->mDead || zombie->IsDeadOrDying() || zombie->IsFlying() || !IsNativeFusionGroundZombie(zombie)) return false;
    // Supported source ground families. Native dead phases are rejected independently;
    // native IsDeadOrDying is not a conversion to the source status enum.
    return FusionStraightProjectileCanHit(GetNativeFusionZombieStatus(zombie),
        zombie->mRow == projectile->mRow, zombie->mMindControlled, GetNativeFusionZombieHealth(zombie));
}

bool NativeCherryProjectileExpired(const Projectile* projectile)
{
    return IsNativeCherryProjectile(projectile) && NativeTimedPeaProjectileExpired(projectile);
}

bool NativeTimedPeaProjectileExpired(const Projectile* projectile)
{
    if (!IsNativeTimedPeaProjectile(projectile) || projectile->mNetGhost || !projectile->mBoard ||
        !projectile->mApp || projectile->mApp->NetIsClient()) return false;
    float x = 0.0f, y = 0.0f;
    if (!GetNativeTimedPeaProjectileCenter(projectile, x, y)) return true;
    Board* board = projectile->mBoard;
    const double sourceX = SourceProjectileX(projectile,x);
    const double sourceY = 2.3-(y-(projectile->mBoard->GridToPixelY(0,0)+50))/(100.0/1.67);
    // Source Board.Awake/SetBoundaries0x180312620/0x18031C5A0; NativeFusion
    // descriptor fixes10x5. Original bullets add+1 in Awake and+1 in MoveDie.
    const double maxX = 1.35*(board->GetColumnCount()-1)-4.8+1.85;
    const double minY = 2.3-1.67*(kNativeFusionRows-1)-0.6;
    return FusionStraightProjectileExpired(*FindFusionStraightProjectileProfile(GetNativeProjectileData(projectile)->kind),
        sourceX, sourceY, projectile->mProjectileAge, -10.0, maxX, minY, 6.5, GetNativeProjectileData(projectile)->specialType);
}
}
