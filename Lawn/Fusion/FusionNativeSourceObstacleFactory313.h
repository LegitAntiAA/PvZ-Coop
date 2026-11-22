#pragma once
#include "FusionSourceObstacle313.h"
#include "FusionSourceDeathBudget319.h"
#include "../../SexyAppFramework/Rect.h"
#include <string>
class Projectile;
namespace Sexy {class Graphics;}
namespace LawnFusion
{
bool EnsureNativeSourceObstacle313Resources(Board*,FusionSourceObstacle313Kind,uint8_t variant,std::string* error=nullptr);
bool PreflightNativeSourceObstacle313(Board*,const FusionSourceObstacle313View&,Projectile* existing,FusionSourceDeathBudget319& additional,std::string* error=nullptr);
bool AdoptNativeSourceObstacle313(Projectile*,const FusionSourceObstacle313View&,std::string* error=nullptr);
bool UpdateNativeSourceObstacle313(Projectile*);
bool DrawNativeSourceObstacle313(Sexy::Graphics*,Projectile*);
bool GetNativeSourceObstacle313Rect(const Projectile*,Sexy::Rect&);
void NativeSourceObstacle313Removing(Projectile*);
}
