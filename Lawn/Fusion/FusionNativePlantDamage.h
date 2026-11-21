#ifndef LAWN_FUSION_NATIVE_PLANT_DAMAGE_H
#define LAWN_FUSION_NATIVE_PLANT_DAMAGE_H
class Plant;
namespace LawnFusion
{
// Source damage types stay distinct from native weapon flags and removal reasons.
void ApplyNativeSourcePlantDamage(Plant* plant,int damage,int sourceType);
bool AdjustNativeSourcePlantDamage(Plant* plant,int& damage,int sourceType);
bool NativeSourcePlantColliderEnabled(const Plant* plant);
}
#endif
