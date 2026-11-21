#include "FusionStraightProjectileRules.h"
#include "FusionCherryShooterRules.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr double pixelsX = 72.0/1.35, pixelsY = 100.0/1.67;
constexpr FusionStraightProjectileProfile profiles[] = {
    {0, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, false, 0, 0, 0, 0, true, 0},
    // Preserve the exact old Cherry expressions and double arithmetic.
    {1, PROJECTILE_PEA, 60, kFusionCherryProjectilePixelsPerTick, 2000, true,
        2.13*0.2*pixelsX, 1.9312102*0.2*pixelsY, 0, 0.025604904*0.2*pixelsY, true, 0},
    {10, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        0.24274449*pixelsX, 0.35107538*pixelsY,
        0.0007451102*pixelsX, 0.051929936*pixelsY, true, 0},
    {15, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        .32*1.5062028*pixelsX, .43096197*1.5062028*pixelsY,
        0, .08048099*1.5062028*pixelsY, true, 0},
    {21, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        .32*pixelsX, .43096197*pixelsY, 0, .08048099*pixelsY, false, 0},
    {22, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        .32*pixelsX, .43096197*pixelsY, 0, .08048099*pixelsY, false, 0},
    {24, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        3.68*.2*pixelsX, 1.7*.2*pixelsY, 0, 0, false, 0},
    {97, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        .48764962*1.7*pixelsX, .19262436*1.7*pixelsY,
        -.0044807494*1.7*pixelsX, -.007438913*1.7*pixelsY, true, 0},
    {18, PROJECTILE_PEA, 20, kFusionCherryProjectilePixelsPerTick, 2000, true,
        3.5532758*.10968053*pixelsX, 1.4299067*.10968053*pixelsY,
        -.036273837*.10968053*pixelsX, .07308346*.10968053*pixelsY, false, 2}
};
}

const FusionStraightProjectileProfile* FindFusionStraightProjectileProfile(uint16_t kind)
{
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    return nullptr;
}

bool FusionStraightProjectileMetadataValid(uint16_t kind, ProjectileType nativeType, int damage, int sunOnHit)
{
    const auto* profile = FindFusionStraightProjectileProfile(kind);
    return profile && profile->nativeType == nativeType && profile->damage == damage && sunOnHit == 0;
}

bool TryMakeFusionStraightProjectileRect(const FusionStraightProjectileProfile& profile,
    double centerX, double centerY, FusionStraightProjectileRect& result)
{
    if (!profile.sourceCollider || !std::isfinite(centerX) || !std::isfinite(centerY)) return false;
    result = {centerX+profile.offsetX-profile.width/2, centerY+profile.offsetY-profile.height/2,
        profile.width, profile.height};
    return true;
}

bool FusionStraightProjectileCanHit(int sourceStatus, bool sameRow, bool mindControlled, float bodyHealth)
{
    // Existing Pea/Cherry and fresh PuffPea share Bullet.CheckZombie. Keep
    // native dead phases separate from this source-status predicate.
    return FusionCherryProjectileCanHit(sourceStatus, sameRow, mindControlled, bodyHealth);
}

bool FusionStraightProjectileExpired(const FusionStraightProjectileProfile& profile,
    double x, double y, int ageTicks, double minX, double maxX, double minY, double maxY, uint8_t specialType)
{
    if (!std::isfinite(x+y+minX+maxX+minY+maxY) || minX >= maxX || minY >= maxY || ageTicks < 0 ||
        specialType > 2 || (specialType && !profile.supportsSplitPhases)) return true;
    // Source Bullet.maxX=Board.maxX+1. Ordinary MoveDie permits one more unit;
    // outbound/returned Split phases do not. Returned shots have no lower-X death.
    return ageTicks > profile.maximumAgeTicks || (specialType == 0 && x < minX-2.0) ||
        x > maxX+(specialType == 0 ? 2.0 : 1.0) || y < minY || y > maxY;
}

bool TryGetFusionPuffPeaMuzzle(uint16_t plantKind, float& sourceX, float& sourceY)
{
    struct Muzzle { uint16_t kind; float x, y; };
    constexpr Muzzle muzzles[] = {{1018,.2695f,.084f},{1019,.27025f,.084f},{1065,.28802f,.09911f}};
    for (const auto& muzzle : muzzles)
        if (muzzle.kind == plantKind) { sourceX = muzzle.x; sourceY = muzzle.y; return true; }
    return false;
}
}
