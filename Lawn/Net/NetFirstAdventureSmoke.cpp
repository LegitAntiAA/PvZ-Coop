#include "../../LawnApp.h"
#include "../../Resources.h"
#include "NetManager.h"
#include "../Board.h"
#include "../LawnCommon.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../CutScene.h"
#include "../MessageWidget.h"
#include "../SeedPacket.h"
#include "../System/PlayerInfo.h"
#include "../System/ProfileMgr.h"
#include "../Widget/AwardScreen.h"
#include "../Widget/GameSelector.h"
#include "../Widget/ChallengeScreen.h"
#include "../Widget/StoreScreen.h"
#include "../Widget/UserDialog.h"
#include "../Widget/GameButton.h"
#include "../Widget/NewOptionsDialog.h"
#include "../Widget/MoreSettingsDialog.h"
#include "../Widget/NetOnlineDialog.h"
#include "../../SexyAppFramework/ListWidget.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/DDImage.h"
#include "../../SexyAppFramework/Slider.h"
#include "../../SexyAppFramework/Checkbox.h"
#include "../../SexyAppFramework/TextEncoding.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/ReanimAtlas.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include <cstdlib>
#include <cmath>

using namespace LawnNet;

namespace
{
struct FirstAdventureState
{
    int mStage = 0;
    int mLoggedStage = -1;
    int mObservedStage = -1;
    bool mProfileChecked = false, mDemoSeen = false, mSodSeen = false, mFlashSeen = false;
    bool mBaselineFrozen = false, mAwardSeen = false, mFixtureSpawned = false, mFixtureKilled = false;
    bool mLongLabelChecked = false;
    bool mGithubChecked = false;
    uint32_t mSodTick = 0, mFlashTick = 0, mFixtureTick = 0, mRayTick = 0;
    uint32_t mLastAckTick = 0;
    uint32_t mObservedTick = 0;
    uint32_t mPreviewTick = 0;
    float mSodTime = 0.0f;
    float mPreviewTime = 0.0f;
    int mFlashGray = -1;
    uint16_t mFixtureId = 0;
    SexyString mOriginalName;
};

FirstAdventureState& FreshState()
{
    // Each isolated smoke process owns exactly one LawnApp/run.
    static FirstAdventureState aState;
    return aState;
}

bool ChatHas(LawnApp* theApp, int theSlot, const std::string& theText)
{
    for (const auto& aLine : theApp->mNetManager->mChatLines)
        if (aLine.mSlot == theSlot && aLine.mText == theText) return true;
    return false;
}

bool Acknowledge(LawnApp* theApp, const char* theTag)
{
    const std::string aText = std::string("FRESH_ACK_") + theTag;
    if (ChatHas(theApp, 1, aText)) return true;
    if (theApp->mCoopSmokeManifest.IsClient())
    {
        FirstAdventureState& s = FreshState();
        const uint32_t aNow = GetTickCount();
        // Exercise the ordinary chat channel without exceeding its 750 ms
        // per-player limit when consecutive UI observations are close together.
        if (s.mLastAckTick && aNow - s.mLastAckTick < 800U) return false;
        // Advance locally before the host can close this screen. Its echo and
        // the following scene command may be handled in one network update.
        s.mLastAckTick = aNow;
        theApp->mNetManager->SendChat(aText);
        return true;
    }
    return false;
}

bool Capture(LawnApp* theApp, const char* theName)
{
    return theApp->CoopSmokeCaptureFrame((std::string("fresh_") + theName).c_str());
}

bool Record(LawnApp* theApp, const char* theTag)
{
    FirstAdventureState& s = FreshState();
    if (s.mObservedStage != s.mStage)
    {
        s.mObservedStage = s.mStage;
        s.mObservedTick = GetTickCount();
    }
    // The hook precedes Redraw. Keep the observed state alive across several
    // renders before capturing its actual pixels and acknowledging the stage.
    if (GetTickCount() - s.mObservedTick < 150U) return false;
    if (s.mLoggedStage != s.mStage)
    {
        if (!Capture(theApp, theTag))
        {
            theApp->CoopSmokeFail("first-adventure render-target capture failed");
            return false;
        }
        NetTrace("SMOKE_FRESH_CHECK role=%s step=%s stage=%d scene=%d level=%d profileLevel=%d finished=%d capture=1",
            theApp->mCoopSmokeManifest.IsHost() ? "host" : "client", theTag, s.mStage,
            (int)theApp->mGameScene, theApp->mBoard ? theApp->mBoard->mLevel : -1,
            theApp->mPlayerInfo->mLevel, theApp->mPlayerInfo->mFinishedAdventure);
        s.mLoggedStage = s.mStage;
    }
    return Acknowledge(theApp, theTag);
}

bool PushClick(LawnApp* theApp, int theX, int theY)
{
    for (bool aDown : { true, false })
    {
        SDL_Event anEvent{};
        anEvent.type = aDown ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        anEvent.button.windowID = SDL_GetWindowID(theApp->mSDLWindow);
        anEvent.button.button = SDL_BUTTON_LEFT;
        anEvent.button.down = aDown;
        anEvent.button.clicks = 1;
        anEvent.button.x = (float)theX;
        anEvent.button.y = (float)theY;
        if (!SDL_PushEvent(&anEvent)) return false;
    }
    return true;
}

bool PushButton(LawnApp* theApp, NewLawnButton* theButton)
{
    return theButton && !theButton->mDisabled && theButton->mVisible &&
        PushClick(theApp, theButton->mX + theButton->mWidth / 2,
            theButton->mY + theButton->mHeight / 2);
}

bool PushDialogButton(LawnApp* theApp, Dialog* theDialog, Widget* theButton)
{
    return theDialog && theButton && !theButton->mDisabled && theButton->mVisible &&
        PushClick(theApp, theDialog->mX + theButton->mX + theButton->mWidth / 2,
            theDialog->mY + theButton->mY + theButton->mHeight / 2);
}

bool PreviewBounds(MoreSettingsDialog* theDialog, float& theCenterY)
{
    Reanimation* aReanim = theDialog->mPreviewReanim;
    if (!aReanim || !aReanim->mDefinition || !theDialog->mLawnYesButton ||
        aReanim->mDead || aReanim->mAnimRate <= 0.0f) return false;
    float aLeft = 10000.0f, aTop = 10000.0f, aRight = -10000.0f, aBottom = -10000.0f;
    int aVisibleTracks = 0;
    for (int i = 0; i < aReanim->mDefinition->mTracks.count; ++i)
    {
        if (aReanim->mTrackInstances[i].mRenderGroup != RENDER_GROUP_NORMAL ||
            aReanim->mTrackInstances[i].mForceDontRender) continue;
        ReanimatorTransform aTransform;
        aReanim->GetCurrentTransform(i, &aTransform);
        Image* anImage = aTransform.mImage;
        if (!anImage || aTransform.mFrame < 0.0f || aTransform.mAlpha <= 0.0f) continue;
        if (ReanimAtlas* anAtlas = aReanim->mDefinition->mReanimAtlas)
            if (ReanimAtlasImage* anAtlasImage = anAtlas->GetEncodedReanimAtlas(anImage))
                anImage = anAtlasImage->mOriginalImage;
        if (!anImage) continue;
        SexyTransform2D aMatrix;
        aReanim->GetTrackMatrix(i, aMatrix);
        for (float aX : { -anImage->GetCelWidth() * 0.5f, anImage->GetCelWidth() * 0.5f })
            for (float aY : { -anImage->GetCelHeight() * 0.5f, anImage->GetCelHeight() * 0.5f })
            {
                const SexyVector2 aPoint = aMatrix * SexyVector2(aX, aY);
                aLeft = min(aLeft, aPoint.x); aRight = max(aRight, aPoint.x);
                aTop = min(aTop, aPoint.y); aBottom = max(aBottom, aPoint.y);
            }
        ++aVisibleTracks;
    }
    theCenterY = (aTop + aBottom) * 0.5f;
    return aVisibleTracks > 0 && aLeft >= 2.0f && aTop >= 2.0f &&
        aRight < theDialog->mWidth - 2 && aBottom < theDialog->mLawnYesButton->mY - 2 &&
        fabsf(theCenterY - 327.0f) <= 2.0f;
}

int ParticleCount(LawnApp* theApp, ParticleEffect theType)
{
    int aCount = 0;
    TodParticleSystem* aParticle = nullptr;
    while (theApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
        if (!aParticle->mDead && aParticle->mEffectType == theType) ++aCount;
    return aCount;
}

bool SwitchProfile(LawnApp* theApp, const SexyString& theName)
{
    theApp->DoUserDialog();
    UserDialog* aDialog = (UserDialog*)theApp->GetDialog(Dialogs::DIALOG_USERDIALOG);
    if (!aDialog || !aDialog->mUserList) return false;
    for (int i = 0; i < aDialog->mUserList->GetLineCount(); i++)
        if (aDialog->mUserList->GetStringAt(i) == theName)
        {
            aDialog->mUserList->SetSelect(i);
            theApp->FinishUserDialog(true);
            return theApp->mPlayerInfo && theApp->mPlayerInfo->mName == theName;
        }
    return false;
}
}

bool LawnApp::CoopSmokeFirstAdventureEnabled() const
{
    const char* anEnabled = getenv("PVZ_COOP_SMOKE_FIRST_ADVENTURE");
    return CoopSmokeActive() && anEnabled && std::string(anEnabled) == "1";
}

bool LawnApp::CoopSmokeFirstAdventureAllowsStart(int theMode, int theLevel) const
{
    if (!CoopSmokeFirstAdventureEnabled()) return false;
    const FirstAdventureState& s = FreshState();
    return (theMode == GAMEMODE_INTRO && theLevel == 1 && s.mStage == 0) ||
        (theMode == GAMEMODE_ADVENTURE && theLevel == 1 && !s.mBaselineFrozen) ||
        (theMode == GAMEMODE_ADVENTURE && theLevel == 2 && s.mAwardSeen);
}

bool LawnApp::CoopSmokeFirstAdventureRunBaseline() const
{
    if (!CoopSmokeFirstAdventureEnabled()) return true;
    const FirstAdventureState& s = FreshState();
    return !s.mBaselineFrozen && mBoard && mGameMode == GAMEMODE_ADVENTURE && mBoard->mLevel == 1 &&
        (mGameScene == SCENE_LEVEL_INTRO || s.mStage == 2);
}

bool LawnApp::CoopSmokeUpdateFirstAdventure()
{
    if (!CoopSmokeFirstAdventureEnabled()) return true;
    FirstAdventureState& s = FreshState();
    const bool aHost = mCoopSmokeManifest.IsHost();
    const char* aRole = aHost ? "host" : "client";
    const uint32_t aNow = GetTickCount();
    if (!mPlayerInfo || !mNetManager || (NetIsClient() && !mNetBlockProfileSave)) return false;
    if (!s.mProfileChecked)
    {
        if (mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_1 ||
            mCoopSmokeManifest.mSkipIntro || mWidth != 800 || mHeight != 600 ||
            mPlayerInfo->mLevel != 1 || mPlayerInfo->mFinishedAdventure != 0)
        {
            CoopSmokeFail("fresh adventure requires a real zero-progress profile, Adventure1, stock intro and 800x600");
            return false;
        }
        s.mProfileChecked = true;
        s.mOriginalName = mPlayerInfo->mName;
        NetTrace("SMOKE_FRESH_PROFILE role=%s level=1 finished=0 source=isolated_profile", aRole);
    }
    if (s.mStage == 0)
    {
        if (mBoard && mGameMode == GAMEMODE_INTRO && mBoard->mCutScene &&
            mBoard->mCutScene->mCutsceneTime >= 8000 && !s.mDemoSeen)
        {
            int aPlants = 0, aZombies = 0;
            uint16_t aPlantId = 0, aZombieId = 0;
            bool aGhostsValid = true;
            Plant* aPlant = nullptr;
            while (mBoard->mPlants.IterateNext(aPlant))
                if (!aPlant->mDead && aPlant->mIsOnBoard && aPlant->mNetId)
                {
                    ++aPlants;
                    if (!aPlantId || aPlant->mNetId < aPlantId) aPlantId = aPlant->mNetId;
                    if (!aHost && !aPlant->mNetGhost) aGhostsValid = false;
                }
            Zombie* aZombie = nullptr;
            while (mBoard->mZombies.IterateNext(aZombie))
                if (!aZombie->mDead && aZombie->mNetId)
                {
                    ++aZombies;
                    if (!aZombieId || aZombie->mNetId < aZombieId) aZombieId = aZombie->mNetId;
                    if (!aHost && !aZombie->mNetGhost) aGhostsValid = false;
                }
            if (aPlants > 0 && aZombies > 0 && aGhostsValid && Capture(this, "intro"))
            {
                s.mDemoSeen = true;
                NetTrace("SMOKE_FRESH_INTRO role=%s plants=%d zombies=%d firstPlant=%u firstZombie=%u ghosts=%d capture=1",
                    aRole, aPlants, aZombies, aPlantId, aZombieId, aHost ? 0 : 1);
            }
        }
        if (mBoard && mGameMode == GAMEMODE_ADVENTURE)
        {
            if (!s.mDemoSeen) { CoopSmokeFail("stock fresh intro had no observed plants/zombies"); return false; }
            s.mStage = 1;
        }
        return false;
    }
    if (s.mStage == 1)
    {
        if (!mBoard || mBoard->mLevel != 1) return false;
        if (!s.mSodSeen)
        {
            Reanimation* aReanim = nullptr;
            while (mEffectSystem->mReanimationHolder->mReanimations.IterateNext(aReanim))
                if (!aReanim->mDead && aReanim->mReanimationType == REANIM_SODROLL)
                {
                    if (!s.mSodTick) { s.mSodTick = aNow; s.mSodTime = aReanim->mAnimTime; }
                    else if (aNow - s.mSodTick >= 150U && fabsf(aReanim->mAnimTime - s.mSodTime) > 0.001f)
                    {
                        if (!Capture(this, "sod")) { CoopSmokeFail("sod frame capture failed"); return false; }
                        s.mSodSeen = true;
                        NetTrace("SMOKE_FRESH_SOD role=%s active=1 advanced=1 time=%.4f capture=1", aRole, aReanim->mAnimTime);
                    }
                }
        }
        if (mGameScene != SCENE_PLAYING) return false;
        if (!s.mSodSeen) { CoopSmokeFail("fresh level1 reached gameplay without a moving sod roll"); return false; }
        const bool aFlashing = mBoard->mTutorialState == TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER &&
            mBoard->mTutorialTimer == -1;
        if (!aFlashing) return false;
        const int aGray = GetFlashingColor(mBoard->mMainCounter, 75).mRed;
        if (!s.mFlashTick) { s.mFlashTick = aNow; s.mFlashGray = aGray; }
        if (aNow - s.mFlashTick < 300U || aGray == s.mFlashGray) return false;
        s.mFlashSeen = true;
        if (Record(this, "flash")) s.mStage = 2;
        return false;
    }
    if (s.mStage == 2)
    {
        if (!mBoard || mGameScene != SCENE_PLAYING ||
            !mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots)) return false;
        const bool aFlashing = mBoard->mTutorialState == TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER &&
            mBoard->mTutorialTimer == -1;
        if (aFlashing) { CoopSmokeFail("tutorial Peashooter still flashes after authoritative planting"); return false; }
        s.mBaselineFrozen = true;
        if (Record(this, "baseline_flash_off")) s.mStage = 3;
        return false;
    }
    if (s.mStage == 3)
    {
        if (!mBoard) return false;
        if (aHost && !s.mFixtureSpawned)
        {
            // Compress only the wave boundary. A real zombie death creates the
            // award through TrySpawnLevelAward; no coin/profile/UI is injected.
            mBoard->RemoveAllZombies();
            mBoard->mCurrentWave = mBoard->mNumWaves;
            mBoard->mZombieCountDown = 5000;
            Zombie* aZombie = mBoard->AddZombieInRow(ZOMBIE_NORMAL, 2, max(0, mBoard->mNumWaves - 1));
            if (!aZombie) { CoopSmokeFail("fresh award boundary fixture failed to spawn"); return false; }
            aZombie->mPosX = 620.0f;
            aZombie->mX = 620;
            s.mFixtureId = aZombie->mNetId;
            s.mFixtureTick = aNow;
            s.mFixtureSpawned = true;
            NetTrace("SMOKE_FRESH_FINAL_WAVE role=host wave=%d zombie=%u source=boundary_fixture", mBoard->mCurrentWave, s.mFixtureId);
        }
        if (aHost && s.mFixtureSpawned && !s.mFixtureKilled && aNow - s.mFixtureTick >= 700U)
        {
            Zombie* aZombie = nullptr;
            Zombie* aCandidate = nullptr;
            while (mBoard->mZombies.IterateNext(aCandidate))
                if (!aCandidate->mDead && aCandidate->mNetId == s.mFixtureId) aZombie = aCandidate;
            if (!aZombie) { CoopSmokeFail("fresh final-wave fixture disappeared before stock death"); return false; }
            s.mFixtureKilled = true;
            aZombie->DieWithLoot();
        }
        Coin* aCoin = nullptr;
        while (mBoard->mCoins.IterateNext(aCoin))
            if (!aCoin->mDead && aCoin->mType == COIN_FINAL_SEED_PACKET && aCoin->mHitGround &&
                aCoin->GetFinalSeedPacketType() == SEED_SUNFLOWER &&
                ParticleCount(this, PARTICLE_SEED_PACKET) > 0)
            {
                if (Record(this, "award_idle_rays"))
                {
                    s.mStage = 4;
                    if (aHost && !PushClick(this, mBoard->mX + (int)aCoin->mPosX + aCoin->mWidth / 2,
                        mBoard->mY + (int)aCoin->mPosY + aCoin->mHeight / 2))
                        CoopSmokeFail("fresh sunflower collection SDL input failed");
                }
                break;
            }
        return false;
    }
    if (s.mStage == 4)
    {
        if (mAwardScreen) { CoopSmokeFail("award UI arrived before observing pickup rays"); return false; }
        if (!mBoard || ParticleCount(this, PARTICLE_SEED_PACKET_PICKUP) <= 0) return false;
        if (!s.mLongLabelChecked)
        {
            const SexyString anExpected = TodStringTranslate(_S("[ADVICE_CLICKED_ON_SUN]"));
            std::wstring aDecoded;
            if (!mBoard->mAdvice || anExpected.size() < MAX_MESSAGE_LENGTH ||
                mBoard->mAdvice->mLabel != anExpected ||
                !Sexy::TryDecodeTextBytes(mBoard->mAdvice->mLabel, Sexy::TextEncoding::UTF8, aDecoded))
            {
                CoopSmokeFail("long localized tutorial label was truncated or corrupted"); return false;
            }
            s.mLongLabelChecked = true;
            NetTrace("SMOKE_FRESH_LABEL role=%s bytes=%zu utf8=1 complete=1", aRole, anExpected.size());
        }
        if (!s.mRayTick) s.mRayTick = aNow;
        if (aNow - s.mRayTick < 150U) return false;
        if (Record(this, "award_pickup_rays")) s.mStage = 5;
        return false;
    }
    if (s.mStage == 5)
    {
        if (!mAwardScreen || mGameScene != SCENE_AWARD) return false;
        const int aSeed = GetAwardSeedForLevel(mPlayerInfo->GetLevel() - 1);
        if (mBoard || mPlayerInfo->mLevel != 2 || aSeed != SEED_SUNFLOWER ||
            mAwardScreen->mAwardType != AWARD_FORLEVEL)
        {
            CoopSmokeFail("fresh sunflower award used wrong profile/plant or retained board"); return false;
        }
        s.mAwardSeen = true;
        if (mAwardScreen->mFadeInCounter > 0) return false;
        if (Record(this, "award_sunflower"))
        {
            s.mStage = 6;
            if (aHost)
            {
                GameButton* aButton = mAwardScreen->mStartButton;
                if (!PushClick(this, aButton->mX + aButton->mWidth / 2, aButton->mY + aButton->mHeight / 2))
                    CoopSmokeFail("fresh award Continue SDL input failed");
            }
        }
        return false;
    }
    if (s.mStage == 6 || s.mStage == 22)
    {
        if (!mBoard || mGameMode != GAMEMODE_ADVENTURE || mBoard->mLevel != 2 || mGameScene != SCENE_PLAYING)
            return false;
        if (mAwardScreen || mChallengeScreen || GetDialog(DIALOG_STORE) || mPlayerInfo->mLevel != 2 ||
            mPlayerInfo->mFinishedAdventure != 0)
        {
            CoopSmokeFail("next adventure level retained previous UI or lost zero-progress profile"); return false;
        }
        const bool aFinal = s.mStage == 22;
        if (Record(this, aFinal ? "complete" : "next_level_clean"))
        {
            s.mStage = aFinal ? 23 : 7;
            if (aHost && !aFinal) DoBackToMain();
        }
        return s.mStage == 23;
    }
    if (s.mStage == 7 || s.mStage == 10 || s.mStage == 13 || s.mStage == 14)
    {
        if (!mGameSelector || mGameScene != SCENE_MENU || mBoard || mChallengeScreen || GetDialog(DIALOG_STORE))
            return false;
        if (mGameSelector->mSelectorState != SELECTOR_IDLE || mGameSelector->mSlideCounter > 0) return false;
        if (!s.mGithubChecked)
        {
            const Rect aRect = mGameSelector->GetGithubIconRect();
            if (!mGameSelector->mGithubImage || mGameSelector->mGithubImage->mWidth != 560 ||
                mGameSelector->mGithubImage->mHeight != 560 ||
                aRect.mX != 8 || aRect.mY != 8 || aRect.mWidth != 32 || aRect.mHeight != 32 ||
                aRect.mX + aRect.mWidth > mWidth || aRect.mY + aRect.mHeight > mHeight)
            {
                CoopSmokeFail("menu GitHub icon asset or visible hit rectangle is invalid"); return false;
            }
            s.mGithubChecked = true;
            NetTrace("SMOKE_FRESH_GITHUB role=%s loaded=1 source=560x560 rect=8,8,32,32", aRole);
        }
        if ((s.mStage == 7 || s.mStage == 14) && (mPlayerInfo->mLevel != 2 ||
            mPlayerInfo->mName != s.mOriginalName || mGameSelector->mMinigamesLocked ||
            mGameSelector->mPuzzleLocked || mGameSelector->mSurvivalLocked)) return false;
        const char* aTag = s.mStage == 7 ? "menu_unlocks" : s.mStage == 10 ? "challenge_closed" :
            s.mStage == 13 ? "store_closed" : "profile_restored";
        if (Record(this, aTag))
        {
            const int aPrevious = s.mStage++;
            if (aHost && aPrevious == 7)
            {
                PlayerInfo* aFixture = mProfileMgr->GetProfile(_S("FreshAlternate"));
                if (!aFixture) aFixture = mProfileMgr->AddProfile(_S("FreshAlternate"));
                if (!aFixture) { CoopSmokeFail("alternate isolated profile fixture failed"); return false; }
                aFixture->SetLevel(45);
                aFixture->mHasUnlockedMinigames = aFixture->mHasUnlockedPuzzleMode = aFixture->mHasUnlockedSurvivalMode = 1;
                if (!SwitchProfile(this, aFixture->mName)) CoopSmokeFail("stock profile selection failed");
            }
            else if (aHost && aPrevious == 10)
            {
                if (!PushButton(this, mGameSelector->mStoreButton)) CoopSmokeFail("store open SDL input failed");
            }
            else if (aHost && aPrevious == 13)
            {
                if (!SwitchProfile(this, s.mOriginalName)) CoopSmokeFail("original isolated profile restore failed");
            }
            else if (aPrevious == 14)
                DoNewOptions(true);
        }
        return false;
    }
    if (s.mStage == 8)
    {
        if (!mGameSelector || mPlayerInfo->mName != _S("FreshAlternate") || mPlayerInfo->mLevel != 45 ||
            mGameSelector->mLevel != 45 || mGameSelector->mMinigamesLocked || mGameSelector->mPuzzleLocked ||
            mGameSelector->mSurvivalLocked) return false;
        if (Record(this, "profile_switched"))
        {
            s.mStage = 9;
            if (aHost) { KillGameSelector(); ShowChallengeScreen(CHALLENGE_PAGE_CHALLENGE); }
        }
        return false;
    }
    if (s.mStage == 9)
    {
        if (!mChallengeScreen || mGameScene != SCENE_CHALLENGE ||
            mChallengeScreen->mPageIndex != CHALLENGE_PAGE_CHALLENGE) return false;
        if (Record(this, "challenge_open"))
        {
            s.mStage = 10;
            if (aHost)
                PushButton(this, mChallengeScreen->mBackButton);
        }
        return false;
    }
    if (s.mStage == 11 || s.mStage == 12)
    {
        StoreScreen* aStore = (StoreScreen*)GetDialog(DIALOG_STORE);
        if (!aStore || (s.mStage == 12 && aStore->mPage != STORE_PAGE_PLANT_UPGRADES)) return false;
        if (!aStore->CanInteractWithButtons()) return false;
        if (Record(this, s.mStage == 11 ? "store_open" : "store_page"))
        {
            const int aPrevious = s.mStage++;
            if (aHost && !PushButton(this, aPrevious == 11 ? aStore->mNextButton : aStore->mBackButton))
                CoopSmokeFail("store navigation SDL input failed");
        }
        return false;
    }
    if (s.mStage == 15)
    {
        NewOptionsDialog* aDialog = (NewOptionsDialog*)GetDialog(DIALOG_NEWOPTIONS);
        if (!aDialog) return false;
        LawnStoneButton* aCoop = aDialog->mCoopAppearanceButton;
        int aControlsBottom = 0;
        for (Widget* aControl : { (Widget*)aDialog->mMusicVolumeSlider, (Widget*)aDialog->mSfxVolumeSlider,
            (Widget*)aDialog->mHardwareAccelerationCheckbox, (Widget*)aDialog->mFullscreenCheckbox,
            (Widget*)aDialog->mAlmanacButton, (Widget*)aDialog->mRestartButton, (Widget*)aDialog->mBackToMainButton })
            if (aControl->mVisible) aControlsBottom = max(aControlsBottom, aControl->mY + aControl->mHeight);
        if (!aCoop || !aCoop->mVisible || aCoop->mDisabled || aCoop->mX < 0 ||
            aCoop->mY < aControlsBottom || aCoop->mX + aCoop->mWidth > aDialog->mWidth ||
            aCoop->mY + aCoop->mHeight >= aDialog->mBackToGameButton->mY ||
            aDialog->mX < 0 || aDialog->mY < 0 || aDialog->mX + aDialog->mWidth > mWidth ||
            aDialog->mY + aDialog->mHeight > mHeight)
        { CoopSmokeFail("native CO-OP options button is outside the dialog or overlaps controls"); return false; }
        if (Record(this, "options_coop"))
        {
            NetTrace("SMOKE_FRESH_OPTIONS role=%s below=1 bounds=1 x=%d y=%d controlsBottom=%d", aRole,
                aCoop->mX, aCoop->mY, aControlsBottom);
            s.mStage = 16;
            if (!PushDialogButton(this, aDialog, aCoop)) CoopSmokeFail("native CO-OP settings SDL click failed");
        }
        return false;
    }
    if (s.mStage == 16 || s.mStage == 17)
    {
        MoreSettingsDialog* aDialog = (MoreSettingsDialog*)GetDialog(DIALOG_MORESETTINGS);
        if (!aDialog) return false;
        float aCenterY = 0.0f;
        if (!PreviewBounds(aDialog, aCenterY) || aDialog->mX < 0 || aDialog->mY < 0 ||
            aDialog->mX + aDialog->mWidth > mWidth || aDialog->mY + aDialog->mHeight > mHeight)
        { CoopSmokeFail("animated sunflower preview is clipped or not aligned with its seed packet"); return false; }
        if (s.mStage == 17 && (aNow - s.mPreviewTick < 250U ||
            fabsf(aDialog->mPreviewReanim->mAnimTime - s.mPreviewTime) < 0.001f)) return false;
        if (Record(this, s.mStage == 16 ? "settings_sunflower_a" : "settings_sunflower_b"))
        {
            const bool aSecond = s.mStage == 17;
            NetTrace("SMOKE_FRESH_PREVIEW role=%s sample=%d time=%.4f centerY=%.1f aligned=1 unclipped=1 advanced=%d",
                aRole, aSecond ? 2 : 1, aDialog->mPreviewReanim->mAnimTime, aCenterY, aSecond ? 1 : 0);
            ++s.mStage;
            s.mPreviewTick = aNow;
            s.mPreviewTime = aDialog->mPreviewReanim->mAnimTime;
            if (aSecond && !PushDialogButton(this, aDialog, aDialog->mLawnYesButton))
                CoopSmokeFail("native appearance settings close SDL click failed");
        }
        return false;
    }
    if (s.mStage == 18)
    {
        NewOptionsDialog* aDialog = (NewOptionsDialog*)GetDialog(DIALOG_NEWOPTIONS);
        if (!aDialog || GetDialog(DIALOG_MORESETTINGS)) return false;
        if (Record(this, "settings_closed"))
        {
            s.mStage = 19;
            aDialog->KeyDown(KEYCODE_ESCAPE);
        }
        return false;
    }
    if (s.mStage == 19)
    {
        if (GetDialog(DIALOG_NEWOPTIONS) || GetDialog(DIALOG_MORESETTINGS)) return false;
        if (Record(this, "options_closed")) { s.mStage = 20; NetShowOnlineBrowser(); }
        return false;
    }
    if (s.mStage == 20)
    {
        NetOnlineDialog* aDialog = (NetOnlineDialog*)GetDialog(DIALOG_COOP_ONLINE);
        if (!aDialog) return false;
        NetOnline* anOnline = mNetManager->GetOnline();
        const int aFrameEdges = IMAGE_DIALOG_TOPLEFT->mWidth + IMAGE_DIALOG_TOPRIGHT->mWidth;
        if (aDialog->mX < 0 || aDialog->mY < 0 || aDialog->mX + aDialog->mWidth > mWidth ||
            aDialog->mY + aDialog->mHeight > mHeight ||
            (aDialog->mWidth - aFrameEdges) % IMAGE_DIALOG_TOPMIDDLE->mWidth != 0 ||
            (anOnline && anOnline->IsConnected()))
        { CoopSmokeFail("online browser bounds or empty-directory isolation failed"); return false; }
        if (Record(this, "online_browser"))
        {
            NetTrace("SMOKE_FRESH_ONLINE role=%s frameFit=1 width=%d directoryConnected=0", aRole, aDialog->mWidth);
            s.mStage = 21;
            if (!PushDialogButton(this, aDialog, aDialog->mLawnYesButton))
                CoopSmokeFail("native online browser close SDL click failed");
        }
        return false;
    }
    if (s.mStage == 21)
    {
        if (GetDialog(DIALOG_COOP_ONLINE) || GetDialog(DIALOG_NEWOPTIONS) || GetDialog(DIALOG_MORESETTINGS)) return false;
        if (mNetManager->GetPlayerCount() != 2 || (aHost ? !mNetManager->IsHosting() : !mNetManager->IsConnectedToHost()))
        { CoopSmokeFail("closing local UI disconnected the existing LAN game"); return false; }
        if (Record(this, "online_closed"))
        {
            s.mStage = 22;
            if (aHost) { KillGameSelector(); PreNewGame(GAMEMODE_ADVENTURE, false, 2); }
        }
        return false;
    }
    return s.mStage == 23;
}
