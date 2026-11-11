#include "../../LawnApp.h"
#include "../../Resources.h"
#include "NetManager.h"
#include "NetOnline.h"
#include "NetLobbyPanel.h"
#include "../Board.h"
#include "../Widget/GameSelector.h"
#include "../Widget/ChallengeScreen.h"
#include "../Widget/NewOptionsDialog.h"
#include "../Widget/MoreSettingsDialog.h"
#include "../Widget/NetOnlineDialog.h"
#include "../Widget/GameButton.h"
#include "../System/PlayerInfo.h"
#include "../../SexyAppFramework/EditWidget.h"
#include "../../SexyAppFramework/ListWidget.h"
#include "../../SexyAppFramework/Slider.h"
#include "../../SexyAppFramework/Image.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include <cstdlib>
#include <cmath>

using namespace LawnNet;

namespace
{
struct NativeState
{
    int mStage = 0, mObserved = -1, mCaptured = -1;
    uint32_t mObservedTick = 0, mLastAckTick = 0, mActionTick = 0;
    int mBefore[3] = {}, mExpected[3] = {};
    float mPreviewTime = 0.0f;
    bool mProfilePrepared = false;
};
NativeState s;

Widget* Child(Dialog* d, int id)
{
    if (!d) return nullptr;
    for (Widget* w : d->mWidgets)
    {
        if (auto* b = dynamic_cast<ButtonWidget*>(w); b && b->mId == id) return w;
        if (auto* e = dynamic_cast<EditWidget*>(w); e && e->mId == id) return w;
        if (auto* l = dynamic_cast<ListWidget*>(w); l && l->mId == id) return w;
    }
    return nullptr;
}

bool Mouse(LawnApp* app, bool down, float x, float y)
{
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = SDL_GetWindowID(app->mSDLWindow);
    e.button.button = SDL_BUTTON_LEFT; e.button.down = down; e.button.clicks = 1;
    e.button.x = x; e.button.y = y;
    return SDL_PushEvent(&e);
}

bool Click(LawnApp* app, Widget* w, Dialog* d = nullptr)
{
    if (!w || !w->mVisible || w->mDisabled) return false;
    const float x = (d ? d->mX : 0) + w->mX + w->mWidth / 2.0f;
    const float y = (d ? d->mY : 0) + w->mY + w->mHeight / 2.0f;
    return Mouse(app, true, x, y) && Mouse(app, false, x, y);
}

bool Drag(LawnApp* app, Dialog* d, Slider* slider, double value)
{
    const int range = slider->mWidth - slider->mThumbImage->mWidth;
    const int offset = slider->mThumbImage->mWidth / 2;
    const float start = d->mX + slider->mX + (int)(slider->mVal * range) + offset;
    const float target = d->mX + slider->mX + (int)(value * range) + offset;
    const float y = d->mY + slider->mY + slider->mHeight / 2.0f;
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    e.motion.state = SDL_BUTTON_LMASK; e.motion.x = target; e.motion.y = y;
    return Mouse(app, true, start, y) && SDL_PushEvent(&e) && Mouse(app, false, target, y);
}

void Values(const NetConfig& c, int out[3])
{ out[0] = c.mHeldPlantTintAlpha; out[1] = c.mHotbarTintAlpha; out[2] = c.mHeldPlantOutlineAlpha; }

bool Matches(MoreSettingsDialog* d, const NetConfig& c)
{
    int v[3]; Values(c, v);
    Slider* sliders[3] = {d->mHeldPlantTintSlider, d->mHotbarTintSlider, d->mHeldPlantOutlineSlider};
    for (int i = 0; i < 3; ++i)
        if (v[i] != s.mExpected[i] || (int)(sliders[i]->mVal * 255.0 + 0.5) != v[i]) return false;
    return !d->mSavePending && !d->mSaveFailed;
}

bool Ack(LawnApp* app, const char* tag)
{
    const std::string text = std::string("NATIVE_ACK_") + tag;
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == 1 && line.mText == text) return true;
    if (app->mCoopSmokeManifest.IsClient())
    {
        const uint32_t now = GetTickCount();
        if (s.mLastAckTick && now - s.mLastAckTick < 800U) return false;
        s.mLastAckTick = now;
        app->mNetManager->SendChat(text);
        return true;
    }
    return false;
}

bool Record(LawnApp* app, const char* tag, bool paired)
{
    if (s.mObserved != s.mStage) { s.mObserved = s.mStage; s.mObservedTick = GetTickCount(); }
    if (GetTickCount() - s.mObservedTick < 200U) return false;
    if (s.mCaptured != s.mStage)
    {
        if (!app->CoopSmokeCaptureFrame((std::string("native_") + tag).c_str()))
        { app->CoopSmokeFail("native UI renderer capture failed"); return false; }
        s.mCaptured = s.mStage;
        NetTrace("SMOKE_NATIVE_UI role=%s step=%s capture=1", app->mCoopSmokeManifest.IsHost() ? "host" : "client", tag);
    }
    return !paired || Ack(app, tag);
}

bool Finish(LawnApp* app)
{
    app->CoopSmokeWriteResult(true, nullptr);
    return true;
}

bool Appearance(LawnApp* app, bool restart)
{
    const bool host = app->mCoopSmokeManifest.IsHost();
    NetManager* net = app->mNetManager;
    if (net->GetPlayerCount() != 2 || (host ? !net->IsHosting() : !net->IsConnectedToHost())) return true;
    NewOptionsDialog* options = (NewOptionsDialog*)app->GetDialog(DIALOG_NEWOPTIONS);
    MoreSettingsDialog* appearance = (MoreSettingsDialog*)app->GetDialog(DIALOG_MORESETTINGS);
    if (s.mStage == 0)
    {
        if (!app->mGameSelector || app->mGameSelector->mSelectorState != SELECTOR_IDLE) return true;
        if (!s.mProfilePrepared)
        {
            // Only the manifest's generated profile is used in this fixture.
            app->mPlayerInfo->SetLevel(1);
            app->mPlayerInfo->mFinishedAdventure = 0;
            app->mPlayerInfo->mHasUnlockedMinigames = 0;
            app->mPlayerInfo->mHasUnlockedPuzzleMode = 0;
            app->mPlayerInfo->mHasUnlockedSurvivalMode = 0;
            app->mGameSelector->SyncProfile(false);
            s.mProfilePrepared = true;
            NetTrace("SMOKE_NATIVE_PROFILE role=%s level=1 finished=0 unlockFlags=0", host ? "host" : "client");
        }
        if (restart)
        {
            const char* expected = getenv("PVZ_COOP_SMOKE_APPEARANCE_EXPECTED");
            if (!expected || sscanf_s(expected, "%d,%d,%d", &s.mExpected[0], &s.mExpected[1], &s.mExpected[2]) != 3)
            { app->CoopSmokeFail("native appearance restart lacks expected values"); return true; }
            app->DoNewOptions(true); s.mStage = 3; return true;
        }
        if (!app->NetUnlockAllMinigames() || app->mGameSelector->mMinigamesLocked ||
            app->mGameSelector->mMinigameButton->mDisabled || !app->mGameSelector->mMinigameButton->mMouseVisible)
        { app->CoopSmokeFail("config override did not unlock the real minigames menu control"); return true; }
        if (Record(app, "override_menu", true))
        {
            ++s.mStage;
            if (host && !Click(app, app->mGameSelector->mMinigameButton)) app->CoopSmokeFail("native minigame SDL click failed");
        }
    }
    else if (s.mStage == 1)
    {
        if (!app->mChallengeScreen) return true;
        if (Record(app, "override_minigames_open", true))
        {
            ++s.mStage;
            if (host && !Click(app, app->mChallengeScreen->mBackButton)) app->CoopSmokeFail("native minigame Back SDL click failed");
        }
    }
    else if (s.mStage == 2)
    {
        if (!app->mGameSelector || app->mChallengeScreen || app->mGameSelector->mSelectorState != SELECTOR_IDLE) return true;
        if (Record(app, "override_minigames_closed", true)) { ++s.mStage; app->DoNewOptions(true); }
    }
    else if (s.mStage == 3)
    {
        if (!options) return true;
        if (Record(app, "options", true))
        {
            ++s.mStage;
            if (!Click(app, options->mCoopAppearanceButton, options)) app->CoopSmokeFail("native CO-OP SDL click failed");
        }
    }
    else if (s.mStage == 4)
    {
        if (!appearance) return true;
        if (restart)
        {
            if (!Matches(appearance, net->mConfig)) { app->CoopSmokeFail("native settings did not persist across process restart"); return true; }
            if (Record(app, "appearance_restart", true))
            {
                NetTrace("SMOKE_NATIVE_RESTART role=%s held=%d hotbar=%d outline=%d ui=1",
                    host ? "host" : "client", s.mExpected[0], s.mExpected[1], s.mExpected[2]);
                return Finish(app);
            }
        }
        else if (Record(app, "appearance_before", true))
        {
            Values(net->mConfig, s.mBefore);
            s.mPreviewTime = appearance->mPreviewReanim->mAnimTime;
            ++s.mStage;
            if (!Drag(app, appearance, appearance->mHeldPlantTintSlider, 0.25) ||
                !Drag(app, appearance, appearance->mHotbarTintSlider, 0.5) ||
                !Drag(app, appearance, appearance->mHeldPlantOutlineSlider, 0.75))
                app->CoopSmokeFail("native slider drag SDL input failed");
        }
    }
    else if (s.mStage == 5)
    {
        if (!appearance || appearance->mSavePending || appearance->mSaveFailed) return true;
        Values(net->mConfig, s.mExpected);
        for (int i = 0; i < 3; ++i) if (s.mExpected[i] == s.mBefore[i]) return true;
        if (fabsf(appearance->mPreviewReanim->mAnimTime - s.mPreviewTime) < 0.001f) return true;
        if (Record(app, "appearance_changed", true))
        {
            NetTrace("SMOKE_NATIVE_SAVED role=%s held=%d hotbar=%d outline=%d sdl=1 saved=1 animated=1",
                host ? "host" : "client", s.mExpected[0], s.mExpected[1], s.mExpected[2]);
            ++s.mStage;
            if (!Click(app, appearance->mLawnYesButton, appearance)) app->CoopSmokeFail("native appearance close SDL failed");
        }
    }
    else if (s.mStage == 6)
    {
        if (appearance || !options) return true;
        if (Record(app, "appearance_closed", true)) { ++s.mStage; options->ButtonDepress(options->mCoopAppearanceButton->mId); }
    }
    else if (s.mStage == 7)
    {
        if (!appearance) return true;
        if (!Matches(appearance, net->mConfig)) { app->CoopSmokeFail("native settings changed after dialog reopen"); return true; }
        if (Record(app, "appearance_reopened", true))
        { ++s.mStage; if (!Click(app, appearance->mLawnYesButton, appearance)) app->CoopSmokeFail("native reopen close SDL failed"); }
    }
    else if (s.mStage == 8)
    {
        if (appearance || !options) return true;
        ++s.mStage; options->KeyDown(KEYCODE_ESCAPE);
    }
    else if (s.mStage == 9 && !appearance && !options)
    {
        if (Record(app, "appearance_complete", true))
        {
            ++s.mStage;
            if (host)
            {
                app->mPlayerInfo->SetLevel(45);
                app->mPlayerInfo->mFinishedAdventure = 1;
                app->mGameSelector->SyncProfile(false);
                app->NetBroadcastProfile();
            }
        }
    }
    else if (s.mStage == 10)
    {
        if (!app->mGameSelector || !app->mNetLobbyPanel ||
            app->mPlayerInfo->mLevel != 45 || app->mPlayerInfo->mFinishedAdventure != 1) return true;
        NewLawnButton* garden = app->mGameSelector->mZenGardenButton;
        const Rect panel = app->mNetLobbyPanel->GetPanelRect();
        if (!garden->mVisible || garden->mDisabled || !garden->mMouseVisible || panel.mWidth != 300)
        { app->CoopSmokeFail("native garden fixture requires the unlocked menu and expanded roster"); return true; }
        if (Record(app, "zen_lobby_expanded", true))
        {
            ++s.mStage;
            if (!Mouse(app, true, panel.mX + 268, panel.mY + 20) ||
                !Mouse(app, false, panel.mX + 268, panel.mY + 20))
                app->CoopSmokeFail("native lobby collapse SDL click failed");
        }
    }
    else if (s.mStage == 11)
    {
        if (!app->mGameSelector || !app->mNetLobbyPanel) return true;
        const Rect panel = app->mNetLobbyPanel->GetPanelRect();
        if (panel.mWidth != 180 || panel.mHeight != 36) return true;
        NewLawnButton* garden = app->mGameSelector->mZenGardenButton;
        const int x = garden->mX + garden->mWidth / 2, y = garden->mY + garden->mHeight / 2;
        if (app->mNetLobbyPanel->IsPointVisible(x, y) ||
            app->mWidgetManager->GetWidgetAt(x, y, nullptr, nullptr) != garden)
        { app->CoopSmokeFail("collapsed lobby still intercepts the native garden button"); return true; }
        if (Record(app, "zen_lobby_collapsed", true))
        {
            ++s.mStage;
            NetTrace("SMOKE_NATIVE_GARDEN role=%s accessible=1 hit=1 sdl=%d", host ? "host" : "client", host ? 1 : 0);
            if (host && !Click(app, garden)) app->CoopSmokeFail("native Zen Garden SDL click failed");
        }
    }
    else if (s.mStage == 12)
    {
        if (!app->mBoard || app->mGameMode != GAMEMODE_CHALLENGE_ZEN_GARDEN ||
            app->mGameScene != SCENE_PLAYING) return true;
        if (app->mGameSelector || app->mBoard->mBackground != BACKGROUND_GREENHOUSE ||
            app->mPlayerInfo->mLevel != 45 || app->mPlayerInfo->mFinishedAdventure != 1 ||
            (app->mNetLobbyPanel && app->mNetLobbyPanel->mVisible))
        { app->CoopSmokeFail("native garden SDL transition retained the menu or wrong profile/background"); return true; }
        if (Record(app, "zen_garden_open", true)) return Finish(app);
    }
    return true;
}

bool SelectRoom(LawnApp* app, NetOnlineDialog* browser, const std::string& name)
{
    ListWidget* list = (ListWidget*)Child(browser, 204);
    if (!list) return false;
    for (int i = 0; i < list->GetLineCount(); ++i)
        if (SexyStringToStringFast(list->GetStringAt(i)).find(name) == 0)
        {
            list->SetSelect(i);
            browser->ListClicked(204, i, 1);
            return true;
        }
    if (GetTickCount() - s.mActionTick >= 800U)
    {
        Widget* next = Child(browser, 106);
        if (next && !next->mDisabled) { Click(app, next, browser); s.mActionTick = GetTickCount(); }
    }
    return false;
}

bool Browser(LawnApp* app, bool probe)
{
    NetManager* net = app->mNetManager;
    NetOnline* online = net->GetOnline();
    NetOnlineDialog* browser = (NetOnlineDialog*)app->GetDialog(DIALOG_COOP_ONLINE);
    const char* id = getenv("PVZ_COOP_SMOKE_RUN_ID");
    const std::string name = std::string("[UI ") + (id ? std::string(id).substr(0, 8) : "missing") + "]";
    const bool host = app->mCoopSmokeManifest.IsHost();
    if (s.mStage == 0)
    {
        if (!app->mGameSelector || app->mGameSelector->mSelectorState != SELECTOR_IDLE) return true;
        app->NetShowOnlineBrowser(); s.mActionTick = GetTickCount(); ++s.mStage; return true;
    }
    if (host)
    {
        if (s.mStage == 1 && browser && online && online->IsConnected() && GetTickCount() - s.mActionTick >= 600U)
        {
            if (!Record(app, "browser_ready", false)) return true;
            ((EditWidget*)Child(browser, 203))->SetText(StringToSexyString(name));
            ((EditWidget*)Child(browser, 205))->SetText(_S("garden-test"));
            browser->ButtonDepress(104); // stock 4 -> 2 capacity control
            ++s.mStage;
            if (!Click(app, Child(browser, 103), browser)) app->CoopSmokeFail("native browser create SDL click failed");
        }
        else if (s.mStage == 2 && net->IsHosting() && online && online->IsHostingRoom() && !browser)
            if (Record(app, "browser_host_room", false)) return Finish(app);
        return true;
    }
    if (s.mStage == 1 && browser && online && online->IsConnected() && GetTickCount() - s.mActionTick >= 600U)
    {
        if (!SelectRoom(app, browser, name)) return true;
        if (!Record(app, probe ? "browser_full_selected" : "browser_populated", false)) return true;
        ((EditWidget*)Child(browser, 202))->SetText(probe ? _S("garden-test") : _S("incorrect"));
        ++s.mStage;
        if (probe)
        {
            // The ordinary callback exercises a stale-click/full-room race;
            // actual capacity rejection still comes from the remote Worker.
            browser->ButtonDepress(102);
            NetTrace("SMOKE_NATIVE_FULL_REQUEST source=stale_callback");
        }
        else if (!Click(app, Child(browser, 102), browser)) app->CoopSmokeFail("native browser join SDL click failed");
    }
    else if (s.mStage == 2 && browser && online)
    {
        const std::string code = online->GetLastErrorCode();
        if (code != (probe ? "room-full" : "wrong-password")) return true;
        if (s.mObserved != s.mStage) { s.mObserved = s.mStage; s.mObservedTick = GetTickCount(); }
        const SexyString detail = StringToSexyString(online->GetLastErrorMessage());
        ListWidget* list = dynamic_cast<ListWidget*>(Child(browser, 204));
        Graphics measure;
        const int required = TodDrawStringWrappedHelper(&measure, detail, Rect(0, 0, 421, 600),
            FONT_BRIANNETOD12, Color::Black, DS_ALIGN_LEFT, false);
        const int available = list ? 436 - list->mY - list->mHeight - 26 : 0;
        if (detail.empty() || !list || required > available)
        {
            if (GetTickCount() - s.mObservedTick < 2000U) return true;
            app->CoopSmokeFail("native online error cause is absent or does not fit its wrapped notice area"); return true;
        }
        if (!Record(app, probe ? "browser_room_full" : "browser_wrong_password", false)) return true;
        NetTrace("SMOKE_NATIVE_BROWSER_ERROR code=%s retry=1 wrapped=1 height=%d available=%d", code.c_str(), required, available);
        ++s.mStage; s.mActionTick = GetTickCount();
        if (probe && !Click(app, browser->mLawnYesButton, browser)) app->CoopSmokeFail("native browser cancel SDL failed");
    }
    else if (s.mStage == 3)
    {
        if (probe)
        {
            if (!browser && !net->IsActive())
                if (Record(app, "browser_full_closed", false)) return Finish(app);
        }
        else if (browser && GetTickCount() - s.mActionTick >= 1500U)
        {
            ((EditWidget*)Child(browser, 202))->SetText(_S("garden-test"));
            ++s.mStage;
            if (!Click(app, Child(browser, 102), browser)) app->CoopSmokeFail("native password retry SDL click failed");
        }
    }
    else if (s.mStage == 4 && net->IsConnectedToHost() && !browser)
        if (Record(app, "browser_joined", false)) return Finish(app);
    return true;
}
}

namespace LawnNet
{
bool NetSmokeNativeUiAllowsStart(LawnApp* app, uint8_t mode, int32_t level)
{
    const char* workflow = getenv("PVZ_COOP_SMOKE_NATIVE_UI");
    return app && app->CoopSmokeActive() && !app->mCoopSmokeResultWritten &&
        workflow && strcmp(workflow, "appearance") == 0 && s.mStage == 12 &&
        mode == (uint8_t)GAMEMODE_CHALLENGE_ZEN_GARDEN && level == 45 &&
        app->mPlayerInfo && app->mPlayerInfo->mLevel == 45 && app->mPlayerInfo->mFinishedAdventure == 1;
}

bool NetSmokeUpdateNativeUi(LawnApp* app)
{
    const char* mode = getenv("PVZ_COOP_SMOKE_NATIVE_UI");
    if (!app->CoopSmokeActive() || !mode || !*mode) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!app->mLoadingThreadCompleted || !app->mNetManager || !app->mPlayerInfo) return true;
    const bool garden = strcmp(mode, "appearance") == 0 && s.mStage == 12 &&
        app->mGameMode == GAMEMODE_CHALLENGE_ZEN_GARDEN;
    if (app->mWidth != 800 || app->mHeight != 600 || (app->mBoard && !garden))
    { app->CoopSmokeFail("native UI smoke requires isolated 800x600 menus or the bounded garden transition"); return true; }
    const std::string workflow(mode);
    if (workflow == "appearance" || workflow == "appearance-restart")
        return Appearance(app, workflow == "appearance-restart");
    if (workflow == "browser" || workflow == "browser-probe") return Browser(app, workflow == "browser-probe");
    app->CoopSmokeFail("unknown native UI smoke workflow");
    return true;
}
}
