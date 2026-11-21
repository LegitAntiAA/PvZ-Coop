#ifndef LAWN_FUSION_ZOMBIE_RULES_H
#define LAWN_FUSION_ZOMBIE_RULES_H

#include <array>
#include <cstdint>

namespace LawnFusion
{
constexpr int kFusionZombieSourcePreset = 2;
constexpr float kFusionZombieMinimumOriginSpeed = .9f;
constexpr float kFusionZombieMaximumOriginSpeed = 1.8f;
constexpr float kFusionZombieWalk2Threshold = 1.35f;
constexpr float kFusionZombieAttackClipSeconds = 1.35f;
constexpr std::array<float,2> kFusionZombieAttackEvents = {.33333334f,1.1f};

struct FusionZombieProfile
{
    uint16_t kind;
    int maxHealth, firstArmor, secondArmor, biteDamage;
    bool nativeLocomotion=false,waterOnly=false,noAttack=false;
};
struct FusionZombieSpawn
{
    uint16_t kind = 0;
    int maxHealth = 0, firstArmor = 0, secondArmor = 0, biteDamage = 0;
    float originSpeed = 0;
    bool walk2 = false;
    bool operator==(const FusionZombieSpawn&) const = default;
};
const FusionZombieProfile* FindFusionZombieProfile(uint16_t kind);
// Profiles include staged source families. Native creation must also pass its
// complete-family capability/resource/wire gate; a profile is not admission.
// The caller supplies one source RNG sample. No RNG or co-op scaling occurs here.
bool TryPrepareFusionZombieSpawn(uint16_t kind, int sourcePreset, float originSpeedDraw, FusionZombieSpawn& output);
bool TryGetFusionZombieStatusSpeedFactor(uint16_t kind, bool chilled, bool frozen, bool buttered,
    bool garlicWaiting, float& output);

// Positions come from the existing authoritative attack/attack2 animation.
// An end position at clipDuration and next-loop zero describe the same instant.
struct FusionZombieClipPosition { uint32_t loop = 0; float seconds = 0; };
// Counts callbacks in (previous,current]; repeated/paused spans emit none.
// The caller retains target/phase authority; this query owns no animation clock.
bool TryCountFusionZombieAttackEvents(uint16_t kind, FusionZombieClipPosition previous,
    FusionZombieClipPosition current, uint32_t& output);

struct FusionZombieHealthStep
{
    float health = 0;
    int integerHealthProjection = 0;
    bool deathAtUpdateStart = false;
    bool operator==(const FusionZombieHealthStep&) const = default;
};
// Source Update checks (int)HP<1 BEFORE its optional headless subtraction.
// integerHealthProjection is source (int)HP, not a native-liveness policy.
// A native bridge must preserve source death timing even when this becomes0.
bool TryStepFusionZombieHealth(uint16_t kind, float health, int maximumHealth, bool headless,
    float deltaSeconds, FusionZombieHealthStep& output);
// Source BodyTakeDamage subtracts integer damage from the retained float HP.
// This is body arithmetic only, not armor, limb callbacks or a death transition.
bool TryApplyFusionZombieBodyDamage(uint16_t kind, float health, int damage, float& remainingHealth, int& integerHealthProjection);
// BodyTakeDamage0x180380280: arm compares HP < float(unchecked(max*2))/3f;
// head compares HP < float(max)/3f. Equality retains the corresponding part.
// Positive I32 max is required; source's unchecked doubling wraps deliberately.
// Unknown kinds/invalid max leave both outputs unchanged.
bool TryGetFusionZombieLimbThresholds(uint16_t kind, int maximumHealth, float& arm, float& head);

// BeSmall halves float HP while flooring the independent integer maximum.
// Repeated admitted halves can therefore retain a fraction above that maximum.
bool FusionZombieHealthFitsMaximum(float health, int maximumHealth);
struct FusionZombieShrinkHealth
{
    float health=0;
    int maximumHealth=0, livenessHealth=0;
    bool operator==(const FusionZombieShrinkHealth&) const = default;
};
// Transactional health part of source BeSmall0x18037FDD0. Only positive body
// health is halved; otherwise HP/max remain unchanged for pending source death.
bool TryShrinkFusionZombieHealth(uint16_t kind, float health, int maximumHealth, FusionZombieShrinkHealth& output);
}

#endif
