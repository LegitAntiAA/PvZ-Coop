#include "FusionRuntime.h"
#include "../../LawnApp.h"
#include "../Board.h"
#include "../Net/NetManager.h"
#include "../Widget/GameButton.h"
#include "../Widget/NetUi.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <Windows.h>

namespace LawnFusion
{
namespace
{
constexpr float kTickSeconds = 0.01f;
constexpr int kValidationIntroTicks = 50;

SexyString Number(int value) { return Sexy::StringToSexyString(std::to_string(value)); }

bool SameRoom(const FusionRoomState& first, const FusionRoomState& second, const FusionStartDescriptor& descriptor)
{
    LawnNet::NetBuffer a, b;
    return EncodeFusionRoom(first, descriptor, a) && EncodeFusionRoom(second, descriptor, b) && a.mData == b.mData;
}

bool FinishMystery(FusionSeedBank& bank, uint32_t epoch)
{
    if (!bank.BeginPlaying(epoch)) return false;
    bool pending = false;
    for (const auto& packet : bank.State().packets) pending |= packet.mysteryPending;
    if (!pending) return bank.Reveal(epoch, {});
    try
    {
        std::random_device device;
        std::seed_seq seed{device(), device(), device(), device(), device(), device()};
        std::mt19937 engine(seed);
        return bank.Reveal(epoch, [&](size_t bound, size_t& index)
        {
            if (!bound) return false;
            std::uniform_int_distribution<size_t> distribution(0, bound - 1);
            index = distribution(engine);
            return true;
        });
    }
    catch (...) { return false; }
}
}

FusionRuntime::FusionRuntime(LawnApp* app, const FusionStartDescriptor& descriptor) : mApp(app), mDescriptor(descriptor) {}

bool FusionRuntime::IsHost() const { return mApp && !mApp->NetIsClient(); }

int FusionRuntime::LocalSlot() const
{
    return mApp && mApp->mNetManager && mApp->mNetManager->IsActive() ? mApp->mNetManager->GetMySlot() : 0;
}

bool FusionRuntime::LocalCanAct() const
{
    const int slot = LocalSlot();
    return mInitialized && slot >= 0 && slot < static_cast<int>(kFusionBankPlayers) &&
        (mBank.State().activeMask & (1U << slot)) &&
        (!mApp->mNetManager || !mApp->mNetManager->IsActive() || !mApp->mNetManager->IsSpectator());
}

bool FusionRuntime::Initialize(std::string* error)
{
    auto fail = [&](const char* message) { if (error) *error = message; return false; };
    if (!mApp || !ValidateFusionDescriptor(mDescriptor)) return fail("Invalid Fusion start descriptor.");
    if (mDescriptor.activity != FusionActivity::Validation) return fail("Original Fusion Adventure orchestration is not implemented yet.");
    for (const auto& card : mDescriptor.eligible) if (!FusionSupportsPlant(card.kind)) return fail("An eligible card has no native Fusion behavior.");
    if (!mSimulation.Reset(mDescriptor.columns, mDescriptor.rows, mDescriptor.initialSun, mDescriptor.seed) ||
        !mBank.Open(mDescriptor.epoch, mDescriptor.difficulty, mDescriptor.eligible, mDescriptor.activeMask))
        return fail("Cannot initialize Fusion simulation or personal seed bank.");
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) return fail("Cannot locate the Fusion content directory beside the executable.");
    executable.resize(length);
    const auto packageRoot = std::filesystem::path(executable).parent_path() / L"fusion" / L"pilot";
    std::string presentationError;
    if (!mPresentation.Load(mApp, packageRoot.string(), presentationError))
    {
        if (error) *error = presentationError;
        return false;
    }
    mRoomRevision = 1;
    mInitialized = true;
    if (error) error->clear();
    return true;
}

void FusionRuntime::ConfigureBoard(Board* board)
{
    mBoard = board;
    if (!board) return;
    board->mPaused = mPaused;
    for (auto* button : {board->mMenuButton, board->mStoreButton}) if (button)
    { button->mBtnNoDraw = true; button->SetDisabled(true); }
}

bool FusionRuntime::BumpRevision()
{
    if (mRoomRevision == (std::numeric_limits<uint32_t>::max)()) return false;
    ++mRoomRevision;
    return true;
}

bool FusionRuntime::TryBeginPlaying()
{
    FusionSeedBank candidate = mBank;
    if (!FinishMystery(candidate, mDescriptor.epoch))
    {
        mNotice = "Unable to prepare mystery cards. Retrying...";
        mIntroTicks = 0;
        return false;
    }
    mBank = std::move(candidate);
    mNotice.clear();
    mCosmeticElapsed = 0.0f;
    mApp->FusionAdvancePlaying();
    return true;
}

void FusionRuntime::Update()
{
    if (!mInitialized) return;
    if (!IsHost())
    {
        if (!mPaused && Phase() == FusionBankPhase::Playing) mCosmeticElapsed = (std::min)(mCosmeticElapsed + kTickSeconds, 0.1f);
        return;
    }
    if (mRoomRevision == (std::numeric_limits<uint32_t>::max)()) return;
    if (mBoard) mPaused = mBoard->mPaused;
    if (!mPaused)
    {
        if (Phase() == FusionBankPhase::Choosing && mBank.State().readyMask == mBank.State().activeMask)
        {
            if (mBank.CommitIntro(mDescriptor.epoch)) mIntroTicks = 0;
        }
        else if (Phase() == FusionBankPhase::Intro)
        {
            if (++mIntroTicks >= kValidationIntroTicks) TryBeginPlaying();
        }
        else if (Phase() == FusionBankPhase::Playing)
        {
            FusionSimulation world = mSimulation;
            FusionSeedBank bank = mBank;
            if (world.Step(kTickSeconds) && bank.AdvanceRecharge(mDescriptor.epoch, kTickSeconds))
            { mSimulation = std::move(world); mBank = std::move(bank); }
        }
    }
    BumpRevision();
    if (mSelectedPacket && !mBank.PeekReadyPacket(mDescriptor.epoch, LocalSlot(), mSelectedPacket)) mSelectedPacket = 0;
}

bool FusionRuntime::HandleCommand(int authenticatedSlot, const FusionCommand& command)
{
    if (!mInitialized || !IsHost() || mPaused || (mBoard && mBoard->mPaused) || command.epoch != mDescriptor.epoch ||
        mRoomRevision == (std::numeric_limits<uint32_t>::max)() || authenticatedSlot < 0 ||
        authenticatedSlot >= static_cast<int>(kFusionBankPlayers) || !(mBank.State().activeMask & (1U << authenticatedSlot))) return false;
    bool accepted = false;
    switch (command.action)
    {
    case FusionCommandKind::Choose:
        accepted = mBank.Choose(command.epoch, authenticatedSlot, command.kind, command.isExtra); break;
    case FusionCommandKind::Unchoose:
        accepted = mBank.Unchoose(command.epoch, authenticatedSlot, command.packetId); break;
    case FusionCommandKind::Mystery:
        accepted = mBank.ToggleMystery(command.epoch, authenticatedSlot, command.enabled); break;
    case FusionCommandKind::Ready:
        accepted = mBank.SetReady(command.epoch, authenticatedSlot, command.enabled); break;
    case FusionCommandKind::Plant:
    {
        const auto* packet = mBank.PeekReadyPacket(command.epoch, authenticatedSlot, command.packetId);
        if (!packet) break;
        FusionPlantRequest request;
        request.type = packet->kind;
        request.column = command.column; request.row = command.row;
        request.expectedExistingId = command.targetId;
        request.cost = packet->price; request.owner = packet->owner;
        request.packetId = packet->id; request.randomOrigin = packet->randomOrigin;
        FusionSimulation world = mSimulation;
        FusionSeedBank bank = mBank;
        const auto result = world.TryPlant(request);
        if ((result.status == FusionPlantingStatus::Planted || result.status == FusionPlantingStatus::Fused) &&
            bank.CommitPacketUse(command.epoch, authenticatedSlot, command.packetId))
        { mSimulation = std::move(world); mBank = std::move(bank); accepted = true; }
        break;
    }
    case FusionCommandKind::Shovel:
    {
        if (Phase() != FusionBankPhase::Playing) break;
        accepted = mSimulation.RemovePlant(command.targetId);
        break;
    }
    case FusionCommandKind::CollectSun:
        if (Phase() == FusionBankPhase::Playing) accepted = mSimulation.CollectSun(command.targetId);
        break;
    default: break;
    }
    if (accepted)
    {
        BumpRevision();
        if (command.action == FusionCommandKind::Plant && authenticatedSlot == LocalSlot()) mSelectedPacket = 0;
    }
    return accepted;
}

FusionRoomState FusionRuntime::CaptureRoom() const
{
    FusionRoomState state;
    state.epoch = mDescriptor.epoch; state.revision = mRoomRevision; state.paused = mPaused;
    state.world = mSimulation.State(); state.bank = mBank.State();
    return state;
}

bool FusionRuntime::ApplyRoom(const FusionRoomState& state, std::string* error)
{
    auto fail = [&](const char* message) { if (error) *error = message; return false; };
    if (!mInitialized || IsHost() || state.epoch != mDescriptor.epoch || !state.revision || state.revision < mRoomRevision ||
        state.world.columns != mDescriptor.columns || state.world.rows != mDescriptor.rows || state.world.tick < mSimulation.State().tick ||
        state.world.nextId < mSimulation.State().nextId) return fail("Stale or incompatible Fusion room state.");
    if (state.revision == mRoomRevision)
    {
        if (!SameRoom(state, CaptureRoom(), mDescriptor)) return fail("Fusion room changed without a new revision.");
        if (error) error->clear();
        return true;
    }
    LawnNet::NetBuffer encoded;
    if (!EncodeFusionRoom(state, mDescriptor, encoded)) return fail("Invalid Fusion room state.");
    FusionSimulation world = mSimulation;
    FusionSeedBank bank = mBank;
    if (!world.ApplySnapshot(state.world) || !bank.ApplySnapshot(state.bank)) return fail("Invalid Fusion world or seed bank state.");
    const bool starting = Phase() != FusionBankPhase::Playing && state.bank.phase == FusionBankPhase::Playing;
    mSimulation = std::move(world); mBank = std::move(bank);
    mPaused = state.paused; mRoomRevision = state.revision; mCosmeticElapsed = 0.0f;
    if (mBoard) mBoard->mPaused = mPaused;
    if (starting) mApp->FusionAdvancePlaying();
    if (mSelectedPacket && !mBank.PeekReadyPacket(mDescriptor.epoch, LocalSlot(), mSelectedPacket)) mSelectedPacket = 0;
    if (error) error->clear();
    return true;
}

uint32_t FusionRuntime::HostSpawnZombie(uint16_t kind, int row, float x)
{
    if (!mInitialized || !IsHost() || mPaused || Phase() != FusionBankPhase::Playing ||
        mRoomRevision == (std::numeric_limits<uint32_t>::max)()) return 0;
    const uint32_t id = mSimulation.SpawnZombie(kind, row, x);
    if (id) BumpRevision();
    return id;
}

bool FusionRuntime::ReassignOwners(uint8_t activeMask)
{
    if (!mInitialized || !IsHost() || !(activeMask & 1) || (activeMask & ~mDescriptor.activeMask) ||
        mRoomRevision == (std::numeric_limits<uint32_t>::max)() ||
        !mBank.ReassignOwners(mDescriptor.epoch, activeMask)) return false;
    BumpRevision();
    if (mSelectedPacket && !LocalPacket(mSelectedPacket)) mSelectedPacket = 0;
    return true;
}

bool FusionRuntime::SetPaused(bool paused)
{
    if (!mInitialized || !IsHost() || mRoomRevision == (std::numeric_limits<uint32_t>::max)()) return false;
    if (mPaused == paused) return true;
    mPaused = paused;
    if (mBoard) mBoard->mPaused = paused;
    return BumpRevision();
}

bool FusionRuntime::Submit(FusionCommand command)
{
    if (!LocalCanAct() || mPaused || mApp->GetDialogCount() || mApp->mNetChatTyping) return false;
    command.epoch = mDescriptor.epoch;
    return mApp->SubmitFusionCommand(command);
}

std::vector<const FusionBankPacket*> FusionRuntime::DisplayPackets() const
{
    std::vector<const FusionBankPacket*> result;
    const int local = LocalSlot();
    for (const auto& packet : mBank.State().packets) if (packet.owner == local) result.push_back(&packet);
    for (const auto& packet : mBank.State().packets) if (packet.owner != local) result.push_back(&packet);
    return result;
}

FusionRuntime::~FusionRuntime() = default;

const FusionBankPacket* FusionRuntime::LocalPacket(uint16_t packetId) const
{
    for (const auto& packet : mBank.State().packets) if (packet.id == packetId && packet.owner == LocalSlot()) return &packet;
    return nullptr;
}

Sexy::Rect FusionRuntime::ChooserRect() const
{
    return Sexy::Rect((std::max)(0, (mApp->mWidth - 800) / 2), 87,
        Sexy::IMAGE_SEEDCHOOSER_BACKGROUND->mWidth, Sexy::IMAGE_SEEDCHOOSER_BACKGROUND->mHeight);
}

Sexy::Rect FusionRuntime::EligibleCardRect(size_t index) const
{
    // This bounded activity exposes four identities. Full-catalog scrolling
    // requires a separate eligibility/unlock and navigation implementation.
    if (index >= 40 || index >= mDescriptor.eligible.size()) return Sexy::Rect();
    const auto panel = ChooserRect();
    return Sexy::Rect(panel.mX + 22 + static_cast<int>(index % 8) * 53,
        panel.mY + 76 + static_cast<int>(index / 8) * 73, 50, 70);
}

int FusionRuntime::BankWidth() const
{
    return 79 + 51 * mBank.State().targetCount + 12;
}

float FusionRuntime::BankScale() const
{
    const int available = (std::max)(1, MenuButtonRect().mX - 8);
    return (std::min)(1.0f, static_cast<float>(available) /
        (BankWidth() + Sexy::IMAGE_SHOVELBANK->mWidth));
}

Sexy::Rect FusionRuntime::BankSlotRect(size_t index) const
{
    const float scale = BankScale();
    return Sexy::Rect(static_cast<int>((79 + 51 * index) * scale), static_cast<int>(8 * scale),
        static_cast<int>(50 * scale), static_cast<int>(70 * scale));
}

Sexy::Rect FusionRuntime::PacketRect(uint16_t packetId) const
{
    const auto packets = DisplayPackets();
    for (const auto* packet : packets) if (packet->id == packetId)
    {
        size_t index = 0;
        if (packet->owner != LocalSlot())
        {
            if (LocalSlot() >= 0 && LocalSlot() < static_cast<int>(kFusionBankPlayers)) index += mBank.State().quotas[LocalSlot()];
            for (int owner = 0; owner < packet->owner; ++owner)
                if (owner != LocalSlot()) index += mBank.State().quotas[owner];
        }
        for (const auto* candidate : packets)
        {
            if (candidate->id == packetId) break;
            if (candidate->owner == packet->owner) ++index;
        }
        return BankSlotRect(index);
    }
    return Sexy::Rect();
}

Sexy::Rect FusionRuntime::ReadyButtonRect() const
{
    const auto panel = ChooserRect();
    return Sexy::Rect(panel.mX + 154, panel.mY + panel.mHeight - 55, 156, 42);
}

Sexy::Rect FusionRuntime::MysteryButtonRect() const
{
    const auto panel = ChooserRect();
    return Sexy::Rect(panel.mX + 22, panel.mY + panel.mHeight - 42,
        Sexy::IMAGE_SEEDCHOOSER_BUTTON2->mWidth, Sexy::IMAGE_SEEDCHOOSER_BUTTON2->mHeight);
}

Sexy::Rect FusionRuntime::MenuButtonRect() const { return Sexy::Rect(mApp->mWidth - 119, 0, 117, 46); }
Sexy::Rect FusionRuntime::ShovelButtonRect() const
{
    const float scale = BankScale();
    return Sexy::Rect(static_cast<int>(BankWidth() * scale), 0,
        static_cast<int>(Sexy::IMAGE_SHOVELBANK->mWidth * scale), static_cast<int>(Sexy::IMAGE_SEEDBANK->mHeight * scale));
}

bool FusionRuntime::CellAt(int x, int y, uint8_t& column, uint8_t& row) const
{
    float worldX, worldY;
    if (!GetFusionViewport(mApp->mWidth, mApp->mHeight).UnprojectScreen(static_cast<float>(x), static_cast<float>(y), worldX, worldY)) return false;
    const int c = static_cast<int>(std::floor((worldX + 5.6f) / 1.35f));
    const float spacing = mDescriptor.rows == 6 ? 1.45f : 1.67f;
    const int r = static_cast<int>(std::floor((2.3f - worldY) / spacing + 0.5f));
    if (c < 0 || c >= mDescriptor.columns || r < 0 || r >= mDescriptor.rows) return false;
    column = static_cast<uint8_t>(c); row = static_cast<uint8_t>(r);
    return true;
}

void FusionRuntime::DrawCard(Sexy::Graphics* g, const Sexy::Rect& rect, FusionPlantId kind, bool extra,
    int price, bool pending, bool selected, bool disabled, float cooldown, float fullRecharge, const SexyString& caption)
{
    if (rect.mWidth <= 0 || rect.mHeight <= 0) return;
    g->PushState();
    const float sx = rect.mWidth / 50.0f, sy = rect.mHeight / 70.0f;
    g->DrawImage(Sexy::IMAGE_SEEDPACKET_LARGER, rect,
        Sexy::Rect(0, 0, Sexy::IMAGE_SEEDPACKET_LARGER->mWidth, Sexy::IMAGE_SEEDPACKET_LARGER->mHeight));
    if (pending)
        NetUi::Literal(g, Sexy::FONT_BRIANNETOD16, _S("?"), rect.mX + rect.mWidth / 2, rect.mY + static_cast<int>(43 * sy), Sexy::Color(35, 55, 30), 0);
    else
    {
        mPresentation.DrawPlantPortrait(g, kind, Sexy::Rect(rect.mX + static_cast<int>(5 * sx), rect.mY + static_cast<int>(12 * sy),
            static_cast<int>(40 * sx), static_cast<int>(41 * sy)));
        NetUi::Literal(g, Sexy::FONT_PICO129, Number(price), rect.mX + static_cast<int>(32 * sx), rect.mY + rect.mHeight - 5, Sexy::Color::Black, 1);
        if (extra) NetUi::Literal(g, Sexy::FONT_PICO129, _S("EXTRA"), rect.mX + rect.mWidth / 2, rect.mY + 11, Sexy::Color::Black, 0);
    }
    if (disabled || cooldown > 0.0f)
    {
        g->SetColor(Sexy::Color(35, 30, 20, disabled ? 110 : 135));
        const int height = cooldown > 0.0f && fullRecharge > 0.0f ? static_cast<int>((rect.mHeight - 2) * cooldown / fullRecharge) : rect.mHeight - 2;
        g->FillRect(rect.mX + 1, rect.mY + 1, rect.mWidth - 2, height);
    }
    if (selected)
    {
        g->SetColor(Sexy::Color(230, 255, 80));
        g->DrawRect(rect.mX - 2, rect.mY - 2, rect.mWidth + 3, rect.mHeight + 3);
    }
    if (!caption.empty()) NetUi::Literal(g, Sexy::FONT_PICO129, caption, rect.mX + rect.mWidth / 2, rect.mY + rect.mHeight + 10, NetUi::Cream(), 0);
    g->PopState();
}

void FusionRuntime::DrawNativeButton(Sexy::Graphics* g, std::unique_ptr<GameButton>& button,
    int id, const Sexy::Rect& rect, const SexyString& label, bool disabled)
{
    if (!button)
    {
        button = std::make_unique<GameButton>(id);
        if (id == 2)
        {
            button->mButtonImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON;
            button->mDisabledImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_DISABLED;
            button->mOverOverlayImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_GLOW;
            button->SetFont(Sexy::FONT_DWARVENTODCRAFT18YELLOW);
            button->mTextOffsetY = -1;
        }
        else if (id == 1)
        {
            button->mButtonImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON2;
            button->mOverImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON2_GLOW;
            button->SetFont(Sexy::FONT_BRIANNETOD12);
            button->mTextOffsetY = 1;
        }
        else button->mDrawStoneButton = true;
    }
    button->Resize(rect.mX, rect.mY, rect.mWidth, rect.mHeight);
    button->SetLabel(label);
    button->SetDisabled(disabled);
    button->mColors[GameButton::COLOR_LABEL] = disabled ? Sexy::Color(64, 64, 64) : id == 1 ? Sexy::Color(42, 42, 90) : Sexy::Color::White;
    button->mColors[GameButton::COLOR_LABEL_HILITE] = button->mColors[GameButton::COLOR_LABEL];
    button->mIsOver = rect.Contains(mMouseX, mMouseY);
    button->mIsDown = mPressedButton == id;
    g->PushState(); button->Draw(g); g->PopState();
}

void FusionRuntime::DrawChooser(Sexy::Graphics* g)
{
    const auto panel = ChooserRect();
    g->DrawImage(Sexy::IMAGE_SEEDCHOOSER_BACKGROUND, panel.mX, panel.mY);
    NetUi::Literal(g, Sexy::FONT_DWARVENTODCRAFT18YELLOW, TodStringTranslate(_S("[CHOOSE_YOUR_PLANTS]")), panel.mX + 229, panel.mY + 23, Sexy::Color::White, 0);
    const int slot = LocalSlot();
    int owned = 0;
    for (const auto& packet : mBank.State().packets) owned += packet.owner == slot;
    const int quota = slot >= 0 && slot < static_cast<int>(kFusionBankPlayers) ? mBank.State().quotas[slot] : 0;
    NetUi::Literal(g, Sexy::FONT_BRIANNETOD12, LocalCanAct() ? _S("YOUR CARDS: ") + Number(owned) + _S(" / ") + Number(quota) : _S("SPECTATING"),
        panel.mX + 229, panel.mY + 55, NetUi::Cream(), 0);
    for (size_t i = 0; i < mDescriptor.eligible.size(); ++i)
    {
        const auto& card = mDescriptor.eligible[i];
        const FusionBankPacket* chosen = nullptr;
        for (const auto& packet : mBank.State().packets)
            if (!packet.mysteryPending && packet.kind == card.kind && packet.isExtra == card.isExtra) { chosen = &packet; break; }
        const bool own = chosen && chosen->owner == slot;
        const bool disabled = !LocalCanAct() || (chosen && !own) || ((mBank.State().mysteryMask & (1U << slot)) != 0);
        const auto rect = EligibleCardRect(i);
        DrawCard(g, rect, card.kind, card.isExtra, card.baseCost, false, own, disabled, 0, card.rechargeSeconds, _S(""));
    }
    const bool mystery = slot >= 0 && slot < static_cast<int>(kFusionBankPlayers) && (mBank.State().mysteryMask & (1U << slot));
    const auto random = MysteryButtonRect(); const auto ready = ReadyButtonRect();
    DrawNativeButton(g, mMysteryButton, 1, random, mystery ? _S("MANUAL") : _S("MYSTERY ?"), !LocalCanAct() || quota == 0);
    const bool isReady = slot >= 0 && slot < static_cast<int>(kFusionBankPlayers) && (mBank.State().readyMask & (1U << slot));
    DrawNativeButton(g, mReadyButton, 2, ready, isReady ? _S("NOT READY") : _S("READY"), !LocalCanAct() || owned != quota);
    int countReady = 0, countPlayers = 0;
    for (int i = 0; i < static_cast<int>(kFusionBankPlayers); ++i)
    { countReady += (mBank.State().readyMask >> i) & 1; countPlayers += (mBank.State().activeMask >> i) & 1; }
    NetUi::Literal(g, Sexy::FONT_BRIANNETOD12, Number(countReady) + _S(" / ") + Number(countPlayers), panel.mX + panel.mWidth - 64, panel.mY + panel.mHeight - 23, NetUi::Cream(), 0);
}

void FusionRuntime::DrawBank(Sexy::Graphics* g)
{
    const float scale = BankScale();
    const int height = static_cast<int>(Sexy::IMAGE_SEEDBANK->mHeight * scale);
    const int left = static_cast<int>(79 * scale), right = static_cast<int>(BankWidth() * scale);
    const int cap = static_cast<int>(12 * scale);
    g->DrawImage(Sexy::IMAGE_SEEDBANK, Sexy::Rect(0, 0, left, height), Sexy::Rect(0, 0, 79, Sexy::IMAGE_SEEDBANK->mHeight));
    g->DrawImage(Sexy::IMAGE_SEEDBANK, Sexy::Rect(left, 0, right - left - cap, height),
        Sexy::Rect(79, 0, Sexy::IMAGE_SEEDBANK->mWidth - 91, Sexy::IMAGE_SEEDBANK->mHeight));
    g->DrawImage(Sexy::IMAGE_SEEDBANK, Sexy::Rect(right - cap, 0, cap, height),
        Sexy::Rect(Sexy::IMAGE_SEEDBANK->mWidth - 12, 0, 12, Sexy::IMAGE_SEEDBANK->mHeight));
    NetUi::Literal(g, Sexy::FONT_CONTINUUMBOLD14, Number(mSimulation.State().sun), static_cast<int>(34 * scale), static_cast<int>(78 * scale), Sexy::Color::Black, 0);
    for (size_t index = 0; index < mBank.State().targetCount; ++index)
    {
        const auto rect = BankSlotRect(index);
        g->DrawImage(Sexy::IMAGE_SEEDPACKETSILHOUETTE, rect,
            Sexy::Rect(0, 0, Sexy::IMAGE_SEEDPACKETSILHOUETTE->mWidth, Sexy::IMAGE_SEEDPACKETSILHOUETTE->mHeight));
    }
    for (const auto* packet : DisplayPackets())
    {
        const bool own = packet->owner == LocalSlot();
        const auto rect = PacketRect(packet->id);
        DrawCard(g, rect, packet->kind, packet->isExtra, packet->price, packet->mysteryPending,
            packet->id == mSelectedPacket, !packet->mysteryPending && packet->price > mSimulation.State().sun,
            packet->remainingRechargeSeconds, packet->rechargeSeconds, _S(""));
        if (!own && mApp->mNetManager)
        {
            const auto color = LawnNet::GetRelativePlayerColor(LocalSlot(), packet->owner);
            g->SetColor(Sexy::Color(color.r, color.g, color.b, mApp->mNetManager->mConfig.mHotbarTintAlpha));
            g->FillRect(rect.mX + 2, rect.mY + 2, rect.mWidth - 4, rect.mHeight - 4);
        }
    }
    const auto shovel = ShovelButtonRect();
    g->DrawImage(Sexy::IMAGE_SHOVELBANK, shovel, Sexy::Rect(0, 0, Sexy::IMAGE_SHOVELBANK->mWidth, Sexy::IMAGE_SHOVELBANK->mHeight));
    if (!mShovelSelected)
    {
        const int width = static_cast<int>(Sexy::IMAGE_SHOVEL_HI_RES->mWidth * 0.5f * scale);
        const int shovelHeight = static_cast<int>(Sexy::IMAGE_SHOVEL_HI_RES->mHeight * 0.5f * scale);
        g->DrawImage(Sexy::IMAGE_SHOVEL_HI_RES, Sexy::Rect(shovel.mX + static_cast<int>(5 * scale), static_cast<int>(5 * scale), width, shovelHeight),
            Sexy::Rect(0, 0, Sexy::IMAGE_SHOVEL_HI_RES->mWidth, Sexy::IMAGE_SHOVEL_HI_RES->mHeight));
    }
}

void FusionRuntime::Draw(Sexy::Graphics* g)
{
    if (!mInitialized || !g) return;
    if (mShovelTargetId)
    {
        const bool exists = std::any_of(mSimulation.State().plants.begin(), mSimulation.State().plants.end(),
            [&](const auto& plant) { return plant.id == mShovelTargetId; });
        if (!exists) { mShovelSelected = false; mShovelTargetId = 0; }
    }
    g->PushState(); g->mTransX = 0; g->mTransY = 0; g->ClearClipRect();
    mPresentation.Draw(g, mSimulation.State(), mApp->mWidth, mApp->mHeight, mCosmeticElapsed, BankScale());
    DrawBank(g);
    const auto menu = MenuButtonRect();
    DrawNativeButton(g, mMenuButton, 3, menu, _S("[MENU_BUTTON]"), false);
    if (Phase() == FusionBankPhase::Choosing) DrawChooser(g);
    else if (Phase() == FusionBankPhase::Intro)
        NetUi::Literal(g, Sexy::FONT_DWARVENTODCRAFT24, _S("READY... SET..."), mApp->mWidth / 2, mApp->mHeight / 2, NetUi::Cream(), 0);
    if (mPaused)
    {
        g->SetColor(Sexy::Color(0, 0, 0, 120)); g->FillRect(0, 0, mApp->mWidth, mApp->mHeight);
        NetUi::Literal(g, Sexy::FONT_DWARVENTODCRAFT24, _S("PAUSED"), mApp->mWidth / 2, mApp->mHeight / 2, NetUi::Cream(), 0);
    }
    if (!mNotice.empty()) NetUi::Literal(g, Sexy::FONT_BRIANNETOD12, Sexy::StringToSexyString(mNotice), mApp->mWidth / 2, mApp->mHeight - 15, NetUi::Cream(), 0);
    if (mShovelSelected && Phase() == FusionBankPhase::Playing && !mPaused && !mApp->GetDialogCount() && !mApp->mNetChatTyping && mMouseX >= 0 && mMouseY >= 0)
    {
        // Same cursor placement as CursorObject::DrawContent: (-25,-35)+(22,-22).
        g->DrawImage(Sexy::IMAGE_SHOVEL_HI_RES, Sexy::Rect(mMouseX - 3, mMouseY - 57,
            Sexy::IMAGE_SHOVEL_HI_RES->mWidth / 2, Sexy::IMAGE_SHOVEL_HI_RES->mHeight / 2),
            Sexy::Rect(0, 0, Sexy::IMAGE_SHOVEL_HI_RES->mWidth, Sexy::IMAGE_SHOVEL_HI_RES->mHeight));
    }
    g->PopState();
}

bool FusionRuntime::OnMouseDown(int x, int y, int button)
{
    mMouseX = x; mMouseY = y;
    if (!mInitialized || mApp->GetDialogCount() || mApp->mNetChatTyping) return true;
    if (button < 0) { mSelectedPacket = 0; mShovelSelected = false; mShovelTargetId = 0; mPressedButton = 0; return true; }
    if (button != 1) return true;
    if (MenuButtonRect().Contains(x, y)) { mPressedButton = 3; return true; }
    if (!LocalCanAct() || mPaused) return true;
    FusionCommand command;
    if (Phase() == FusionBankPhase::Choosing)
    {
        if (MysteryButtonRect().Contains(x, y)) { mPressedButton = 1; return true; }
        if (ReadyButtonRect().Contains(x, y)) { mPressedButton = 2; return true; }
        for (const auto* packet : DisplayPackets()) if (PacketRect(packet->id).Contains(x, y))
        {
            if (packet->owner == LocalSlot() && !packet->mysteryPending)
            { command.action = FusionCommandKind::Unchoose; command.packetId = packet->id; Submit(command); }
            return true;
        }
        for (size_t i = 0; i < mDescriptor.eligible.size(); ++i) if (EligibleCardRect(i).Contains(x, y))
        {
            const auto& card = mDescriptor.eligible[i];
            for (const auto& packet : mBank.State().packets) if (!packet.mysteryPending && packet.owner == LocalSlot() && packet.kind == card.kind && packet.isExtra == card.isExtra)
            { command.action = FusionCommandKind::Unchoose; command.packetId = packet.id; Submit(command); return true; }
            command.action = FusionCommandKind::Choose; command.kind = card.kind; command.isExtra = card.isExtra;
            Submit(command); return true;
        }
        return true;
    }
    if (Phase() != FusionBankPhase::Playing) return true;
    if (ShovelButtonRect().Contains(x, y)) { mShovelSelected = !mShovelSelected; mShovelTargetId = 0; mSelectedPacket = 0; return true; }
    for (const auto* packet : DisplayPackets()) if (PacketRect(packet->id).Contains(x, y))
    {
        if (mBank.PeekReadyPacket(mDescriptor.epoch, LocalSlot(), packet->id) && packet->price <= mSimulation.State().sun)
        { mSelectedPacket = mSelectedPacket == packet->id ? 0 : packet->id; mShovelSelected = false; mShovelTargetId = 0; }
        return true;
    }
    const auto viewport = GetFusionViewport(mApp->mWidth, mApp->mHeight);
    for (auto sun = mSimulation.State().suns.rbegin(); sun != mSimulation.State().suns.rend(); ++sun)
    {
        float sx, sy; viewport.ProjectWorld(sun->x, sun->y, sx, sy);
        const float radius = (std::max)(12.0f, viewport.pixelsPerUnit * 0.45f);
        if (!sun->collecting && std::fabs(sx - x) <= radius && std::fabs(sy - y) <= radius)
        { command.action = FusionCommandKind::CollectSun; command.targetId = sun->id; Submit(command); return true; }
    }
    if (!CellAt(x, y, command.column, command.row)) return true;
    const auto* plant = mSimulation.PlantAt(command.column, command.row);
    command.targetId = plant ? plant->id : 0;
    if (mShovelSelected)
    {
        if (plant && !mShovelTargetId)
        {
            command.action = FusionCommandKind::Shovel;
            if (Submit(command)) mShovelTargetId = command.targetId;
        }
        return true;
    }
    if (mSelectedPacket)
    {
        command.action = FusionCommandKind::Plant; command.packetId = mSelectedPacket;
        Submit(command);
    }
    return true;
}

bool FusionRuntime::OnMouseUp(int x, int y, int button)
{
    mMouseX = x; mMouseY = y;
    const int pressed = mPressedButton; mPressedButton = 0;
    if (!mInitialized || button != 1 || mApp->GetDialogCount() || mApp->mNetChatTyping) return true;
    if (pressed == 3 && MenuButtonRect().Contains(x, y)) { mApp->DoPauseDialog(); return true; }
    if (Phase() != FusionBankPhase::Choosing || !LocalCanAct() || mPaused) return true;
    FusionCommand command;
    if (pressed == 1 && MysteryButtonRect().Contains(x, y))
    { command.action = FusionCommandKind::Mystery; command.enabled = (mBank.State().mysteryMask & (1U << LocalSlot())) == 0; Submit(command); }
    else if (pressed == 2 && ReadyButtonRect().Contains(x, y))
    { command.action = FusionCommandKind::Ready; command.enabled = (mBank.State().readyMask & (1U << LocalSlot())) == 0; Submit(command); }
    return true;
}

bool FusionRuntime::OnMouseMove(int x, int y) { mMouseX = x; mMouseY = y; return true; }

bool FusionRuntime::OnKeyDown(Sexy::KeyCode key, bool repeat)
{
    if (!mInitialized || mApp->GetDialogCount() || mApp->mNetChatTyping) return false;
    if (key == Sexy::KEYCODE_ESCAPE)
    {
        if (mSelectedPacket || mShovelSelected) { mSelectedPacket = 0; mShovelSelected = false; mShovelTargetId = 0; }
        else if (!repeat) mApp->DoPauseDialog();
        return true;
    }
    if (!LocalCanAct() || mPaused) return true;
    if (Phase() == FusionBankPhase::Choosing && (key == Sexy::KEYCODE_RETURN || key == Sexy::KEYCODE_SPACE))
    {
        if (!repeat)
        { FusionCommand command; command.action = FusionCommandKind::Ready; command.enabled = (mBank.State().readyMask & (1U << LocalSlot())) == 0; Submit(command); }
        return true;
    }
    int digit = static_cast<int>(key);
    if (key >= Sexy::KEYCODE_NUMPAD0 && key <= Sexy::KEYCODE_NUMPAD9) digit = '0' + (key - Sexy::KEYCODE_NUMPAD0);
    if (digit == 'R' && Phase() == FusionBankPhase::Playing)
    { if (!repeat) { mShovelSelected = !mShovelSelected; mShovelTargetId = 0; mSelectedPacket = 0; } return true; }
    if (digit < '0' || digit > '9' || Phase() != FusionBankPhase::Playing) return false;
    if (repeat) return true;
    const int index = digit == '0' ? 9 : digit - '1';
    const auto packets = DisplayPackets();
    int ownIndex = 0;
    for (const auto* packet : packets) if (packet->owner == LocalSlot())
    {
        if (ownIndex++ != index) continue;
        const auto rect = PacketRect(packet->id);
        return OnMouseDown(rect.mX + rect.mWidth / 2, rect.mY + rect.mHeight / 2, 1);
    }
    return true;
}
}
