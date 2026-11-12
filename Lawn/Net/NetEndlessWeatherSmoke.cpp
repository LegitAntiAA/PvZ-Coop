#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetEndlessWeather.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../System/Music.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameSelector.h"
#include "../Widget/GameButton.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/BassMusicInterface.h"
#include "../../SexyAppFramework/BassLoader.h"
#include "../../Resources.h"
#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace LawnNet;
namespace
{
struct WeatherSmoke
{
    int stage = 0, picked = 0, plantStep = 0, plantType = -1, sampleBucket = -1;
    bool prepared = false, setup = false, captured = false, frozen = false, resumeProbed = false;
    uint32_t lastSend = 0, observed = 0, stable = 0, plantTick = 0, frozenHash = 0;
    double userVolume = 0;
    std::shared_ptr<SDL_Surface> frame;
};
WeatherSmoke s;

bool Check(LawnApp* a, bool ok, const char* why)
{ if (!ok) a->CoopSmokeFail(why); return ok; }

void Next()
{
    ++s.stage; s.setup = s.captured = s.frozen = s.resumeProbed = false;
    s.lastSend = s.observed = s.stable = s.frozenHash = 0; s.frame.reset(); s.sampleBucket = -1;
}

bool Chat(LawnApp* a, const std::string& text, int slot)
{
    for (const auto& line : a->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

void Send(LawnApp* a, const std::string& text)
{
    if (!s.lastSend || GetTickCount() - s.lastSend >= 800U)
    { a->mNetManager->SendChat(text); s.lastSend = GetTickCount(); }
}

bool Motion(LawnApp* a, int x, int y)
{
    SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = SDL_GetWindowID(a->mSDLWindow);
    e.motion.x = (float)x; e.motion.y = (float)y; return SDL_PushEvent(&e);
}

bool Click(LawnApp* a, int x, int y)
{
    if (a->mWidgetManager->mLastMouseX != x || a->mWidgetManager->mLastMouseY != y)
    { Motion(a, x, y); return false; }
    for (bool down : {true, false})
    {
        SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = SDL_GetWindowID(a->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
        e.button.down = down; e.button.clicks = 1; e.button.x = (float)x; e.button.y = (float)y;
        if (!SDL_PushEvent(&e)) return false;
    }
    return true;
}

Plant* At(Board* b, SeedType type, int col, int row)
{
    Plant* p = nullptr;
    while (b->IteratePlants(p))
        if (!p->mDead && p->mSeedType == type && p->mPlantCol == col && p->mRow == row) return p;
    return nullptr;
}

int Suns(Board* b)
{
    int n = 0; Coin* c = nullptr;
    while (b->IterateCoins(c)) if (!c->mDead && c->IsSun()) ++n;
    return n;
}

bool PlantWithSDL(LawnApp* a, SeedType type, int col, int row)
{
    Board* b = a->mBoard;
    if (At(b, type, col, row))
    { if (s.plantType == (int)type) { s.plantType = -1; s.plantStep = 0; } return true; }
    if (s.plantType != (int)type) { s.plantType = (int)type; s.plantStep = 0; }
    SeedPacket* packet = nullptr;
    for (int i = 0; i < b->mSeedBank->mNumPackets; ++i)
        if (b->mSeedBank->mSeedPackets[i].mPacketType == type) packet = &b->mSeedBank->mSeedPackets[i];
    if (!Check(a, packet != nullptr, "weather SDL plant is missing from its real chosen bank")) return false;
    const int ox = a->mDDInterface->mWideScreenOffsetX, oy = a->mDDInterface->mWideScreenOffsetY;
    const int px = ox + b->mSeedBank->mX + packet->mX + packet->mOffsetX + packet->mWidth / 2;
    const int py = oy + b->mSeedBank->mY + packet->mY + packet->mHeight / 2;
    const int lx = ox + b->GridToPixelX(col, row) + 40, ly = oy + b->GridToPixelY(col, row) + 45;
    if (s.plantStep == 0) { Motion(a, px, py); s.plantTick = GetTickCount(); s.plantStep = 1; }
    else if (s.plantStep == 1 && GetTickCount() - s.plantTick >= 150U)
    { if (Click(a, px, py)) s.plantStep = 2; }
    else if (s.plantStep == 2 && b->mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK && b->mCursorObject->mType == type)
    { Motion(a, lx, ly); s.plantTick = GetTickCount(); s.plantStep = 3; }
    else if (s.plantStep == 3 && GetTickCount() - s.plantTick >= 150U)
    { if (Click(a, lx, ly)) s.plantStep = 4; }
    return false;
}

uint32_t WeatherHash(Board* b)
{
    NetEndlessWeatherState w;
    w.mDayPhase = b->mNetDayPhase; w.mWeatherTimer = b->mNetWeatherTimer;
    w.mFogTimer = b->mNetFogTimer; w.mStormTimer = b->mNetStormTimer;
    w.mNightBlend = b->mNetNightBlend; w.mStormFlashKind = b->mNetStormFlashKind;
    w.mStormFlashCounter = b->mNetStormFlashCounter;
    for (size_t x = 0; x < NET_ENDLESS_FOG_COLUMNS; ++x)
        for (size_t y = 0; y < NET_ENDLESS_FOG_ROWS; ++y)
            w.mFogGrid[x * NET_ENDLESS_FOG_ROWS + y] = (uint8_t)b->mGridCelFog[x][y];
    NetBuffer data;
    if (!NetEncodeEndlessWeather(w, data, true)) return 0;
    data.WriteI32((int)(b->mFogOffset * 2)); data.WriteI32(b->mFogBlownCountDown);
    return NetFnv1a32(data.Data(), data.Size());
}

bool MusicProbe(LawnApp* a)
{
    Music* m = a->mMusic;
    if (!Check(a, m && m->mMusicInterface && std::abs(a->mMusicVolume - s.userVolume) < 0.000001,
        "weather changed user music volume or has no native music backend")) return false;
    const float expectedGain = std::abs(2.0f * a->mBoard->mNetVisualNightBlend / 1000.0f - 1.0f);
    if (!Check(a, std::abs(m->mWeatherGain - expectedGain) < 0.025f, "weather audio gain disagrees with the rendered blend")) return false;
    BassMusicInterface* backend = static_cast<BassMusicInterface*>(m->mMusicInterface);
    const bool paused = a->mBoard->mPaused;
    if (!Check(a, m->mPaused == paused, "native weather board/music pause states disagree")) return false;
    int channels = 0;
    for (MusicFile file : {m->mCurMusicFileMain, m->mCurMusicFileDrums, m->mCurMusicFileHihats})
    {
        if (file <= MUSIC_FILE_NONE || file >= NUM_MUSIC_FILES) continue;
        const auto found = backend->mMusicMap.find((int)file);
        if (!Check(a, found != backend->mMusicMap.end(), "weather music channel is absent from the actual backend")) return false;
        const double expected = m->mLayerVolumes[file] * m->mWeatherGain;
        if (!Check(a, gBass != nullptr, "native weather audio backend is unavailable")) return false;
        const DWORD active = gBass->BASS_ChannelIsActive(found->second.GetHandle());
        if (paused)
        {
            // Native GameMusicPause stops the modules. StopMusic's cached
            // mVolume=0 does not rewrite the stopped channel's VOL attribute.
            if (!Check(a, active == BASS_ACTIVE_STOPPED || active == BASS_ACTIVE_PAUSED,
                "native weather pause left an audio channel playing")) return false;
        }
        else
        {
            float volume = -1;
            if (!Check(a, active == BASS_ACTIVE_PLAYING && std::abs(found->second.mVolume - expected) < 0.002 &&
                gBass->BASS_ChannelGetAttribute(found->second.GetHandle(), BASS_ATTRIB_VOL, &volume) &&
                std::abs(volume - expected) < 0.01, "weather native playing volume is compounded or differs from its layer gain")) return false;
        }
        ++channels;
    }
    if (!Check(a, channels > 0, "weather has no actual playing music channels")) return false;
    NetTrace("SMOKE_WEATHER_AUDIO role=%s stage=%d visual=%.1f gain=%.4f channels=%d backend=1 userVolumeUnchanged=1 paused=%d",
        a->NetIsHost() ? "host" : "client", s.stage, a->mBoard->mNetVisualNightBlend, m->mWeatherGain, channels, paused ? 1 : 0);
    return true;
}

int Luma(LawnApp* a)
{
    if (!a->mCoopSmokeFrame) return -1;
    Uint8 r = 0, g = 0, b = 0, alpha = 0;
    const int x = a->mDDInterface->mWideScreenOffsetX + 550;
    const int y = a->mDDInterface->mWideScreenOffsetY + 150;
    if (!SDL_ReadSurfacePixel(a->mCoopSmokeFrame.get(), x, y, &r, &g, &b, &alpha)) return -1;
    return ((int)r + g + b) / 3;
}

bool Checkpoint(LawnApp* a, const char* name, bool hostReady)
{
    Board* b = a->mBoard;
    const bool host = a->NetIsHost();
    const std::string ack = std::string("WA_") + name;
    if (!host && s.captured && Chat(a, ack, 1)) return true;
    if (!host && !b->mPaused && !s.resumeProbed && b->mNetWeatherPresentationInitialized)
    { if (!MusicProbe(a)) return false; s.resumeProbed = true; }
    if (host)
    {
        if (!hostReady && !s.frozen) return false;
        if (!s.frozen) { b->Pause(true); s.frozenHash = WeatherHash(b); s.frozen = true; }
        if (!Check(a, s.frozenHash && WeatherHash(b) == s.frozenHash, "paused authoritative weather/grid advanced")) return false;
    }
    else
    {
        for (const auto& line : a->mNetManager->mChatLines)
        {
            const std::string prefix = std::string("WS_") + name + " ";
            if (line.mSlot != 0 || line.mText.find(prefix) != 0) continue;
            std::istringstream in(line.mText.substr(prefix.size())); in >> s.frozenHash;
        }
        if (!s.frozenHash || !b->mPaused || WeatherHash(b) != s.frozenHash) return false;
    }
    if (!s.observed) { s.observed = GetTickCount(); s.frame = a->mCoopSmokeFrame; return false; }
    if (GetTickCount() - s.observed < 250U || a->mCoopSmokeFrame == s.frame) return false;
    if (!s.captured)
    {
        if (!MusicProbe(a) || !Check(a, a->CoopSmokeCaptureFrame((std::string("weather_") + name).c_str()), "weather renderer capture failed")) return false;
        if (std::string(name) == "storm_dark" && !Check(a, Luma(a) >= 0 && Luma(a) <= 3, "native storm did not black out the actual field")) return false;
        if (std::string(name) == "storm_flash" && !Check(a, Luma(a) > 15, "native storm flash did not reveal the actual field")) return false;
        s.captured = true;
        NetTrace("SMOKE_WEATHER_CHECK role=%s step=%s hash=%u blend=%u visual=%.1f fog=%u storm=%u hole=%d far=%d wind=%d offset=%.2f luma=%d capture=1 native=1",
            host ? "host" : "client", name, s.frozenHash, b->mNetNightBlend, b->mNetVisualNightBlend,
            b->mNetFogTimer, b->mNetStormTimer, b->mGridCelFog[6][2], b->mGridCelFog[8][5],
            b->mFogBlownCountDown, b->mFogOffset, Luma(a));
    }
    if (host)
    {
        Send(a, std::string("WS_") + name + " " + std::to_string(s.frozenHash));
        if (Chat(a, ack, 1)) { b->Pause(false); return MusicProbe(a); }
    }
    else { Send(a, ack); if (Chat(a, ack, 1)) return true; }
    return false;
}

bool Stable(bool condition)
{
    if (!condition) { s.stable = 0; return false; }
    if (!s.stable) s.stable = GetTickCount();
    return GetTickCount() - s.stable >= 300U;
}

void Sample(LawnApp* a, bool descending)
{
    Board* b = a->mBoard;
    const int bucket = b->mNetNightBlend / 100;
    if (bucket == s.sampleBucket) return;
    s.sampleBucket = bucket;
    if (MusicProbe(a)) NetTrace("SMOKE_WEATHER_BLEND direction=%s blend=%u night=%d gain=%.4f native=1",
        descending ? "day" : "night", b->mNetNightBlend, b->StageIsNight() ? 1 : 0, a->mMusic->mWeatherGain);
}
}

namespace LawnNet
{
bool NetSmokeUpdateEndlessWeather(LawnApp* a)
{
    const char* mode = getenv("PVZ_COOP_SMOKE_WEATHER");
    if (!a->CoopSmokeActive() || !mode || !*mode) return false;
    if (a->mCoopSmokeResultWritten) return true;
    const bool st = !strcmp(mode, "ST"), host = a->mCoopSmokeManifest.IsHost();
    if (!Check(a, st || !strcmp(mode, "T"), "unknown weather smoke mode")) return true;
    if (!a->mNetManager || !a->mPlayerInfo || a->mNetManager->GetPlayerCount() != 2) return true;
    if (s.stage == 0)
    {
        if (!a->mGameSelector || a->mGameSelector->mSelectorState != SELECTOR_IDLE) return true;
        if (host && !s.prepared)
        {
            a->mPlayerInfo->SetLevel(50); a->mPlayerInfo->mFinishedAdventure = 1;
            a->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] = 4;
            a->mNetManager->SetDifficulty(1); a->NetBroadcastProfile(); s.prepared = true;
        }
        if (a->mPlayerInfo->mLevel != 50 || a->mPlayerInfo->mFinishedAdventure != 1 ||
            a->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] != 4) return true;
        if (!host) Send(a, "WEATHER_PROFILE_READY");
        if (!Chat(a, "WEATHER_PROFILE_READY", 1)) return true;
        a->mCoopSmokeGameStarted = true; a->mCoopSmokePeerReady = true; s.userVolume = a->mMusicVolume;
        Next();
        if (host) { a->KillGameSelector(); a->PreNewGame(st ? GAMEMODE_COOP_ENDLESS_ST : GAMEMODE_COOP_ENDLESS_T, false, 1); }
        return true;
    }
    const SeedType desired[2][5] = {{SEED_PLANTERN, SEED_BLOVER, SEED_SUNFLOWER, SEED_PUFFSHROOM, SEED_WALLNUT},
        {SEED_PEASHOOTER, SEED_REPEATER, SEED_CHERRYBOMB, SEED_POTATOMINE, SEED_GARLIC}};
    if (s.stage == 1)
    {
        SeedChooserScreen* c = a->mSeedChooserScreen;
        if (c && c->mMouseVisible)
        {
            if (s.picked < 5)
            {
                ChosenSeed& seed = c->mChosenSeeds[desired[host ? 0 : 1][s.picked]];
                if (seed.mSeedState == SEED_IN_BANK) ++s.picked;
                else if (seed.mSeedState == SEED_IN_CHOOSER && !c->mSeedsInFlight)
                    Click(a, c->mX + seed.mX + 25, c->mY + seed.mY + 35);
            }
            else if (c->CoopCanReady() && c->CoopAllPicksComplete() && !a->mNetManager->IsMyReady())
            {
                GameButton* button = c->mStartButton;
                if (!button->mDisabled && !button->mBtnNoDraw)
                    Click(a, button->mX + button->mWidth / 2, button->mY + button->mHeight / 2);
            }
        }
        if (a->mGameScene != SCENE_PLAYING) return true;
        if (!Check(a, a->mGameMode == (st ? GAMEMODE_COOP_ENDLESS_ST : GAMEMODE_COOP_ENDLESS_T) && a->mBoard->IsEndlessT(),
            "weather peer entered the wrong actual native mode")) return true;
        NetTrace("SMOKE_WEATHER_MODE role=%s actual=%s sdlChooser=1 quota=5 native=1", host ? "host" : "client", mode);
        Next();
    }
    if (s.stage == 13)
    {
        if (!s.setup) { a->KillBoard(); a->ShowGameSelector(); s.setup = true; s.observed = GetTickCount(); return true; }
        if (GetTickCount() - s.observed < 300U) return true;
        if (!Check(a, a->mMusic && std::abs(a->mMusic->mWeatherGain - 1.0f) < 0.001f &&
            std::abs(a->mMusicVolume - s.userVolume) < 0.000001, "weather gain remained stuck after native board/menu teardown")) return true;
        NetTrace("SMOKE_WEATHER_COMPLETE role=%s actual=%s checkpoints=11 menuGain=1 userVolumeUnchanged=1 natural60sCycle=0 heardAudio=0",
            host ? "host" : "client", mode);
        a->CoopSmokeWriteResult(true, nullptr); return true;
    }
    Board* b = a->mBoard;
    if (!b || a->mGameScene != SCENE_PLAYING) return true;
    if (host) b->mZombieCountDown = b->mZombieCountDownStart = 100000; // weather fixture, no combat campaign claim
    bool ready = false; const char* name = "";
    if (s.stage == 2)
    {
        name = "day";
        if (host)
        {
            if (!s.setup) { b->mSunMoney = 1000; s.setup = true; NetTrace("SMOKE_WEATHER_SETUP sun=1000 isolated=1 combatCountdown=100000 boundaryTimers=1"); }
            if (!PlantWithSDL(a, SEED_SUNFLOWER, 0, 1) || !PlantWithSDL(a, SEED_PUFFSHROOM, 1, 1)) return true;
            Plant* shroom = At(b, SEED_PUFFSHROOM, 1, 1); Plant* producer = At(b, SEED_SUNFLOWER, 0, 1);
            if (!s.frozen)
            {
                const int before = Suns(b); b->mNumSunsFallen = 0; b->mSunCountDown = 1; b->UpdateSunSpawning();
                if (!Check(a, Suns(b) > before && shroom->mIsAsleep, "native day sky-sun or mushroom sleep failed")) return true;
                const int produced = Suns(b); producer->mLaunchCounter = 1; producer->UpdateProductionPlant();
                if (!Check(a, Suns(b) > produced, "real SDL sunflower did not produce native sun")) return true;
            }
            ready = true;
        }
    }
    else if (s.stage == 3 || s.stage == 5)
    {
        const bool reverse = s.stage == 5; name = reverse ? "reverse_mid" : "mid";
        if (host)
        {
            if (!s.setup) { b->mNetWeatherTimer = 0; s.setup = true; return true; }
            // Isolate backdrop/audio measurements from independent random weather.
            b->mNetFogTimer = b->mNetStormTimer = 0; b->mNetStormFlashKind = 0; b->mNetStormFlashCounter = 0;
            Sample(a, reverse);
            ready = reverse ? b->mNetNightBlend <= 496 : b->mNetNightBlend >= 500;
            if (ready && !s.frozen)
            {
                Plant* shroom = At(b, SEED_PUFFSHROOM, 1, 1);
                const int before = Suns(b); b->mSunCountDown = 1; b->UpdateSunSpawning();
                if (!Check(a, shroom && shroom->mIsAsleep == reverse && (reverse ? Suns(b) > before : Suns(b) == before),
                    "native midpoint did not switch mushroom sleep and sky-sun rules")) return true;
            }
        }
    }
    else if (s.stage == 4 || s.stage == 6)
    {
        name = s.stage == 4 ? "night" : "day_return";
        if (host) { Sample(a, s.stage == 6); ready = Stable(b->mNetNightBlend == (s.stage == 4 ? 1000 : 0)); }
    }
    else if (s.stage == 7)
    {
        name = "fog";
        if (host)
        {
            if (!s.setup) { b->mNetFogTimer = 3000; s.setup = true; NetTrace("SMOKE_WEATHER_EPISODE fog=3000 storm=0 controlled=1"); }
            ready = b->mGridCelFog[8][5] >= 250;
        }
    }
    else if (s.stage == 8)
    {
        name = "plantern";
        if (host) ready = PlantWithSDL(a, SEED_PLANTERN, 6, 2) && b->mGridCelFog[6][2] == 0 && b->mGridCelFog[8][5] >= 200;
    }
    else if (s.stage == 9)
    {
        name = "blover";
        if (host)
        {
            if (!s.setup) s.setup = PlantWithSDL(a, SEED_BLOVER, 0, 3);
            ready = s.setup && b->mFogBlownCountDown >= 2000 && b->mFogOffset > 500.0f;
        }
    }
    else if (s.stage == 10)
    {
        name = "fog_return";
        if (host)
        {
            if (!s.setup) { b->mNetFogTimer = 0; b->mFogBlownCountDown = 2; s.setup = true; return true; }
            if (b->mFogBlownCountDown || b->mFogOffset != 0) return true;
            if (!b->mNetFogTimer) { b->mNetFogTimer = 3000; NetTrace("SMOKE_WEATHER_WIND inactiveExpiry=1 countdownSetup=2 offset=0 native=1"); }
            ready = b->mGridCelFog[8][5] >= 200 && b->mGridCelFog[6][2] == 0;
        }
    }
    else if (s.stage == 11)
    {
        name = "storm_dark";
        if (host)
        {
            if (!s.setup)
            {
                b->mNetFogTimer = 3000; b->mNetStormTimer = 2000;
                b->mNetStormFlashKind = 0; b->mNetStormFlashCounter = 0; s.setup = true;
                NetTrace("SMOKE_WEATHER_EPISODE fog=3000 storm=2000 overlap=1 counterSetup=0 nativeSchedule=1"); return true;
            }
            ready = b->mNetStormFlashKind && b->mNetStormFlashCounter > 310 && Luma(a) >= 0 && Luma(a) <= 3;
        }
    }
    else if (s.stage == 12)
    {
        name = "storm_flash";
        if (host)
        {
            if (!s.setup)
            {
                b->mNetStormFlashCounter = 0; // compress the end of an existing nonzero-kind native cycle
                b->UpdateEndlessWeather(); s.setup = true;
                if (!Check(a, b->mNetStormFlashKind > 0 && b->mNetStormFlashCounter >= 350 && b->mNetStormFlashCounter <= 900,
                    "native nonzero-kind/zero-counter boundary did not schedule the next bounded flash")) return true;
                NetTrace("SMOKE_WEATHER_FLASH_BOUNDARY counterSetup=0 kindNonzero=1 nativeRescheduled=1");
            }
            ready = Stable(Luma(a) > 15 && b->mNetStormFlashCounter < 300);
        }
    }
    if (*name && Checkpoint(a, name, ready)) Next();
    return true;
}
}
