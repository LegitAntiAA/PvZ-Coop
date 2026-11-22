#pragma once
#include "FusionPlantDisable363Rules.h"
#include <array>
#include <cstdint>
class Board;class Plant;class Reanimation;
namespace LawnFusion
{
// HOST/private presentation references only. Canonical remaining ticks are
// NativePlantData::disable363, alongside the real entity's existing metadata.
struct NativePlantDisable363State
{uint32_t epoch=0;int lastTick=-1;std::array<uint32_t,8> clocks{};};
bool NativePlantDisabled363(const Plant*);
bool CanDisableNativePlant363(const Plant*);
bool DisableNativePlant363(Plant*);
void UpdateNativePlantDisables363(Board*);
void ReconcileNativePlantDisables363(Board*); // Complete raw snapshot + metadata already adopted.
bool NativePlantDisable363HoldsClock(const Reanimation*);
void ApplyNativePlantDisable363Color(Plant*);
bool HandleNativeDisabledPlantAbilities363(Plant*);
}
