#pragma once
#include "FusionDriver363Rules.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion {
struct NativeExtendedZombie313Hooks;
const NativeExtendedZombie313Hooks& GetNativeDriver363Hooks();
bool EnsureNativeDriver363Resources(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeDriver363(Zombie*);
bool UpdateNativeDriver363(Zombie*);
bool ApplyNativeDriver363Body(Zombie*,int damage);
bool NativeDriver363Caltrop(Zombie*);
// Invoke deadline before the ordinary Immobilized/UpdateActions gate.
bool UpdateNativeDriver363PendingDeath(Zombie*);
bool NativeDriver363ColliderEnabled(const Zombie*);
bool CanDriver363GraphEvent(Zombie*,FusionSourceGraph313Event,std::string* error=nullptr);
bool Driver363GraphEvent(Zombie*,FusionSourceGraph313Event);
bool Driver363GraphParameters(Zombie*,uint8_t,FusionSourceGraph313Parameters&);
}
