#pragma once
#include "FusionNativeHeavyZombie313B.h"
namespace LawnFusion
{
enum class NativeBossBungee366Start:uint8_t {NotLinked,Started,Failed};
// Native Source45 initialization deliberately defers Start until its first
// Update. Configure is legal only in that unadvanced full-generation window.
bool CanEmitNativeBossBungeeTriple366(Zombie*,std::string* error=nullptr);
bool EmitNativeBossBungeeTriple366(Zombie*,std::string* error=nullptr);
bool ConfigureNativeBossBungee366(Zombie* child,Zombie* boss,int column);
NativeBossBungee366Start StartNativeBossBungee366(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&);
}
