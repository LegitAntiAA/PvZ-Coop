#ifndef LAWN_FUSION_ROLLING_PROJECTILE_RULES_H
#define LAWN_FUSION_ROLLING_PROJECTILE_RULES_H

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

namespace LawnFusion
{
constexpr double kFusionRollingPixelsX = 72.0 / 1.35;
constexpr double kFusionRollingPixelsY = 100.0 / 1.67;
constexpr float kFusionRollingPixelsPerTick = static_cast<float>(2.4 * kFusionRollingPixelsX * .01);
constexpr float kFusionRollingGravityPerTick = static_cast<float>(9.8 * kFusionRollingPixelsY * .01 * .01);
constexpr float kFusionRollingStartVelocity = static_cast<float>(9.8 * .02 * kFusionRollingPixelsY * .01);
constexpr float kFusionRollingRadiansPerTick = static_cast<float>(-6.0 * 3.14159265358979323846 * .01);

// Existing Projectile mPosZ is negative height and mVelZ is downward pixels
// per native tick. Source Start initializes velocity once without moving.
struct FusionRollingMotionProfile { float velocityX, accelerationZ, rotationPerTick; };
constexpr FusionRollingMotionProfile kFusionNutRollingMotion =
    {kFusionRollingPixelsPerTick, kFusionRollingGravityPerTick, kFusionRollingRadiansPerTick};
bool AdvanceFusionRollingMotion(const FusionRollingMotionProfile& profile,
    float& x, float& heightZ, float& downwardVelocity, float& rotation);
int FusionNutShooterDamageStage(int health);
struct FusionRollingRect { double x = 0, y = 0, width = 0, height = 0; };
bool TryMakeFusionRollingRect(double centerX, double centerY, FusionRollingRect& result);
bool TryMakeFusionRollingRect(uint16_t kind,double centerX,double centerY,FusionRollingRect& result);
bool FusionRollingProjectileExpired(double sourceX, double sourceY, int ageTicks,
    double boardMinX, double boardMaxX, double boardMinY, double boardMaxY);
bool FusionRollingProjectileCanHit(int sourceStatus, bool sameRow,
    bool mindControlled, bool flying, float bodyHealth);

// Keys are full DataArray generation IDs, never slot indices or wire NetIds.
// This is host-only impact history for real projectiles, not an entity store.
using FusionRollingHitHistory = std::unordered_map<uint32_t, std::unordered_set<uint32_t>>;
bool RememberFusionRollingHit(FusionRollingHitHistory& history, uint32_t projectileId, uint32_t zombieId);
std::size_t FusionProjectileHitCount(const FusionRollingHitHistory& history, uint32_t projectileId);
}
#endif
