#pragma once
#include "FusionSnowGun363Rules.h"
#include <string>
class Board;class Zombie;class Plant;class Projectile;
namespace LawnFusion {
bool EnsureNativeSnowGun363Resources(Board*,std::string* error=nullptr);
bool InitializeNativeSnowGun363(Zombie*);
bool UpdateNativeSnowGun363(Zombie*);
bool NativeSnowGun363Shooting(const Zombie*);
bool CanShootNativeSnowGun363(Zombie*,std::string* error=nullptr);
bool ShootNativeSnowGun363(Zombie*);
bool IsNativeSnowBall363(const Projectile*);
bool UpdateNativeSnowBall363(Projectile*);
bool EnsureNativeSnowBall363Resources(Board*,std::string* error=nullptr);
}
