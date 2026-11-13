#include "NetZenGardenSmoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetLobbyPanel.h"
#include "../Board.h"
#include "../ZenGarden.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../GridItem.h"
#include "../CursorObject.h"
#include "../System/PlayerInfo.h"
#include "../Widget/GameSelector.h"
#include "../Widget/GameButton.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

using namespace LawnNet;

namespace
{
const char* const steps[] = {"profile", "needs", "water", "fertilizer", "spray", "music",
    "glove", "wheelbarrow_full", "wheelbarrow_empty", "snail_awake", "snail_chocolate",
    "plant_chocolate", "coin", "mushroom", "aquarium", "main", "complete"};
struct State
{
    int stage = 0, observed = -1, captured = -1, action = 0;
    uint32_t observedTick = 0, lastAck = 0;
    uint32_t lastPublish = 0;
    std::string pendingMessage;
    bool prepared = false, entered = false;
    int privateCount = -1, coinsBefore = 0;
    uint32_t privateHash = 0;
    uint16_t coin = 0;
    bool toolSeen[4] = {};
    std::map<uint16_t, int> drawn;
} s;

bool Enabled(LawnApp* app)
{
    const char* enabled = getenv("PVZ_COOP_SMOKE_ZEN_GARDEN");
    return app && app->CoopSmokeActive() && enabled && !strcmp(enabled, "1");
}

bool Has(LawnApp* app, int slot, const std::string& text)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

void ResetStage(int stage)
{
    s.stage = stage; s.observed = s.captured = -1; s.action = 0;
    s.observedTick = s.lastAck = 0; s.drawn.clear();
}

bool Move(LawnApp* app, float x, float y)
{
    SDL_Event event{}; event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    event.motion.x = x; event.motion.y = y;
    return SDL_PushEvent(&event);
}

bool Click(LawnApp* app, float x, float y)
{
    if (!Move(app, x, y)) return false;
    SDL_Event down{}; down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    down.button.windowID = SDL_GetWindowID(app->mSDLWindow);
    down.button.button = SDL_BUTTON_LEFT; down.button.down = true; down.button.clicks = 1;
    down.button.x = x; down.button.y = y;
    SDL_Event up = down; up.type = SDL_EVENT_MOUSE_BUTTON_UP; up.button.down = false;
    return SDL_PushEvent(&down) && SDL_PushEvent(&up);
}

bool ClickTool(LawnApp* app, GameObjectType tool)
{
    Rect rect = app->mBoard->GetShovelButtonRect();
    if (tool == GameObjectType::OBJECT_TYPE_NEXT_GARDEN) rect.mX = 564;
    else app->mBoard->GetZenButtonRect(tool, rect);
    return Click(app, app->mBoard->mX + rect.mX + rect.mWidth / 2.0f,
        app->mBoard->mY + rect.mY + rect.mHeight / 2.0f);
}

Plant* At(LawnApp* app, int column, int row = 1)
{
    return app->mBoard ? app->mBoard->GetTopPlantAt(column, row, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER) : nullptr;
}

bool ClientTool(LawnApp* app, GameObjectType tool, CursorType cursor, int column, int row = 1)
{
    if (!app->NetIsClient()) return true;
    if (s.action == 0)
    {
        if (!ClickTool(app, tool)) { app->CoopSmokeFail("Zen client tool selection SDL failed"); return false; }
        s.action = 1;
    }
    else if (s.action == 1 && app->mBoard->mCursorObject->mCursorType == cursor)
    {
        const float x = app->mBoard->mX + app->mZenGarden->GridToPixelX(column, row) + 40.0f;
        const float y = app->mBoard->mY + app->mZenGarden->GridToPixelY(column, row) + 40.0f;
        if (!Click(app, x, y)) { app->CoopSmokeFail("Zen client tool target SDL failed"); return false; }
        s.action = 2;
        NetTrace("SMOKE_ZEN_INPUT role=client step=%s tool=%d cell=(%d,%d) sdl=1", steps[s.stage], tool, column, row);
    }
    return true;
}

bool Checkpoint(LawnApp* app)
{
    if (s.observed != s.stage) { s.observed = s.stage; s.observedTick = GetTickCount(); }
    if (GetTickCount() - s.observedTick < 250U) return false;
    if (s.captured != s.stage)
    {
        if (!app->CoopSmokeCaptureFrame((std::string("zen_") + steps[s.stage]).c_str()))
        { app->CoopSmokeFail("Zen renderer capture failed"); return false; }
        s.captured = s.stage;
        NetTrace("SMOKE_ZEN_FRAME role=%s step=%s capture=1 privatePotted=%d supplies=%ld,%ld,%ld",
            app->NetIsHost() ? "host" : "client", steps[s.stage], app->mPlayerInfo->mNumPottedPlants,
            app->mPlayerInfo->mPurchases[STORE_ITEM_FERTILIZER],
            app->mPlayerInfo->mPurchases[STORE_ITEM_BUG_SPRAY], app->mPlayerInfo->mPurchases[STORE_ITEM_CHOCOLATE]);
        if (s.stage == 12)
            NetTrace("SMOKE_ZEN_COIN role=%s id=%u before=%d after=%d collected=1",
                app->NetIsHost() ? "host" : "client", s.coin, s.coinsBefore, app->mPlayerInfo->mCoins);
    }
    const std::string ack = std::string("ZEN_ACK_") + std::to_string(s.stage);
    if (app->NetIsClient())
    {
        if (!s.lastAck || GetTickCount() - s.lastAck >= 800U)
        { s.lastAck = GetTickCount(); app->mNetManager->SendChat(ack); }
        return false;
    }
    return Has(app, 1, ack);
}

bool PublishHost(LawnApp* app, const std::string& message)
{
    if (Has(app, 0, message)) return true;
    if (s.pendingMessage != message) { s.pendingMessage = message; s.lastPublish = 0; }
    if (!s.lastPublish || GetTickCount() - s.lastPublish >= 800U)
    {
        s.lastPublish = GetTickCount();
        app->mNetManager->SendChat(message);
    }
    return Has(app, 0, message);
}

void Advance(LawnApp* app)
{
    const int stage = s.stage + 1;
    if (PublishHost(app, std::string("ZEN_STAGE_") + std::to_string(stage))) ResetStage(stage);
}

bool Need(LawnApp* app, int column, PottedPlantNeed expected)
{
    Plant* plant = At(app, column);
    if (!plant || !plant->mNetId || app->mZenGarden->GetPlantNeedForDisplay(plant) != expected) return false;
    const auto drawn = s.drawn.find(plant->mNetId);
    return drawn != s.drawn.end() && drawn->second == (int)expected;
}

bool NoFeedingTools(LawnApp* app)
{
    GridItem* item = nullptr;
    while (app->mBoard->IterateGridItems(item))
        if (item->mGridItemType == GridItemType::GRIDITEM_ZEN_TOOL) return false;
    return true;
}

void ObserveTool(LawnApp* app, int index)
{
    if (s.toolSeen[index]) return;
    GridItem* item = nullptr;
    while (app->mBoard->IterateGridItems(item))
    {
        if (item->mGridItemType != GRIDITEM_ZEN_TOOL) continue;
        Reanimation* animation = app->ReanimationTryToGet(item->mGridItemReanimID);
        if (!animation || animation->mAnimRate <= 0.0f || animation->mAnimTime < 0.30f) continue;
        if (!app->CoopSmokeCaptureFrame((std::string("zen_tool_") + steps[s.stage]).c_str()))
        { app->CoopSmokeFail("Zen active native tool renderer capture failed"); return; }
        s.toolSeen[index] = true;
        NetTrace("SMOKE_ZEN_TOOL role=%s step=%s type=%d rate=%.2f time=%.4f capture=1",
            app->NetIsHost() ? "host" : "client", steps[s.stage], animation->mReanimationType,
            animation->mAnimRate, animation->mAnimTime);
        return;
    }
}

bool HappyGlow(LawnApp* app, int column)
{
    Plant* plant = At(app, column);
    if (!plant || plant->mState != STATE_ZEN_GARDEN_HAPPY) return false;
    Plant* pot = app->mBoard->GetTopPlantAt(column, 1, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
    TodParticleSystem* glow = app->ParticleTryToGet((pot ? pot : plant)->mParticleID);
    return glow && !glow->mDead && (glow->mEffectType == PARTICLE_POTTED_PLANT_GLOW ||
        glow->mEffectType == PARTICLE_POTTED_WATER_PLANT_GLOW || glow->mEffectType == PARTICLE_POTTED_ZEN_GLOW);
}

bool PrivateProfileUnchanged(LawnApp* app)
{
    if (!app->NetIsClient()) return true;
    const int count = app->mPlayerInfo->mNumPottedPlants;
    const uint32_t hash = NetFnv1a32((const uint8_t*)app->mPlayerInfo->mPottedPlant, sizeof(app->mPlayerInfo->mPottedPlant));
    if (s.privateCount < 0) { s.privateCount = count; s.privateHash = hash; }
    if (count != s.privateCount || hash != s.privateHash)
    { app->CoopSmokeFail("Zen client modified private potted profile records"); return false; }
    return true;
}
}

namespace LawnNet
{
void NetSmokeObserveZenOverlay(LawnApp* app, const Plant* plant, PottedPlantNeed need)
{
    if (!Enabled(app) || !plant || !plant->mNetId) return;
    const auto previous = s.drawn.find(plant->mNetId);
    if (previous == s.drawn.end() || previous->second != (int)need)
        NetTrace("SMOKE_ZEN_OVERLAY role=%s stage=%d id=%u need=%d drawn=%d",
            app->NetIsHost() ? "host" : "client", s.stage, plant->mNetId, (int)need, need != PLANTNEED_NONE ? 1 : 0);
    s.drawn[plant->mNetId] = need;
}

bool NetSmokeZenGardenAllowsStart(LawnApp* app, uint8_t mode, int32_t level)
{
    return Enabled(app) && !app->mCoopSmokeResultWritten &&
        (s.stage == 1 || (s.stage == 0 && Has(app, 0, "ZEN_STAGE_1"))) &&
        mode == (uint8_t)GAMEMODE_CHALLENGE_ZEN_GARDEN && level == 45 &&
        app->mPlayerInfo && app->mPlayerInfo->mLevel == 45 && app->mPlayerInfo->mFinishedAdventure == 1;
}

bool NetSmokeUpdateZenGarden(LawnApp* app)
{
    if (!Enabled(app)) return false;
    if (app->mCoopSmokeResultWritten) return true;
    NetManager* net = app->mNetManager;
    if (!app->mLoadingThreadCompleted || !net || !app->mPlayerInfo || net->GetPlayerCount() != 2) return true;
    if (app->mWidth != 800 || app->mHeight != 600 || app->mCoopSmokeManifest.mSkipIntro)
    { app->CoopSmokeFail("Zen fixture requires isolated 800x600 full-intro manifest"); return true; }
    app->mCoopSmokePeerReady = true;
    if (!PrivateProfileUnchanged(app)) return true;
    const bool host = app->NetIsHost();
    if (!host)
    {
        int newest = s.stage;
        for (const auto& line : net->mChatLines)
            if (line.mSlot == 0 && line.mText.find("ZEN_STAGE_") == 0)
                newest = max(newest, atoi(line.mText.c_str() + 10));
        if (newest > s.stage && newest <= 16) ResetStage(newest);
    }
    if (s.stage == 0)
    {
        if (!app->mGameSelector || app->mGameSelector->mSelectorState != SELECTOR_IDLE) return true;
        if (host && !s.prepared)
        {
            PlayerInfo* profile = app->mPlayerInfo;
            profile->mLevel = 45; profile->mFinishedAdventure = 1; profile->mHasSeenStinky = 1;
            profile->mHasWokenStinky = 1;
            profile->mPurchases[STORE_ITEM_STINKY_THE_SNAIL] = 1;
            profile->mPurchases[STORE_ITEM_FERTILIZER] = profile->mPurchases[STORE_ITEM_BUG_SPRAY] =
                profile->mPurchases[STORE_ITEM_CHOCOLATE] = 1005;
            profile->mPurchases[STORE_ITEM_PHONOGRAPH] = profile->mPurchases[STORE_ITEM_GARDENING_GLOVE] =
                profile->mPurchases[STORE_ITEM_WHEEL_BARROW] = profile->mPurchases[STORE_ITEM_MUSHROOM_GARDEN] =
                profile->mPurchases[STORE_ITEM_AQUARIUM_GARDEN] = 1;
            profile->mStinkyPosX = 550; profile->mStinkyPosY = 461;
            profile->mNumPottedPlants = 4;
            const SeedType types[] = {SEED_PEASHOOTER, SEED_SUNFLOWER, SEED_CHOMPER, SEED_WALLNUT};
            for (int i = 0; i < 4; ++i)
            {
                PottedPlant& p = profile->mPottedPlant[i]; p.InitializePottedPlant(types[i]);
                p.mPlantAge = i == 1 ? PLANTAGE_MEDIUM : PLANTAGE_FULL;
                p.mWhichZenGarden = GARDEN_MAIN; p.mX = i; p.mY = 1; p.mFacing = PottedPlant::FACING_RIGHT;
                p.mFeedingsPerGrow = 3; p.mTimesFed = i == 0 ? 0 : 3;
                p.mLastWateredTime = _time64(nullptr) - 60;
                p.mPlantNeed = i == 2 ? PLANTNEED_BUGSPRAY : i == 3 ? PLANTNEED_PHONOGRAPH : PLANTNEED_NONE;
            }
            app->mGameSelector->SyncProfile(false); app->NetBroadcastProfile(); s.prepared = true;
        }
        if (app->mPlayerInfo->mLevel == 45 && app->mPlayerInfo->mFinishedAdventure == 1 &&
            app->mPlayerInfo->mPurchases[STORE_ITEM_FERTILIZER] == 1005 && Checkpoint(app)) Advance(app);
        return true;
    }
    if (s.stage == 1 && host && !s.entered && app->mGameSelector)
    {
        if (app->mNetLobbyPanel && !app->mNetLobbyPanel->IsCollapsed())
        {
            if (!s.action)
            {
                const Rect rect = app->mNetLobbyPanel->GetButtonRect(NetLobbyPanel::BTN_TOGGLE);
                if (!Click(app, rect.mX + rect.mWidth / 2.0f, rect.mY + rect.mHeight / 2.0f))
                    app->CoopSmokeFail("Zen lobby collapse SDL failed");
                s.action = 1;
            }
            return true;
        }
        NewLawnButton* button = app->mGameSelector->mZenGardenButton;
        if (!button->mVisible || button->mDisabled) return true;
        s.entered = true;
        if (!Click(app, button->mX + button->mWidth / 2.0f, button->mY + button->mHeight / 2.0f))
            app->CoopSmokeFail("Zen host native menu entry SDL failed");
        return true;
    }
    if (!app->mBoard || app->mGameMode != GAMEMODE_CHALLENGE_ZEN_GARDEN || app->mGameScene != SCENE_PLAYING) return true;
    if (host) Move(app, 790, 590); // deliberately away from every client target
    ZenGarden* garden = app->mZenGarden;
    bool ready = false;
    if (s.stage == 1)
        ready = Need(app, 0, PLANTNEED_WATER) && Need(app, 1, PLANTNEED_FERTILIZER) &&
            Need(app, 2, PLANTNEED_BUGSPRAY) && Need(app, 3, PLANTNEED_PHONOGRAPH);
    else if (s.stage >= 2 && s.stage <= 5)
    {
        const GameObjectType tools[] = {OBJECT_TYPE_WATERING_CAN, OBJECT_TYPE_FERTILIZER, OBJECT_TYPE_BUG_SPRAY, OBJECT_TYPE_PHONOGRAPH};
        const CursorType cursors[] = {CURSOR_TYPE_WATERING_CAN, CURSOR_TYPE_FERTILIZER, CURSOR_TYPE_BUG_SPRAY, CURSOR_TYPE_PHONOGRAPH};
        const int column = s.stage - 2;
        ClientTool(app, tools[column], cursors[column], column);
        ObserveTool(app, column);
        ready = s.toolSeen[column] && Need(app, column, PLANTNEED_NONE) && NoFeedingTools(app) &&
            (s.stage < 3 || app->mPlayerInfo->mPurchases[STORE_ITEM_FERTILIZER] == 1004) &&
            (s.stage < 4 || app->mPlayerInfo->mPurchases[STORE_ITEM_BUG_SPRAY] == 1004) &&
            (s.stage < 4 || HappyGlow(app, column));
    }
    else if (s.stage == 6)
    {
        ClientTool(app, OBJECT_TYPE_GLOVE, CURSOR_TYPE_GLOVE, 0);
        if (!host && s.action == 2 && app->mBoard->mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_GLOVE)
        {
            if (!Click(app, garden->GridToPixelX(4, 1) + 40.0f, garden->GridToPixelY(4, 1) + 40.0f))
                app->CoopSmokeFail("Zen client glove destination SDL failed");
            s.action = 3;
        }
        Plant* moved = At(app, 4);
        ready = moved && moved->mSeedType == SEED_PEASHOOTER && !At(app, 0) &&
            app->mBoard->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL;
    }
    else if (s.stage == 7)
    {
        ClientTool(app, OBJECT_TYPE_WHEELBARROW, CURSOR_TYPE_WHEEELBARROW, 4);
        PottedPlant* potted = garden->GetPottedPlantInWheelbarrow();
        ready = potted && potted->mSeedType == SEED_PEASHOOTER && !At(app, 4);
    }
    else if (s.stage == 8)
    {
        ClientTool(app, OBJECT_TYPE_WHEELBARROW, CURSOR_TYPE_WHEEELBARROW, 5);
        Plant* returned = At(app, 5);
        ready = !garden->GetPottedPlantInWheelbarrow() && returned && returned->mSeedType == SEED_PEASHOOTER;
    }
    else if (s.stage == 9 || s.stage == 10)
    {
        GridItem* snail = garden->GetStinky();
        if (!snail) return true;
        if (!host && s.action == 0)
        {
            const bool queued = s.stage == 10 ? ClickTool(app, OBJECT_TYPE_CHOCOLATE) :
                Click(app, snail->mPosX + 35, snail->mPosY + 30);
            if (!queued) { app->CoopSmokeFail("Zen client snail SDL action failed"); return true; }
            s.action = 1;
            if (s.stage == 9) NetTrace("SMOKE_ZEN_INPUT role=client step=snail_awake sdl=1");
        }
        if (!host && s.stage == 10 && s.action == 1 &&
            app->mBoard->mCursorObject->mCursorType == CURSOR_TYPE_CHOCOLATE)
        {
            if (!Click(app, snail->mPosX + 35, snail->mPosY + 30))
            { app->CoopSmokeFail("Zen client snail chocolate SDL target failed"); return true; }
            s.action = 2;
            NetTrace("SMOKE_ZEN_INPUT role=client step=snail_chocolate sdl=1");
        }
        ready = s.stage == 9 ? !garden->IsStinkySleeping() :
            garden->IsStinkyHighOnChocolate() && app->mPlayerInfo->mPurchases[STORE_ITEM_CHOCOLATE] == 1004;
    }
    else if (s.stage == 11)
    {
        ClientTool(app, OBJECT_TYPE_CHOCOLATE, CURSOR_TYPE_CHOCOLATE, 2);
        Plant* plant = At(app, 2);
        Reanimation* body = plant ? app->ReanimationTryToGet(plant->mBodyReanimID) : nullptr;
        ready = plant && !garden->PlantCanHaveChocolate(plant) && body && body->mAnimRate >= 25.0f &&
            app->mPlayerInfo->mPurchases[STORE_ITEM_CHOCOLATE] == 1003;
        if (ready) s.coinsBefore = app->mPlayerInfo->mCoins;
    }
    else if (s.stage == 12)
    {
        if (host && !s.coin)
        {
            Coin* coin = app->mBoard->AddCoin(610, 110, COIN_SILVER, COIN_MOTION_COIN);
            if (!coin) { app->CoopSmokeFail("Zen native coin fixture creation failed"); return true; }
            coin->mNetId = app->mBoard->NetAllocId(); s.coin = coin->mNetId;
        }
        if (host && s.coin) PublishHost(app, std::string("ZEN_COIN_") + std::to_string(s.coin));
        if (!host && !s.coin)
            for (const auto& line : net->mChatLines)
                if (line.mSlot == 0 && line.mText.find("ZEN_COIN_") == 0)
                    s.coin = (uint16_t)atoi(line.mText.c_str() + 9);
        Coin* coin = nullptr; Coin* target = nullptr;
        while (app->mBoard->IterateCoins(coin)) if (coin->mNetId == s.coin) target = coin;
        if (!host && target && !target->mIsBeingCollected && !s.action)
        {
            const float x = target->mPosX + 25, y = target->mPosY + 25;
            s.coinsBefore = app->mPlayerInfo->mCoins;
            if (!Click(app, x, y))
            { app->CoopSmokeFail("Zen client coin SDL action failed"); return true; }
            s.action = 1;
            NetTrace("SMOKE_ZEN_INPUT role=client step=coin id=%u x=%d y=%d before=%d sdl=1",
                s.coin, (int)x, (int)y, s.coinsBefore);
        }
        ready = s.coin && (host || s.action == 1) && !target &&
            app->mPlayerInfo->mCoins > s.coinsBefore;
    }
    else if (s.stage >= 13 && s.stage <= 15)
    {
        if (!host && !s.action)
        {
            if (!ClickTool(app, OBJECT_TYPE_NEXT_GARDEN))
            { app->CoopSmokeFail("Zen client next garden SDL action failed"); return true; }
            s.action = 1;
            NetTrace("SMOKE_ZEN_INPUT role=client step=%s sdl=1", steps[s.stage]);
        }
        const GardenType expected[] = {GARDEN_MUSHROOM, GARDEN_AQUARIUM, GARDEN_MAIN};
        const BackgroundType backgrounds[] = {BACKGROUND_MUSHROOM_GARDEN, BACKGROUND_ZOMBIQUARIUM, BACKGROUND_GREENHOUSE};
        ready = garden->mGardenType == expected[s.stage - 13] && app->mBoard->mBackground == backgrounds[s.stage - 13] &&
            (s.stage != 15 || At(app, 5));
    }
    else if (s.stage == 16)
    {
        ClientTool(app, OBJECT_TYPE_MONEY_SIGN, CURSOR_TYPE_MONEY_SIGN, 2);
        for (const auto& line : net->mChatLines)
            if (line.mText == "Only the host can confirm selling a shared garden plant.") ready = true;
        ready = ready && app->GetDialogCount() == 0 && At(app, 2) &&
            app->mBoard->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL;
    }
    if (ready && Checkpoint(app))
    {
        if (s.stage == 16)
        {
            if (PublishHost(app, "ZEN_DONE")) app->CoopSmokeWriteResult(true, nullptr);
        }
        else Advance(app);
    }
    if (!host && s.stage == 16 && Has(app, 0, "ZEN_DONE"))
        app->CoopSmokeWriteResult(true, nullptr);
    return true;
}
}
