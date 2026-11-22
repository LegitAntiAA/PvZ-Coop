#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace LawnFusion
{
constexpr uint16_t kFusionSnowBallDisable363Ticks=500;
// Each source TryBeDisable(5) schedules exactly one matching TryBeActive.
// The count is the vector length; no duplicate scalar or animation clock.
bool FusionPlantDisable363ViewValid(std::span<const uint16_t> remaining);
bool TryAddFusionPlantDisable363(std::span<const uint16_t> before,std::vector<uint16_t>& after);
bool TickFusionPlantDisable363(std::vector<uint16_t>& remaining,bool& reactivated);
bool FusionSourcePlantFire363(uint16_t);
bool FusionSourcePlantIce363(uint16_t);
bool FusionSourcePlantDisable363Immune(uint16_t);
}
