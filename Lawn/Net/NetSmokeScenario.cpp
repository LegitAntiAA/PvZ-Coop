#include "NetSmokeScenario.h"

#include "NetCommon.h"
#include "NetManager.h"
#include "../Board.h"
#include "../CutScene.h"
#include "../CursorObject.h"
#include "../GridItem.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/DDInterface.h"

#include <windows.h>

namespace LawnNet
{
namespace
{
const uint32_t SMOKE_INTRO_TIMEOUT_MS = 60000;
const uint32_t SMOKE_INTRO_ACTION_INTERVAL_MS = 500;

int SmokeLocalX(LawnApp* theApp, int theCanonicalX)
{
	return theApp && theApp->mDDInterface ?
		theCanonicalX + theApp->mDDInterface->mWideScreenOffsetX : theCanonicalX;
}

int SmokeLocalY(LawnApp* theApp, int theCanonicalY)
{
	return theApp && theApp->mDDInterface ?
		theCanonicalY + theApp->mDDInterface->mWideScreenOffsetY : theCanonicalY;
}

void SmokeHostClick(LawnApp* theApp, int theCanonicalX, int theCanonicalY)
{
	if (!theApp || !theApp->mBoard)
		return;
	const int aLocalX = SmokeLocalX(theApp, theCanonicalX);
	const int aLocalY = SmokeLocalY(theApp, theCanonicalY);
	theApp->mBoard->MouseDown(aLocalX, aLocalY, 1);
	theApp->mBoard->MouseUp(aLocalX, aLocalY, 1);
}

void SmokeClientClick(LawnApp* theApp, int theCanonicalX, int theCanonicalY)
{
	if (!theApp || !theApp->mNetManager)
		return;
	const int aLocalX = SmokeLocalX(theApp, theCanonicalX);
	const int aLocalY = SmokeLocalY(theApp, theCanonicalY);
	theApp->mNetManager->SendInput(true, 1, aLocalX, aLocalY);
	theApp->mNetManager->SendInput(false, 1, aLocalX, aLocalY);
}

int CountScaryPots(Board* theBoard)
{
	if (!theBoard)
		return 0;
	int aCount = 0;
	GridItem* aGridItem = nullptr;
	while (theBoard->mGridItems.IterateNext(aGridItem))
	{
		if (!aGridItem->mDead && aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)
			++aCount;
	}
	return aCount;
}

GridItem* FindFirstScaryPot(Board* theBoard)
{
	if (!theBoard)
		return nullptr;
	GridItem* aGridItem = nullptr;
	while (theBoard->mGridItems.IterateNext(aGridItem))
	{
		if (!aGridItem->mDead && aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)
			return aGridItem;
	}
	return nullptr;
}

SeedPacket* FindUsablePacket(Board* theBoard)
{
	if (!theBoard || !theBoard->mSeedBank)
		return nullptr;
	for (int i = 0; i < theBoard->mSeedBank->mNumPackets; ++i)
	{
		SeedPacket* aPacket = &theBoard->mSeedBank->mSeedPackets[i];
		if (aPacket->mPacketType != SeedType::SEED_NONE && aPacket->CanPickUp())
			return aPacket;
	}
	return nullptr;
}

bool FindEmptySmokeCell(Board* theBoard, int& theGridX, int& theGridY)
{
	if (!theBoard)
		return false;
	// Row 2 is the single active strip in Adventure 1 and is also valid for
	// Adventure 5 Bowling.  Prefer the left three columns to keep the action far
	// from preview zombies and independent of asymmetric window widths.
	for (int aColumn = 0; aColumn <= 2; ++aColumn)
	{
		if (!theBoard->GetTopPlantAt(aColumn, 2, PlantPriority::TOPPLANT_ANY))
		{
			theGridX = aColumn;
			theGridY = 2;
			return true;
		}
	}
	return false;
}

bool IssueHostSeedAction(LawnApp* theApp)
{
	Board* aBoard = theApp ? theApp->mBoard : nullptr;
	SeedPacket* aPacket = FindUsablePacket(aBoard);
	int aGridX = 0;
	int aGridY = 0;
	if (!aBoard || !aPacket || !FindEmptySmokeCell(aBoard, aGridX, aGridY))
		return false;

	const int aPacketX = aBoard->mSeedBank->mX + aPacket->mX + aPacket->mOffsetX + aPacket->mWidth / 2;
	const int aPacketY = aBoard->mSeedBank->mY + aPacket->mY + aPacket->mHeight / 2;
	SmokeHostClick(theApp, aPacketX, aPacketY);
	if (!aBoard->mCursorObject ||
		(aBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_BANK &&
		 aBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN))
		return false;

	const int aPlantX = aBoard->GridToPixelX(aGridX, aGridY) + 40;
	const int aPlantY = aBoard->GridToPixelY(aGridX, aGridY) + 45;
	SmokeHostClick(theApp, aPlantX, aPlantY);
	return true;
}

Plant* FindFirstTutorialPeashooter(Board* theBoard)
{
	if (!theBoard)
		return nullptr;
	Plant* aPlant = nullptr;
	while (theBoard->IteratePlants(aPlant))
	{
		if (!aPlant->mDead && aPlant->mIsOnBoard &&
			aPlant->mSeedType == SeedType::SEED_PEASHOOTER)
		{
			return aPlant;
		}
	}
	return nullptr;
}
}

NetSmokeScenarioRuntime::NetSmokeScenarioRuntime()
{
	Reset();
}

void NetSmokeScenarioRuntime::Reset(NetSmokeScenarioId theScenario)
{
	mScenario = theScenario;
	mBoardIdentity = nullptr;
	mIntroTick = 0;
	mIntroLastActionTick = 0;
	mPlayingTick = 0;
	mActionTick = 0;
	mIntroLastCutsceneTime = -1;
	mIntroLastDaveMessage = -2;
	mIntroLastTutorialState = -1;
	mBaselinePlants = 0;
	mBaselinePots = 0;
	mIntroObserved = false;
	mIntroProgressObserved = false;
	mIntroSawDave = false;
	mChooserStartIssued = false;
	mBaselineCaptured = false;
	mActionIssued = false;
	mActionApplied = false;
	mPostActionSamples = 0;
}

bool NetSmokeScenarioRuntime::UpdateIntro(LawnApp* theApp,
	const NetSmokeManifest& theManifest,
	const NetSmokeScenarioDefinition& theDefinition, std::string& theFailure)
{
	Board* aBoard = theApp ? theApp->mBoard : nullptr;
	CutScene* aCutScene = aBoard ? aBoard->mCutScene : nullptr;
	if (!aBoard || !aCutScene)
	{
		theFailure = "smoke intro is missing its board or cutscene";
		return false;
	}

	const uint32_t aNow = GetTickCount();
	if (!mIntroObserved)
	{
		mIntroObserved = true;
		mIntroTick = aNow;
		mIntroLastCutsceneTime = aCutScene->mCutsceneTime;
		mIntroLastDaveMessage = theApp->mCrazyDaveMessageIndex;
		mIntroLastTutorialState = (int)aBoard->mTutorialState;
		mIntroSawDave = theApp->mCrazyDaveMessageIndex >= 0;
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=INTRO_BEGIN cutscene=%d dave=%d tutorial=%d skip=%d",
			theManifest.IsHost() ? "host" : "client", theDefinition.mName,
			aCutScene->mCutsceneTime, theApp->mCrazyDaveMessageIndex,
			(int)aBoard->mTutorialState, theManifest.mSkipIntro ? 1 : 0);
	}

	if (aCutScene->mCutsceneTime != mIntroLastCutsceneTime)
	{
		mIntroProgressObserved = true;
		mIntroLastCutsceneTime = aCutScene->mCutsceneTime;
	}
	if (theApp->mCrazyDaveMessageIndex != mIntroLastDaveMessage)
	{
		mIntroLastDaveMessage = theApp->mCrazyDaveMessageIndex;
		if (mIntroLastDaveMessage >= 0)
		{
			mIntroSawDave = true;
			NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=INTRO_DAVE_OBSERVED message=%d cutscene=%d",
				theManifest.IsHost() ? "host" : "client", theDefinition.mName,
				mIntroLastDaveMessage, aCutScene->mCutsceneTime);
		}
	}
	if ((int)aBoard->mTutorialState != mIntroLastTutorialState)
	{
		mIntroLastTutorialState = (int)aBoard->mTutorialState;
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=INTRO_TUTORIAL_OBSERVED state=%d cutscene=%d",
			theManifest.IsHost() ? "host" : "client", theDefinition.mName,
			mIntroLastTutorialState, aCutScene->mCutsceneTime);
	}

	const uint32_t anElapsed = aNow - mIntroTick;
	if (anElapsed >= SMOKE_INTRO_TIMEOUT_MS)
	{
		NetTrace("SMOKE_INTRO_TIMEOUT role=%s scenario=%s elapsed=%u cutscene=%d dave=%d daveState=%d tutorial=%d seedChoosing=%d chooser=%p mouse=%d",
			theManifest.IsHost() ? "host" : "client", theDefinition.mName,
			anElapsed, aCutScene->mCutsceneTime, theApp->mCrazyDaveMessageIndex,
			(int)theApp->mCrazyDaveState, (int)aBoard->mTutorialState,
			aCutScene->mSeedChoosing ? 1 : 0, (void*)theApp->mSeedChooserScreen,
			theApp->mSeedChooserScreen && theApp->mSeedChooserScreen->mMouseVisible ? 1 : 0);
		theFailure = "real intro did not reach gameplay within 60 seconds";
		return false;
	}

	// skip_intro=1 is deliberately still owned by LawnApp's existing CancelIntro
	// hook.  The driver below exists only to exercise the real, synchronized
	// host presentation path.
	if (theManifest.mSkipIntro || !theManifest.IsHost() ||
		(uint32_t)(aNow - mIntroLastActionTick) < SMOKE_INTRO_ACTION_INTERVAL_MS)
	{
		return true;
	}

	if (theApp->mCrazyDaveMessageIndex >= 0 && aCutScene->IsShowingCrazyDave())
	{
		const int aBefore = theApp->mCrazyDaveMessageIndex;
		mIntroLastActionTick = aNow;
		// Go through Board::MouseDown/MouseUp, exactly as a local host click does;
		// CutScene::MouseDown remains the sole dialog authority.
		SmokeHostClick(theApp, 400, 300);
		if (theApp->mBoard == aBoard && aBoard->mCutScene == aCutScene)
		{
			NetTrace("SMOKE_CHECKPOINT role=host scenario=%s step=INTRO_DAVE_CLICK from=%d to=%d cutscene=%d",
				theDefinition.mName, aBefore, theApp->mCrazyDaveMessageIndex,
				aCutScene->mCutsceneTime);
		}
		return true;
	}

	// Adventure 5's first-time presentation contains the real shovel tutorial
	// between Dave's two dialog runs.  Exercise its normal board hit-testing and
	// tool path instead of mutating tutorial/cutscene state.
	if (aBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_PICKUP)
	{
		const Rect aShovel = aBoard->GetShovelButtonRect();
		mIntroLastActionTick = aNow;
		SmokeHostClick(theApp, aShovel.mX + aShovel.mWidth / 2,
			aShovel.mY + aShovel.mHeight / 2);
		if (theApp->mBoard == aBoard)
		{
			NetTrace("SMOKE_CHECKPOINT role=host scenario=%s step=INTRO_SHOVEL_PICKUP tutorial=%d",
				theDefinition.mName, (int)aBoard->mTutorialState);
		}
		return true;
	}
	if (aBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG ||
		aBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING)
	{
		// DigPlant clears the cursor after every plant.  The real tutorial expects
		// the player to pick the shovel up again before each remaining peashooter.
		if (!aBoard->mCursorObject ||
			aBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_SHOVEL)
		{
			const Rect aShovel = aBoard->GetShovelButtonRect();
			mIntroLastActionTick = aNow;
			SmokeHostClick(theApp, aShovel.mX + aShovel.mWidth / 2,
				aShovel.mY + aShovel.mHeight / 2);
			if (theApp->mBoard == aBoard)
			{
				NetTrace("SMOKE_CHECKPOINT role=host scenario=%s step=INTRO_SHOVEL_REPICKUP tutorial=%d cursor=%d",
					theDefinition.mName, (int)aBoard->mTutorialState,
					aBoard->mCursorObject ? (int)aBoard->mCursorObject->mCursorType : -1);
			}
			return true;
		}

		Plant* aPlant = FindFirstTutorialPeashooter(aBoard);
		if (aPlant)
		{
			const int aGridX = aPlant->mPlantCol;
			const int aGridY = aPlant->mRow;
			const int aPlantX = aBoard->GridToPixelX(aGridX, aGridY) + 40;
			const int aPlantY = aBoard->GridToPixelY(aGridX, aGridY) + 50;
			mIntroLastActionTick = aNow;
			SmokeHostClick(theApp, aPlantX, aPlantY);
			if (theApp->mBoard == aBoard)
			{
				NetTrace("SMOKE_CHECKPOINT role=host scenario=%s step=INTRO_SHOVEL_DIG cell=(%d,%d) tutorial=%d",
					theDefinition.mName, aGridX, aGridY, (int)aBoard->mTutorialState);
			}
		}
		return true;
	}

	// Some no-card challenge intros still present the chooser and require the
	// host to press Start.  Use the same public start handler as the button only when the
	// authoritative generation says this host owns exactly zero picks.
	if (aCutScene->mSeedChoosing && theApp->mSeedChooserScreen)
	{
		SeedChooserScreen* aChooser = theApp->mSeedChooserScreen;
		if (!mChooserStartIssued && aChooser->mCoopChooserActive &&
			aChooser->mCoopPicksPerPlayer == 0 &&
			aChooser->CoopGetLocalQuotaPickCount() == 0 &&
			aChooser->mSeedsInFlight == 0 && aChooser->mMouseVisible &&
			aChooser->mStartButton && !aChooser->mStartButton->mDisabled)
		{
			mIntroLastActionTick = aNow;
			aChooser->OnStartButton();
			const bool aStillChoosing = theApp->mBoard == aBoard &&
				aBoard->mCutScene == aCutScene && aCutScene->mSeedChoosing;
			mChooserStartIssued = !aStillChoosing;
			NetTrace("SMOKE_CHECKPOINT role=host scenario=%s step=INTRO_ZERO_QUOTA_START seedChoosing=%d",
				theDefinition.mName, aStillChoosing ? 1 : 0);
		}
	}

	return true;
}

bool NetSmokeScenarioRuntime::IssueAction(LawnApp* theApp, const NetSmokeManifest& theManifest)
{
	const NetSmokeScenarioDefinition* aDefinition = NetGetSmokeScenario(mScenario);
	if (!aDefinition || !theApp || !theApp->mBoard)
		return false;
	if (aDefinition->mActionIsClientOwned != theManifest.IsClient())
		return false;

	if (mScenario == NETSMOKE_SCENARIO_VASEBREAKER)
	{
		GridItem* aPot = FindFirstScaryPot(theApp->mBoard);
		if (!aPot)
			return false;
		const int aX = theApp->mBoard->GridToPixelX(aPot->mGridX, aPot->mGridY) + 40;
		const int aY = theApp->mBoard->GridToPixelY(aPot->mGridX, aPot->mGridY) + 45;
		SmokeClientClick(theApp, aX, aY);
		return true;
	}

	return IssueHostSeedAction(theApp);
}

bool NetSmokeScenarioRuntime::ObserveActionApplied(Board* theBoard) const
{
	if (!theBoard || !mBaselineCaptured)
		return false;
	if (mScenario == NETSMOKE_SCENARIO_VASEBREAKER)
		return CountScaryPots(theBoard) < mBaselinePots;
	return theBoard->mPlants.mSize > mBaselinePlants;
}

bool NetSmokeScenarioRuntime::Update(LawnApp* theApp, const NetSmokeManifest& theManifest,
	uint32_t theGameplaySnapshots, std::string& theFailure)
{
	theFailure.clear();
	if (!theApp || !theApp->mBoard)
		return true;
	const NetSmokeScenarioDefinition* aDefinition = NetGetSmokeScenario(mScenario);
	if (!aDefinition)
	{
		theFailure = "smoke scenario runtime has no resolved definition";
		return false;
	}

	Board* aBoard = theApp->mBoard;
	if (mBoardIdentity != aBoard)
	{
		Reset(mScenario);
		mBoardIdentity = aBoard;
		mIntroTick = GetTickCount();
	}

	if (theApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		return UpdateIntro(theApp, theManifest, *aDefinition, theFailure);
	if (theApp->mGameScene != GameScenes::SCENE_PLAYING)
		return true;

	if (mPlayingTick == 0)
	{
		if (!theManifest.mSkipIntro)
		{
			if (!mIntroObserved || !mIntroProgressObserved)
			{
				theFailure = "gameplay began without observing synchronized real intro progress";
				return false;
			}
			if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING && !mIntroSawDave)
			{
				theFailure = "Adventure 5 gameplay began without observing synchronized Dave presentation";
				return false;
			}
		}
		mPlayingTick = GetTickCount();
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=INTRO_COMPLETE elapsed=%u progress=%d dave=%d",
			theManifest.IsHost() ? "host" : "client", aDefinition->mName,
			mIntroTick ? (uint32_t)(mPlayingTick - mIntroTick) : 0,
			mIntroProgressObserved ? 1 : 0, mIntroSawDave ? 1 : 0);
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=PLAYING size=%dx%d board=%p",
			theManifest.IsHost() ? "host" : "client", aDefinition->mName,
			theApp->mWidth, theApp->mHeight, (void*)aBoard);
	}

	if (theApp->mSeedChooserScreen || theApp->GetDialogCount() > 0 || aBoard->mPaused)
		return true;
	if (!mBaselineCaptured)
	{
		// Two gameplay snapshots prevent the client from capturing a partially
		// populated ghost board as its baseline.
		if (theGameplaySnapshots < 2 || (uint32_t)(GetTickCount() - mPlayingTick) < 500)
			return true;
		mBaselinePlants = aBoard->mPlants.mSize;
		mBaselinePots = CountScaryPots(aBoard);
		if (mScenario == NETSMOKE_SCENARIO_VASEBREAKER && mBaselinePots <= 0)
			return true;
		mBaselineCaptured = true;
		mActionTick = GetTickCount();
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=BASELINE plants=%d pots=%d snapshots=%u",
			theManifest.IsHost() ? "host" : "client", aDefinition->mName,
			mBaselinePlants, mBaselinePots, theGameplaySnapshots);
	}

	if (!mActionIssued && aDefinition->mActionIsClientOwned == theManifest.IsClient())
	{
		if (IssueAction(theApp, theManifest))
		{
			mActionIssued = true;
			mActionTick = GetTickCount();
			NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=ACTION_ISSUED owner=%s",
				theManifest.IsHost() ? "host" : "client", aDefinition->mName,
				aDefinition->mActionIsClientOwned ? "client" : "host");
		}
		else if ((uint32_t)(GetTickCount() - mActionTick) >= 30000)
		{
			theFailure = "scripted smoke action could not be issued within 30 seconds";
			return false;
		}
	}

	if (!mActionApplied && ObserveActionApplied(aBoard))
	{
		mActionApplied = true;
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=ACTION_APPLIED plants=%d pots=%d",
			theManifest.IsHost() ? "host" : "client", aDefinition->mName,
			aBoard->mPlants.mSize, CountScaryPots(aBoard));
	}
	if (mActionIssued && !mActionApplied &&
		(uint32_t)(GetTickCount() - mActionTick) >= 30000)
	{
		theFailure = "scripted smoke action was not applied within 30 seconds";
		return false;
	}
	return true;
}

void NetSmokeScenarioRuntime::OnSnapshotTx(uint32_t theHash, size_t theBytes)
{
	if (!mActionApplied)
		return;
	++mPostActionSamples;
	NetTrace("SMOKE_POST_ACTION_TX index=%u bytes=%zu hash=%08x",
		mPostActionSamples, theBytes, theHash);
}

void NetSmokeScenarioRuntime::OnSnapshotApplied(uint32_t theHash, size_t theBytes)
{
	if (!mActionApplied)
		return;
	++mPostActionSamples;
	NetTrace("SMOKE_POST_ACTION_APPLIED index=%u bytes=%zu hash=%08x",
		mPostActionSamples, theBytes, theHash);
}

bool NetSmokeScenarioRuntime::IsComplete(uint32_t theRequiredPostActionSnapshots) const
{
	return mActionApplied && theRequiredPostActionSnapshots > 0 &&
		mPostActionSamples >= theRequiredPostActionSnapshots;
}

} // namespace LawnNet
