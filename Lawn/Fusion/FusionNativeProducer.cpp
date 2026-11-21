#include "FusionNativeProducer.h"
#include "FusionNative.h"
#include "FusionNativeMoney.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"

namespace LawnFusion
{
namespace
{
NativePlantData* HostData(Plant* plant)
{
    if (!plant || !plant->mApp || !plant->mBoard || plant->mApp->mBoard != plant->mBoard ||
        plant->mDead || plant->mNetGhost || plant->mApp->NetIsClient() || !plant->mIsOnBoard || !plant->IsInPlay()) return nullptr;
    auto* data = GetNativePlantData(plant);
    return data && IsNativeProducerKind(data->kind) && GetNativeSeedForPlant(data->kind) == plant->mSeedType ? data : nullptr;
}

bool Gameplay(Plant* plant)
{
    return CanApplyNativeFusionGameplay(plant->mBoard);
}

int& Counter(Plant* plant, NativePlantData& data)
{
    return data.kind == 1000 ? data.productionTicks : plant->mLaunchCounter;
}

}

bool EmitNativeSourceSun(Plant* plant, uint16_t sourceItem, bool sound)
{
    int value=0;
    if (!plant || !TryGetFusionSourceSunValue(sourceItem,value) || !plant->mBoard || !plant->mApp ||
        plant->mApp->mBoard!=plant->mBoard || plant->mApp->NetIsClient() || plant->mNetGhost ||
        !IsNativeFusionBoard(plant->mBoard) || plant->mDead || !plant->mIsOnBoard || !Gameplay(plant) ||
        !FusionProducerCanFitCallback(plant->mBoard->mCoins.mSize,plant->mBoard->mCoins.mMaxSize,1)) return false;
    const CoinType type=value<25?COIN_SMALLSUN:value==25?COIN_SUN:COIN_LARGESUN;
    auto* coin=plant->mBoard->AddCoin(plant->mX,plant->mY,type,COIN_MOTION_FROM_PLANT,SEED_NONE,value);
    if (!coin || !AttachNativeCoinValue(plant->mBoard,coin,value)) return false;
    if (sound) plant->mApp->PlayFoley(FOLEY_SPAWN_SUN);
    LawnNet::NetTrace("Fusion native Producer sourceSun plant=%u kind=%u coin=%u item=%u value=%d tick=%d",
        plant->mNetId,GetNativePlantData(plant)->kind,coin->mNetId,sourceItem,value,plant->mBoard->mMainCounter);
    return true;
}

bool EmitNativeSunBatch(Plant* plant, int count, bool sound)
{
    if (!plant || !plant->mBoard || !plant->mApp || plant->mApp->mBoard != plant->mBoard ||
        plant->mDead || plant->mNetGhost || plant->mApp->NetIsClient() || !plant->mIsOnBoard ||
        !plant->IsInPlay() || !Gameplay(plant)) return false;
    const auto* data = GetNativePlantData(plant);
    if (!data || GetNativeSeedForPlant(data->kind) != plant->mSeedType) return false;
    auto* board = plant->mBoard;
    if (!FusionProducerCanFitCallback(board->mCoins.mSize,board->mCoins.mMaxSize,count)) return false;
    for (int i = 0; i < count; ++i)
        board->AddCoin(plant->mX,plant->mY,COIN_SUN,COIN_MOTION_FROM_PLANT);
    if (sound) plant->mApp->PlayFoley(FOLEY_SPAWN_SUN);
    LawnNet::NetTrace("Fusion native Producer emit id=%u token=%u kind=%u count=%d value=25 sound=%d tick=%d nextDeadline=%d",
        plant->mNetId,data->instanceToken,data->kind,count,sound ? 1 : 0,board->mMainCounter,
        data->kind == 1000 ? data->productionTicks : plant->mLaunchCounter);
    return true;
}

bool IsNativeProducer(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && IsNativeProducerKind(data->kind);
}

void InitializeNativeProducer(Plant* plant)
{
    auto* data = HostData(plant);
    const auto* profile = data ? FindFusionProducerProfile(data->kind) : nullptr;
    const auto* definition = data ? FusionFindPlant(data->kind) : nullptr;
    if (!profile || !FusionProducerProfileValid(*profile) || !definition || !definition->hasStats ||
        definition->maxHealth <= 0 || definition->productionIntervalSeconds != profile->intervalSeconds) return;
    int initial = 0;
    const float initialSeconds = RandRangeFloat(4,7);
    if (!TryGetFusionProducerInitialTicks(*profile,initialSeconds,initial)) return;
    plant->mPlantHealth = plant->mPlantMaxHealth = definition->maxHealth;
    Counter(plant,*data) = initial;
    if (data->kind != 1000) plant->mLaunchRate = static_cast<int>(profile->intervalSeconds*100);
    data->productionTicks = initial; // Pure producers mirror their actual native counter.
    data->pendingProductionTicks = 0;
    data->producerRewardFlags = 0;
    LawnNet::NetTrace("Fusion native Producer init id=%u token=%u kind=%u hp=%d initialSeconds=%.9g counter=%d interval=%.1f count=%d tick=%d",
        plant->mNetId,data->instanceToken,data->kind,plant->mPlantMaxHealth,initialSeconds,initial,
        profile->intervalSeconds,profile->periodicCoinCount,plant->mBoard->mMainCounter);
}

bool UpdateNativeProducer(Plant* plant)
{
    if (!IsNativeProducer(plant)) return false;
    auto* data = HostData(plant);
    if (!data || !Gameplay(plant) || plant->mBoard->mPaused || plant->mSquished || plant->mPlantHealth <= 0) return true;
    const auto& profile = *FindFusionProducerProfile(data->kind);
    // Invoke has already been scheduled: process it before the animator gate.
    if (AdvanceFusionProducerPending(data->pendingProductionTicks))
    {
        const bool emitted = profile.mode == FusionProducerMode::Marigold ? EmitNativeMoneyCoins(plant,profile.periodicCoinCount) :
            data->kind==1136||data->kind==1137 ? EmitNativeMoneyItem(plant->mBoard,float(plant->mX+20),float(plant->mY+20),38) :
            data->kind==1184 ? EmitNativeSourceSun(plant,2) :
            data->kind==1031 ? EmitNativeSourceSun(plant,data->batchMode?0:2) : EmitNativeSunBatch(plant,profile.periodicCoinCount,true);
        if (!emitted)
        {
            data->pendingProductionTicks = 1;
            // Saturated native arrays retain the whole callback. Stall this
            // producer until it fits, rather than overwriting an older Invoke.
            return true;
        }
    }
    // Native nut cosmetic mAnimRate0 is not source Animator.speed0. Ordinary
    // source property1 is the supported baseline; future freezes need an
    // explicit gameplay status, not a test of the reanimation's art rate.
    if (!FusionProducerGateOpen(profile,true,plant->mApp->IsIZombieLevel(),1)) return true;
    int& counter = Counter(plant,*data);
    if (AdvanceFusionProducerCounter(counter))
    {
        int cycle = 0;
        const bool valid = profile.mode == FusionProducerMode::MoneyPot ? TryGetFusionProducerCycleTicks(profile,RandRangeFloat(.9f,1.1f),cycle) :
            profile.mode == FusionProducerMode::Marigold ? TryGetFusionProducerCycleTicks(profile,RandRangeFloat(-2,2),cycle) :
            TryGetFusionProducerCycleTicks(profile,RandRangeInt(-2,2),cycle);
        if (!valid) return true;
        counter = cycle;
        data->pendingProductionTicks = kFusionProducerDelayTicks;
        if (plant->mEatenFlashCountdown < kFusionProducerDelayTicks)
            plant->mEatenFlashCountdown = kFusionProducerDelayTicks;
        LawnNet::NetTrace("Fusion native Producer trigger id=%u token=%u kind=%u count=%d cycle=%d pending=50 tick=%d nextDeadline=%d",
            plant->mNetId,data->instanceToken,data->kind,profile.periodicCoinCount,cycle,plant->mBoard->mMainCounter,cycle);
    }
    if (data->kind != 1000) data->productionTicks = counter;
    return true;
}

void NativeProducerHealthChanged(Plant* plant)
{
    auto* data = HostData(plant);
    if (!data || data->kind != 1006 || !Gameplay(plant)) return;
    int count = 0;
    if (!TryConsumeFusionSunNutReward(plant->mPlantHealth,plant->mApp->IsIZombieLevel(),data->producerRewardFlags,count) || !count) return;
    // Capacity is a bounded native adaptation: consume the threshold bit and
    // omit the complete synchronous callback, never emit a partial reward.
    if (!EmitNativeSunBatch(plant,count,true))
        LawnNet::NetTrace("Fusion native Producer threshold omitted capacity id=%u hp=%d flags=%u count=%d",
            plant->mNetId,plant->mPlantHealth,data->producerRewardFlags,count);
}

void NativeProducerDying(Plant* plant)
{
    auto* data = HostData(plant);
    if (!data || data->kind != 1006 || !Gameplay(plant) || plant->mApp->IsIZombieLevel()) return;
    // Lethal direct-health writers also cross the low band before source Die.
    // A bite-path health hook may already have consumed that same one-shot bit.
    NativeProducerHealthChanged(plant);
    if (!EmitNativeSunBatch(plant,2,false))
        LawnNet::NetTrace("Fusion native Producer death omitted capacity id=%u count=2",plant->mNetId);
}
}
