#ifndef LAWN_FUSION_NATIVE_POISON_H
#define LAWN_FUSION_NATIVE_POISON_H

#include "FusionPoisonRules.h"
class Zombie;
namespace LawnFusion
{
int GetNativePoisonLevel(const Zombie* zombie);
bool AddNativePoisonLevel(Zombie* zombie);
bool SetNativePoison(Zombie* zombie,int ticks = kFusionPoisonDefaultDurationTicks);
void WarmNativePoison(Zombie* zombie);
// Called once from the canonical host Zombie update. Frozen/buttered actors
// still advance this source status; ghosts adopt metadata without ticking it.
void UpdateNativePoison(Zombie* zombie);
}
#endif
