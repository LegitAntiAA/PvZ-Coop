#include "FusionNativeShooterTiming.h"
#include "FusionNative.h"
#include "FusionNativeAction267.h"
#include "FusionSplitShooterRules.h"
#include "FusionLobberRules.h"
#include "FusionNativeLobber.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeRecipientShooters.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeLastPlants269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeStar269.h"
#include "FusionNativeDoom269.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeMorePlants269.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeRemainingPlants269.h"
#include "../Plant.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"

namespace LawnFusion
{
namespace
{
NativePlantData* HostData(Plant* plant, const FusionShooterTimingProfile& profile)
{
    if (!plant || !plant->mApp || !plant->mBoard || plant->mDead || plant->mNetGhost ||
        plant->mApp->NetIsClient() || GetNativeSeedForPlant(profile.kind) == SEED_NONE ||
        !FindFusionShooterTimingProfile(profile.kind) || !FusionShooterTimingProfileValid(profile)) return nullptr;
    auto* data = GetNativePlantData(plant);
    return data && data->kind == profile.kind ? data : nullptr;
}
}

bool InitializeNativeShooterTiming(Plant* plant, const FusionShooterTimingProfile& profile, bool wake)
{
    auto* data = HostData(plant, profile);
    if (!data) return false; // Classic/unknown/unsupported consumes no RNG.
    plant->mLaunchRate = static_cast<int>(profile.attackIntervalSeconds * 100.0f);
    data->animationSpeed = RandRangeFloat(.9f, 1.1f);
    if (wake)
    {
        plant->SetSleeping(false);
        plant->mWakeUpCounter = 0;
    }
    if (!TryGetFusionShooterInitialTicks(profile, RandRangeFloat(.2f, .8f), plant->mLaunchCounter)) return false;
    data->productionTicks = data->pendingProductionTicks = 0;
    data->idleSeconds = 0;
    data->shootSeconds = -1;
    return true;
}

bool ResetNativeShooterLaunchCounter(Plant* plant, const FusionShooterTimingProfile& profile)
{
    if (!HostData(plant, profile)) return false;
    if (FindFusionLobberProfile(profile.kind))
        return ResetNativeLobberLaunchCounter(plant);
    return TryGetFusionShooterCycleTicks(profile, RandRangeFloat(.95f, 1.05f), plant->mLaunchCounter);
}

bool BeginNativeShooterShot(Plant* plant, const FusionShooterTimingProfile& profile)
{
    auto* data = HostData(plant, profile);
    if (!data || !TryGetFusionShooterShotCounter(profile, data->animationSpeed, plant->mShootingCounter)) return false;
    data->shootSeconds = 0;
    return true;
}

void UpdateNativeShooterPhase(Plant* plant, const FusionShooterTimingProfile& profile)
{
    auto* data = HostData(plant, profile);
    if (!data) return;
    data->idleSeconds += .01f;
    if (data->shootSeconds < 0) return;
    if (plant->mShootingCounter > 0)
    {
        int initial = 0;
        if (!TryGetFusionShooterShotCounter(profile, data->animationSpeed, initial)) return;
        data->shootSeconds = static_cast<float>(initial - plant->mShootingCounter) * .01f;
    }
    else data->shootSeconds += .01f;
    if (data->shootSeconds >= FusionShooterDuration(profile, data->animationSpeed)) data->shootSeconds = -1;
}

bool DispatchNativeShooterEvents(Plant* plant)
{
    // Event callbacks sample animated source nodes before the one ordinary
    // phase update at the native tick tail. Mirror the real counter here;
    // do not advance idleSeconds or introduce another animation clock.
    const FusionShooterTimingProfile* source269 = nullptr;
    if (IsNativeLastPlants269(plant)) source269 = FindFusionLastPlants269Timing(GetNativePlantData(plant)->kind);
    else if (IsNativeWorldRemainder269Plant(plant)) source269 = FindFusionWorldRemainder269Timing(GetNativePlantData(plant)->kind);
    else if (IsNativeWorld269Plant(plant)) source269 = GetNativeWorld269Timing(plant);
    else if (IsNativeStar269Plant(plant)) source269 = GetNativeStar269Timing(plant);
    else if (IsNativeDoom269Plant(plant)) source269 = FindFusionDoom269Timing(GetNativePlantData(plant)->kind);
    else if (IsNativeHypno269Plant(plant)) source269 = GetNativeHypno269Timing(plant);
    else if (IsNativeSolar269(plant)) source269 = GetNativeLobberTimingProfile(plant);
    if (!source269&&IsNativeMorePlants269(plant))source269=FindFusionMorePlants269Timing(GetNativePlantData(plant)->kind,GetNativePlantData(plant)->batchBagCount);
    if (source269)
        if (auto* source = HostData(plant,*source269))
        {
            int initial = 0;
            if (source->shootSeconds >= 0 && TryGetFusionShooterShotCounter(*source269,source->animationSpeed,initial) &&
                plant->mShootingCounter > 0 && plant->mShootingCounter <= initial)
                source->shootSeconds = static_cast<float>(initial-plant->mShootingCounter)*.01f;
        }
    if(DispatchNativeLastPlants269Events(plant)||DispatchNativeWorldRemainder269Shot(plant))return true;
    if(DispatchNativeMorePlants269Events(plant)||DispatchNativeRemainingLeaves269Events(plant)||DispatchNativeRemaining269Events(plant)||DispatchNativeSummonPlants269Events(plant)||DispatchNativeCompanions269Events(plant)||DispatchNativeWorld269Shot(plant))return true;
    if (DispatchNativeStar269Shot(plant)) return true;
    if (DispatchNativeDoom269Events(plant)) return true;
    if (DispatchNativeHypno269Shot(plant)) return true;
    if (DispatchNativeAction267Events(plant)) return true;
    if (DispatchNativeShooterBatchShot(plant)) return true;
    if (DispatchNativeRecipientShot(plant)) return true;
    if (DispatchNativeBatchShot(plant)) return true;
    const auto* data = GetNativePlantData(plant);
    const bool lobber = IsNativeLobber(plant);
    const auto* profile = lobber ? GetNativeLobberTimingProfile(plant) : data ? FindFusionShooterTimingProfile(data->kind) : nullptr;
    if (!profile || GetNativeSeedForPlant(profile->kind) == SEED_NONE) return lobber; // Known source family never falls into vanilla Kernel events.
    if (!HostData(plant, *profile)) return true;
    const auto emissions = GetFusionSplitEmissionSpecs(data->kind);
    for (std::size_t event = 0; event < profile->eventClipSeconds.size(); ++event)
    {
        bool due = false;
        if (!TryGetFusionShooterEventAtCounter(*profile, data->animationSpeed, plant->mShootingCounter, event, due) || !due) continue;
        if (emissions.empty()) plant->Fire(nullptr, plant->mRow, WEAPON_PRIMARY);
        else for (const auto& emission : emissions)
            if (emission.eventIndex == event) plant->Fire(nullptr, plant->mRow, WEAPON_PRIMARY, &emission);
    }
    return true;
}
}
