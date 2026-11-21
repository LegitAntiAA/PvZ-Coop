#ifndef LAWN_FUSION_SHOOTER_TIMING_H
#define LAWN_FUSION_SHOOTER_TIMING_H

#include <cstdint>
#include <cstddef>
#include <span>

namespace LawnFusion
{
// Verified source controller/CSV timing. This describes the existing native
// Plant counters; it owns no clock, actor, targeting or damage implementation.
struct FusionShooterTimingProfile
{
    uint16_t kind;
    float attackIntervalSeconds;
    std::span<const double> eventClipSeconds;
    double stateSpeed;
    double clipDurationSeconds;
    bool speedParameterActive;
    uint16_t projectileKind; //0xffff denotes an area payload, not a projectile.
    const char* visualState = "shoot";
};

// Native aliases and abilities determine which source profiles are playable.
const FusionShooterTimingProfile* FindFusionShooterTimingProfile(uint16_t kind);
// Exact source Animator state; no default/alias fallback for an unknown state.
const FusionShooterTimingProfile* FindFusionShooterTimingProfile(uint16_t kind, const char* state);
bool FusionShooterTimingProfileValid(const FusionShooterTimingProfile& profile);
bool TryGetFusionShooterInitialTicks(const FusionShooterTimingProfile& profile, float sourceFraction, int& ticks);
bool TryGetFusionShooterCycleTicks(const FusionShooterTimingProfile& profile, float sourceMultiplier, int& ticks);
bool TryGetFusionShooterShotCounter(const FusionShooterTimingProfile& profile, float animationSpeed, int& counter);
// Called once after the real native counter decrements. Quantized coincident
// events each contribute one shot; the counter is the sole timeline state.
bool TryGetFusionShooterEventsAtCounter(const FusionShooterTimingProfile& profile,
    float animationSpeed, int remainingCounter, int& eventCount);
bool TryGetFusionShooterEventAtCounter(const FusionShooterTimingProfile& profile,
    float animationSpeed, int remainingCounter, std::size_t eventIndex, bool& eventDue);
float FusionShooterDuration(const FusionShooterTimingProfile& profile, float animationSpeed);
// A validation envelope, never an action clock or a state selection policy.
float FusionShooterMaximumDuration(uint16_t kind, float animationSpeed);
}
#endif
