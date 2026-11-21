#ifndef LAWN_FUSION_RECIPIENT_SHOOTER_RULES_H
#define LAWN_FUSION_RECIPIENT_SHOOTER_RULES_H
#include "FusionShooterTiming.h"
#include "../../ConstEnums.h"
#include <cstddef>

namespace LawnFusion
{
struct FusionRecipientEmissionSpec
{
    uint16_t projectileKind = 0;
    int sourceMovingWay = 0;
    double muzzleX = 0, muzzleY = 0; // Source event-time Shoot minus Shadow.
    double velocityX = 0, velocityY = 0; // Native pixels per100Hz tick.
};
struct FusionRecipientRect { double x=0, y=0, width=0, height=0; };

// Ground21 and23 are the normal-NPC slice. The source21 upper timeline is
// retained for audit, not an assertion that source airborne NPC10 is ported.
const FusionShooterTimingProfile* FindFusionRecipientTimingProfile(uint16_t kind, bool airborne=false);
const FusionShooterTimingProfile* FindFusionRecipientTimingProfile(uint16_t kind, const char* state);
bool TryGetFusionCactusCycleTicks(float additiveJitter, int& ticks);
std::size_t FusionRecipientEmissionCount(uint16_t plantKind);
bool TryGetFusionRecipientEmission(uint16_t plantKind, std::size_t index, FusionRecipientEmissionSpec& result);
bool FusionCactusCanAcquire(int sourceStatus, bool sameRow, bool controlled, bool dying,
    double plantX, double targetX, double visionX);
// Source ray order:(2,1),(2,-1),(0,1),(0,-1),(-1,0). Only the left ray
// additionally requires the original source row. Inputs use board pixels.
bool TryGetFusionStarAcquisitionDistance(std::size_t ray, bool sameRow,
    double originX, double originY, const FusionRecipientRect& target, double& distance);
bool FusionRecipientProjectileMetadataValid(uint16_t kind, int damage, int sun, uint8_t sourceMovingWay,
    ProjectileType type, ProjectileMotion motion);
bool FusionRecipientProjectileVelocityValid(uint16_t kind, uint8_t sourceMovingWay, double velocityX, double velocityY);
bool FusionRecipientProjectileCanHit(uint16_t kind, uint8_t sourceMovingWay, int sourceStatus, bool sameRow,
    bool controlled, bool dying, double bodyHealth);
unsigned int FusionRecipientDamageFlags(uint16_t kind, uint8_t sourceMovingWay);
bool TryMakeFusionRecipientProjectileRect(uint16_t kind, double centerX, double centerY,
    double velocityX, double velocityY, FusionRecipientRect& result);
bool FusionRecipientProjectileTouchesRect(uint16_t kind, double centerX, double centerY,
    double velocityX, double velocityY, const FusionRecipientRect& target);
}
#endif
