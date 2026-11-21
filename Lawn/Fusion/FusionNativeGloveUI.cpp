#include "FusionNativeGloveUI.h"
#include "FusionWire.h"
#include "FusionNativePlacement.h"
#include "../Board.h"
#include "../CursorObject.h"
#include "../Plant.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"

namespace LawnFusion
{
namespace
{
bool GloveCursor(const CursorObject* cursor)
{
    return cursor && (cursor->mCursorType == CURSOR_TYPE_GLOVE ||
        cursor->mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE);
}

int LocalSlot(Board* board)
{
    return board->mApp->NetActive() && board->mApp->mNetManager ?
        board->mApp->mNetManager->GetMySlot() : 0;
}

bool LocalCursor(Board* board)
{
    if (!board->mCursorObject) return false;
    auto* net = board->mApp->mNetManager;
    // The host temporarily substitutes a remote acting cursor while draining
    // input. That is not a local cancellation or local tool activation.
    return !board->mApp->NetActive() || !net || net->GetRawActingPlayerSlot() < 0 ||
        net->GetRawActingPlayerSlot() == net->GetMySlot();
}

NativeGloveUIState& UI(NativeContext& context, int slot)
{
    if (!context.gloveUI) context.gloveUI = std::make_unique<NativeGloveUIState>();
    auto& state = *context.gloveUI;
    if (state.epoch != context.epoch || state.localSlot != slot)
    {
        state = {};
        state.epoch = context.epoch;
        state.localSlot = slot;
    }
    return state;
}

bool SameHold(const NativeGloveHold& first, const NativeGloveHold& second)
{
    return first.plantId && first.plantId == second.plantId &&
        first.instanceToken == second.instanceToken;
}

void ClearPreview(CursorObject* cursor)
{
    cursor->mGlovePlantID = PLANTID_NULL;
    if (!GloveCursor(cursor)) return;
    cursor->mCursorType = CURSOR_TYPE_NORMAL;
    cursor->mType = cursor->mImitaterType = SEED_NONE;
    cursor->mSeedBankIndex = -1;
}

void BindPreview(Board* board, CursorObject* cursor, Plant* plant)
{
    cursor->mGlovePlantID = static_cast<PlantID>(board->mPlants.DataArrayGetID(plant));
    cursor->mCursorType = CURSOR_TYPE_PLANT_FROM_GLOVE;
    cursor->mType = plant->mSeedType;
    cursor->mImitaterType = plant->mImitaterType;
    cursor->mSeedBankIndex = -1;
}

bool CanInteract(Board* board, const NativeContext& context, int slot)
{
    if (slot < 0 || slot >= static_cast<int>(kFusionBankPlayers) ||
        !(context.selection.State().activeMask & (1U << slot)) ||
        context.selection.State().phase != FusionBankPhase::Playing ||
        board->mApp->mGameScene != SCENE_PLAYING || board->mPaused ||
        board->mApp->GetDialogCount() || board->mApp->mNetChatTyping ||
        !board->CanUsePlantTools(slot)) return false;
    auto* net = board->mApp->mNetManager;
    return !board->mApp->NetActive() || (net && !net->IsSpectator() &&
        net->GetPlayer(slot).mActive && !net->GetPlayer(slot).mSpectator &&
        net->GetInputPhase() == LawnNet::NETINPUT_PLAYING &&
        (!board->mApp->NetIsClient() || !net->mRemotePaused));
}
}

Plant* GetNativeGloveHeldPlant(Board* board, int slot)
{
    const auto* context = GetNativeContext(board);
    if (!context || slot < 0 || slot >= static_cast<int>(context->gloveHolds.size())) return nullptr;
    const auto hold = context->gloveHolds[slot];
    if (!hold.plantId || !hold.instanceToken) return nullptr;
    Plant* plant = nullptr;
    while (board->mPlants.IterateNext(plant))
    {
        if (plant->mDead || plant->mPlantHealth <= 0 || plant->mNetId != hold.plantId) continue;
        const auto* data = GetNativePlantData(plant);
        if (data && data->instanceToken == hold.instanceToken) return plant;
    }
    return nullptr;
}

void OnNativeGloveCursorClearing(Board* board)
{
    auto* context = GetNativeContext(board);
    if (!context || !LocalCursor(board)) return;
    const int slot = LocalSlot(board);
    if (slot < 0 || slot >= static_cast<int>(context->gloveHolds.size())) return;
    auto& state = UI(*context, slot);
    if (state.clearing || (!GloveCursor(board->mCursorObject) &&
        (state.suppressLocalHold || (!context->gloveHolds[slot].plantId && !state.requestedPickup.plantId)))) return;
    state.clearing = true;
    state.suppressLocalHold = true;
    state.ignoredHold = context->gloveHolds[slot].plantId ? context->gloveHolds[slot] : state.requestedPickup;
    state.requestedPickup = {};
    FusionCommand command;
    command.action = FusionCommandKind::GloveCancel;
    // Unconditional own-hand cancel also follows an in-flight pickup. Its
    // authenticated sequence is stamped by the existing native command bridge.
    board->mApp->SubmitNativeFusionCommand(command);
    state.clearing = false;
}

bool ToggleNativeGloveFromKeyboard(Board* board)
{
    auto* context = GetNativeContext(board);
    if (!context || !LocalCursor(board) || !board->mCursorObject ||
        board->mApp->mSeedChooserScreen || board->mTimeStopCounter > 0 ||
        !CanInteract(board,*context,LocalSlot(board))) return false;
    Sexy::Rect rect = board->GetShovelButtonRect();
    board->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);
    return HandleNativeGlovePointer(board,1,rect.mX+rect.mWidth/2,rect.mY+rect.mHeight/2);
}

bool HandleNativeGlovePointer(Board* board, int button, int x, int y)
{
    auto* context = GetNativeContext(board);
    if (!context || !LocalCursor(board) || !button) return false;
    const int slot = LocalSlot(board);
    if (slot < 0 || slot >= static_cast<int>(context->gloveHolds.size())) return false;
    auto* cursor = board->mCursorObject;
    Sexy::Rect buttonRect = board->GetShovelButtonRect();
    board->GetZenButtonRect(OBJECT_TYPE_GLOVE, buttonRect);
    const bool hitButton = buttonRect.Contains(x, y);
    if (!CanInteract(board, *context, slot)) return hitButton;
    auto& state = UI(*context, slot);
    if (button < 0 && (GloveCursor(cursor) || context->gloveHolds[slot].plantId || state.requestedPickup.plantId))
    {
        board->ClearCursor();
        return true;
    }
    if (button <= 0) return false;
    if (hitButton)
    {
        if (GloveCursor(cursor))
        {
            board->ClearCursor();
            return true;
        }
        if (context->gloveCooldownTicks || !board->CanInteractWithBoardButtons() ||
            !board->CanUseGameObject(OBJECT_TYPE_GLOVE)) return true;
        board->ClearCursor();
        state.suppressLocalHold = false;
        state.requestedPickup = {};
        cursor->mCursorType = CURSOR_TYPE_GLOVE;
        cursor->mGlovePlantID = PLANTID_NULL;
        board->mApp->PlaySample(Sexy::SOUND_TAP);
        return true;
    }
    if (!GloveCursor(cursor)) return false;
    HitResult hit;
    board->MouseHitTest(x, y, &hit);
    if (hit.mObjectType != OBJECT_TYPE_NONE && hit.mObjectType != OBJECT_TYPE_PLANT) return false;
    const int column = board->PixelToGridX(x, y), row = board->PixelToGridY(x, y);
    if (!board->IsValidGridCell(column, row)) return true;
    FusionCommand command;
    if (cursor->mCursorType == CURSOR_TYPE_GLOVE)
    {
        Plant* plant = board->ToolHitTest(x, y);
        const auto* data = GetNativePlantData(plant);
        if (!plant || plant->mDead || !plant->mNetId || !data || !data->instanceToken) return true;
        command.action = FusionCommandKind::GlovePickup;
        command.sourceId = plant->mNetId;
        command.sourceToken = data->instanceToken;
        if (board->mApp->SubmitNativeFusionCommand(command))
        {
            state.suppressLocalHold = false;
            state.requestedPickup = {command.sourceId, command.sourceToken};
        }
        return true;
    }
    const auto hold = context->gloveHolds[slot];
    Plant* moving = GetNativeGloveHeldPlant(board, slot);
    if (!moving) return true;
    command.action = FusionCommandKind::GlovePlace;
    command.sourceId = hold.plantId;
    command.sourceToken = hold.instanceToken;
    command.column = static_cast<uint8_t>(column);
    command.row = static_cast<uint8_t>(row);
    Plant* target = nullptr;
    if (!ResolveNativeFusionMoveTarget(board,column,row,moving,target)) return true;
    if (target)
    {
        const auto* data = GetNativePlantData(target);
        if (!data || !data->instanceToken) return true;
        command.targetId = target->mNetId;
        command.targetToken = data->instanceToken;
    }
    board->mApp->SubmitNativeFusionCommand(command);
    return true; // Preserve the preview until the host's canonical state clears.
}

void SyncNativeGloveCursors(Board* board)
{
    auto* context = GetNativeContext(board);
    if (!context || !LocalCursor(board)) return;
    const int local = LocalSlot(board);
    auto* localCursor = board->mCursorObject;
    for (int slot = 0; slot < static_cast<int>(context->gloveHolds.size()); ++slot)
    {
        CursorObject* cursor = slot == local ? localCursor : board->mNetPlayerCursors[slot];
        // On a client array[0] is also its local cursor. The remote draw path
        // resolves that slot's canonical tuple temporarily, without clobbering it.
        if (!cursor || (slot != local && cursor == localCursor)) continue;
        Plant* plant = GetNativeGloveHeldPlant(board, slot);
        if (slot != local)
        {
            if (plant) BindPreview(board, cursor, plant);
            else if (cursor->mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE) ClearPreview(cursor);
            continue;
        }
        auto& state = UI(*context, local);
        if (state.suppressLocalHold)
        {
            if (GloveCursor(cursor)) ClearPreview(cursor);
            continue;
        }
        if (!GloveCursor(cursor))
        {
            state.requestedPickup = {};
            continue; // Never resurrect Normal/bank/tool cursors.
        }
        const auto hold = context->gloveHolds[local];
        if (plant && (!SameHold(hold, state.ignoredHold) || SameHold(hold, state.requestedPickup)))
            BindPreview(board, cursor, plant);
        else if (cursor->mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE)
        {
            ClearPreview(cursor);
            if (plant) cursor->mCursorType = CURSOR_TYPE_GLOVE;
        }
    }
}
}
