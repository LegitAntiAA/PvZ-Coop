#pragma once
#include "FusionNativeNightZombie281.h"
#include <string>
class Board;
class Zombie;
namespace LawnFusion
{
NativeNightZombie281State* EditNativeJackson291(Zombie*,bool create=false);
const NativeNightZombie281State* GetNativeJackson291(const Zombie*);
// Resource/whole-current-batch admission before RNG; source Intro is permitted.
bool CanSpawnNativeJackson291(Board*,std::string* error=nullptr);
bool CanSpawnNativeJackson291(Board*,uint16_t sourceKind,std::string* error=nullptr);
bool InitializeNativeJackson291(Zombie*);
const NativeNightZombie281Hooks& GetNativeJackson291Hooks();
// Consuming typed10 wrappers. Ghosts/paused actors never tick source gameplay.
bool UpdateNativeJackson291(Zombie*);
bool UpdateNativeJackson291Animation(Zombie*);
bool StartNativeJackson291Walk(Zombie*);
bool MoveNativeJackson291(Zombie*);
// Root invokes BEFORE stock PlayDeathAnim resets the outgoing raw body span.
bool BeginNativeJackson291Death(Zombie*);
bool GetNativeJackson291Presentation(const Zombie*,const char*&,float&);
}
