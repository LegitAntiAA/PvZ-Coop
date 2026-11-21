#ifndef LAWN_FUSION_ZOMBIE_BATCH270_RULES_H
#define LAWN_FUSION_ZOMBIE_BATCH270_RULES_H

#include "../../ConstEnums.h"
#include <cstddef>
#include <cstdint>

namespace LawnFusion
{
// Source class families, not aliases of the stock ZombieType carrier.
enum class FusionZombieBatch270Family:uint8_t
{
    Paper,PoleVault,Football,Dance,Submarine,Driver,Snorkel,Dolphin,Drown,Doll,
    Jackbox,Balloon,Airship,Digger,Catapult,Pogo,Ladder,Gargantuar,Imp,
    IronGargantuar,MachineNut,Bungee,PeaHead,CherryHead,WallNutArmor
};

enum FusionZombieBatch270Capability:uint64_t
{
    Zombie270Model=uint64_t{1}<<0,
    Zombie270LifecycleEffects=uint64_t{1}<<1,
    Zombie270BodyMotion=uint64_t{1}<<2,
    Zombie270BodyDamage=uint64_t{1}<<3,
    Zombie270SourceOverrides=uint64_t{1}<<4,
    Zombie270NativeFactory=uint64_t{1}<<5,
    Zombie270ReplicatedState=uint64_t{1}<<6,
    Zombie270PairedRuntimeProof=uint64_t{1}<<7,
    Zombie270BiteEvents=uint64_t{1}<<8,
    Zombie270FirstArmor=uint64_t{1}<<9,
    Zombie270SecondArmor=uint64_t{1}<<10,
    Zombie270Vault=uint64_t{1}<<11,
    Zombie270SummonRoster=uint64_t{1}<<12,
    Zombie270Water=uint64_t{1}<<13,
    Zombie270CrashContact=uint64_t{1}<<14,
    Zombie270PaperTransition=uint64_t{1}<<15,
    Zombie270PaperMotion=uint64_t{1}<<16,
    Zombie270HeadLayer=uint64_t{1}<<17,
    Zombie270SourceProjectile=uint64_t{1}<<18,
    Zombie270BombCherryStart=uint64_t{1}<<19,
    Zombie270IceTerrain=uint64_t{1}<<20,
    Zombie270StatusOverrides=uint64_t{1}<<21,
    Zombie270Dive=uint64_t{1}<<22,
    Zombie270CaptureThrow=uint64_t{1}<<23,
    Zombie270FuseAndExplosion=uint64_t{1}<<24,
    Zombie270FlightAndDrop=uint64_t{1}<<25,
    Zombie270AirshipBomb=uint64_t{1}<<26,
    Zombie270DigAndSurface=uint64_t{1}<<27,
    Zombie270EquipmentCallbacks=uint64_t{1}<<28,
    Zombie270CatapultAimAmmo=uint64_t{1}<<29,
    Zombie270Pogo=uint64_t{1}<<30,
    Zombie270Ladder=uint64_t{1}<<31,
    Zombie270GargSmash=uint64_t{1}<<32,
    Zombie270ImpThrow=uint64_t{1}<<33,
    Zombie270SuperBall=uint64_t{1}<<34,
    Zombie270ThrownLanding=uint64_t{1}<<35,
    Zombie270IronBody=uint64_t{1}<<36,
    Zombie270MachineType=uint64_t{1}<<37,
    Zombie270Bungee=uint64_t{1}<<38,
    Zombie270ArmorNoFallStages=uint64_t{1}<<39
};
constexpr uint64_t kFusionZombieBatch270KnownCapabilities=(uint64_t{1}<<40)-1;

struct FusionZombieBatch270Profile
{
    uint16_t kind;
    FusionZombieBatch270Family family;
    ZombieType carrier;
    int maxHealth,firstArmor,secondArmor,biteDamage;
    const char* sourceClass;
};

// Source InitZombieData, preset2; no co-op health scaling and no selection RNG.
// carrier is only a real allocation candidate. Running its stock AI is not a
// substitute for this source family's required controllers.
const FusionZombieBatch270Profile* FindFusionZombieBatch270Profile(uint16_t kind);
const FusionZombieBatch270Profile* FusionZombieBatch270ProfileAt(std::size_t index);
std::size_t FusionZombieBatch270ProfileCount();
bool FusionZombieBatch270SourceStatsMatch(uint16_t kind,int maximumHealth,
    int firstArmor,int secondArmor,int biteDamage);
bool TryGetFusionZombieBatch270ArmorLayers(uint16_t kind,uint8_t& layers);
uint64_t FusionZombieBatch270RequiredCapabilities(uint16_t kind);
// This pure gate consumes actual integration/evidence supplied by Root. It
// discovers no implementation, publishes no availability, and creates no actor.
bool TryGetFusionZombieBatch270MissingCapabilities(uint16_t kind,uint64_t actual,uint64_t& missing);
bool FusionZombieBatch270CapabilitiesReady(uint16_t kind,uint64_t actual);
}

#endif
