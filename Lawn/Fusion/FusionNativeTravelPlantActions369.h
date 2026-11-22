#pragma once
#include "FusionNativeZombies.h"
#include <vector>
class Plant;
namespace LawnFusion {
// Capture the source row/Shadow tuples before real Plant.Die0. Whole native
// birth capacity/resources precede that destructive call. No parent ownership.
bool PrepareNativeHypnoDoom369(const Plant*,std::vector<NativeSourceZombieSpawnRequest>&);
bool CommitNativeHypnoDoom369(Board*,const std::vector<NativeSourceZombieSpawnRequest>&);
}
