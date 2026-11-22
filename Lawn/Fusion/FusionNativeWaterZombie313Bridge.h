#pragma once
#include "FusionNativeWaterZombie313.h"
#include "FusionWaterBridge321Rules.h"
namespace LawnFusion
{
NativeWaterZombie313State* EditNativeWaterZombie313(Zombie*,bool create=false);
const NativeWaterZombie313State* GetNativeWaterZombie313State(const Zombie*);
const NativeWaterZombie313Hooks& GetNativeWaterZombie313Hooks();
// Resource-only closure: no actor/RNG/capacity/playing-state or row admission.
bool EnsureNativeWaterZombie313Resources(Board*,uint16_t,std::string* error=nullptr);
// Actual family resources/whole birth capacity, before source RNG. Carrier
// definitions are clock-only ZAMBONI16/NORMAL17/19/20/27; no stock water AI.
bool CanSpawnNativeWaterZombie313(Board*,uint16_t,int row,bool controlled,std::string* error=nullptr);
bool CanAllocateNativeWaterZombie313Batch(Board*,std::span<const uint16_t>,std::string* error=nullptr);
bool InitializeNativeWaterZombie313Actor(Zombie*);
bool UpdateNativeWaterZombie313Actor(Zombie*);
bool AdoptNativeWaterZombie313Actor(Zombie*,const FusionWaterZombie313View&);
void RemoveNativeWaterZombie313Actor(Zombie*);
bool BeginNativeWaterZombie313ActorDeath(Zombie*,int sourceReason=0);
bool NativeWaterZombie313ActorHeadLost(Zombie*);
bool NativeWaterZombie313ActorCaltrop(Zombie*);
bool NativeWaterZombie313ActorSetFacing(Zombie*,bool right);
bool ApplyNativeWaterZombie313BodyDamage(Zombie*,int resolved);
// Leaf callbacks for Root's common source retirement dispatcher. These never
// recurse through the complete virtual Die dispatcher themselves.
bool CanRetireNativeWaterZombie313(Zombie*,int sourceReason,std::string* error=nullptr);
bool BeforeRetireNativeWaterZombie313(Zombie*,int sourceReason);
bool GetNativeWaterZombie313Semantics(const Zombie*,int& status,bool& beforeDying,
    bool& colliderEnabled,bool& sourceInWater,bool& rootFlip);
}
