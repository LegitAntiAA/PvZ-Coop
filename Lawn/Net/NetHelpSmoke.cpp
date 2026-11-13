#include "../../LawnApp.h"
#include "../../Resources.h"
#include "NetManager.h"
#include "NetLobbyPanel.h"
#include "../Board.h"
#include "../Widget/CoopHelpDialog.h"
#include "../Widget/GameSelector.h"
#include "../Widget/LawnDialog.h"
#include "../Widget/GameButton.h"
#include "../Widget/NetOnlineDialog.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/Font.h"
#include <cstdlib>

namespace
{
int stage = 0, observed = -1, captured = -1;
uint32_t since = 0;
uint32_t previewSince = 0;
uint32_t chatAttempt = 0;
uint32_t typingFinished = 0, finishedTicks = 0;
bool typingEarly = false;
std::shared_ptr<SDL_Surface> typingInitialFrame;
float animation = 0.0f;
bool collapsed = false, lan = false;

bool Has(LawnApp* app, int slot, const char* text)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

bool Send(LawnApp* app, const char* text)
{
    const int slot = app->mNetManager->GetMySlot();
    if (Has(app, slot, text)) return true;
    const uint32_t now = GetTickCount();
    if (chatAttempt && now - chatAttempt < 850U) return false;
    chatAttempt = now;
    app->mNetManager->SendChat(text);
    return Has(app, slot, text);
}

bool PartialSpeechWasRendered(LawnApp* app, CoopHelpDialog* help)
{
    if (!typingInitialFrame)
    {
        typingInitialFrame = app->mCoopSmokeFrame;
        return false;
    }
    SDL_Surface* frame = app->mCoopSmokeFrame.get();
    if (!frame || frame == typingInitialFrame.get()) return false;
    const Sexy::Rect bubble = help->GetSpeechBubbleRect();
    Uint8 r = 0, g = 0, b = 0, a = 0;
    if (!SDL_ReadSurfacePixel(frame, bubble.mX + bubble.mWidth / 2, bubble.mY + bubble.mHeight - 22, &r, &g, &b, &a) ||
        r != 249 || g != 247 || b != 242) return false;
    const Sexy::Rect body = help->GetSpeechBodyBounds();
    int ink = 0;
    for (int row = body.mY; row < body.mY + help->GetSpeechFontHeight(); ++row)
        for (int col = body.mX; col < body.mX + body.mWidth; ++col)
            if (SDL_ReadSurfacePixel(frame, col, row, &r, &g, &b, &a) && r < 130 && g < 115 && b < 100 && ++ink >= 8)
                return true;
    return false;
}

bool Click(LawnApp* app, const Sexy::Rect& r)
{
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    motion.motion.x = r.mX + r.mWidth / 2.0f; motion.motion.y = r.mY + r.mHeight / 2.0f;
    SDL_Event down{}; down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    down.button.windowID = motion.motion.windowID; down.button.button = SDL_BUTTON_LEFT;
    down.button.down = true; down.button.clicks = 1; down.button.x = motion.motion.x; down.button.y = motion.motion.y;
    SDL_Event up = down; up.type = SDL_EVENT_MOUSE_BUTTON_UP; up.button.down = false;
    return SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up);
}

bool Key(LawnApp* app, SDL_Keycode key)
{
    SDL_Event e{}; e.type = SDL_EVENT_KEY_DOWN; e.key.windowID = SDL_GetWindowID(app->mSDLWindow);
    e.key.key = key; e.key.scancode = SDL_GetScancodeFromKey(key, nullptr); e.key.down = true;
    SDL_Event up = e; up.type = SDL_EVENT_KEY_UP; up.key.down = false;
    return SDL_PushEvent(&e) && SDL_PushEvent(&up);
}

bool Record(LawnApp* app, const char* name)
{
    if (observed != stage) { observed = stage; since = GetTickCount(); return false; }
    if (GetTickCount() - since < (stage == 6 ? 900U : 350U)) return false;
    if (captured == stage) return true;
    if (auto* help = dynamic_cast<CoopHelpDialog*>(app->GetDialog(DIALOG_COOP_HELP)))
    {
        const Sexy::Rect bubble = help->GetSpeechBubbleRect();
        const Sexy::Point samples[] = { Sexy::Point(bubble.mX + bubble.mWidth / 2, bubble.mY + bubble.mHeight - 22),
            help->GetSpeechTailProbe(), help->GetSpeechTailJoinProbe() };
        for (const auto& point : samples)
        {
            Uint8 r = 0, g = 0, b = 0, a = 0;
            if (!app->mCoopSmokeFrame || !SDL_ReadSurfacePixel(app->mCoopSmokeFrame.get(), point.mX, point.mY, &r, &g, &b, &a) ||
                r != 249 || g != 247 || b != 242)
            { app->CoopSmokeFail("native renderer did not draw the pastel speech bubble with a seamless head-pointing tail"); return false; }
        }
        LawnNet::NetTrace("SMOKE_HELP_PIXELS role=%s step=%d ovalOffWhite=1 tailOffWhite=1 seamlessJoin=1", app->NetIsHost() ? "host" : "client", help->GetStep() + 1);
    }
    if (!app->CoopSmokeCaptureFrame(name)) { app->CoopSmokeFail("help renderer capture failed"); return false; }
    LawnNet::NetTrace("SMOKE_HELP role=%s step=%s sdl=1 capture=1", app->mCoopSmokeManifest.IsHost() ? "host" : "client", name);
    captured = stage;
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateHelp(LawnApp* app)
{
    const char* enabled = getenv("PVZ_COOP_SMOKE_HELP");
    if (!app || !app->CoopSmokeActive() || !enabled || strcmp(enabled, "1") != 0) return false;
    if (app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_1)
    { app->CoopSmokeFail("help lifecycle fixture requires the bounded Adventure1 manifest"); return true; }
    if (stage >= 20)
    {
        if (!app->mBoard)
        {
            if (app->NetIsClient()) Send(app, "HELP_CLIENT_PREVIEW_READY");
            return true;
        }
        if (app->mGameMode != GAMEMODE_ADVENTURE || app->mBoard->mLevel != 1 || app->mGameSelector ||
            app->GetDialog(DIALOG_COOP_HELP) || app->GetDialog(DIALOG_COOP_ONLINE) || app->GetDialog(DIALOG_NEWOPTIONS))
        { app->CoopSmokeFail("host level start retained tutorial or a native preview"); return true; }
        if (app->mGameScene != SCENE_PLAYING ||
            (app->NetIsHost() ? app->mCoopSmokeSnapshotsSent == 0 : app->mCoopSmokeSnapshotsApplied == 0)) return true;
        if (!Record(app, "help_scene_started") || !Send(app, "HELP_SCENE_OK")) return true;
        if (!Has(app, app->NetIsHost() ? 1 : 0, "HELP_SCENE_OK")) return true;
        NetTrace("SMOKE_HELP_SCENE_START role=%s realAdventure1=1 playing=1 helpClosed=1 previewsClosed=1",
            app->NetIsHost() ? "host" : "client");
        NetTrace("SMOKE_HELP_COMPLETE nativeZombieAnimated=1 steps=7 back=1 escape=1 urlButton=1 urlEscape=1 browserBounds=1 sceneCleanup=1 textFits=1");
        app->CoopSmokeWriteResult(true, nullptr);
        return true;
    }
    if (!app->mGameSelector || !app->mNetLobbyPanel || app->mBoard || !app->mPlayerInfo) return true;
    if (!app->mNetManager || app->mNetManager->GetPlayerCount() != 2) return true;
    app->mCoopSmokePeerReady = true;
    if (app->mGameSelector->mSelectorState != SELECTOR_IDLE || app->mGameSelector->mSlideCounter) return true;
    auto* help = dynamic_cast<CoopHelpDialog*>(app->GetDialog(DIALOG_COOP_HELP));
    if (stage == 0)
    {
        if (app->GetDialogCount()) return true;
        collapsed = app->mNetLobbyPanel->IsCollapsed(); lan = app->mNetLobbyPanel->IsLanExpanded();
        if (!Click(app, app->mGameSelector->GetCoopHelpIconRect())) app->CoopSmokeFail("help icon SDL click failed");
        stage = 1;
    }
    else if (stage >= 1 && stage <= CoopHelpDialog::STEP_COUNT)
    {
        if (!help || help->GetStep() != stage - 1) return true;
        if (!help->BodyTextFits())
        { app->CoopSmokeFail("tutorial body exceeds its measured native text rectangle"); return true; }
        if (stage == 1 && observed != stage) animation = help->GetZombieAnimationTime();
        const Sexy::Rect zombie = help->GetZombieBounds();
        const Sexy::Rect bubble = help->GetSpeechBubbleRect();
        if (zombie.mX != app->mWidth - 158 || zombie.mY != app->mHeight - 198 ||
            zombie.mX + zombie.mWidth > app->mWidth || zombie.mY + zombie.mHeight > app->mHeight ||
            bubble.mX < 0 || bubble.mY < 0 || bubble.mX + bubble.mWidth > app->mWidth || bubble.mY + bubble.mHeight > app->mHeight)
        { app->CoopSmokeFail("speech bubble or fixed ordinary zombie is outside the native canvas"); return true; }
        if (stage == 1 && !typingEarly)
        {
            const size_t visible = help->GetVisibleCharacters();
            if (visible < 16) return true;
            if (visible >= help->GetSpeechCharacters())
            { app->CoopSmokeFail("typewriter did not expose a partial native speech frame"); return true; }
            if (!PartialSpeechWasRendered(app, help)) return true;
            if (!app->CoopSmokeCaptureFrame("help_typing_early"))
            { app->CoopSmokeFail("early typewriter frame capture failed"); return true; }
            const double gain = help->GetTypingSoundGain();
            if (gain != 0.0 && (gain < 0.08 || gain > 0.10))
            { app->CoopSmokeFail("typewriter native sound gain is not quiet"); return true; }
            NetTrace("SMOKE_HELP_TYPING role=%s phase=early visible=%zu total=%zu ticks=%u gain=%.2f userVolume=%.2f muted=%d freshRendererFrame=1 renderedInk=1",
                app->NetIsHost() ? "host" : "client", visible, help->GetSpeechCharacters(), help->GetTypingSoundTicks(),
                gain, app->GetSfxVolume(), app->IsMuted() ? 1 : 0);
            if (!Key(app, SDLK_SPACE)) app->CoopSmokeFail("typewriter Space completion SDL key failed");
            typingInitialFrame.reset();
            typingEarly = true; return true;
        }
        if (!help->IsTypingComplete()) return true;
        if (stage == 1)
        {
            if (!typingFinished) { typingFinished = GetTickCount(); finishedTicks = help->GetTypingSoundTicks(); return true; }
            if (GetTickCount() - typingFinished < 250U) return true;
            if (help->GetTypingSoundTicks() != finishedTicks || help->GetTypingSoundGain() != 0.0)
            { app->CoopSmokeFail("typing ticks continued after Space completed the speech"); return true; }
        }
        const Sexy::Rect h = help->GetHighlightRect();
        if (stage <= 4 && (h.mWidth <= 0 || h.mHeight <= 0 || h.mX < 0 || h.mY < 0 ||
            h.mX + h.mWidth > app->mWidth || h.mY + h.mHeight > app->mHeight))
        { app->CoopSmokeFail("help target is outside the actual game canvas"); return true; }
        if (h.mWidth && h.mHeight && h.Intersects(bubble))
        { app->CoopSmokeFail("speech bubble covers the actual spotlight target"); return true; }
        const std::string tag = "help_" + std::to_string(stage);
        if (captured != stage && observed != stage)
            NetTrace("SMOKE_HELP_LAYOUT role=%s step=%d fullStringMeasurement=1 bodyFits=1", app->NetIsHost() ? "host" : "client", stage);
        if (!Record(app, tag.c_str())) return true;
        if (stage == 1)
            NetTrace("SMOKE_HELP_TYPING role=%s phase=complete space=1 stoppedTicks=1 visible=%zu total=%zu fixedZombieBottomRight=1 ovalSpeechTail=1",
                app->NetIsHost() ? "host" : "client", help->GetVisibleCharacters(), help->GetSpeechCharacters());
        if (stage == 6)
        {
            if (!previewSince) { previewSince = GetTickCount(); return true; }
            if (GetTickCount() - previewSince < 700U) return true;
            if (!app->CoopSmokeCaptureFrame("help_6_animated"))
            { app->CoopSmokeFail("animated communications preview capture failed"); return true; }
            NetTrace("SMOKE_HELP_PREVIEWS middlePing=1 ctrlStroke=1 sunflowerBlink=1 twoFrames=1");
        }
        if (stage == 1 && help->GetZombieAnimationTime() == animation)
        { app->CoopSmokeFail("help native zombie did not animate"); return true; }
        if (stage == 3)
        {
            auto* browser = app->GetDialog(DIALOG_COOP_ONLINE);
            int refresh = 0, create = 0;
            for (Sexy::Widget* widget : browser->mWidgets)
            {
                auto* b = dynamic_cast<Sexy::ButtonWidget*>(widget);
                if (!b || (b->mId != 101 && b->mId != 103)) continue;
                if (b->mX + b->mWidth > browser->mWidth - 44)
                { app->CoopSmokeFail("browser control intersects the stone frame"); return true; }
                if (b->mId == 101) ++refresh; else ++create;
            }
            if (refresh != 1 || create != 1) { app->CoopSmokeFail("browser actual controls were not found"); return true; }
        }
        if (!Click(app, help->GetNavigationRect(2))) app->CoopSmokeFail("help NEXT/DONE SDL click failed");
        ++stage;
    }
    else if (stage == 8)
    {
        if (help) return true;
        if (app->GetDialogCount() || app->mNetLobbyPanel->IsCollapsed() != collapsed || app->mNetLobbyPanel->IsLanExpanded() != lan)
        { app->CoopSmokeFail("help DONE did not clean previews and restore the lobby"); return true; }
        if (Record(app, "help_done"))
        { Click(app, app->mGameSelector->GetCoopHelpIconRect()); ++stage; }
    }
    else if (stage == 9 && help && help->GetStep() == 0)
    { Key(app, SDLK_RIGHT); ++stage; }
    else if (stage == 10 && help && help->GetStep() == 1)
    { Key(app, SDLK_LEFT); ++stage; }
    else if (stage == 11 && help && help->GetStep() == 0)
    { Key(app, SDLK_ESCAPE); ++stage; }
    else if (stage == 12 && !help)
    {
        if (!Record(app, "help_escape")) return true;
        app->URLOpenFailed("https://github.com/LegitAntiAA/PvZ-Coop");
        ++stage;
    }
    else if (stage == 13)
    {
        auto* dialog = dynamic_cast<LawnDialog*>(app->GetDialog(DIALOG_OPENURL_WAIT));
        if (!dialog) return true;
        if (!dialog->mLawnYesButton || dialog->mLawnYesButton->mLabel != _S("OK") ||
            dialog->mDialogLines.find(_S("https://github.com/LegitAntiAA/PvZ-Coop")) == SexyString::npos)
        { app->CoopSmokeFail("URL failure dialog lacks actual OK button and URL body"); return true; }
        if (!Record(app, "github_error")) return true;
        const auto* b = dialog->mLawnYesButton;
        Click(app, Sexy::Rect(dialog->mX + b->mX, dialog->mY + b->mY, b->mWidth, b->mHeight));
        ++stage;
    }
    else if (stage == 14 && !app->GetDialog(DIALOG_OPENURL_WAIT))
    { app->URLOpenFailed("https://github.com/LegitAntiAA/PvZ-Coop"); ++stage; }
    else if (stage == 15 && app->GetDialog(DIALOG_OPENURL_WAIT))
    { Key(app, SDLK_ESCAPE); ++stage; }
    else if (stage == 16 && !app->GetDialog(DIALOG_OPENURL_WAIT))
    {
        if (Record(app, "github_closed") && Send(app, "HELP_NAVIGATION_DONE"))
        {
            ++stage;
        }
    }
    else if (stage == 17)
    {
        if (!Has(app, 0, "HELP_NAVIGATION_DONE") || !Has(app, 1, "HELP_NAVIGATION_DONE")) return true;
        if (app->NetIsClient())
        {
            if (!Click(app, app->mGameSelector->GetCoopHelpIconRect()))
                app->CoopSmokeFail("scene cleanup tutorial reopen SDL click failed");
            ++stage;
        }
        else if (Has(app, 1, "HELP_CLIENT_PREVIEW_READY"))
        {
            // A fresh stock Adventure menu click first launches GAMEMODE_INTRO.
            // Exercise the ordinary authoritative level entry for this bounded
            // Adventure1 manifest, keeping its decoder allowlist unchanged.
            stage = 20;
            NetTrace("SMOKE_HELP_SCENE_TRIGGER hostNativeStart=1 clientPreviewReady=1 exactAdventure1=1");
            app->KillGameSelector();
            app->NetHostStartGame(GAMEMODE_ADVENTURE, 1);
        }
    }
    else if (stage == 18 && help && help->GetStep() == 0)
    {
        if (!Key(app, SDLK_RIGHT) || !Key(app, SDLK_RIGHT)) app->CoopSmokeFail("scene cleanup preview keys failed");
        ++stage;
    }
    else if (stage == 19 && help && help->GetStep() == 2 && app->GetDialog(DIALOG_COOP_ONLINE))
    {
        if (!Record(app, "help_scene_pending")) return true;
        NetTrace("SMOKE_HELP_SCENE_PREVIEW role=client ordinaryZombieLive=1 onlinePreview=1");
        stage = 20;
        Send(app, "HELP_CLIENT_PREVIEW_READY");
    }
    return true;
}
}
