#ifndef LAWN_FUSION_RUNTIME_H
#define LAWN_FUSION_RUNTIME_H

#include "FusionWire.h"
#include "FusionPresentation.h"
#include "../../SexyAppFramework/KeyCodes.h"
#include "../../SexyAppFramework/Rect.h"
#include <memory>

class LawnApp;
class Board;
class GameButton;

namespace LawnFusion
{
// Native validation activity. Source Adventure orchestration is intentionally
// unavailable until its tutorial, waves, rewards and progression are ported.
class FusionRuntime
{
public:
    FusionRuntime(LawnApp* app, const FusionStartDescriptor& descriptor);
    ~FusionRuntime();
    bool Initialize(std::string* error = nullptr);
    void ConfigureBoard(Board* board);
    void Update();
    void Draw(Sexy::Graphics* graphics);
    bool OnMouseDown(int x, int y, int button);
    bool OnMouseUp(int x, int y, int button);
    bool OnMouseMove(int x, int y);
    bool OnKeyDown(Sexy::KeyCode key, bool repeat = false);

    // Root authenticates session/spectator/input epoch/sequence before calling.
    bool HandleCommand(int authenticatedSlot, const FusionCommand& command);
    bool ApplyRoom(const FusionRoomState& state, std::string* error = nullptr);
    FusionRoomState CaptureRoom() const;
    const FusionStartDescriptor& Descriptor() const { return mDescriptor; }
    const FusionSimulationState& World() const { return mSimulation.State(); }
    const FusionBankState& Bank() const { return mBank.State(); }
    FusionBankPhase Phase() const { return mBank.State().phase; }
    bool IsPaused() const { return mPaused; }
    bool IsInitialized() const { return mInitialized; }

    // Host orchestration/fixture entry points, without mutable simulation access.
    uint32_t HostSpawnZombie(uint16_t kind, int row, float x);
    bool ReassignOwners(uint8_t activeMask);
    bool SetPaused(bool paused);
    Sexy::Rect EligibleCardRect(size_t index) const;
    Sexy::Rect PacketRect(uint16_t packetId) const;
    Sexy::Rect ReadyButtonRect() const;
    Sexy::Rect MysteryButtonRect() const;
    Sexy::Rect ShovelButtonRect() const;

private:
    LawnApp* mApp;
    Board* mBoard = nullptr;
    FusionStartDescriptor mDescriptor;
    FusionSimulation mSimulation;
    FusionSeedBank mBank;
    FusionPresentation mPresentation;
    uint32_t mRoomRevision = 0;
    bool mInitialized = false;
    bool mPaused = false;
    int mIntroTicks = 0;
    float mCosmeticElapsed = 0.0f;
    uint16_t mSelectedPacket = 0;
    bool mShovelSelected = false;
    uint32_t mShovelTargetId = 0;
    int mMouseX = -1, mMouseY = -1;
    int mPressedButton = 0;
    std::string mNotice;
    std::unique_ptr<GameButton> mReadyButton, mMysteryButton, mMenuButton;

    bool IsHost() const;
    int LocalSlot() const;
    bool LocalCanAct() const;
    bool BumpRevision();
    bool Submit(FusionCommand command);
    bool TryBeginPlaying();
    bool CellAt(int x, int y, uint8_t& column, uint8_t& row) const;
    std::vector<const FusionBankPacket*> DisplayPackets() const;
    const FusionBankPacket* LocalPacket(uint16_t packetId) const;
    Sexy::Rect ChooserRect() const;
    float BankScale() const;
    int BankWidth() const;
    Sexy::Rect BankSlotRect(size_t index) const;
    Sexy::Rect MenuButtonRect() const;
    void DrawCard(Sexy::Graphics* graphics, const Sexy::Rect& rect, FusionPlantId kind, bool extra,
        int price, bool pending, bool selected, bool disabled, float cooldown, float fullRecharge, const SexyString& caption);
    void DrawChooser(Sexy::Graphics* graphics);
    void DrawBank(Sexy::Graphics* graphics);
    void DrawNativeButton(Sexy::Graphics* graphics, std::unique_ptr<GameButton>& button,
        int id, const Sexy::Rect& rect, const SexyString& label, bool disabled);
};
}

#endif
