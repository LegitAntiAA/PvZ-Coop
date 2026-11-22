#include "FusionNative.h"
#include "FusionNativeCapturedSourcePlant314.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativePlacement.h"
#include "FusionNativeVisual.h"
#include "FusionNativeGlove.h"
#include "FusionDefenderRules.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeWater268.h"
#include "FusionWater268Rules.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeMoneyAction.h"
#include "FusionNativeTools268.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeLastPlants269.h"
#include "FusionMorePlants269Rules.h"
#include "FusionRecipe.h"
#include "FusionWire.h"
#include "../Board.h"
#include "../Coin.h"
#include "FusionNativeAdventure273.h"
#include "FusionCampaign272Rules.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../SeedPacket.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../SexyAppFramework/Image.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>

namespace LawnFusion
{
namespace
{
bool Host(const Board* board)
{
    return board && board->mApp && (!board->mApp->NetActive() || board->mApp->NetIsHost());
}

bool Active(const FusionBankState& state, int slot)
{
    return slot >= 0 && slot < static_cast<int>(kFusionBankPlayers) &&
        (state.activeMask & (1U << slot)) != 0;
}

using PlacementLayer = FusionDefenderLayer;
bool LayerForKind(uint16_t kind, PlacementLayer& layer)
{
    const SeedType seed = GetNativeSeedForPlant(kind);
    if(IsFusionSourceFlyingKind(kind)&&seed!=SEED_NONE){layer=PlacementLayer::Flying;return true;}
    if (seed == SEED_NONE || Plant::IsFlying(seed)) return false;
    const auto* profile = FindFusionDefenderProfile(kind);
    const auto value = seed==SEED_LILYPAD?PlacementLayer::Under:profile ? profile->layer : PlacementLayer::Body;
    // The source profile and the real Board slot classification must agree.
    if ((seed == SEED_FLOWERPOT || seed == SEED_LILYPAD) != (value == PlacementLayer::Under) ||
        (seed == SEED_PUMPKINSHELL) != (value == PlacementLayer::Pumpkin)) return false;
    layer = value; return true;
}
Plant* LayerActor(const PlantsOnLawn& cell, PlacementLayer layer)
{
    switch (layer)
    {
    case PlacementLayer::Body: return cell.mNormalPlant;
    case PlacementLayer::Under: return cell.mUnderPlant;
    case PlacementLayer::Pumpkin: return cell.mPumpkinPlant;
    case PlacementLayer::Flying:return cell.mFlyingPlant;
    }
    return nullptr;
}
bool CurrentLayerActor(Board* board, const Plant* plant, PlacementLayer layer)
{
    if (!plant) return true;
    const auto* data = GetNativePlantData(plant);
    PlacementLayer actual;
    if (!data || !data->instanceToken || !plant->mNetId || plant->mBoard != board || plant->mDead ||
        !plant->mIsOnBoard || plant->mSquished || plant->mBurnedCounter != -1 || plant->mPlantHealth <= 0 ||
        plant->mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE ||
        !LayerForKind(data->kind,actual) || actual != layer || plant->mSeedType != GetNativeSeedForPlant(data->kind)) return false;
    // Client tokens are opaque host identities, never local DataArray IDs.
    return !Host(board) || (!plant->mNetGhost && board->mPlants.DataArrayTryToGet(data->instanceToken) == plant);
}
struct PlacementPlan
{
    Plant* parent = nullptr;
    uint16_t kind = kFusionNoPlant, firstParent = kFusionNoPlant;
    std::vector<Plant*> puffParents;
};
bool PuffMixCapacity(Board* board,uint16_t kind,size_t count)
{
    // Legacy Bowling adds a native blink/attachment during PlantInitialize.
    // Its different initializer peak is outside this grass-placement proof.
    if(board->mApp->IsWallnutBowlingLevel())return false;
    const SeedType seed=GetNativeSeedForPlant(kind);
    unsigned heads=0;
    switch(seed)
    {
    case SEED_PEASHOOTER:heads=1;break;
    case SEED_WALLNUT:case SEED_PUFFSHROOM:case SEED_SUNSHROOM:case SEED_HYPNOSHROOM:break;
    default:return false; // Other initializers need their own proven peak.
    }
    const auto plain=[](ReanimationType type)
    {
        ReanimatorEnsureDefinitionLoaded(type,false);
        if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return false;
        const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
        if(!tracks.tracks||tracks.count<=0)return false;
        for(int i=0;i<tracks.count;++i)
            if(tracks.tracks[i].mName&&strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)return false;
        return true;
    };
    const bool sleep=Plant::IsNocturnal(seed)&&!board->StageIsNight();
    if(!plain(GetPlantDefinition(seed).mReanimationType)||(heads&&!plain(REANIM_PEASHOOTER))||
        (sleep&&!plain(REANIM_SLEEPING)))return false;
    auto* effects=board->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)return false;
    const auto room=[](const auto& pool,size_t need)
    {return pool.mBlock&&pool.mSize<=pool.mMaxSize&&need<=size_t(pool.mMaxSize-pool.mSize);};
    return room(board->mPlants,count)&&
        room(effects->mReanimationHolder->mReanimations,count*(1+heads+unsigned(sleep)))&&
        room(effects->mAttachmentHolder->mAttachments,count*heads);
}
bool WaterKind(uint16_t kind)
{ return kind==12||kind==19||kind==1077||kind==1078||kind==1086||kind==1101||IsFusionWater268WaterPlant(kind); }
bool TerrainAllows(Board* board,int column,int row,uint16_t kind,const PlantsOnLawn& cell,const Plant* replaced=nullptr)
{
    const auto tile=board->GetGridSquareTypeAt(column,row);
    if(IsFusionSourceFlyingKind(kind))return tile==GRIDSQUARE_GRASS||tile==GRIDSQUARE_POOL;
    if(tile==GRIDSQUARE_GRASS)return !WaterKind(kind);
    if(tile!=GRIDSQUARE_POOL)return false;
    if(!WaterKind(kind))
    {
        const auto* lily=GetNativePlantData(cell.mUnderPlant);
        return lily&&lily->kind==12&&!cell.mUnderPlant->mDead;
    }
    // Source water plants require an empty non-flying cell. A consumed recipe
    // parent is excluded; the incoming land card is not the final terrain type.
    Plant* other=nullptr;
    while(board->mPlants.IterateNext(other))
        if(other!=replaced&&!other->mDead&&other->mIsOnBoard&&other->mPlantCol==column&&other->mRow==row&&
            !Plant::IsFlying(other->mSeedType))return false;
    return true;
}
bool CurrentMovingActor(Board* board, const Plant* moving)
{
    const auto* data = GetNativePlantData(moving);
    PlacementLayer layer;
    if (!board || !moving || !data || !LayerForKind(data->kind,layer) ||
        !board->IsValidGridCell(moving->mPlantCol,moving->mRow) ||
        (board->GetGridSquareTypeAt(moving->mPlantCol,moving->mRow) != GRIDSQUARE_GRASS &&
            board->GetGridSquareTypeAt(moving->mPlantCol,moving->mRow) != GRIDSQUARE_POOL) ||
        board->GetLadderAt(moving->mPlantCol,moving->mRow) ||
        board->GetGraveStoneAt(moving->mPlantCol,moving->mRow) || board->GetCraterAt(moving->mPlantCol,moving->mRow) ||
        board->GetScaryPotAt(moving->mPlantCol,moving->mRow) || board->IsIceAt(moving->mPlantCol,moving->mRow) ||
        NativeBatchPlantUntargetable(moving) || !CurrentLayerActor(board,moving,layer)) return false;
    PlantsOnLawn source;
    board->GetPlantsOnLawn(moving->mPlantCol,moving->mRow,&source);
    return !source.mFlyingPlant &&
        (IsFusionSourcePresentOverlayKind(data->kind)||LayerActor(source,layer)==moving);
}
bool PresentAllowsCell(Board* board,int column,int row,const Plant* moving)
{
    bool recipient=false,recipe=false;Plant* other=nullptr;
    while(board->IteratePlants(other))
    {
        if(other==moving||other->mDead||!other->mIsOnBoard||other->mPlantCol!=column||other->mRow!=row)continue;
        const auto* data=GetNativePlantData(other);PlacementLayer layer;
        if(!data||!LayerForKind(data->kind,layer)||!CurrentLayerActor(board,other,layer))return false;
        bool flying=false,pumpkin=false,pot=false,lily=false;
        if(!GetNativeRemaining269PlantTags(other,flying,pumpkin,pot,lily))return false;
        if(flying||pumpkin||pot||lily)continue;
        recipient=true;
        for(uint16_t base:FusionPresent269AllCandidates())if(FindFusionRecipe(data->kind,base)){recipe=true;break;}
    }
    // PresentCheck44AB50: empty/excluded-only cell, or at least one real
    // recipient with a base0..32 recipe. No selection RNG or recipient removal.
    return !recipient||recipe;
}
bool CellHasPresent(Board* board,int column,int row)
{
    Plant* plant=nullptr;
    while(board->IteratePlants(plant))if(!plant->mDead&&plant->mIsOnBoard&&plant->mPlantCol==column&&plant->mRow==row)
        if(const auto* data=GetNativePlantData(plant);data&&IsFusionSourcePresentOverlayKind(data->kind))return true;
    return false;
}
bool MakePlacementPlan(Board* board, int column, int row, uint16_t incoming, PlacementPlan& plan,
    const Plant* moving = nullptr, bool worldSpawn = false)
{
    PlacementLayer incomingLayer;
    if (!board || !board->mApp || board->mApp->mBoard != board || !IsNativeFusionBoard(board) ||
        !board->IsValidGridCell(column,row) ||
        (board->mPlantRow[row]!=PLANTROW_NORMAL&&board->mPlantRow[row]!=PLANTROW_POOL) ||
        (board->GetGridSquareTypeAt(column,row) != GRIDSQUARE_GRASS && board->GetGridSquareTypeAt(column,row) != GRIDSQUARE_POOL) ||
        board->GetGraveStoneAt(column,row) || board->GetCraterAt(column,row) ||
        board->GetScaryPotAt(column,row) || board->IsIceAt(column,row) || !LayerForKind(incoming,incomingLayer) ||
        (moving && (board->GetLadderAt(column,row) || !CurrentMovingActor(board,moving) || GetNativePlantData(moving)->kind != incoming ||
            (moving->mPlantCol == column && moving->mRow == row)))) return false;
    if(incoming==900&&!CanPlaceNativeEmperor269(board,row))return false;
    if(incoming==244||incoming==245)
    {Plant* other=nullptr;while(board->IteratePlants(other))if(other!=moving&&!other->mDead&&other->mPlantCol==column&&other->mRow==row)
        if(const auto* d=GetNativePlantData(other);d&&(d->kind==244||d->kind==245))return false;
     plan={nullptr,incoming,kFusionNoPlant};return true;}
    PlantsOnLawn cell;
    board->GetPlantsOnLawn(column,row,&cell);
    if(IsFusionSourcePresentOverlayKind(incoming))
    {
        if(!PresentAllowsCell(board,column,row,moving))return false;
        plan={nullptr,incoming,kFusionNoPlant};return true;
    }
    // Source CheckMix445B60 refuses a body mix while a live Present owns its
    // pending same-cell event. Other native support/layer policies stay intact.
    if(incomingLayer==PlacementLayer::Body&&CellHasPresent(board,column,row))return false;
    if(!moving&&cell.mNormalPlant)
        if(const auto* d=GetNativePlantData(cell.mNormalPlant);d&&d->kind==243)
        {
            uint8_t next=0;int health=0;float interval=0;
            if(TryAdvanceFusionHamburger269(uint8_t(d->batchBagCount),incoming,next,health,interval))
            {plan={cell.mNormalPlant,243,243};return true;}
        }
    if(incomingLayer==PlacementLayer::Flying&&cell.mFlyingPlant&&cell.mFlyingPlant!=moving)return false;
    constexpr PlacementLayer layers[] = {PlacementLayer::Body,PlacementLayer::Under,PlacementLayer::Pumpkin,PlacementLayer::Flying};
    for (auto layer : layers) if (!CurrentLayerActor(board,LayerActor(cell,layer),layer)) return false;
    if (incomingLayer==PlacementLayer::Body&&IsFusionSourcePuffKind(incoming)&&
        (moving||worldSpawn||IsNativeBasePlantKind(incoming)))
    {
        int count=0;bool compatible=true;Plant* occupant=nullptr;
        while(board->mPlants.IterateNext(occupant))
        {
            if(occupant==moving||occupant->mDead||!occupant->mIsOnBoard||
                occupant->mPlantCol!=column||occupant->mRow!=row)continue;
            const auto* data=GetNativePlantData(occupant);PlacementLayer layer;
            if(!data||!LayerForKind(data->kind,layer)||!CurrentLayerActor(board,occupant,layer))return false;
            if(layer==PlacementLayer::Body)
            {
                if(!IsFusionSourcePuffKind(data->kind))compatible=false;
                else ++count;
            }
        }
        if(compatible&&count<3&&TerrainAllows(board,column,row,incoming,cell,moving))
        {plan={nullptr,incoming,kFusionNoPlant};return true;}
    }
    enum class RecipeChoice { NoRecipe, Selected, Rejected };
    auto recipeAt = [&](PlacementLayer layer)
    {
        Plant* parent = LayerActor(cell,layer);
        if (!parent) return RecipeChoice::NoRecipe;
        const auto* data = GetNativePlantData(parent);
        if (!data) return RecipeChoice::Rejected;
        const auto* recipe = FindFusionRecipe(data->kind,incoming);
        if (!recipe) return RecipeChoice::NoRecipe;
        PlacementLayer destination;
        if (!LayerForKind(recipe->result,destination) || (moving && NativeBatchPlantUntargetable(parent))) return RecipeChoice::Rejected;
        Plant* occupant = LayerActor(cell,destination);
        if (occupant && occupant != parent) return RecipeChoice::Rejected;
        if(!TerrainAllows(board,column,row,recipe->result,cell,parent)||
            (recipe->result==900&&!CanPlaceNativeEmperor269(board,row)))return RecipeChoice::Rejected;
        plan = {parent,recipe->result,data->kind};
        if(layer==PlacementLayer::Body&&IsFusionSourcePuffKind(data->kind)&&IsFusionSourcePuffKind(recipe->result))
        {
            // CheckMix snapshots old recipients; newly born Puff results must
            // never become the next parent of the same incoming card.
            Plant* old=nullptr;
            while(board->mPlants.IterateNext(old))
            {
                if(old==moving||old->mDead||!old->mIsOnBoard||old->mPlantCol!=column||old->mRow!=row)continue;
                const auto* d=GetNativePlantData(old);PlacementLayer oldLayer;
                if(!d||!LayerForKind(d->kind,oldLayer))return RecipeChoice::Rejected;
                if(oldLayer!=PlacementLayer::Body)continue;
                if(!CurrentLayerActor(board,old,oldLayer)||!IsFusionSourcePuffKind(d->kind))return RecipeChoice::Rejected;
                plan.puffParents.push_back(old);
            }
            if(plan.puffParents.size()>1)
            {
                // Base SmallPuff has an empty DieEvent and no source temporary
                // or dieMeansLose flag. Hybrid removal callbacks need their
                // own batch proof before they can mutate additional recipients.
                for(auto* old:plan.puffParents)
                    if(GetNativePlantData(old)->kind!=6||NativeBatchPlantUntargetable(old))return RecipeChoice::Rejected;
            }
        }
        return RecipeChoice::Selected;
    };
    // Source-directed recipes, first the incoming layer, then a fixed order.
    // A known blocked/unsupported recipe rejects the action, rather than
    // silently becoming a different recipe or an ordinary direct placement.
    auto choice = recipeAt(incomingLayer);
    if (choice != RecipeChoice::NoRecipe) return choice == RecipeChoice::Selected;
    for (auto layer : layers)
        if (layer != incomingLayer)
        {
            choice = recipeAt(layer);
            if (choice != RecipeChoice::NoRecipe) return choice == RecipeChoice::Selected;
        }
    if ((!moving && !worldSpawn && !IsNativeBasePlantKind(incoming)) || LayerActor(cell,incomingLayer)) return false;
    if(!TerrainAllows(board,column,row,incoming,cell,moving))return false;
    const SeedType seed = GetNativeSeedForPlant(incoming);
    const auto reason = incomingLayer==PlacementLayer::Flying||cell.mFlyingPlant||board->GetGridSquareTypeAt(column,row)==GRIDSQUARE_POOL?PLANTING_OK:board->CanPlantAt(column,row,seed);
    if (reason != PLANTING_OK && !(moving && reason == PLANTING_NEEDS_UPGRADE && Plant::IsUpgrade(seed))) return false;
    plan = {nullptr,incoming,kFusionNoPlant}; return true;
}

// Reconcile chooser identities with actual SeedPackets, retaining their native
// IDs and live recharge counters. Selection IDs never share the entity ID space.
bool SyncPackets(Board* board, NativeContext& context)
{
    if (!Host(board) || !board->mSeedBank) return false;
    SeedBank* bank = board->mSeedBank;
    const auto& state = context.selection.State();
    if (state.packets.size() > SEEDBANK_CAPACITY) return false;
    std::vector<uint16_t> visualKinds;
    for (const auto& selected : state.packets)
        if (!selected.mysteryPending && selected.kind != kFusionNoPlant) visualKinds.push_back(selected.kind);
    std::string visualError;
    if (!EnsureNativeVisuals(board,visualKinds,{},&visualError))
    {
        LawnNet::NetTrace("Fusion native bank resource admission rejected error=%s",visualError.c_str());
        return false;
    }
    std::vector<SeedPacket> packets;
    std::unordered_map<uint16_t, NativePacketData> metadata;
    std::unordered_map<uint16_t, int> oldIndices;
    for (int i = 0; i < bank->mNumPackets && i < SEEDBANK_CAPACITY; ++i)
        oldIndices.emplace(bank->mSeedPackets[i].mNetId, i);
    packets.reserve(state.packets.size());
    for (const auto& selected : state.packets)
    {
        const SeedPacket* previous = nullptr;
        const NativePacketData* oldData = nullptr;
        for (int i = 0; i < bank->mNumPackets && i < SEEDBANK_CAPACITY; ++i)
        {
            const auto found = context.packets.find(bank->mSeedPackets[i].mNetId);
            if (found != context.packets.end() && found->second.selectionId == selected.id)
            { previous = &bank->mSeedPackets[i]; oldData = &found->second; break; }
        }
        SeedPacket packet = previous ? *previous : SeedPacket();
        packet.mApp = board->mApp;
        packet.mBoard = board;
        packet.mNetGhost = false;
        if (!previous)
        {
            packet.mNetId = board->NetAllocId();
            if (!packet.mNetId) return false;
        }
        const bool changed = !oldData || oldData->kind != selected.kind || oldData->isExtra != selected.isExtra;
        if (changed)
        {
            if (selected.mysteryPending) packet.SetMysteryPlaceholder(selected.randomOrigin);
            else
            {
                const SeedType seed = GetNativeSeedForPlant(selected.kind);
                if (seed == SEED_NONE) return false;
                packet.SetPacketType(seed, SEED_NONE);
                // Source CardUI starts with CD==fullCD, including Cherry50s.
                // SetPacketType also applies Classic's initial long-card wait;
                // initialize only this new/revealed identity as source-ready.
                // Retained identities keep their real native recharge below.
                packet.mRefreshCounter = 0;
                packet.mRefreshTime = 0;
                packet.mRefreshing = false;
                packet.mActive = true;
            }
        }
        packet.mIndex = static_cast<int>(packets.size());
        packet.mNetOwnerSlot = selected.owner;
        packet.mRandomDiscountLevel = selected.randomOrigin;
        NativePacketData data;
        data.selectionId = selected.id;
        data.kind = selected.kind;
        data.isExtra = selected.isExtra;
        data.owner = selected.owner;
        data.randomOrigin = selected.randomOrigin;
        data.price = selected.price;
        data.rechargeSeconds = selected.rechargeSeconds;
        metadata.emplace(packet.mNetId, data);
        packets.push_back(packet);
    }
    // Keep retained cards in their old slots until the shared bank sorter can
    // retarget held cursors by ID. Selection order is not a physical slot.
    std::stable_sort(packets.begin(), packets.end(), [&](const SeedPacket& first, const SeedPacket& second)
    {
        const auto a = oldIndices.find(first.mNetId), b = oldIndices.find(second.mNetId);
        return (a == oldIndices.end() ? SEEDBANK_CAPACITY : a->second) <
            (b == oldIndices.end() ? SEEDBANK_CAPACITY : b->second);
    });
    bank->mNumPackets = static_cast<int>(packets.size());
    for (int i = 0; i < bank->mNumPackets; ++i)
    {
        bank->mSeedPackets[i] = packets[i];
    }
    for (int i = bank->mNumPackets; i < SEEDBANK_CAPACITY; ++i)
    {
        bank->mSeedPackets[i].SetPacketType(SEED_NONE, SEED_NONE);
        bank->mSeedPackets[i].mNetId = 0;
    }
    bank->mWidth = IMAGE_SEEDBANK->GetWidth() + board->GetSeedBankExtraWidth();
    context.packets = std::move(metadata);
    bank->SortPacketsForViewer(board->mApp->NetActive() ? board->mApp->mNetManager->GetMySlot() : 0);
    return true;
}

bool PlantPacket(Board* board, NativeContext& context, int slot, const FusionCommand& command)
{
    if (context.selection.State().phase != FusionBankPhase::Playing ||
        board->mApp->mGameScene != SCENE_PLAYING || board->mPaused ||
        !board->mSeedBank || command.targetId > UINT16_MAX) return false;

    SeedPacket* packet = nullptr;
    for (int i = 0; i < board->mSeedBank->mNumPackets && i < SEEDBANK_CAPACITY; ++i)
        if (board->mSeedBank->mSeedPackets[i].mNetId == command.packetId)
        { packet = &board->mSeedBank->mSeedPackets[i]; break; }
    const NativePacketData* data = GetNativePacketData(packet);
    if (!packet || !data || data->owner != slot || !packet->mNetId ||
        !packet->mActive || packet->mRefreshing ||
        !std::isfinite(data->rechargeSeconds) || data->rechargeSeconds < 0.0f ||
        GetNativeSeedForPlant(data->kind) == SEED_NONE || packet->mPacketType != GetNativeSeedForPlant(data->kind) ||
        !board->CanTakeSunMoney(board->GetSeedPacketCost(*packet))) return false;

    NativePlantData incoming;
    incoming.kind = data->kind;
    incoming.packetId = packet->mNetId;
    incoming.owner = data->owner;
    incoming.randomOrigin = data->randomOrigin;
    if (!TryPlaceNativeFusionPlant(board, command.column, command.row, incoming,
        static_cast<uint16_t>(command.targetId), slot, command.targetToken)) return false;
    board->TakeSunMoney(board->GetSeedPacketCost(*packet));
    packet->WasPlanted();
    board->OnSeedPacketPlanted(packet->mPacketType);
    return true;
}

template <typename T, typename Value> void Prune(DataArray<T>& array, std::unordered_map<uint32_t, Value>& metadata)
{
    for (auto item = metadata.begin(); item != metadata.end(); )
    {
        if (!array.DataArrayTryToGet(item->first)) item = metadata.erase(item);
        else ++item;
    }
}
}

bool ResolveNativeFusionPlacementTarget(Board* board, int column, int row, uint16_t incoming, Plant*& target, bool worldSpawn)
{
    PlacementPlan plan;
    if (!MakePlacementPlan(board,column,row,incoming,plan,nullptr,worldSpawn)) return false;
    target = plan.parent; return true;
}
bool ResolveNativeFusionMoveTarget(Board* board, int column, int row, const Plant* moving, Plant*& target)
{
    const auto* data = GetNativePlantData(moving);
    PlacementPlan plan;
    if (!data || !MakePlacementPlan(board,column,row,data->kind,plan,moving)) return false;
    target = plan.parent; return true;
}

Plant* TryPlaceNativeFusionPlant(Board* board, int column, int row,
    const NativePlantData& incoming, uint16_t expectedTargetId, int requestingSlot, uint32_t expectedTargetToken,
    Plant* movingSource)
{
    NativeContext* context = GetNativeContext(board);
    if (!context || !Host(board) || board->mApp->mBoard != board ||
        context->selection.State().phase != FusionBankPhase::Playing ||
        board->mApp->mGameScene != SCENE_PLAYING || board->mPaused ||
        !board->IsValidGridCell(column, row) ||
        (board->mPlantRow[row] != PLANTROW_NORMAL && board->mPlantRow[row] != PLANTROW_POOL) ||
        requestingSlot < -1 || (requestingSlot >= 0 &&
            (!Active(context->selection.State(), requestingSlot) || (!movingSource && incoming.owner != requestingSlot)))) return nullptr;
    const bool validOwner = incoming.owner < kFusionBankPlayers ||
        (incoming.owner == LawnNet::NET_SHARED_OWNER && incoming.packetId == 0 && incoming.randomOrigin == 0);
    if (!validOwner || incoming.randomOrigin > 10 || (!incoming.packetId && incoming.randomOrigin)) return nullptr;
    if (movingSource)
    {
        if (requestingSlot < 0 || !CurrentMovingActor(board,movingSource)) return nullptr;
        const auto* source = GetNativePlantData(movingSource);
        const auto& held = context->gloveHolds[requestingSlot];
        if (held.plantId != movingSource->mNetId || held.instanceToken != source->instanceToken ||
            IsNativeGlovePlantReserved(board,movingSource,requestingSlot) ||
            source->kind != incoming.kind || source->packetId != incoming.packetId ||
            source->owner != incoming.owner || source->randomOrigin != incoming.randomOrigin) return nullptr;
    }
    if (GetNativeSeedForPlant(incoming.kind) == SEED_NONE)
    {
        LawnNet::NetTrace("Fusion native placement rejected reason=unsupported-incoming kind=%u column=%d row=%d slot=%d",
            incoming.kind, column, row, requestingSlot);
        return nullptr;
    }
    PlacementPlan plan;
    const auto* heldCard=requestingSlot>=0?GetNativeDroppedCard269Data(GetNativeDroppedCard269Held(board,requestingSlot)):nullptr;
    const bool sourceCard=heldCard&&heldCard->kind==incoming.kind&&!incoming.packetId&&!incoming.randomOrigin;
    if (!MakePlacementPlan(board,column,row,incoming.kind,plan,movingSource,requestingSlot == -1||sourceCard)) return nullptr;
    if ((!movingSource || plan.parent) && board->mPlants.mSize >= board->mPlants.mMaxSize)
    {
        LawnNet::NetTrace("Fusion native placement rejected reason=capacity kind=%u column=%d row=%d slot=%d",
            incoming.kind, column, row, requestingSlot);
        return nullptr;
    }

    Plant* parent = plan.parent;
    const FusionPlantId result = plan.kind;
    if (parent)
    {
        const NativePlantData* parentData = GetNativePlantData(parent);
        if (!expectedTargetId || parent->mNetId != expectedTargetId || !parentData ||
            (requestingSlot >= 0 && !expectedTargetToken) ||
            (expectedTargetToken && expectedTargetToken != parentData->instanceToken) ||
            board->mPlants.DataArrayTryToGet(parentData->instanceToken) != parent ||
            IsNativeGlovePlantReserved(board, parent, requestingSlot) ||
            parent->mDead || parent->mNetGhost || parent->mSquished || parent->mPlantHealth <= 0 ||
            parent->mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE ||
            board->GetGraveStoneAt(column, row) || board->GetCraterAt(column, row)) return nullptr;
        if (parentData->kind != plan.firstParent) return nullptr;
        if(parentData->kind==243&&result==243)
            return TryUpgradeNativeHamburger269(parent,incoming.kind)?parent:nullptr;
    }
    else
    {
        if (expectedTargetId || expectedTargetToken) return nullptr;
    }
    if (movingSource && !parent)
    {
        const int oldColumn = movingSource->mPlantCol, oldRow = movingSource->mRow;
        board->MovePlant(movingSource,column,row);
        RebuildNativeEquipmentGraph(board);
        const auto* data = GetNativePlantData(movingSource);
        LawnNet::NetTrace("Fusion native placement moved kind=%u actor=%u token=%u from=%d,%d to=%d,%d hp=%d launch=%d shooting=%d pending=%d slot=%d",
            incoming.kind,movingSource->mNetId,data->instanceToken,oldColumn,oldRow,column,row,movingSource->mPlantHealth,
            movingSource->mLaunchCounter,movingSource->mShootingCounter,data->pendingProductionTicks,requestingSlot);
        return movingSource; // Same full generation, HP, ancestry and native clocks; no OnCreate callback.
    }

    NativePlantData fresh;
    fresh.kind = result;
    fresh.firstParent = plan.firstParent;
    fresh.secondParent = parent ? incoming.kind : kFusionNoPlant;
    fresh.packetId = incoming.packetId;
    fresh.owner = incoming.owner;
    fresh.randomOrigin = incoming.randomOrigin;
    if (requestingSlot == -1 && !parent && !IsNativeBasePlantKind(result))
    {
        fresh.packetId=0; fresh.owner=LawnNet::NET_SHARED_OWNER; fresh.randomOrigin=0;
    }
    // All ordinary rejection checks precede allocation/RNG and parent removal.
    // The real factory initializes a fresh ability state/token before commit.
    std::string visualError;
    if (!EnsureNativePlantVisuals(board,result,&visualError))
    {
        LawnNet::NetTrace("Fusion native placement resource admission rejected kind=%u error=%s",result,visualError.c_str());
        return nullptr;
    }
    if(!plan.puffParents.empty()&&plan.firstParent==6)
    {
        for(auto* old:plan.puffParents)
        {
            const auto* d=GetNativePlantData(old);
            if(!CurrentLayerActor(board,old,PlacementLayer::Body)||!d||d->kind!=plan.firstParent||
                IsNativeGlovePlantReserved(board,old,requestingSlot))return nullptr;
        }
        if(!PuffMixCapacity(board,result,plan.puffParents.size()))
        {LawnNet::NetTrace("Fusion native multi-Puff deferred result=%u count=%zu reason=whole-initializer-peak",result,plan.puffParents.size());return nullptr;}
        Plant* first=nullptr;
        for(auto* old:plan.puffParents)
        {
            const auto oldId=old->mNetId;
            old->Die(2); // Source CheckMix removes this old recipient BEFORE SetPlant.
            auto* child=board->AddPlant(column,row,GetNativeSeedForPlant(result),SEED_NONE,&fresh);
            const auto* bound=GetNativePlantData(child);
            if(!child||child->mDead||!child->mNetId||!bound||!bound->instanceToken||
                board->mPlants.DataArrayTryToGet(bound->instanceToken)!=child||
                child->mSeedType!=GetNativeSeedForPlant(result)||bound->kind!=fresh.kind||
                bound->firstParent!=fresh.firstParent||bound->secondParent!=fresh.secondParent||
                bound->packetId!=fresh.packetId||bound->owner!=fresh.owner||bound->randomOrigin!=fresh.randomOrigin)
            {
                if(child&&!child->mDead)child->Die(255);
                // A post-preflight factory failure is not a capacity retry:
                // source parents have already been consumed. Stop the source
                // controller instead of replaying a partially committed group.
                if(context->adventure.enabled)
                {context->adventure.schedulerFailed=true;context->adventureScheduler.failed=true;}
                RebuildNativeEquipmentGraph(board);
                LawnNet::NetTrace("Fusion native multi-Puff failed after mutation result=%u old=%u partial=%d",result,oldId,first!=nullptr);
                return nullptr;
            }
            if(!first)first=child;
            NativeBatchPlantPlaced(child);
            LawnNet::NetTrace("Fusion native multi-Puff child incoming=%u result=%u old=%u child=%u token=%u first=%u second=%u column=%d row=%d slot=%d",
                incoming.kind,result,oldId,child->mNetId,bound->instanceToken,fresh.firstParent,fresh.secondParent,column,row,requestingSlot);
        }
        if(movingSource)movingSource->Die(2);
        RebuildNativeEquipmentGraph(board);
        return first; // PlantPacket charges/recharges the one incoming card once.
    }
    Plant* plant = board->AddPlant(column, row, GetNativeSeedForPlant(result), SEED_NONE, &fresh);
    if (!plant) return nullptr;
    const auto* actual = GetNativePlantData(plant);
    if (plant->mDead || !plant->mNetId || !actual || !actual->instanceToken ||
        board->mPlants.DataArrayTryToGet(actual->instanceToken) != plant ||
        plant->mSeedType != GetNativeSeedForPlant(result) || actual->kind != fresh.kind ||
        actual->firstParent != fresh.firstParent || actual->secondParent != fresh.secondParent ||
        actual->packetId != fresh.packetId || actual->owner != fresh.owner || actual->randomOrigin != fresh.randomOrigin)
    {
        // Construction failure must not trigger a gameplay death callback.
        plant->Die(255);
        LawnNet::NetTrace("Fusion native placement rejected reason=factory-bind kind=%u column=%d row=%d slot=%d",
            result, column, row, requestingSlot);
        return nullptr;
    }
    const uint32_t token = actual->instanceToken;
    if (parent) parent->Die(2); // Source ByMix; child exists before effects.
    if (movingSource) movingSource->Die(2);
    NativeBatchPlantPlaced(plant);
    RebuildNativeEquipmentGraph(board);
    LawnNet::NetTrace("Fusion native placement committed incoming=%u result=%u child=%u token=%u parent=%u first=%u second=%u column=%d row=%d slot=%d",
        incoming.kind,result,plant->mNetId,token,expectedTargetId,fresh.firstParent,fresh.secondParent,column,row,requestingSlot);
    return plant;
}

Plant* TrySpawnNativeFusionPlant(Board* board, int column, int row,
    const NativePlantData& incomingProvenance)
{
    if (!board || !board->mApp || !board->IsValidGridCell(column, row)) return nullptr;
    PlacementPlan plan;
    if (!MakePlacementPlan(board,column,row,incomingProvenance.kind,plan,nullptr,true)) return nullptr;
    Plant* parent=plan.parent;
    const auto* data = GetNativePlantData(parent);
    return TryPlaceNativeFusionPlant(board,column,row,incomingProvenance,parent ? parent->mNetId : 0,-1,
        data ? data->instanceToken : 0);
}

bool InitializeNativeBoardContext(Board* board, uint32_t epoch, uint32_t seed,
    int level, int difficulty, uint8_t activeMask)
{
    if (!board || !board->mApp || board->mApp->GetContentMode() != ContentMode::Fusion ||
        board->mApp->mFusionRuntime || !epoch || level < 1) return false;
    if (board->mFusionNative)
    {
        const auto& existing = *board->mFusionNative;
        return existing.epoch == epoch && existing.seed == seed && existing.level == level &&
            existing.selection.State().difficulty == difficulty && existing.selection.State().activeMask == activeMask;
    }
    auto context = std::make_unique<NativeContext>();
    context->epoch = epoch;
    context->seed = seed;
    context->level = level;
    context->adventure = board->mApp->mSourceAdventure;
    context->validation = !context->adventure.enabled&&board->mApp->IsFusionValidationFixture();
    const auto* sourceLevel=context->adventure.enabled?FindFusionSourceAdventure272(context->adventure.run.level):nullptr;
    if(context->adventure.enabled&&!sourceLevel)return false;
    for (size_t index = 0; index < FusionPlantCount(); ++index)
    {
        const auto* plant = FusionPlantAt(index);
        if (!plant || !IsNativeBasePlantKind(plant->id)) continue;
        const FusionPlantId kind = plant->id;
        if(context->adventure.enabled)
        {
            if(!FusionAdventureBaseUnlocked273(context->adventure.profile,kind))continue;
        }
        else if(!context->validation)
        {
            // Carrier seeds are implementation aliases (Torchwood uses Pea),
            // so unlock by the actual Classic counterpart instead.
            static constexpr SeedType classicSeeds[] = {
                SEED_PEASHOOTER,SEED_SUNFLOWER,SEED_CHERRYBOMB,SEED_WALLNUT,SEED_POTATOMINE,SEED_CHOMPER,
                SEED_PUFFSHROOM,SEED_FUMESHROOM,SEED_HYPNOSHROOM,SEED_SCAREDYSHROOM,SEED_ICESHROOM,SEED_DOOMSHROOM,
                SEED_LILYPAD,SEED_SQUASH,SEED_THREEPEATER,SEED_TANGLEKELP,SEED_JALAPENO,SEED_SPIKEWEED,
                SEED_TORCHWOOD,SEED_SEASHROOM,SEED_PLANTERN,SEED_CACTUS,SEED_BLOVER,SEED_STARFRUIT,
                SEED_PUMPKINSHELL,SEED_MAGNETSHROOM,SEED_CABBAGEPULT,SEED_FLOWERPOT,SEED_KERNELPULT,
                SEED_GARLIC,SEED_UMBRELLA,SEED_MARIGOLD,SEED_MELONPULT};
            const int unlocked=board->mApp->GetAwardSeedForLevel(level);
            // Native seed order owns Classic adventure unlocks; Fusion recipes
            // remain available through the plants already earned.
            if(kind<33 ? classicSeeds[kind]>=unlocked : unlocked<40)continue;
        }
        for (bool extra : {false, true})
        {
            FusionBankEligibleCard card;
            // Use the source scene before the borrowed native layout is configured.
            // Ordinary Adventure has no shooting-board tag.
            if(sourceLevel ? !TryMakeFusionEnvironmentEligibleCard280(kind,extra,sourceLevel->scene==1||sourceLevel->scene==3,false,card) :
                !TryMakeFusionEligibleCard(kind,extra,card))return false;
            context->eligible.push_back(card);
        }
    }
    if (!context->selection.Open(epoch, difficulty, context->eligible, activeMask, kNativeFusionBankCapacity)) return false;
    board->mFusionNative = std::move(context);
    std::string visualError;
    if (!InitializeNativeVisuals(board, &visualError))
    {
        LawnNet::NetTrace("Fusion native visual initialization failed error=%s", visualError.c_str());
        board->mFusionNative.reset();
        return false;
    }
    if(!board->mApp->NetIsClient())InitializeNativeWorldRemainder269Board(board);
    return true;
}

void SyncNativeSelectionPackets(Board* board)
{
    NativeContext* context = GetNativeContext(board);
    if (context) SyncPackets(board, *context);
}

bool HandleNativeFusionCommand(Board* board, int slot, const FusionCommand& command)
{
    NativeContext* context = GetNativeContext(board);
    if (!context || !Host(board) || command.epoch != context->epoch ||
        !Active(context->selection.State(), slot)) return false;
    if(command.action==FusionCommandKind::AdventureRewardCollect)
        return HandleNativeSourceAdventureRewardCommand273(board,slot,command);
    if(command.action==FusionCommandKind::DroppedCardPickup||command.action==FusionCommandKind::DroppedCardPutDown||command.action==FusionCommandKind::DroppedCardPlant)
        return HandleNativeDroppedCard269Command(board,slot,command.action==FusionCommandKind::DroppedCardPickup?NativeDroppedCard269Action::Pickup:
            command.action==FusionCommandKind::DroppedCardPutDown?NativeDroppedCard269Action::PutDown:NativeDroppedCard269Action::Plant,command);
    if(command.action==FusionCommandKind::EquipmentPickup||command.action==FusionCommandKind::EquipmentUse||
        command.action==FusionCommandKind::EquipmentCancel||command.action==FusionCommandKind::CannonAim)
        return HandleNativeTools268Command(board,slot,command);
    if (command.action == FusionCommandKind::Plant) return PlantPacket(board, *context, slot, command);
    if (command.action == FusionCommandKind::ActivatePlant || command.action == FusionCommandKind::MoneyPlant)
    {
        if (board->mPaused || context->selection.State().phase != FusionBankPhase::Playing ||
            board->mApp->mGameScene != SCENE_PLAYING) return false;
        Plant* plant=nullptr;
        while (board->mPlants.IterateNext(plant))
        {
            const auto* data=GetNativePlantData(plant);
            if (plant->mNetId != command.sourceId || !data || data->instanceToken != command.sourceToken) continue;
            if (plant->mDead || plant->mNetGhost || !plant->mIsOnBoard ||
                (data->owner != slot && data->owner != LawnNet::NET_SHARED_OWNER) ||
                IsNativeGlovePlantReserved(board,plant,-1)) return false;
            return command.action==FusionCommandKind::MoneyPlant?TryNativeMoneyPlantAction(plant,slot):TryActivateNativeDoomFume(plant);
        }
        return false;
    }
    if (command.action == FusionCommandKind::GlovePickup || command.action == FusionCommandKind::GlovePlace ||
        command.action == FusionCommandKind::GloveCancel) return HandleNativeGloveCommand(board, slot, command);
    if (context->selection.State().phase != FusionBankPhase::Choosing) return false;
    FusionSeedBank previous = context->selection;
    bool changed = false;
    switch (command.action)
    {
    case FusionCommandKind::Choose: changed = context->selection.Choose(command.epoch, slot, command.kind, command.isExtra); break;
    case FusionCommandKind::Unchoose: changed = context->selection.Unchoose(command.epoch, slot, command.packetId); break;
    case FusionCommandKind::Mystery: changed = context->selection.ToggleMystery(command.epoch, slot, command.enabled); break;
    case FusionCommandKind::Ready: changed = context->selection.SetReady(command.epoch, slot, command.enabled); break;
    default: return false;
    }
    if (!changed) return false;
    if (SyncPackets(board, *context)) return true;
    context->selection = std::move(previous);
    return false;
}

bool ReconcileNativePlayers(Board* board, uint8_t activeMask)
{
    auto* context = GetNativeContext(board);
    if (!context || !Host(board) || !activeMask) return false;
    const uint8_t previousMask = context->selection.State().activeMask;
    const bool added = (activeMask & ~previousMask) != 0;
    if (added && context->selection.State().phase != FusionBankPhase::Playing) return false;
    if (context->selection.State().phase == FusionBankPhase::Intro)
    {
        context->pendingRosterMask = activeMask;
    }
    else
    {
        auto previous = context->selection;
        const bool reconciled = added ? context->selection.ReassignOwners(context->epoch,activeMask) :
            context->selection.ReconcilePlayers(context->epoch,activeMask);
        if (!reconciled || !FusionSeedBank::ValidateSnapshot(context->selection.State(),context->eligible,kNativeFusionBankCapacity))
        {
            context->selection = std::move(previous);
            return false;
        }
        if (!SyncPackets(board,*context))
        {
            context->selection = std::move(previous);
            return false;
        }
        context->pendingRosterMask = 0;
    }
    for (size_t slot = 0; slot < kFusionBankPlayers; ++slot)
        if (!(activeMask & (1U << slot))) context->gloveHolds[slot] = {};
    LawnNet::NetTrace("Fusion native roster reconciled epoch=%u previous=%u active=%u phase=%u deferred=%d",
        context->epoch,previousMask,activeMask,static_cast<unsigned>(context->selection.State().phase),
        context->pendingRosterMask ? 1 : 0);
    return true;
}

bool AdvanceNativeSelection(Board* board)
{
    NativeContext* context = GetNativeContext(board);
    if (!context || !Host(board) || context->selectionCommitted) return false;
    FusionSeedBank previous = context->selection;
    if (!context->selection.CommitIntro(context->epoch)) return false;
    if (!SyncPackets(board, *context)) { context->selection = std::move(previous); return false; }
    context->selectionCommitted = true;
    return true;
}

bool BeginNativePlaying(Board* board)
{
    NativeContext* context = GetNativeContext(board);
    if (!context || !Host(board) || !context->selectionCommitted ||
        board->mApp->mGameScene != SCENE_PLAYING) return false;
    if (context->selection.State().phase == FusionBankPhase::Playing) return true;
    FusionSeedBank candidate = context->selection;
    if (!candidate.BeginPlaying(context->epoch)) return false;
    const bool pendingMystery = std::any_of(candidate.State().packets.begin(), candidate.State().packets.end(),
        [](const auto& packet) { return packet.mysteryPending; });
    if (pendingMystery)
    {
        try
        {
            // The private mystery RNG is independent of the native gameplay seed.
            if (board->mApp->CoopSmokeFailMysteryRngOnce())
                throw std::runtime_error("isolated mystery RNG smoke fault");
            std::random_device entropy;
            if (!candidate.Reveal(context->epoch, [&](size_t count, size_t& index)
                {
                    if (!count) return false;
                    index = std::uniform_int_distribution<size_t>(0, count - 1)(entropy);
                    return true;
                })) return false;
        }
        catch (...) { return false; }
    }
    if (context->pendingRosterMask && !candidate.ReconcilePlayers(context->epoch,context->pendingRosterMask)) return false;
    FusionSeedBank previous = context->selection;
    context->selection = std::move(candidate);
    if (SyncPackets(board, *context)) { context->pendingRosterMask = 0; return true; }
    context->selection = std::move(previous);
    return false;
}

void PruneNativeMetadata(Board* board)
{
    NativeContext* context = GetNativeContext(board);
    if (!context) return;
    UpdateNativeGloveState(board);
    if(context->capturedPlants314)context->capturedPlants314->Prune();
    if(auto* effects=GetNativeSourceEffects269(board,false))effects->UpdateEmbedded(board);
    Prune(board->mPlants, context->plants);
    Prune(board->mPlants, context->lobberAims);
    Prune(board->mProjectiles, context->projectiles);
    Prune(board->mProjectiles, context->superCherryStarts);
    Prune(board->mPlants, context->star269Plants);
    Prune(board->mProjectiles, context->star269Projectiles);
    Prune(board->mPlants, context->doom269Plants);
    Prune(board->mPlants, context->doom269Clouds);
    Prune(board->mProjectiles, context->companions269);
    Prune(board->mPlants, context->companionPlants269);
    Prune(board->mPlants, context->worldPlants269);Prune(board->mProjectiles, context->worldBeams269);
    Prune(board->mCoins,context->droppedCards269);Prune(board->mCoins,context->fertilizeCoins269);Prune(board->mPlants,context->rollingPlants269);Prune(board->mPlants,context->wheatPlants269);RefreshNativeDroppedCard269Cursors(board);
    Prune(board->mZombies, context->coldZombies);
    Prune(board->mZombies, context->butterZombies);
    Prune(board->mZombies, context->zombieActions);
    Prune(board->mZombies, context->sourceZombies);
    for(auto it=context->sourceDeathPoints313.begin();it!=context->sourceDeathPoints313.end();)
        if(!board->mZombies.DataArrayTryToGet(*it))it=context->sourceDeathPoints313.erase(it);else ++it;
    Prune(board->mZombies, context->sourceZombieBirthOrdinals);
    Prune(board->mZombies, context->sourceGraphs313);
    Prune(board->mZombies, context->waterZombies313);
    Prune(board->mZombies, context->extendedZombies313);
    Prune(board->mZombies, context->heavyZombies313B);
    Prune(board->mProjectiles, context->sourceObstacles313);
    PruneNativeSourceGraph313Clocks(board);
    Prune(board->mZombies, context->sourceZombieCursors);
    Prune(board->mZombies, context->zombieInstanceTokens);
    Prune(board->mCoins, context->equipmentCoins);
    Prune(board->mPlants, context->equipmentPlants);
    if (!board->mApp->NetIsClient()) ReleaseUnusedNativeSourceZombieVisuals(board);
    PruneNativeZombieGarlicData(board);
    Prune(board->mProjectiles, context->lobberPierceLimits);
    Prune(board->mProjectiles, context->lobberPlantTargets);
    Prune(board->mProjectiles, context->rollingHits);
    Prune(board->mProjectiles, context->umbrellaBlocks);
    Prune(board->mPlants, context->umbrellaPoses);
    Prune(board->mProjectiles, context->fireContacts);
    // Dead coins still allocated in the DataArray retain their value for
    // ScoreCoin, which asks GetSunValue after calling Die.
    Prune(board->mCoins, context->coinValues);
    Prune(board->mZombies,context->dayZombies273);
    Prune(board->mZombies,context->nightZombies281);
    Prune(board->mZombies,context->jackboxZombies300);
    for (auto item = context->packets.begin(); item != context->packets.end(); )
    {
        bool live = false;
        if (board->mSeedBank)
            for (int i = 0; i < board->mSeedBank->mNumPackets && i < SEEDBANK_CAPACITY; ++i)
                if (board->mSeedBank->mSeedPackets[i].mNetId == item->first) { live = true; break; }
        if (!live) item = context->packets.erase(item);
        else ++item;
    }
}
}
