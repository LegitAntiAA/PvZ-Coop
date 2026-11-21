#ifndef LAWN_FUSION_NATIVE_SUMMON269_H
#define LAWN_FUSION_NATIVE_SUMMON269_H
#include "FusionSummon269Rules.h"
class Plant;
class Zombie;
namespace LawnFusion
{
// Caller supplies the actual source callback choice. Unknown specialist leaves
// fail, never fall back to a generic Zombie. Coordinates are Board shadow X.
Zombie* TrySpawnNativeSummon269(Plant* parent,const FusionSummon269Choice& choice,int row,float shadowX,bool replaceLinked=false);
void NativeSummon269PlantRemoving(Plant* parent);
void NativeSummon269ZombieRemoving(Zombie* zombie);
// Hook before generic Zombie.TakeDamage armor/immunity/limb processing. True
// means MoneyZombie's direct body-only source override handled the call.
bool NativeSummon269MoneyDamage(Zombie* zombie,int damage);
// ONLY Cold/Freeze/MindControl/Poison/Garlic/EatGarlic entry points. Butter
// and Grap remain inherited source behavior and must not use this guard.
bool NativeSummon269RejectStatus(const Zombie* zombie);
// TallIceNut111 overrides only Cold/Freeze; its MC/Poison/Garlic are inherited.
bool NativeSummon269RejectCold(const Zombie* zombie);
void NativeSummon269MoneyDied(Zombie* zombie);
// Call after the existing native position update; no new movement clock.
void FinishNativeSummon269ZombiePosition(Zombie* zombie);
}
#endif
