#include "FusionNativeGlove.h"
#include "FusionGloveRules.h"
#include "FusionNative.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativePlacement.h"
#include "FusionNativeTools268.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeLaserConnections269.h"
#include "FusionWire.h"
#include "../Board.h"
#include "../CursorObject.h"
#include "../Plant.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include <limits>

namespace LawnFusion
{
namespace
{
bool Host(const Board* board)
{
    return board && board->mApp && (!board->mApp->NetActive() || board->mApp->NetIsHost());
}

bool Active(const Board* board, const NativeContext& context, int slot)
{
    if (slot < 0 || slot >= static_cast<int>(kFusionBankPlayers) ||
        !(context.selection.State().activeMask & (1U << slot))) return false;
    if (!board->mApp->NetActive()) return slot == 0;
    const auto& player = board->mApp->mNetManager->GetPlayer(slot);
    return player.mActive && !player.mSpectator;
}

FusionGloveIdentity Identity(const NativeGloveHold& hold)
{
    return {hold.plantId, hold.instanceToken};
}

CursorObject* Cursor(Board* board, int slot)
{
    if (slot < 0 || slot >= static_cast<int>(kFusionBankPlayers)) return nullptr;
    if (board->mApp->NetActive()) return board->mNetPlayerCursors[slot];
    return slot == 0 ? board->mCursorObject : nullptr;
}

Plant* Resolve(Board* board, FusionGloveIdentity identity)
{
    if (!FusionGloveIdentityValid(identity)) return nullptr;
    Plant* plant = board->mPlants.DataArrayTryToGet(identity.instanceToken);
    const NativePlantData* data = GetNativePlantData(plant);
    return plant && plant->mNetId == identity.plantId && data &&
        data->instanceToken == identity.instanceToken ? plant : nullptr;
}

bool Live(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return plant && data && plant->mIsOnBoard && !plant->mDead && !plant->mNetGhost &&
        !NativeBatchPlantUntargetable(plant) &&
        !plant->mSquished && plant->mBurnedCounter == -1 && plant->mPlantHealth > 0 &&
        plant->mOnBungeeState == PlantOnBungeeState::NOT_ON_BUNGEE &&
        GetNativeSeedForPlant(data->kind) != SEED_NONE &&
        plant->mSeedType == GetNativeSeedForPlant(data->kind);
}

bool SupportedTerrainCell(Board* board, int column, int row)
{
    if (!board->IsValidGridCell(column,row)) return false;
    const auto tile=board->GetGridSquareTypeAt(column,row);
    const bool terrain=(board->mPlantRow[row]==PLANTROW_NORMAL&&tile==GRIDSQUARE_GRASS)||
        (board->mPlantRow[row]==PLANTROW_POOL&&tile==GRIDSQUARE_POOL);
    // The shared placement planner applies the moving/result kind's water
    // and Lily requirements; reservation itself also supports native Pool.
    return terrain && !board->StageHasRoof() &&
        !board->GetLadderAt(column, row) && !board->GetGraveStoneAt(column, row) &&
        !board->GetCraterAt(column, row) && !board->GetScaryPotAt(column, row) &&
        !board->IsIceAt(column, row);
}

bool SupportedPlantCell(Board* board, Plant* plant)
{
    if (!Live(plant) || !SupportedTerrainCell(board, plant->mPlantCol, plant->mRow)) return false;
    PlantsOnLawn cell;
    board->GetPlantsOnLawn(plant->mPlantCol, plant->mRow, &cell);
    const auto* data=GetNativePlantData(plant);
    const bool overlay=data&&IsFusionSourcePresentOverlayKind(data->kind);
    // The command already resolved this actor's full generation. A transient
    // Present is intentionally absent from the persistent occupancy summary.
    return !cell.mFlyingPlant && (overlay || cell.mNormalPlant == plant || cell.mUnderPlant == plant || cell.mPumpkinPlant == plant);
}

void HoldCursor(Board* board, int slot, Plant* plant)
{
    if (auto* cursor = Cursor(board, slot))
    {
        cursor->mGlovePlantID = static_cast<PlantID>(board->mPlants.DataArrayGetID(plant));
        cursor->mCursorType = CURSOR_TYPE_PLANT_FROM_GLOVE;
        cursor->mType = plant->mSeedType;
        cursor->mImitaterType = plant->mImitaterType;
        cursor->mSeedBankIndex = -1;
    }
}
}

bool IsNativeGlovePlantReserved(const Board* board, const Plant* plant, int exceptSlot)
{
    const auto* context = GetNativeContext(board);
    const auto* data = GetNativePlantData(plant);
    if (!context || !data || !plant || plant->mBoard != board) return false;
    const FusionGloveIdentity identity{plant->mNetId, data->instanceToken};
    for (size_t slot = 0; slot < context->gloveHolds.size(); ++slot)
        if (static_cast<int>(slot) != exceptSlot &&
            FusionGloveIdentityMatches(Identity(context->gloveHolds[slot]), identity)) return true;
    return false;
}

void CancelNativeGlove(Board* board, int slot)
{
    auto* context = GetNativeContext(board);
    if (!context || !Host(board) || slot < 0 || slot >= static_cast<int>(kFusionBankPlayers)) return;
    Plant* released=Resolve(board,Identity(context->gloveHolds[slot]));
    context->gloveHolds[slot] = {};
    if(released)NativeLaserConnections269PlantPlaced(released);
    if (auto* cursor = Cursor(board, slot))
    {
        cursor->mGlovePlantID = PLANTID_NULL;
        if (cursor->mCursorType == CURSOR_TYPE_GLOVE || cursor->mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE)
        {
            cursor->mCursorType = CURSOR_TYPE_NORMAL;
            cursor->mType = cursor->mImitaterType = SEED_NONE;
            cursor->mSeedBankIndex = -1;
        }
    }
}

void UpdateNativeGloveState(Board* board)
{
    auto* context = GetNativeContext(board);
    if (!context || !Host(board)) return;
    if (context->selection.State().phase == FusionBankPhase::Playing &&
        board->mApp->mGameScene == SCENE_PLAYING && !board->mPaused)
        context->gloveCooldownTicks = AdvanceFusionGloveCooldown(
            context->gloveCooldownTicks, context->gloveTick, board->mMainCounter);
    context->gloveTick = board->mMainCounter;
    for (int slot = 0; slot < static_cast<int>(kFusionBankPlayers); ++slot)
    {
        const auto identity = Identity(context->gloveHolds[slot]);
        if (!Active(board, *context, slot) || context->selection.State().phase != FusionBankPhase::Playing ||
            !FusionGloveIdentityValid(identity, true) ||
            (identity.plantId && !Live(Resolve(board, identity)))) CancelNativeGlove(board, slot);
    }
}

bool HandleNativeGloveCommand(Board* board, int slot, const FusionCommand& command)
{
    auto* context = GetNativeContext(board);
    if (!context || !Host(board) || command.epoch != context->epoch || !Active(board, *context, slot)) return false;
    const FusionGloveIdentity source{command.sourceId, command.sourceToken};
    const auto held = Identity(context->gloveHolds[slot]);
    if (command.action == FusionCommandKind::GloveCancel)
    {
        // Empty cancel also drains an in-flight pickup whose echo has not
        // reached its owner. Sequence/epoch authentication is upstream.
        if (!FusionGloveIdentityValid(source, true) ||
            (source.plantId && !FusionGloveIdentityMatches(source, held))) return false;
        CancelNativeGlove(board, slot);
        return true;
    }
    if (context->selection.State().phase != FusionBankPhase::Playing ||
        board->mApp->mGameScene != SCENE_PLAYING || board->mPaused || board->mApp->GetDialogCount()) return false;
    Plant* moving = Resolve(board, source);
    if (!SupportedPlantCell(board, moving) || !Cursor(board, slot)) return false;
    if (command.action == FusionCommandKind::GlovePickup)
    {
        if(GetNativeEquipmentClaimedBy(board,slot)||GetNativeFertilize269ClaimedBy(board,slot)||GetNativeDroppedCard269Held(board,slot))return false;
        std::array<FusionGloveIdentity, kFusionBankPlayers> holds{};
        for (size_t i = 0; i < holds.size(); ++i) holds[i] = Identity(context->gloveHolds[i]);
        if (!CanReserveFusionGlove(holds, context->selection.State().activeMask, slot,
            source, context->gloveCooldownTicks)) return false;
        context->gloveHolds[slot] = {source.plantId, source.instanceToken};
        RebuildNativeLaserConnections269Column(board,moving->mPlantCol);
        HoldCursor(board, slot, moving);
        return true;
    }
    if (command.action != FusionCommandKind::GlovePlace || !FusionGloveIdentityMatches(source, held) ||
        !SupportedTerrainCell(board, command.column, command.row) ||
        (moving->mPlantCol == command.column && moving->mRow == command.row) ||
        command.targetId > (std::numeric_limits<uint16_t>::max)()) return false;

    Plant* target = nullptr;
    if (!ResolveNativeFusionMoveTarget(board,command.column,command.row,moving,target)) return false;
    const FusionGloveIdentity destination{static_cast<uint16_t>(command.targetId), command.targetToken};
    const NativePlantData incoming = *GetNativePlantData(moving);
    if (target)
    {
        if (!FusionGloveIdentityValid(destination) || target != Resolve(board, destination) ||
            !SupportedPlantCell(board, target) || IsNativeGlovePlantReserved(board, target, slot)) return false;
    }
    else
    {
        if (!FusionGloveIdentityValid(destination, true) || destination.plantId) return false;
    }
    const int previousColumn=moving->mPlantCol;
    if (!TryPlaceNativeFusionPlant(board,command.column,command.row,incoming,destination.plantId,slot,
        destination.instanceToken,moving)) return false;
    if(!moving->mDead)NativeCompanions269PlantMoved(moving);
    context->gloveCooldownTicks = FusionGloveSuccessCooldown(incoming.kind);
    context->gloveTick = board->mMainCounter;
    CancelNativeGlove(board, slot);
    NativeLaserConnections269PlantMoved(moving,previousColumn);
    return true;
}
}
