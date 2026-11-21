#ifndef LAWN_FUSION_NATIVE_269_REMAINDER_CALLBACKS_H
#define LAWN_FUSION_NATIVE_269_REMAINDER_CALLBACKS_H
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeLastPlants269.h"
// Normal-D2 native adapters. Unavailable source world/award/Eve transactions
// reject their preflight; they never consume a plant, reward draw or currency.
namespace LawnFusion
{
// Complete source138-outcome resource admission; one host-local Context audit.
bool NativeEve269PoolReady(Board*);
// Real source Die1/FX/draw/spawn/card transaction. committed reports actor
// lifecycle consumption; a null child can be an actual dropped-card fallback.
Plant* ReplaceNativeEve269Plant(Plant*,bool& committed);
}
#endif
