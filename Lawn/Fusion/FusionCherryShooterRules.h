#ifndef LAWN_FUSION_CHERRY_SHOOTER_RULES_H
#define LAWN_FUSION_CHERRY_SHOOTER_RULES_H

#include <cstdint>

namespace LawnFusion
{
constexpr int kFusionCherryShooterHealth = 300;
constexpr int kFusionCherryShooterDamage = 60;
constexpr float kFusionCherryShooterEvent = 0.6666667f;
constexpr float kFusionCherryProjectilePixelsPerTick = 3.2f;
constexpr float kFusionCherryProjectileCenterOffset = 20.0f;

// Source countdown fires strictly after zero. These are native100Hz deadlines,
// using the same launch/shoot counters as Plant, with no separate game clock.
bool TryGetFusionCherryInitialTicks(float sourceFraction, int& ticks);
bool TryGetFusionCherryCycleTicks(float sourceMultiplier, int& ticks);
// UpdateShooting decrements before firing at1; the trigger sets delayTicks+1.
bool TryGetFusionCherryShotCounter(float animationSpeed, int& counter);
bool FusionNativeAnimationSpeedValid(uint16_t kind, float speed);
float FusionCherryShootDuration(float animationSpeed);

bool FusionCherryProjectileCanHit(int sourceStatus, bool sameRow,
    bool mindControlled, float bodyHealth);

struct FusionCherryProjectileRect
{
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
};
// Original projectile BoxCollider2D projected onto the native72x100 grid.
bool TryMakeFusionCherryProjectileRect(double centerX, double centerY,
    FusionCherryProjectileRect& result);
// Bounds are source Board bounds, in source units. Root position is the source
// projectile centre. Awake's maxX+1 and MoveDieUpdate's extra+1 are both retained.
bool FusionCherryProjectileExpired(double sourceX, double sourceY, int ageTicks,
    double boardMinX, double boardMaxX, double boardMinY, double boardMaxY);
}

#endif
