#include "NetSmokeScenario.h"

#include "NetCommon.h"
#include "NetManager.h"
#include "../Board.h"
#include "../CursorObject.h"
#include "../GridItem.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/DDInterface.h"

#include <windows.h>

namespace LawnNet
{
namespace
{
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
}

NetSmokeScenarioRuntime::NetSmokeScenarioRuntime()
{
	Reset();
}

void NetSmokeScenarioRuntime::Reset(NetSmokeScenarioId theScenario)
{
	mScenario = theScenario;
	mBoardIdentity = nullptr;
	mPlayingTick = 0;
	mActionTick = 0;
	mBaselinePlants = 0;
	mBaselinePots = 0;
	mBaselineCaptured = false;
	mActionIssued = false;
	mActionApplied = false;
	mPostActionSamples = 0;
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
	if (!theApp || !theApp->mBoard || theApp->mGameScene != GameScenes::SCENE_PLAYING)
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
		mPlayingTick = GetTickCount();
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
