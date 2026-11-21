#ifndef LAWN_FUSION_NATIVE_PRODUCER_H
#define LAWN_FUSION_NATIVE_PRODUCER_H

#include "FusionProducerRules.h"
class Plant;
namespace LawnFusion
{
bool IsNativeProducer(const Plant* plant);
void InitializeNativeProducer(Plant* plant);
// A bound producer is always handled, including clients and blocked gates.
bool UpdateNativeProducer(Plant* plant);
// Shared native Coin callback. All coins use the actor's current cell; a full
// native array rejects the entire batch before allocating any coin.
bool EmitNativeSunBatch(Plant* plant, int count, bool sound);
bool EmitNativeSourceSun(Plant* plant, uint16_t sourceItem, bool sound = true);
void NativeProducerHealthChanged(Plant* plant);
// Called by the first actual Plant.Die before mDead is set, not ghost cleanup.
void NativeProducerDying(Plant* plant);
}

#endif
