#pragma once
#include "FusionNextZombie343Rules.h"
#include "FusionNativeHeavyZombie313B.h"
namespace LawnFusion
{
// Called ONLY by the existing Heavy313B graph event owner. No new AI/timer.
bool ShootNativeCherryCatapult343(Zombie*,NativeHeavyZombie313BState&,Plant* target,
 float shootX,float shootY,const NativeHeavyZombie313BHooks&);
bool ShootNativeDrownCatapult343(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&);
bool ApplyNativeDrownCatapult343Body(Zombie*,NativeHeavyZombie313BState&,int,const NativeHeavyZombie313BHooks&);
}
