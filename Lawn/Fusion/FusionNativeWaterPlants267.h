#ifndef LAWN_FUSION_NATIVE_WATER_PLANTS267_H
#define LAWN_FUSION_NATIVE_WATER_PLANTS267_H
#include <cstdint>
class Plant;
namespace LawnFusion
{
bool InitializeNativeWater267(Plant*);
bool UpdateNativeWater267(Plant*);
bool UpdateNativeWater267Phase(Plant*);
bool SetNativeLilyType267(Plant*,uint16_t sourceKind);
bool NativeLily267Covered(const Plant*);
}
#endif
