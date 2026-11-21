#ifndef LAWN_FUSION_POISON_RULES_H
#define LAWN_FUSION_POISON_RULES_H

#include <limits>

namespace LawnFusion
{
constexpr int kFusionPoisonDamageIntervalTicks = 100;
constexpr int kFusionPoisonStackExtensionTicks = 500;
constexpr int kFusionPoisonDefaultDurationTicks = 1000;
// Checked native I32 damage boundary, not a source gameplay stack cap.
constexpr int kFusionPoisonMaximumLevel = (std::numeric_limits<int>::max)()/20;
struct FusionPoisonState
{
    int level = 0;
    int ticks = 0;
    int damageTicks = kFusionPoisonDamageIntervalTicks;
    bool operator==(const FusionPoisonState&) const = default;
};
bool FusionPoisonStateValid(int level,int ticks,int damageTicks);
bool FusionPoisonStateValid(const FusionPoisonState& state);
// Source AddPoisonLevel:0..4 increment without a timer;5 extends5s without
// incrementing;6+ delegates to SetPoison10s, which increments again.
bool TryAddFusionPoisonLevel(const FusionPoisonState& before,FusionPoisonState& after);
bool TrySetFusionPoison(const FusionPoisonState& before,int ticks,FusionPoisonState& after);
// Source MoveUpdate expires duration before Update's damage pulse. Inactive
// time preserves the pulse remainder; reapplication never resets it.
bool TryStepFusionPoison(const FusionPoisonState& before,FusionPoisonState& after,int& damage,bool& pulse);
// Warm clears the level only while a timed poison is active. Both counters
// survive, including a zero-damage pulse during the remaining duration.
bool TryWarmFusionPoison(const FusionPoisonState& before,FusionPoisonState& after);
}
#endif
