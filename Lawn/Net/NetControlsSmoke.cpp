#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Widget/GameButton.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
struct ControlsState
{
    int stage = 0;
    uint32_t started = 0, changed = 0, observed = 0, lastChat = 0;
    uint16_t packet = 0, plant = 0;
    SeedType seed = SEED_NONE;
    std::vector<uint16_t> originalPlants;
    bool action = false, recorded = false;
    bool bankPrepared = false, offclipIssued = false, offclipChecked = false;
    uint32_t offclipTick = 0;
};
ControlsState s;

bool Key(LawnApp* app, SDL_Keycode key)
{
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.windowID = SDL_GetWindowID(app->mSDLWindow);
    e.key.key = key; e.key.down = true;
    if (!SDL_PushEvent(&e)) return false;
    e.type = SDL_EVENT_KEY_UP; e.key.down = false;
    return SDL_PushEvent(&e);
}

bool Click(LawnApp* app, int x, int y)
{
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.windowID = SDL_GetWindowID(app->mSDLWindow);
    e.button.button = SDL_BUTTON_LEFT; e.button.down = true; e.button.clicks = 1;
    e.button.x = (float)x; e.button.y = (float)y;
    if (!SDL_PushEvent(&e)) return false;
    e.type = SDL_EVENT_MOUSE_BUTTON_UP; e.button.down = false;
    return SDL_PushEvent(&e);
}

bool Has(LawnApp* app, const std::string& text)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mText == text) return true;
    return false;
}

bool Send(LawnApp* app, const std::string& text)
{
    if (Has(app, text)) return true;
    const uint32_t now = GetTickCount();
    if (s.lastChat && now - s.lastChat < 850U) return false;
    app->mNetManager->SendChat(text);
    if (app->NetIsHost() && !Has(app, text)) return false;
    s.lastChat = now;
    return true;
}

uint16_t TaggedId(LawnApp* app, const char* tag)
{
    const std::string prefix(tag);
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == 0 && line.mText.rfind(prefix, 0) == 0)
            return (uint16_t)std::strtoul(line.mText.c_str() + prefix.size(), nullptr, 10);
    return 0;
}

CursorObject* ClientCursor(LawnApp* app)
{
    return app->NetIsHost() ? app->mBoard->mNetPlayerCursors[1] : app->mBoard->mCursorObject;
}

SeedPacket* HeldPacket(LawnApp* app, CursorObject* cursor)
{
    SeedBank* bank = app->mBoard->mSeedBank;
    if (!cursor || cursor->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK || !bank ||
        cursor->mSeedBankIndex < 0 || cursor->mSeedBankIndex >= bank->mNumPackets) return nullptr;
    return &bank->mSeedPackets[cursor->mSeedBankIndex];
}

Plant* NewPlant(LawnApp* app)
{
    Plant* plant = nullptr;
    while (app->mBoard->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard && plant->mSeedType == s.seed &&
            std::find(s.originalPlants.begin(), s.originalPlants.end(), plant->mNetId) == s.originalPlants.end())
            return plant;
    return nullptr;
}

bool FirstCardReady(LawnApp* app)
{
    SeedBank* bank = app->mBoard->mSeedBank;
    for (int i = 0; bank && i < bank->mNumPackets; ++i)
    {
        SeedPacket& packet = bank->mSeedPackets[i];
        if (packet.mPacketType == SEED_NONE || !app->mBoard->IsSeedPacketVisibleToSlot(i,app->mNetManager->GetMySlot())) continue;
        const int x = bank->mX + packet.mX + packet.mOffsetX + packet.mWidth / 2;
        const int y = bank->mY + packet.mY + packet.mHeight / 2;
        HitResult hit{};
        // Wait for native conveyor movement to expose a real clickable card.
        // CanPickUp alone is true even while its center is outside the bank.
        return packet.CanPickUp() && bank->MouseHitTest(x,y,&hit) &&
            hit.mObjectType == OBJECT_TYPE_SEEDPACKET && hit.mObject == &packet && packet.mNetId != 0;
    }
    return false;
}

bool Record(LawnApp* app, const char* step)
{
    if (s.recorded) return true;
    if (!s.observed) { s.observed = GetTickCount(); return false; }
    if (GetTickCount() - s.observed < 180U) return false;
    if (!app->CoopSmokeCaptureFrame((std::string("controls_") + step).c_str())) return false;
    LawnNet::NetTrace("SMOKE_CONTROLS_FRAME role=%s step=%s capture=1",
        app->NetIsHost() ? "host" : "client", step);
    s.recorded = true;
    return true;
}

void Advance(int stage)
{
    s.stage = stage; s.changed = GetTickCount(); s.observed = 0; s.action = s.recorded = false;
}
}

namespace LawnNet
{
// Completion gate after the existing scenario's real action and snapshot proof.
bool NetSmokeControlsComplete(LawnApp* app)
{
    const char* enabled = getenv("PVZ_COOP_SMOKE_CONTROLS");
    if (!enabled || std::string(enabled) != "1") return true;
    if (!app->CoopSmokeActive() || !app->mNetManager || !app->mBoard || app->mGameScene != SCENE_PLAYING ||
        !app->mCoopSmokeScenarioRuntime.IsComplete(app->mCoopSmokeManifest.mMinimumSnapshots)) return false;
    Board* board = app->mBoard;
    NetManager* net = app->mNetManager;
    const bool host = app->NetIsHost();
    const char* role = host ? "host" : "client";
    const uint32_t now = GetTickCount();
    if (!s.started)
    {
        if (app->mWidth != 800 || app->mHeight != 600 ||
            (app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_1 &&
             app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT))
        { app->CoopSmokeFail("controls fixture requires Adventure1 or Adventure5BowlingClient at 800x600"); return false; }
        s.started = s.changed = now;
        Plant* p = nullptr;
        while (board->mPlants.IterateNext(p)) if (!p->mDead) s.originalPlants.push_back(p->mNetId);
        NetTrace("SMOKE_CONTROLS_BEGIN role=%s conveyor=%d input=SDL_queue foreground=0", role, board->HasConveyorBeltSeedBank());
    }
    if (now - s.started > (board->HasConveyorBeltSeedBank() ? 90000U : 40000U))
    { app->CoopSmokeFail("controls native keyboard workflow timed out"); return false; }
    CursorObject* remote = ClientCursor(app);
    if (!remote) return false;
    if (s.stage == 0)
    {
        if (host)
        {
            // The early Adventure fixtures have no unlocked shovel and may have
            // exhausted their starting sun. Only test resources are prepared.
            board->mShowShovel = true; board->mSunMoney = 1000;
            if (board->HasConveyorBeltSeedBank() && !s.bankPrepared)
            {
                for (int i = board->mSeedBank->mNumPackets - 1; i >= 0; --i)
                    if (board->mSeedBank->mSeedPackets[i].mPacketType != SEED_NONE) board->mSeedBank->RemoveSeed(i);
                board->mSeedBank->AddSeed(SEED_WALLNUT, false);
                s.bankPrepared = true;
            }
            for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
                if (board->mSeedBank->mSeedPackets[i].mPacketType != SEED_NONE)
                { board->mSeedBank->mSeedPackets[i].mActive = true; board->mSeedBank->mSeedPackets[i].mRefreshing = false; }
            NetTrace("SMOKE_CONTROLS_FIXTURE shovelVisible=1 sun=1000 packetsReady=1");
            if (!Send(app, "CTL_PICK_GO")) return false;
            Advance(1);
        }
        else if (Has(app, "CTL_PICK_GO") && board->mShowShovel)
        {
            if (board->HasConveyorBeltSeedBank() && !s.offclipChecked)
            {
                if (!s.offclipIssued)
                {
                    const SeedPacket& first = board->mSeedBank->mSeedPackets[0];
                    if (first.mPacketType == SEED_NONE || !first.mNetId) return false;
                    if (board->mSeedBank->mX + first.mX + first.mOffsetX < board->mSeedBank->mX + 591)
                    { app->CoopSmokeFail("offclip fixture missed the native entering card"); return false; }
                    if (!Key(app, SDLK_1)) { app->CoopSmokeFail("offclip KEY1 SDL queue failed"); return false; }
                    s.offclipIssued = true; s.offclipTick = now; return false;
                }
                if (now - s.offclipTick < 300U) return false;
                if (remote->mCursorType != CURSOR_TYPE_NORMAL)
                { app->CoopSmokeFail("offclip digit selected a hidden card or another HUD tool"); return false; }
                if (!Send(app,"CTL_OFFCLIP_OK") || !Has(app,"CTL_OFFCLIP_OK")) return false;
                NetTrace("SMOKE_CONTROLS_OFFCLIP role=client key=1 hiddenCard=1 noPickup=1 noHud=1 sdl=1");
                s.offclipChecked = true;
            }
            if (!FirstCardReady(app)) return false;
            if (!Key(app, SDLK_1) || !Key(app, SDLK_1))
                app->CoopSmokeFail("controls rapid KEY1 SDL queue failed");
            Advance(1);
        }
        return false;
    }
    if (s.stage == 1)
    {
        if (host && board->HasConveyorBeltSeedBank() && !s.offclipChecked)
        {
            if (!Has(app,"CTL_OFFCLIP_OK")) return false;
            if (remote->mCursorType != CURSOR_TYPE_NORMAL)
            { app->CoopSmokeFail("host applied an offclip digit to a card or HUD tool"); return false; }
            NetTrace("SMOKE_CONTROLS_OFFCLIP role=host key=1 hiddenCard=1 noPickup=1 noHud=1 sdl=1");
            s.offclipChecked = true;
        }
        SeedPacket* held = HeldPacket(app, remote);
        if (!held || !held->mNetId) return false;
        if (host)
        {
            s.packet = held->mNetId; s.seed = held->mPacketType;
            if (!s.observed && !Send(app, "CTL_PICKED:" + std::to_string(s.packet))) return false;
        }
        else
        {
            const uint16_t expected = TaggedId(app, "CTL_PICKED:");
            if (!expected) return false;
            if (held->mNetId != expected) { app->CoopSmokeFail("digit pickup changed authoritative packet identity"); return false; }
            s.packet = expected; s.seed = held->mPacketType;
        }
        if (!Record(app, "client_pick")) return false;
        NetTrace("SMOKE_CONTROLS_PICK role=%s packet=%u key=1 type=%d", role, s.packet, (int)s.seed);
        Advance(2); return false;
    }
    if (s.stage == 2)
    {
        if (!host && !s.action)
        {
            for (int pass = 0; pass < MAX_GRID_SIZE_Y && !s.action; ++pass)
                for (int col = 0; col < MAX_GRID_SIZE_X && !s.action; ++col)
                    if (const int row = (pass + 2) % MAX_GRID_SIZE_Y;
                        board->CanPlantAt(col, row, s.seed) == PLANTING_OK)
                    {
                        if (!Click(app, board->GridToPixelX(col, row) + 40, board->GridToPixelY(col, row) + 40))
                            app->CoopSmokeFail("controls planting SDL queue failed");
                        s.action = true;
                    }
        }
        Plant* plant = NewPlant(app);
        if (!plant || !plant->mNetId) return false;
        if (host)
        {
            if (!s.observed && !Send(app, "CTL_PLANTED:" + std::to_string(plant->mNetId))) return false;
        }
        else if (TaggedId(app, "CTL_PLANTED:") != plant->mNetId) return false;
        s.plant = plant->mNetId;
        if (!Record(app, "client_plant")) return false;
        NetTrace("SMOKE_CONTROLS_PLANT role=%s packet=%u plant=%u type=%d", role, s.packet, s.plant, (int)s.seed);
        if (!host && !Key(app, SDLK_Q)) app->CoopSmokeFail("controls Q SDL queue failed");
        Advance(3); return false;
    }
    if (s.stage == 3)
    {
        if (remote->mCursorType != CURSOR_TYPE_SHOVEL) return false;
        if (host)
        {
            if (!s.observed && !Send(app, "CTL_SHOVEL")) return false;
        }
        else if (!Has(app, "CTL_SHOVEL")) return false;
        if (!Record(app, "client_shovel")) return false;
        NetTrace("SMOKE_CONTROLS_SHOVEL role=%s key=Q authoritative=1", role);
        if (!host && !Key(app, SDLK_Q)) app->CoopSmokeFail("controls Q cancel SDL queue failed");
        Advance(4); return false;
    }
    if (s.stage == 4)
    {
        if (remote->mCursorType != CURSOR_TYPE_NORMAL) return false;
        if (host) { if (!Send(app, "CTL_CHAT_GO")) return false; Advance(5); }
        else if (Has(app, "CTL_CHAT_GO"))
        {
            if (!Key(app, SDLK_RETURN)) app->CoopSmokeFail("controls chat Enter SDL queue failed");
            Advance(5);
        }
        return false;
    }
    if (s.stage == 5)
    {
        if (!host)
        {
            if (!s.action)
            {
                if (!app->mNetChatTyping) return false;
                SDL_Event text{}; text.type = SDL_EVENT_TEXT_INPUT;
                text.text.windowID = SDL_GetWindowID(app->mSDLWindow); text.text.text = "1q";
                if (!SDL_PushEvent(&text) || !Key(app, SDLK_1) || !Key(app, SDLK_Q))
                    app->CoopSmokeFail("controls chat input SDL queue failed");
                s.action = true; s.changed = now; return false;
            }
            if (now - s.changed < 250U) return false;
            if (!app->mNetChatTyping || SexyStringToStringFast(app->mNetChatDraft) != "1q" || remote->mCursorType != CURSOR_TYPE_NORMAL)
            { app->CoopSmokeFail("digit or Q hotkey leaked through native chat typing"); return false; }
            if (!Record(app, "chat_blocked") || !Send(app, "CTL_CHAT_OK") || !Key(app, SDLK_ESCAPE)) return false;
            Advance(6); return false;
        }
        if (!Has(app, "CTL_CHAT_OK")) return false;
        for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
            if (board->mSeedBank->mSeedPackets[i].mPacketType != SEED_NONE)
            {
                board->mSeedBank->mSeedPackets[i].mActive = true;
                board->mSeedBank->mSeedPackets[i].mRefreshing = false;
            }
        if (!FirstCardReady(app)) return false;
        // A real mouse can remain over Menu while a keyboard slot is selected.
        board->mMenuButton->mIsOver = true;
        if (!Key(app, SDLK_1)) app->CoopSmokeFail("controls host KEY1 SDL queue failed");
        Advance(6); return false;
    }
    if (s.stage == 6)
    {
        if (!host) { if (Has(app, "CTL_HOST_PICKED")) Advance(7); return false; }
        if (!HeldPacket(app, board->mCursorObject)) return false;
        if (app->GetDialogCount()) { app->CoopSmokeFail("host keyboard slot activated hovered Menu"); return false; }
        board->mMenuButton->mIsOver = false;
        if (!Record(app, "host_pick")) return false;
        if (!Send(app, "CTL_HOST_PICKED")) return false;
        if (!Key(app, SDLK_Q)) app->CoopSmokeFail("controls host Q SDL queue failed");
        Advance(7); return false;
    }
    if (s.stage == 7)
    {
        if (!host) { if (Has(app, "CTL_PAUSE_GO")) Advance(9); return false; }
        if (board->mCursorObject->mCursorType != CURSOR_TYPE_SHOVEL) return false;
        if (!Record(app, "host_shovel") || !Key(app, SDLK_Q)) return false;
        Advance(8); return false;
    }
    if (s.stage == 8)
    {
        if (board->mCursorObject->mCursorType != CURSOR_TYPE_NORMAL) return false;
        if (!Send(app, "CTL_PAUSE_GO")) return false;
        board->mPaused = true; net->SendPauseState(true);
        if (!Key(app, SDLK_1) || !Key(app, SDLK_Q)) app->CoopSmokeFail("controls paused SDL queue failed");
        Advance(9); return false;
    }
    if (s.stage == 9)
    {
        if (host)
        {
            if (now - s.changed < 250U) return false;
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_NORMAL)
            { app->CoopSmokeFail("hotkeys bypassed paused host state"); return false; }
            if (!Send(app, "CTL_COMPLETE")) return false;
            board->mPaused = false; net->SendPauseState(false);
            Advance(10); return false;
        }
        if (!Has(app, "CTL_COMPLETE") || net->mRemotePaused) return false;
        if (app->mNetChatTyping || remote->mCursorType != CURSOR_TYPE_NORMAL)
        { app->CoopSmokeFail("controls recovery did not restore ordinary client input"); return false; }
        if (!Send(app, "CTL_DONE_ACK")) return false;
        Advance(10);
    }
    if (s.stage == 10)
    {
        if (host && !Has(app, "CTL_DONE_ACK")) return false;
        if (!s.action)
        {
            if (!Record(app, "complete")) return false;
            NetTrace("SMOKE_CONTROLS_COMPLETE role=%s conveyor=%d clientDigit=1 hostDigit=1 shovel=1 cancel=1 chatGate=1 pauseGate=1 rapidRepeat=1 packet=%u plant=%u",
                role, board->HasConveyorBeltSeedBank(), s.packet, s.plant);
            s.action = true;
        }
        return true;
    }
    return false;
}
}
