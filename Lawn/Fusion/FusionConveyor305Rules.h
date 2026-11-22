#pragma once

#include "FusionCatalog.h"
#include <functional>
#include <span>

namespace LawnFusion
{
constexpr size_t kFusionSourceConveyor305Capacity = 13;

struct FusionConveyor305Profile
{
    uint8_t sourceLevel;
    float intervalSeconds;
    std::span<const FusionPlantId> orderedPool; // Duplicates are source weights.
};
const FusionConveyor305Profile* FindFusionConveyor305Profile(int sourceLevel);

// Host-private feed clock. The actual bank packets are the only queue.
struct FusionConveyor305State
{
    float elapsedSeconds = 10.0f;
    uint32_t birthCount = 0;
    bool operator==(const FusionConveyor305State&) const = default;
};
struct FusionConveyor305Step
{
    bool birth = false;
    FusionPlantId kind = kFusionNoPlant;
    bool operator==(const FusionConveyor305Step&) const = default;
};
using FusionConveyor305RandomIndex = std::function<bool(size_t exclusiveMaximum, size_t& index)>;
// Source campaign27/36/45 only. Caller gates Playing and supplies the ordinary
// native simulation delta. A failed/throwing/out-of-range RNG leaves state and
// output unchanged; rollback of the caller's RNG is the caller's responsibility.
// Full/paused queues do not accumulate time. At most one birth per call.
bool StepFusionConveyor305(int sourceLevel, FusionConveyor305State& state,
    std::span<const FusionPlantId> queuedKinds, float deltaSeconds, bool paused,
    const FusionConveyor305RandomIndex& randomIndex, FusionConveyor305Step& output);
}
