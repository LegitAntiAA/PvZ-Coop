#include "NetSmokeScenario.h"

#include "NetCommon.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../Coin.h"
#include "../CutScene.h"
#include "../CursorObject.h"
#include "../GridItem.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../Zombie.h"
#include "../Widget/GameButton.h"
#include "../Widget/AwardScreen.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Sexy.TodLib/Reanimator.h"

#include <windows.h>

namespace LawnNet
{
namespace
{
const uint32_t SMOKE_INTRO_TIMEOUT_MS = 60000;
const uint32_t SMOKE_INTRO_ACTION_INTERVAL_MS = 500;
const uint32_t SMOKE_SOAK_DURATION_MS = 60000;
const uint32_t SMOKE_VICTORY_INTERACTION_MS = 20000;
const uint32_t SMOKE_VICTORY_TERMINAL_TIMEOUT_MS = 30000;

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

void SmokeClientDrag(LawnApp* theApp, int theFromX, int theFromY,
	int theToX, int theToY)
{
	if (!theApp || !theApp->mNetManager)
		return;
	const int aFromLocalX = SmokeLocalX(theApp, theFromX);
	const int aFromLocalY = SmokeLocalY(theApp, theFromY);
	const int aToLocalX = SmokeLocalX(theApp, theToX);
	const int aToLocalY = SmokeLocalY(theApp, theToY);
	theApp->mNetManager->SendInput(true, 1, aFromLocalX, aFromLocalY);
	theApp->mNetManager->SendMouseMove(1, aToLocalX, aToLocalY);
	theApp->mNetManager->SendInput(false, 1, aToLocalX, aToLocalY);
}

SeedType SmokeBeghouledSeedAt(Board* theBoard, int theGridX, int theGridY)
{
	Plant* aPlant = theBoard->GetTopPlantAt(theGridX, theGridY,
		PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
	return aPlant ? aPlant->mSeedType : SeedType::SEED_NONE;
}

uint32_t SmokePlantGridHash(Board* theBoard)
{
	uint32_t aHash = 2166136261U;
	if (!theBoard)
		return aHash;
	for (int aRow = 0; aRow < BEGHOULED_MAX_GRIDSIZEY; ++aRow)
	{
		for (int aColumn = 0; aColumn < BEGHOULED_MAX_GRIDSIZEX; ++aColumn)
		{
			const uint32_t aValue = (uint32_t)SmokeBeghouledSeedAt(theBoard,
				aColumn, aRow) + 1U;
			aHash ^= aValue;
			aHash *= 16777619U;
		}
	}
	return aHash;
}

bool SmokeBeghouledMatchAt(SeedType theGrid[BEGHOULED_MAX_GRIDSIZEX][BEGHOULED_MAX_GRIDSIZEY],
	int theGridX, int theGridY)
{
	const SeedType aSeed = theGrid[theGridX][theGridY];
	if (aSeed == SeedType::SEED_NONE)
		return false;
	int aHorizontal = 1;
	for (int x = theGridX - 1; x >= 0 && theGrid[x][theGridY] == aSeed; --x)
		++aHorizontal;
	for (int x = theGridX + 1; x < BEGHOULED_MAX_GRIDSIZEX &&
		theGrid[x][theGridY] == aSeed; ++x)
		++aHorizontal;
	int aVertical = 1;
	for (int y = theGridY - 1; y >= 0 && theGrid[theGridX][y] == aSeed; --y)
		++aVertical;
	for (int y = theGridY + 1; y < BEGHOULED_MAX_GRIDSIZEY &&
		theGrid[theGridX][y] == aSeed; ++y)
		++aVertical;
	return aHorizontal >= 3 || aVertical >= 3;
}

bool FindBeghouledSmokeMove(Board* theBoard, int& theFromX, int& theFromY,
	int& theToX, int& theToY)
{
	if (!theBoard || !theBoard->mChallenge ||
		theBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_NORMAL ||
		theBoard->mChallenge->mBeghouledMouseCapture)
		return false;

	SeedType aGrid[BEGHOULED_MAX_GRIDSIZEX][BEGHOULED_MAX_GRIDSIZEY];
	for (int y = 0; y < BEGHOULED_MAX_GRIDSIZEY; ++y)
		for (int x = 0; x < BEGHOULED_MAX_GRIDSIZEX; ++x)
			aGrid[x][y] = SmokeBeghouledSeedAt(theBoard, x, y);

	for (int y = 0; y < BEGHOULED_MAX_GRIDSIZEY; ++y)
	{
		for (int x = 0; x < BEGHOULED_MAX_GRIDSIZEX; ++x)
		{
			const int aNeighbours[2][2] = { { x + 1, y }, { x, y + 1 } };
			for (int i = 0; i < 2; ++i)
			{
				const int aToX = aNeighbours[i][0];
				const int aToY = aNeighbours[i][1];
				if (aToX >= BEGHOULED_MAX_GRIDSIZEX || aToY >= BEGHOULED_MAX_GRIDSIZEY ||
					theBoard->mChallenge->mBeghouledEated[x][y] ||
					theBoard->mChallenge->mBeghouledEated[aToX][aToY] ||
					aGrid[x][y] == SeedType::SEED_NONE || aGrid[aToX][aToY] == SeedType::SEED_NONE ||
					aGrid[x][y] == aGrid[aToX][aToY])
					continue;

				const SeedType aSwap = aGrid[x][y];
				aGrid[x][y] = aGrid[aToX][aToY];
				aGrid[aToX][aToY] = aSwap;
				const bool aMatches = SmokeBeghouledMatchAt(aGrid, x, y) ||
					SmokeBeghouledMatchAt(aGrid, aToX, aToY);
				aGrid[aToX][aToY] = aGrid[x][y];
				aGrid[x][y] = aSwap;
				if (aMatches)
				{
					theFromX = x;
					theFromY = y;
					theToX = aToX;
					theToY = aToY;
					return true;
				}
			}
		}
	}
	return false;
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

SeedPacket* FindOwnedUsablePacket(Board* theBoard, unsigned char theOwner)
{
        if (!theBoard || !theBoard->mSeedBank)
                return nullptr;
        for (int i = 0; i < theBoard->mSeedBank->mNumPackets; ++i)
        {
                SeedPacket* aPacket = &theBoard->mSeedBank->mSeedPackets[i];
                if (aPacket->mPacketType != SeedType::SEED_NONE && aPacket->CanPickUp() &&
                        (aPacket->mNetOwnerSlot == theOwner ||
                         aPacket->mNetOwnerSlot == NET_SHARED_OWNER))
                        return aPacket;
        }
        return nullptr;
}

bool IssueSoakSeedAction(LawnApp* theApp, bool theClient)
{
        Board* aBoard = theApp ? theApp->mBoard : nullptr;
        if (!aBoard || !theApp->mNetManager)
                return false;
        const unsigned char anOwner = (unsigned char)theApp->mNetManager->GetMySlot();
        SeedPacket* aPacket = FindOwnedUsablePacket(aBoard, anOwner);
        if (!aPacket)
                return false;
        const int aFirstRow = theClient ? 3 : 1;
        int aGridX = -1;
        int aGridY = -1;
        for (int aRowPass = 0; aRowPass < 5 && aGridX < 0; ++aRowPass)
        {
                const int aRow = (aFirstRow + aRowPass) % 5;
                for (int aColumn = 0; aColumn < 5; ++aColumn)
                {
                        if (!aBoard->GetTopPlantAt(aColumn, aRow, PlantPriority::TOPPLANT_ANY) &&
                                aBoard->CanPlantAt(aColumn, aRow, aPacket->mPacketType) ==
                                        PlantingReason::PLANTING_OK)
                        {
                                aGridX = aColumn;
                                aGridY = aRow;
                                break;
                        }
                }
        }
        if (aGridX < 0)
                return false;
        const int aPacketX = aBoard->mSeedBank->mX + aPacket->mX +
                aPacket->mOffsetX + aPacket->mWidth / 2;
        const int aPacketY = aBoard->mSeedBank->mY + aPacket->mY + aPacket->mHeight / 2;
        const int aPlantX = aBoard->GridToPixelX(aGridX, aGridY) + 40;
        const int aPlantY = aBoard->GridToPixelY(aGridX, aGridY) + 45;
        if (theClient)
        {
                SmokeClientClick(theApp, aPacketX, aPacketY);
                SmokeClientClick(theApp, aPlantX, aPlantY);
        }
        else
        {
                SmokeHostClick(theApp, aPacketX, aPacketY);
                SmokeHostClick(theApp, aPlantX, aPlantY);
        }
        NetTrace("SMOKE_SOAK_PLANT role=%s owner=%u type=%d cell=(%d,%d)",
                theClient ? "client" : "host", anOwner, (int)aPacket->mPacketType,
                aGridX, aGridY);
        return true;
}

bool IssueSoakSunClick(LawnApp* theApp, bool theClient)
{
        Board* aBoard = theApp ? theApp->mBoard : nullptr;
        if (!aBoard)
                return false;
        Coin* aCoin = nullptr;
        while (aBoard->mCoins.IterateNext(aCoin))
        {
                if (aCoin->mDead || aCoin->mIsBeingCollected || !aCoin->IsSun())
                        continue;
                const int aX = (int)aCoin->mPosX + 20;
                const int aY = (int)aCoin->mPosY + 20;
                if (theClient)
                        SmokeClientClick(theApp, aX, aY);
                else
                        SmokeHostClick(theApp, aX, aY);
                NetTrace("SMOKE_SOAK_SUN_CLICK role=%s pos=(%d,%d) type=%d",
                        theClient ? "client" : "host", aX, aY, (int)aCoin->mType);
                return true;
        }
        return false;
}

SeedPacket* FindDuplicateUsablePacket(Board* theBoard, int& theDuplicateCount)
{
	theDuplicateCount = 0;
	if (!theBoard || !theBoard->mSeedBank)
		return nullptr;
	SeedBank* aBank = theBoard->mSeedBank;
	for (int i = 0; i < aBank->mNumPackets; ++i)
	{
		SeedPacket* aPacket = &aBank->mSeedPackets[i];
		if (aPacket->mPacketType == SeedType::SEED_NONE || aPacket->mNetId == 0 ||
			!aPacket->CanPickUp())
			continue;
		int aMatchCount = 1;
		for (int j = 0; j < aBank->mNumPackets; ++j)
		{
			if (i == j)
				continue;
			const SeedPacket& anOther = aBank->mSeedPackets[j];
			if (anOther.mNetId != 0 && anOther.mPacketType == aPacket->mPacketType &&
				anOther.mImitaterType == aPacket->mImitaterType &&
				anOther.mNetOwnerSlot == aPacket->mNetOwnerSlot)
				aMatchCount++;
		}
		if (aMatchCount >= 2)
		{
			theDuplicateCount = aMatchCount;
			return aPacket;
		}
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

bool IssueClientSeedAction(LawnApp* theApp)
{
	Board* aBoard = theApp ? theApp->mBoard : nullptr;
	// Wait for a real duplicate conveyor identity.  Adventure 5 has two packet
	// types, so its third live card guarantees this case and proves that stable
	// packet ids—not an ambiguous type tuple—drive the remote click.
	int aDuplicateCount = 0;
	SeedPacket* aPacket = FindDuplicateUsablePacket(aBoard, aDuplicateCount);
	int aGridX = 0;
	int aGridY = 0;
	if (!aBoard || !aPacket || !FindEmptySmokeCell(aBoard, aGridX, aGridY))
		return false;

	const int aPacketIndex = (int)(aPacket - aBoard->mSeedBank->mSeedPackets);
	const int aPacketX = aBoard->mSeedBank->mX + aPacket->mX + aPacket->mOffsetX + aPacket->mWidth / 2;
	const int aPacketY = aBoard->mSeedBank->mY + aPacket->mY + aPacket->mHeight / 2;
	const int aPlantX = aBoard->GridToPixelX(aGridX, aGridY) + 40;
	const int aPlantY = aBoard->GridToPixelY(aGridX, aGridY) + 45;

	// Both clicks travel through the same ordered client input stream as real
	// gameplay.  The host resolves the conveyor packet and owns creation of the
	// rolling nut; the client never mutates its mirror directly.
	SmokeClientClick(theApp, aPacketX, aPacketY);
	SmokeClientClick(theApp, aPlantX, aPlantY);
	NetTrace("SMOKE_BOWLING_CLIENT_INPUT index=%d packetId=%u type=%d owner=%u duplicates=%d cell=(%d,%d)",
		aPacketIndex, aPacket->mNetId, (int)aPacket->mPacketType,
		aPacket->mNetOwnerSlot, aDuplicateCount, aGridX, aGridY);
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

bool IsBowlingSmokeNut(const Plant* thePlant)
{
	if (!thePlant || thePlant->mDead || !thePlant->mIsOnBoard)
		return false;
	return thePlant->mSeedType == SeedType::SEED_WALLNUT ||
		thePlant->mSeedType == SeedType::SEED_EXPLODE_O_NUT ||
		thePlant->mSeedType == SeedType::SEED_GIANT_WALLNUT;
}

Plant* FindBowlingSmokeNut(Board* theBoard, uint16_t thePreferredId)
{
	if (!theBoard)
		return nullptr;
	Plant* aFallback = nullptr;
	Plant* aPlant = nullptr;
	while (theBoard->IteratePlants(aPlant))
	{
		if (!IsBowlingSmokeNut(aPlant))
			continue;
		if (thePreferredId != 0 && aPlant->mNetId == thePreferredId)
			return aPlant;
		if (!aFallback && aPlant->mNetId != 0)
			aFallback = aPlant;
	}
	return aFallback;
}

Zombie* FindSmokeZombieById(Board* theBoard, uint16_t theId)
{
	if (!theBoard || theId == 0)
		return nullptr;
	Zombie* aZombie = nullptr;
	while (theBoard->mZombies.IterateNext(aZombie))
	{
		if (aZombie->mNetId == theId)
			return aZombie;
	}
	return nullptr;
}

Zombie* FindWhackSmokeTarget(Board* theBoard)
{
	if (!theBoard || !theBoard->mApp || !theBoard->mCursorObject ||
		theBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_HAMMER)
		return nullptr;
	Zombie* aZombie = nullptr;
	Zombie* aBest = nullptr;
	while (theBoard->mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead || aZombie->IsDeadOrDying() || aZombie->mNetId == 0 ||
			aZombie->mZombieType != ZombieType::ZOMBIE_NORMAL ||
			aZombie->mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
			!aZombie->mHasHead || aZombie->mHelmType != HelmType::HELMTYPE_NONE ||
			aZombie->mShieldType != ShieldType::SHIELDTYPE_NONE ||
			!theBoard->mApp->ReanimationTryToGet(aZombie->mBodyReanimID))
		{
			continue;
		}
		const Rect aRect = aZombie->GetZombieRect();
		if (aRect.mWidth <= 0 || aRect.mHeight <= 0 || aRect.mX < 0 ||
			aRect.mY < 40 || aRect.mX + aRect.mWidth > BOARD_WIDTH ||
			aRect.mY + aRect.mHeight > BOARD_HEIGHT)
		{
			continue;
		}
		if (!aBest || aZombie->mNetId < aBest->mNetId)
			aBest = aZombie;
	}
	return aBest;
}

bool IssueClientWhackAction(LawnApp* theApp, uint16_t& theTargetId)
{
	Board* aBoard = theApp ? theApp->mBoard : nullptr;
	Zombie* aTarget = FindWhackSmokeTarget(aBoard);
	if (!aBoard || !aTarget)
		return false;
	const Rect aRect = aTarget->GetZombieRect();
	const int aClickX = aRect.mX + aRect.mWidth / 2;
	// MouseDownWhackAZombie tests a 45px circle centred 20px above the click.
	const int aClickY = aRect.mY + aRect.mHeight / 2 + 20;
	theTargetId = aTarget->mNetId;
	SmokeClientClick(theApp, aClickX, aClickY);
	NetTrace("SMOKE_WHACK_CLIENT_INPUT targetId=%u type=%d phase=%d rect=(%d,%d,%d,%d) click=(%d,%d)",
		theTargetId, (int)aTarget->mZombieType, (int)aTarget->mZombiePhase,
		aRect.mX, aRect.mY, aRect.mWidth, aRect.mHeight, aClickX, aClickY);
	return true;
}

bool IssueClientSlotMachineAction(LawnApp* theApp)
{
	Board* aBoard = theApp ? theApp->mBoard : nullptr;
	if (!aBoard || !aBoard->mChallenge || !aBoard->mSeedBank ||
		!theApp->IsSlotMachineLevel() || aBoard->mSunMoney < 25 ||
		aBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_NORMAL ||
		aBoard->mSeedBank->mNumPackets < 3)
	{
		return false;
	}
	for (int i = 0; i < 3; ++i)
		if (aBoard->mSeedBank->mSeedPackets[i].mSlotMachineCountDown > 0)
			return false;
	Reanimation* aHandle =
		theApp->ReanimationTryToGet(aBoard->mChallenge->mReanimChallenge);
	if (!aHandle)
		return false;
	const Rect aHandleRect = aBoard->mChallenge->SlotMachineGetHandleRect();
	const int aClickX = aHandleRect.mX + aHandleRect.mWidth / 2;
	const int aClickY = aHandleRect.mY + aHandleRect.mHeight / 2;
	SmokeClientClick(theApp, aClickX, aClickY);
	NetTrace("SMOKE_SLOT_MACHINE_CLIENT_INPUT roll=%d sun=%d rect=(%d,%d,%d,%d) click=(%d,%d)",
		aBoard->mChallenge->mSlotMachineRollCount, aBoard->mSunMoney,
		aHandleRect.mX, aHandleRect.mY, aHandleRect.mWidth, aHandleRect.mHeight,
		aClickX, aClickY);
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
	mIntroTick = 0;
	mIntroLastActionTick = 0;
	mPlayingTick = 0;
	mActionTick = 0;
	mIntroLastCutsceneTime = -1;
	mIntroLastDaveMessage = -2;
	mIntroLastTutorialState = -1;
	mBaselinePlants = 0;
	mBaselinePots = 0;
	mBaselineChallengeScore = 0;
	mBaselineSlotMachineRollCount = 0;
	mBaselineSunMoney = 0;
	mBaselinePlantGridHash = 0;
	mBowlingNutId = 0;
	mWhackTargetId = 0;
	mBowlingFirstX = 0;
	mBowlingFirstY = 0;
	mIntroObserved = false;
	mIntroProgressObserved = false;
	mIntroSawDave = false;
	mChooserStartIssued = false;
	mBaselineCaptured = false;
	mActionIssued = false;
	mActionApplied = false;
	mBowlingNutObserved = false;
	mBowlingNutMoved = false;
	mBowlingZombieObserved = false;
	mWhackRemovalObserved = false;
        mSlotMachinePresentationObserved = false;
        mSlotMachineReturnObserved = false;
        mSlotMachineSoundCount = 0;
        mSoakChooserLastCount = -1;
        mSoakPlantsIssued = 0;
        mSoakSunClicks = 0;
        mSoakLastActionTick = 0;
        mSoakChooserVerified = false;
        mSoakLossIssued = false;
        mSoakLossObserved = false;
        mSoakLossCameraObserved = false;
        mSoakLossBrainsObserved = false;
        mSoakLossDialogObserved = false;
	mVictoryTick = 0;
	mVictoryIssued = false;
	mVictoryFadeObserved = false;
	mVictoryLevelCompleteSnapshotObserved = false;
	mVictoryAwardObserved = false;
	mVictoryBoardReleasedObserved = false;
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

        // Adventure 8 is the first ordinary first-time Adventure level with a
        // genuine chooser.  Exercise each peer's real chooser hit-testing and
        // network pick path; never synthesize the final seed bank in the test.
        if (NetIsAdventure8InteractiveSmokeScenario(mScenario) &&
                aCutScene->mSeedChoosing && theApp->mSeedChooserScreen)
        {
                SeedChooserScreen* aChooser = theApp->mSeedChooserScreen;
                if (!aChooser->mCoopChooserActive || !aChooser->mMouseVisible)
                        return true;
                const int aLocalSlot = theApp->mNetManager ?
                        theApp->mNetManager->GetMySlot() : (theManifest.IsHost() ? 0 : 1);
                const int aOtherSlot = aLocalSlot == 0 ? 1 : 0;
                const int aLocalCount = aChooser->CoopGetLocalQuotaPickCount();
                const int aQuota = aChooser->mCoopPicksPerPlayer;
                if (aLocalCount != mSoakChooserLastCount)
                {
                        mSoakChooserLastCount = aLocalCount;
                        NetTrace("SMOKE_SOAK_CHOOSER_PROGRESS role=%s local=%d remote=%d quota=%d flight=%d",
                                theManifest.IsHost() ? "host" : "client", aLocalCount,
                                aChooser->mCoopRemotePickCount[aOtherSlot], aQuota,
                                aChooser->mSeedsInFlight);
                }
                if (aQuota <= 0)
                {
                        theFailure = "Adventure soak chooser unexpectedly assigned a zero pick quota";
                        return false;
                }
                if (aLocalCount < aQuota && aChooser->mSeedsInFlight == 0 &&
                        (uint32_t)(aNow - mIntroLastActionTick) >= SMOKE_INTRO_ACTION_INTERVAL_MS)
                {
                        const int aStart = theManifest.IsClient() ? NUM_SEEDS_IN_CHOOSER - 1 : 0;
                        const int aStep = theManifest.IsClient() ? -1 : 1;
                        for (int i = aStart; i >= 0 && i < NUM_SEEDS_IN_CHOOSER; i += aStep)
                        {
                                ChosenSeed& aSeed = aChooser->mChosenSeeds[i];
                                if (aSeed.mSeedState != SEED_IN_CHOOSER ||
                                        !theApp->SeedTypeAvailable(aSeed.mSeedType) ||
                                        aChooser->SeedNotAllowedToPick(aSeed.mSeedType) ||
                                        aChooser->SeedNotAllowedDuringTrial(aSeed.mSeedType) ||
                                        aChooser->CoopIsPickedByOthers((int)aSeed.mSeedType))
                                        continue;
                                mIntroLastActionTick = aNow;
                                aChooser->MouseDown(aSeed.mX + 25, aSeed.mY + 35, 1);
                                NetTrace("SMOKE_SOAK_CHOOSER_CLICK role=%s seed=%d pos=(%d,%d)",
                                        theManifest.IsHost() ? "host" : "client",
                                        (int)aSeed.mSeedType, aSeed.mX + 25, aSeed.mY + 35);
                                return true;
                        }
                }
                const int aRemoteCount = aChooser->mCoopRemotePickCount[aOtherSlot];
                if (aLocalCount == aQuota && aRemoteCount == aQuota &&
                        aChooser->mSeedsInFlight == 0)
                {
                        if (!mSoakChooserVerified)
                        {
                                mSoakChooserVerified = true;
                                NetTrace("SMOKE_SOAK_CHOOSER_VERIFIED role=%s local=%d remote=%d quota=%d",
                                        theManifest.IsHost() ? "host" : "client",
                                        aLocalCount, aRemoteCount, aQuota);
                        }
                        if (theManifest.IsHost() && !mChooserStartIssued &&
                                aChooser->mStartButton && !aChooser->mStartButton->mDisabled)
                        {
                                mIntroLastActionTick = aNow;
                                aChooser->OnStartButton();
                                mChooserStartIssued = true;
                                NetTrace("SMOKE_SOAK_CHOOSER_START role=host quota=%d", aQuota);
                        }
                }
                return true;
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
	if (mScenario == NETSMOKE_SCENARIO_BEGHOULED)
	{
		int aFromX = 0;
		int aFromY = 0;
		int aToX = 0;
		int aToY = 0;
		if (!FindBeghouledSmokeMove(theApp->mBoard, aFromX, aFromY, aToX, aToY))
			return false;
		const int aFromPixelX = theApp->mBoard->GridToPixelX(aFromX, aFromY) + 40;
		const int aFromPixelY = theApp->mBoard->GridToPixelY(aFromX, aFromY) + 45;
		const int aToPixelX = theApp->mBoard->GridToPixelX(aToX, aToY) + 40;
		const int aToPixelY = theApp->mBoard->GridToPixelY(aToX, aToY) + 45;
		SmokeClientDrag(theApp, aFromPixelX, aFromPixelY, aToPixelX, aToPixelY);
		NetTrace("SMOKE_BEGHOULED_DRAG from=(%d,%d) to=(%d,%d)",
			aFromX, aFromY, aToX, aToY);
		return true;
	}
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT)
		return IssueClientSeedAction(theApp);
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT)
		return IssueClientWhackAction(theApp, mWhackTargetId);
	if (mScenario == NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT)
		return IssueClientSlotMachineAction(theApp);

	return IssueHostSeedAction(theApp);
}

bool NetSmokeScenarioRuntime::ObserveActionApplied(Board* theBoard,
	const NetSmokeManifest& theManifest, std::string& theFailure)
{
	if (!theBoard || !mBaselineCaptured)
		return false;
	if (mScenario == NETSMOKE_SCENARIO_VASEBREAKER)
		return CountScaryPots(theBoard) < mBaselinePots;
	if (mScenario == NETSMOKE_SCENARIO_BEGHOULED)
		return theBoard->mChallenge &&
			(theBoard->mChallenge->mChallengeScore > mBaselineChallengeScore ||
				SmokePlantGridHash(theBoard) != mBaselinePlantGridHash);
	if (NetIsBowlingSmokeScenario(mScenario))
	{
		Plant* aNut = FindBowlingSmokeNut(theBoard, mBowlingNutId);
		if (!aNut)
			return false;
		if (aNut->mNetId == 0 || (theManifest.IsClient() && !aNut->mNetGhost))
		{
			theFailure = "Bowling action produced a plant without an authoritative ghost identity";
			return false;
		}

		const bool aTargetIsUninitialized = aNut->mNetGhost &&
			((fabsf(aNut->mNetTargetX) < 0.01f && aNut->mX != 0) ||
			 (fabsf(aNut->mNetTargetY) < 0.01f && aNut->mY != 0));
		if (aTargetIsUninitialized)
		{
			NetTrace("SMOKE_BOWLING_TARGET_INVALID role=%s id=%u pos=(%d,%d) target=(%.2f,%.2f)",
				theManifest.IsHost() ? "host" : "client", aNut->mNetId,
				aNut->mX, aNut->mY, aNut->mNetTargetX, aNut->mNetTargetY);
			theFailure = "Bowling nut ghost retained the default (0,0) interpolation target";
			return false;
		}

		if (!mBowlingNutObserved)
		{
			Reanimation* aBodyReanim = theBoard->mApp->ReanimationTryToGet(aNut->mBodyReanimID);
			if (!aNut->mVisible || !aBodyReanim || aNut->mX < -160 || aNut->mX > 900 ||
				aNut->mY < 40 || aNut->mY > 560)
			{
				NetTrace("SMOKE_BOWLING_PRESENTATION_INVALID role=%s id=%u visible=%d body=%p pos=(%d,%d)",
					theManifest.IsHost() ? "host" : "client", aNut->mNetId,
					aNut->mVisible ? 1 : 0, (void*)aBodyReanim, aNut->mX, aNut->mY);
				theFailure = "Bowling nut was created without a visible on-lawn presentation";
				return false;
			}
			mBowlingNutId = aNut->mNetId;
			mBowlingFirstX = aNut->mX;
			mBowlingFirstY = aNut->mY;
			mBowlingNutObserved = true;
			NetTrace("SMOKE_BOWLING_NUT_OBSERVED role=%s id=%u ghost=%d type=%d pos=(%d,%d) target=(%.2f,%.2f)",
				theManifest.IsHost() ? "host" : "client", aNut->mNetId,
				aNut->mNetGhost ? 1 : 0, (int)aNut->mSeedType, aNut->mX, aNut->mY,
				aNut->mNetTargetX, aNut->mNetTargetY);
		}

		if (!mBowlingNutMoved && abs(aNut->mX - mBowlingFirstX) >= 8)
		{
			mBowlingNutMoved = true;
			NetTrace("SMOKE_BOWLING_NUT_MOVING role=%s id=%u from=(%d,%d) to=(%d,%d) state=%d order=%d",
				theManifest.IsHost() ? "host" : "client", aNut->mNetId,
				mBowlingFirstX, mBowlingFirstY, aNut->mX, aNut->mY,
				(int)aNut->mState, aNut->mRenderOrder);
		}
		return mBowlingNutObserved && mBowlingNutMoved;
	}
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT)
	{
		if (mWhackTargetId == 0)
			return false;
		Zombie* aTarget = FindSmokeZombieById(theBoard, mWhackTargetId);
		// Whack-a-Zombie intentionally calls DieWithLoot directly rather than
		// PlayDeathAnim, so the stock result is immediate removal after the POW.
		// A retired client ghost can remain in DataArray for one cleanup tick with
		// mDead set; treat that as the same authoritative removal.
		if (!aTarget || aTarget->mDead)
		{
			if (!mWhackRemovalObserved)
			{
				mWhackRemovalObserved = true;
				NetTrace("SMOKE_WHACK_REMOVAL_OBSERVED role=%s targetId=%u",
					theManifest.IsHost() ? "host" : "client", mWhackTargetId);
			}
		}
		return mWhackRemovalObserved;
	}
	if (mScenario == NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT)
	{
		if (!theBoard->mChallenge || !theBoard->mSeedBank ||
			theBoard->mSeedBank->mNumPackets < 3)
		{
			return false;
		}
		const int aRoll = theBoard->mChallenge->mSlotMachineRollCount;
		if (mSlotMachineSoundCount > 1)
		{
			theFailure = "Slot Machine sound played more than once on one peer";
			return false;
		}
		const bool aRolling =
			theBoard->mChallenge->mChallengeState ==
				ChallengeState::STATECHALLENGE_SLOT_MACHINE_ROLLING &&
			aRoll > mBaselineSlotMachineRollCount &&
			theBoard->mSeedBank->mSeedPackets[0].mSlotMachineCountDown > 0;
		Reanimation* aHandle = theBoard->mApp->ReanimationTryToGet(
			theBoard->mChallenge->mReanimChallenge);
		const bool aPullVisible = aHandle && aHandle->IsAnimPlaying("anim_pull") &&
			aHandle->mAnimRate > 0.0f && aHandle->mAnimTime > 0.0f;
		if (aRolling && aPullVisible && !mSlotMachinePresentationObserved)
		{
			mSlotMachinePresentationObserved = true;
			NetTrace("SMOKE_SLOT_MACHINE_PRESENTATION role=%s roll=%d countdown=%d animTime=%.4f animRate=%.2f",
				theManifest.IsHost() ? "host" : "client", aRoll,
				theBoard->mSeedBank->mSeedPackets[0].mSlotMachineCountDown,
				aHandle->mAnimTime, aHandle->mAnimRate);
		}
		const bool aStopped =
			theBoard->mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_NORMAL &&
			aRoll == mBaselineSlotMachineRollCount + 1 &&
			theBoard->mSunMoney == mBaselineSunMoney - 25 &&
			theBoard->mSeedBank->mSeedPackets[0].mSlotMachineCountDown == 0 &&
			theBoard->mSeedBank->mSeedPackets[1].mSlotMachineCountDown == 0 &&
			theBoard->mSeedBank->mSeedPackets[2].mSlotMachineCountDown == 0;
		const bool aReturnVisible = aHandle && aHandle->IsAnimPlaying("anim_return") &&
			aHandle->mAnimRate > 0.0f;
		if (mSlotMachinePresentationObserved && aStopped && aReturnVisible &&
			mSlotMachineSoundCount == 1 && !mSlotMachineReturnObserved)
		{
			mSlotMachineReturnObserved = true;
			NetTrace("SMOKE_SLOT_MACHINE_RETURN role=%s roll=%d sun=%d sounds=%d animTime=%.4f animRate=%.2f",
				theManifest.IsHost() ? "host" : "client", aRoll, theBoard->mSunMoney,
				mSlotMachineSoundCount, aHandle->mAnimTime, aHandle->mAnimRate);
		}
		return mSlotMachineReturnObserved;
	}
	return theBoard->mPlants.mSize > mBaselinePlants;
}

bool NetSmokeScenarioRuntime::Update(LawnApp* theApp, const NetSmokeManifest& theManifest,
	uint32_t theGameplaySnapshots, std::string& theFailure)
{
	theFailure.clear();
	const NetSmokeScenarioDefinition* aDefinition = NetGetSmokeScenario(mScenario);
	if (!aDefinition)
	{
		theFailure = "smoke scenario runtime has no resolved definition";
		return false;
	}
	if (!theApp)
		return true;

	// A successful Adventure transition destroys the Board before installing
	// the award widget.  Inspect this terminal state before the ordinary
	// board-required path so the smoke runtime cannot silently stop observing at
	// the exact lifecycle boundary it is meant to verify.
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY &&
		theApp->mGameScene == GameScenes::SCENE_AWARD)
	{
		AwardScreen* anAward = theApp->mAwardScreen;
		if (theApp->mBoard || !anAward ||
			anAward->mAwardType != AwardType::AWARD_FORLEVEL ||
			theApp->GetDialog(Dialogs::DIALOG_GAME_OVER))
		{
			theFailure = "victory award scene retained a board, lacked its award UI, used the wrong award, or showed game-over";
			return false;
		}
		if (!mVictoryFadeObserved || !mVictoryLevelCompleteSnapshotObserved)
		{
			theFailure = "victory award arrived before this peer observed the synchronized fade and level-complete snapshot";
			return false;
		}
		mVictoryBoardReleasedObserved = true;
		if (!mVictoryAwardObserved)
		{
			mVictoryAwardObserved = true;
			NetTrace("SMOKE_VICTORY_AWARD role=%s award=%d board=0 screen=1 fadeCounter=%d size=%dx%d",
				theManifest.IsHost() ? "host" : "client", (int)anAward->mAwardType,
				anAward->mFadeInCounter, theApp->mWidth, theApp->mHeight);
		}
		return true;
	}
	if (!theApp->mBoard)
	{
		if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY && mVictoryFadeObserved)
		{
			theFailure = "victory destroyed the board without entering the synchronized award scene";
			return false;
		}
		return true;
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
        if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_SOAK &&
                theApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
        {
                if (!mSoakLossObserved)
                {
                        mSoakLossObserved = true;
                        NetTrace("SMOKE_SOAK_LOSS_OBSERVED role=%s scene=%d plants=%d sunClicks=%d",
                                theManifest.IsHost() ? "host" : "client",
                                (int)theApp->mGameScene, mSoakPlantsIssued, mSoakSunClicks);
                }
                if (!mSoakLossCameraObserved && aBoard->mCutScene &&
                        aBoard->mCutScene->mCutsceneTime >= 3500 &&
                        aBoard->mX == BOARD_OFFSET)
                {
                        mSoakLossCameraObserved = true;
                        NetTrace("SMOKE_SOAK_LOSS_CAMERA role=%s cutscene=%d boardX=%d",
                                theManifest.IsHost() ? "host" : "client",
                                aBoard->mCutScene->mCutsceneTime, aBoard->mX);
                }
                if (!mSoakLossBrainsObserved && aBoard->mCutScene &&
                        aBoard->mCutScene->mZombiesWonReanimID != ReanimationID::REANIMATIONID_NULL &&
                        theApp->ReanimationTryToGet(aBoard->mCutScene->mZombiesWonReanimID))
                {
                        mSoakLossBrainsObserved = true;
                        NetTrace("SMOKE_SOAK_LOSS_BRAINS role=%s cutscene=%d reanim=%u",
                                theManifest.IsHost() ? "host" : "client",
                                aBoard->mCutScene->mCutsceneTime,
                                (unsigned)aBoard->mCutScene->mZombiesWonReanimID);
                }
                if (!mSoakLossDialogObserved &&
                        theApp->GetDialog(Dialogs::DIALOG_GAME_OVER))
                {
                        mSoakLossDialogObserved = true;
                        NetTrace("SMOKE_SOAK_LOSS_DIALOG role=%s cutscene=%d",
                                theManifest.IsHost() ? "host" : "client",
                                aBoard->mCutScene ? aBoard->mCutScene->mCutsceneTime : -1);
                }
                return true;
        }
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY &&
		theApp->mGameScene == GameScenes::SCENE_PLAYING &&
		aBoard->mBoardFadeOutCounter > 0)
	{
		if (!mVictoryFadeObserved)
		{
			mVictoryFadeObserved = true;
			mVictoryTick = GetTickCount();
			NetTrace("SMOKE_VICTORY_FADE role=%s fade=%d levelComplete=%d plants=%d sunClicks=%d",
				theManifest.IsHost() ? "host" : "client", aBoard->mBoardFadeOutCounter,
				aBoard->mLevelComplete ? 1 : 0, mSoakPlantsIssued, mSoakSunClicks);
		}
	}
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY &&
		mVictoryFadeObserved && !mVictoryAwardObserved && mVictoryTick != 0 &&
		(uint32_t)(GetTickCount() - mVictoryTick) >= SMOKE_VICTORY_TERMINAL_TIMEOUT_MS)
	{
		theFailure = "victory fade did not reach the synchronized award UI within 30 seconds";
		return false;
	}
	if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY && mVictoryFadeObserved)
		return true;
        if (theApp->mGameScene != GameScenes::SCENE_PLAYING)
                return true;

	if (theManifest.IsClient())
	{
		Zombie* aGhost = nullptr;
		while (aBoard->mZombies.IterateNext(aGhost))
		{
			if (aGhost->mDead || !aGhost->mNetGhost)
				continue;
			if ((fabsf(aGhost->mNetTargetX) < 0.01f && fabsf(aGhost->mPosX) >= 0.01f) ||
				(fabsf(aGhost->mNetTargetY) < 0.01f && fabsf(aGhost->mPosY) >= 0.01f))
			{
				NetTrace("SMOKE_ZOMBIE_TARGET_INVALID id=%u type=%d pos=(%.2f,%.2f) target=(%.2f,%.2f)",
					aGhost->mNetId, (int)aGhost->mZombieType, aGhost->mPosX, aGhost->mPosY,
					aGhost->mNetTargetX, aGhost->mNetTargetY);
				theFailure = "network zombie retained the default (0,0) interpolation target";
				return false;
			}
		}
	}

	if (NetIsBowlingSmokeScenario(mScenario) &&
		!mBowlingZombieObserved && aBoard->mCurrentWave > 0)
	{
		Zombie* aZombie = nullptr;
		Zombie* aCandidate = nullptr;
		while (aBoard->mZombies.IterateNext(aZombie))
		{
			if (aZombie->mDead || aZombie->mFromWave != 0 ||
				aZombie->mNetId == 0 || (theManifest.IsClient() && !aZombie->mNetGhost))
				continue;
			if (!aCandidate || aZombie->mNetId < aCandidate->mNetId)
				aCandidate = aZombie;
		}
		if (aCandidate)
		{
			Reanimation* aBodyReanim = theApp->ReanimationTryToGet(aCandidate->mBodyReanimID);
			if (aCandidate->mPosX < 730.0f || !aBodyReanim)
			{
				NetTrace("SMOKE_ZOMBIE_SPAWN_INVALID role=%s id=%u ghost=%d type=%d pos=(%.2f,%.2f) target=(%.2f,%.2f) body=%p",
					theManifest.IsHost() ? "host" : "client", aCandidate->mNetId,
					aCandidate->mNetGhost ? 1 : 0, (int)aCandidate->mZombieType,
					aCandidate->mPosX, aCandidate->mPosY, aCandidate->mNetTargetX,
					aCandidate->mNetTargetY, (void*)aBodyReanim);
				theFailure = "first Bowling zombie presentation appeared on the lawn instead of offscreen right";
				return false;
			}
			mBowlingZombieObserved = true;
			NetTrace("SMOKE_ZOMBIE_SPAWN_OBSERVED role=%s id=%u ghost=%d type=%d pos=(%.2f,%.2f) target=(%.2f,%.2f)",
				theManifest.IsHost() ? "host" : "client", aCandidate->mNetId,
				aCandidate->mNetGhost ? 1 : 0, (int)aCandidate->mZombieType,
				aCandidate->mPosX, aCandidate->mPosY,
				aCandidate->mNetTargetX, aCandidate->mNetTargetY);
		}
	}

        if (mPlayingTick == 0)
        {
                if (NetIsAdventure8InteractiveSmokeScenario(mScenario) &&
                        !mSoakChooserVerified && aBoard->mSeedBank && theApp->mNetManager)
                {
                        int anOwnerCounts[NET_MAX_PLAYERS] = {};
                        for (int i = 0; i < aBoard->mSeedBank->mNumPackets; ++i)
                        {
                                const unsigned char anOwner =
                                        aBoard->mSeedBank->mSeedPackets[i].mNetOwnerSlot;
                                if (anOwner < NET_MAX_PLAYERS)
                                        ++anOwnerCounts[anOwner];
                        }
                        const int aHostQuota = theApp->mNetManager->mChooserPickQuota[0];
                        const int aClientQuota = theApp->mNetManager->mChooserPickQuota[1];
                        if (aHostQuota > 0 && aClientQuota > 0 &&
                                anOwnerCounts[0] == aHostQuota &&
                                anOwnerCounts[1] == aClientQuota)
                        {
                                mSoakChooserVerified = true;
                                NetTrace("SMOKE_SOAK_CHOOSER_VERIFIED role=%s local=%d remote=%d quota=%d source=seedbank",
                                        theManifest.IsHost() ? "host" : "client",
                                        anOwnerCounts[theApp->mNetManager->GetMySlot()],
                                        anOwnerCounts[theApp->mNetManager->GetMySlot() == 0 ? 1 : 0],
                                        theApp->mNetManager->mChooserPickQuota[
                                                theApp->mNetManager->GetMySlot()]);
                        }
                }
                if (NetIsBowlingSmokeScenario(mScenario) &&
			(!aBoard->mChallenge || !aBoard->mChallenge->mShowBowlingLine))
		{
			theFailure = "Bowling line was not enabled on this peer at gameplay start";
			return false;
		}
		if (NetIsBowlingSmokeScenario(mScenario))
			NetTrace("SMOKE_BOWLING_LINE role=%s visible=1",
				theManifest.IsHost() ? "host" : "client");
		if (!theManifest.mSkipIntro)
		{
			if (!mIntroObserved || !mIntroProgressObserved)
			{
				theFailure = "gameplay began without observing synchronized real intro progress";
				return false;
			}
			if (NetIsBowlingSmokeScenario(mScenario) && !mIntroSawDave)
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
		mBaselineChallengeScore = aBoard->mChallenge ? aBoard->mChallenge->mChallengeScore : 0;
		mBaselineSlotMachineRollCount = aBoard->mChallenge ?
			aBoard->mChallenge->mSlotMachineRollCount : 0;
		mBaselineSunMoney = aBoard->mSunMoney;
		mBaselinePlantGridHash = SmokePlantGridHash(aBoard);
		if (mScenario == NETSMOKE_SCENARIO_VASEBREAKER && mBaselinePots <= 0)
			return true;
		if (mScenario == NETSMOKE_SCENARIO_BEGHOULED &&
			(!aBoard->mChallenge ||
				aBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_NORMAL))
			return true;
		if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT &&
			!FindWhackSmokeTarget(aBoard))
			return true;
		if (mScenario == NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT &&
			(!aBoard->mChallenge || !aBoard->mSeedBank ||
			 aBoard->mSunMoney < 25 || aBoard->mSeedBank->mNumPackets < 3 ||
			 aBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_NORMAL ||
			 !theApp->ReanimationTryToGet(aBoard->mChallenge->mReanimChallenge)))
		{
			return true;
		}
                mBaselineCaptured = true;
                mActionTick = GetTickCount();
                mSoakLastActionTick = mActionTick;
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=BASELINE plants=%d pots=%d zombies=%d snapshots=%u",
			theManifest.IsHost() ? "host" : "client", aDefinition->mName,
                        mBaselinePlants, mBaselinePots, aBoard->mZombies.mSize, theGameplaySnapshots);
        }

        if (NetIsAdventure8InteractiveSmokeScenario(mScenario))
        {
                const uint32_t aNow = GetTickCount();
                const uint32_t aSoakElapsed = aNow - mPlayingTick;
                // Stagger the two peers so their real input streams overlap without
                // repeatedly racing for the same sun or lawn cell.
                const uint32_t anOffset = theManifest.IsClient() ? 1800U : 0U;
                if (aSoakElapsed >= 2500U + anOffset &&
                        (uint32_t)(aNow - mSoakLastActionTick) >= 4500U)
                {
                        bool anIssued = false;
                        if (mSoakPlantsIssued < 3)
                        {
                                anIssued = IssueSoakSeedAction(theApp, theManifest.IsClient());
                                if (anIssued)
                                        ++mSoakPlantsIssued;
                        }
                        if (!anIssued)
                        {
                                anIssued = IssueSoakSunClick(theApp, theManifest.IsClient());
                                if (anIssued)
                                        ++mSoakSunClicks;
                        }
                        if (anIssued)
                                mSoakLastActionTick = aNow;
                }
                if (!mActionIssued && mSoakPlantsIssued > 0)
                {
                        mActionIssued = true;
                        mActionTick = aNow;
                        NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=ACTION_ISSUED owner=%s",
                                theManifest.IsHost() ? "host" : "client", aDefinition->mName,
                                theManifest.IsHost() ? "host" : "client");
                }
                if (!mActionApplied && aBoard->mPlants.mSize > mBaselinePlants)
                {
                        mActionApplied = true;
                        NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=ACTION_APPLIED plants=%d baseline=%d",
                                theManifest.IsHost() ? "host" : "client", aDefinition->mName,
                                aBoard->mPlants.mSize, mBaselinePlants);
                }
                if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY &&
			aSoakElapsed >= SMOKE_VICTORY_INTERACTION_MS && !mVictoryIssued)
		{
			if (mSoakPlantsIssued < 1 || !mActionApplied)
			{
				theFailure = "Adventure victory reached its transition without a real local plant input and applied action";
				return false;
			}
			if (theManifest.IsHost())
			{
				mVictoryIssued = true;
				mVictoryTick = aNow;
				NetTrace("SMOKE_VICTORY_FORCE_FADE role=host elapsed=%u plants=%d sunClicks=%d snapshots=%u",
					aSoakElapsed, mSoakPlantsIssued, mSoakSunClicks,
					theGameplaySnapshots);
				// Use the stock completion sequence: win audio, mower scoring,
				// board fade, level-complete snapshot and award transition.
				aBoard->FadeOutLevel();
			}
		}
                else if (mScenario == NETSMOKE_SCENARIO_ADVENTURE_8_SOAK &&
			aSoakElapsed >= SMOKE_SOAK_DURATION_MS && !mSoakLossIssued)
                {
                        if (mSoakPlantsIssued < 1 || mSoakSunClicks < 1 || !mActionApplied)
                        {
                                theFailure = "Adventure soak reached one minute without a local plant, sun collection, and applied action";
                                return false;
                        }
                        if (theManifest.IsHost())
                        {
                                mSoakLossIssued = true;
                                NetTrace("SMOKE_SOAK_FORCE_LOSS role=host elapsed=%u plants=%d sunClicks=%d snapshots=%u",
                                        aSoakElapsed, mSoakPlantsIssued, mSoakSunClicks,
                                        theGameplaySnapshots);
                                aBoard->ZombiesWon(nullptr);
                        }
                }
                return true;
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

	if (!mActionApplied && ObserveActionApplied(aBoard, theManifest, theFailure))
	{
		mActionApplied = true;
		NetTrace("SMOKE_CHECKPOINT role=%s scenario=%s step=ACTION_APPLIED plants=%d pots=%d targetId=%u",
			theManifest.IsHost() ? "host" : "client", aDefinition->mName,
			aBoard->mPlants.mSize, CountScaryPots(aBoard), mWhackTargetId);
	}
	if (!theFailure.empty())
		return false;
	if (mActionIssued && !mActionApplied &&
		(uint32_t)(GetTickCount() - mActionTick) >= 30000)
	{
		theFailure = "scripted smoke action was not applied within 30 seconds";
		return false;
	}
	if (NetIsBowlingSmokeScenario(mScenario) && !mActionApplied &&
		(uint32_t)(GetTickCount() - mActionTick) >= 30000)
	{
		theFailure = "Bowling nut was not visibly moving on this peer within 30 seconds";
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

void NetSmokeScenarioRuntime::OnLevelCompleteSnapshot(bool theHost, int theFadeCounter)
{
	if (mScenario != NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY ||
		mVictoryLevelCompleteSnapshotObserved)
	{
		return;
	}
	mVictoryLevelCompleteSnapshotObserved = true;
	NetTrace("SMOKE_VICTORY_LEVEL_COMPLETE_SNAPSHOT role=%s fade=%d",
		theHost ? "host" : "client", theFadeCounter);
}

void NetSmokeScenarioRuntime::OnAuthoritativeWhackHit(uint16_t theZombieId)
{
	if (mScenario != NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT || theZombieId == 0)
		return;
	mWhackTargetId = theZombieId;
	NetTrace("SMOKE_WHACK_HOST_HIT targetId=%u", mWhackTargetId);
}

void NetSmokeScenarioRuntime::OnSlotMachineSound(bool theHost)
{
	if (mScenario != NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT)
		return;
	++mSlotMachineSoundCount;
	NetTrace("SMOKE_SLOT_MACHINE_SOUND role=%s count=%d",
		theHost ? "host" : "client", mSlotMachineSoundCount);
}

bool NetSmokeScenarioRuntime::IsComplete(uint32_t theRequiredPostActionSnapshots) const
{
        return mActionApplied &&
                (mScenario != NETSMOKE_SCENARIO_ADVENTURE_8_SOAK ||
                        (mSoakChooserVerified && mSoakLossObserved &&
                         mSoakLossCameraObserved && mSoakLossBrainsObserved &&
                         mSoakLossDialogObserved && mSoakPlantsIssued > 0 &&
                         mSoakSunClicks > 0)) &&
		(mScenario != NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY ||
			(mSoakChooserVerified && mVictoryFadeObserved &&
			 mVictoryLevelCompleteSnapshotObserved && mVictoryAwardObserved &&
			 mVictoryBoardReleasedObserved && mSoakPlantsIssued > 0)) &&
                (!NetIsBowlingSmokeScenario(mScenario) || mBowlingZombieObserved) &&
		theRequiredPostActionSnapshots > 0 &&
		mPostActionSamples >= theRequiredPostActionSnapshots;
}

} // namespace LawnNet
