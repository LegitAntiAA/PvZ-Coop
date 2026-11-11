#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Cutscene.h"
#include "../SeedPacket.h"
#include "../Plant.h"
#include "../CursorObject.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameSelector.h"
#include "../Widget/GameButton.h"
#include "../Widget/LawnDialog.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace LawnNet { bool NetSmokeMysteryObserverCheckpoint(LawnApp* theApp, const char* theCheckpoint); }

namespace
{
struct MysterySmokeState
{
    int mStage = 0, mCaptured = -1, mObserved = -1, mManual[2] = {-1, -1}, mManualPicked = 0;
    int mDifficulty = 10, mCost = 0, mSun = 0, mCol = -1, mRow = -1;
    int mCooldownFirst = -1;
    uint32_t mLastAck = 0, mFixedHash = 0, mObservedTick = 0;
    uint32_t mLastDiagnosticTick = 0;
    uint32_t mWarningHash = 0, mWarningObservedTick = 0;
    uint32_t mCooldownSyncTick = 0, mCooldownTick = 0;
    int mDiagnosticStage = -1;
    uint16_t mSpendId = 0;
    bool mPrepared = false, mIntroSeen = false, mMoneySet = false, mPicked = false, mPlanted = false;
    bool mWarningClickQueued = false;
    bool mCooldownProved = false;
    bool mFaultBaseline = false, mFaultInjected = false, mFaultRecovered = false, mFaultProofCaptured = false;
    int mFaultStage = 0;
    uint32_t mFaultHash = 0, mFaultEpoch = 0, mFaultTick = 0;
    std::vector<uint16_t> mPending;
    std::string mRelease;
};
MysterySmokeState s;

bool Mouse(LawnApp* app, bool down, float x, float y)
{
    SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
    e.button.down = down; e.button.clicks = 1; e.button.x = x; e.button.y = y;
    return SDL_PushEvent(&e);
}

void Move(LawnApp* app, float x, float y)
{
    SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    e.motion.x = x; e.motion.y = y; SDL_PushEvent(&e);
}

bool Button(LawnApp* app, GameButton* b)
{
    if (!b || b->mDisabled || b->mBtnNoDraw) return false;
    const float x = (float)(b->mX + b->mWidth / 2), y = (float)(b->mY + b->mHeight / 2);
    if (!b->IsMouseOver()) { Move(app, x, y); return false; }
    return Mouse(app, true, x, y) && Mouse(app, false, x, y);
}

bool Record(LawnApp* app, const char* tag)
{
    if (s.mObserved != s.mStage) { s.mObserved = s.mStage; s.mObservedTick = GetTickCount(); }
    if (GetTickCount() - s.mObservedTick < 150U) return false;
    if (s.mCaptured != s.mStage)
    {
        if (!app->CoopSmokeCaptureFrame((std::string("mystery_") + tag).c_str()))
        { app->CoopSmokeFail("mystery game renderer capture failed"); return false; }
        s.mCaptured = s.mStage;
        LawnNet::NetTrace("SMOKE_MYSTERY_FRAME role=%s step=%s capture=1",
            app->NetIsHost() ? "host" : "client", tag);
    }
    const std::string ack = std::string("MY_ACK_") + tag;
    const std::string release = std::string("MY_OK_") + tag;
    bool seen = false;
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == 1 && line.mText == (app->NetIsHost() ? release : ack)) seen = true;
    if (seen)
    {
        if (app->NetIsClient()) s.mRelease = release;
        return true;
    }
    if (app->NetIsClient() && (!s.mLastAck || GetTickCount() - s.mLastAck >= 800U))
    { s.mLastAck = GetTickCount(); app->mNetManager->SendChat(ack); }
    return false;
}

uint32_t FixedHash(SeedChooserScreen* chooser)
{
    LawnNet::NetBuffer b;
    for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; ++i)
        if (chooser->mChosenSeeds[i].mCrazyDavePicked)
        { b.WriteU8((uint8_t)i); b.WriteU8((uint8_t)chooser->mChosenSeeds[i].mSeedIndexInBank); }
    return LawnNet::NetFnv1a32(b.Data(), b.Size());
}

SeedPacket* FindPacket(LawnApp* app, uint16_t id)
{
    if (!app->mBoard || !app->mBoard->mSeedBank) return nullptr;
    for (int i = 0; i < app->mBoard->mSeedBank->mNumPackets; ++i)
        if (app->mBoard->mSeedBank->mSeedPackets[i].mNetId == id) return &app->mBoard->mSeedBank->mSeedPackets[i];
    return nullptr;
}

bool HasCommand(LawnApp* app, const char* command, int slot = 0)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == command) return true;
    return false;
}

void Control(LawnApp* app, const char* command)
{
    if (!HasCommand(app, command, app->mNetManager->GetMySlot()) &&
        (!s.mLastAck || GetTickCount() - s.mLastAck >= 800U))
    { s.mLastAck = GetTickCount(); app->mNetManager->SendChat(command); }
}

uint32_t ChoiceFingerprint(LawnApp* app)
{
    LawnNet::NetManager* net = app->mNetManager;
    SeedChooserScreen* chooser = app->mSeedChooserScreen;
    LawnNet::NetBuffer b;
    b.WriteU32(net->GetInputEpoch()); b.WriteU8(net->mChooserOpen); b.WriteU8(net->mChooserMysteryMask);
    b.WriteU8((uint8_t)net->mPendingChooserStarts.size());
    b.WriteI32(chooser->mSeedsInBank); b.WriteI32(chooser->mSeedsInFlight);
    for (int slot = 0; slot < LawnNet::NET_MAX_PLAYERS; ++slot)
    {
        b.WriteU8(net->GetPlayer(slot).mReady); b.WriteI32(net->mChooserPickQuota[slot]);
        b.WriteI32(chooser->mCoopRemotePickCount[slot]);
        for (int j = 0; j < chooser->mCoopRemotePickCount[slot] && j < 12; ++j)
        { b.WriteI32(chooser->mCoopRemotePicks[slot][j]); b.WriteI32(chooser->mCoopRemoteImitaters[slot][j]); }
    }
    for (const ChosenSeed& seed : chooser->mChosenSeeds)
    { b.WriteI32(seed.mSeedState); b.WriteI32(seed.mSeedIndexInBank); b.WriteI32(seed.mImitaterType); b.WriteU8(seed.mCrazyDavePicked); }
    SeedBank* bank = app->mBoard->mSeedBank;
    b.WriteI32(bank->mNumPackets);
    for (int i = 0; i < bank->mNumPackets; ++i)
    {
        const SeedPacket& p = bank->mSeedPackets[i];
        b.WriteU16(p.mNetId); b.WriteI32(p.mPacketType); b.WriteI32(p.mImitaterType);
        b.WriteU8(p.mNetOwnerSlot); b.WriteU8(p.mRandomDiscountLevel);
    }
    return LawnNet::NetFnv1a32(b.Data(), b.Size());
}

bool MalformedStartBeforeReady(LawnApp* app, bool host)
{
    const char* fault = getenv("PVZ_COOP_SMOKE_MYSTERY_FAULT");
    if (!fault || strcmp(fault, "MalformedStart") || s.mFaultRecovered) return true;
    auto* net = app->mNetManager; auto* chooser = app->mSeedChooserScreen;
    if (!s.mFaultBaseline)
    {
        if (net->mChooserMysteryMask != 2 || !chooser->CoopAllPicksComplete()) return false;
        s.mFaultBaseline = true; s.mFaultHash = ChoiceFingerprint(app); s.mFaultEpoch = net->GetInputEpoch();
    }
    if (host)
    {
        if (!HasCommand(app, "MY_BAD_BASE_READY", 1)) return false;
        if (!s.mFaultInjected)
        {
            LawnNet::NetBuffer truncated;
            truncated.WriteU8((uint8_t)net->mChooserPickQuota[0]); truncated.WriteU8(4); truncated.WriteU8(net->mChooserMysteryMask);
            net->BroadcastMessage(LawnNet::NETMSG_CHOOSER_START, truncated);
            s.mFaultInjected = true;
            LawnNet::NetTrace("SMOKE_MYSTERY_FAULT_INJECT kind=MalformedStart bytes=3");
        }
        Control(app, "MY_BAD_START_SENT");
        if (!HasCommand(app, "MY_BAD_RECOVERED", 1)) return false;
    }
    else if (s.mFaultStage == 0)
    {
        Control(app, "MY_BAD_BASE_READY");
        // The following chat is on the same ordered connection as the bad frame.
        if (!HasCommand(app, "MY_BAD_START_SENT")) return false;
        if (!s.mFaultTick) s.mFaultTick = GetTickCount();
        if (GetTickCount() - s.mFaultTick < 150U) return false;
        if (!net->mChooserOpen || net->GetInputEpoch() != s.mFaultEpoch || ChoiceFingerprint(app) != s.mFaultHash)
        { app->CoopSmokeFail("malformed Mystery START changed chooser, epoch, bank or readiness"); return false; }
        if (!app->CoopSmokeCaptureFrame("mystery_malformed_rejected"))
        { app->CoopSmokeFail("malformed Mystery rejection capture failed"); return false; }
        LawnNet::NetTrace("SMOKE_MYSTERY_FAULT_REJECT kind=MalformedStart unchanged=1 chooserOpen=1 epoch=%u", s.mFaultEpoch);
        s.mFaultStage = 1;
        return false;
    }
    else if (s.mFaultStage == 1)
    {
        Control(app, "MY_BAD_REJECTED");
        if (HasCommand(app, "MY_BAD_REJECTED", 1) && Button(app, chooser->mRandomButton)) s.mFaultStage = 2;
        return false;
    }
    else if (s.mFaultStage == 2)
    {
        if (!net->IsChooserMystery(1) && Button(app, chooser->mRandomButton)) s.mFaultStage = 3;
        return false;
    }
    else
    {
        if (!net->IsChooserMystery(1) || !chooser->CoopAllPicksComplete()) return false;
        Control(app, "MY_BAD_RECOVERED");
        if (!HasCommand(app, "MY_BAD_RECOVERED", 1)) return false;
    }
    s.mFaultRecovered = true;
    LawnNet::NetTrace("SMOKE_MYSTERY_FAULT_RECOVER role=%s kind=MalformedStart toggleOffOn=1 chooserOpen=1", host ? "host" : "client");
    return true;
}

bool RngRecovery(LawnApp* app, bool host)
{
    const char* fault = getenv("PVZ_COOP_SMOKE_MYSTERY_FAULT");
    if (!fault || strcmp(fault, "RngFailure") || s.mFaultRecovered || s.mStage != 6) return false;
    auto* net = app->mNetManager; auto* chooser = app->mSeedChooserScreen;
    if (app->mGameScene == SCENE_PLAYING)
    { app->CoopSmokeFail("failed Mystery RNG crossed the PLAYING/reveal barrier"); return true; }
    if (!chooser || !chooser->mMouseVisible || !app->mBoard || !app->mBoard->mCutScene ||
        !app->mBoard->mSeedBank || !app->mBoard->mCutScene->mSeedChoosing ||
        !net->mChooserOpen || net->GetInputEpoch() <= s.mFaultEpoch) return false;
    if (!s.mIntroSeen || FixedHash(chooser) != s.mFixedHash)
    { app->CoopSmokeFail("failed Mystery RNG lost the secret intro proof or mandatory fixed cards"); return true; }
    if (net->mChooserMysteryMask || net->GetReadyCount() || chooser->mSeedsInFlight) return true;
    for (int slot = 0; slot < LawnNet::NET_MAX_PLAYERS; ++slot)
    {
        const int count = slot == net->GetMySlot() ? chooser->CoopGetLocalQuotaPickCount() : chooser->mCoopRemotePickCount[slot];
        if (count != (slot == 0 ? 3 : 0)) return true;
    }
    for (int i = 0; i < app->mBoard->mSeedBank->mNumPackets; ++i)
        if (app->mBoard->mSeedBank->mSeedPackets[i].mRandomDiscountLevel &&
            !app->mBoard->mSeedBank->mSeedPackets[i].IsMysteryPending())
        { app->CoopSmokeFail("failed Mystery RNG exposed a random plant before fresh selection"); return true; }
    if (!s.mFaultProofCaptured)
    {
        s.mFaultProofCaptured = true;
        LawnNet::NetTrace("SMOKE_MYSTERY_FAULT_REOPEN role=%s kind=RngFailure oldEpoch=%u epoch=%u mask=0 ready=0 mutable=0 noPlaying=1",
            host ? "host" : "client", s.mFaultEpoch, net->GetInputEpoch());
    }
    if (host)
    {
        if (!Record(app, "rng_reopened")) return true;
        Control(app, "MY_GO_RNG_RETRY");
    }
    else
    {
        if (!HasCommand(app, "MY_GO_RNG_RETRY")) { Record(app, "rng_reopened"); return true; }
    }
    if (!HasCommand(app, "MY_GO_RNG_RETRY")) return true;
    s.mFaultRecovered = true; s.mStage = 4; s.mManualPicked = 0;
    s.mCaptured = -1; s.mObserved = -1; s.mIntroSeen = false; s.mPending.clear(); s.mSpendId = 0;
    LawnNet::NetTrace("SMOKE_MYSTERY_FAULT_RECOVER role=%s kind=RngFailure freshPick=1", host ? "host" : "client");
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateMystery(LawnApp* app)
{
    const char* enabled = getenv("PVZ_COOP_SMOKE_MYSTERY");
    if (!app->CoopSmokeActive() || !enabled || strcmp(enabled, "1")) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_8_SOAK ||
        app->mCoopSmokeManifest.mSkipIntro || app->mWidth != 800 || app->mHeight != 600)
    { app->CoopSmokeFail("mystery fixture requires isolated full-intro Adventure8Soak800"); return true; }
    NetManager* net = app->mNetManager;
    if (!net || !app->mPlayerInfo) return true;
    const char* observer = getenv("PVZ_COOP_SMOKE_MYSTERY_OBSERVER");
    const bool observerRun = observer && (!strcmp(observer, "IntroJoin") || !strcmp(observer, "PlayingKick"));
    const char* fault = getenv("PVZ_COOP_SMOKE_MYSTERY_FAULT");
    const bool malformedFault = fault && !strcmp(fault, "MalformedStart");
    const bool rngFault = fault && !strcmp(fault, "RngFailure");
    if ((fault && *fault && strcmp(fault, "None") && !malformedFault && !rngFault) ||
        (observerRun && (malformedFault || rngFault)))
    { app->CoopSmokeFail("invalid or combined Mystery fault fixture"); return true; }
    if (net->GetPlayerCount() != 2 && !(observerRun && net->GetPlayerCount() == 3 &&
        net->GetPlayer(2).mActive && net->GetPlayer(2).mSpectator)) return true;
    const bool host = app->mCoopSmokeManifest.IsHost();
    if (!host && !s.mRelease.empty())
    {
        bool echoed = false;
        for (const auto& line : net->mChatLines) echoed |= line.mSlot == 1 && line.mText == s.mRelease;
        if (echoed) s.mRelease.clear();
        else if (!s.mLastAck || GetTickCount() - s.mLastAck >= 800U)
        { s.mLastAck = GetTickCount(); net->SendChat(s.mRelease); }
    }
    app->mCoopSmokePeerReady = true;
    const char* requested = getenv("PVZ_COOP_SMOKE_MYSTERY_DIFFICULTY");
    s.mDifficulty = requested ? atoi(requested) : 10;
    if (s.mDifficulty < 1 || s.mDifficulty > 10)
    { app->CoopSmokeFail("invalid mystery fixture difficulty"); return true; }
    if (s.mStage == 0)
    {
        if (!app->mGameSelector || app->mGameSelector->mSelectorState != SELECTOR_IDLE) return true;
        if (host && !s.mPrepared)
        {
            app->mPlayerInfo->SetLevel(8); app->mPlayerInfo->mFinishedAdventure = 1;
            app->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] = 4;
            app->mPlayerInfo->mPurchases[STORE_ITEM_PLANT_IMITATER] = 1;
            net->SetDifficulty(s.mDifficulty); app->NetBroadcastProfile(); s.mPrepared = true;
        }
        if (app->mPlayerInfo->mLevel != 8 || app->mPlayerInfo->mFinishedAdventure != 1 ||
            app->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] != 4 ||
            !app->mPlayerInfo->mPurchases[STORE_ITEM_PLANT_IMITATER] || net->GetDifficulty() != s.mDifficulty) return true;
        if (Record(app, "profile"))
        {
            ++s.mStage;
            if (host) { app->mCoopSmokeGameStarted = true; app->KillGameSelector(); app->PreNewGame(GAMEMODE_ADVENTURE, false, 8); }
        }
        return true;
    }
    SeedChooserScreen* chooser = app->mSeedChooserScreen;
    LawnDialog* warning = dynamic_cast<LawnDialog*>(app->GetDialog(DIALOG_CHOOSER_WARNING));
    if (warning)
    {
        if (!host || s.mStage != 6)
        { app->CoopSmokeFail("unexpected chooser warning outside the host commit stage"); return true; }
        const SexyString sunKey = _S("[SEED_CHOOSER_SUN_WARNING]"), nightSunKey = _S("[SEED_CHOOSER_NIGHT_SUN_WARNING]");
        if (warning->mDialogLines == sunKey || warning->mDialogLines == nightSunKey ||
            warning->mDialogLines == TodStringTranslate(sunKey) || warning->mDialogLines == TodStringTranslate(nightSunKey))
        { app->CoopSmokeFail("sun warning appeared despite an active Mystery deck"); return true; }
        const std::string body = SexyStringToStringFast(warning->mDialogLines);
        const uint32_t hash = NetFnv1a32(body.data(), body.size());
        if (s.mWarningHash != hash)
        { s.mWarningHash = hash; s.mWarningObservedTick = GetTickCount(); s.mWarningClickQueued = false; }
        if (GetTickCount() - s.mWarningObservedTick < 150U || s.mWarningClickQueued) return true;
        LawnStoneButton* yes = warning->mLawnYesButton;
        if (!yes || !yes->mVisible || yes->mDisabled) return true;
        const int x = warning->mX + yes->mX + yes->mWidth / 2, y = warning->mY + yes->mY + yes->mHeight / 2;
        if (app->mWidgetManager->mLastMouseX != x || app->mWidgetManager->mLastMouseY != y)
        { Move(app, (float)x, (float)y); return true; }
        if (app->mWidgetManager->GetWidgetAt(x, y, nullptr, nullptr) != yes) return true;
        if (!app->CoopSmokeCaptureFrame("mystery_stock_warning") ||
            !Mouse(app, true, (float)x, (float)y) || !Mouse(app, false, (float)x, (float)y))
        { app->CoopSmokeFail("stock chooser warning capture/Yes SDL confirmation failed"); return true; }
        const SexyString flyerKey = _S("[SEED_CHOOSER_FLYER_WARNING]");
        const bool flyer = warning->mDialogLines == flyerKey || warning->mDialogLines == TodStringTranslate(flyerKey);
        NetTrace("SMOKE_MYSTERY_WARNING kind=%s sun=0 sdlYes=1 capture=1", flyer ? "flyer" : "stock");
        s.mWarningClickQueued = true;
        return true;
    }
    s.mWarningHash = 0; s.mWarningClickQueued = false;
    if (s.mDiagnosticStage != s.mStage || GetTickCount() - s.mLastDiagnosticTick >= 1000U)
    {
        s.mDiagnosticStage = s.mStage; s.mLastDiagnosticTick = GetTickCount();
        NetTrace("SMOKE_MYSTERY_WAIT role=%s stage=%d scene=%d mask=%u local=%d real=%d quota=%d flight=%d pending=%d manualPicked=%d manual=%d:%d,%d:%d randomHover=%d mouse=(%d,%d)",
            host ? "host" : "client", s.mStage, (int)app->mGameScene, net->mChooserMysteryMask,
            chooser ? chooser->CoopGetLocalQuotaPickCount() : -1, chooser ? chooser->mSeedsInBank : -1,
            chooser ? chooser->mCoopPicksPerPlayer : -1, chooser ? chooser->mSeedsInFlight : -1,
            chooser && chooser->mCoopMysteryRequestPending ? 1 : 0, s.mManualPicked,
            s.mManual[0], chooser && s.mManual[0] >= 0 ? (int)chooser->mChosenSeeds[s.mManual[0]].mSeedState : -1,
            s.mManual[1], chooser && s.mManual[1] >= 0 ? (int)chooser->mChosenSeeds[s.mManual[1]].mSeedState : -1,
            chooser && chooser->mRandomButton->mIsOver ? 1 : 0,
            app->mWidgetManager->mLastMouseX, app->mWidgetManager->mLastMouseY);
    }
    if (RngRecovery(app, host)) return true;
    if (s.mStage <= 5)
    {
        if (!chooser || !chooser->mCoopChooserActive || !chooser->mMouseVisible ||
            !app->mBoard || !app->mBoard->mCutScene->mSeedChoosing) return true;
        if (chooser->mSeedsInFlight || chooser->mCoopMysteryRequestPending) return true;
        if (!s.mFixedHash)
        {
            if (chooser->CoopCountGlobalFixedPicks() != 3 || net->mChooserPickQuota[0] != 5 || net->mChooserPickQuota[1] != 5)
            { app->CoopSmokeFail("mystery fixture requires native fixed3 and fair5+5"); return true; }
            if (chooser->mChosenSeeds[SEED_SUNFLOWER].mCrazyDavePicked || chooser->mChosenSeeds[SEED_SUNSHROOM].mCrazyDavePicked ||
                chooser->mChosenSeeds[SEED_TWINSUNFLOWER].mCrazyDavePicked)
            { app->CoopSmokeFail("mystery no-sun-warning fixture unexpectedly contains a fixed sun producer"); return true; }
            s.mFixedHash = FixedHash(chooser);
            int n = 0;
            for (int i = 0; i < NUM_SEEDS_IN_CHOOSER && n < 4; ++i)
                if (i != SEED_IMITATER && !chooser->mChosenSeeds[i].mCrazyDavePicked && app->SeedTypeAvailable((SeedType)i) &&
                    !chooser->SeedNotAllowedToPick((SeedType)i) && !chooser->SeedNotAllowedDuringTrial((SeedType)i))
                {
                    if (i != SEED_CHERRYBOMB && i != SEED_SUNFLOWER && i != SEED_SUNSHROOM &&
                        i != SEED_TWINSUNFLOWER && s.mManual[1] < 0)
                    { if (s.mManual[0] < 0) s.mManual[0] = i; else s.mManual[1] = i; }
                    ++n;
                }
            if (n != 4 || s.mManual[1] < 0 || app->mBoard->GetMysteryPoolCapacity() != 5)
            { app->CoopSmokeFail("mystery bounded eligible test pool is unavailable"); return true; }
            NetTrace("SMOKE_MYSTERY_WARNING_FIXTURE role=%s fixedSun=0 manualSun=0", host ? "host" : "client");
        }
        if (FixedHash(chooser) != s.mFixedHash)
        { app->CoopSmokeFail("mystery toggle changed mandatory fixed cards"); return true; }
        if (s.mStage == 1)
        { if (Button(app, chooser->mRandomButton)) ++s.mStage; }
        else if (s.mStage == 2)
        {
            if (net->mChooserMysteryMask == 3 &&
                (chooser->mSeedsInBank != 3 || chooser->CoopGetLocalQuotaPickCount() != 5))
            { app->CoopSmokeFail("mystery reservation escaped into chosen real seeds"); return true; }
            const bool recorded = net->mChooserMysteryMask == 3 && Record(app, "both_secret");
            // Let the host observe both frames before either public mode is cleared.
            if ((host ? recorded : s.mCaptured == s.mStage && !net->IsChooserMystery(0)) &&
                Button(app, chooser->mRandomButton)) ++s.mStage;
        }
        else if (s.mStage == 3 && net->mChooserMysteryMask == 0)
        {
            if (chooser->mSeedsInBank != 3 || chooser->CoopGetLocalQuotaPickCount() != (host ? 3 : 0) || net->IsMyReady())
            { app->CoopSmokeFail("mystery cancel did not restore an empty manual selection"); return true; }
            if (Record(app, "cancelled"))
            { ++s.mStage; if (host) net->SendChat("MY_GO_MIX"); }
        }
        else if (s.mStage == 4)
        {
            if (!host)
            { if (HasCommand(app, "MY_GO_MIX") && Button(app, chooser->mRandomButton)) ++s.mStage; }
            else if (s.mManualPicked < 2)
            {
                if (net->IsChooserMystery(0))
                { app->CoopSmokeFail("mystery fixture re-enabled host random mode during manual input"); return true; }
                ChosenSeed& seed = chooser->mChosenSeeds[s.mManual[s.mManualPicked]];
                if (seed.mSeedState == SEED_IN_BANK) ++s.mManualPicked;
                else if (seed.mSeedState == SEED_IN_CHOOSER && net->IsChooserMystery(1))
                {
                    if (chooser->CoopIsPickedByOthers(seed.mSeedType))
                    { app->CoopSmokeFail("manual type was blocked by an unknown random reservation"); return true; }
                    const int x = chooser->mX + seed.mX + 25, y = chooser->mY + seed.mY + 35;
                    // GameButton hover is refreshed by the chooser's next Update.
                    // A button event alone can still target the old Mystery hover.
                    if (app->mWidgetManager->mLastMouseX != x || app->mWidgetManager->mLastMouseY != y ||
                        chooser->mRandomButton->mIsOver)
                    { Move(app, (float)x, (float)y); return true; }
                    if (!Mouse(app, true, (float)x, (float)y) || !Mouse(app, false, (float)x, (float)y))
                        app->CoopSmokeFail("mystery manual SDL pick failed");
                }
            }
            else ++s.mStage;
        }
        else if (s.mStage == 5)
        {
            if (!MalformedStartBeforeReady(app, host)) return true;
            if (net->mChooserMysteryMask != 2 || !chooser->CoopAllPicksComplete()) return true;
            if (rngFault && !s.mFaultBaseline)
            { s.mFaultBaseline = true; s.mFaultEpoch = net->GetInputEpoch(); }
            const char* tag = rngFault && s.mFaultRecovered ? "mixed_ready_after_rng" : "mixed_ready";
            if (Record(app, tag) && Button(app, chooser->mStartButton)) ++s.mStage;
        }
        return true;
    }
    Board* board = app->mBoard;
    if (!board || !board->mSeedBank) return true;
    if (s.mStage == 6 && app->mGameScene == SCENE_LEVEL_INTRO)
    {
        if (!chooser || chooser->mMouseVisible) return true;
        if (board->mSeedBank->mNumPackets != 10) { app->CoopSmokeFail("mystery commit lost bank slots"); return true; }
        if (!s.mIntroSeen)
        {
            for (int i = 0; i < 10; ++i)
            {
                const SeedPacket& packet = board->mSeedBank->mSeedPackets[i];
                if ((i < 5) != (packet.mNetOwnerSlot == net->GetMySlot()))
                { app->CoopSmokeFail("mystery committed bank lost viewer-relative own prefix"); return true; }
                if (packet.mNetOwnerSlot == 1)
                {
                    if (!packet.IsMysteryPending() || !packet.mNetId || packet.mImitaterType != SEED_NONE ||
                        packet.mRandomDiscountLevel != s.mDifficulty || packet.mActive)
                    { app->CoopSmokeFail("mystery leaked a real seed or invalid placeholder before PLAYING"); return true; }
                    s.mPending.push_back(packet.mNetId);
                }
            }
            if (s.mPending.size() != 5) { app->CoopSmokeFail("mystery intro reservation count differs from quota"); return true; }
            std::sort(s.mPending.begin(), s.mPending.end());
            if (!app->CoopSmokeCaptureFrame("mystery_intro_secret")) { app->CoopSmokeFail("mystery intro capture failed"); return true; }
            s.mIntroSeen = true;
            NetTrace("SMOKE_MYSTERY_SECRET role=%s pending=5 realTypes=0 fixed=3 ids=1 capture=1", host ? "host" : "client");
        }
        if (!NetSmokeMysteryObserverCheckpoint(app, "intro_pending")) return true;
        return true;
    }
    if (app->mGameScene != SCENE_PLAYING) return true;
    if (!s.mIntroSeen) { app->CoopSmokeFail("mystery intro secrecy was not observed"); return true; }
    if (s.mStage == 6)
    {
        int overlap = 0, imitators = 0;
        for (uint16_t id : s.mPending)
        {
            SeedPacket* packet = FindPacket(app, id);
            if (!packet || packet->IsMysteryPending() || packet->mPacketType == SEED_NONE || packet->mNetOwnerSlot != 1 ||
                packet->mRandomDiscountLevel != s.mDifficulty)
            { app->CoopSmokeFail("mystery reveal changed identity or failed to reveal the pending card"); return true; }
            if (packet->mPacketType == s.mManual[0]) { ++overlap; s.mSpendId = id; }
            if (packet->mPacketType == SEED_IMITATER)
            {
                ++imitators; bool found = false;
                for (uint16_t other : s.mPending)
                { SeedPacket* target = FindPacket(app, other); found |= target && target->mPacketType == packet->mImitaterType; }
                if (!found) { app->CoopSmokeFail("mystery Imitator did not copy its own rolled deck"); return true; }
            }
        }
        if (overlap != 1 || imitators != 1 || !s.mSpendId)
        { app->CoopSmokeFail("mystery bounded pool did not prove manual overlap and one Imitator"); return true; }
        if (Record(app, "revealed"))
        { NetTrace("SMOKE_MYSTERY_REVEAL role=%s stableIDs=5 overlap=1 imitator=1 difficulty=%d", host ? "host" : "client", s.mDifficulty); ++s.mStage; }
        return true;
    }
    SeedPacket* packet = FindPacket(app, s.mSpendId);
    if (!packet) { app->CoopSmokeFail("mystery purchase packet disappeared"); return true; }
    if (s.mStage == 7)
    {
        if (host && !s.mMoneySet) { board->mSunMoney = 1000; s.mMoneySet = true; }
        if (board->mSunMoney != 1000 || !packet->CanPickUp()) return true;
        s.mCost = board->GetSeedPacketCost(*packet); s.mSun = board->mSunMoney;
        if (s.mCost <= 0 || s.mCost >= Plant::GetCost(packet->mPacketType, packet->mImitaterType))
        { app->CoopSmokeFail("mystery packet does not display a discounted cost"); return true; }
        if (Record(app, "discount"))
        { ++s.mStage; if (host) net->SendChat("MY_GO_BUY"); }
        return true;
    }
    if (s.mStage == 8)
    {
        if (!host && !s.mPicked && HasCommand(app, "MY_GO_BUY"))
        {
            const float x = (float)(board->mSeedBank->mX + packet->mX + packet->mOffsetX + 25);
            const float y = (float)(board->mSeedBank->mY + packet->mY + 35);
            if (!Mouse(app, true, x, y) || !Mouse(app, false, x, y)) app->CoopSmokeFail("mystery real pickup SDL queue failed");
            s.mPicked = true;
        }
        if (!host && s.mPicked && !s.mPlanted && board->mCursorObject &&
            board->mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK)
        {
            for (int row = 0; row < MAX_GRID_SIZE_Y && !s.mPlanted; ++row)
                for (int col = 0; col < 9 && !s.mPlanted; ++col)
                    if (board->CanPlantAt(col, row, packet->mPacketType) == PLANTING_OK)
                    {
                        const float x = (float)(board->GridToPixelX(col, row) + 40), y = (float)(board->GridToPixelY(col, row) + 40);
                        if (!Mouse(app, true, x, y) || !Mouse(app, false, x, y)) app->CoopSmokeFail("mystery planting SDL queue failed");
                        s.mCol = col; s.mRow = row; s.mPlanted = true;
                    }
        }
        bool planted = false; Plant* plant = nullptr;
        while (board->mPlants.IterateNext(plant))
            planted |= !plant->mDead && plant->mSeedType == packet->mPacketType;
        if (!planted || board->mSunMoney != s.mSun - s.mCost || (host && !packet->mTimesUsed)) return true;
        if (!s.mCooldownProved)
        {
            const int expected = Plant::GetRefreshTime(packet->mPacketType, packet->mImitaterType);
            const uint32_t now = GetTickCount();
            if (!s.mCooldownSyncTick) s.mCooldownSyncTick = now;
            if (expected <= 0 || packet->mRefreshTime != expected || !packet->mRefreshing || packet->mActive)
            {
                if (now - s.mCooldownSyncTick >= 2000U)
                    app->CoopSmokeFail("mystery purchase did not synchronize the positive stock refresh time and flags");
                return true;
            }
            if (s.mCooldownFirst < 0)
            { s.mCooldownFirst = packet->mRefreshCounter; s.mCooldownTick = now; return true; }
            if (now - s.mCooldownTick < 150U) return true;
            if (packet->mRefreshCounter <= s.mCooldownFirst || packet->mRefreshCounter >= expected)
            {
                if (now - s.mCooldownTick >= 2000U)
                    app->CoopSmokeFail("mystery stock cooldown did not advance after planting");
                return true;
            }
            NetTrace("SMOKE_MYSTERY_COOLDOWN role=%s time=%d expected=%d first=%d later=%d progress=1",
                host ? "host" : "client", packet->mRefreshTime, expected, s.mCooldownFirst, packet->mRefreshCounter);
            s.mCooldownProved = true;
        }
        if (Record(app, "spent"))
        {
            NetTrace("SMOKE_MYSTERY_PURCHASE role=%s packet=%u displayed=%d spent=%d cooldown=%d origin=%u sdl=1",
                host ? "host" : "client", s.mSpendId, s.mCost, s.mSun - board->mSunMoney, packet->mRefreshTime, packet->mRandomDiscountLevel);
            ++s.mStage;
        }
    }
    if (s.mStage == 9 && (host || s.mRelease.empty()))
    {
        if (!NetSmokeMysteryObserverCheckpoint(app, "spent")) return true;
        if (!NetSmokeMysteryObserverCheckpoint(app, "final")) return true;
        app->CoopSmokeWriteResult(true, nullptr);
    }
    return true;
}
}
