#include "FusionNativeTravelBuff369.h"
#include "FusionNativeProjectileBuff369.h"
#include "FusionNative.h"
#include "FusionNativeConveyor308.h"
#include "FusionNativeCharred311.h"
#include "FusionNativeObstacle313.h"
#include "FusionNativeCapturedSourcePlant314.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeJackbox309Bridge.h"
#include "../Net/NetJackbox309Audio.h"
#include "NativeSpecialZombie269.h"
#include "FusionDefenderRules.h"
#include "FusionBatchActionRules.h"
#include "FusionMoneyRules.h"
#include "FusionNativeProjectileMetadata.h"
#include <cmath>
#include "FusionNativeVisual.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeGloveUI.h"
#include "FusionCherryShooterRules.h"
#include "FusionStraightProjectileRules.h"
#include "FusionSplitShooterRules.h"
#include "FusionColdRules.h"
#include "FusionFireRules.h"
#include "FusionProducerRules.h"
#include "FusionPoisonRules.h"
#include "FusionLobberRules.h"
#include "FusionButterRules.h"
#include "FusionChomper268Rules.h"
#include "../Board.h"
#include "../SeedPacket.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Widget/GameButton.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
namespace
{
template <typename T> uint32_t LiveId(DataArray<T>& array, const T* entity)
{
    if (!array.mBlock || !entity) return 0;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(array.mBlock);
    const uintptr_t pointer = reinterpret_cast<uintptr_t>(entity);
    const size_t stride = sizeof(typename DataArray<T>::DataArrayItem);
    if (pointer < begin || pointer - begin >= static_cast<size_t>(array.mMaxUsedCount) * stride ||
        (pointer - begin) % stride != 0) return 0;
    const auto& item = array.mBlock[(pointer - begin) / stride];
    return array.DataArrayTryToGet(item.mID) == entity ? item.mID : 0;
}
}

NativeContext::NativeContext()
{
    for (auto& terrain : lastColumnTerrain) terrain = GRIDSQUARE_GRASS;
}
NativeContext::~NativeContext()
{if(capturedPlants314)capturedPlants314->Clear();}

NativeCapturedPlants314* GetNativeCapturedPlants314(Board* board)
{
    auto* context=GetNativeContext(board);if(!context)return nullptr;
    if(!context->capturedPlants314)context->capturedPlants314=std::make_unique<NativeCapturedPlants314>(board);
    return context->capturedPlants314.get();
}

NativeSourceEffects269* GetNativeSourceEffects269(Board* board,bool create)
{
    auto* context=GetNativeContext(board);
    if(!context)return nullptr;
    if(!context->sourceEffects269&&create)
        context->sourceEffects269=std::make_unique<NativeSourceEffects269>(board->mApp);
    return context->sourceEffects269.get();
}

bool IsNativeFusionBoard(const Board* board)
{
    return board && board->mApp && board->mFusionNative &&
        board->mApp->GetContentMode() == ContentMode::Fusion && !board->mApp->mFusionRuntime;
}

void ApplyNativeSourceRealDamage(Zombie* zombie, int damage)
{
    if (!zombie || !IsNativeFusionBoard(zombie->mBoard) || !zombie->mApp ||
        zombie->mApp->NetIsClient() || zombie->mNetGhost || damage<=0 || zombie->IsDeadOrDying()) return;
    // Source DmgType6 ignores each armor callback's remainder. All three
    // native components receive the original amount, in source order.
    if (zombie->mShieldType!=SHIELDTYPE_NONE) zombie->TakeShieldDamage(damage,0);
    if (zombie->mHelmType!=HELMTYPE_NONE) zombie->TakeHelmDamage(damage,0);
    zombie->TakeBodyDamage(damage,0);
}
bool CanApplyNativeFusionGameplay(Board* board)
{
    return board&&board->mApp&&board->mApp->mBoard==board&&IsNativeFusionBoard(board)&&
        board->mApp->mGameScene==SCENE_PLAYING&&!board->mPaused&&board->mBoardFadeOutCounter<0&&
        !board->mApp->GetDialog(DIALOG_ALMANAC)&&!board->HasLevelAwardDropped();
}

NativeContext* GetNativeContext(Board* board)
{
    return IsNativeFusionBoard(board) ? board->mFusionNative.get() : nullptr;
}

const NativeContext* GetNativeContext(const Board* board)
{
    return IsNativeFusionBoard(board) ? board->mFusionNative.get() : nullptr;
}

NativePlantData* GetNativePlantData(Plant* plant)
{
    auto* context = GetNativeContext(plant ? plant->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(plant->mBoard->mPlants, plant);
    const auto found = context->plants.find(id);
    return id && found != context->plants.end() ? &found->second : nullptr;
}

const NativePlantData* GetNativePlantData(const Plant* plant)
{
    return GetNativePlantData(const_cast<Plant*>(plant));
}

NativeProjectileData* GetNativeProjectileData(Projectile* projectile)
{
    auto* context = GetNativeContext(projectile ? projectile->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(projectile->mBoard->mProjectiles, projectile);
    const auto found = context->projectiles.find(id);
    return id && found != context->projectiles.end() ? &found->second : nullptr;
}

const NativeProjectileData* GetNativeProjectileData(const Projectile* projectile)
{
    return GetNativeProjectileData(const_cast<Projectile*>(projectile));
}

const NativePacketData* GetNativePacketData(const SeedPacket* packet)
{
    const auto* context = GetNativeContext(packet ? packet->mBoard : nullptr);
    if (!context || !packet->mNetId) return nullptr;
    const auto found = context->packets.find(packet->mNetId);
    return found != context->packets.end() ? &found->second : nullptr;
}

NativeSourceZombieData* GetNativeSourceZombieData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies, zombie);
    const auto found = context->sourceZombies.find(id);
    return id && found != context->sourceZombies.end() ? &found->second : nullptr;
}

const NativeSourceZombieData* GetNativeSourceZombieData(const Zombie* zombie)
{
    return GetNativeSourceZombieData(const_cast<Zombie*>(zombie));
}

bool AttachNativeSourceZombieData(Board* board, Zombie* zombie, const NativeSourceZombieData& data)
{
    auto* context = GetNativeContext(board);
    if (!context || !zombie || zombie->mBoard != board ||
        !NativeSourceZombieDataValid(data,zombie->mBodyMaxHealth) ||
        zombie->mZombieType != GetNativeSourceZombieCarrier(data.kind)) return false;
    const uint32_t id = LiveId(board->mZombies,zombie);
    if (!id) return false;
    if(!board->mApp->NetIsClient()&&!zombie->mNetGhost&&!context->sourceZombieBirthOrdinals.contains(id))
    {
        if(context->sourceZombieBirthSequence==UINT64_MAX)return false;
        context->sourceZombieBirthOrdinals.emplace(id,++context->sourceZombieBirthSequence);
    }
    context->sourceZombies[id] = data;
    return true;
}

int GetNativeFusionZombieKind(const Zombie* zombie)
{
    if (!zombie) return -1;
    const auto* data = GetNativeSourceZombieData(zombie);
    if (data) return FindFusionZombieProfile(data->kind) && zombie->mZombieType == GetNativeSourceZombieCarrier(data->kind) ? data->kind : -1;
    // Known source equivalents are used by type-specific Fusion effects.
    // Physical target eligibility below uses native rules for every Classic type.
    switch(zombie->mZombieType)
    {
    case ZOMBIE_NORMAL:case ZOMBIE_FLAG:return 0;
    case ZOMBIE_TRAFFIC_CONE:return 2;
    case ZOMBIE_PAIL:return 4;
    case ZOMBIE_DOOR:return 8;
    default:return -1;
    }
}

bool IsNativeFusionGroundZombie(const Zombie* zombie)
{
    if (!zombie) return false;
    if (GetNativeSourceZombieData(zombie)) return GetNativeFusionZombieKind(zombie) >= 0;
    if (!zombie->mBoard || !zombie->mApp) return false;
    unsigned flags = 1U << DAMAGES_GROUND;
    if (zombie->mMindControlled) flags |= 1U << DAMAGES_ONLY_MINDCONTROLLED;
    return const_cast<Zombie*>(zombie)->EffectedByDamage(flags);
}

int GetNativeFusionZombieStatus(const Zombie* zombie)
{
    if (!zombie) return -1;
    NativeZombie313Semantics semantic;
    if(GetNativeZombie313Semantics(zombie,semantic))return semantic.status;
    if (!GetNativeSourceZombieData(zombie))
    {
        auto* native = const_cast<Zombie*>(zombie);
        if (native->IsDeadOrDying()) return 1;
        if (IsNativeFusionGroundZombie(zombie)) return 0;
        if (!zombie->mBoard || !zombie->mApp) return -1;
        const unsigned allegiance = zombie->mMindControlled ? 1U << DAMAGES_ONLY_MINDCONTROLLED : 0U;
        if (native->IsFlying() && native->EffectedByDamage(allegiance | (1U << DAMAGES_FLYING))) return 10;
        if (zombie->mZombiePhase == PHASE_DIGGER_TUNNELING &&
            native->EffectedByDamage(allegiance | (1U << DAMAGES_UNDERGROUND))) return 12;
        if (native->EffectedByDamage(allegiance | (1U << DAMAGES_SUBMERGED))) return 16;
        return 7; // Native invulnerable, entering, jumping, or otherwise unavailable.
    }
    int dayStatus=0;
    if(const auto* jackbox=GetNativeJackbox309(zombie))return FusionJackbox300SourceStatus(*jackbox);
    if(GetNativeDayZombie273Status(zombie,dayStatus))return dayStatus;
    if (!IsNativeFusionGroundZombie(zombie)) return -1;
    int specialStatus=0;
    if(GetNativeSpecialZombie269Status(zombie,specialStatus))return specialStatus;
    // Source theStatus is independent of its sparse theZombieType identity.
    // The admitted ordinary ground families have status0; death changes it to1.
    return const_cast<Zombie*>(zombie)->IsDeadOrDying() ? 1 : 0;
}

float GetNativeFusionZombieHealth(const Zombie* zombie)
{
    const auto* data = GetNativeSourceZombieData(zombie);
    return data ? data->health : zombie ? static_cast<float>(zombie->mBodyHealth) : 0.0f;
}

NativeZombieColdData* GetNativeZombieColdData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies, zombie);
    const auto found = context->coldZombies.find(id);
    return id && found != context->coldZombies.end() ? &found->second : nullptr;
}

const NativeZombieColdData* GetNativeZombieColdData(const Zombie* zombie)
{
    return GetNativeZombieColdData(const_cast<Zombie*>(zombie));
}

NativeZombieColdData* GetOrCreateNativeZombieColdData(Zombie* zombie, bool includeRetiring)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context || (zombie->mDead && !includeRetiring) || zombie->mNetGhost || !zombie->mApp || zombie->mApp->NetIsClient()) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies, zombie);
    return id ? &context->coldZombies.try_emplace(id).first->second : nullptr;
}

NativeZombieButterData* GetNativeZombieButterData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies,zombie);
    const auto found = context->butterZombies.find(id);
    return id && found != context->butterZombies.end() ? &found->second : nullptr;
}
const NativeZombieButterData* GetNativeZombieButterData(const Zombie* zombie)
{ return GetNativeZombieButterData(const_cast<Zombie*>(zombie)); }
NativeZombieButterData* GetOrCreateNativeZombieButterData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context || zombie->mDead || zombie->mNetGhost || !zombie->mApp ||
        zombie->mApp->NetIsClient() || zombie->mApp->mBoard != zombie->mBoard) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies,zombie);
    return id ? &context->butterZombies.try_emplace(id).first->second : nullptr;
}
bool AttachNativeZombieButterData(Board* board, Zombie* zombie, const NativeZombieButterData& data)
{
    auto* context = GetNativeContext(board);
    if (!context || !zombie || zombie->mBoard != board || zombie->mDead ||
        !FusionButterStateValid(data.ticks,data.visible)) return false;
    const uint32_t id = LiveId(board->mZombies,zombie);
    if (!id) return false;
    context->butterZombies[id] = data;
    return true;
}

bool AttachNativeZombieColdData(Board* board, Zombie* zombie, const NativeZombieColdData& data)
{
    auto* context = GetNativeContext(board);
    if (!context || !zombie || zombie->mBoard != board || zombie->mDead ||
        !FusionColdStateValid(data.level, data.threshold, data.chilledTicks, data.frozenTicks)) return false;
    const uint32_t id = LiveId(board->mZombies, zombie);
    if (!id) return false;
    context->coldZombies[id] = data;
    return true;
}

bool AttachNativePlant(Board* board, Plant* plant, const NativePlantData& data)
{
    auto* context = GetNativeContext(board);
    if (!context || !plant || plant->mBoard != board ||
        GetNativeSeedForPlant(data.kind) == SEED_NONE || !FusionNativeAnimationSpeedValid(data.kind, data.animationSpeed) ||
        (data.kind == 1012 || FindFusionStrongChomperProfile(data.kind) || FusionDefenderHasDamageStages(data.kind) || data.kind == 29 ? data.damageStage > 2 : data.damageStage != 0)) return false;
    const uint32_t id = LiveId(board->mPlants, plant);
    if (!id) return false;
    NativePlantData attached = data;
    if (plant->mNetGhost || (plant->mApp && plant->mApp->NetIsClient()))
    {
        if (!attached.instanceToken) return false;
    }
    else attached.instanceToken = id;
    context->plants[id] = attached;
    if (plant->mNetGhost || (plant->mApp && plant->mApp->NetIsClient()))
        RestoreNativeFireActionPresentation(plant);
    return true;
}

bool AttachNativeProjectile(Board* board, Projectile* projectile, const NativeProjectileData& data)
{
    auto* context = GetNativeContext(board);
    if (!context || !projectile || projectile->mBoard != board ||
        !NativeTravelProjectileLedgerValid369(data,GetNativeTravelBuff369(board))) return false;
    const uint32_t id = LiveId(board->mProjectiles, projectile);
    if (!id) return false;
    const auto state=context->star269Projectiles.find(id);
    const auto* star=state==context->star269Projectiles.end()?nullptr:&state->second;
    const bool freshStar=!star&&!board->mApp->NetIsClient()&&!projectile->mNetGhost&&
        NativeStar269MetadataValid(data,projectile->mProjectileType,projectile->mMotionType);
    if(!NativeProjectileMetadataValid(data,projectile->mProjectileType,projectile->mMotionType,star)&&!freshStar)return false;
    context->projectiles[id] = data;
    if(FusionStar269ProjectileClass(data.kind)&&!board->mApp->NetIsClient()&&!projectile->mNetGhost)
        return NativeStar269ProjectileCreated(projectile);
    return true;
}

NativeZombieActionData* GetNativeZombieActionData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies,zombie);
    const auto found = context->zombieActions.find(id);
    return id && found != context->zombieActions.end() ? &found->second : nullptr;
}
const NativeZombieActionData* GetNativeZombieActionData(const Zombie* zombie)
{ return GetNativeZombieActionData(const_cast<Zombie*>(zombie)); }
NativeZombieActionData* GetOrCreateNativeZombieActionData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context || zombie->mDead || zombie->mNetGhost || !zombie->mApp ||
        zombie->mApp->NetIsClient() || zombie->mApp->mBoard != zombie->mBoard) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies,zombie);
    if (!id) return nullptr;
    auto [entry,inserted] = context->zombieActions.try_emplace(id);
    if (inserted) entry->second.scale = zombie->mScaleZombie;
    return &entry->second;
}
bool AttachNativeZombieActionData(Board* board, Zombie* zombie, const NativeZombieActionData& data)
{
    auto* context = GetNativeContext(board);
    if (!context || !zombie || zombie->mBoard != board || zombie->mDead || data.colorMask > 127 ||
        !std::isfinite(data.scale) || data.scale <= 0 || data.scale > 8 ||
        !FusionPoisonStateValid({data.poisonLevel,data.poisonTicks,data.poisonDamageTicks})) return false;
    const uint32_t id = LiveId(board->mZombies,zombie);
    if (!id) return false;
    context->zombieActions[id] = data;
    return true;
}

namespace
{
bool GarlicZombieSupported(Board* board, Zombie* zombie)
{
    if (!GetNativeContext(board) || !zombie || zombie->mBoard != board || zombie->mApp != board->mApp ||
        board->mApp->mBoard != board || !NativeFusionBackgroundValid(static_cast<uint8_t>(board->mBackground)) ||
        zombie->mDead || zombie->IsDeadOrDying() || zombie->mBodyHealth <= 0 || !zombie->mHasHead ||
        !IsNativeFusionGroundZombie(zombie) || zombie->mZombiePhase != PHASE_ZOMBIE_NORMAL ||
        zombie->mZombieHeight != HEIGHT_ZOMBIE_NORMAL || zombie->mMindControlled ||
        zombie->mOnHighGround || zombie->mRow < 0 || zombie->mRow >= GetNativeFusionRowCount(board) ||
        !std::isfinite(zombie->mPosX) || !std::isfinite(zombie->mPosY)) return false;
    return board->mPlantRow[zombie->mRow]==PLANTROW_NORMAL||board->mPlantRow[zombie->mRow]==PLANTROW_POOL;
}
}
GarlicActionState* GetNativeZombieGarlicData(Zombie* zombie)
{
    auto* context = GetNativeContext(zombie ? zombie->mBoard : nullptr);
    if (!context) return nullptr;
    const uint32_t id = LiveId(zombie->mBoard->mZombies,zombie);
    const auto found = context->garlicZombies.find(id);
    return id && found != context->garlicZombies.end() ? &found->second : nullptr;
}
const GarlicActionState* GetNativeZombieGarlicData(const Zombie* zombie)
{ return GetNativeZombieGarlicData(const_cast<Zombie*>(zombie)); }
GarlicActionState* GetOrCreateNativeZombieGarlicData(Zombie* zombie)
{
    if (!zombie || !GarlicZombieSupported(zombie->mBoard,zombie) || zombie->mNetGhost || zombie->mApp->NetIsClient()) return nullptr;
    auto* context = GetNativeContext(zombie->mBoard);
    const uint32_t id = LiveId(zombie->mBoard->mZombies,zombie);
    return id ? &context->garlicZombies.try_emplace(id).first->second : nullptr;
}
bool AttachNativeZombieGarlicData(Board* board, Zombie* zombie, const GarlicActionState& data)
{
    if (!GarlicZombieSupported(board,zombie) || !NativeGarlicActionStateValid(data,GetNativeFusionRowCount(board)) ||
        FusionGarlicLogicalRow(data,zombie->mRow) != zombie->mRow) return false;
    const uint32_t id = LiveId(board->mZombies,zombie);
    if (!id) return false;
    GetNativeContext(board)->garlicZombies[id] = data;
    return true;
}
void PruneNativeZombieGarlicData(Board* board)
{
    auto* context = GetNativeContext(board);
    if (!context) return;
    for (auto it = context->garlicZombies.begin(); it != context->garlicZombies.end(); )
    {
        auto* zombie = board->mZombies.DataArrayTryToGet(it->first);
        if (!GarlicZombieSupported(board,zombie) || !NativeGarlicActionStateValid(it->second,GetNativeFusionRowCount(board)) ||
            FusionGarlicLogicalRow(it->second,zombie->mRow) != zombie->mRow)
        {
            const bool resume = zombie && FusionGarlicStopsWalking(it->second) && !zombie->mNetGhost &&
                !board->mApp->NetIsClient() && !zombie->mDead && !zombie->IsDeadOrDying() && zombie->mHasHead;
            if (zombie) RestoreNativeGarlicPresentation(zombie,{});
            it = context->garlicZombies.erase(it);
            if (resume) zombie->UpdateAnimSpeed();
        }
        else ++it;
    }
}

bool AttachNativeCoinValue(Board* board, Coin* coin, int value, bool money)
{
    auto* context = GetNativeContext(board);
    if (!context || !coin || coin->mBoard != board ||
        (money ? (!coin->IsMoney() || !FusionMoneyCoinMetadataValid(coin->mType,value)) :
            (!coin->IsSun() || !FusionSunCoinMetadataValid(coin->mType,value)))) return false;
    const uint32_t id = LiveId(board->mCoins, coin);
    if (!id) return false;
    context->coinValues[id] = NativeCoinData(value,money);
    return true;
}

int GetNativeCoinValue(const Coin* coin, int fallback)
{
    const auto* context = GetNativeContext(coin ? coin->mBoard : nullptr);
    if (!context) return fallback;
    // ScoreCoin marks the native Coin dead before asking its value. The live
    // DataArray generation, rather than mDead, identifies that final credit.
    const uint32_t id = LiveId(coin->mBoard->mCoins, coin);
    const auto found = context->coinValues.find(id);
    return id && found != context->coinValues.end() ? found->second.value : fallback;
}

bool IsNativeMoneyCoin(const Coin* coin)
{
    const auto* context = GetNativeContext(coin ? coin->mBoard : nullptr);
    if (!context) return false;
    const uint32_t id = LiveId(coin->mBoard->mCoins,coin);
    const auto found = context->coinValues.find(id);
    return id && found != context->coinValues.end() && found->second.money;
}

bool ClaimNativeMoneyCoinValue(Coin* coin, int& value)
{
    auto* context = GetNativeContext(coin ? coin->mBoard : nullptr);
    if (!context || !coin->IsMoney() || coin->mNetGhost || !coin->mApp ||
        coin->mApp->NetIsClient() || coin->mApp->mBoard != coin->mBoard) return false;
    const uint32_t id = LiveId(coin->mBoard->mCoins,coin);
    const auto found = context->coinValues.find(id);
    if (!id || found == context->coinValues.end() || !found->second.money) return false;
    value = found->second.value;
    found->second.value = 0; // Keep the typed claimed tombstone until the native generation retires.
    return true;
}

void ApplyNativeSourceSquashDamage(Zombie* zombie,int damage)
{
    if(damage<=0||!zombie||!IsNativeFusionBoard(zombie->mBoard)||zombie->mNetGhost||
        zombie->mApp->NetIsClient()||zombie->mDead||zombie->IsDeadOrDying()||zombie->mMindControlled)return;
    if(NativeSummon269MoneyDamage(zombie,damage))return;
    // Source damage11 sends the full operand to second armor, then compares
    // that same operand with body+first armor (not the shield remainder).
    if(zombie->mShieldType!=SHIELDTYPE_NONE)zombie->TakeShieldDamage(damage,2U);
    const bool garg=zombie->mZombieType==ZOMBIE_GARGANTUAR||zombie->mZombieType==ZOMBIE_REDEYE_GARGANTUAR;
    if(!garg&&float(damage)>=GetNativeFusionZombieHealth(zombie)+float(zombie->mHelmHealth))zombie->DieWithLoot();
    else zombie->TakeDamage(damage,1U); // Second armor was already processed exactly once.
}
}
