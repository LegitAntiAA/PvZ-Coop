#pragma once
#include "FusionAdventureScheduler273.h"
#include <string>
class Board;
namespace LawnFusion
{
// Imported source descriptor admission, including source replay cycles. All possible row variants and
// actual transitive factories/resources are prepared before any wave-plan RNG.
// Caller supplies the existing Board RNG callbacks; these never run on client.
FusionAdventureScheduler273Result InitializeNativeAdventureScheduler273(
    Board*,FusionAdventureScheduler273State&,int sourceLevel,int cycle,int sourceDifficulty,
    const FusionAdventureScheduler273Random&,std::string* error=nullptr,int coopDifficulty=1);
FusionAdventureScheduler273Result UpdateNativeAdventureScheduler273(
    Board*,FusionAdventureScheduler273State&,const FusionAdventureScheduler273Input&,
    const FusionAdventureScheduler273Random&,FusionAdventureScheduler273Events&,
    std::string* error=nullptr);
}
