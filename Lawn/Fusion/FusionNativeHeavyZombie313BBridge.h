#pragma once
#include "FusionNativeHeavyZombie313B.h"
#include "FusionHeavyGold313BView.h"
namespace LawnFusion
{
bool GetNativeHeavyGold313BView(const Zombie*,FusionHeavyGold313BView&);
bool ApplyNativeHeavyGold313BPost(Zombie*,int awardLevel,int sourceRound,int sourceDifficulty);
// Only full LOCAL DataArray generations index these maps. Host wire tokens
// are validated metadata, never substituted for a client's local generation.
NativeHeavyZombie313BState* EditNativeHeavyZombie313B(Zombie*,bool create=false);
const NativeHeavyZombie313BState* GetNativeHeavyZombie313B(const Zombie*);
const NativeHeavyZombie313BHooks& GetNativeHeavyZombie313BHooks();
const NativeSourceGraph313Hooks& GetNativeHeavyZombie313BGraphHooks();
bool EnsureNativeHeavyZombie313BResources(Board*,uint16_t,std::string* error=nullptr);
bool CanSpawnNativeHeavyZombie313B(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeHeavyZombie313BActor(Zombie*);
bool UpdateNativeHeavyZombie313BActor(Zombie*);
// Complete raw/FNS validation precedes this no-RNG hydration. Actual body/aux
// clocks are supplied by Root's raw native animation adoption first.
bool AdoptNativeHeavyZombie313B(Zombie*,const FusionHeavyZombie313BView&,const FusionHeavyGold313BView&);
bool BeginNativeHeavyZombie313BActorDeath(Zombie*);
bool NativeHeavyZombie313BActorContact(Zombie*,Plant*);
bool NativeHeavyZombie313BActorLoseObject(Zombie*,int sourceReason=0);
// Source equipment enums for the existing Magnet capture planner, distinct
// from carrier type. Successful real Coin capture calls ActorLoseObject.
bool GetNativeHeavyZombie313BEquipment(const Zombie*,int& first,int& second,int& unique);
bool NativeHeavyZombie313BActorCaltrop(Zombie*);
bool NativeHeavyZombie313BActorHeadLost(Zombie*);
bool NativeHeavyZombie313BRejectStatus(const Zombie*,bool mindControl,bool knockback,bool freezeOrButter);
bool NativeHeavyZombie313BRejectCold(const Zombie*);
bool ApplyNativeHeavyFreeze313B(Zombie*,int ticks);
// Source damage dispatch: callers pass an ALREADY common-resolved value.
// Only Charred entry accepts RAW and performs base GetDamage exactly once.
int ResolveNativeHeavyDamage313B(const Zombie*,int resolved);
bool ApplyNativeHeavyTakeDamage313B(Zombie*,int resolved,int sourceType);
bool ApplyNativeHeavyBodyDamage313B(Zombie*,int,unsigned nativeFlags);
bool ApplyNativeHeavyArmorDamage313B(Zombie*,bool first,int,int& overflow);
bool UsesNativeHeavyCharred313B(const Zombie*);
bool CanApplyNativeHeavyCharred313B(Zombie*,int raw,bool fix,std::string* error=nullptr);
bool ApplyNativeHeavyCharred313B(Zombie*,int raw,bool fix);
bool CanRetireNativeHeavyZombie313B(Zombie*,int sourceReason,std::string* error=nullptr);
// Family leaves only. Root may use this before its common source-base Die
// bookkeeping/final native removal; the actor remains alive on success.
bool BeforeRetireNativeHeavyZombie313B(Zombie*,int sourceReason);
bool RetireNativeHeavyZombie313B(Zombie*,int sourceReason);
// Source semantic state for collider/status/Charred; no native NORMAL fallback.
bool GetNativeHeavyZombie313BSemantics(const Zombie*,int& status,bool& beforeDying,bool& colliderEnabled,bool& rootFlip);
}
