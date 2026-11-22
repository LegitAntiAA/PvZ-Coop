#ifndef LAWN_FUSION_NATIVE_GARLIC_H
#define LAWN_FUSION_NATIVE_GARLIC_H
#include "FusionGarlicRules.h"
class Plant;
class Zombie;
namespace LawnFusion
{
enum class NativeGarlicBiteResult { NotGarlic, HandledBlocked, Started };
bool IsNativeGarlicPlant(const Plant* plant);
bool CanBeginNativeGarlicBite(Zombie* zombie, Plant* victim, const GarlicActionState& state);
// Root applies plantDamage through the existing EatPlant health/death path.
// Both HandledBlocked and Started consume the ordinary4-damage bite.
NativeGarlicBiteResult BeginNativeGarlicBite(Zombie* zombie, Plant* victim, GarlicActionState& state, int& plantDamage);
bool BeginNativeGarlicEffect(Zombie* zombie,uint16_t waitTicks,bool certainRow);
// Direct source Garliced(false,certain): no plant bite and no delayed wait.
bool BeginNativeGarliced(Zombie* zombie,bool certainRow = false);
// Caller owns state by full native generation and calls once per host tick.
// Speed is the existing effective movement speed: only0 stalls row progress.
bool UpdateNativeGarlicAction(Zombie* zombie, GarlicActionState& state, float effectiveMovementSpeed);
// Snapshot-only client adoption: image/sorting presentation, never a timer/RNG.
bool RestoreNativeGarlicPresentation(Zombie* zombie, const GarlicActionState& state);
bool BeginNativeSourceRowDuration313(Zombie*,float sourceSeconds);
void NativeGarlicHealthChanged(Plant* plant);
}
#endif
