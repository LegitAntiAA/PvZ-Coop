#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Cutscene.h"
#include "../SeedPacket.h"
#include "../ToolTipWidget.h"
#include "../System/PlayerInfo.h"
#include "../Widget/GameSelector.h"
#include "../Widget/LawnDialog.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <cstdlib>

namespace
{
struct GameOverSmokeState
{
    bool mStarted = false, mPlayingSeen = false, mLossIssued = false, mLossCaptured = false;
    bool mActionAnnounced = false, mActionClicked = false, mFinalCaptured = false;
    bool mClientKeyQueued = false, mClientKeyProved = false;
    bool mInitialTeamCaptured = false, mRetryTeamCaptured = false;
    bool mInitialPicksCaptured = false, mRetryPicksCaptured = false;
    bool mInitialIntroCaptured = false, mRetryIntroCaptured = false;
    uint32_t mLastChat = 0, mActionTick = 0, mLastDiagnostic = 0, mLastIntroDiagnostic = 0, mDialogObserved = 0;
    uint32_t mClientKeyTick = 0;
    uint32_t mTeamObserved = 0;
    uint32_t mPrivacyObserved = 0, mClosingObserved = 0;
    int mPrivacyPhase = 0;
    uint32_t mInitialEpoch = 0;
    GameOverDialog* mClientDialogAtKey = nullptr;
};
GameOverSmokeState s;

bool Chat(LawnApp* app, int slot, const std::string& text)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

void Signal(LawnApp* app, const std::string& text)
{
    const uint32_t now = GetTickCount();
    if (!Chat(app, app->mNetManager->GetMySlot(), text) && (!s.mLastChat || now - s.mLastChat >= 800U))
    { s.mLastChat = now; app->mNetManager->SendChat(text); }
}

bool PressKey(LawnApp* app, bool retry)
{
    for (bool down : {true, false})
    {
        SDL_Event e{}; e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = SDL_GetWindowID(app->mSDLWindow); e.key.down = down;
        e.key.key = retry ? SDLK_RETURN : SDLK_ESCAPE;
        e.key.scancode = retry ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
        if (!SDL_PushEvent(&e)) return false;
    }
    return true;
}

bool Click(LawnApp* app, LawnDialog* dialog, ButtonWidget* button)
{
    if (!button || !button->mVisible || button->mDisabled) return false;
    const float x = (float)(dialog->mX + button->mX + button->mWidth / 2);
    const float y = (float)(dialog->mY + button->mY + button->mHeight / 2);
    if (app->mWidgetManager->mLastMouseX != (int)x || app->mWidgetManager->mLastMouseY != (int)y)
    {
        SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
        SDL_PushEvent(&motion); return false;
    }
    if (app->mWidgetManager->GetWidgetAt((int)x, (int)y, nullptr, nullptr) != button) return false;
    for (bool down : {true, false})
    {
        SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
        e.button.down = down; e.button.clicks = 1; e.button.x = x; e.button.y = y;
        if (!SDL_PushEvent(&e)) return false;
    }
    return true;
}

ButtonWidget* TeamButton(LawnDialog* dialog, int id)
{
    for (Widget* child : dialog->mWidgets)
        if (ButtonWidget* button = dynamic_cast<ButtonWidget*>(child); button && button->mId == id) return button;
    return nullptr;
}

bool VerifyPvpPrivacy()
{
    const char* enabled = getenv("PVZ_COOP_SMOKE_PVP_PICK_PRIVACY");
    return enabled && !strcmp(enabled, "1");
}

bool PvpPrivacyBeforeReady(LawnApp* app, bool host)
{
    if (!VerifyPvpPrivacy()) return true;
    auto* net = app->mNetManager; auto* chooser = app->mSeedChooserScreen;
    bool& captured = s.mActionClicked ? s.mRetryPicksCaptured : s.mInitialPicksCaptured;
    const std::string round = s.mActionClicked ? "retry" : "initial";
    const std::string proof = "GO_WAITING_" + round;
    if (!captured)
    {
        if (!s.mPrivacyObserved) s.mPrivacyObserved = GetTickCount();
        if (!host)
        {
            if (!chooser->CoopIsPvpZombieWaiting() || chooser->mCoopPicksPerPlayer ||
                chooser->CoopGetLocalQuotaPickCount() || chooser->mCoopRemotePickCount[0] != 10 ||
                chooser->mToolTip->mVisible || !chooser->mRandomButton->mBtnNoDraw ||
                !chooser->mImitaterButton->mDisabled || chooser->mScrollbar->mVisible || chooser->mStartButton->mDisabled)
            { app->CoopSmokeFail("PvP zombie waiting view leaked a control, tooltip, quota or enemy pick"); return false; }
            if (s.mPrivacyPhase == 0)
            {
                const int type = chooser->mCoopRemotePicks[0][0];
                if (type < 0 || type >= NUM_SEEDS_IN_CHOOSER)
                { app->CoopSmokeFail("PvP privacy fixture requires real manual plant picks"); return false; }
                int x, y; chooser->GetSeedPositionInChooser(type, x, y);
                x += chooser->mX + 25; y += chooser->mY + 35;
                if (chooser->SeedHitTest(x, y) != SEED_NONE)
                { app->CoopSmokeFail("PvP zombie waiting grid still exposes a plant hit"); return false; }
                SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
                if (app->mWidgetManager->mLastMouseX != x || app->mWidgetManager->mLastMouseY != y || chooser->mStartButton->IsMouseOver())
                { e.motion.x = (float)x; e.motion.y = (float)y; SDL_PushEvent(&e); return false; }
                for (bool down : {true, false})
                {
                    e = {}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                    e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
                    e.button.down = down; e.button.clicks = 1; e.button.x = (float)x; e.button.y = (float)y;
                    if (!SDL_PushEvent(&e)) app->CoopSmokeFail("PvP privacy hidden-grid SDL input failed");
                }
                s.mPrivacyPhase = 1; s.mPrivacyObserved = GetTickCount(); return false;
            }
            if (GetTickCount() - s.mPrivacyObserved < 150U) return false;
            if (s.mPrivacyPhase == 1)
            {
                int x, y; chooser->GetSeedPositionInBank(0, x, y);
                SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
                e.motion.x = (float)(chooser->mX + x + 25); e.motion.y = (float)(chooser->mY + y + 35);
                SDL_PushEvent(&e); s.mPrivacyPhase = 2; s.mPrivacyObserved = GetTickCount(); return false;
            }
        }
        if (GetTickCount() - s.mPrivacyObserved < 150U) return false;
        if (!app->CoopSmokeCaptureFrame((std::string(host ? "gameover_pvp_plants_" : "gameover_pvp_waiting_") + round).c_str()))
        { app->CoopSmokeFail("PvP pre-ready privacy capture failed"); return false; }
        captured = true;
        LawnNet::NetTrace("SMOKE_PVP_PICK_PRIVACY role=%s round=%s manual=10 waiting=%d tooltip=0 quota=%d readyEnabled=1 size=%dx%d sdlHiddenClick=%d capture=1",
            host ? "host" : "client", round.c_str(), host ? 0 : 1, chooser->mCoopPicksPerPlayer,
            app->mWidth, app->mHeight, host ? 0 : 1);
    }
    if (!host) Signal(app, proof);
    return Chat(app, 1, proof);
}

void PvpClosingPrivacy(LawnApp* app, bool host)
{
    if (!VerifyPvpPrivacy() || host || app->mGameScene != SCENE_LEVEL_INTRO || app->mNetManager->mChooserOpen ||
        !(s.mActionClicked ? s.mRetryPicksCaptured : s.mInitialPicksCaptured)) return;
    auto* chooser = app->mSeedChooserScreen;
    if (!chooser || chooser->mMouseVisible) return;
    bool& captured = s.mActionClicked ? s.mRetryIntroCaptured : s.mInitialIntroCaptured;
    if (captured) return;
    if (!chooser->CoopIsPvpZombieWaiting() || chooser->mToolTip->mVisible)
    { app->CoopSmokeFail("PvP zombie waiting privacy ended before gameplay"); return; }
    if (!s.mClosingObserved) s.mClosingObserved = GetTickCount();
    if (GetTickCount() - s.mClosingObserved < 150U) return;
    if (!app->CoopSmokeCaptureFrame(s.mActionClicked ? "gameover_pvp_waiting_intro_retry" : "gameover_pvp_waiting_intro_initial"))
    { app->CoopSmokeFail("PvP closing intro privacy capture failed"); return; }
    captured = true; s.mClosingObserved = 0;
    LawnNet::NetTrace("SMOKE_PVP_PICK_PRIVACY_INTRO role=client round=%s waiting=1 mouse=0 tooltip=0 size=%dx%d capture=1",
        s.mActionClicked ? "retry" : "initial", app->mWidth, app->mHeight);
}

bool PvpIntro(LawnApp* app, bool host)
{
    LawnNet::NetManager* net = app->mNetManager;
    if (GetTickCount() - s.mLastIntroDiagnostic >= 1000U)
    {
        s.mLastIntroDiagnostic = GetTickCount();
        SeedChooserScreen* chooser = app->mSeedChooserScreen;
        LawnNet::NetTrace("SMOKE_GAMEOVER_PVP_INTRO role=%s teams=%d board=%d gameover=%d chooser=%d quota=%d local=%d flight=%d ready=%d",
            host ? "host" : "client", app->NetIsChoosingPvpTeams() ? 1 : 0, app->mBoard ? 1 : 0,
            app->GetDialog(DIALOG_GAME_OVER) ? 1 : 0, chooser ? 1 : 0,
            chooser ? chooser->mCoopPicksPerPlayer : -1, chooser ? chooser->CoopGetLocalQuotaPickCount() : -1,
            chooser ? chooser->mSeedsInFlight : -1, net->IsMyReady() ? 1 : 0);
    }
    if (app->NetIsChoosingPvpTeams())
    {
        s.mPrivacyObserved = 0; s.mPrivacyPhase = 0; s.mClosingObserved = 0;
        if (app->mBoard || app->GetDialog(DIALOG_GAME_OVER) || app->mGameScene != SCENE_MENU)
        { app->CoopSmokeFail("PvP rematch team selection retained the previous board or GameOver"); return true; }
        LawnDialog* teams = dynamic_cast<LawnDialog*>(app->GetDialog(DIALOG_COOP_TEAMS));
        if (!teams) return true;
        bool& captured = s.mActionClicked ? s.mRetryTeamCaptured : s.mInitialTeamCaptured;
        if (!captured)
        {
            if (!s.mTeamObserved) s.mTeamObserved = GetTickCount();
            if (GetTickCount() - s.mTeamObserved < 150U) return true;
            if (!app->CoopSmokeCaptureFrame(s.mActionClicked ? "gameover_pvp_teams_retry" : "gameover_pvp_teams_initial"))
            { app->CoopSmokeFail("PvP GameOver team selection capture failed"); return true; }
            captured = true;
            LawnNet::NetTrace("SMOKE_GAMEOVER_PVP_TEAMS role=%s round=%s board=0 gameover=0 scene=menu capture=1",
                host ? "host" : "client", s.mActionClicked ? "retry" : "initial");
        }
        const int desired = host ? LawnNet::NETTEAM_PLANTS : LawnNet::NETTEAM_ZOMBIES;
        if (net->GetMyTeam() != desired) Click(app, teams, TeamButton(teams, host ? 101 : 102));
        else if (!net->IsMyReady()) Click(app, teams, TeamButton(teams, 103));
        return true;
    }
    s.mTeamObserved = 0;
    SeedChooserScreen* chooser = app->mSeedChooserScreen;
    if (!chooser || !chooser->mCoopChooserActive || !chooser->mMouseVisible ||
        !app->mBoard || !app->mBoard->mCutScene->mSeedChoosing || app->mGameScene != SCENE_LEVEL_INTRO) return false;
    if (chooser->mSeedsInFlight || app->GetDialogCount()) return true;
    GameButton* ready = chooser->mStartButton;
    if (chooser->CoopGetLocalQuotaPickCount() < chooser->mCoopPicksPerPlayer)
    {
        for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; ++i)
        {
            ChosenSeed& seed = chooser->mChosenSeeds[i];
            if (seed.mSeedState != SEED_IN_CHOOSER || !app->SeedTypeAvailable(seed.mSeedType) ||
                chooser->SeedNotAllowedToPick(seed.mSeedType) || chooser->SeedNotAllowedDuringTrial(seed.mSeedType) ||
                chooser->CoopIsPickedByOthers(seed.mSeedType)) continue;
            const int x = chooser->mX + seed.mX + 25, y = chooser->mY + seed.mY + 35;
            SDL_Event e{}; e.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
            if (app->mWidgetManager->mLastMouseX != x || app->mWidgetManager->mLastMouseY != y ||
                ready->mIsOver || chooser->mRandomButton->mIsOver)
            {
                e.type = SDL_EVENT_MOUSE_MOTION; e.motion.x = (float)x; e.motion.y = (float)y; SDL_PushEvent(&e);
                return true;
            }
            for (bool down : {true, false})
            {
                e = {}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
                e.button.down = down; e.button.clicks = 1; e.button.x = (float)x; e.button.y = (float)y;
                if (!SDL_PushEvent(&e)) app->CoopSmokeFail("PvP GameOver chooser SDL seed input failed");
            }
            return true;
        }
    }
    else if (chooser->CoopAllPicksComplete() && !net->IsMyReady() && !ready->mDisabled)
    {
        if (!PvpPrivacyBeforeReady(app, host)) return true;
        const int x = ready->mX + ready->mWidth / 2, y = ready->mY + ready->mHeight / 2;
        if (!ready->IsMouseOver())
        {
            SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
            e.motion.x = (float)x; e.motion.y = (float)y; SDL_PushEvent(&e); return true;
        }
        for (bool down : {true, false})
        {
            SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
            e.button.down = down; e.button.clicks = 1; e.button.x = (float)x; e.button.y = (float)y;
            if (!SDL_PushEvent(&e)) app->CoopSmokeFail("PvP GameOver chooser SDL Ready input failed");
        }
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateGameOver(LawnApp* app)
{
    const char* requested = getenv("PVZ_COOP_SMOKE_GAME_OVER");
    if (!app->CoopSmokeActive() || !requested || !*requested) return false;
    const std::string action(requested);
    if (action != "retry" && action != "close")
    { app->CoopSmokeFail("unknown GameOver fixture action"); return true; }
    const char* input = getenv("PVZ_COOP_SMOKE_GAME_OVER_INPUT");
    if (input && *input && strcmp(input, "mouse") && strcmp(input, "keyboard"))
    { app->CoopSmokeFail("unknown GameOver fixture input"); return true; }
    const bool keyboard = input && !strcmp(input, "keyboard");
    const char* mode = getenv("PVZ_COOP_SMOKE_GAME_OVER_MODE");
    const bool pvp = mode && !strcmp(mode, "PvP");
    if (pvp && action != "retry") { app->CoopSmokeFail("PvP GameOver fixture supports Retry only"); return true; }
    if (app->mCoopSmokeResultWritten) return true;
    if (app->mCoopSmokeManifest.mScenario != (pvp ? NETSMOKE_SCENARIO_PVP_1V1 : NETSMOKE_SCENARIO_ADVENTURE_1) ||
        app->mCoopSmokeManifest.mSkipIntro || (app->mWidth != 800 && !(pvp && app->mWidth == 1066)) || app->mHeight != 600)
    { app->CoopSmokeFail("GameOver fixture requires matching isolated full-intro 800/1066x600 peers"); return true; }
    NetManager* net = app->mNetManager;
    if (!net || !app->mPlayerInfo || net->GetPlayerCount() != 2) return true;
    const bool host = app->mCoopSmokeManifest.IsHost();
    app->mCoopSmokePeerReady = true;
    if (!s.mStarted)
    {
        if (host)
        {
            if (!app->mGameSelector || app->mGameSelector->mSelectorState != SELECTOR_IDLE || app->mBoard) return true;
            app->mPlayerInfo->SetLevel(1); app->mPlayerInfo->mFinishedAdventure = pvp ? 1 : 0;
            app->NetBroadcastProfile(); app->mCoopSmokeGameStarted = true;
            app->KillGameSelector(); app->PreNewGame(pvp ? GAMEMODE_COOP_PVP : GAMEMODE_ADVENTURE, false, 1);
        }
        s.mStarted = true;
    }
    const std::string actionMessage = "GO_ACTION_" + action;
    if (!host && s.mLossCaptured && !s.mActionClicked && Chat(app, 0, actionMessage))
    { s.mActionClicked = true; s.mActionTick = GetTickCount(); }
    if (pvp && PvpIntro(app, host)) return true;
    if (pvp) PvpClosingPrivacy(app, host);
    GameOverDialog* dialog = dynamic_cast<GameOverDialog*>(app->GetDialog(DIALOG_GAME_OVER));
    if (GetTickCount() - s.mLastDiagnostic >= 1000U)
    {
        s.mLastDiagnostic = GetTickCount();
        NetTrace("SMOKE_GAMEOVER_WAIT role=%s action=%s scene=%d dialog=%d board=%d result=%d clicked=%d epoch=%u input=%s focus=%d",
            host ? "host" : "client", action.c_str(), (int)app->mGameScene, dialog ? 1 : 0,
            app->mBoard ? 1 : 0, (int)app->mBoardResult, s.mActionClicked ? 1 : 0, net->GetInputEpoch(),
            keyboard ? "keyboard" : "mouse", dialog && app->mWidgetManager->mFocusWidget == dialog ? 1 : 0);
    }
    if (!s.mPlayingSeen && app->mBoard && app->mGameScene == SCENE_PLAYING)
    {
        if (pvp && VerifyPvpPrivacy() && !host && !s.mInitialIntroCaptured)
        { app->CoopSmokeFail("PvP first gameplay began before closing-intro privacy was observed"); return true; }
        s.mPlayingSeen = true; s.mInitialEpoch = net->GetInputEpoch();
    }
    if (!host && s.mPlayingSeen && !s.mLossCaptured) Signal(app, "GO_INITIAL_PLAYING");
    if (host && s.mPlayingSeen && !s.mLossIssued && Chat(app, 1, "GO_INITIAL_PLAYING"))
    {
        // Exercise the complete stock loss cutscene/dialog; no UI injection or timer skip.
        app->mBoard->ZombiesWon(nullptr); s.mLossIssued = true;
        NetTrace("SMOKE_GAMEOVER_LOSS action=%s source=stock_ZombiesWon", action.c_str());
    }
    if (dialog && !s.mLossCaptured)
    {
        if (!s.mDialogObserved) s.mDialogObserved = GetTickCount();
        if (GetTickCount() - s.mDialogObserved < 150U) return true;
        if (!app->CoopSmokeCaptureFrame("gameover_shown"))
        { app->CoopSmokeFail("GameOver shown renderer capture failed"); return true; }
        s.mLossCaptured = true;
        NetTrace("SMOKE_GAMEOVER_SHOWN role=%s action=%s dialog=1", host ? "host" : "client", action.c_str());
    }
    if (!host && keyboard && s.mLossCaptured && !s.mClientKeyProved)
    {
        if (!s.mClientKeyQueued)
        {
            if (app->mWidgetManager->mFocusWidget != dialog) return true;
            if (!PressKey(app, action == "retry"))
            { app->CoopSmokeFail("GameOver client key SDL queue failed"); return true; }
            s.mClientKeyQueued = true; s.mClientKeyTick = GetTickCount(); s.mClientDialogAtKey = dialog;
            return true;
        }
        if (GetTickCount() - s.mClientKeyTick < 150U) return true;
        if (!dialog || dialog != s.mClientDialogAtKey || app->mGameScene != SCENE_ZOMBIES_WON ||
            app->mBoardResult != BOARDRESULT_LOST || app->mNetChatTyping)
        { app->CoopSmokeFail("client GameOver keyboard input bypassed host authority"); return true; }
        if (!app->CoopSmokeCaptureFrame("gameover_client_key_ignored"))
        { app->CoopSmokeFail("GameOver client ignored-key capture failed"); return true; }
        s.mClientKeyProved = true;
        NetTrace("SMOKE_GAMEOVER_CLIENT_KEY key=%s ignored=1 dialog=1 capture=1", action == "retry" ? "Enter" : "Escape");
    }
    if (!host && s.mLossCaptured && !s.mActionClicked) Signal(app, "GO_DIALOG_READY");
    if (host && dialog && s.mLossCaptured && Chat(app, 1, "GO_DIALOG_READY") && !s.mActionClicked)
    {
        if (keyboard && app->mWidgetManager->mFocusWidget != dialog) return true;
        ButtonWidget* target = action == "retry" ? dialog->mLawnYesButton : dialog->mMenuButton;
        if (!target || !target->mVisible || target->mDisabled) return true;
        if (!s.mActionAnnounced)
        {
            Signal(app, actionMessage);
            if (!Chat(app, 0, actionMessage)) return true;
            s.mActionAnnounced = true;
        }
        if (keyboard ? PressKey(app, action == "retry") : Click(app, dialog, target))
        {
            s.mActionClicked = true; s.mActionTick = GetTickCount();
            NetTrace("SMOKE_GAMEOVER_ACTION role=host action=%s sdl=1 input=%s key=%s", action.c_str(),
                keyboard ? "keyboard" : "mouse", keyboard ? (action == "retry" ? "Enter" : "Escape") : "none");
        }
    }
    if (!s.mActionClicked) return true;
    if (dialog)
    {
        if (GetTickCount() - s.mActionTick >= 3000U)
        {
            app->CoopSmokeCaptureFrame("gameover_stale_after_action");
            NetTrace("SMOKE_GAMEOVER_STALE role=%s action=%s dialog=1 scene=%d", host ? "host" : "client", action.c_str(), (int)app->mGameScene);
            app->CoopSmokeFail("GameOver modal survived the host retry/menu action");
        }
        return true;
    }
    const bool clean = action == "retry" ?
        app->mBoard && app->mGameScene == SCENE_PLAYING && app->mBoardResult == BOARDRESULT_NONE && net->GetInputEpoch() > s.mInitialEpoch &&
            (!pvp || (s.mRetryTeamCaptured && app->mBoard->IsPvPMode() && (!VerifyPvpPrivacy() || host || s.mRetryIntroCaptured))) :
        !app->mBoard && app->mGameSelector && app->mGameScene == SCENE_MENU && app->mGameSelector->mSelectorState == SELECTOR_IDLE;
    if (!clean) return true;
    if (!s.mFinalCaptured)
    {
        if (!app->CoopSmokeCaptureFrame("gameover_clean_after_action"))
        { app->CoopSmokeFail("GameOver cleanup renderer capture failed"); return true; }
        s.mFinalCaptured = true;
        NetTrace("SMOKE_GAMEOVER_CLEAN role=%s action=%s dialog=0 board=%d scene=%d epoch=%u initialEpoch=%u capture=1",
            host ? "host" : "client", action.c_str(), app->mBoard ? 1 : 0, (int)app->mGameScene, net->GetInputEpoch(), s.mInitialEpoch);
    }
    if (!host) Signal(app, "GO_CLEAN_" + action);
    if (Chat(app, 1, "GO_CLEAN_" + action)) app->CoopSmokeWriteResult(true, nullptr);
    return true;
}
}
