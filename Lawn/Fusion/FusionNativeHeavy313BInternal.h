#pragma once
#include "FusionNativeHeavyZombie313BBridge.h"
namespace LawnFusion
{
// Root lifecycle boundary: sum real per-kind virtual Die callback peaks for
// the WHOLE selected list before the first conversion mutation or RNG.
bool CanRetireNativeSourceZombieBatch313(Board*,std::span<const uint32_t>,int sourceReason,std::string*);
}
namespace LawnFusion::Heavy313B
{
bool World(const Board*);bool Host(const Zombie*);bool Playing(const Zombie*);
uint16_t Kind(const Zombie*);void Failed(Board*,const char*);
Reanimation* Clock(Zombie*,uint8_t);
bool Pool(Board*,int zombies,int reanimations,int attachments,int projectiles,int coins);
bool CrashContacts(Zombie*);
bool GoldResources(Board*,uint16_t,std::string*);
bool GoldUpdate(Zombie*,NativeHeavyZombie313BState&);
bool GoldEvent(Zombie*,NativeHeavyZombie313BState&,FusionSourceGraph313Event);
bool GoldPrepareEvent(Zombie*,FusionSourceGraph313Event,std::string*);
bool GoldCanRetire(Zombie*,std::string*);
bool GoldBeforeRetire(Zombie*);
bool GoldDamage(Zombie*,int resolved,int type,bool dryRun,std::string*);
bool GoldArmorDamage(Zombie*,bool first,int damage,int& overflow);
bool GoldDirectFall(Zombie*);
}
