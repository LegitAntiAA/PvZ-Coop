#ifndef LAWN_FUSION_PRODUCER_RULES_H
#define LAWN_FUSION_PRODUCER_RULES_H

#include <cstdint>

namespace LawnFusion
{
enum class FusionProducerGate { AnimatorSpeed, OutsideIZ, GameRunning };
enum class FusionProducerMode { Sun, Marigold, MoneyPot };
struct FusionProducerProfile
{
    uint16_t kind;
    float intervalSeconds;
    int periodicCoinCount;
    FusionProducerGate gate;
    FusionProducerMode mode = FusionProducerMode::Sun;
};
constexpr int kFusionProducerDelayTicks = 50;
constexpr uint8_t kFusionSunNutRewardMask = 3;

const FusionProducerProfile* FindFusionProducerProfile(uint16_t kind);
bool IsNativeProducerKind(uint16_t kind);
bool FusionProducerProfileValid(const FusionProducerProfile& profile);
bool TryGetFusionProducerInitialTicks(float sourceSeconds, int& ticks);
bool TryGetFusionProducerInitialTicks(const FusionProducerProfile& profile, float sourceSeconds, int& ticks);
bool TryGetFusionProducerCycleTicks(const FusionProducerProfile& profile, int jitterSeconds, int& ticks);
bool TryGetFusionProducerCycleTicks(const FusionProducerProfile& profile, float jitterSeconds, int& ticks);
int FusionProducerMaximumTicks(uint16_t kind);
bool FusionProducerGateOpen(const FusionProducerProfile& profile, bool playing, bool isIZ, float animatorSpeed);
// Positive strict-underflow deadlines and already-scheduled Invoke callbacks
// advance independently. Assign new pending50 only after the old pending step.
bool AdvanceFusionProducerCounter(int& ticks);
bool AdvanceFusionProducerPending(int& ticks);
bool FusionProducerCanFitCallback(int occupied, int capacity, int coinCount);
// Source CoinSun.Start, without the separate Travel reward modifier.
bool TryGetFusionSourceSunValue(uint16_t sourceItem, int& value);
bool FusionSunCoinMetadataValid(int type, int value);
int FusionSunNutStage(int health);
bool TryConsumeFusionSunNutReward(int health, bool isIZ, uint8_t& flags, int& coinCount);
}

#endif
