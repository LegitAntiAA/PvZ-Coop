#include "FusionConveyor305Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace LawnFusion
{
namespace
{
constexpr FusionPlantId pool27[] = {12,12,13,14,14,18,16,17,15,3};
constexpr FusionPlantId pool36[] = {12,19,23,23,21,20,20,22,24,10,25};
constexpr FusionPlantId pool45[] = {27,28,32,26,30,27,28,32,26,30,27,32,10,16};
const FusionConveyor305Profile profiles[] = {{27,3.0f,pool27},{36,4.0f,pool36},{45,3.0f,pool45}};
}

const FusionConveyor305Profile* FindFusionConveyor305Profile(int sourceLevel)
{
    for (const auto& profile : profiles) if (profile.sourceLevel == sourceLevel) return &profile;
    return nullptr;
}

bool StepFusionConveyor305(int sourceLevel, FusionConveyor305State& state,
    std::span<const FusionPlantId> queuedKinds, float deltaSeconds, bool paused,
    const FusionConveyor305RandomIndex& randomIndex, FusionConveyor305Step& output)
{
    const auto* profile = FindFusionConveyor305Profile(sourceLevel);
    if (!profile || !std::isfinite(state.elapsedSeconds) || state.elapsedSeconds < 0.0f ||
        !std::isfinite(deltaSeconds) || deltaSeconds < 0.0f || queuedKinds.size() > kFusionSourceConveyor305Capacity) return false;
    for (const auto kind : queuedKinds)
        if (std::find(profile->orderedPool.begin(), profile->orderedPool.end(), kind) == profile->orderedPool.end()) return false;
    FusionConveyor305Step step;
    if (paused || queuedKinds.size() == kFusionSourceConveyor305Capacity) { output = step; return true; }
    auto candidate = state;
    candidate.elapsedSeconds += deltaSeconds;
    if (!std::isfinite(candidate.elapsedSeconds)) return false;
    if (candidate.elapsedSeconds <= profile->intervalSeconds) { state = candidate; output = step; return true; }
    if (!randomIndex || candidate.birthCount == (std::numeric_limits<uint32_t>::max)()) return false;
    try
    {
        std::vector<FusionPlantId> pool(profile->orderedPool.begin(), profile->orderedPool.end());
        for (;;)
        {
            size_t index = 0, rejection = 0;
            if (!randomIndex(pool.size(), index) || index >= pool.size()) return false;
            step.kind = pool[index];
            const auto count = std::count(queuedKinds.begin(), queuedKinds.end(), step.kind);
            if (!randomIndex(3, rejection) || rejection >= 3) return false;
            if (rejection >= static_cast<size_t>(count)) break;
            pool.erase(std::remove(pool.begin(), pool.end(), step.kind), pool.end());
            if (pool.empty()) break; // Source returns the last candidate, including overrepresented kinds.
        }
    }
    catch (...) { return false; }
    candidate.elapsedSeconds = 0.0f;
    ++candidate.birthCount;
    step.birth = true;
    state = candidate;
    output = step;
    return true;
}
}
