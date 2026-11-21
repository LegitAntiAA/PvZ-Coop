#ifndef LAWN_FUSION_NATIVE_EXTRAS268_H
#define LAWN_FUSION_NATIVE_EXTRAS268_H
#include "FusionExtras268Rules.h"
class Plant;class Board;class Coin;
namespace LawnFusion
{
void InitializeNativeExtras268Plant(Plant* plant);
// Root's source base continuation calls this before subtraction. MachineNut's
// Instead override is false: cap then continue base, with no protection search.
bool IsNativeMachine268(const Plant* plant);
bool CapNativeMachine268Damage(Plant* plant,int& damage);
// Call only from the shared Plant.Instead stage, after more-specific overrides.
bool TryRedirectNativeMachine268Damage(Plant* plant,int damage);
bool RecoverNativeMachine268(Plant* plant,int amount);
void NativeExtras268HealthChanged(Plant* plant);
// Source Freeze is global(row=-1), FireLine is confined to its actual row.
void NativeMachine268Element(Board* board,int row,uint8_t form);
void NativeExtras268PlantRemoving(Plant* plant,int sourceReason);
bool CanUseNativeExtras268Equipment(const Plant* plant,FusionEquipmentType type);
bool TryUseNativeExtras268Equipment(Plant* plant,FusionEquipmentType type,Coin* item);
}
#endif
