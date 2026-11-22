#pragma once
#include "FusionTravelBuff360Rules.h"
#include <string>
class Board;
namespace LawnFusion {
struct NativeTravelBuff360Hooks {
 // Prepare actual query/apply support for EVERY possible pool entry, no
 // filtering. In particular all plant/global consumers must read this ledger.
 bool (*prepare)(Board*,FusionTravelBuff360Award,std::string*)=nullptr;
 // Infallible-after-prepare transaction. advanced10:+20% current plant damage;
 // advanced20:SetMaxLightLevel4; advanced25:GetSun3000(type2,flagtrue);
 // ultimate24:Supplement every existing UltimatePumpkin(911/922 derivatives).
 // Source ADVANCED publishes its bit AFTER immediate effect; ULTIMATE BEFORE.
 // Candidate ledger allows callbacks to observe exactly that source ordering.
 void (*applyImmediate)(Board*,FusionTravelBuff360Award,const FusionTravelBuff360Ledger& visibleLedger)=nullptr;
 void (*showAdvice)(Board*,FusionTravelBuff360Award,float seconds)=nullptr;
};
bool PreflightNativeTravelBuff360(Board*,const FusionTravelBuff360Ledger&,const NativeTravelBuff360Hooks&,std::string* error=nullptr);
// ALL fallible source outcome support is prepared before either authoritative
// native RNG draw. No filtered pool or false-award callback.
bool UnlockNativeTravelBuff360(Board*,FusionTravelBuff360Ledger&,float& boardCooldownSeconds,int survivalRound,
 const NativeTravelBuff360Hooks&,bool& awarded,std::string* error=nullptr);
}
