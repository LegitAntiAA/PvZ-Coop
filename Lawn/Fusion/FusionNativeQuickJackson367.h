#pragma once
#include "FusionQuickJackson367Rules.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion {
// Root implements this one shared overload in the existing embedded effect
// cache (same owner/node/emission mechanism used by Qing208). No new clock.
bool SetNativeSourceZombieEmission360(Zombie*,const char* sourceNode,bool enabled);
bool EnsureNativeQuickJackson367Resources(Board*,std::string* error=nullptr);
bool InitializeNativeQuickJackson367Presentation(Zombie*);
bool FinishNativeQuickJackson367MoonWalk(Zombie*);
bool StepNativeQuickJackson367Position(Zombie*,bool& stopMoonWalk);
bool ResolveNativeQuickJackson367RawDamage(const Zombie*,int raw,bool fix,int& preprocessed);
}
