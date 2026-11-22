#pragma once
#include "FusionNativeWaterZombie313.h"
#include <string>
namespace LawnFusion
{
bool EnsureNativeWaterShoot343Resources(Board*,uint16_t kind,std::string* error=nullptr);
bool InitializeNativeWaterShoot343(Zombie*,NativeWaterZombie313State&);
bool UpdateNativeWaterShoot343(Zombie*,NativeWaterZombie313State&);
bool CaptureNativeWaterShoot343(const Zombie*,const NativeWaterZombie313State&,FusionWaterZombie313View&);
bool AdoptNativeWaterShoot343(Zombie*,NativeWaterZombie313State&,const FusionWaterZombie313View&);
void RemoveNativeWaterShoot343(Zombie*,NativeWaterZombie313State&);
// All Draw/Node/Box consumers use this exact genuine additive layer2 pose.
bool GetNativeWaterShoot343Presentation(const Zombie*,const char*& incoming,float& incomingSeconds,
    const char*& outgoing,float& outgoingSeconds,float& incomingWeight);
}
