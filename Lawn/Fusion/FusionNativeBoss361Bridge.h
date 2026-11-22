#pragma once
#include "FusionNativeBoss361.h"
namespace LawnFusion
{
// NativeContext owns the canonical/controller state by actual LOCAL generation.
// A client's source token stays opaque and is never treated as a local UID.
NativeBoss361State* EditNativeBoss361State(Zombie*,bool create=false);
const NativeBoss361State* GetNativeBoss361State(const Zombie*);
// Native BossSetupReanim already owns body+driver. Add only the genuine RV
// native clock and its actual Attachment, after whole 3R/2A birth preflight.
bool PrepareNativeBoss361Clocks(Zombie*,std::string* error=nullptr);
void RemoveNativeBoss361State(Zombie*);
const NativeBoss361Hooks& GetNativeBoss361Hooks();
const NativeSourceGraph313Hooks& GetNativeBoss361GraphHooks();
bool CanSpawnNativeBoss361Actor(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeBoss361Actor(Zombie*);
bool UpdateNativeBoss361Actor(Zombie*);
bool AdoptNativeBoss361Actor(Zombie*,const FusionBoss361View&);
bool ApplyNativeBoss361ActorBodyDamage(Zombie*,int resolved);
bool BeginNativeBoss361ActorDeath(Zombie*);
bool GetNativeBoss361Semantics(const Zombie*,int& status,bool& colliderEnabled);
}
