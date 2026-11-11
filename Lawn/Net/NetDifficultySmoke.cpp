#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../Zombie.h"
#include "../System/PlayerInfo.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace LawnNet;
namespace
{
struct FamilyCase { const char* mName; GameMode mMode; int mLevel; };
const FamilyCase kFamilies[] = {
    {"waves", GAMEMODE_COOP_TOWER_DEFENSE, 1},
    {"whack", GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE, 1},
    {"boss", GAMEMODE_CHALLENGE_FINAL_BOSS, 1},
    {"match", GAMEMODE_CHALLENGE_BEGHOULED, 1},
    {"aquarium", GAMEMODE_CHALLENGE_ZOMBIQUARIUM, 1},
    {"vases", GAMEMODE_SCARY_POTTER_2, 1},
    {"izombie", GAMEMODE_PUZZLE_I_ZOMBIE_1, 1},
    {"squirrel", GAMEMODE_CHALLENGE_SQUIRREL, 1},
    {"finite", GAMEMODE_CHALLENGE_BOMB_ALL_TOGETHER, 1}
};
struct FamilyMetrics
{
    int mWaves = 0, mEnemies = 0, mCadence = 0, mGoal = 0;
    int mBody = 0, mArmor = 0, mGroup = 0, mHints = 0, mLifetime = 0;
};
struct DifficultyState
{
    bool mPrepared = false;
    int mCase = 0, mStage = 0, mCaptured = -1;
    uint32_t mTick = 0, mLastSend = 0;
    std::shared_ptr<SDL_Surface> mFrame;
    int mObservedStage = -1;
    uint32_t mObservedTick = 0;
    std::shared_ptr<SDL_Surface> mObservedFrame;
    FamilyMetrics mStock;
    uint16_t mPacket = 0, mSpawn = 0, mPlantPacket = 0;
    int mPlantPrice = 0;
    int mPrice = 0, mBrains = 0, mBeforeZombies = 0;
    int mAttack = 0, mSun = 0, mSky = 0;
    Zombie* mWalker = nullptr;
    Plant* mShooter = nullptr;
    Plant* mProducer = nullptr;
    float mStockDistance = 0.0f;
    bool mClicked = false;
};
DifficultyState s;
struct DiggerState
{
    int mStage = 0, mCapturedStage = -1, mObservedStage = -1;
    int mBrains = 0, mKills = 0;
    uint32_t mStarted = 0, mObserved = 0, mLastSend = 0;
    ZombieID mZombie = ZOMBIEID_NULL;
    bool mDone = false, mOpeningChecked = false, mBelowThirty = false, mRose = false, mStunned = false;
    float mWalkX = 0.0f;
    std::shared_ptr<SDL_Surface> mFrame;
};
DiggerState digger;

bool Require(LawnApp* app, bool condition, const char* reason)
{
    if (!condition) app->CoopSmokeFail(reason);
    return condition;
}

int LiveZombies(Board* b)
{
    int count = 0;
    Zombie* z = nullptr;
    while (b->IterateZombies(z)) if (!z->IsDeadOrDying() && z->IsOnBoard()) ++count;
    return count;
}

Zombie* FindZombie(Board* b, ZombieType type, int row = -1)
{
    Zombie* z = nullptr;
    while (b->IterateZombies(z))
        if (!z->IsDeadOrDying() && z->IsOnBoard() && z->mZombieType == type && (row < 0 || z->mRow == row)) return z;
    return nullptr;
}

int SunCoins(Board* b)
{
    int count = 0; Coin* c = nullptr;
    while (b->IterateCoins(c)) if (!c->mDead && c->IsSun()) ++count;
    return count;
}

bool SawChat(NetManager* net, int slot, const std::string& text)
{
    for (const auto& line : net->mChatLines) if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

void SendPeriodically(NetManager* net, const std::string& text)
{
    const uint32_t now = GetTickCount();
    if (!s.mLastSend || now - s.mLastSend >= 800U) { net->SendChat(text); s.mLastSend = now; }
}

bool Capture(LawnApp* app, const std::string& tag)
{
    if (s.mCaptured == s.mStage) return true;
    if (GetTickCount() - s.mTick < 200U || app->mCoopSmokeFrame == s.mFrame) return false;
    if (!app->CoopSmokeCaptureFrame(tag.c_str())) return false;
    s.mCaptured = s.mStage;
    return true;
}

bool ObservedFrameSettled(LawnApp* app, bool condition)
{
    if (!condition) { s.mObservedStage = -1; s.mObservedFrame.reset(); return false; }
    if (s.mObservedStage != s.mStage)
    {
        s.mObservedStage = s.mStage; s.mObservedTick = GetTickCount();
        s.mObservedFrame = app->mCoopSmokeFrame; return false;
    }
    return GetTickCount() - s.mObservedTick >= 200U && app->mCoopSmokeFrame && app->mCoopSmokeFrame != s.mObservedFrame;
}

void Next(LawnApp* app)
{
    ++s.mStage; s.mTick = GetTickCount(); s.mFrame = app->mCoopSmokeFrame;
    s.mLastSend = 0; s.mClicked = false;
}

bool MeasureFamily(LawnApp* app, const FamilyCase& test, int difficulty, FamilyMetrics& out)
{
    app->KillBoard();
    app->mNetManager->SetDifficulty(difficulty);
    Sexy::SRand(8000U + static_cast<unsigned int>(test.mMode));
    app->PreNewGame(test.mMode, false, test.mLevel);
    Board* b = app->mBoard;
    if (!Require(app, b && b->mChallenge && b->mCutScene, "family fixture failed native board creation")) return false;
    b->mCutScene->PreloadResources();
    app->KillSeedChooserScreen();
    b->mCutScene->mSeedChoosing = false;
    app->mGameScene = SCENE_PLAYING;
    b->StartLevel();
    b->Move(0, 0);
    b->mSeedBank->Move(SEED_BANK_OFFSET_X_END, 0);
    Challenge* c = b->mChallenge;
    out.mWaves = b->mNumWaves;
    for (int wave = 0; wave < b->mNumWaves; ++wave)
        for (int i = 0; i < MAX_ZOMBIES_IN_WAVE && b->mZombiesInWave[wave][i] != ZOMBIE_INVALID; ++i) ++out.mEnemies;
    const std::string name(test.mName);
    if (name == "waves" || name == "finite")
    {
        b->mCurrentWave = 0; b->mZombieCountDown = 1;
        b->UpdateZombieSpawning();
        out.mCadence = b->mZombieCountDown;
        out.mGroup = LiveZombies(b);
        Zombie* z = b->AddZombieInRow(ZOMBIE_PAIL, 2, 0);
        if (!Require(app, z != nullptr, "family fixture could not spawn real stock zombie")) return false;
        out.mBody = z->mBodyHealth; out.mArmor = z->mHelmHealth;
        if (!Require(app, b->mCurrentWave == 1 && out.mGroup > 0 && out.mBody == 270 && out.mArmor == 1100,
            "PvE native wave/stock HP invariant failed")) return false;
        if (name == "finite" && !Require(app, c->mBombsLeft == 25, "finite-ammo difficulty changed the authored bomb supply")) return false;
    }
    else if (name == "whack")
    {
        if (b->GetGraveStonesCount() < 5) c->WhackAZombiePlaceGraves(5);
        b->mCurrentWave = 1; b->mZombieCountDown = 2000; c->mChallengeStateCounter = 1;
        const int before = LiveZombies(b);
        c->WhackAZombieSpawning();
        out.mGroup = LiveZombies(b) - before; out.mCadence = c->mChallengeStateCounter;
        if (!Require(app, out.mGroup == (difficulty == 1 ? 1 : 3), "Whack native spawner did not scale its group")) return false;
    }
    else if (name == "boss")
    {
        Zombie* boss = FindZombie(b, ZOMBIE_BOSS);
        if (!boss) { c->PlayBossEnter(); boss = FindZombie(b, ZOMBIE_BOSS); }
        if (!Require(app, boss != nullptr, "boss native entry did not spawn the boss")) return false;
        out.mBody = boss->mBodyHealth;
        boss->mBossMode = 0; boss->BossSpawnAttack(); out.mCadence = boss->mSummonCounter;
        const int before = LiveZombies(b);
        boss->BossSpawnContact(); out.mGroup = LiveZombies(b) - before;
        if (!Require(app, out.mGroup == (difficulty == 1 ? 1 : 2), "boss native summon contact did not scale its group")) return false;
    }
    else if (name == "match")
    {
        out.mGoal = c->GetDifficultyGoal();
        c->mChallengeScore = 74; c->BeghouledScore(2, 2, 3, true);
        if (!Require(app, app->mBoard == b && b->mLevelAwardSpawned == (difficulty == 1), "match3 native completion used the wrong difficulty goal")) return false;
        if (difficulty == 10)
        {
            c->mChallengeScore = 119; c->BeghouledScore(2, 2, 3, true);
            if (!Require(app, app->mBoard == b && b->mLevelAwardSpawned && c->mChallengeScore == 120, "match3 native new goal did not award")) return false;
        }
    }
    else if (name == "aquarium")
    {
        out.mGoal = c->GetDifficultyGoal();
        Zombie* fish = FindZombie(b, ZOMBIE_SNORKEL);
        if (!Require(app, fish != nullptr, "aquarium native start did not create fish")) return false;
        const int health = fish->mBodyHealth;
        fish->mZombieAge = 70; fish->mSummonCounter = 500;
        fish->UpdateZombiquarium();
        const int damage = health - fish->mBodyHealth;
        if (!Require(app, damage == (difficulty == 1 ? 0 : 10), "aquarium actual hunger tick did not follow difficulty")) return false;
        fish->mZombieAge = 1; fish->mSummonCounter = 1;
        Sexy::SRand(2026); fish->UpdateZombiquarium(); out.mCadence = fish->mSummonCounter;
        SeedPacket& trophy = b->mSeedBank->mSeedPackets[1];
        const int cost = b->GetSeedPacketCost(trophy);
        b->mSunMoney = cost; c->ZombiquariumPacketClicked(&trophy);
        if (!Require(app, app->mBoard == b && cost == out.mGoal && b->mSunMoney == 0 && b->mLevelAwardSpawned,
            "aquarium trophy native price/spend did not match its goal")) return false;
    }
    else if (name == "vases")
    {
        GridItem* pot = nullptr;
        while (b->IterateGridItems(pot))
            if (pot->mGridItemType == GRIDITEM_SCARY_POT && pot->mGridItemState == GRIDITEM_STATE_SCARY_POT_LEAF) ++out.mHints;
        out.mGroup = c->ScaryPotterCountPots();
        Coin* card = b->AddCoin(300, 220, COIN_USABLE_SEED_PACKET, COIN_MOTION_FROM_PLANT, SEED_PEASHOOTER);
        if (!Require(app, card != nullptr, "vase fixture could not create real usable card")) return false;
        out.mLifetime = card->GetDisappearTime();
        if (!Require(app, out.mHints == (difficulty == 1 ? 2 : 0) && out.mLifetime == (difficulty == 1 ? 1500 : 900),
            "vase native hints/lifetime did not scale")) return false;
    }
    else if (name == "izombie")
    {
        Plant* shooter = nullptr; Plant* p = nullptr;
        while (b->IteratePlants(p)) if (p->mSeedType == SEED_PEASHOOTER) { shooter = p; break; }
        if (!Require(app, shooter != nullptr, "IZombie native layout has no expected defender")) return false;
        Zombie* z = b->AddZombieInRow(ZOMBIE_PAIL, shooter->mRow, 0);
        if (!Require(app, z != nullptr, "IZombie fixture native attacker creation failed")) return false;
        z->mPosX = shooter->mX + 180.0f;
        out.mBody = z->mBodyHealth; out.mArmor = z->mHelmHealth;
        Sexy::SRand(2468); shooter->mLaunchCounter = 1; shooter->UpdateShooter();
        out.mCadence = shooter->mLaunchCounter;
        const unsigned int before = b->mProjectiles.mSize;
        shooter->Fire(z, shooter->mRow);
        if (!Require(app, b->mProjectiles.mSize > before && out.mBody == 270 && out.mArmor == 1100,
            "IZombie native defender fire or unchanged attacker HP failed")) return false;
    }
    else if (name == "squirrel")
    {
        out.mGoal = c->GetDifficultyGoal(); out.mGroup = c->SquirrelCountUncaught();
        GridItem* squirrel = nullptr; GridItem* item = nullptr;
        while (b->IterateGridItems(item)) if (item->mGridItemType == GRIDITEM_SQUIRREL &&
            item->mGridItemState != GRIDITEM_STATE_SQUIRREL_ZOMBIE) { squirrel = item; break; }
        if (!Require(app, squirrel && out.mGroup == (difficulty == 1 ? 7 : 12), "squirrel native target count did not scale")) return false;
        c->SquirrelPeek(squirrel); out.mCadence = squirrel->mGridItemCounter;
        if (!Require(app, out.mCadence == (difficulty == 1 ? 50 : 38), "squirrel native peek cadence did not scale")) return false;
    }
    if (difficulty == 10)
    {
        bool harder = false;
        if (name == "waves") harder = out.mWaves > s.mStock.mWaves && out.mEnemies > s.mStock.mEnemies && out.mCadence < s.mStock.mCadence;
        else if (name == "whack") harder = out.mGroup > s.mStock.mGroup && out.mCadence >= 65 && out.mCadence <= 130;
        else if (name == "boss") harder = out.mGroup > s.mStock.mGroup && out.mCadence < s.mStock.mCadence;
        else if (name == "match" || name == "aquarium") harder = out.mGoal > s.mStock.mGoal;
        else if (name == "vases") harder = out.mGroup == s.mStock.mGroup && out.mHints < s.mStock.mHints && out.mLifetime < s.mStock.mLifetime;
        else if (name == "izombie") harder = out.mCadence < s.mStock.mCadence;
        else if (name == "squirrel") harder = out.mGroup > s.mStock.mGroup && out.mCadence < s.mStock.mCadence;
        else if (name == "finite") harder = out.mWaves == s.mStock.mWaves && out.mCadence < s.mStock.mCadence;
        if (out.mBody && s.mStock.mBody) harder &= out.mBody == s.mStock.mBody && out.mArmor == s.mStock.mArmor;
        if (!Require(app, harder, "native family D10 did not increase its measured challenge while preserving PvE HP/resources")) return false;
    }
    b->mPaused = true;
    NetTrace("SMOKE_DIFF_FAMILY family=%s difficulty=%d waves=%d enemies=%d cadence=%d goal=%d group=%d body=%d armor=%d hints=%d lifetime=%d native=1",
        test.mName, difficulty, out.mWaves, out.mEnemies, out.mCadence, out.mGoal, out.mGroup,
        out.mBody, out.mArmor, out.mHints, out.mLifetime);
    return true;
}

bool Families(LawnApp* app)
{
    if (!s.mPrepared)
    {
        if (!Require(app, app->mCoopSmokeManifest.IsHost() && app->mNetManager->IsHosting() &&
            app->mNetManager->GetPlayerCount() == 1 && app->mPlayerInfo,
            "family initialization fixture requires one isolated host and no clients")) return true;
        app->mCoopSmokeGameStarted = true;
        app->mPlayerInfo->mFinishedAdventure = 1; app->mPlayerInfo->mLevel = 50;
        app->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] = 4;
        app->KillGameSelector(); s.mPrepared = true;
    }
    const int count = static_cast<int>(sizeof(kFamilies) / sizeof(kFamilies[0]));
    if (s.mCase >= count * 2)
    {
        NetTrace("SMOKE_DIFF_FAMILIES_COMPLETE cases=%d native=1 fullCampaign=0", count * 2);
        app->CoopSmokeWriteResult(true, nullptr); app->CloseRequestAsync(); return true;
    }
    if (s.mStage == 0)
    {
        const int difficulty = (s.mCase % 2 == 0) ? 1 : 10;
        FamilyMetrics measured;
        if (!MeasureFamily(app, kFamilies[s.mCase / 2], difficulty, measured)) return true;
        if (difficulty == 1) s.mStock = measured;
        Next(app);
    }
    else
    {
        const FamilyCase& test = kFamilies[s.mCase / 2];
        const std::string tag = std::string("difficulty_") + test.mName + (s.mCase % 2 == 0 ? "_1" : "_10");
        if (!Capture(app, tag)) return true;
        ++s.mCase; s.mStage = 0; s.mCaptured = -1;
    }
    return true;
}

void DiggerSend(NetManager* net, const char* text)
{
    if (!digger.mLastSend || GetTickCount() - digger.mLastSend >= 800U)
    { net->SendChat(text); digger.mLastSend = GetTickCount(); }
}

bool DiggerCapture(LawnApp* app, const char* name)
{
    if (digger.mCapturedStage == digger.mStage) return true;
    if (digger.mObservedStage != digger.mStage)
    {
        digger.mObservedStage = digger.mStage; digger.mObserved = GetTickCount();
        digger.mFrame = app->mCoopSmokeFrame; return false;
    }
    if (GetTickCount() - digger.mObserved < 200U || app->mCoopSmokeFrame == digger.mFrame) return false;
    if (!app->CoopSmokeCaptureFrame(name)) return false;
    digger.mCapturedStage = digger.mStage; return true;
}

bool NativeHouseEntry(LawnApp* app, Board* b, bool expectScore = true)
{
    Zombie* ordinary = b->AddZombieInRow(ZOMBIE_NORMAL, 3, 0);
    if (!Require(app, ordinary != nullptr, "house-entry fixture could not create a normal zombie")) return false;
    // Boundary setup only: movement, head/phase eligibility and house scoring
    // are performed by the stock Update/CheckForBoardEdge lifecycle.
    ordinary->mPosX = BOARD_EDGE + 2.0f; ordinary->mX = static_cast<int>(ordinary->mPosX);
    for (int i = 0; i < 100 && !ordinary->mDead; ++i)
    {
        ordinary->Update();
        if (!ordinary->mDead)
        {
            Reanimation* body = app->ReanimationTryToGet(ordinary->mBodyReanimID);
            if (body) body->Update();
        }
    }
    if (!Require(app, ordinary->mX <= BOARD_EDGE &&
        (expectScore ? (ordinary->mDead && ordinary->mNetKillCounted) : (!ordinary->mDead && !ordinary->mNetKillCounted)),
        "normal zombie did not obey the native house boundary/terminal guard")) return false;
    b->NetPvPZombieScored(ordinary); b->NetPvPZombieScored(ordinary);
    if (!expectScore) ordinary->DieNoLoot(); // remove while the completed kill goal still blocks further points
    return true;
}

bool GoalOrders(LawnApp* app, Board* b)
{
    const int savedKills = b->mNetPvPKillScore, savedBrains = b->mNetPvPBrainScore;
    for (bool killFirst : {true, false})
    {
        b->mNetPvPKillScore = 99; b->mNetPvPBrainScore = 4;
        Zombie* victim = b->AddZombieInRow(ZOMBIE_NORMAL, 0, 0);
        if (!Require(app, victim != nullptr, "first-goal fixture could not create a victim")) return false;
        if (killFirst) victim->DieWithLoot();
        if (!NativeHouseEntry(app, b, !killFirst)) return false;
        if (!killFirst) victim->DieWithLoot();
        if (!Require(app, b->mNetPvPKillScore == (killFirst ? 100 : 99) &&
            b->mNetPvPBrainScore == (killFirst ? 4 : 5), "PvP did not preserve the first native goal edge")) return false;
        NetTrace("SMOKE_DIFF_PVP_GOAL_ORDER first=%s kills=%d brains=%d nativeDeath=1 nativeHouse=1",
            killFirst ? "kill" : "house", b->mNetPvPKillScore, b->mNetPvPBrainScore);
    }
    // The fixture verifies both edges atomically before the next world update;
    // it does not open two terminal screens or advance the global simulation.
    b->mNetPvPKillScore = savedKills; b->mNetPvPBrainScore = savedBrains;
    return true;
}

bool DiggerRegression(LawnApp* app)
{
    if (digger.mDone) return true;
    Board* b = app->mBoard; NetManager* net = app->mNetManager;
    const bool host = app->mCoopSmokeManifest.IsHost();
    if (!digger.mOpeningChecked)
    {
        if (!host && !app->mCoopSmokeSnapshotsApplied) return false;
        const SeedPacket& normal = b->mSeedBank->mSeedPackets[PVP_PLANT_PACKET_COUNT];
        int bungeeCost = 0;
        for (int i = PVP_PLANT_PACKET_COUNT; i < b->mSeedBank->mNumPackets; ++i)
            if (b->mSeedBank->mSeedPackets[i].mPacketType == SEED_ZOMBIE_BUNGEE)
                bungeeCost = b->GetSeedPacketCost(b->mSeedBank->mSeedPackets[i]);
        if (!Require(app, b->mNetPvPBrains == 25 && b->mSunMoney == 50 &&
            b->GetSeedPacketCost(normal) == 25 && b->CanTakeBrains(25) && !b->CanTakeBrains(50) &&
            bungeeCost == 62 && !b->CanTakeBrains(bungeeCost),
            "PvP native opening was not 25 shared brains, 50 plant sun and one basic zombie")) return false;
        digger.mOpeningChecked = true;
        NetTrace("SMOKE_DIFF_PVP_OPENING role=%s brains=25 sun=50 normalCost=25 basicAffordable=1 twoBasicsAffordable=0 native=1 bungeeCost=62 bungeeAffordable=0",
            host ? "host" : "client");
    }
    if (!digger.mStarted) digger.mStarted = GetTickCount();
    if (!Require(app, GetTickCount() - digger.mStarted < 60000U, "native Digger lifecycle/peer acknowledgment timed out")) return false;
    if (host)
    {
        if (digger.mStage == 0)
        {
            Zombie* z = b->AddZombieInRow(ZOMBIE_DIGGER, 4, 0);
            if (!Require(app, z && z->mZombiePhase == PHASE_DIGGER_TUNNELING && z->mHasObject,
                "Digger fixture did not start in its actual stock tunneling phase")) return false;
            z->mPosX = 35.0f; z->mX = 35;
            digger.mZombie = b->ZombieGetID(z); digger.mBrains = b->mNetPvPBrainScore; digger.mKills = b->mNetPvPKillScore;
            digger.mStage = 1;
        }
        Zombie* z = b->ZombieTryToGet(digger.mZombie);
        if (digger.mStage <= 3)
        {
            if (!Require(app, z && !z->mDead && b->mNetPvPBrainScore == digger.mBrains && b->mNetPvPKillScore == digger.mKills,
                "Digger tunneling/emergence/walk-back incorrectly scored or died")) return false;
            if (z->mZombiePhase == PHASE_DIGGER_TUNNELING && z->mPosX < 30.0f) digger.mBelowThirty = true;
            if (z->mZombiePhase == PHASE_DIGGER_RISING) digger.mRose = true;
            if (z->mZombiePhase == PHASE_DIGGER_STUNNED) digger.mStunned = true;
        }
        if (digger.mStage == 1 && z->mZombiePhase == PHASE_DIGGER_RISING)
        {
            if (!Require(app, digger.mBelowThirty && z->mPosX < 10.0f, "Digger bypassed its native emergence point")) return false;
            NetTrace("SMOKE_DIFF_PVP_DIGGER step=emerge x=%.3f phase=%d brains=%d kills=%d native=1 boundarySetup=35",
                z->mPosX, static_cast<int>(z->mZombiePhase), b->mNetPvPBrainScore, b->mNetPvPKillScore);
            digger.mStage = 2;
        }
        if (digger.mStage == 2 && z->mZombiePhase == PHASE_DIGGER_WALKING)
        {
            if (!Require(app, digger.mRose && digger.mStunned && z->IsWalkingBackwards(), "Digger skipped its native rise/stun/turn sequence")) return false;
            digger.mWalkX = z->mPosX; digger.mStage = 3;
        }
        if (digger.mStage == 3 && z->mPosX > digger.mWalkX + 1.0f)
        {
            if (!DiggerCapture(app, "difficulty_digger_walking")) return false;
            DiggerSend(net, "DIFF_DIGGER_WALKING");
            if (!SawChat(net, 1, "DIFF_DIGGER_SEEN")) return false;
            const int oldBrains = b->mNetPvPBrainScore, oldKills = b->mNetPvPKillScore;
            z->mPosX = 851.0f; z->mX = 851; z->CheckForBoardEdge();
            if (!Require(app, z->mDead && b->mNetPvPBrainScore == oldBrains && b->mNetPvPKillScore == oldKills,
                "Digger native return exit counted as a brain or combat kill")) return false;
            if (!NativeHouseEntry(app, b)) return false;
            if (!Require(app, b->mNetPvPBrainScore == oldBrains + 1 && b->mNetPvPKillScore == oldKills,
                "normal native house entry was not exactly one brain and zero plant kills")) return false;
            if (!GoalOrders(app, b)) return false;
            NetTrace("SMOKE_DIFF_PVP_DIGGER step=complete brainsDelta=0 killsDelta=0 normalHouse=1 goalOrders=2 native=1");
            digger.mStage = 4; digger.mLastSend = 0;
        }
        if (digger.mStage == 4)
        {
            DiggerSend(net, "DIFF_DIGGER_DONE");
            if (SawChat(net, 1, "DIFF_DIGGER_RELEASE")) digger.mDone = true;
        }
    }
    else
    {
        if (digger.mStage == 0)
        {
            Zombie* z = FindZombie(b, ZOMBIE_DIGGER, 4);
            if (!z || z->mZombiePhase != PHASE_DIGGER_WALKING || !z->IsWalkingBackwards() ||
                !SawChat(net, 0, "DIFF_DIGGER_WALKING")) return false;
            digger.mBrains = b->mNetPvPBrainScore; digger.mKills = b->mNetPvPKillScore;
            if (!Require(app, digger.mBrains == 0 && digger.mKills == 0, "client Digger walk-back already changed the objectives")) return false;
            if (!DiggerCapture(app, "difficulty_digger_walking_mirror")) return false;
            NetTrace("SMOKE_DIFF_PVP_DIGGER role=client step=walking x=%.3f phase=%d brains=0 kills=0 width=%d height=%d mirror=1",
                z->mPosX, static_cast<int>(z->mZombiePhase), app->mWidth, app->mHeight);
            digger.mStage = 1; digger.mLastSend = 0;
        }
        if (digger.mStage == 1)
        {
            DiggerSend(net, "DIFF_DIGGER_SEEN");
            if (SawChat(net, 0, "DIFF_DIGGER_DONE") && b->mNetPvPBrainScore == digger.mBrains + 1 &&
                b->mNetPvPKillScore == digger.mKills)
            { digger.mStage = 2; digger.mLastSend = 0; }
        }
        if (digger.mStage == 2)
        {
            DiggerSend(net, "DIFF_DIGGER_RELEASE");
            if (SawChat(net, 1, "DIFF_DIGGER_RELEASE")) digger.mDone = true;
        }
    }
    return digger.mDone;
}

// Controlled score histories call the production PvP update without advancing
// the global simulation, other entities, or the game's network clocks.
void History(Board* b, int kills, int brains)
{
    const int currency = b->mNetPvPBrains, tick = b->mNetPvPBrainTick;
    b->mNetPvPKillScore = kills; b->mNetPvPBrainScore = brains;
    for (int i = 0; i < 12000; ++i) b->UpdatePvP();
    b->mNetPvPBrains = currency; b->mNetPvPBrainTick = tick;
}

bool Mouse(LawnApp* app, int x, int y, bool click)
{
    x += app->mDDInterface ? app->mDDInterface->mWideScreenOffsetX : 0;
    y += app->mDDInterface ? app->mDDInterface->mWideScreenOffsetY : 0;
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = static_cast<float>(x); motion.motion.y = static_cast<float>(y);
    if (!SDL_PushEvent(&motion)) return false;
    if (!click) return true;
    for (bool down : {true, false})
    {
        SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = SDL_BUTTON_LEFT;
        e.button.down = down; e.button.clicks = 1; e.button.x = static_cast<float>(x); e.button.y = static_cast<float>(y);
        if (!SDL_PushEvent(&e)) return false;
    }
    return true;
}

struct BungeeState
{
    int mStage = 0, mCase = 0, mCost = 0, mBefore = 0, mAwardAfter = 0;
    int mKills = 0, mBrains = 0, mEaten = 0;
    uint16_t mPacket = 0;
    ZombieID mZombie = ZOMBIEID_NULL;
    uint32_t mStarted = 0, mTick = 0, mLastSend = 0, mObserved = 0;
    bool mClicked = false, mCaptured = false, mDone = false;
    std::shared_ptr<SDL_Surface> mFrame;
};
BungeeState bungee;

SeedPacket* BungeePacket(Board* b)
{
    for (int i = PVP_PLANT_PACKET_COUNT; i < b->mSeedBank->mNumPackets; ++i)
        if (b->mSeedBank->mSeedPackets[i].mNetId == bungee.mPacket) return &b->mSeedBank->mSeedPackets[i];
    return nullptr;
}

void BungeeNext(int stage)
{
    bungee.mStage = stage; bungee.mTick = GetTickCount(); bungee.mLastSend = 0;
    bungee.mObserved = 0; bungee.mCaptured = false; bungee.mClicked = false; bungee.mFrame.reset();
}

void BungeeSend(NetManager* net, const std::string& text)
{
    if (!bungee.mLastSend || GetTickCount() - bungee.mLastSend >= 800U)
    { net->SendChat(text); bungee.mLastSend = GetTickCount(); }
}

bool BungeeCapture(LawnApp* app, const std::string& name)
{
    if (bungee.mCaptured) return true;
    if (!bungee.mObserved)
    { bungee.mObserved = GetTickCount(); bungee.mFrame = app->mCoopSmokeFrame; return false; }
    if (GetTickCount() - bungee.mObserved < 200U || app->mCoopSmokeFrame == bungee.mFrame) return false;
    if (!app->CoopSmokeCaptureFrame(name.c_str())) return false;
    bungee.mCaptured = true; return true;
}

bool BungeeReadBuy(NetManager* net)
{
    for (const auto& line : net->mChatLines)
    {
        const std::string prefix = "DIFF_BUNGEE_BUY" + std::to_string(bungee.mCase) + " ";
        if (line.mSlot != 0 || line.mText.find(prefix) != 0) continue;
        int packet = 0; std::istringstream in(line.mText.substr(prefix.size()));
        if (in >> packet >> bungee.mCost >> bungee.mBefore)
        { bungee.mPacket = static_cast<uint16_t>(packet); return true; }
    }
    return false;
}

bool BungeeRegression(LawnApp* app)
{
    if (bungee.mDone) return true;
    Board* b = app->mBoard; NetManager* net = app->mNetManager;
    const bool host = app->mCoopSmokeManifest.IsHost();
    if (!bungee.mStarted) bungee.mStarted = GetTickCount();
    if (!Require(app, GetTickCount() - bungee.mStarted < 60000U, "PvP Bungee SDL/native lifecycle timed out")) return false;
    const int col = bungee.mCase == 0 ? 4 : 6, row = bungee.mCase == 0 ? 2 : 1;
    const std::string suffix = std::to_string(bungee.mCase);
    const std::string frame = bungee.mCase == 0 ? "difficulty_bungee_target" : "difficulty_bungee_empty";
    auto DecoysIntact = [&]() {
        return b->GetTopPlantAt(0, 0, TOPPLANT_BUNGEE_ORDER) && b->GetTopPlantAt(7, 3, TOPPLANT_BUNGEE_ORDER);
    };
    if (host)
    {
        if (bungee.mStage == 0)
        {
            for (int i = PVP_PLANT_PACKET_COUNT; i < b->mSeedBank->mNumPackets; ++i)
                if (b->mSeedBank->mSeedPackets[i].mPacketType == SEED_ZOMBIE_BUNGEE)
                { bungee.mPacket = b->mSeedBank->mSeedPackets[i].mNetId; bungee.mCost = b->GetSeedPacketCost(b->mSeedBank->mSeedPackets[i]); }
            if (!Require(app, bungee.mPacket && bungee.mCost == 62 &&
                b->NewPlant(4, 2, SEED_WALLNUT, SEED_NONE) && b->NewPlant(0, 0, SEED_WALLNUT, SEED_NONE) &&
                b->NewPlant(7, 3, SEED_WALLNUT, SEED_NONE), "Bungee target/decoy fixture setup failed")) return false;
            const int before = b->mNetPvPBrains;
            // Compress only the timer to its last native tick. No currency is
            // assigned, and this is not a ten-second wall-clock assertion.
            b->mNetPvPBrainTick = 999; b->UpdatePvP(); bungee.mAwardAfter = b->mNetPvPBrains;
            if (!Require(app, bungee.mAwardAfter == before + 25 && b->mNetPvPBrainTick == 0,
                "PvP native passive award was not exactly 25")) return false;
            NetTrace("SMOKE_DIFF_PVP_PASSIVE role=host before=%d after=%d delta=25 timerSetup=999 native=1", before, bungee.mAwardAfter);
            BungeeNext(1);
        }
        if (bungee.mStage == 1)
        {
            BungeeSend(net, "DIFF_BUNGEE_PASSIVE " + std::to_string(bungee.mAwardAfter));
            if (!SawChat(net, 1, "DIFF_BUNGEE_PASSIVE_SEEN")) return false;
            BungeeNext(2);
        }
        if (bungee.mStage == 2)
        {
            for (int i = 0; b->mNetPvPBrains < bungee.mCost && i < 3; ++i)
            { b->mNetPvPBrainTick = 999; b->UpdatePvP(); }
            if (!Require(app, b->mNetPvPBrains >= bungee.mCost && DecoysIntact(), "Bungee fixture could not earn its native price or lost a decoy")) return false;
            bungee.mBefore = b->mNetPvPBrains; bungee.mKills = b->mNetPvPKillScore;
            bungee.mBrains = b->mNetPvPBrainScore; bungee.mEaten = b->mPlantsEaten;
            BungeeNext(3);
        }
        if (bungee.mStage == 3)
        {
            BungeeSend(net, "DIFF_BUNGEE_BUY" + suffix + " " + std::to_string(bungee.mPacket) + " " +
                std::to_string(bungee.mCost) + " " + std::to_string(bungee.mBefore));
            Zombie* z = FindZombie(b, ZOMBIE_BUNGEE, row);
            if (!z) return false;
            if (!Require(app, z->mTargetCol == col && std::abs(z->mPosX - b->GridToPixelX(col, row)) < 0.01f &&
                b->mNetPvPBrains == bungee.mBefore - bungee.mCost, "Bungee SDL purchase did not bind the clicked cell or exact host price")) return false;
            if (z->mAltitude > BUNGEE_ZOMBIE_HEIGHT - 450.0f || !BungeeCapture(app, frame)) return false;
            bungee.mZombie = b->ZombieGetID(z);
            NetTrace("SMOKE_DIFF_PVP_BUNGEE role=host case=%d step=placed col=%d row=%d x=%.2f cost=%d before=%d after=%d sdl=1 native=1",
                bungee.mCase, col, row, z->mPosX, bungee.mCost, bungee.mBefore, b->mNetPvPBrains);
            BungeeNext(4);
        }
        if (bungee.mStage == 4)
        {
            Zombie* z = b->ZombieTryToGet(bungee.mZombie);
            if (z && !z->mDead) return false;
            if (!Require(app, DecoysIntact() && !b->GetTopPlantAt(col, row, TOPPLANT_BUNGEE_ORDER) &&
                b->mPlantsEaten == bungee.mEaten + (bungee.mCase == 0 ? 1 : 0) &&
                b->mNetPvPKillScore == bungee.mKills && b->mNetPvPBrainScore == bungee.mBrains,
                "Bungee stole a different cell, retargeted an empty cell or counted a noncombat exit")) return false;
            NetTrace("SMOKE_DIFF_PVP_BUNGEE role=host case=%d step=complete targetRemoved=%d decoys=2 fakeKills=0 native=1",
                bungee.mCase, bungee.mCase == 0 ? 1 : 0);
            BungeeNext(5);
        }
        if (bungee.mStage == 5)
        {
            BungeeSend(net, "DIFF_BUNGEE_DONE" + suffix);
            if (!SawChat(net, 1, "DIFF_BUNGEE_ACK" + suffix)) return false;
            if (bungee.mCase++ == 0) BungeeNext(2); else BungeeNext(6);
        }
        if (bungee.mStage == 6)
        {
            BungeeSend(net, "DIFF_BUNGEE_FINISH");
            if (SawChat(net, 1, "DIFF_BUNGEE_FINISH_ACK")) bungee.mDone = true;
        }
    }
    else
    {
        if (bungee.mStage == 0)
        {
            for (const auto& line : net->mChatLines)
                if (line.mSlot == 0 && line.mText.find("DIFF_BUNGEE_PASSIVE ") == 0)
                { std::istringstream in(line.mText.substr(20)); in >> bungee.mAwardAfter; }
            if (!bungee.mAwardAfter || b->mNetPvPBrains != bungee.mAwardAfter) return false;
            NetTrace("SMOKE_DIFF_PVP_PASSIVE role=client after=%d delta=25 mirror=1", bungee.mAwardAfter);
            BungeeNext(1);
        }
        if (bungee.mStage == 1)
        {
            BungeeSend(net, "DIFF_BUNGEE_PASSIVE_SEEN");
            if (!BungeeReadBuy(net) || b->mNetPvPBrains != bungee.mBefore) return false;
            SeedPacket* packet = BungeePacket(b);
            if (!Require(app, packet && packet->mPacketType == SEED_ZOMBIE_BUNGEE &&
                b->GetSeedPacketCost(*packet) == bungee.mCost && packet->CanPickUp(), "client Bungee packet quote/ownership was invalid")) return false;
            if (!Require(app, Mouse(app, b->mSeedBank->mX + packet->mX + packet->mOffsetX + packet->mWidth / 2,
                b->mSeedBank->mY + packet->mY + packet->mHeight / 2, false), "Bungee SDL card motion failed")) return false;
            BungeeNext(2);
        }
        if (bungee.mStage == 2)
        {
            if (!bungee.mClicked && GetTickCount() - bungee.mTick >= 150U)
            {
                SeedPacket* packet = BungeePacket(b);
                if (!Require(app, packet && Mouse(app, b->mSeedBank->mX + packet->mX + packet->mOffsetX + packet->mWidth / 2,
                    b->mSeedBank->mY + packet->mY + packet->mHeight / 2, true), "Bungee SDL card pick failed")) return false;
                bungee.mClicked = true;
            }
            if (b->mCursorObject->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK || b->mCursorObject->mType != SEED_ZOMBIE_BUNGEE) return false;
            if (!Require(app, Mouse(app, b->GridToPixelX(col, row) + 40, b->GridToPixelY(col, row) + 45, false), "Bungee SDL target motion failed")) return false;
            BungeeNext(3);
        }
        if (bungee.mStage == 3)
        {
            if (!bungee.mClicked && GetTickCount() - bungee.mTick >= 150U)
            {
                if (!Require(app, Mouse(app, b->GridToPixelX(col, row) + 40, b->GridToPixelY(col, row) + 45, true), "Bungee SDL target click failed")) return false;
                bungee.mClicked = true;
            }
            Zombie* z = FindZombie(b, ZOMBIE_BUNGEE, row);
            if (!z || !z->mNetGhost || z->mAltitude > BUNGEE_ZOMBIE_HEIGHT - 450.0f) return false;
            if (!Require(app, std::abs(z->mPosX - b->GridToPixelX(col, row)) < 0.01f &&
                b->mNetPvPBrains == bungee.mBefore - bungee.mCost, "client Bungee target position or spend did not mirror authority")) return false;
            if (!BungeeCapture(app, frame + "_mirror")) return false;
            NetTrace("SMOKE_DIFF_PVP_BUNGEE role=client case=%d step=placed col=%d row=%d x=%.2f mirror=1 sdl=1", bungee.mCase, col, row, z->mPosX);
            BungeeNext(4);
        }
        if (bungee.mStage == 4)
        {
            if (!SawChat(net, 0, "DIFF_BUNGEE_DONE" + suffix)) return false;
            if (!DecoysIntact() || b->GetTopPlantAt(col, row, TOPPLANT_BUNGEE_ORDER)) return false;
            if (!BungeeCapture(app, frame + "_complete_mirror")) return false;
            NetTrace("SMOKE_DIFF_PVP_BUNGEE role=client case=%d step=complete targetRemoved=%d decoys=2 mirror=1", bungee.mCase, bungee.mCase == 0 ? 1 : 0);
            BungeeNext(5);
        }
        if (bungee.mStage == 5)
        {
            BungeeSend(net, "DIFF_BUNGEE_ACK" + suffix);
            if (bungee.mCase == 0)
            {
                bungee.mCase = 1;
                if (BungeeReadBuy(net)) BungeeNext(1); else bungee.mCase = 0;
            }
            else if (SawChat(net, 0, "DIFF_BUNGEE_FINISH")) BungeeNext(6);
        }
        if (bungee.mStage == 6)
        {
            BungeeSend(net, "DIFF_BUNGEE_FINISH_ACK");
            if (SawChat(net, 1, "DIFF_BUNGEE_FINISH_ACK")) bungee.mDone = true;
        }
    }
    return bungee.mDone;
}

bool PvP(LawnApp* app, int difficulty)
{
    NetManager* net = app->mNetManager;
    const bool host = app->mCoopSmokeManifest.IsHost();
    if (!s.mPrepared)
    {
        if (host && net->IsHosting() && !app->mBoard) { net->SetDifficulty(difficulty); s.mPrepared = true; }
        else if (!host) s.mPrepared = true;
    }
    if (!app->mBoard || app->mGameScene != SCENE_PLAYING) return false;
    Board* b = app->mBoard;
    if (!Require(app, b->IsPvPMode() && net->GetDifficulty() == difficulty, "adaptive fixture requires the matching native PvP scenario/difficulty")) return true;
    if (!DiggerRegression(app)) return true;
    if (!BungeeRegression(app)) return true;
    if (s.mStage && GetTickCount() - s.mTick > 15000U) { app->CoopSmokeFail("adaptive fixture peer/SDL acknowledgment timed out"); return true; }
    const int zombieSide = difficulty == 1 ? PVP_SUPPORT_NONE : PVP_SUPPORT_ZOMBIES;
    const int plantSide = difficulty == 1 ? PVP_SUPPORT_NONE : PVP_SUPPORT_PLANTS;
    const int strength = difficulty == 1 ? 0 : 1000;
    if (host)
    {
        if (s.mStage == 0)
        {
            if (app->mCoopSmokeSnapshotsSent < 1 || b->mSeedBank->mNumPackets <= PVP_PLANT_PACKET_COUNT ||
                b->mSeedBank->mSeedPackets[PVP_PLANT_PACKET_COUNT].mNetId == 0) return true;
            s.mWalker = b->AddZombieInRow(ZOMBIE_CATAPULT, 4, 0);
            s.mShooter = b->NewPlant(1, 0, SEED_PEASHOOTER, SEED_NONE);
            s.mProducer = b->NewPlant(1, 1, SEED_SUNFLOWER, SEED_NONE);
            if (!Require(app, s.mWalker && s.mShooter && s.mProducer, "adaptive native fixture entity creation failed")) return true;
            s.mWalker->mPosX = 700.0f; s.mWalker->mVelX = 0.25f;
            s.mWalker->UpdateZombieWalking(); s.mStockDistance = 700.0f - s.mWalker->mPosX;
            Sexy::SRand(2468); s.mShooter->mLaunchCounter = 1; s.mShooter->UpdateShooter(); s.mAttack = s.mShooter->mLaunchCounter;
            Sexy::SRand(1234); s.mProducer->mLaunchCounter = 1; s.mProducer->UpdateProductionPlant(); s.mSun = s.mProducer->mLaunchCounter;
            b->mNumSunsFallen = 0; b->mSunCountDown = 1; Sexy::SRand(2222); b->UpdateSunSpawning(); s.mSky = b->mSunCountDown;
            const int oldHealth = s.mWalker->mBodyHealth;
            History(b, 80, 0);
            if (!Require(app, b->mNetPvPBalance.mBeneficiary == zombieSide && b->mNetPvPBalance.mStrength == strength &&
                s.mWalker->mBodyHealth == oldHealth, "adaptive zombie support did not activate through production update or healed an existing unit")) return true;
            Zombie* z = b->AddZombieInRow(ZOMBIE_PAIL, 2, 0);
            if (!Require(app, z && z->mBodyHealth == (difficulty == 1 ? 270 : 324) &&
                z->mBodyMaxHealth == z->mBodyHealth && z->mHelmHealth == (difficulty == 1 ? 1100 : 1320) &&
                z->mHelmMaxHealth == z->mHelmHealth, "adaptive real spawn body/armor/max HP was wrong")) return true;
            z->mBodyHealth -= 20; const int damaged = z->mBodyHealth;
            History(b, 80, 0);
            if (!Require(app, z->mBodyHealth == damaged, "repeated adaptive updates healed or compounded existing HP")) return true;
            float distance = 0;
            for (int i = 0; i < 3; ++i)
            {
                s.mWalker->mPosX = 700.0f; s.mWalker->UpdateZombieWalking();
                const float step = 700.0f - s.mWalker->mPosX;
                if (i == 0) distance = step;
                if (!Require(app, std::abs(step - distance) < 0.0001f && s.mWalker->mVelX == 0.25f,
                    "adaptive real movement compounded the underlying velocity")) return true;
            }
            if (!Require(app, s.mStockDistance > 0 && std::abs(distance / s.mStockDistance - (difficulty == 1 ? 1.0f : 1.15f)) < 0.002f,
                "adaptive actual walking distance did not have the bounded speed factor")) return true;
            SeedPacket& packet = b->mSeedBank->mSeedPackets[PVP_PLANT_PACKET_COUNT];
            s.mPacket = packet.mNetId; s.mPrice = b->GetSeedPacketCost(packet);
            if (!Require(app, packet.mPacketType == SEED_ZOMBIE_NORMAL && s.mPrice == (difficulty == 1 ? 25 : 20),
                "adaptive real zombie packet did not receive the intended discount")) return true;
            b->mNetPvPBrains = s.mBrains = 500; b->mNetPvPBrainTick = 0; s.mBeforeZombies = LiveZombies(b);
            NetTrace("SMOKE_DIFF_PVP_ZOMBIES role=host difficulty=%d side=%d strength=%d body=%d armor=%d speed=%.4f noncompound=1 noheal=1 price=%d native=1",
                difficulty, zombieSide, strength, z->mBodyMaxHealth, z->mHelmHealth, distance / s.mStockDistance, s.mPrice);
            Next(app);
        }
        if (s.mStage == 1)
        {
            if (!Capture(app, "difficulty_zombie_support")) return true;
            SendPeriodically(net, "DIFF_BUY " + std::to_string(s.mPacket) + " " + std::to_string(s.mPrice) + " " + std::to_string(s.mBrains));
            Zombie* purchased = FindZombie(b, ZOMBIE_NORMAL, 3);
            if (!purchased || !purchased->mNetId || !SawChat(net, 1, "DIFF_SPEND_MIRRORED")) return true;
            if (!Require(app, b->mNetPvPBrains == s.mBrains - s.mPrice && LiveZombies(b) > s.mBeforeZombies,
                "adaptive SDL purchase did not deduct the exact host packet price")) return true;
            s.mSpawn = purchased->mNetId;
            NetTrace("SMOKE_DIFF_PVP_SPEND role=host difficulty=%d packet=%u zombie=%u cost=%d before=%d after=%d sdl=1 native=1",
                difficulty, s.mPacket, s.mSpawn, s.mPrice, s.mBrains, b->mNetPvPBrains);
            History(b, 0, 4);
            if (!Require(app, b->mNetPvPBalance.mBeneficiary == plantSide && b->mNetPvPBalance.mStrength == strength,
                "adaptive plant support did not activate through production update")) return true;
            const int coins = SunCoins(b);
            Sexy::SRand(2468); s.mShooter->mLaunchCounter = 1; s.mShooter->UpdateShooter(); const int attack = s.mShooter->mLaunchCounter;
            const unsigned int projectiles = b->mProjectiles.mSize;
            s.mShooter->Fire(purchased, s.mShooter->mRow);
            Sexy::SRand(1234); s.mProducer->mLaunchCounter = 1; s.mProducer->UpdateProductionPlant(); const int sun = s.mProducer->mLaunchCounter;
            b->mNumSunsFallen = 0; b->mSunCountDown = 1; Sexy::SRand(2222); b->UpdateSunSpawning(); const int sky = b->mSunCountDown;
            if (!Require(app, b->mProjectiles.mSize > projectiles && SunCoins(b) >= coins + 2 &&
                (difficulty == 1 ? (attack == s.mAttack && sun == s.mSun && sky == s.mSky) :
                    (attack < s.mAttack && sun < s.mSun && sky < s.mSky)), "adaptive native fire/sun/sky reset did not change as expected")) return true;
            const PvPBalanceState frozen = b->mNetPvPBalance; const int tick = b->mNetPvPBrainTick;
            b->mPaused = true; for (int i = 0; i < 1000; ++i) b->UpdatePvP(); b->mPaused = false;
            if (!Require(app, b->mNetPvPBalance.mStrength == frozen.mStrength && b->mNetPvPBalance.mFilteredLead == frozen.mFilteredLead &&
                b->mNetPvPBrainTick == tick, "paused native PvP advanced adaptive or economy clocks")) return true;
            NetTrace("SMOKE_DIFF_PVP_PLANTS role=host difficulty=%d side=%d strength=%d attack=%d stockAttack=%d sun=%d stockSun=%d sky=%d stockSky=%d projectile=1 sunCoins=1 pauseFrozen=1 native=1",
                difficulty, plantSide, strength, attack, s.mAttack, sun, s.mSun, sky, s.mSky);
            SeedPacket* plantPacket = nullptr;
            for (int i = 0; i < PVP_PLANT_PACKET_COUNT; ++i)
                if (b->mSeedBank->mSeedPackets[i].mPacketType != SEED_NONE &&
                    b->GetCurrentPlantCost(b->mSeedBank->mSeedPackets[i].mPacketType, b->mSeedBank->mSeedPackets[i].mImitaterType) > 0)
                { plantPacket = &b->mSeedBank->mSeedPackets[i]; break; }
            if (!Require(app, plantPacket != nullptr, "adaptive fixture has no real priced plant packet")) return true;
            const int plantBase = b->GetCurrentPlantCost(plantPacket->mPacketType, plantPacket->mImitaterType);
            s.mPlantPacket = plantPacket->mNetId; s.mPlantPrice = b->GetSeedPacketCost(*plantPacket);
            if (!Require(app, s.mPlantPrice == (difficulty == 1 ? plantBase : (plantBase * 80 + 99) / 100),
                "adaptive actual plant packet price did not receive the bounded discount")) return true;
            Next(app);
        }
        if (s.mStage == 2)
        {
            if (!Capture(app, "difficulty_plant_support")) return true;
            SendPeriodically(net, "DIFF_PLANTS_READY " + std::to_string(s.mPlantPacket) + " " + std::to_string(s.mPlantPrice));
            if (!SawChat(net, 1, "DIFF_PLANTS_MIRRORED")) return true;
            b->mNetPvPKillScore = 100; b->mNetPvPBrainScore = 0; b->UpdatePvP();
            const PvPBalanceState frozen = b->mNetPvPBalance; const int tick = b->mNetPvPBrainTick;
            for (int i = 0; i < 1000; ++i) b->UpdatePvP();
            if (!Require(app, b->mBoardFadeOutCounter >= 0 && b->mLevelAwardSpawned &&
                b->mNetPvPBalance.mStrength == frozen.mStrength && b->mNetPvPBalance.mFilteredLead == frozen.mFilteredLead &&
                b->mNetPvPBrainTick == tick, "native PvP terminal did not freeze adaptive/economy state")) return true;
            NetTrace("SMOKE_DIFF_PVP_TERMINAL role=host difficulty=%d kills=100 fade=%d frozen=1 native=1", difficulty, b->mBoardFadeOutCounter);
            Next(app);
        }
        if (s.mStage == 3)
        {
            if (!Capture(app, "difficulty_terminal")) return true;
            SendPeriodically(net, "DIFF_TERMINAL_READY");
            if (SawChat(net, 1, "DIFF_DONE")) app->CoopSmokeWriteResult(true, nullptr);
        }
    }
    else
    {
        if (s.mStage == 0)
        {
            for (const auto& line : net->mChatLines)
            {
                if (line.mSlot != 0 || line.mText.find("DIFF_BUY ") != 0) continue;
                std::istringstream in(line.mText.substr(9)); int packet = 0;
                if (in >> packet >> s.mPrice >> s.mBrains) s.mPacket = static_cast<uint16_t>(packet);
            }
            if (!s.mPacket || b->mNetPvPBalance.mBeneficiary != zombieSide || b->mNetPvPBalance.mStrength != strength || b->mNetPvPBrains != s.mBrains) return true;
            int index = -1;
            for (int i = PVP_PLANT_PACKET_COUNT; i < b->mSeedBank->mNumPackets; ++i) if (b->mSeedBank->mSeedPackets[i].mNetId == s.mPacket) index = i;
            if (index < 0) return true;
            SeedPacket& packet = b->mSeedBank->mSeedPackets[index];
            Zombie* mirror = FindZombie(b, ZOMBIE_PAIL, 2);
            if (!mirror) return true;
            if (!Require(app, mirror->mBodyMaxHealth == (difficulty == 1 ? 270 : 324) &&
                mirror->mHelmMaxHealth == (difficulty == 1 ? 1100 : 1320), "adaptive client did not mirror native spawn/max HP")) return true;
            if (!Require(app, packet.mPacketType == SEED_ZOMBIE_NORMAL && b->GetSeedPacketCost(packet) == s.mPrice,
                "adaptive client packet price did not match authoritative quote")) return true;
            NetTrace("SMOKE_DIFF_PVP_ZOMBIES role=client difficulty=%d side=%d strength=%d body=%d armor=%d price=%d mirror=1",
                difficulty, zombieSide, strength, mirror->mBodyMaxHealth, mirror->mHelmMaxHealth, s.mPrice);
            const int x = b->mSeedBank->mX + packet.mX + packet.mOffsetX + packet.mWidth / 2;
            const int y = b->mSeedBank->mY + packet.mY + packet.mHeight / 2;
            if (!Require(app, Mouse(app, x, y, false), "adaptive SDL card motion failed")) return true;
            Next(app);
        }
        if (s.mStage == 1)
        {
            if (!s.mClicked && GetTickCount() - s.mTick >= 150U)
            {
                SeedPacket* packet = nullptr;
                for (int i = PVP_PLANT_PACKET_COUNT; i < b->mSeedBank->mNumPackets; ++i) if (b->mSeedBank->mSeedPackets[i].mNetId == s.mPacket) packet = &b->mSeedBank->mSeedPackets[i];
                if (!packet) return true;
                if (!Require(app, Mouse(app, b->mSeedBank->mX + packet->mX + packet->mOffsetX + packet->mWidth / 2,
                    b->mSeedBank->mY + packet->mY + packet->mHeight / 2, true), "adaptive SDL card click failed")) return true;
                s.mClicked = true;
            }
            const int held = b->mCursorObject->mSeedBankIndex;
            if (b->mCursorObject->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK || held < 0 || held >= b->mSeedBank->mNumPackets ||
                b->mSeedBank->mSeedPackets[held].mNetId != s.mPacket) return true;
            if (!Require(app, Mouse(app, b->GridToPixelX(6, 3) + 40, b->GridToPixelY(6, 3) + 45, false), "adaptive SDL lawn motion failed")) return true;
            Next(app);
        }
        if (s.mStage == 2)
        {
            if (!s.mClicked && GetTickCount() - s.mTick >= 150U)
            {
                if (!Require(app, Mouse(app, b->GridToPixelX(6, 3) + 40, b->GridToPixelY(6, 3) + 45, true), "adaptive SDL lawn purchase failed")) return true;
                s.mClicked = true;
            }
            Zombie* purchased = FindZombie(b, ZOMBIE_NORMAL, 3);
            if (!ObservedFrameSettled(app, purchased && purchased->mNetId && b->mNetPvPBrains == s.mBrains - s.mPrice &&
                b->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL)) return true;
            if (!Capture(app, "difficulty_purchase_mirror")) return true;
            NetTrace("SMOKE_DIFF_PVP_SPEND role=client difficulty=%d packet=%u zombie=%u cost=%d before=%d after=%d sdl=1 mirror=1",
                difficulty, s.mPacket, purchased->mNetId, s.mPrice, s.mBrains, b->mNetPvPBrains);
            net->SendChat("DIFF_SPEND_MIRRORED"); Next(app);
        }
        if (s.mStage == 3)
        {
            // The real chat transport enforces a 750ms rate limit. Keep the
            // previous acknowledgment alive until the next host phase arrives.
            SendPeriodically(net, "DIFF_SPEND_MIRRORED");
            for (const auto& line : net->mChatLines)
                if (line.mSlot == 0 && line.mText.find("DIFF_PLANTS_READY ") == 0)
                {
                    std::istringstream in(line.mText.substr(18)); int id = 0;
                    if (in >> id >> s.mPlantPrice) s.mPlantPacket = static_cast<uint16_t>(id);
                }
            if (!ObservedFrameSettled(app, s.mPlantPacket && b->mNetPvPBalance.mBeneficiary == plantSide &&
                b->mNetPvPBalance.mStrength == strength)) return true;
            SeedPacket* packet = nullptr;
            for (int i = 0; i < PVP_PLANT_PACKET_COUNT; ++i)
                if (b->mSeedBank->mSeedPackets[i].mNetId == s.mPlantPacket) packet = &b->mSeedBank->mSeedPackets[i];
            if (!Require(app, packet && b->GetSeedPacketCost(*packet) == s.mPlantPrice, "adaptive client plant price did not mirror host")) return true;
            if (!Capture(app, "difficulty_plant_support_mirror")) return true;
            NetTrace("SMOKE_DIFF_PVP_PLANTS role=client difficulty=%d side=%d strength=%d packet=%u price=%d mirror=1", difficulty, plantSide, strength, s.mPlantPacket, s.mPlantPrice);
            net->SendChat("DIFF_PLANTS_MIRRORED"); Next(app);
        }
        if (s.mStage == 4)
        {
            SendPeriodically(net, "DIFF_PLANTS_MIRRORED");
            if (!ObservedFrameSettled(app, SawChat(net, 0, "DIFF_TERMINAL_READY") && b->mNetPvPKillScore == 100 &&
                b->mBoardFadeOutCounter >= 0)) return true;
            if (!Capture(app, "difficulty_terminal_mirror")) return true;
            NetTrace("SMOKE_DIFF_PVP_TERMINAL role=client difficulty=%d kills=100 fade=%d mirror=1", difficulty, b->mBoardFadeOutCounter);
            Next(app);
        }
        if (s.mStage == 5)
        {
            SendPeriodically(net, "DIFF_DONE");
            if (SawChat(net, 1, "DIFF_DONE")) app->CoopSmokeWriteResult(true, nullptr);
        }
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateDifficulty(LawnApp* app)
{
    const char* mode = getenv("PVZ_COOP_SMOKE_DIFFICULTY");
    if (!mode || !*mode || !app->CoopSmokeActive()) return false;
    if (std::string(mode) == "families") return Families(app);
    if (std::string(mode) == "pvp1") return PvP(app, 1);
    if (std::string(mode) == "pvp10") return PvP(app, 10);
    app->CoopSmokeFail("unknown difficulty smoke fixture");
    return true;
}
}
