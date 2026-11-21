#include "FusionSplitShooterRules.h"
#include "FusionStraightProjectileRules.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr FusionSplitEmissionSpec splitPea[] = {
    {1090,0,0,-.9703125f,.600305796f,1},
    {1090,0,0,.514f,.6003f,0},
    {1090,1,0,-.9703125f,.600305796f,1}
};
constexpr FusionSplitEmissionSpec cherrySplit[] = {
    {1108,0,1,-.9703125f,.600305796f,1},
    {1108,0,1,.414f,.6003f,0},
    {1108,1,1,-.9703125f,.600305796f,1}
};
constexpr FusionSplitEmissionSpec splitPuff[] = {
    {1153,0,10,-.2985f,.306f,1},
    {1153,0,10,.204f,.08175f,0},
    {1153,1,10,-.2985f,.306f,1}
};
constexpr FusionSplitEmissionSpec snowSplit[] = {
    {1107,0,15,-.8624999724f,.66355f,1},
    {1107,0,15,.367999977f,.66355f,0},
    {1107,1,15,-.8624999724f,.66355f,1}
};
}

std::span<const FusionSplitEmissionSpec> GetFusionSplitEmissionSpecs(uint16_t kind)
{
    switch (kind)
    {
    case 1090: return splitPea;
    case 1108: return cherrySplit;
    case 1153: return splitPuff;
    case 1107: return snowSplit;
    default: return {};
    }
}

bool FusionSplitEmissionValid(uint16_t kind, const FusionSplitEmissionSpec& emission)
{
    for (const auto& known : GetFusionSplitEmissionSpecs(kind)) if (known == emission) return true;
    return false;
}

bool FusionSplitProjectilePhaseValid(uint16_t kind, uint8_t specialType, ProjectileMotion motion)
{
    const auto* profile = FindFusionStraightProjectileProfile(kind);
    if ((kind == 15 || kind == 97) && (specialType == 3 || specialType == 4)) return motion == MOTION_THREEPEATER;
    return profile && specialType <= 2 && (!specialType || profile->supportsSplitPhases) &&
        motion == (specialType == 1 ? MOTION_BACKWARDS : MOTION_STRAIGHT);
}

bool TryAdvanceFusionSplitProjectilePhase(double x, double minX, uint8_t specialType, uint8_t& next)
{
    if (!std::isfinite(x) || !std::isfinite(minX) || specialType > 2) return false;
    next = specialType == 1 && x < minX ? 2 : specialType;
    return true;
}

bool TryGetFusionSplitProjectileDamageFlags(uint16_t kind, uint8_t specialType, unsigned int& flags)
{
    const auto* profile = FindFusionStraightProjectileProfile(kind);
    if (!profile || specialType > 2 || (specialType && !profile->supportsSplitPhases)) return false;
    // Source Shieldless4 skips only the shield. Native TakeDamage still applies
    // helmet/body normally. PuffPea10 always uses Normal0, including rear shots.
    flags = specialType == 1 && kind != 10 ? 1U << DAMAGE_BYPASSES_SHIELD : profile->nativeDamageFlags;
    return true;
}

bool FusionSplitShooterCanTarget(int status, bool sameRow, bool mindControlled,
    float health, double x, double vision)
{
    if (status < 0 || !sameRow || mindControlled || !std::isfinite(health) || health <= 0 ||
        !std::isfinite(x) || !std::isfinite(vision) || x >= vision) return false;
    switch (status)
    {
    case 1: case 3: case 7: case 9: case 10: case 12:
    case 16: case 20: case 21: case 22: case 23: case 24:
        return false;
    default: return true;
    }
}
}
