#pragma once
#include "FusionNativeRandom360.h"
#include "FusionNativeTravelBuff360.h"
class Board;class Zombie;
namespace LawnFusion {
const NativeRandom360Hooks& GetNativeRandomTravel369Hooks();
const NativeTravelBuff360Hooks& GetNativeTravel369Hooks();
bool PreflightNativeRandomTravel369(Board*,std::string* error=nullptr);
bool UnlockNativeRandomTravel369(Zombie*,bool& awarded,std::string* error=nullptr);
// Reliable Advice receiver may call this once for the committed award; no RNG,
// ledger mutation or repeating observer on every FNS snapshot.
void ShowNativeTravelAward369(Board*,FusionTravelBuff360Award,float seconds=10);
}
