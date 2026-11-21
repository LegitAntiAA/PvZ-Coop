#include "FusionZombieRules.h"
#include "SpecialZombie269Rules.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr FusionZombieProfile profiles[] = {
    {0,270,0,0,50},{2,270,370,0,50},{4,270,1100,0,50},
    {8,270,0,1100,50,true},{11,270,0,0,50,true,true},{12,270,370,0,50,true,true},
    {13,270,1100,0,50,true,true},{41,200,0,0,50,true,false,true},{42,300,0,0,50,true,false,true},
    {111,270,2200,0,50,false},{109,270,4400,0,50,false}};
bool ProjectSourceHealth(float health, int& output)
{
    if (!std::isfinite(health) || static_cast<double>(health)<(std::numeric_limits<int>::min)() ||
        static_cast<double>(health)>(std::numeric_limits<int>::max)()) return false;
    output=static_cast<int>(health);
    return true;
}
bool ClipPositionValid(FusionZombieClipPosition position,float duration)
{ return std::isfinite(position.seconds) && position.seconds>=0 && position.seconds<=duration; }
}
const FusionZombieProfile* FindFusionZombieProfile(uint16_t kind)
{ for (const auto& profile:profiles) if (profile.kind==kind) return &profile; return FindSpecialZombie269Profile(kind); }

bool TryPrepareFusionZombieSpawn(uint16_t kind, int sourcePreset, float draw, FusionZombieSpawn& output)
{
    const auto* profile=FindFusionZombieProfile(kind);
    if (!profile || sourcePreset!=kFusionZombieSourcePreset || !std::isfinite(draw) ||
        draw<kFusionZombieMinimumOriginSpeed || draw>kFusionZombieMaximumOriginSpeed) return false;
    output={kind,profile->maxHealth,profile->firstArmor,profile->secondArmor,profile->biteDamage,draw,
        kind!=111&&kind!=109&&draw>kFusionZombieWalk2Threshold};
    return true;
}

bool TryGetFusionZombieStatusSpeedFactor(uint16_t kind, bool chilled, bool frozen, bool buttered,
    bool garlicWaiting, float& output)
{
    if (!FindFusionZombieProfile(kind)) return false;
    output=(kind!=111&&frozen) || buttered || garlicWaiting ? 0.f : kind!=111&&chilled ? .5f : 1.f;
    return true;
}

bool TryCountFusionZombieAttackEvents(uint16_t kind, FusionZombieClipPosition previous,
    FusionZombieClipPosition current, uint32_t& output)
{
    const auto* profile=FindFusionZombieProfile(kind);
    const float duration=kind==109?2.9166667f:kFusionZombieAttackClipSeconds;
    if (!profile || profile->noAttack || !ClipPositionValid(previous,duration) || !ClipPositionValid(current,duration) ||
        current.loop<previous.loop || (current.loop==previous.loop && current.seconds<previous.seconds)) return false;
    uint64_t count=0;
    const std::array<float,2> callbacks=kind==109?std::array<float,2>{1.0166667f,2.4833333f}:
        profile->waterOnly?std::array<float,2>{.33333334f,.96666664f}:kFusionZombieAttackEvents;
    for (float event:callbacks)
    {
        const uint64_t before=static_cast<uint64_t>(previous.loop)+(previous.seconds>=event ? 1U:0U);
        const uint64_t after=static_cast<uint64_t>(current.loop)+(current.seconds>=event ? 1U:0U);
        count+=after-before;
    }
    if (count>(std::numeric_limits<uint32_t>::max)()) return false;
    output=static_cast<uint32_t>(count);
    return true;
}

bool TryStepFusionZombieHealth(uint16_t kind, float health, int maximumHealth, bool headless,
    float deltaSeconds, FusionZombieHealthStep& output)
{
    int before=0;
    if (!FindFusionZombieProfile(kind) || maximumHealth<0 || !std::isfinite(deltaSeconds) || deltaSeconds<0 ||
        !ProjectSourceHealth(health,before) || (maximumHealth==0 && before>=1)) return false;
    float after=health;
    if (headless)
    {
        // Zombie.Update0x18038BED0: dt*.1f, then *float(maxHP), then HP-loss.
        // Explicit F32 stores prevent reassociation into the different max*.1*dt.
        const volatile float fraction=deltaSeconds*.1f;
        const volatile float loss=fraction*static_cast<float>(maximumHealth);
        after=health-loss;
    }
    int mirror=0;
    if (!ProjectSourceHealth(after,mirror)) return false;
    output={after,mirror,before<1};
    return true;
}

bool TryApplyFusionZombieBodyDamage(uint16_t kind, float health, int damage, float& remainingHealth, int& integerHealthProjection)
{
    int before=0;
    if (!FindFusionZombieProfile(kind) || damage<0 || !ProjectSourceHealth(health,before)) return false;
    const float after=health-static_cast<float>(damage);
    int mirror=0;
    if (!ProjectSourceHealth(after,mirror)) return false;
    remainingHealth=after;
    integerHealthProjection=mirror;
    return true;
}

bool TryGetFusionZombieLimbThresholds(uint16_t kind, int maximumHealth, float& arm, float& head)
{
    if (!FindFusionZombieProfile(kind) || maximumHealth<=0) return false;
    const uint32_t doubled=static_cast<uint32_t>(maximumHealth)*uint32_t{2};
    const int64_t wrapped=(doubled & 0x80000000U) ? static_cast<int64_t>(doubled)-0x100000000LL : doubled;
    // Avoid signed-overflow UB and implementation-defined U32->I32 conversion.
    // Volatile operands retain the source F32 conversion/division operation.
    const volatile float armNumerator=static_cast<float>(wrapped);
    const volatile float headNumerator=static_cast<float>(maximumHealth), divisor=3.0f;
    const float armValue=armNumerator/divisor, headValue=headNumerator/divisor;
    arm=armValue; head=headValue;
    return true;
}

bool FusionZombieHealthFitsMaximum(float health, int maximumHealth)
{
    return maximumHealth>=0 && std::isfinite(health) &&
        static_cast<double>(health)>=(std::numeric_limits<int>::min)() &&
        static_cast<double>(health)<static_cast<double>(maximumHealth)+1.0;
}

bool TryShrinkFusionZombieHealth(uint16_t kind, float health, int maximumHealth, FusionZombieShrinkHealth& output)
{
    if (!FindFusionZombieProfile(kind) || !FusionZombieHealthFitsMaximum(health,maximumHealth)) return false;
    const float remaining=health>0 ? health*.5f : health;
    const int maximum=health>0 ? maximumHealth/2 : maximumHealth;
    if (!FusionZombieHealthFitsMaximum(remaining,maximum)) return false;
    output={remaining,maximum,remaining<=1.f ? 1 : static_cast<int>(std::ceil(static_cast<double>(remaining)))};
    return true;
}
}
