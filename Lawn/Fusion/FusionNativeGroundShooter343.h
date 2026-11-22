#pragma once
#include "FusionGroundShooter343Rules.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
bool EnsureNativeGroundShooter343Resources(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeGroundShooter343(Zombie*);
bool UpdateNativeGroundShooter343Attribute(Zombie*);
bool CanShootNativeGround343(Zombie*,std::string* error=nullptr);
bool ShootNativeGround343(Zombie*);
// Return true only for these genuine source virtual armor implementations.
bool ApplyNativeGround343FirstArmor(Zombie*,int damage,int& remaining);
bool ApplyNativeGround343SecondArmor(Zombie*,int damage,int& remaining);
}
