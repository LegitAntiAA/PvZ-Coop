#ifndef LAWN_FUSION_NATIVE_DEATH_H
#define LAWN_FUSION_NATIVE_DEATH_H
#include "FusionDeathRules.h"
class Board;
class Plant;
namespace LawnFusion
{
void ApplyNativeFusionBlast(Board* board, const NativeBlastSpec& spec);
// Capture admission before an originating kill can award the level. Continue
// synchronously with that same board/spec; it must finish the accepted effect.
bool CanStartNativeFusionBlast(Board* board, const NativeBlastSpec& spec);
void ContinueNativeFusionBlast(Board* board, const NativeBlastSpec& spec);
// Also called by the existing source2 Cherry explosion, using its own exact center.
void HealNativeCherryNutFromBlast(Board* board, double centerX, double centerY, int row);
// After a real HP change, before a lethal Plant.Die.
void NativePlantHealthChangedForDeath(Plant* plant);
// First canonical removal only, after mDead=true; never from destructors.
// Source reasons0..7 run effects,255 is a silent native allocation rollback.
void NativePlantRemoving(Plant* plant, int sourceReason);
}
#endif
