#include "FusionShooterTiming.h"
#include "FusionBatchActionRules.h"
#include "FusionRecipientShooterRules.h"
#include "FusionBasePlantRules.h"
#include "FusionShooterBatchRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionLobberRules.h"
#include "FusionCannon268Rules.h"
#include "FusionWater268Rules.h"
#include "FusionShulk268Rules.h"
#include "FusionAction266Rules.h"
#include "FusionAction267Rules.h"
#include "FusionUmbrellaBatchRules.h"
#include "FusionLastPlants269Rules.h"
#include "FusionWorldRemainder269Rules.h"
#include "FusionWorldActors269Rules.h"
#include "FusionStar269Rules.h"
#include "FusionDoom269Rules.h"
#include "FusionSummonPlants269Rules.h"
#include "FusionCompanions269Rules.h"
#include "FusionRemainingPlants269Rules.h"
#include "FusionRemainingLeaves269Rules.h"
#include "FusionMorePlants269Rules.h"
#include "FusionSummon269Rules.h"
#include "FusionSolar269Rules.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <initializer_list>

namespace LawnFusion
{
namespace
{
#include "generated/FusionShooterProfiles.inc"
bool InRange(float value, float minimum, float maximum)
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool Countdown(const FusionShooterTimingProfile& profile, float multiplier, int& ticks)
{
    if (!FusionShooterTimingProfileValid(profile) || profile.attackIntervalSeconds <= 0) return false;
    // Preserve the original float RNG multiplication before double floor.
    const float seconds = multiplier * profile.attackIntervalSeconds;
    const double deadline = std::floor(static_cast<double>(seconds) * 100.0) + 1.0;
    if (!std::isfinite(deadline) || deadline > (std::numeric_limits<int>::max)()) return false;
    ticks = static_cast<int>(deadline);
    return true;
}
double PlaybackSpeed(const FusionShooterTimingProfile& profile, float speed)
{
    return profile.stateSpeed * (profile.speedParameterActive ? static_cast<double>(speed) : 1.0);
}

int EventDeadline(const FusionShooterTimingProfile& profile, float speed, double event)
{
    return static_cast<int>(std::ceil(event / PlaybackSpeed(profile, speed) * 100.0));
}
}

const FusionShooterTimingProfile* FindFusionShooterTimingProfile(uint16_t kind)
{
    if(const auto* profile=FindFusionWorldActors269Timing(kind))return profile;
    if(const auto* profile=FindFusionLastPlants269Timing(kind))return profile;
    if(const auto* profile=FindFusionWorldRemainder269Timing(kind))return profile;
    if(const auto* profile=FindFusionMorePlants269Timing(kind))return profile;
    if(const auto* profile=FindFusionRemainingLeaves269Timing(kind))return profile;
    if(const auto* profile=FindFusionRemaining269Timing(kind))return profile;
    if(const auto* profile=FindFusionSummonPlants269Timing(kind))return profile;
    if(const auto* profile=FindFusionCompanions269Timing(kind))return profile;
    if(const auto* profile=FindFusionStar269TimingProfile(kind))return profile;
    if(const auto* profile=FindFusionDoom269Timing(kind))return profile;
    if(const auto* profile=FindFusionHypno269TimingProfile(kind))return profile;
    if(const auto* profile=FindFusionWater268Timing(kind))return profile;
    if(const auto* profile=FindFusionShulk268Timing(kind))return profile;
    if(const auto* profile=FindFusionCannon268LobberTiming(kind))return profile;
    if (const auto* profile=FindFusionAction267TimingProfile(kind)) return profile;
    if (const auto* profile=FindFusionExtendedLobberTimingProfile(kind)) return profile;
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    if (const auto* profile = FindFusionBaseTimingProfile(kind)) return profile;
    if (const auto* profile = FindFusionShooterBatchTimingProfile(kind)) return profile;
    if (const auto* profile = FindFusionBatchTimingProfile(kind)) return profile;
    return FindFusionRecipientTimingProfile(kind);
}

const FusionShooterTimingProfile* FindFusionShooterTimingProfile(uint16_t kind, const char* state)
{
    if(const auto* profile=FindFusionWorldActors269Timing(kind))return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(const auto* profile=FindFusionLastPlants269Timing(kind))return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(const auto* profile=FindFusionWorldRemainder269Timing(kind))return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(const auto* profile=FindFusionSummonPlants269Timing(kind))return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(const auto* profile=FindFusionCompanions269Timing(kind))return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(const auto* profile=FindFusionStar269TimingProfile(kind))
        return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(const auto* profile=FindFusionDoom269Timing(kind))
        return state&&std::strcmp(profile->visualState,state)==0?profile:nullptr;
    if(FindFusionHypno269TimingProfile(kind))return FindFusionHypno269TimingProfile(kind,state);
    if(const auto* profile=FindFusionWater268Timing(kind,state))return profile;
    if(const auto* profile=FindFusionShulk268Timing(kind))
        if(state&&std::strcmp(profile->visualState,state)==0)return profile;
    if(const auto* profile=FindFusionCannon268LobberTiming(kind,state))return profile;
    if (const auto* profile=FindFusionExtendedLobberTimingProfile(kind,state)) return profile;
    if (!state || !*state) return nullptr;
    if (const auto* profile=FindFusionAction267TimingProfile(kind))
        if(std::strcmp(profile->visualState,state)==0)return profile;
    for (const auto& profile : profiles)
        if (profile.kind == kind && profile.visualState && std::strcmp(profile.visualState,state) == 0)
            return FusionShooterTimingProfileValid(profile) ? &profile : nullptr;
    for (bool closeRange : {false,true})
        if (const auto* profile = FindFusionBatchTimingProfile(kind,closeRange))
            if (std::strcmp(profile->visualState,state) == 0) return profile;
    if (const auto* profile = FindFusionBaseTimingProfile(kind))
        if (std::strcmp(profile->visualState,state) == 0) return profile;
    if (const auto* profile = FindFusionShooterBatchTimingProfile(kind,state)) return profile;
    return FindFusionRecipientTimingProfile(kind,state);
}

bool FusionShooterTimingProfileValid(const FusionShooterTimingProfile& profile)
{
    if (!(std::isfinite(profile.attackIntervalSeconds) && profile.attackIntervalSeconds >= 0 &&
        std::isfinite(profile.clipDurationSeconds) && profile.clipDurationSeconds > 0 &&
        std::isfinite(profile.stateSpeed) && profile.stateSpeed > 0) || profile.eventClipSeconds.empty()) return false;
    double previous = 0;
    for (double event : profile.eventClipSeconds)
    {
        if (!std::isfinite(event) || event <= 0 || event < previous || event > profile.clipDurationSeconds) return false;
        previous = event;
    }
    return true;
}

bool TryGetFusionShooterInitialTicks(const FusionShooterTimingProfile& profile, float fraction, int& ticks)
{
    return InRange(fraction, .2f, .8f) && Countdown(profile, fraction, ticks);
}

bool TryGetFusionShooterCycleTicks(const FusionShooterTimingProfile& profile, float multiplier, int& ticks)
{
    return InRange(multiplier, .95f, 1.05f) && Countdown(profile, multiplier, ticks);
}

bool TryGetFusionShooterShotCounter(const FusionShooterTimingProfile& profile, float speed, int& counter)
{
    if (!FusionShooterTimingProfileValid(profile) || !InRange(speed, .9f, 1.1f)) return false;
    const double deadline = std::ceil(profile.eventClipSeconds.back() / PlaybackSpeed(profile, speed) * 100.0) + 1.0;
    if (!std::isfinite(deadline) || deadline > (std::numeric_limits<int>::max)()) return false;
    counter = static_cast<int>(deadline);
    return true;
}

bool TryGetFusionShooterEventsAtCounter(const FusionShooterTimingProfile& profile, float speed,
    int remainingCounter, int& eventCount)
{
    int initial = 0;
    if (!TryGetFusionShooterShotCounter(profile, speed, initial) || remainingCounter < 0 || remainingCounter >= initial) return false;
    int count = 0;
    for (double event : profile.eventClipSeconds)
    {
        const int deadline = EventDeadline(profile, speed, event);
        if (remainingCounter == initial - deadline) ++count;
    }
    eventCount = count;
    return true;
}

bool TryGetFusionShooterEventAtCounter(const FusionShooterTimingProfile& profile, float speed,
    int remainingCounter, std::size_t eventIndex, bool& due)
{
    int initial = 0;
    if (eventIndex >= profile.eventClipSeconds.size() ||
        !TryGetFusionShooterShotCounter(profile, speed, initial) || remainingCounter < 0 || remainingCounter >= initial) return false;
    due = remainingCounter == initial-EventDeadline(profile, speed, profile.eventClipSeconds[eventIndex]);
    return true;
}

float FusionShooterDuration(const FusionShooterTimingProfile& profile, float speed)
{
    return FusionShooterTimingProfileValid(profile) && InRange(speed, .9f, 1.1f) ?
        static_cast<float>(profile.clipDurationSeconds / PlaybackSpeed(profile, speed)) : 0.0f;
}

float FusionShooterMaximumDuration(uint16_t kind, float speed)
{
    float maximum = 0;
    if(const auto* profile=FindFusionWorldActors269Timing(kind))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionLastPlants269Timing(kind))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionWorldRemainder269Timing(kind))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    for(unsigned stage=0;stage<4;++stage)if(const auto* profile=FindFusionMorePlants269Timing(kind,stage))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionRemainingLeaves269Timing(kind))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionRemaining269Timing(kind))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(kind==1175)maximum=(std::max)(maximum,FusionShooterDuration(*FindFusionRemaining269Timing(kind,true),speed));
    maximum=(std::max)(maximum,FusionSummonPlants269MaximumDuration(kind,speed));
    if(const auto* profile=FindFusionCompanions269Timing(kind))maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionStar269TimingProfile(kind))
        maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionDoom269Timing(kind))
        maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    maximum=(std::max)(maximum,FusionHypno269MaximumDuration(kind,speed));
    if(kind==934)for(const char* state:{"shoot","super"})
        maximum=(std::max)(maximum,FusionShooterDuration(*FindFusionSolar269Timing(state),speed));
    for(const char* state:{"shoot","shoot2","shoot_grown","shoot2_grown"})
        if(const auto* profile=FindFusionWater268Timing(kind,state))
            maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    if(const auto* profile=FindFusionShulk268Timing(kind))
        maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    for(const char* state:{"shoot","shoot2","shoot3"})
        if(const auto* profile=FindFusionCannon268LobberTiming(kind,state))
            maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    for(const char* state:{"shoot","shoot2","supershoot"})
        if(const auto* profile=FindFusionExtendedLobberTimingProfile(kind,state))
            maximum=(std::max)(maximum,FusionShooterDuration(*profile,speed));
    maximum = (std::max)({maximum,FusionShooterBatchMaximumDuration(kind,speed),FusionFireActionMaximumDuration(kind,speed),
        FusionAction267MaximumDuration(kind,speed),FusionUmbrellaBatchMaximumDuration(kind,speed)});
    if (const auto* profile = FindFusionBaseTimingProfile(kind)) maximum = (std::max)(maximum,FusionShooterDuration(*profile,speed));
    for (const char* state : {"shoot","shoot_loop","shoot1","attack","superskill"})
        if (const auto* profile = FindFusionShooterBatchTimingProfile(kind,state))
            maximum = (std::max)(maximum,FusionShooterDuration(*profile,speed));
    for (const auto& profile : profiles)
        if (profile.kind == kind && FusionShooterTimingProfileValid(profile))
        {
            const float duration = FusionShooterDuration(profile,speed);
            if (duration > maximum) maximum = duration;
        }
    for (bool closeRange : {false,true})
        if (const auto* profile = FindFusionBatchTimingProfile(kind,closeRange))
        {
            const float duration = FusionShooterDuration(*profile,speed);
            if (duration > maximum) maximum = duration;
        }
    for (bool airborne : {false,true})
        if (const auto* profile = FindFusionRecipientTimingProfile(kind,airborne))
        {
            const float duration = FusionShooterDuration(*profile,speed);
            if (duration > maximum) maximum = duration;
        }
    return maximum;
}
}
