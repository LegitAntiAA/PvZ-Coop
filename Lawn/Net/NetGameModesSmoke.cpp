#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Widget/GameModesDialog.h"
#include "../Widget/GameSelector.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <cstdlib>
#include <cstring>

namespace
{
int stage = 0;
int capturedStage = -1;
uint32_t since = 0, nextChat = 0;

void Advance(int next) { stage = next; since = GetTickCount(); }
bool Settled() { return GetTickCount() - since >= 400U; }

bool Has(LawnApp* app, int slot, const char* text)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

bool Send(LawnApp* app, const char* text)
{
    if (Has(app, app->mNetManager->GetMySlot(), text)) return true;
    const auto now = GetTickCount();
    if (nextChat && now - nextChat < 850U) return false;
    nextChat = now;
    app->mNetManager->SendChat(text);
    return Has(app, app->mNetManager->GetMySlot(), text);
}

bool Click(LawnApp* app, const Sexy::Rect& rect)
{
    SDL_Event move{};
    move.type = SDL_EVENT_MOUSE_MOTION;
    move.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    move.motion.x = rect.mX + rect.mWidth / 2.0f;
    move.motion.y = rect.mY + rect.mHeight / 2.0f;
    SDL_Event down{};
    down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    down.button.windowID = move.motion.windowID;
    down.button.button = SDL_BUTTON_LEFT;
    down.button.clicks = 1;
    down.button.down = true;
    down.button.x = move.motion.x;
    down.button.y = move.motion.y;
    SDL_Event up = down;
    up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    up.button.down = false;
    return SDL_PushEvent(&move) && SDL_PushEvent(&down) && SDL_PushEvent(&up);
}

bool Key(LawnApp* app, SDL_Keycode code)
{
    SDL_Event down{};
    down.type = SDL_EVENT_KEY_DOWN;
    down.key.windowID = SDL_GetWindowID(app->mSDLWindow);
    down.key.key = code;
    down.key.scancode = SDL_GetScancodeFromKey(code, nullptr);
    down.key.down = true;
    SDL_Event up = down;
    up.type = SDL_EVENT_KEY_UP;
    up.key.down = false;
    return SDL_PushEvent(&down) && SDL_PushEvent(&up);
}

bool Capture(LawnApp* app, const char* name)
{
    if (capturedStage == stage) return true;
    if (!app->CoopSmokeCaptureFrame(name))
    { app->CoopSmokeFail("gamemode renderer capture failed"); return false; }
    LawnNet::NetTrace("SMOKE_GAMEMODES_FRAME role=%s name=%s nativeSDL=1",
        app->NetIsHost() ? "host" : "client", name);
    capturedStage = stage;
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateGameModes(LawnApp* app)
{
    using LawnFusion::ContentMode;
    const char* enabled = getenv("PVZ_COOP_SMOKE_GAME_MODES");
    if (!app || !app->CoopSmokeActive() || !enabled || strcmp(enabled, "1")) return false;
    if (app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_1)
    { app->CoopSmokeFail("gamemode fixture requires the bounded Adventure1 manifest"); return true; }
    if (stage >= 10)
    {
        if (!app->mBoard)
        {
            if (app->NetIsClient()) Send(app, "GAMEMODES_CLIENT_MODAL");
            return true;
        }
        if (app->mGameScene != SCENE_PLAYING) return true;
        const auto snapshots = app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied;
        if (snapshots < app->mCoopSmokeManifest.mMinimumSnapshots || !Settled()) return true;
        if (app->GetContentMode() != ContentMode::Classic || app->mGameSelector ||
            app->GetDialog(DIALOG_GAME_MODES) || app->mGameMode != GAMEMODE_ADVENTURE || app->mBoard->mLevel != 1)
        { app->CoopSmokeFail("gamemode dialog survived the authoritative Classic start"); return true; }
        if (stage == 10)
        {
            if (!Capture(app, "gamemodes_classic_started")) return true;
            Advance(11);
        }
        if (!Send(app, "GAMEMODES_PLAY_OK") || !Has(app, app->NetIsHost() ? 1 : 0, "GAMEMODES_PLAY_OK")) return true;
        NetTrace("SMOKE_GAMEMODES_COMPLETE role=%s mouse=1 keyboard=1 hostAuthority=1 unavailableGuard=1 sceneCleanup=1 classicPlaying=1",
            app->NetIsHost() ? "host" : "client");
        app->CoopSmokeWriteResult(true, nullptr);
        return true;
    }
    if (!app->mNetManager || app->mNetManager->GetPlayerCount() != 2 || !app->mGameSelector ||
        !app->mGameSelector->MainMenuIconsVisible() || app->mBoard) return true;
    app->mCoopSmokePeerReady = true;
    auto* dialog = dynamic_cast<GameModesDialog*>(app->GetDialog(DIALOG_GAME_MODES));
    if (stage == 0)
    {
        if (!since) since = GetTickCount();
        if (!Settled() || app->GetDialogCount()) return true;
        if (!Capture(app, "gamemodes_menu") || !Click(app, app->mGameSelector->GetGameModesButtonRect())) return true;
        Advance(1);
    }
    else if (stage == 1 && dialog && Settled())
    {
        const auto classic = dialog->GetModeButtonRect(ContentMode::Classic);
        const auto fusion = dialog->GetModeButtonRect(ContentMode::Fusion);
        if (classic.mX < 0 || fusion.mY + fusion.mHeight > app->mHeight ||
            fusion.mX + fusion.mWidth > app->mWidth || classic.Intersects(fusion))
        { app->CoopSmokeFail("gamemode native choices are clipped or overlapping"); return true; }
        if (!Capture(app, "gamemodes_open") || !Click(app, fusion)) return true;
        Advance(2);
    }
    else if (stage == 2 && dialog && Settled())
    {
        if (app->GetContentMode() != ContentMode::Classic || app->CanSelectContentMode() == app->NetIsClient())
        { app->CoopSmokeFail("gamemode mouse action violated host authority or availability"); return true; }
        if (app->NetIsHost() && dialog->GetNotice().empty())
        { app->CoopSmokeFail("unavailable gamemode click did not show its actual reason"); return true; }
        if (!Capture(app, "gamemodes_fusion_guard") || !Key(app, SDLK_ESCAPE)) return true;
        Advance(3);
    }
    else if (stage == 3 && !dialog && Settled())
    {
        if (!Click(app, app->mGameSelector->GetGameModesButtonRect())) return true;
        Advance(4);
    }
    else if (stage == 4 && dialog && Settled())
    {
        if (!Key(app, SDLK_TAB) || !Key(app, SDLK_RETURN)) return true;
        Advance(5);
    }
    else if (stage == 5 && dialog && Settled())
    {
        if (app->GetContentMode() != ContentMode::Classic || dialog->GetKeyboardChoice() != 1 || dialog->GetNotice().empty())
        { app->CoopSmokeFail("gamemode keyboard action bypassed availability or host authority"); return true; }
        if (!Capture(app, "gamemodes_keyboard_guard") || !Key(app, SDLK_ESCAPE)) return true;
        Advance(6);
    }
    else if (stage == 6 && !dialog && Settled())
    {
        if (!Capture(app, "gamemodes_closed") || !Send(app, "GAMEMODES_DONE")) return true;
        Advance(7);
    }
    else if (stage == 7)
    {
        if (!Has(app, 0, "GAMEMODES_DONE") || !Has(app, 1, "GAMEMODES_DONE")) return true;
        if (app->NetIsClient())
        {
            if (!Click(app, app->mGameSelector->GetGameModesButtonRect())) return true;
            Advance(8);
        }
        else if (Has(app, 1, "GAMEMODES_CLIENT_MODAL"))
        {
            Advance(10);
            app->KillGameSelector();
            app->NetHostStartGame(GAMEMODE_ADVENTURE, 1);
        }
    }
    else if (stage == 8 && dialog && Settled())
    {
        Advance(10);
        Send(app, "GAMEMODES_CLIENT_MODAL");
    }
    return true;
}
}
