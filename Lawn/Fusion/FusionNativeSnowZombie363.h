#pragma once
#include "FusionSnowZombie363Rules.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
bool EnsureNativeSnowZombie363Resources(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeSnowZombie363(Zombie*);
bool UpdateNativeSnowZombie363Attribute(Zombie*);
bool ApplyNativeSnow363FirstArmor(Zombie*,int damage,int& remaining);
}
