#ifndef LAWN_FUSION_ZOMBIE_MOTION_H
#define LAWN_FUSION_ZOMBIE_MOTION_H

#include "FusionZombieRules.h"

namespace LawnFusion
{
constexpr float kFusionZombieWalkClipSeconds = 1.9166666f;
// Query for the caller's existing animation phase. No phase is advanced here.
// The selected walk variant remains explicit; cold does not reselect the clip.
bool TryGetFusionZombieWalkRate(uint16_t kind, bool walk2, float originSpeed, float statusFactor, float& output);
// Exact serialized local-X curve, including its constant end-of-clip tail.
bool TrySampleFusionZombieRootX(uint16_t kind, bool walk2, float clipSeconds, float& output);
// Unwrapped curve displacement, reflected once for right-facing/MC actors.
// explicitScale=1 returns raw curve units. A different nonnegative scale is
// caller policy, never proof of Unity scale/application or total world motion.
bool TryGetFusionZombieRootDelta(uint16_t kind, bool walk2, FusionZombieClipPosition previous,
    FusionZombieClipPosition current, bool facingRight, double& output, double explicitScale = 1.0);
}

#endif
