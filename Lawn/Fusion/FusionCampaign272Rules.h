#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace LawnFusion
{
// Source IDs remain independent of native Adventure numbers and carrier seeds.
struct FusionSourceAdventure272
{
    uint16_t level;
    uint8_t scene, rows, columns;
    uint16_t firstCycleWaves;
    int initialSun, initialMoney;
    std::span<const uint16_t> allowedZombieKinds;
};
struct FusionSourceChallenge272
{
    uint16_t level;
    const char* enumName; // Source identity, not localized user-facing text.
    int scene; // -1: source CustomBlockMgr data is required.
    uint16_t maximumWaves; //0: dynamic custom map, never a playable default.
    bool requiresCustomMap;
};
struct FusionSourceZombieWeight272
{
    uint16_t kind;
    int weight, cost;
};
struct FusionSourceNativeProjection272
{
    int layoutLevel, background;
};
struct FusionSourceWavePick272
{
    uint16_t emittedKind;
    int remainingBudget;
};

const FusionSourceAdventure272* FindFusionSourceAdventure272(int level);
const FusionSourceChallenge272* FindFusionSourceChallenge272(int level);
std::span<const FusionSourceZombieWeight272> FusionSourceZombieWeights272();
bool TryGetFusionSourceAdventureWaves272(int level,int cycle,int& waves);
// Native layout reuse only. This number must NOT determine source progress,
// unlocks, source wave generation, source tutorial or reward completion.
bool TryProjectFusionSourceAdventure272(int level,FusionSourceNativeProjection272& projection);
// Source base and sparse special-card IDs; challenge/achievement-only rules
// require their own completion state and are not inferred from Adventure.
bool TryGetFusionSourceBaseUnlock272(uint16_t kind,int& completedAdventureLevel);
bool FusionSourceAdventureAllowsZombie272(int level,uint16_t kind);
bool TryGetFusionSourceWaveBudget272(int level,int cycle,int wave,int sourceDifficulty,int& budget);
bool TryGetFusionSourceZombieWeightTotal272(int level,int& total);
// Caller owns the source integer RNG draw in [1,totalWeight]. No local RNG.
bool TryPickFusionSourceAdventureZombie272(int level,int draw,uint16_t& kind);
// If chosenCost exceeds the remaining budget, source Adventure emits Normal0
// but subtracts the ORIGINAL chosen cost. No unadmitted-NPC fallback is implied.
bool TryResolveFusionSourceAdventurePick272(int remaining,uint16_t chosen,FusionSourceWavePick272& pick);
// Source final wave appends each allow-listed kind AFTER the weighted fill.
// These include final-only actors without any ordinary spawn weight.
bool TryGetFusionSourceFinalWaveKinds272(int level,int cycle,int wave,std::span<const uint16_t>& kinds);
}
