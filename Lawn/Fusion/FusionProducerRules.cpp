#include "FusionProducerRules.h"
#include "FusionRollingProjectileRules.h"
#include "../../ConstEnums.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr FusionProducerProfile profiles[] = {
    {1,25,1,FusionProducerGate::AnimatorSpeed},
    {1154,25,1,FusionProducerGate::AnimatorSpeed},
    {1187,15,1,FusionProducerGate::AnimatorSpeed},
    {1000,25,1,FusionProducerGate::OutsideIZ},
    {1006,15,1,FusionProducerGate::AnimatorSpeed},
    {1033,20,2,FusionProducerGate::AnimatorSpeed},
    {1031,25,1,FusionProducerGate::AnimatorSpeed},
    {1009,15,1,FusionProducerGate::OutsideIZ},
    {1184,25,1,FusionProducerGate::GameRunning,FusionProducerMode::MoneyPot},
    {1136,25,1,FusionProducerGate::GameRunning,FusionProducerMode::MoneyPot},
    {1137,25,1,FusionProducerGate::GameRunning,FusionProducerMode::MoneyPot},
    {31,25,1,FusionProducerGate::GameRunning,FusionProducerMode::Marigold},
    {1140,20,2,FusionProducerGate::GameRunning,FusionProducerMode::Marigold}
};
bool Deadline(float seconds, int& ticks, bool inclusive = false)
{
    const double result = inclusive ? std::ceil(static_cast<double>(seconds)*100.0) : std::floor(static_cast<double>(seconds)*100.0)+1.0;
    if (!std::isfinite(result) || seconds < 0 || result > (std::numeric_limits<int>::max)()) return false;
    ticks = static_cast<int>(result);
    return true;
}
}

const FusionProducerProfile* FindFusionProducerProfile(uint16_t kind)
{
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    return nullptr;
}
bool IsNativeProducerKind(uint16_t kind) { return FindFusionProducerProfile(kind) != nullptr; }

bool FusionProducerProfileValid(const FusionProducerProfile& profile)
{
    const auto* known = FindFusionProducerProfile(profile.kind);
    const bool gate = profile.mode != FusionProducerMode::Sun ? profile.gate == FusionProducerGate::GameRunning :
        profile.gate == FusionProducerGate::AnimatorSpeed || profile.gate == FusionProducerGate::OutsideIZ;
    return known && profile.mode == known->mode && gate && std::isfinite(profile.intervalSeconds) &&
        profile.intervalSeconds > 2 && static_cast<double>(profile.intervalSeconds)*100+201 <= (std::numeric_limits<int>::max)() &&
        (profile.periodicCoinCount == 1 || profile.periodicCoinCount == 2);
}

bool TryGetFusionProducerInitialTicks(float sourceSeconds, int& ticks)
{
    return sourceSeconds >= 4 && sourceSeconds <= 7 && Deadline(sourceSeconds,ticks);
}
bool TryGetFusionProducerInitialTicks(const FusionProducerProfile& profile, float seconds, int& ticks)
{
    return FusionProducerProfileValid(profile) && seconds >= 4 && seconds <= 7 &&
        Deadline(seconds,ticks,profile.mode != FusionProducerMode::Sun);
}

bool TryGetFusionProducerCycleTicks(const FusionProducerProfile& profile, int jitterSeconds, int& ticks)
{
    return FusionProducerProfileValid(profile) && profile.mode == FusionProducerMode::Sun && jitterSeconds >= -2 && jitterSeconds <= 2 &&
        Deadline(profile.intervalSeconds+static_cast<float>(jitterSeconds),ticks);
}
bool TryGetFusionProducerCycleTicks(const FusionProducerProfile& profile, float jitter, int& ticks)
{
    if (FusionProducerProfileValid(profile) && profile.mode == FusionProducerMode::MoneyPot)
        return std::isfinite(jitter) && jitter >= .9f && jitter <= 1.1f &&
            Deadline(profile.intervalSeconds*jitter,ticks,true);
    return FusionProducerProfileValid(profile) && profile.mode == FusionProducerMode::Marigold &&
        std::isfinite(jitter) && jitter >= -2 && jitter <= 2 && Deadline(profile.intervalSeconds+jitter,ticks,true);
}
int FusionProducerMaximumTicks(uint16_t kind)
{
    const auto* profile = FindFusionProducerProfile(kind); int initial = 0, cycle = 0;
    if (!profile || !TryGetFusionProducerInitialTicks(*profile,7,initial)) return 0;
    const bool valid = profile->mode == FusionProducerMode::MoneyPot ? TryGetFusionProducerCycleTicks(*profile,1.1f,cycle) :
        profile->mode == FusionProducerMode::Marigold ? TryGetFusionProducerCycleTicks(*profile,2.0f,cycle) :
        TryGetFusionProducerCycleTicks(*profile,2,cycle);
    return valid ? (initial > cycle ? initial : cycle) : 0;
}

bool FusionProducerGateOpen(const FusionProducerProfile& profile, bool playing, bool isIZ, float animatorSpeed)
{
    if (!playing || !FusionProducerProfileValid(profile)) return false;
    if (profile.gate == FusionProducerGate::GameRunning) return true;
    return profile.gate == FusionProducerGate::OutsideIZ ? !isIZ : std::isfinite(animatorSpeed) && animatorSpeed != 0;
}

bool AdvanceFusionProducerCounter(int& ticks)
{
    if (ticks < 0) return false;
    if (ticks > 0) --ticks;
    return ticks == 0;
}

bool AdvanceFusionProducerPending(int& ticks)
{
    if (ticks <= 0 || ticks > kFusionProducerDelayTicks) return false;
    return --ticks == 0;
}

bool FusionProducerCanFitCallback(int occupied, int capacity, int coinCount)
{
    return occupied >= 0 && capacity >= occupied && coinCount > 0 && coinCount <= capacity-occupied;
}

bool TryGetFusionSourceSunValue(uint16_t sourceItem, int& value)
{
    switch (sourceItem)
    {
    case 0: value=25; return true;
    case 1: value=50; return true;
    case 2: value=15; return true;
    case 13: value=5; return true;
    default: return false;
    }
}

bool FusionSunCoinMetadataValid(int type, int value)
{
    return (type == COIN_SMALLSUN && (value == 5 || value == 15)) ||
        (type == COIN_SUN && value == 25) || (type == COIN_LARGESUN && value == 50);
}

int FusionSunNutStage(int health)
{
    // Both source classes use the same strict thirds of4000, including intact
    // equality at1333/2666. Reward history remains separate from presentation.
    return FusionNutShooterDamageStage(health);
}

bool TryConsumeFusionSunNutReward(int health, bool isIZ, uint8_t& flags, int& coinCount)
{
    if (flags & ~kFusionSunNutRewardMask) return false;
    const int stage = FusionSunNutStage(health);
    const uint8_t bit = stage == 1 ? 1 : stage == 2 ? 2 : 0;
    coinCount = 0;
    if (!isIZ && bit && !(flags & bit)) { flags |= bit; coinCount = 1; }
    return true;
}
}
