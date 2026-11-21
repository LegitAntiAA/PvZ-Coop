#ifndef LAWN_FUSION_BASE_PLANT_RULES_H
#define LAWN_FUSION_BASE_PLANT_RULES_H

#include <cstdint>

namespace LawnFusion
{
struct FusionShooterTimingProfile;
struct FusionFumeProfile
{
    uint16_t kind;
    int coldLevelsAfterDamage;
    bool hypnotize;
};
const FusionFumeProfile* FindFusionFumeProfile(uint16_t kind);
// Existing6/7 keep their original registered rows/precision. New audited Fume
// rows are shared with the global timing lookup by Root's family delegation.
const FusionShooterTimingProfile* FindFusionBaseTimingProfile(uint16_t kind);
bool TryGetFusionHypnoFumeThreshold(float health,int maximumHealth,int firstArmor,int maximumFirstArmor,
    uint8_t colorMask,float& threshold);
constexpr int kFusionChomperDigestTicks = 3000;
constexpr float kFusionPuffPixelsPerTick = 3.2f;
constexpr double kFusionBasePixelsX = 72.0 / 1.35;
constexpr double kFusionBasePixelsY = 100.0 / 1.67;

struct FusionChomperTiming
{
    int bite = 0, chew = 0, biteEnd = 0, swallow = 0;
};
bool TryGetFusionChomperTiming(float speed, FusionChomperTiming& result);
bool TryGetFusionBaseShotCounter(uint16_t kind, float speed, int& counter);
float FusionBaseShootDuration(uint16_t kind, float speed);
// Acquisition uses source Shadow points and strict bounds; Fume's event uses
// inclusive bounds. These are independent of a native zombie's body rectangle.
bool FusionBaseCanAcquire(uint16_t kind, int sourceStatus, bool sameRow,
    bool mindControlled, bool beforeDying, double deltaX);
bool FusionPlantSearchUniqueZombieAllowed(int sourceStatus, bool mindControlled, bool beforeDying);
bool FusionFumeCanHit(int sourceStatus, bool sameRow,
    bool mindControlled, bool beforeDying, double deltaX);
bool FusionChomperCanAcquire(int sourceStatus, bool sameRow,
    bool mindControlled, bool beforeDying);

struct FusionBaseRect { double x = 0, y = 0, width = 0, height = 0; };
bool TryMakeFusionChomperRect(double shadowX, double shadowY, FusionBaseRect& result);
bool TryMakeFusionPuffRect(double centerX, double centerY, FusionBaseRect& result);
bool FusionPuffLifetimeExpired(int ageTicks);
}
#endif
