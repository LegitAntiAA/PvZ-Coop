#ifndef LAWN_FUSION_NATIVE_UMBRELLA_H
#define LAWN_FUSION_NATIVE_UMBRELLA_H

#include "FusionUmbrellaRules.h"
class Plant;
class Projectile;

namespace LawnFusion
{
enum class NativeUmbrellaVerticalAxis { Unknown, PlanarY, LobZ };
// Only a future audited hostile factory may supply this normalized view.
// No source-hostile/native-body registry is enabled by this module.
struct NativeUmbrellaProjectileView
{
    uint32_t generation = 0;
    uint16_t sourceKind = 65535;
    uint8_t sourceWay = 0;
    bool isZombieBullet = false;
    bool hitLatched = false;
    int bulletRow = -1;
    NativeUmbrellaVerticalAxis verticalAxis = NativeUmbrellaVerticalAxis::Unknown;
};
// Return actual source Animator.speed eligibility. Native mAnimRate is not
// that property; absence of the canonical eligibility adapter rejects entry.
using NativeUmbrellaEligibility = bool (*)(const Plant*, float& sourceAnimatorSpeed);
Plant* FindNativeUmbrellaForTarget(Projectile* projectile, Plant* attacked,
    const NativeUmbrellaProjectileView* view, NativeUmbrellaEligibility eligibility);
bool TryNativeUmbrellaIntercept(Projectile* projectile, Plant* attacked, Plant* umbrella,
    const NativeUmbrellaProjectileView* view, NativeUmbrellaEligibility eligibility,
    FusionUmbrellaBlockState& state, FusionUmbrellaPoseState& pose, bool& startPose);
FusionUmbrellaAdvance AdvanceNativeUmbrellaDeflection(Projectile* projectile,
    FusionUmbrellaBlockState& state, NativeUmbrellaVerticalAxis axis);
bool IsNativeUmbrellaHarmless(const Projectile* projectile, const FusionUmbrellaBlockState& state);
// Caller dispatches the three exact source clip events using its existing
// native phase. This helper owns no animation clock or gameplay event loop.
bool ApplyNativeUmbrellaPoseEvent(Plant* umbrella, FusionUmbrellaPoseState& pose,
    FusionUmbrellaPoseEvent event);
}

#endif
