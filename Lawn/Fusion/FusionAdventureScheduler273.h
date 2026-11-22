#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace LawnFusion
{
// Host-private state over the existing native actors, not a second simulation.
// Kinds are generated at initialization; rows are drawn when that wave spawns.
struct FusionAdventureScheduler273State
{
    int level=0,cycle=0,sourceDifficulty=0,rows=0,wave=0,maximumWaves=0;
    int coopDifficulty=1;
    float countdown=0,hold=0,healthElapsed=0,hugeElapsed=0;
    float currentHealth=0,latestWaveHealth=0;
    bool initialized=false,hugeWarning=false,failed=false;
    std::vector<std::vector<uint16_t>> waves;
};
struct FusionAdventureScheduler273Random
{
    void* user=nullptr;
    // Source Unity integer range: minimum inclusive, maximum exclusive.
    int (*integer)(void*,int minimum,int maximum)=nullptr;
    float (*real)(void*,float minimum,float maximum)=nullptr;
};
struct FusionAdventureScheduler273Spawn
{
    uint16_t kind;
    uint8_t row;
};
struct FusionAdventureScheduler273Input
{
    float deltaSeconds=0;
    bool host=false,paused=false,tutorial=false,iceDoom=false;
    bool allowSummon=true,boss=false,scaryPot=false,over=false;
};
struct FusionAdventureScheduler273Callbacks
{
    void* user=nullptr;
    FusionAdventureScheduler273Random random;
    // Include dying, non-destroyed bodies; exclude only mind-controlled actors.
    bool (*health)(void*,float& sourceIntegerSum)=nullptr;
    // Capacity/resources for the entire wave, before row RNG or actor writes.
    bool (*canSpawn)(void*,std::span<const uint16_t> kinds)=nullptr;
    // Rows have all been drawn before this call. Return actual birth HP sum.
    // A failure latches state.failed; never retry a partially emitted batch.
    bool (*spawn)(void*,int sourceWave,std::span<const FusionAdventureScheduler273Spawn>,float& initialHealth)=nullptr;
};
enum class FusionAdventureScheduler273Result
{
    Okay,Invalid,NotHost,NotReady,RandomFailed,HealthFailed,SpawnFailed,Failed
};
struct FusionAdventureScheduler273Events
{
    bool healthSampled=false,hugeWarning=false,firstWave=false,finalWave=false;
    int spawnedWave=0;
    uint32_t spawnedCount=0;
};
struct FusionAdventureScheduler273Health
{
    float body;
    int firstArmor,secondArmor;
    bool mindControlled=false;
};

// Ready(kind) must cover actual AI/factory/terrain/resources, including final-
// only kinds and Normal0 budget fallback. Checked before any RNG consumption.
FusionAdventureScheduler273Result InitializeFusionAdventureScheduler273(
    FusionAdventureScheduler273State&,bool host,int sourceLevel,int cycle,int sourceDifficulty,
    const FusionAdventureScheduler273Random&,void* readinessUser,bool (*ready)(void*,uint16_t),int coopDifficulty=1);
FusionAdventureScheduler273Result UpdateFusionAdventureScheduler273(
    FusionAdventureScheduler273State&,const FusionAdventureScheduler273Input&,
    const FusionAdventureScheduler273Callbacks&,FusionAdventureScheduler273Events&);
bool FusionAdventureScheduler273TutorOver(FusionAdventureScheduler273State&);
// Exact source health cache: sum I32(int(body)+armor1+armor2), then cast to F32.
bool TrySumFusionAdventureHealth273(std::span<const FusionAdventureScheduler273Health>,float& sum);
// Source wave birth accumulation casts each actor I32 total to F32 first.
bool TrySumFusionAdventureBirthHealth273(std::span<const FusionAdventureScheduler273Health>,float& sum);
}
