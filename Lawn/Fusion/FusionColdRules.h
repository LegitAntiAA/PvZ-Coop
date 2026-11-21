#ifndef LAWN_FUSION_COLD_RULES_H
#define LAWN_FUSION_COLD_RULES_H

#include <cstdint>

namespace LawnFusion
{
// Explicit damage delivery for non-projectile source callbacks. The default
// keeps the existing per-projectile policy; these values are not bullet IDs.
enum class FusionColdDamageDelivery : uint8_t { ProjectileProfile=0, Ice=2, IceAll=3, IceShieldless=5 };
bool FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery delivery, bool shieldPassed);
constexpr int kFusionColdFreezeTicks = 400;
constexpr int kFusionColdSlowTicks = 1000;
constexpr int kFusionColdFrozenSlowTicks = 1400;
constexpr int kFusionColdMaximumFreezeTicks = 500; // IceDoom's source SetFreeze(5).
constexpr int kFusionColdMaximumSlowTicks = 1500;
constexpr int kFusionIceShroomFuseTicks = 151;
struct FusionColdState
{
    int level = 0, threshold = 100, chilledTicks = 0, frozenTicks = 0;
};
struct FusionColdProjectileProfile
{
    uint16_t kind;
    int levelPerHit, maximumHits;
    bool coldThroughShield;
    int baseDamageMultiplier = 1;
};
struct FusionColdContactProfile
{
    uint16_t kind;
    double centerXFromShadow, centerYFromShadow, width, height;
    int freezeTicks;
};
const FusionColdContactProfile* FindFusionColdContactProfile(uint16_t kind);
bool FusionColdContactCanTrigger(uint16_t kind,int sourceStatus,bool sameRow,bool mindControlled,
    bool beforeDying,int frozenTicks);
bool FusionColdBurstCanDamage(int sourceStatus,bool mindControlled,int rowDelta,int maximumRowDelta);
const FusionColdProjectileProfile* FindFusionColdProjectileProfile(uint16_t kind);
bool FusionColdStateValid(int level, int threshold, int chilledTicks, int frozenTicks);
bool TrySetFusionColdFreeze(const FusionColdState& before, int ticks, FusionColdState& after);
bool TrySetFusionColdSlow(const FusionColdState& before, FusionColdState& after);
bool TryAddFusionColdLevels(const FusionColdState& before, int amount, FusionColdState& after);
bool TryPrepareFusionColdHit(uint16_t projectileKind, const FusionColdState& before,
    int baseDamage, FusionColdState& after, int& damage);
bool FusionColdAppliesAfterShield(uint16_t projectileKind, bool shieldPassed);
bool TryEndFusionColdFreeze(const FusionColdState& before, FusionColdState& after);
bool TryGetFusionColdMuzzle(uint16_t plantKind, float& sourceX, float& sourceY);
}
#endif
