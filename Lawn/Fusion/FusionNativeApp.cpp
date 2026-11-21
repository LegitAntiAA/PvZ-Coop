#include "../../LawnApp.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionWire.h"
#include "FusionNativeGloveUI.h"
#include "FusionNativeMoneyAction.h"
#include "FusionNativeTools268.h"
#include "../Board.h"
#include "../CursorObject.h"
#include "../Plant.h"
#include "../Net/NetManager.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"

namespace LawnFusion
{
void UpdateNativeFieldButton(Board* board)
{
    auto* context = GetNativeContext(board);
    if (!context || !board->mMenuButton) return;
    if (!context->fieldButton)
    {
        context->fieldButton = std::make_unique<GameButton>(-1);
        context->fieldButton->mParentWidget = board;
        context->fieldButton->mDrawStoneButton = true;
        context->fieldButton->SetLabel(_S("FIELD"));
    }
    auto* button = context->fieldButton.get();
    auto* menu = board->mMenuButton;
    // The native stone borders overlap by four pixels, as in stacked menus.
    // The full46px native image ends above the first lawn row at y80.
    button->Resize(menu->mX, menu->mY + menu->mHeight - 4, menu->mWidth, menu->mHeight);
    button->mBtnNoDraw = board->mApp->mGameScene != SCENE_PLAYING;
    button->mDisabled = board->mApp->GetDialogCount() != 0 || !board->mApp->mNetManager;
    if (button->mBtnNoDraw || button->mDisabled) context->fieldButtonPressed = false;
    button->Update();
    button->mIsDown = context->fieldButtonPressed;
}

bool HandleNativeFieldPointer(Board* board, bool down, int button, int x, int y)
{
    auto* context = GetNativeContext(board);
    if (!context || button != 1 || !context->fieldButton) return false;
    auto* field = context->fieldButton.get();
    const bool enabled = board->mApp->mGameScene == SCENE_PLAYING &&
        board->mApp->GetDialogCount() == 0 && board->mApp->mNetManager;
    const auto parent = board->GetAbsPos();
    const bool inside = Sexy::Rect(field->mX, field->mY, field->mWidth, field->mHeight).Contains(x-parent.mX, y-parent.mY);
    if (down)
    {
        // A fresh press supersedes a release lost during a focus transition.
        context->fieldButtonPressed = context->fieldButtonReleasePending = false;
        if (!enabled || !inside || field->mDisabled || field->mBtnNoDraw ||
            (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_ALT))) return false;
        context->fieldButtonPressed = true;
        context->fieldButtonReleasePending = true;
        field->mIsDown = true;
        return true;
    }
    if (!context->fieldButtonReleasePending) return false;
    const bool clicked = context->fieldButtonPressed && enabled && inside;
    context->fieldButtonPressed = false;
    context->fieldButtonReleasePending = false;
    field->mIsDown = false;
    if (!clicked) return true;
    auto& config = board->mApp->mNetManager->mConfig;
    config.mFusionClassicField = !config.mFusionClassicField;
    if (!LawnNet::NetSaveConfig(config))
    {
        config.mFusionClassicField = !config.mFusionClassicField;
        field->SetLabel(_S("RETRY"));
        LawnNet::NetTrace("Fusion field preference save failed");
        return true;
    }
    field->SetLabel(_S("FIELD"));
    LawnNet::NetTrace("Fusion field preference saved skin=%s localOnly=1 columns=%d pitch=%d",
        config.mFusionClassicField ? "native" : "fusion", board->GetColumnCount(), board->GetCellPitchX());
    board->MarkDirty();
    return true;
}
}

bool LawnApp::SubmitNativeFusionCommand(const LawnFusion::FusionCommand& command)
{
    const auto* context = LawnFusion::GetNativeContext(mBoard);
    if (!context) return false;
    auto intent = command;
    intent.epoch = context->epoch;
    if (mNetManager && mNetManager->IsActive()) return mNetManager->SubmitFusionCommand(intent);
    return LawnFusion::HandleNativeFusionCommand(mBoard, 0, intent);
}

bool LawnApp::NativeFusionPointerDown(int button, int x, int y)
{
    if (!LawnFusion::IsNativeFusionBoard(mBoard) || mGameScene != SCENE_PLAYING ||
        GetDialogCount() || !mBoard->mCursorObject) return false;
    x -= mBoard->mX + mDDInterface->mWideScreenOffsetX;
    y -= mBoard->mY + mDDInterface->mWideScreenOffsetY;
    if (LawnFusion::HandleNativeTools268Pointer(mBoard,button,x,y)) return true;
    // Host Money/Glove retain their existing Board::MouseDown routing.
    if (!NetIsClient()) return false;
    if (LawnFusion::HandleNativeMoneyPointer(mBoard, button, x, y) ||
        LawnFusion::HandleNativeGlovePointer(mBoard, button, x, y)) return true;
    if (button > 0 && mBoard->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL && !mBoard->mPaused)
    {
        HitResult actionHit; mBoard->MouseHitTest(x,y,&actionHit);
        if (actionHit.mObjectType == OBJECT_TYPE_PLANT)
        {
            auto* plant=static_cast<Plant*>(actionHit.mObject);
            const auto* data=LawnFusion::GetNativePlantData(plant);
            if (data && data->kind == 1043) { plant->MouseDown(x,y,button); return true; }
        }
    }
    if (button <= 0 || mBoard->mCursorObject->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK) return false;
    HitResult hit;
    mBoard->MouseHitTest(x, y, &hit);
    if (hit.mObjectType != OBJECT_TYPE_NONE && hit.mObjectType != OBJECT_TYPE_PLANT) return false;
    mBoard->MouseDownWithPlant(x, y, button);
    return true;
}

void LawnApp::NativeFusionTickNetwork()
{
    if (!LawnFusion::IsNativeFusionBoard(mBoard)) return;
    LawnFusion::UpdateNativeFieldButton(mBoard);
    LawnFusion::SyncNativeGloveCursors(mBoard);
    LawnFusion::SyncNativeTools268Cursors(mBoard);
    LawnFusion::UpdateNativeVisualPrewarm(mBoard);
    if (NetIsClient()) return;
    Board* identity = mBoard;
    if (mNetManager && mNetManager->IsHosting())
    {
        uint8_t slot = 0;
        LawnFusion::FusionCommand command;
        for (int count = 0; count < LawnNet::NET_INPUT_APPLY_BUDGET && mBoard == identity &&
            mNetManager->PopFusionCommand(slot, command); ++count)
        {
            const int previousSlot = mNetManager->GetRawActingPlayerSlot();
            CursorObject* previousCursor = mBoard->mCursorObject;
            mNetManager->SetActingPlayerSlot(slot);
            if (slot < LawnNet::NET_MAX_PLAYERS && mBoard->mNetPlayerCursors[slot])
                mBoard->mCursorObject = mBoard->mNetPlayerCursors[slot];
            const bool accepted = mNetManager->CanApplyFusionCommand(slot) &&
                command.inputEpoch == mNetManager->GetInputEpoch() &&
                LawnFusion::HandleNativeFusionCommand(mBoard, slot, command);
            if (command.action == LawnFusion::FusionCommandKind::Plant) mBoard->ClearCursor();
            mBoard->mCursorObject = previousCursor;
            mNetManager->SetActingPlayerSlot(previousSlot);
            LawnNet::NetTrace("Fusion native command slot=%u action=%u sequence=%u applied=%d column=%u row=%u packet=%u",
                slot, static_cast<unsigned>(command.action), command.sequence, accepted ? 1 : 0,
                command.column, command.row, command.packetId);
        }
    }
    if (mBoard != identity) return;
    if (LawnFusion::AdvanceNativeSelection(mBoard) && mSeedChooserScreen)
        mSeedChooserScreen->CloseSeedChooser();
    LawnFusion::PruneNativeMetadata(mBoard);
    LawnFusion::ReleaseUnusedNativeVisuals(mBoard);
    LawnFusion::SyncNativeGloveCursors(mBoard);
    LawnFusion::SyncNativeTools268Cursors(mBoard);
}
