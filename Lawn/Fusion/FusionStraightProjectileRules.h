#ifndef LAWN_FUSION_STRAIGHT_PROJECTILE_RULES_H
#define LAWN_FUSION_STRAIGHT_PROJECTILE_RULES_H

#include "../../ConstEnums.h"
#include <cstdint>

namespace LawnFusion
{
struct FusionStraightProjectileProfile
{
    uint16_t kind;
    ProjectileType nativeType;
    int damage;
    float pixelsPerTick;
    int maximumAgeTicks;
    bool sourceCollider;
    double width, height, offsetX, offsetY;
    bool supportsSplitPhases = false;
    unsigned int nativeDamageFlags = 0;
};

struct FusionStraightProjectileRect
{
    double x = 0, y = 0, width = 0, height = 0;
};

const FusionStraightProjectileProfile* FindFusionStraightProjectileProfile(uint16_t kind);
bool FusionStraightProjectileMetadataValid(uint16_t kind, ProjectileType nativeType, int damage, int sunOnHit);
// Kind0 retains the existing native pea rectangle and returns false here.
bool TryMakeFusionStraightProjectileRect(const FusionStraightProjectileProfile& profile,
    double centerX, double centerY, FusionStraightProjectileRect& result);
bool FusionStraightProjectileCanHit(int sourceStatus, bool sameRow, bool mindControlled, float bodyHealth);
bool FusionStraightProjectileExpired(const FusionStraightProjectileProfile& profile,
    double sourceX, double sourceY, int ageTicks,
    double boardMinX, double boardMaxX, double boardMinY, double boardMaxY, uint8_t specialType = 0);
// Nominal source Shoot-Shadow offsets, including AnimShoot's +.1 source X.
// Existing Pea/Cherry artwork keeps its current native muzzle adaptation.
bool TryGetFusionPuffPeaMuzzle(uint16_t plantKind, float& sourceX, float& sourceY);
}

#endif
