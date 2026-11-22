#pragma once
#include "FusionNative313Snapshot.h"
#include "FusionSourceDeathBudget319.h"
#include <optional>
#include <span>
#include <string>
class Zombie;class Board;
namespace LawnFusion
{
// Borrowed/copyable decoded canonical data, not another actor/clock container.
struct Native313Snapshot
{
    std::optional<NativeSourceGraph313Snapshot> graph;
    std::optional<FusionWaterZombie313View> water;
    std::optional<NativeHeavyZombie313Snapshot> heavy;
    std::optional<FusionExtendedZombie313View> extended;
};
inline bool Native313SnapshotValid(uint16_t kind,const Native313Snapshot& v)
{
    const bool water=FusionWaterZombie313Kind(kind),heavy=NativeHeavyZombie313BKind(kind);
    const bool extended=FindFusionExtendedZombie313Profile(kind)!=nullptr;
    if(unsigned(water)+unsigned(heavy)+unsigned(extended)!=1||v.water.has_value()!=water||
       v.heavy.has_value()!=heavy||v.extended.has_value()!=extended||v.graph.has_value()!=(heavy||extended))return false;
    return (!water||Native313WaterViewValid(kind,*v.water))&&(!heavy||Native313HeavyViewValid(kind,*v.heavy))&&
        (!extended||Native313ExtendedViewValid(kind,*v.extended))&&(!v.graph||Native313GraphViewValid(kind,*v.graph));
}
bool CaptureNative313(const Zombie*,Native313Snapshot& out,std::string* error=nullptr);
// Exact existing GetFramesForLayer range, read from the loaded definition.
bool Native313ClockTrackMatches(uint16_t nativeType,const char* track,const LawnNet::NetZombieAnimState&);
struct Native313GhostRequest
{
    Zombie* existing=nullptr; // Root-resolved LOCAL full generation, null for a new raw actor.
    uint16_t kind=0;
    LawnNet::NetZombieAnimState body;
    const Native313Snapshot* snapshot=nullptr; // Synchronous borrowed decoded data.
};
// Does not inspect/change the current scene/rows: Root validates the incoming
// world and raw ID/token joint references before configuring that world.
// Loads resources but does not allocate actors/R/A, tick, play callbacks or RNG.
// Additional counts ONLY missing family auxiliary R/A, not raw Zombie/body.
// Root combines these with ALL raw/314/projectile budgets and checks ONE Fits.
bool PreflightNative313GhostBatch(Board*,std::span<const Native313GhostRequest>,
    FusionSourceDeathBudget319& additional,std::string* error=nullptr);
// Client only, AFTER Root's combined preflight + raw body + source metadata.
// Same synchronous commit, no message pump/factories between preflight/adopt.
// Initializes canonical views and real auxiliary clocks, never source Start.
bool AdoptNative313(Zombie*,const Native313Snapshot&,std::string* error=nullptr);
}
