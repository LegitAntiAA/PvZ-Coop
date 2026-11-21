#pragma once
#include "FusionAdventureScheduler273.h"
#include <string>
class Board;
namespace LawnFusion
{
// Bounded native world admission: real Day source1..6, cycle1. Source5 needs
// genuine100; source6 preflights the complete21->22->23->2 factory closure.
// Caller supplies the existing Board RNG callbacks; these never run on client.
FusionAdventureScheduler273Result InitializeNativeAdventureScheduler273(
    Board*,FusionAdventureScheduler273State&,int sourceLevel,int cycle,int sourceDifficulty,
    const FusionAdventureScheduler273Random&,std::string* error=nullptr);
FusionAdventureScheduler273Result UpdateNativeAdventureScheduler273(
    Board*,FusionAdventureScheduler273State&,const FusionAdventureScheduler273Input&,
    const FusionAdventureScheduler273Random&,FusionAdventureScheduler273Events&,
    std::string* error=nullptr);
}
