#pragma once
#include "FusionCampaign272Rules.h"
#include "FusionAdventureScheduler273.h"
#include <vector>

namespace LawnFusion
{
// One imported source descriptor over the existing Board, bank and scheduler.
// Building this immutable plan does not claim native/controller readiness.
struct FusionCampaign342Plan
{
    const FusionSourceAdventure272* source=nullptr;
    int maximumWaves=0;
    uint8_t waterRows=0;
    bool night=false;
    std::vector<uint16_t> possibleKinds; // Includes Normal0 budget fallback and row variants.
};
bool BuildFusionCampaign342Plan(int level,uint32_t cycle,FusionCampaign342Plan&,
    std::string* error=nullptr);
// Actual factory/world capability gate, separate from imported data and profile unlocks.
bool NativeSourceAdventureLevelReady342(int level,uint32_t cycle,std::string* error=nullptr);
// Source NewZombieUpdate repairs the pre-shuffled row before SetZombie; it
// never filters the roster or treats a null factory result as a skipped birth.
// hasIce is a read-only row view. Caller owns RNG; output is atomic on failure.
bool ResolveFusionCampaign342Spawn(const FusionCampaign342Plan&,uint16_t kind,int initialRow,
    std::span<const uint8_t> hasIce,const FusionAdventureScheduler273Random&,
    FusionAdventureScheduler273Spawn& out);
// Possible factory identities for ONE scheduled source kind, not a weighted pool.
bool FusionCampaign342SpawnKinds(const FusionCampaign342Plan&,uint16_t,
    std::vector<uint16_t>& out);
}
