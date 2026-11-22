#include "FusionColdRules.h"
#include "FusionBasePlantRules.h"
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr FusionColdProjectileProfile profiles[] = {{15,10,1,false},{18,5,3,true},{21,10,1,false},{24,5,3,true},{97,20,1,false,2},{101,10,1,false}};
constexpr FusionColdContactProfile contactProfiles[]={{1036,-.0028865205,.227090355,.48337584,.557946645,400}};
bool Valid(const FusionColdState& state)
{
    return FusionColdStateValid(state.level,state.threshold,state.chilledTicks,state.frozenTicks);
}
}
const FusionColdProjectileProfile* FindFusionColdProjectileProfile(uint16_t kind)
{
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    return nullptr;
}
const FusionColdContactProfile* FindFusionColdContactProfile(uint16_t kind)
{
    for (const auto& profile:contactProfiles) if (profile.kind==kind) return &profile;
    return nullptr;
}
bool FusionColdContactCanTrigger(uint16_t kind,int status,bool sameRow,bool mindControlled,bool beforeDying,int frozen)
{
    return FindFusionColdContactProfile(kind) && sameRow && frozen==0 &&
        FusionPlantSearchUniqueZombieAllowed(status,mindControlled,beforeDying);
}
bool FusionColdBurstCanDamage(int status,bool controlled,int rowDelta,int maximumRowDelta)
{
    if (controlled || maximumRowDelta<0 || rowDelta < -maximumRowDelta || rowDelta>maximumRowDelta || status<0) return false;
    switch (status)
    {
    case 1: case 7: case 10: case 12: case 16: case 20: case 21: case 22: case 23: case 24: return false;
    default: return true;
    }
}
bool FusionColdStateValid(int level, int threshold, int chilled, int frozen)
{
    // Frozen targets keep accumulating level, including beyond their threshold.
    return level >= 0 && (threshold == 100 || threshold == 200 || threshold == 300 || threshold == 400) &&
        chilled >= 0 && chilled <= kFusionColdMaximumSlowTicks && frozen >= 0 && frozen <= kFusionColdMaximumFreezeTicks;
}
bool TrySetFusionColdFreeze(const FusionColdState& before, int ticks, FusionColdState& after)
{
    if (!Valid(before) || ticks <= 0 || ticks > kFusionColdMaximumFreezeTicks) return false;
    auto next = before;
    if (before.frozenTicks == 0)
    {
        next.frozenTicks = ticks;
        next.chilledTicks = kFusionColdSlowTicks+ticks;
    }
    else if (ticks > next.frozenTicks) next.frozenTicks = ticks;
    after = next;
    return true;
}
bool TrySetFusionColdSlow(const FusionColdState& before, FusionColdState& after)
{
    if (!Valid(before)) return false;
    auto next = before;
    next.chilledTicks = kFusionColdSlowTicks; // Source overwrite, including14→10.
    after = next;
    return true;
}
bool TryAddFusionColdLevels(const FusionColdState& before, int amount, FusionColdState& after)
{
    if (!Valid(before) || amount <= 0 || before.level > (std::numeric_limits<int>::max)()-amount) return false;
    auto next = before;
    next.level += amount;
    if (next.frozenTicks == 0 && next.level >= next.threshold)
    {
        next.level = 0;
        if (next.threshold < 400) next.threshold += 100;
        if (!TrySetFusionColdFreeze(next,kFusionColdFreezeTicks,next)) return false;
    }
    after = next;
    return true;
}
bool TryPrepareFusionColdHit(uint16_t kind, const FusionColdState& before,
    int baseDamage, FusionColdState& after, int& damage)
{
    const auto* profile = FindFusionColdProjectileProfile(kind);
    FusionColdState next;
    if (!profile || baseDamage <= 0 || !TryAddFusionColdLevels(before,profile->levelPerHit,next)) return false;
    const int multiplier=profile->baseDamageMultiplier*(next.frozenTicks>0 ? 4 : 1);
    if (baseDamage > (std::numeric_limits<int>::max)()/multiplier) return false;
    const int resolved = baseDamage*multiplier;
    after = next;
    damage = resolved;
    return true;
}
bool FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery delivery, bool shieldPassed)
{
    switch (delivery)
    {
    case FusionColdDamageDelivery::Ice: return shieldPassed;
    case FusionColdDamageDelivery::IceAll:
    case FusionColdDamageDelivery::IceShieldless: return true;
    default: return false;
    }
}
bool FusionColdAppliesAfterShield(uint16_t kind, bool shieldPassed)
{
    // WinterMelon applies IceAll through shields, while its accumulation is
    // ordered separately by the primary/splash hit. It is not a Pea profile.
    if (kind == 60) return true;
    const auto* profile = FindFusionColdProjectileProfile(kind);
    return profile && (profile->coldThroughShield || shieldPassed);
}
bool TryEndFusionColdFreeze(const FusionColdState& before, FusionColdState& after)
{
    if (!Valid(before)) return false;
    after = before;
    after.frozenTicks = 0;
    after.level = 0;
    return true;
}
bool TryGetFusionColdMuzzle(uint16_t kind, float& x, float& y)
{
    struct Muzzle { uint16_t kind; float x,y; };
    // ART109: actual prefab matrices plus callback's +.3sourceX, relativeShadow.
    constexpr Muzzle muzzles[] = {{1034,.667999977f,.66355f},{1105,.667999977f,.66355f},
        {1106,.6840998597f,.707249862f},{1035,.4695f,.0885f},{908,.48802f,.09911f}};
    for (const auto& muzzle : muzzles)
        if (muzzle.kind == kind) { x=muzzle.x; y=muzzle.y; return true; }
    return false;
}
}
