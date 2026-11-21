#ifndef LAWN_FUSION_NATIVE_ECONOMY_H
#define LAWN_FUSION_NATIVE_ECONOMY_H
#include "FusionEconomyRules.h"
class Plant;
namespace LawnFusion
{
bool InitializeNativeEconomyPlant(Plant* plant);
bool UpdateNativeEconomyPlant(Plant* plant);
bool UpdateNativeEconomyPhase(Plant* plant);
bool TryActivateNativeEconomySkill(Plant* plant);
bool GetNativeEconomyVisualPose(const Plant* plant,const char*& state,float& seconds);
}
#endif
