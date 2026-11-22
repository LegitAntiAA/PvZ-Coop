#pragma once
#include "FusionNativeDrownWeapon313.h"
namespace Sexy {class Graphics;}
namespace LawnFusion
{
const NativeDrownWeapon313Hooks& GetNativeDrownWeapon313Hooks();
bool EnsureNativeDrownWeapon313Visuals(Board*,std::string* error=nullptr);
bool DrawNativeDrownWeapon313(Sexy::Graphics*,Projectile*);
}
