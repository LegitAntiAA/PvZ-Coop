#ifndef LAWN_FUSION_NATIVE_SHOOTER_TIMING_H
#define LAWN_FUSION_NATIVE_SHOOTER_TIMING_H

#include "FusionShooterTiming.h"
class Plant;

namespace LawnFusion
{
bool InitializeNativeShooterTiming(Plant* plant, const FusionShooterTimingProfile& profile,
    bool wakeMushroomBeforeInitialFraction);
bool ResetNativeShooterLaunchCounter(Plant* plant, const FusionShooterTimingProfile& profile);
bool BeginNativeShooterShot(Plant* plant, const FusionShooterTimingProfile& profile);
void UpdateNativeShooterPhase(Plant* plant, const FusionShooterTimingProfile& profile);
// Handles a registered native timeline after mShootingCounter--. The caller
// skips its legacy firing branches but still runs native idle-animation cleanup.
bool DispatchNativeShooterEvents(Plant* plant);
}
#endif
