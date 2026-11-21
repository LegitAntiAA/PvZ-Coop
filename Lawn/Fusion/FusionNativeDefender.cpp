#include "FusionNativeDefender.h"
#include "FusionNative.h"
#include "FusionNativeCold.h"
#include "FusionNativeFire.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <algorithm>

namespace LawnFusion
{
namespace
{
bool Current(const Plant* plant)
{
    return plant && plant->mApp && plant->mBoard && plant->mApp->mBoard == plant->mBoard &&
        IsNativeFusionBoard(plant->mBoard) &&
        !plant->mDead && plant->mIsOnBoard && plant->mPlantHealth > 0;
}
bool Host(const Plant* plant)
{ return Current(plant) && !plant->mNetGhost && !plant->mApp->NetIsClient(); }
const FusionDefenderProfile* Profile(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    const auto* profile = data ? FindFusionDefenderProfile(data->kind) : nullptr;
    return profile && GetNativeSeedForPlant(profile->kind) != SEED_NONE &&
        GetNativeSeedForPlant(profile->kind) == plant->mSeedType ? profile : nullptr;
}
bool SameCell(const Plant* a, const Plant* b)
{ return a && b && a->mBoard == b->mBoard && a->mPlantCol == b->mPlantCol && a->mRow == b->mRow; }
bool Recover(Plant* plant, int amount)
{
    if (!Host(plant) || !Profile(plant) || amount <= 0 || plant->mPlantMaxHealth <= 0 ||
        plant->mPlantHealth > plant->mPlantMaxHealth) return false;
    plant->mPlantHealth = static_cast<int>((std::min)(static_cast<int64_t>(plant->mPlantMaxHealth),
        static_cast<int64_t>(plant->mPlantHealth)+amount));
    NativeDefenderHealthChanged(plant);
    return true;
}
bool Biter(Zombie* zombie, Plant* victim)
{
    return Host(victim) && zombie && zombie->mBoard == victim->mBoard && !zombie->mNetGhost &&
        !zombie->mDead && !zombie->IsDeadOrDying() && zombie->mBodyHealth > 0 &&
        IsNativeFusionGroundZombie(zombie) &&
        (!GetNativeSourceZombieData(zombie) || (zombie->mZombiePhase == PHASE_ZOMBIE_NORMAL && !zombie->mInPool && !zombie->mOnHighGround)) &&
        !zombie->mMindControlled && !zombie->IsFlying() &&
        zombie->mRow == victim->mRow;
}
}

bool IsNativeDefender(const Plant* plant) { return Profile(plant) != nullptr; }
void InitializeNativeDefender(Plant* plant)
{
    const auto* profile = Profile(plant);
    if (!profile || !Host(plant)) return;
    plant->mPlantMaxHealth = plant->mPlantHealth = profile->maxHealth;
    auto* data = GetNativePlantData(plant);
    data->damageStage = 0;
    LawnNet::NetTrace("Fusion native Defender init id=%u token=%u kind=%u hp=%d layer=%d pending=%u",
        plant->mNetId,data->instanceToken,data->kind,plant->mPlantHealth,static_cast<int>(profile->layer),profile->pendingDependencies);
}
void NativeDefenderHealthChanged(Plant* plant)
{
    const auto* profile = Profile(plant);
    if (!profile || !plant->mApp || !plant->mBoard || plant->mApp->mBoard != plant->mBoard ||
        !IsNativeFusionBoard(plant->mBoard) || !NativeFusionBackgroundValid(static_cast<uint8_t>(plant->mBoard->mBackground)) ||
        plant->mApp->NetIsClient() || plant->mNetGhost || !profile->damageNodes[0] || plant->mDead) return;
    auto* data = GetNativePlantData(plant);
    uint8_t stage = data->damageStage;
    if (TryGetFusionDefenderDamageStage(plant->mPlantHealth,plant->mPlantMaxHealth,data->damageStage,stage))
        data->damageStage = stage;
}
uint16_t GetNativeDefenderPotKind(Plant* plant)
{
    if (!Current(plant) || !GetNativePlantData(plant) || plant->mPlantCol < 0 || plant->mRow < 0 ||
        plant->mPlantCol >= plant->mBoard->GetColumnCount() || plant->mRow >= GetNativeFusionRowCount(plant->mBoard)) return 0xffff;
    PlantsOnLawn layers{};
    plant->mBoard->GetPlantsOnLawn(plant->mPlantCol,plant->mRow,&layers);
    const auto* pot = layers.mUnderPlant;
    const auto* profile = Profile(pot);
    return profile && profile->layer == FusionDefenderLayer::Under && Current(pot) && SameCell(plant,pot) ? profile->kind : 0xffff;
}
float GetNativeDefenderLobberInterval(Plant* plant, float original)
{
    const auto* data = GetNativePlantData(plant);
    return data ? FusionDefenderLobberInterval(data->kind,GetNativeDefenderPotKind(plant),original) : original;
}
int GetNativeDefenderCornDenominator(Plant* plant, int original)
{
    const auto* data = GetNativePlantData(plant);
    return data ? FusionDefenderCornDenominator(data->kind,GetNativeDefenderPotKind(plant),original) : original;
}
bool GetNativeDefenderMelonEnhanced(Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && FusionDefenderMelonEnhanced(data->kind,GetNativeDefenderPotKind(plant));
}
void NativeDefenderBeforeBite(Zombie* biter, Plant* victim, int biteDamage)
{
    const auto* profile = Profile(victim);
    FusionDefenderBiteEffect effect;
    if (!profile || !Biter(biter,victim) || !TryGetFusionDefenderBiteEffect(profile->kind,biteDamage,effect)) return;
    if (effect.shieldlessDamage) biter->TakeDamage(effect.shieldlessDamage,1U);
    if (effect.warm) NativeWarmAfterDamage(biter);
    if (effect.chilledTicks)
    {
        auto* cold = GetOrCreateNativeZombieColdData(biter);
        if (cold)
        {
            // EatEffect first overwrites SetCold10, then adds10 levels. The
            // existing cold primitive alone owns threshold/freeze behavior.
            biter->SetColdCounters(effect.chilledTicks,biter->mIceTrapCounter);
            cold->chilledTicks = biter->mChilledCounter;
            cold->frozenTicks = biter->mIceTrapCounter;
            AddNativeColdLevel(biter,effect.coldLevels);
        }
    }
    if (effect.shieldlessDamage || effect.chilledTicks || effect.warm)
        LawnNet::NetTrace("Fusion native Defender bite plant=%u kind=%u target=%u bite=%d reflect=%d slow=%d levels=%d warm=%d tick=%d",
            victim->mNetId,profile->kind,biter->mNetId,biteDamage,effect.shieldlessDamage,effect.chilledTicks,
            effect.coldLevels,effect.warm?1:0,victim->mBoard->mMainCounter);
    // EatEffect returns false for these effects. Caller still performs the
    // original bite; no source actor, plant timer or native phase is replaced.
}
bool NativeDefenderBlocksDamage(Plant* plant, int sourceDamageType)
{
    const auto* profile = Profile(plant);
    if (!profile || !Host(plant) || profile->layer != FusionDefenderLayer::Under) return false;
    PlantsOnLawn layers{};
    plant->mBoard->GetPlantsOnLawn(plant->mPlantCol,plant->mRow,&layers);
    const bool carrying = (Current(layers.mNormalPlant) && SameCell(plant,layers.mNormalPlant)) ||
        (Current(layers.mPumpkinPlant) && SameCell(plant,layers.mPumpkinPlant));
    return FusionDefenderBlocksPotDamage(profile->kind,carrying,sourceDamageType);
}
void NativeDefenderElementPulse(Board* board, FusionDefenderElement element, int row)
{
    if (!board || !board->mApp || board->mApp->mBoard != board || board->mApp->NetIsClient() ||
        !IsNativeFusionBoard(board) || !NativeFusionBackgroundValid(static_cast<uint8_t>(board->mBackground)) ||
        (element == FusionDefenderElement::Fire && (row < 0 || row >= GetNativeFusionRowCount(board)))) return;
    Plant* plant = nullptr;
    while (board->IteratePlants(plant))
    {
        const auto* profile = Profile(plant);
        if (!profile || (element == FusionDefenderElement::Fire && plant->mRow != row)) continue;
        const int amount = FusionDefenderElementRecovery(profile->kind,element);
        const int before = plant->mPlantHealth;
        if (amount && Recover(plant,amount))
            LawnNet::NetTrace("Fusion native Defender pulse plant=%u kind=%u element=%d before=%d after=%d amount=%d row=%d tick=%d",
                plant->mNetId,profile->kind,static_cast<int>(element),before,plant->mPlantHealth,amount,plant->mRow,board->mMainCounter);
    }
}
bool CanUseNativeDefenderBucket(const Plant* plant, int bucket, FusionDefenderBucketEffect& effect)
{
    const auto* profile = Profile(plant);
    return profile && Current(plant) && TryGetFusionDefenderBucketEffect(profile->kind,bucket,effect);
}
bool ApplyNativeDefenderBucket(Plant* plant, int bucket, Plant* replacement)
{
    FusionDefenderBucketEffect effect;
    if (!Host(plant) || !CanUseNativeDefenderBucket(plant,bucket,effect)) return false;
    const auto* old = GetNativePlantData(plant);
    if (effect.replacementKind != 0xffff)
    {
        const auto* fresh = GetNativePlantData(replacement);
        const auto* expected = FindFusionDefenderProfile(effect.replacementKind);
        if (!Host(replacement) || replacement == plant || !SameCell(plant,replacement) || !Profile(replacement) ||
            !fresh || fresh->kind != effect.replacementKind || !expected || !fresh->instanceToken ||
            fresh->instanceToken == old->instanceToken || fresh->owner != old->owner || fresh->packetId != old->packetId ||
            fresh->randomOrigin != old->randomOrigin || replacement->mPlantMaxHealth != expected->maxHealth ||
            replacement->mPlantHealth != expected->maxHealth) return false;
        const uint16_t beforeId = plant->mNetId, afterId = replacement->mNetId;
        plant->Die(0); // Original UseItem retires the old actor with source Default.
        LawnNet::NetTrace("Fusion native Defender bucket replace old=%u new=%u kind=%u bucket=%d tick=%d",
            beforeId,afterId,effect.replacementKind,bucket,replacement->mBoard->mMainCounter);
        return true;
    }
    if (replacement) return false;
    const int before = plant->mPlantHealth;
    if (!Recover(plant,effect.recoverHealth)) return false;
    LawnNet::NetTrace("Fusion native Defender bucket recover plant=%u kind=%u bucket=%d before=%d after=%d tick=%d",
        plant->mNetId,old->kind,bucket,before,plant->mPlantHealth,plant->mBoard->mMainCounter);
    return true;
}
}
