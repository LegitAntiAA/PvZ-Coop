#include "FusionCherryShooterRules.h"
#include "FusionShooterTiming.h"
#include "FusionAction267Rules.h"
#include "FusionUmbrellaBatchRules.h"
#include "FusionChomperRules.h"
#include "FusionBatchActionRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionEconomyRules.h"
#include "FusionAction266Rules.h"
#include "FusionEquipmentRules.h"
#include "FusionWater268Rules.h"
#include "FusionCannon268Rules.h"
#include "FusionChomper268Rules.h"
#include "FusionShulk268Rules.h"
#include "FusionExtras268Rules.h"
#include "FusionRemainingPlants269Rules.h"
#include "FusionCompanions269Rules.h"
#include "FusionSummonPlants269Rules.h"
#include "FusionLastPlants269Rules.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
bool InRange(float value, float minimum, float maximum)
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
}

bool TryGetFusionCherryInitialTicks(float sourceFraction, int& ticks)
{
    return TryGetFusionShooterInitialTicks(*FindFusionShooterTimingProfile(1001), sourceFraction, ticks);
}

bool TryGetFusionCherryCycleTicks(float sourceMultiplier, int& ticks)
{
    return TryGetFusionShooterCycleTicks(*FindFusionShooterTimingProfile(1001), sourceMultiplier, ticks);
}

bool TryGetFusionCherryShotCounter(float animationSpeed, int& counter)
{
    return TryGetFusionShooterShotCounter(*FindFusionShooterTimingProfile(1001), animationSpeed, counter);
}

bool FusionNativeAnimationSpeedValid(uint16_t kind, float speed)
{
    return FindFusionChomperProfile(kind) || FindFusionBatchActionProfile(kind) ||
        FindFusionEquipmentProfile(kind) || IsFusionWater268Kind(kind) || FindFusionCannon268Profile(kind) ||
        FindFusionStrongChomperProfile(kind) || IsFusionShulk268Kind(kind) || IsFusionExtras268Kind(kind) ||
        IsFusionFireActionSupported(kind) || IsFusionEconomyBatchKind(kind) || IsFusionAction266Kind(kind) ||
        IsFusionAction267Kind(kind) || IsFusionUmbrellaBatchKind(kind) || kind==1078 ||
        IsFusionRemaining269Kind(kind) || FindFusionCompanions269Profile(kind) ||
        IsFusionSummonPlants269Kind(kind) || kind==34 || kind==937 ||
        IsFusionLastPlants269Kind(kind) || kind==909 || kind==926 ||
        FindFusionShooterTimingProfile(kind) != nullptr ?
        InRange(speed, 0.9f, 1.1f) : std::isfinite(speed) && speed == 1.0f;
}

float FusionCherryShootDuration(float animationSpeed)
{
    return FusionShooterDuration(*FindFusionShooterTimingProfile(1001), animationSpeed);
}

bool FusionCherryProjectileCanHit(int status, bool sameRow, bool mindControlled, float bodyHealth)
{
    if (status < 0 || !sameRow || mindControlled || !std::isfinite(bodyHealth) || bodyHealth <= 0) return false;
    switch (status)
    {
    case 3: case 7: case 9: case 10: case 12: case 13: case 16: case 20: case 21:
        return false;
    default:
        return true;
    }
}

bool TryMakeFusionCherryProjectileRect(double centerX, double centerY, FusionCherryProjectileRect& result)
{
    if (!std::isfinite(centerX) || !std::isfinite(centerY)) return false;
    constexpr double pixelsX = 72.0 / 1.35, pixelsY = 100.0 / 1.67;
    constexpr double width = 2.13 * 0.2 * pixelsX;
    constexpr double height = 1.9312102 * 0.2 * pixelsY;
    constexpr double centerOffsetY = 0.025604904 * 0.2 * pixelsY;
    result = {centerX-width/2.0, centerY+centerOffsetY-height/2.0, width, height};
    return true;
}

bool FusionCherryProjectileExpired(double x, double y, int ageTicks,
    double minX, double maxX, double minY, double maxY)
{
    if (!std::isfinite(x+y+minX+maxX+minY+maxY) || minX >= maxX || minY >= maxY || ageTicks < 0) return true;
    return ageTicks > 2000 || x < minX-2.0 || x > maxX+2.0 || y < minY || y > maxY;
}
}
