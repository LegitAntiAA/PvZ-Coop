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
#include "../System/Music.h"
#include "../Zombie.h"
#include "../Widget/GameButton.h"
#include "../Widget/AwardScreen.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Sexy.TodLib/Reanimator.h"

#include <windows.h>
#include <sstream>

namespace LawnNet
{
namespace
{
const uint32_t SMOKE_INTRO_TIMEOUT_MS = 60000;
const uint32_t SMOKE_INTRO_ACTION_INTERVAL_MS = 500;
const uint32_t SMOKE_SOAK_DURATION_MS = 60000;
// Give both peers enough time to earn and collect shared sun after their real
// chooser picks.  Twenty seconds was too short when the client owned only
// higher-cost packets and correctly caused a false-negative before its first
// legitimate planting opportunity.
const uint32_t SMOKE_VICTORY_INTERACTION_MS = 60000;
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

bool ValidateTowerDefenseSmokePath(Board* theBoard,
	const NetSmokeManifest& theManifest, std::string& theFailure)
{
	if (!theBoard || theBoard->mTDPath.empty() ||
		(theBoard->mTDPath.size() & 1U) != 0U)
	{
		theFailure = "Tower Defense path is empty or has an odd byte count";
		return false;
	}
	int aPreviousRow = -1;
	int aPreviousCol = -1;
	int anEdgeVisits[4] = {};
	int anEdgeVisitCount = 0;
	int aDirtCells = 0;
	int aPathIndex[MAX_GRID_SIZE_X][5];
	for (int aCol = 0; aCol < MAX_GRID_SIZE_X; ++aCol)
		for (int aRow = 0; aRow < 5; ++aRow)
			aPathIndex[aCol][aRow] = -1;
	for (size_t i = 0; i < theBoard->mTDPath.size(); i += 2)
	{
		const uint8_t aRowByte = theBoard->mTDPath[i];
		const int aRow = aRowByte >> 1;
		const int aCol = theBoard->mTDPath[i + 1];
		if ((aRowByte & 1U) != 0U || aRow < 0 || aRow >= 5 ||
			aCol < 0 || aCol >= MAX_GRID_SIZE_X)
		{
			theFailure = "Tower Defense path contains an out-of-range cell";
			return false;
		}
		if (aPathIndex[aCol][aRow] >= 0)
		{
			theFailure = "Tower Defense path repeats a cell";
			return false;
		}
		aPathIndex[aCol][aRow] = (int)(i / 2);
		if (i == 0)
		{
			if (aCol != MAX_GRID_SIZE_X - 1)
			{
				theFailure = "Tower Defense path does not start at the right edge";
				return false;
			}
		}
		else if (abs(aRow - aPreviousRow) + abs(aCol - aPreviousCol) != 1)
		{
			theFailure = "Tower Defense path contains a disconnected step";
			return false;
		}
		const int anEdge = aRow == 0 ? 0 : (aRow == 4 ? 1 : -1);
		if (anEdge >= 0 && aPreviousRow != aRow)
		{
			if (anEdgeVisitCount < 4)
				anEdgeVisits[anEdgeVisitCount] = anEdge;
			++anEdgeVisitCount;
		}
		if (theBoard->mGridSquareType[aCol][aRow] != GridSquareType::GRIDSQUARE_DIRT)
		{
			theFailure = "Tower Defense path cell was not converted to dug dirt";
			return false;
		}
		++aDirtCells;
		aPreviousRow = aRow;
		aPreviousCol = aCol;
	}
	const bool anAlternatingEdges = anEdgeVisitCount == 3 &&
		anEdgeVisits[0] == anEdgeVisits[2] &&
		anEdgeVisits[0] != anEdgeVisits[1];
	if (aPreviousCol != 0 || !anAlternatingEdges)
	{
		theFailure = "Tower Defense path missed its left edge or alternating edge pattern";
		return false;
	}
	for (int aCol = 0; aCol < MAX_GRID_SIZE_X; ++aCol)
	{
		for (int aRow = 0; aRow < 5; ++aRow)
		{
			if (aCol + 1 < MAX_GRID_SIZE_X && aRow + 1 < 5 &&
				aPathIndex[aCol][aRow] >= 0 && aPathIndex[aCol + 1][aRow] >= 0 &&
				aPathIndex[aCol][aRow + 1] >= 0 && aPathIndex[aCol + 1][aRow + 1] >= 0)
			{
				theFailure = "Tower Defense path contains a filled 2x2 road block";
				return false;
			}
			if (aPathIndex[aCol][aRow] < 0)
				continue;
			if (aCol + 1 < MAX_GRID_SIZE_X && aPathIndex[aCol + 1][aRow] >= 0 &&
				abs(aPathIndex[aCol][aRow] - aPathIndex[aCol + 1][aRow]) != 1)
			{
				theFailure = "Tower Defense path has touching parallel horizontal runs";
				return false;
			}
			if (aRow + 1 < 5 && aPathIndex[aCol][aRow + 1] >= 0 &&
				abs(aPathIndex[aCol][aRow] - aPathIndex[aCol][aRow + 1]) != 1)
			{
				theFailure = "Tower Defense path has touching parallel vertical runs";
				return false;
			}
		}
	}
	bool aGrassOutsideRoad = false;
	for (int aRow = 0; aRow < 5 && !aGrassOutsideRoad; ++aRow)
		for (int aCol = 0; aCol < MAX_GRID_SIZE_X; ++aCol)
			if (!theBoard->NetTdIsPathCell(aCol, aRow) &&
				theBoard->mGridSquareType[aCol][aRow] != GridSquareType::GRIDSQUARE_DIRT)
			{
				aGrassOutsideRoad = true;
				break;
			}
	if (!aGrassOutsideRoad || !Sexy::IMAGE_BACKGROUND1UNSODDED ||
		Sexy::IMAGE_BACKGROUND1UNSODDED->GetWidth() <= 0 ||
		Sexy::IMAGE_BACKGROUND1UNSODDED->GetHeight() <= 0)
	{
		theFailure = "Tower Defense dug-road image or surrounding grass is unavailable";
		return false;
	}
	const uint32_t aHash = NetFnv1a32(theBoard->mTDPath.data(), theBoard->mTDPath.size());
	NetTrace("SMOKE_TD_PATH role=%s bytes=%zu hash=%08x edgeVisits=%d edgePattern=%s separated=1 no2x2=1 dirt=%d grass=1 image=%dx%d",
		theManifest.IsHost() ? "host" : "client", theBoard->mTDPath.size(),
		aHash, anEdgeVisitCount, anEdgeVisits[0] == 0 ? "TBT" : "BTB", aDirtCells,
		Sexy::IMAGE_BACKGROUND1UNSODDED->GetWidth(),
		Sexy::IMAGE_BACKGROUND1UNSODDED->GetHeight());
	return true;
}

struct PortalSmokeRecord
{
	uint16_t mId;
	uint8_t mType;
	uint8_t mState;
	uint8_t mGridX;
	uint8_t mGridY;
	int mTrackCount;
};

bool ValidatePortalCombatSmokeLayout(Board* theBoard,
	const NetSmokeManifest& theManifest, std::string& theFailure,
	uint32_t* theLayoutHash = nullptr)
{
	if (!theBoard || !theBoard->mApp)
	{
		theFailure = "Portal Combat board is unavailable";
		return false;
	}

	PortalSmokeRecord aRecords[4] = {};
	int aCount = 0;
	int aCircleCount = 0;
	int aSquareCount = 0;
	int aTotalTracks = 0;
	GridItem* aGridItem = nullptr;
	while (theBoard->mGridItems.IterateNext(aGridItem))
	{
		if (aGridItem->mDead ||
			(aGridItem->mGridItemType != GridItemType::GRIDITEM_PORTAL_CIRCLE &&
			 aGridItem->mGridItemType != GridItemType::GRIDITEM_PORTAL_SQUARE))
			continue;
		if (aCount >= 4 || aGridItem->mNetId == 0 ||
			aGridItem->mGridItemState != GridItemState::GRIDITEM_STATE_NORMAL ||
			aGridItem->mGridX < 0 || aGridItem->mGridX > MAX_GRID_SIZE_X ||
			aGridItem->mGridY < 0 || aGridItem->mGridY >= 5)
		{
			NetTrace("SMOKE_PORTAL_LAYOUT_INVALID role=%s count=%d id=%u type=%d state=%d cell=(%d,%d)",
				theManifest.IsHost() ? "host" : "client", aCount + 1,
				aGridItem->mNetId, (int)aGridItem->mGridItemType,
				(int)aGridItem->mGridItemState, aGridItem->mGridX, aGridItem->mGridY);
			theFailure = "Portal Combat contains an invalid or extra live portal";
			return false;
		}

		Reanimation* aReanim =
			theBoard->mApp->ReanimationTryToGet(aGridItem->mGridItemReanimID);
		const ReanimationType anExpectedType =
			aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ?
			ReanimationType::REANIM_PORTAL_CIRCLE : ReanimationType::REANIM_PORTAL_SQUARE;
		bool aRealImage = false;
		int aTrackCount = 0;
		if (aReanim && !aReanim->mDead && aReanim->mReanimationType == anExpectedType &&
			aReanim->mDefinition && aReanim->mDefinition->mTracks.tracks &&
			aReanim->mDefinition->mTracks.count > 0)
		{
			aTrackCount = aReanim->mDefinition->mTracks.count;
			for (int i = 0; i < aTrackCount; ++i)
			{
				const char* aTrackName = aReanim->mDefinition->mTracks.tracks[i].mName;
				if (!aTrackName || !*aTrackName)
					continue;
				Image* anImage = aReanim->GetCurrentTrackImage(aTrackName);
				if (anImage && anImage->GetWidth() > 1 && anImage->GetHeight() > 1)
				{
					aRealImage = true;
					break;
				}
			}
		}
		if (!aRealImage)
		{
			NetTrace("SMOKE_PORTAL_RESOURCE_INVALID role=%s id=%u type=%d reanim=%p expected=%d actual=%d tracks=%d",
				theManifest.IsHost() ? "host" : "client", aGridItem->mNetId,
				(int)aGridItem->mGridItemType, (void*)aReanim, (int)anExpectedType,
				aReanim ? (int)aReanim->mReanimationType : -1, aTrackCount);
			theFailure = "Portal Combat portal reanimation has no real image resource";
			return false;
		}

		PortalSmokeRecord aRecord = {};
		aRecord.mId = aGridItem->mNetId;
		aRecord.mType = (uint8_t)aGridItem->mGridItemType;
		aRecord.mState = (uint8_t)aGridItem->mGridItemState;
		aRecord.mGridX = (uint8_t)aGridItem->mGridX;
		aRecord.mGridY = (uint8_t)aGridItem->mGridY;
		aRecord.mTrackCount = aTrackCount;
		int anInsert = aCount;
		while (anInsert > 0 && aRecords[anInsert - 1].mId > aRecord.mId)
		{
			aRecords[anInsert] = aRecords[anInsert - 1];
			--anInsert;
		}
		aRecords[anInsert] = aRecord;
		++aCount;
		aTotalTracks += aTrackCount;
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE)
			++aCircleCount;
		else
			++aSquareCount;
	}

	if (aCount != 4 || aCircleCount != 2 || aSquareCount != 2)
	{
		NetTrace("SMOKE_PORTAL_LAYOUT_INVALID role=%s count=%d circles=%d squares=%d",
			theManifest.IsHost() ? "host" : "client", aCount, aCircleCount, aSquareCount);
		theFailure = "Portal Combat did not expose exactly two live portals of each type";
		return false;
	}
	for (int i = 1; i < aCount; ++i)
	{
		if (aRecords[i - 1].mId == aRecords[i].mId)
		{
			theFailure = "Portal Combat portals reused a network identity";
			return false;
		}
	}

	uint8_t aCanonical[24] = {};
	for (int i = 0; i < 4; ++i)
	{
		const int aBase = i * 6;
		aCanonical[aBase] = (uint8_t)(aRecords[i].mId & 0xFFU);
		aCanonical[aBase + 1] = (uint8_t)(aRecords[i].mId >> 8);
		aCanonical[aBase + 2] = aRecords[i].mType;
		aCanonical[aBase + 3] = aRecords[i].mState;
		aCanonical[aBase + 4] = aRecords[i].mGridX;
		aCanonical[aBase + 5] = aRecords[i].mGridY;
	}
	const uint32_t aHash = NetFnv1a32(aCanonical, sizeof(aCanonical));
	if (theLayoutHash)
		*theLayoutHash = aHash;
	NetTrace("SMOKE_PORTAL_LAYOUT role=%s count=4 circles=2 squares=2 hash=%08x ids=%u,%u,%u,%u types=%u,%u,%u,%u cells=%u:%u,%u:%u,%u:%u,%u:%u states=%u,%u,%u,%u resources=4 tracks=%d",
		theManifest.IsHost() ? "host" : "client", aHash,
		aRecords[0].mId, aRecords[1].mId, aRecords[2].mId, aRecords[3].mId,
		aRecords[0].mType, aRecords[1].mType, aRecords[2].mType, aRecords[3].mType,
		aRecords[0].mGridX, aRecords[0].mGridY,
		aRecords[1].mGridX, aRecords[1].mGridY,
		aRecords[2].mGridX, aRecords[2].mGridY,
		aRecords[3].mGridX, aRecords[3].mGridY,
		aRecords[0].mState, aRecords[1].mState,
		aRecords[2].mState, aRecords[3].mState, aTotalTracks);
	return true;
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

bool IssuePortalClientSeedPick(LawnApp* theApp, int thePreferredRow,
	uint16_t& thePacketId, int& thePacketIndex, int& thePlantColumn, int& thePlantRow)
{
	Board* aBoard = theApp ? theApp->mBoard : nullptr;
	if (!aBoard || !aBoard->mSeedBank || !theApp->mNetManager ||
		theApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT)
		return false;
	const unsigned char anOwner = (unsigned char)theApp->mNetManager->GetMySlot();
	for (int i = 0; i < aBoard->mSeedBank->mNumPackets; ++i)
	{
		SeedPacket* aPacket = &aBoard->mSeedBank->mSeedPackets[i];
		if (aPacket->mPacketType == SeedType::SEED_NONE || aPacket->mNetId == 0 ||
			!aPacket->CanPickUp() ||
			(aPacket->mNetOwnerSlot != anOwner &&
			 aPacket->mNetOwnerSlot != NET_SHARED_OWNER))
			continue;
		for (int aRowPass = 0; aRowPass < 5; ++aRowPass)
		{
			const int aRow = (thePreferredRow + aRowPass + 5) % 5;
			for (int aColumn = 0; aColumn < 5; ++aColumn)
			{
				if (aBoard->GetTopPlantAt(aColumn, aRow, PlantPriority::TOPPLANT_ANY) ||
					aBoard->CanPlantAt(aColumn, aRow, aPacket->mPacketType) !=
						PlantingReason::PLANTING_OK)
					continue;
				const int aPacketX = aBoard->mSeedBank->mX + aPacket->mX +
					aPacket->mOffsetX + aPacket->mWidth / 2;
				const int aPacketY = aBoard->mSeedBank->mY + aPacket->mY +
					aPacket->mHeight / 2;
				SmokeClientClick(theApp, aPacketX, aPacketY);
				thePacketId = aPacket->mNetId;
				thePacketIndex = i;
				thePlantColumn = aColumn;
				thePlantRow = aRow;
				NetTrace("SMOKE_PORTAL_CLIENT_INPUT packetId=%u index=%d owner=%u type=%d cell=(%d,%d)",
					aPacket->mNetId, i, aPacket->mNetOwnerSlot,
					(int)aPacket->mPacketType, aColumn, aRow);
				return true;
			}
		}
	}
	return false;
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

void NetPrepareSmokeScenarioLobby(NetSmokeScenarioId theScenario, NetManager* theManager)
{
	if (theScenario != NETSMOKE_SCENARIO_PVP_1V1 || !theManager || !theManager->IsActive())
		return;

	if (theManager->IsHosting())
	{
		bool aChanged = false;
		NetPlayerInfo& aHost = theManager->GetPlayerMutable(0);
		if (aHost.mActive && aHost.mTeam != NETTEAM_PLANTS)
		{
			aHost.mTeam = NETTEAM_PLANTS;
			aChanged = true;
		}
		NetPlayerInfo& aClient = theManager->GetPlayerMutable(1);
		if (aClient.mActive && aClient.mTeam != NETTEAM_ZOMBIES)
		{
			aClient.mTeam = NETTEAM_ZOMBIES;
			aChanged = true;
		}
		if (aChanged)
		{
			theManager->SendLobbyState();
			NetTrace("SMOKE_PVP_TEAM_ASSIGN role=host host=plants client=zombies players=%d",
				theManager->GetPlayerCount());
		}
	}
	else if (theManager->IsConnectedToHost() && theManager->GetMyTeam() != NETTEAM_ZOMBIES)
	{
		// Exercise the ordinary client team request as well.  The host-side
		// deterministic assignment above closes the startup race before PreNewGame.
		theManager->SetMyTeam(NETTEAM_ZOMBIES);
		NetTrace("SMOKE_PVP_TEAM_ASSIGN role=client requested=zombies slot=%d",
			theManager->GetMySlot());
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
	mTdPathVerified = false;
	mTdSpawnAccelerated = false;
	mTdZombieEntryObserved = false;
	mTdZombieTurnObserved = false;
	mTdClientPlantObserved = false;
	mTdZombieId = 0;
	mTdPlantId = 0;
	mTdZombieFirstX = 0.0f;
	mPortalLayoutVerified = false;
	mPortalConveyorVerified = false;
	mPortalSpawnAccelerated = false;
	mPortalZombieEntryObserved = false;
	mPortalZombieTeleportObserved = false;
	mPortalClientPlantObserved = false;
	mPortalAdviceShown = false;
	mPortalAdviceCleared = false;
	mPortalAdviceClearTick = 0;
	mPortalRelocationVerified = false;
	mPortalCameraVerified = false;
	mPortalCameraMovementVerified = false;
	mPortalCameraSteadySamples = 0;
	mPortalCameraMaxDelta = 0;
        mPortalZombieId = 0;
        mPortalPlantId = 0;
	mPortalHeldPacketId = 0;
	mPortalHeldPacketIndex = -1;
	mPortalPlantColumn = -1;
	mPortalPlantRow = -1;
	mPortalHeldPacketEchoed = false;
	mPortalHeldEchoMismatch = false;
        mPortalPlantClickIssued = false;
		mPvpRosterVerified = false;
		mPvpDeckVerified = false;
		mPvpInitialBrainsVerified = false;
		mPvpPassiveBrainsVerified = false;
		mPvpCursorHiddenVerified = false;
		mPvpCursorMotionIssued = false;
		mPvpZombieEntryObserved = false;
		mPvpHostPlantIssued = false;
		mPvpHostPlantObserved = false;
		mPvpZombieId = 0;
		mPvpBaselineMaxZombieId = 0;
		mPvpBrainsAfterAction = -1;
		mPvpInitialReadyTick = 0;
        mPortalConveyorInitialCounter = -1;
	mPortalZombieEntryRow = -1;
	mPortalZombieEntryTick = 0;
	mPortalZombieEntryX = 0.0f;
	mPortalInitialLayoutHash = 0;
	mPortalInitialCameraX = 0;
	mPortalInitialCameraY = 0;
	memset(mPortalObservedZombieIds, 0, sizeof(mPortalObservedZombieIds));
	memset(mPortalObservedZombieRows, 0, sizeof(mPortalObservedZombieRows));
        memset(mPortalObservedZombieXs, 0, sizeof(mPortalObservedZombieXs));
        memset(mPortalObservedZombieTicks, 0, sizeof(mPortalObservedZombieTicks));
        memset(mPortalObservedZombieEligible, 0, sizeof(mPortalObservedZombieEligible));
        mEndlessModeVerified = false;
        mEndlessWeatherAccelerated = false;
        mEndlessTimeChanged = false;
        mEndlessWaveProgressed = false;
	mEndlessStageTriggered = false;
	mEndlessRepickOpened = false;
	mEndlessRepickLogged = false;
	mEndlessRepickCompleted = false;
	mEndlessHealthScaled = false;
	mEndlessThawed = false;
	mEndlessInitialPhase = 0;
	mEndlessInitialWave = 0;
	mEndlessInitialStage = 0;
	mEndlessWaveTick = 0;
	mEndlessStageTick = 0;
	mEndlessThawReanim = 0;
	mEndlessThawAnimTime = 0.0f;
	mEndlessThawSampleTick = 0;
	mEndlessPreRepickMaxZombieId = 0;
	memset(mEndlessBaseHealth, 0, sizeof(mEndlessBaseHealth));
	mOverflowModeVerified = false;
	mOverflowStage255Set = false;
	mOverflowStage255Observed = false;
	mOverflowGargSpawned = false;
	mOverflowGargObserved = false;
	mOverflowTransitionTriggered = false;
	mOverflowRepickOpened = false;
	mOverflowRepickLogged = false;
	mOverflowStage256Observed = false;
	mOverflowRepickCompleted = false;
	mOverflowBaselineMaxZombieId = 0;
	mOverflowGargId = 0;
	mOverflowStageSetTick = 0;
	mOverflowGargTick = 0;
	mOverflowTransitionTick = 0;
	mOverflowGargBodyHealth = 0;
	mOverflowGargMaxHealth = 0;
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
	if (mScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW && mOverflowModeVerified &&
		mOverflowStage255Observed && !mOverflowRepickOpened && aBoard->mChallenge &&
		aBoard->mChallenge->mSurvivalStage == 256)
	{
		// The stock CheckForGameEnd/InitSurvivalStage transition owns stage 256.
		// Reset only smoke bookkeeping so both peers exercise the second chooser.
		mOverflowRepickOpened = true;
		mOverflowTransitionTick = aNow;
		mIntroObserved = false;
		mIntroProgressObserved = false;
		mIntroSawDave = false;
		mIntroTick = aNow;
		mIntroLastCutsceneTime = -1;
		mIntroLastDaveMessage = -2;
		mIntroLastTutorialState = -1;
		mIntroLastActionTick = 0;
		mPlayingTick = 0;
		mSoakChooserVerified = false;
		mSoakChooserLastCount = -1;
		mChooserStartIssued = false;
	}
	if (mScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW && mOverflowRepickOpened &&
		!mOverflowRepickLogged && aCutScene->mSeedChoosing &&
		theApp->mSeedChooserScreen)
	{
		mOverflowRepickLogged = true;
		NetTrace("SMOKE_ENDLESS_OVERFLOW_REPICK_OPEN role=%s stage=%d scene=%d seedChoosing=1 chooser=%p wavesPerStage=%d",
			theManifest.IsHost() ? "host" : "client",
			aBoard->mChallenge->mSurvivalStage, (int)theApp->mGameScene,
			(void*)theApp->mSeedChooserScreen,
			aBoard->GetNumWavesPerSurvivalStage());
	}
	if (mScenario == NETSMOKE_SCENARIO_ENDLESS_ST && mEndlessModeVerified &&
                !mEndlessRepickOpened && aBoard->mChallenge &&
                aBoard->mChallenge->mSurvivalStage > mEndlessInitialStage)
        {
                // CheckForGameEnd/InitSurvivalStage owns this transition.  Reset only
                // smoke bookkeeping so the second real chooser is exercised afresh.
                mEndlessRepickOpened = true;
                mEndlessStageTick = aNow;
                mIntroObserved = false;
                mIntroProgressObserved = false;
                mIntroSawDave = false;
                mIntroTick = aNow;
                mIntroLastCutsceneTime = -1;
		mIntroLastDaveMessage = -2;
		mIntroLastTutorialState = -1;
		mIntroLastActionTick = 0;
		mPlayingTick = 0;
		mSoakChooserVerified = false;
                mSoakChooserLastCount = -1;
                mChooserStartIssued = false;
	}
	if (mScenario == NETSMOKE_SCENARIO_ENDLESS_ST && mEndlessRepickOpened &&
		!mEndlessRepickLogged && aCutScene->mSeedChoosing &&
		theApp->mSeedChooserScreen)
	{
		mEndlessRepickLogged = true;
		NetTrace("SMOKE_ENDLESS_ST_REPICK_OPEN role=%s stage=%d scene=%d seedChoosing=1 chooser=%p wavesPerStage=%d",
			theManifest.IsHost() ? "host" : "client",
			aBoard->mChallenge->mSurvivalStage, (int)theApp->mGameScene,
			(void*)theApp->mSeedChooserScreen,
			aBoard->GetNumWavesPerSurvivalStage());
	}
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
        if ((NetIsInteractiveChooserSmokeScenario(mScenario) ||
			mScenario == NETSMOKE_SCENARIO_PVP_1V1) &&
                aCutScene->mSeedChoosing && theApp->mSeedChooserScreen)
        {
                SeedChooserScreen* aChooser = theApp->mSeedChooserScreen;
                if (!aChooser->mCoopChooserActive || !aChooser->mMouseVisible)
                        return true;
                const int aLocalSlot = theApp->mNetManager ?
                        theApp->mNetManager->GetMySlot() : (theManifest.IsHost() ? 0 : 1);
                const int aOtherSlot = aLocalSlot == 0 ? 1 : 0;
                const int aLocalCount = aChooser->CoopGetLocalQuotaPickCount();
		const bool aPvpChooser = mScenario == NETSMOKE_SCENARIO_PVP_1V1;
		const bool aPvpZombie = aPvpChooser && theApp->mNetManager &&
			theApp->mNetManager->GetMyTeam() == NETTEAM_ZOMBIES;
		const int aQuota = theApp->mNetManager ?
			theApp->mNetManager->mChooserPickQuota[aLocalSlot] :
			aChooser->mCoopPicksPerPlayer;
                if (aLocalCount != mSoakChooserLastCount)
                {
                        mSoakChooserLastCount = aLocalCount;
                        NetTrace("SMOKE_SOAK_CHOOSER_PROGRESS role=%s local=%d remote=%d quota=%d flight=%d",
                                theManifest.IsHost() ? "host" : "client", aLocalCount,
                                aChooser->mCoopRemotePickCount[aOtherSlot], aQuota,
                                aChooser->mSeedsInFlight);
                }
		if (aPvpZombie)
		{
			if (aQuota != 0 || aLocalCount != 0)
			{
				theFailure = "PvP zombie chooser received a non-zero plant quota";
				return false;
			}
			if (!mSoakChooserVerified)
			{
				mSoakChooserVerified = true;
				NetTrace("SMOKE_PVP_CHOOSER_VERIFIED role=client team=zombies quota=0 picks=0");
			}
			return true;
		}
                if (aQuota <= 0)
                {
                        theFailure = "Adventure soak chooser unexpectedly assigned a zero pick quota";
                        return false;
                }
                if (aLocalCount < aQuota && aChooser->mSeedsInFlight == 0 &&
                        (uint32_t)(aNow - mIntroLastActionTick) >= SMOKE_INTRO_ACTION_INTERVAL_MS)
                {
				// The generic soak intentionally separates peers by choosing from
				// opposite ends of the chooser.  The far end is made of expensive
				// upgrade packets, though, so Tower Defense uses the next five stock
				// plants after the host's first five.  This still exercises real
				// 5+5 ownership while guaranteeing at least one packet can satisfy
				// the normal cost/prerequisite rules during the bounded smoke run.
				const bool aSafeClientStock =
					mScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT ||
					mScenario == NETSMOKE_SCENARIO_ENDLESS_ST ||
					mScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW;
                                const int aStart = theManifest.IsClient() ?
                                        (aSafeClientStock ? 5 : NUM_SEEDS_IN_CHOOSER - 1) : 0;
                                const int aStep = theManifest.IsClient() && !aSafeClientStock ? -1 : 1;
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
		bool anAllEligibleReady = aLocalCount == aQuota;
		if (aPvpChooser && theApp->mNetManager)
		{
			for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
			{
				const NetPlayerInfo& aPlayer = theApp->mNetManager->GetPlayer(aSlot);
				if (!aPlayer.mActive || aPlayer.mTeam != NETTEAM_PLANTS)
					continue;
				const int aCount = aSlot == aLocalSlot ? aLocalCount :
					aChooser->mCoopRemotePickCount[aSlot];
				if (aCount != theApp->mNetManager->mChooserPickQuota[aSlot])
					anAllEligibleReady = false;
			}
		}
		else
		{
			anAllEligibleReady = anAllEligibleReady && aRemoteCount == aQuota;
		}
                if (anAllEligibleReady && aChooser->mSeedsInFlight == 0)
                {
                        if (!mSoakChooserVerified)
                        {
                                mSoakChooserVerified = true;
				NetTrace(aPvpChooser ?
					"SMOKE_PVP_CHOOSER_VERIFIED role=%s team=plants local=%d remote=%d quota=%d" :
					"SMOKE_SOAK_CHOOSER_VERIFIED role=%s local=%d remote=%d quota=%d",
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
	if (mScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT)
		return IssueSoakSeedAction(theApp, true);
	if (mScenario == NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT)
		return false; // Portal smoke uses a deliberate pick -> host echo -> plant sequence.

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
		if (!mVictoryFadeObserved)
		{
			theFailure = "victory award arrived before this peer observed the synchronized fade";
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
			if (fabsf(aGhost->mNetTargetX) < 0.01f &&
				fabsf(aGhost->mNetTargetY) < 0.01f &&
				(fabsf(aGhost->mPosX) >= 0.01f || fabsf(aGhost->mPosY) >= 0.01f))
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
                if (mScenario == NETSMOKE_SCENARIO_ENDLESS_ST && !mEndlessModeVerified)
                {
                        if (theApp->mGameMode != GameMode::GAMEMODE_COOP_ENDLESS_ST ||
                                !theApp->IsCoopEndlessMode(theApp->mGameMode) ||
                                !theApp->IsCoopEndlessRepick(theApp->mGameMode) ||
                                !aBoard->IsEndlessT() || !aBoard->mChallenge ||
                                aBoard->GetNumWavesPerSurvivalStage() != 10)
                        {
                                theFailure = "EndlessST did not start with endless, timed-weather, ten-wave, and repick properties combined";
                                return false;
                        }
                        mEndlessModeVerified = true;
                        mEndlessInitialPhase = aBoard->mNetDayPhase;
                        mEndlessInitialWave = aBoard->mCurrentWave;
                        mEndlessInitialStage = aBoard->mChallenge->mSurvivalStage;
			NetTrace("SMOKE_ENDLESS_ST_MODE role=%s mode=%d endless=1 timed=1 repick=1 wavesPerStage=10 stage=%d wave=%d phase=%u authority=%s",
                                theManifest.IsHost() ? "host" : "client",
                                (int)theApp->mGameMode, mEndlessInitialStage,
                                mEndlessInitialWave, (unsigned)mEndlessInitialPhase,
				theManifest.IsHost() ? "host" : "snapshot");
		}
		if (mScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW && !mOverflowModeVerified)
		{
			if (theApp->mGameMode != GameMode::GAMEMODE_COOP_ENDLESS_ST ||
				!theApp->IsCoopEndlessMode(theApp->mGameMode) ||
				!theApp->IsCoopEndlessRepick(theApp->mGameMode) ||
				!aBoard->IsEndlessT() || !aBoard->mChallenge ||
				aBoard->GetNumWavesPerSurvivalStage() != 10)
			{
				theFailure = "EndlessOverflow did not start on the deterministic EndlessST board";
				return false;
			}
			mOverflowModeVerified = true;
			NetTrace("SMOKE_ENDLESS_OVERFLOW_MODE role=%s mode=%d endless=1 timed=1 repick=1 wavesPerStage=10 stage=%d authority=%s",
				theManifest.IsHost() ? "host" : "client", (int)theApp->mGameMode,
				aBoard->mChallenge->mSurvivalStage,
				theManifest.IsHost() ? "host" : "snapshot");
		}
                if (mScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT &&
			!mTdPathVerified)
		{
			if (!ValidateTowerDefenseSmokePath(aBoard, theManifest, theFailure))
				return false;
			mTdPathVerified = true;
		}
                if (NetIsInteractiveChooserSmokeScenario(mScenario) &&
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
		if (mScenario == NETSMOKE_SCENARIO_PVP_1V1)
		{
			Zombie* aZombie = nullptr;
			while (aBoard->mZombies.IterateNext(aZombie))
				mPvpBaselineMaxZombieId = max(mPvpBaselineMaxZombieId, aZombie->mNetId);
		}
		if (mScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW)
		{
			Zombie* aZombie = nullptr;
			while (aBoard->mZombies.IterateNext(aZombie))
				mOverflowBaselineMaxZombieId = max(mOverflowBaselineMaxZombieId, aZombie->mNetId);
		}
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

	if (mScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW)
	{
		const uint32_t aNow = GetTickCount();
		if (!aBoard->mChallenge || !mOverflowModeVerified ||
			theApp->mGameMode != GameMode::GAMEMODE_COOP_ENDLESS_ST)
		{
			theFailure = "EndlessOverflow authoritative mode or challenge state disappeared";
			return false;
		}

		if (theManifest.IsHost() && !mOverflowStage255Set)
		{
			// This is the only direct boundary setup.  The actual 255 -> 256
			// transition below remains owned by the stock survival lifecycle.
			aBoard->mChallenge->mSurvivalStage = 255;
			mOverflowStage255Set = true;
			mOverflowStageSetTick = aNow;
			NetTrace("SMOKE_ENDLESS_OVERFLOW_STAGE_SET role=host stage=255 source=test_boundary_setup");
		}

		const int anObservedStage = aBoard->mChallenge->mSurvivalStage;
		if (!mOverflowStage255Observed && anObservedStage == 255)
		{
			mOverflowStage255Observed = true;
			if (mOverflowStageSetTick == 0)
				mOverflowStageSetTick = aNow;
			NetTrace("SMOKE_ENDLESS_OVERFLOW_STAGE role=%s phase=pre stage=255 authority=%s noWrap=1",
				theManifest.IsHost() ? "host" : "client",
				theManifest.IsHost() ? "host" : "snapshot");
		}
		else if (mOverflowStage255Observed && !mOverflowRepickOpened &&
			anObservedStage != 255 && anObservedStage != 256)
		{
			NetTrace("SMOKE_ENDLESS_OVERFLOW_STAGE_INVALID role=%s phase=pre expected=255 actual=%d wrap=%d",
				theManifest.IsHost() ? "host" : "client", anObservedStage,
				anObservedStage == 0 ? 1 : 0);
			theFailure = "EndlessOverflow stage 255 wrapped or changed before the stock transition";
			return false;
		}

		if (theManifest.IsHost() && mOverflowStage255Observed && !mOverflowGargSpawned &&
			(uint32_t)(aNow - mOverflowStageSetTick) >= 1500U)
		{
			Zombie* aGarg = aBoard->AddZombieInRow(ZombieType::ZOMBIE_REDEYE_GARGANTUAR, 2, 0);
			if (!aGarg || aGarg->mNetId == 0)
			{
				theFailure = "EndlessOverflow stock Gargantuar creation lacked a network identity";
				return false;
			}
			mOverflowGargSpawned = true;
			mOverflowGargId = aGarg->mNetId;
			mOverflowGargTick = aNow;
			NetTrace("SMOKE_ENDLESS_OVERFLOW_GARG_SPAWN role=host id=%u row=%d stockAddZombie=1",
				aGarg->mNetId, aGarg->mRow);
		}

		if (!mOverflowGargObserved && mOverflowStage255Observed)
		{
			Zombie* aZombie = nullptr;
			Zombie* aCandidate = nullptr;
			while (aBoard->mZombies.IterateNext(aZombie))
			{
				if (aZombie->mDead || aZombie->mNetId <= mOverflowBaselineMaxZombieId ||
				aZombie->mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
					(theManifest.IsClient() && !aZombie->mNetGhost))
					continue;
				if (!aCandidate || aZombie->mNetId < aCandidate->mNetId)
					aCandidate = aZombie;
			}
			if (aCandidate)
			{
				if (aCandidate->mBodyHealth <= 65535 || aCandidate->mBodyMaxHealth <= 65535 ||
					aCandidate->mBodyHealth > aCandidate->mBodyMaxHealth)
				{
					NetTrace("SMOKE_ENDLESS_OVERFLOW_HEALTH_INVALID role=%s id=%u stage=%d body=%d max=%d clipped=%d",
						theManifest.IsHost() ? "host" : "client", aCandidate->mNetId,
						anObservedStage, aCandidate->mBodyHealth, aCandidate->mBodyMaxHealth,
						aCandidate->mBodyMaxHealth <= 65535 ? 1 : 0);
					theFailure = "EndlessOverflow Gargantuar health was clipped at the 16-bit boundary";
					return false;
				}
				mOverflowGargObserved = true;
				mOverflowGargId = aCandidate->mNetId;
				mOverflowGargBodyHealth = aCandidate->mBodyHealth;
				mOverflowGargMaxHealth = aCandidate->mBodyMaxHealth;
				mOverflowGargTick = aNow;
				NetTrace("SMOKE_ENDLESS_OVERFLOW_HEALTH role=%s id=%u type=%d stage=255 body=%d max=%d ghost=%d over16=1",
					theManifest.IsHost() ? "host" : "client", aCandidate->mNetId,
					(int)aCandidate->mZombieType, aCandidate->mBodyHealth,
					aCandidate->mBodyMaxHealth, aCandidate->mNetGhost ? 1 : 0);
			}
		}

		if (theManifest.IsHost() && mOverflowGargObserved && !mOverflowTransitionTriggered &&
			(uint32_t)(aNow - mOverflowGargTick) >= 1500U)
		{
			mOverflowTransitionTriggered = true;
			mOverflowTransitionTick = aNow;
			mActionIssued = true;
			mActionTick = aNow;
			aBoard->mLevelComplete = true;
			NetTrace("SMOKE_CHECKPOINT role=host scenario=endless_overflow step=ACTION_ISSUED owner=host");
			NetTrace("SMOKE_ENDLESS_OVERFLOW_STAGE_TRIGGER role=host from=255 wave=%d stockCheckForGameEnd=1",
				aBoard->mCurrentWave);
			return true;
		}

		if (mOverflowRepickOpened && theApp->mGameScene == GameScenes::SCENE_PLAYING &&
			mSoakChooserVerified)
		{
			if (anObservedStage != 256)
			{
				NetTrace("SMOKE_ENDLESS_OVERFLOW_STAGE_INVALID role=%s phase=post expected=256 actual=%d wrap=%d",
					theManifest.IsHost() ? "host" : "client", anObservedStage,
					anObservedStage == 0 ? 1 : 0);
				theFailure = "EndlessOverflow stock repick did not preserve exact stage 256";
				return false;
			}
			if (!mOverflowStage256Observed)
			{
				mOverflowStage256Observed = true;
				NetTrace("SMOKE_ENDLESS_OVERFLOW_STAGE role=%s phase=post stage=256 authority=%s noWrap=1",
					theManifest.IsHost() ? "host" : "client",
					theManifest.IsHost() ? "host" : "snapshot");
			}
			if (!mOverflowRepickCompleted)
			{
				mOverflowRepickCompleted = true;
				NetTrace("SMOKE_ENDLESS_OVERFLOW_REPICK_COMPLETE role=%s from=255 to=256 chooser=1 wavesPerStage=10",
					theManifest.IsHost() ? "host" : "client");
			}
			if (!mActionApplied)
			{
				mActionApplied = true;
				NetTrace("SMOKE_CHECKPOINT role=%s scenario=endless_overflow step=ACTION_APPLIED stage=256 chooser=1",
					theManifest.IsHost() ? "host" : "client");
			}
		}

		if (!mOverflowStage255Observed && mPlayingTick != 0 &&
			(uint32_t)(aNow - mPlayingTick) >= 15000U)
		{
			theFailure = "EndlessOverflow client did not receive exact stage 255 within 15 seconds";
			return false;
		}
		if (mOverflowStage255Observed && !mOverflowGargObserved &&
			(uint32_t)(aNow - mOverflowStageSetTick) >= 15000U)
		{
			theFailure = "EndlessOverflow did not observe the same oversized Gargantuar within 15 seconds";
			return false;
		}
		if (mOverflowTransitionTriggered && !mOverflowRepickCompleted &&
			(uint32_t)(aNow - mOverflowTransitionTick) >= 60000U)
		{
			theFailure = "EndlessOverflow stock stage 256 repick did not complete within 60 seconds";
			return false;
		}
		return true;
	}

	if (mScenario == NETSMOKE_SCENARIO_PVP_1V1)
	{
		const uint32_t aNow = GetTickCount();
		NetManager* aNet = theApp->mNetManager;
		if (!aNet || !aBoard->IsPvPMode() || !aBoard->mSeedBank || !aBoard->mChallenge)
		{
			theFailure = "PvP smoke lost its network, board, seed bank, or challenge state";
			return false;
		}

		if (!mPvpRosterVerified)
		{
			const NetPlayerInfo& aHost = aNet->GetPlayer(0);
			const NetPlayerInfo& aClient = aNet->GetPlayer(1);
			if (aNet->GetPlayerCount() != 2 || !aHost.mActive || !aClient.mActive ||
				aHost.mTeam != NETTEAM_PLANTS || aClient.mTeam != NETTEAM_ZOMBIES ||
				aNet->GetMySlot() != (theManifest.IsHost() ? 0 : 1))
			{
				theFailure = "PvP smoke did not start with the canonical plants-host/zombies-client 1v1 roster";
				return false;
			}
			mPvpRosterVerified = true;
			NetTrace("SMOKE_PVP_ROSTER role=%s players=2 hostTeam=plants clientTeam=zombies localSlot=%d localTeam=%s",
				theManifest.IsHost() ? "host" : "client", aNet->GetMySlot(),
				aNet->GetMyTeam() == NETTEAM_PLANTS ? "plants" : "zombies");
		}

		if (!mPvpDeckVerified)
		{
			if (aBoard->mSeedBank->mNumPackets != PVP_PACKET_COUNT)
			{
				theFailure = "PvP smoke seed bank did not contain exactly 10 plant and 16 zombie cards";
				return false;
			}
			std::ostringstream aTypes;
			std::ostringstream anOwners;
			for (int i = 0; i < PVP_PACKET_COUNT; ++i)
			{
				SeedPacket& aPacket = aBoard->mSeedBank->mSeedPackets[i];
				const bool aZombieCard = aBoard->mChallenge->IsZombieSeedType(aPacket.mPacketType);
				const bool aValidPlant = i < PVP_PLANT_PACKET_COUNT && !aZombieCard &&
					aPacket.mPacketType != SeedType::SEED_NONE && aPacket.mNetOwnerSlot == 0;
				const int aZombieIndex = i - PVP_PLANT_PACKET_COUNT;
				const bool aValidZombie = i >= PVP_PLANT_PACKET_COUNT && aZombieCard &&
					aPacket.mPacketType == Challenge::GetPvPZombieSeed(aZombieIndex) &&
					aPacket.mNetOwnerSlot == NET_SHARED_OWNER;
				if ((!aValidPlant && i < PVP_PLANT_PACKET_COUNT) ||
					(!aValidZombie && i >= PVP_PLANT_PACKET_COUNT) || aPacket.mNetId == 0)
				{
					NetTrace("SMOKE_PVP_DECK_INVALID role=%s index=%d id=%u type=%d owner=%u zombie=%d",
						theManifest.IsHost() ? "host" : "client", i, aPacket.mNetId,
						(int)aPacket.mPacketType, aPacket.mNetOwnerSlot, aZombieCard ? 1 : 0);
					theFailure = "PvP smoke found an invalid card identity, type, order, or owner";
					return false;
				}
				if (i != 0) { aTypes << ','; anOwners << ','; }
				aTypes << (int)aPacket.mPacketType;
				anOwners << (int)aPacket.mNetOwnerSlot;
			}
			mPvpDeckVerified = true;
			NetTrace("SMOKE_PVP_DECK role=%s plants=10 zombies=16 shared=16 total=26 types=%s owners=%s",
				theManifest.IsHost() ? "host" : "client",
				aTypes.str().c_str(), anOwners.str().c_str());
		}

		if (!mPvpInitialBrainsVerified)
		{
			if (aBoard->mNetPvPBrains != 100 ||
				(theManifest.IsHost() && (aBoard->mNetPvPBrainTick < 0 || aBoard->mNetPvPBrainTick >= 1000)))
			{
				NetTrace("SMOKE_PVP_BRAINS_INVALID role=%s phase=initial brains=%d tick=%d",
					theManifest.IsHost() ? "host" : "client", aBoard->mNetPvPBrains,
					theManifest.IsHost() ? aBoard->mNetPvPBrainTick : -1);
				theFailure = "PvP initial brains or host trickle cadence state was invalid";
				return false;
			}
			mPvpInitialBrainsVerified = true;
			NetTrace("SMOKE_PVP_BRAINS role=%s phase=initial brains=100 tick=%d",
				theManifest.IsHost() ? "host" : "client",
				theManifest.IsHost() ? aBoard->mNetPvPBrainTick : -1);
			if (theManifest.IsHost()) mPvpInitialReadyTick = aNow;
		}
		if (theManifest.IsHost() && mPvpInitialReadyTick && aNow - mPvpInitialReadyTick >= 800U)
		{
			aNet->SendChat("smoke-pvp-initial-ready");
			mPvpInitialReadyTick = 0;
		}

		if (!mPvpCursorMotionIssued)
		{
			SDL_Event aMotion{}; aMotion.type = SDL_EVENT_MOUSE_MOTION;
			aMotion.motion.windowID = SDL_GetWindowID(theApp->mSDLWindow);
			aMotion.motion.x = (float)(aBoard->GridToPixelX(4, 1) + 40);
			aMotion.motion.y = (float)(aBoard->GridToPixelY(4, 1) + 45);
			if (!SDL_PushEvent(&aMotion)) { theFailure = "PvP SDL cursor motion was rejected"; return false; }
			mPvpCursorMotionIssued = true;
			NetTrace("SMOKE_PVP_CURSOR_INPUT role=%s source=SDL row=1 col=4", theManifest.IsHost() ? "host" : "client");
		}
		// Snapshot baselines are reached independently. Let both peers inspect
		// the initial balance before the client's real purchase changes it.
		bool aHostInitialReady = theManifest.IsHost();
		for (const auto& aLine : aNet->mChatLines)
			if (aLine.mSlot == 0 && aLine.mText == "smoke-pvp-initial-ready") aHostInitialReady = true;
		if (theManifest.IsClient() && !mActionIssued && mPvpRosterVerified &&
			mPvpDeckVerified && mPvpInitialBrainsVerified && aHostInitialReady)
		{
			SeedPacket& aPacket = aBoard->mSeedBank->mSeedPackets[PVP_PLANT_PACKET_COUNT];
			if (aPacket.CanPickUp())
			{
				const int aPacketX = aBoard->mSeedBank->mX + aPacket.mX +
					aPacket.mOffsetX + aPacket.mWidth / 2;
				const int aPacketY = aBoard->mSeedBank->mY + aPacket.mY + aPacket.mHeight / 2;
				const int aSpawnX = BOARD_WIDTH - 80;
				const int aSpawnY = aBoard->GridToPixelY(8, 2) + 45;
				SmokeClientClick(theApp, aPacketX, aPacketY);
				SmokeClientClick(theApp, aSpawnX, aSpawnY);
				mActionIssued = true;
				mActionTick = aNow;
				NetTrace("SMOKE_PVP_CLIENT_ZOMBIE_INPUT index=%d type=%d owner=%u brains=%d row=2",
					PVP_PLANT_PACKET_COUNT, (int)aPacket.mPacketType,
					aPacket.mNetOwnerSlot, aBoard->mNetPvPBrains);
				NetTrace("SMOKE_CHECKPOINT role=client scenario=pvp_1v1 step=ACTION_ISSUED owner=client");
			}
		}

		Zombie* aSpawned = mPvpZombieId ? FindSmokeZombieById(aBoard, mPvpZombieId) : nullptr;
		if (!mPvpZombieEntryObserved)
		{
			Zombie* aZombie = nullptr;
			while (aBoard->mZombies.IterateNext(aZombie))
			{
				if (aZombie->mDead || aZombie->mNetId <= mPvpBaselineMaxZombieId ||
					(theManifest.IsClient() && !aZombie->mNetGhost))
					continue;
				if (!aSpawned || aZombie->mNetId < aSpawned->mNetId)
					aSpawned = aZombie;
			}
			if (aSpawned)
			{
				if (aSpawned->mPosX <= BOARD_WIDTH)
				{
					NetTrace("SMOKE_PVP_ZOMBIE_ENTRY_INVALID role=%s id=%u x=%.2f row=%d ghost=%d",
						theManifest.IsHost() ? "host" : "client", aSpawned->mNetId,
						aSpawned->mPosX, aSpawned->mRow, aSpawned->mNetGhost ? 1 : 0);
					theFailure = "PvP zombie first appeared on the lawn instead of offscreen right";
					return false;
				}
				mPvpZombieId = aSpawned->mNetId;
				mPvpZombieEntryObserved = true;
				mActionApplied = true;
				mPvpBrainsAfterAction = aBoard->mNetPvPBrains;
				NetTrace("SMOKE_PVP_ZOMBIE_ENTRY role=%s id=%u x=%.2f row=%d ghost=%d brains=%d",
					theManifest.IsHost() ? "host" : "client", aSpawned->mNetId,
					aSpawned->mPosX, aSpawned->mRow, aSpawned->mNetGhost ? 1 : 0,
					aBoard->mNetPvPBrains);
				NetTrace("SMOKE_CHECKPOINT role=%s scenario=pvp_1v1 step=ACTION_APPLIED zombies=%d baselineId=%u",
					theManifest.IsHost() ? "host" : "client", aBoard->mZombies.mSize,
					mPvpBaselineMaxZombieId);
			}
		}

		if (theManifest.IsHost() && !mPvpHostPlantIssued &&
			(uint32_t)(aNow - mPlayingTick) >= 1500U && IssueSoakSeedAction(theApp, false))
		{
			mPvpHostPlantIssued = true;
			NetTrace("SMOKE_PVP_HOST_PLANT_INPUT baseline=%d", mBaselinePlants);
		}
		if (!mPvpHostPlantObserved && aBoard->mPlants.mSize > mBaselinePlants)
		{
			mPvpHostPlantObserved = true;
			NetTrace("SMOKE_PVP_HOST_PLANT role=%s plants=%d baseline=%d",
				theManifest.IsHost() ? "host" : "client", aBoard->mPlants.mSize, mBaselinePlants);
		}

		if (mPvpZombieEntryObserved && !mPvpPassiveBrainsVerified &&
			mPvpBrainsAfterAction >= 0 && aBoard->mNetPvPBrains != mPvpBrainsAfterAction)
		{
			const int aDelta = aBoard->mNetPvPBrains - mPvpBrainsAfterAction;
			if (aDelta == 50)
			{
				mPvpPassiveBrainsVerified = true;
				NetTrace("SMOKE_PVP_BRAINS role=%s phase=passive before=%d after=%d delta=%d",
					theManifest.IsHost() ? "host" : "client", mPvpBrainsAfterAction,
					aBoard->mNetPvPBrains, aDelta);
			}
			else if (aDelta > 50)
			{
				NetTrace("SMOKE_PVP_BRAINS_INVALID role=%s phase=passive before=%d after=%d",
					theManifest.IsHost() ? "host" : "client", mPvpBrainsAfterAction,
					aBoard->mNetPvPBrains);
				theFailure = "PvP passive brain award skipped the exact +50 authoritative state";
				return false;
			}
			else
			{
				// A client can receive the zombie cost (-25) and the first +50
				// award in one authoritative snapshot.  Preserve that combined
				// transition as the new baseline; the next positive transition
				// must still prove the exact +50 cadence on both peers.
				NetTrace("SMOKE_PVP_BRAINS_REBASE role=%s before=%d after=%d delta=%d",
					theManifest.IsHost() ? "host" : "client", mPvpBrainsAfterAction,
					aBoard->mNetPvPBrains, aDelta);
				mPvpBrainsAfterAction = aBoard->mNetPvPBrains;
			}
		}

		if (mActionIssued && !mPvpZombieEntryObserved && aNow - mActionTick >= 15000U)
		{
			theFailure = "PvP client zombie input did not create an authoritative zombie within 15 seconds";
			return false;
		}
		return true;
	}

        if (mScenario == NETSMOKE_SCENARIO_ENDLESS_ST)
        {
                const uint32_t aNow = GetTickCount();
                if (!aBoard->mChallenge || !mEndlessModeVerified ||
                        theApp->mGameMode != GameMode::GAMEMODE_COOP_ENDLESS_ST)
                {
                        theFailure = "EndlessST authoritative mode or challenge state disappeared during gameplay";
                        return false;
                }

                if (theManifest.IsHost() && !mEndlessWeatherAccelerated &&
                        aBoard->mChallenge->mSurvivalStage == mEndlessInitialStage)
                {
                        // Test-only timer compression: stock UpdateEndlessWeather and
                        // UpdateZombieSpawning still own both transitions.
                        aBoard->mNetWeatherTimer = 2;
                        aBoard->mZombieCountDown = 6;
                        aBoard->mZombieCountDownStart = 6;
                        mEndlessWeatherAccelerated = true;
                        NetTrace("SMOKE_ENDLESS_ST_ACCELERATE role=host weatherTimer=2 zombieCountdown=6");
                }

                if (!mEndlessTimeChanged &&
                        aBoard->mNetDayPhase != mEndlessInitialPhase)
                {
                        mEndlessTimeChanged = true;
                        NetTrace("SMOKE_ENDLESS_ST_TIME role=%s from=%u to=%u weatherTimer=%u fog=%u storm=%u visible=1",
                                theManifest.IsHost() ? "host" : "client",
                                (unsigned)mEndlessInitialPhase,
                                (unsigned)aBoard->mNetDayPhase,
                                (unsigned)aBoard->mNetWeatherTimer,
                                (unsigned)aBoard->mNetFogTimer,
                                (unsigned)aBoard->mNetStormTimer);
                }

                if (!mEndlessRepickOpened &&
                        aBoard->mCurrentWave > mEndlessInitialWave)
                {
			if (!mEndlessWaveProgressed)
			{
				mEndlessWaveProgressed = true;
				mEndlessWaveTick = aNow;
                                NetTrace("SMOKE_ENDLESS_ST_WAVE role=%s from=%d to=%d stage=%d stockSpawner=1",
                                        theManifest.IsHost() ? "host" : "client",
                                        mEndlessInitialWave, aBoard->mCurrentWave,
                                        aBoard->mChallenge->mSurvivalStage);
                        }
                        Zombie* aZombie = nullptr;
                        while (aBoard->mZombies.IterateNext(aZombie))
                        {
                                const int aType = (int)aZombie->mZombieType;
                                if (aZombie->mDead || aZombie->mFromWave < 0 ||
                                        aType < 0 || aType >= NUM_ZOMBIE_TYPES ||
                                        aZombie->mBodyMaxHealth <= 0)
                                {
                                        continue;
                                }
				if (mEndlessBaseHealth[aType] == 0)
				{
                                        mEndlessBaseHealth[aType] = aZombie->mBodyMaxHealth;
                                        NetTrace("SMOKE_ENDLESS_ST_HEALTH_BASE role=%s type=%d stage=%d max=%d",
                                                theManifest.IsHost() ? "host" : "client", aType,
                                                aBoard->mChallenge->mSurvivalStage,
						aZombie->mBodyMaxHealth);
				}
				mEndlessPreRepickMaxZombieId = max(
					mEndlessPreRepickMaxZombieId, aZombie->mNetId);
                        }
                }

                bool aHasBaseHealth = false;
                for (int i = 0; i < NUM_ZOMBIE_TYPES; ++i)
                        aHasBaseHealth |= mEndlessBaseHealth[i] > 0;
		if (theManifest.IsHost() && !mEndlessStageTriggered &&
			mEndlessTimeChanged && mEndlessWaveProgressed && aHasBaseHealth &&
			mEndlessWaveTick != 0 &&
			(uint32_t)(aNow - mEndlessWaveTick) >= 1000U)
                {
                        // Complete the current ten-wave stage through the same stock
                        // CheckForGameEnd -> InitSurvivalStage path used in play.  No
                        // zombie is repositioned or advanced by the smoke harness.
                        mEndlessStageTriggered = true;
                        mEndlessStageTick = aNow;
                        mActionIssued = true;
                        mActionTick = aNow;
                        aBoard->mLevelComplete = true;
                        NetTrace("SMOKE_CHECKPOINT role=host scenario=endless_st step=ACTION_ISSUED owner=host");
                        NetTrace("SMOKE_ENDLESS_ST_STAGE_TRIGGER role=host stage=%d wave=%d wavesPerStage=10 stockCheckForGameEnd=1",
                                aBoard->mChallenge->mSurvivalStage, aBoard->mCurrentWave);
                        return true;
                }

                if (mEndlessRepickOpened && theApp->mGameScene == GameScenes::SCENE_PLAYING &&
                        aBoard->mChallenge->mSurvivalStage > mEndlessInitialStage &&
                        mSoakChooserVerified)
                {
                        if (!mEndlessRepickCompleted)
                        {
                                mEndlessRepickCompleted = true;
                                NetTrace("SMOKE_ENDLESS_ST_REPICK_COMPLETE role=%s fromStage=%d toStage=%d chooser=1 wavesPerStage=10",
                                        theManifest.IsHost() ? "host" : "client",
                                        mEndlessInitialStage,
                                        aBoard->mChallenge->mSurvivalStage);
                        }
			if (theManifest.IsHost() && !mEndlessHealthScaled &&
				aBoard->mZombieCountDown > 6)
                        {
                                aBoard->mZombieCountDown = 6;
				aBoard->mZombieCountDownStart = 6;
			}

			// A synchronized snapshot can continue arriving even when the client
			// presentation is still frozen.  Require a real non-attachment
			// reanimation to advance between two local samples, and require game
			// music to be unpaused, before this smoke can pass.
			if (!mEndlessThawed)
			{
				bool aStoredReanimFound = false;
				Reanimation* aReanim = nullptr;
				while (aBoard->IterateReanimations(aReanim))
				{
					if (aReanim->mDead || aReanim->mIsAttachment ||
						aReanim->mAnimRate <= 0.0f)
					{
						continue;
					}
					if (mEndlessThawReanim == 0)
					{
						mEndlessThawReanim = (uintptr_t)aReanim;
						mEndlessThawAnimTime = aReanim->mAnimTime;
						mEndlessThawSampleTick = aNow;
						break;
					}
					if ((uintptr_t)aReanim != mEndlessThawReanim)
						continue;
					aStoredReanimFound = true;
					if ((uint32_t)(aNow - mEndlessThawSampleTick) >= 100U &&
						fabsf(aReanim->mAnimTime - mEndlessThawAnimTime) > 0.0001f &&
						theApp->mMusic && !theApp->mMusic->mPaused)
					{
						mEndlessThawed = true;
						NetTrace("SMOKE_ENDLESS_ST_THAW role=%s thawed=1 reanim=%p rate=%.2f from=%.4f to=%.4f musicPaused=0",
							theManifest.IsHost() ? "host" : "client", (void*)aReanim,
							aReanim->mAnimRate, mEndlessThawAnimTime,
							aReanim->mAnimTime);
					}
					break;
				}
				if (mEndlessThawReanim != 0 && !aStoredReanimFound &&
					(uint32_t)(aNow - mEndlessThawSampleTick) >= 100U)
				{
					mEndlessThawReanim = 0;
					mEndlessThawAnimTime = 0.0f;
					mEndlessThawSampleTick = 0;
				}
			}

			Zombie* aZombie = nullptr;
                        while (!mEndlessHealthScaled && aBoard->mZombies.IterateNext(aZombie))
                        {
                                const int aType = (int)aZombie->mZombieType;
				if (aZombie->mDead || aZombie->mFromWave < 0 ||
					aZombie->mNetId <= mEndlessPreRepickMaxZombieId ||
					aType < 0 || aType >= NUM_ZOMBIE_TYPES ||
                                        mEndlessBaseHealth[aType] <= 0)
                                {
                                        continue;
                                }
                                const float aScale = 1.0f +
                                        aBoard->mChallenge->mSurvivalStage * 0.12f;
                                const int anExpected =
                                        (int)(mEndlessBaseHealth[aType] * aScale);
                                if (aZombie->mBodyMaxHealth != anExpected ||
                                        aZombie->mBodyMaxHealth <= mEndlessBaseHealth[aType])
                                {
                                        theFailure = "EndlessST spawned a post-repick zombie without authoritative health scaling";
                                        return false;
                                }
				mEndlessHealthScaled = true;
				NetTrace("SMOKE_ENDLESS_ST_HEALTH_SCALE role=%s type=%d stage=%d base=%d scaled=%d expected=%d factor=%.2f",
                                        theManifest.IsHost() ? "host" : "client", aType,
                                        aBoard->mChallenge->mSurvivalStage,
                                        mEndlessBaseHealth[aType], aZombie->mBodyMaxHealth,
                                        anExpected, aScale);
			}
			if (mEndlessHealthScaled && mEndlessThawed && !mActionApplied)
			{
				mActionApplied = true;
				NetTrace("SMOKE_CHECKPOINT role=%s scenario=endless_st step=ACTION_APPLIED stage=%d wave=%d thawed=1",
					theManifest.IsHost() ? "host" : "client",
					aBoard->mChallenge->mSurvivalStage,
					aBoard->mCurrentWave);
			}
                }

                if (mEndlessRepickOpened && !mActionApplied && mEndlessStageTick != 0 &&
                        (uint32_t)(aNow - mEndlessStageTick) >= 60000U)
                {
                        theFailure = "EndlessST repick did not return to scaled stock gameplay within 60 seconds";
                        return false;
                }
                return true;
        }

        if (mScenario == NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT)
	{
		const uint32_t aNow = GetTickCount();
		if (!mPortalCameraVerified)
		{
			const int aTargetX = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mBoardX : aBoard->mX;
			const int aTargetY = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mBoardY : aBoard->mY;
			const bool aValid = theManifest.IsHost() ||
				aBoard->mNetGameplayCameraTargets.mValid;
			const int aDeltaX = abs(aBoard->mX - aTargetX);
			const int aDeltaY = abs(aBoard->mY - aTargetY);
			const int aTargetTree = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mTreeX : aBoard->mTreeX;
			const int aTargetPole = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mPoleX : aBoard->mPoleX;
			if (!aValid || aDeltaX > 4 || aDeltaY > 4 ||
				abs(aBoard->mTreeX - aTargetTree) > 4 || abs(aBoard->mPoleX - aTargetPole) > 4)
			{
				NetTrace("SMOKE_CAMERA_SYNC_INVALID role=%s phase=start board=(%d,%d) target=(%d,%d) tree=%d targetTree=%d pole=%d targetPole=%d delta=(%d,%d) valid=%d",
					theManifest.IsHost() ? "host" : "client", aBoard->mX, aBoard->mY,
					aTargetX, aTargetY, aBoard->mTreeX, aTargetTree, aBoard->mPoleX,
					aTargetPole, aDeltaX, aDeltaY, aValid ? 1 : 0);
				return true;
			}
			mPortalCameraVerified = true;
			mPortalInitialCameraX = aTargetX;
			mPortalInitialCameraY = aTargetY;
			NetTrace("SMOKE_CAMERA_SYNC role=%s phase=start board=(%d,%d) target=(%d,%d) tree=%d targetTree=%d pole=%d targetPole=%d delta=(%d,%d) valid=1",
				theManifest.IsHost() ? "host" : "client", aBoard->mX, aBoard->mY,
				aTargetX, aTargetY, aBoard->mTreeX, aTargetTree, aBoard->mPoleX,
				aTargetPole, aDeltaX, aDeltaY);
		}
		else
		{
			const int aTargetX = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mBoardX : aBoard->mX;
			const int aTargetY = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mBoardY : aBoard->mY;
			const int aTargetTree = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mTreeX : aBoard->mTreeX;
			const int aTargetPole = theManifest.IsClient() ?
				aBoard->mNetGameplayCameraTargets.mPoleX : aBoard->mPoleX;
			const bool aTargetValid = theManifest.IsHost() ||
				aBoard->mNetGameplayCameraTargets.mValid;
			if (aTargetX != mPortalInitialCameraX || aTargetY != mPortalInitialCameraY)
			{
				mPortalInitialCameraX = aTargetX;
				mPortalInitialCameraY = aTargetY;
				mPortalCameraSteadySamples = 0;
				mPortalCameraMaxDelta = 0;
				mPortalCameraMovementVerified = false;
			}
			const int aDeltaX = abs(aBoard->mX - aTargetX);
			const int aDeltaY = abs(aBoard->mY - aTargetY);
			const int aTreeDelta = abs(aBoard->mTreeX - aTargetTree);
			const int aPoleDelta = abs(aBoard->mPoleX - aTargetPole);
			const int aMaxDelta = max(max(aDeltaX, aDeltaY), max(aTreeDelta, aPoleDelta));
			if (!aTargetValid || (theManifest.IsClient() && aBoard->mNetIntroTargets.mValid) ||
				aMaxDelta > 4)
			{
				NetTrace("SMOKE_CAMERA_SYNC_INVALID role=%s phase=steady board=(%d,%d) target=(%d,%d) treeDelta=%d poleDelta=%d maxDelta=%d introTargetValid=%d targetValid=%d samples=%d",
					theManifest.IsHost() ? "host" : "client", aBoard->mX, aBoard->mY,
					aTargetX, aTargetY, aTreeDelta, aPoleDelta, aMaxDelta,
					aBoard->mNetIntroTargets.mValid ? 1 : 0, aTargetValid ? 1 : 0,
					mPortalCameraSteadySamples);
				theFailure = "Portal camera drifted from the authoritative gameplay target";
				return false;
			}
			if (!mPortalCameraMovementVerified)
			{
				mPortalCameraSteadySamples++;
				mPortalCameraMaxDelta = max(mPortalCameraMaxDelta, aMaxDelta);
				if (mPortalCameraSteadySamples >= 20)
				{
					mPortalCameraMovementVerified = true;
					NetTrace("SMOKE_CAMERA_SYNC role=%s phase=steady samples=%d maxDelta=%d introTargetValid=%d targetValid=1",
						theManifest.IsHost() ? "host" : "client", mPortalCameraSteadySamples,
						mPortalCameraMaxDelta, aBoard->mNetIntroTargets.mValid ? 1 : 0);
				}
			}
		}
		if (!mPortalLayoutVerified)
		{
			if (!ValidatePortalCombatSmokeLayout(aBoard, theManifest, theFailure,
				&mPortalInitialLayoutHash))
				return false;
			mPortalLayoutVerified = true;
		}

		if (!mPortalConveyorVerified)
		{
			SeedBank* aBank = aBoard->mSeedBank;
			if (!aBank || !aBoard->HasConveyorBeltSeedBank())
			{
				theFailure = "Portal Combat is missing its stock conveyor seed bank";
				return false;
			}
			if (mPortalConveyorInitialCounter < 0)
			{
				mPortalConveyorInitialCounter = aBank->mConveyorBeltCounter;
				return true;
			}
			int aLivePackets = 0;
			std::ostringstream anIds;
			std::ostringstream aTypes;
			std::ostringstream aPositions;
			for (int i = 0; i < aBank->mNumPackets; ++i)
			{
				const SeedPacket& aPacket = aBank->mSeedPackets[i];
				if (aPacket.mPacketType == SeedType::SEED_NONE)
					continue;
				if (aPacket.mNetId == 0)
				{
					// Stock AddSeed and snapshot encoding happen in adjacent host ticks.
					// Wait for the authoritative encoder to allocate the id; only the
					// correlated marker below is accepted by the runner.
					return true;
				}
				if (aLivePackets != 0)
				{
					anIds << ',';
					aTypes << ',';
					aPositions << ',';
				}
				anIds << aPacket.mNetId;
				aTypes << (int)aPacket.mPacketType;
				aPositions << (aPacket.mX + aPacket.mOffsetX) << ':' << aPacket.mY;
				++aLivePackets;
			}
			if (aLivePackets == 0 || aBank->mConveyorBeltCounter == mPortalConveyorInitialCounter)
				return true;
			const bool aVisible = aBank->mX < BOARD_WIDTH && aBank->mY < BOARD_HEIGHT &&
				aBank->mX + aBank->mWidth > 0 && aBank->mY + aBank->mHeight > 0;
			const bool aImages = Sexy::IMAGE_CONVEYORBELT_BACKDROP && Sexy::IMAGE_CONVEYORBELT &&
				Sexy::IMAGE_CONVEYORBELT_BACKDROP->GetWidth() > 1 &&
				Sexy::IMAGE_CONVEYORBELT_BACKDROP->GetHeight() > 1 &&
				Sexy::IMAGE_CONVEYORBELT->GetWidth() > 1 && Sexy::IMAGE_CONVEYORBELT->GetHeight() > 1;
			if (!aVisible || !aImages)
			{
				NetTrace("SMOKE_PORTAL_CONVEYOR_INVALID role=%s bank=(%d,%d,%d,%d) count=%d counter=%d initial=%d images=%d",
					theManifest.IsHost() ? "host" : "client", aBank->mX, aBank->mY,
					aBank->mWidth, aBank->mHeight, aLivePackets,
					aBank->mConveyorBeltCounter, mPortalConveyorInitialCounter, aImages ? 1 : 0);
				theFailure = "Portal Combat conveyor presentation is offscreen or missing stock resources";
				return false;
			}
			mPortalConveyorVerified = true;
			NetTrace("SMOKE_PORTAL_CONVEYOR role=%s bank=(%d,%d,%d,%d) count=%d counter=%d initial=%d ids=%s types=%s positions=%s images=%dx%d,%dx%d",
				theManifest.IsHost() ? "host" : "client", aBank->mX, aBank->mY,
				aBank->mWidth, aBank->mHeight, aLivePackets, aBank->mConveyorBeltCounter,
				mPortalConveyorInitialCounter, anIds.str().c_str(), aTypes.str().c_str(),
				aPositions.str().c_str(), Sexy::IMAGE_CONVEYORBELT_BACKDROP->GetWidth(),
				Sexy::IMAGE_CONVEYORBELT_BACKDROP->GetHeight(), Sexy::IMAGE_CONVEYORBELT->GetWidth(),
				Sexy::IMAGE_CONVEYORBELT->GetHeight());
		}
		if (theManifest.IsHost() && !mPortalSpawnAccelerated)
		{
			// Exercise the stock wave scheduler and stock zombie walk.  The smoke
			// only shortens the initial wait; it never creates or moves an entity.
			aBoard->mZombieCountDown = 6;
			aBoard->mZombieCountDownStart = 6;
			mPortalSpawnAccelerated = true;
			NetTrace("SMOKE_PORTAL_WAVE_ACCELERATED role=host countdown=6");
		}

		if (!mPortalAdviceShown && aBoard->mHelpIndex == AdviceType::ADVICE_PORTAL_RELOCATING)
		{
			mPortalAdviceShown = true;
			NetTrace("SMOKE_PORTAL_ADVICE role=%s phase=show help=%d",
				theManifest.IsHost() ? "host" : "client", (int)aBoard->mHelpIndex);
		}
		if (mPortalAdviceShown && !mPortalAdviceCleared &&
			aBoard->mHelpIndex == AdviceType::ADVICE_NONE)
		{
			mPortalAdviceCleared = true;
			mPortalAdviceClearTick = aNow;
			NetTrace("SMOKE_PORTAL_ADVICE role=%s phase=clear help=%d",
				theManifest.IsHost() ? "host" : "client", (int)aBoard->mHelpIndex);
		}

		Zombie* aPortalZombie = mPortalZombieId != 0 ?
			FindSmokeZombieById(aBoard, mPortalZombieId) : nullptr;
		if (!mPortalZombieTeleportObserved)
		{
			// Observe every stock zombie.  Portal relocation can invalidate a
			// previously promising path, so selecting one spawn up front is not
			// fail-closed: it can legitimately leave the lawn without teleporting.
			// We correlate the first real row jump whose same-row portal was ahead
			// of the zombie while it walked left, without moving either object.
			Zombie* aZombie = nullptr;
			while (aBoard->mZombies.IterateNext(aZombie))
			{
				if (aZombie->mDead || aZombie->IsDeadOrDying() || aZombie->mFromWave < 0 ||
					aZombie->mNetId == 0 || aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE ||
					(theManifest.IsClient() && !aZombie->mNetGhost))
					continue;

				int anObservation = -1;
				int anEmptyObservation = -1;
				for (int i = 0; i < PORTAL_ZOMBIE_OBSERVATION_CAPACITY; ++i)
				{
					if (mPortalObservedZombieIds[i] == aZombie->mNetId)
					{
						anObservation = i;
						break;
					}
					if (anEmptyObservation < 0 && mPortalObservedZombieIds[i] == 0)
						anEmptyObservation = i;
				}
				if (anObservation < 0)
				{
					anObservation = anEmptyObservation;
					if (anObservation < 0)
						continue;
					mPortalObservedZombieIds[anObservation] = aZombie->mNetId;
					mPortalObservedZombieRows[anObservation] = aZombie->mRow;
					mPortalObservedZombieXs[anObservation] = aZombie->mPosX;
					mPortalObservedZombieTicks[anObservation] = aNow;
				}

				if (mPortalObservedZombieEligible[anObservation] &&
					aZombie->mRow != mPortalObservedZombieRows[anObservation])
				{
					mPortalZombieId = aZombie->mNetId;
					mPortalZombieEntryRow = mPortalObservedZombieRows[anObservation];
					mPortalZombieEntryTick = mPortalObservedZombieTicks[anObservation];
					mPortalZombieEntryX = mPortalObservedZombieXs[anObservation];
					mPortalZombieEntryObserved = true;
					aPortalZombie = aZombie;
					break;
				}

				bool aPortalAhead = false;
				const int aZombieCenterX = aZombie->GetZombieRect().mX +
					aZombie->GetZombieRect().mWidth / 2;
				GridItem* aPortal = nullptr;
				while (aBoard->mGridItems.IterateNext(aPortal))
				{
					if (!aPortal->mDead && aPortal->mGridItemState == GridItemState::GRIDITEM_STATE_NORMAL &&
						(aPortal->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
						 aPortal->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE) &&
						aPortal->mGridY == aZombie->mRow &&
						aPortal->mGridX * 80 + 25 < aZombieCenterX - 45)
					{
						aPortalAhead = true;
						break;
					}
				}
				if (aPortalAhead && !mPortalObservedZombieEligible[anObservation])
				{
					mPortalObservedZombieEligible[anObservation] = true;
					mPortalObservedZombieRows[anObservation] = aZombie->mRow;
				}
				// Keep the last authoritative same-row sample, rather than the spawn
				// position at which this candidate first became eligible.  A portal at
				// the left of the lawn may be reached many seconds later; the real jump
				// distance is destination X minus this immediate pre-teleport X.
				if (mPortalObservedZombieEligible[anObservation] &&
					aZombie->mRow == mPortalObservedZombieRows[anObservation])
				{
					mPortalObservedZombieXs[anObservation] = aZombie->mPosX;
					mPortalObservedZombieTicks[anObservation] = aNow;
				}
			}

			if (mPortalZombieEntryObserved && aPortalZombie)
			{
				Reanimation* aBody = theApp->ReanimationTryToGet(aPortalZombie->mBodyReanimID);
				const float aMoved = fabsf(mPortalZombieEntryX - aPortalZombie->mPosX);
				const bool aSnapped = !theManifest.IsClient() ||
					(fabsf(aPortalZombie->mPosX - aPortalZombie->mNetTargetX) <= 2.0f &&
					 fabsf(aPortalZombie->mPosY - aPortalZombie->mNetTargetY) <= 2.0f);
				const int anExpectedOrder = Board::MakeRenderOrder(
					RenderLayer::RENDER_LAYER_ZOMBIE, aPortalZombie->mRow, 4);
				const bool aRenderRow = aPortalZombie->mRenderOrder == anExpectedOrder;
				bool aDestinationOpen = false;
				GridItem* aPortal = nullptr;
				while (aBoard->mGridItems.IterateNext(aPortal))
				{
					if (!aPortal->mDead && aPortal->mGridItemState == GridItemState::GRIDITEM_STATE_NORMAL &&
						(aPortal->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
						 aPortal->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE) &&
						aPortal->mGridY == aPortalZombie->mRow)
					{
						aDestinationOpen = true;
						break;
					}
				}
				NetTrace("SMOKE_PORTAL_ZOMBIE_ENTRY role=%s id=%u ghost=%d row=%d x=%.2f animRate=%.2f",
					theManifest.IsHost() ? "host" : "client", aPortalZombie->mNetId,
					aPortalZombie->mNetGhost ? 1 : 0, mPortalZombieEntryRow,
					mPortalZombieEntryX, aBody ? aBody->mAnimRate : 0.0f);
				if (aMoved < 300.0f || !aBody || aBody->mDead || aBody->mAnimRate <= 0.0f ||
					!aSnapped || !aRenderRow || !aDestinationOpen)
				{
					NetTrace("SMOKE_PORTAL_ZOMBIE_TELEPORT_INVALID role=%s id=%u fromRow=%d toRow=%d beforeX=%.2f afterX=%.2f target=(%.2f,%.2f) moved=%.2f snap=%d render=%d destination=%d body=%p animRate=%.2f",
						theManifest.IsHost() ? "host" : "client", aPortalZombie->mNetId,
						mPortalZombieEntryRow, aPortalZombie->mRow, mPortalZombieEntryX,
						aPortalZombie->mPosX, aPortalZombie->mNetTargetX,
						aPortalZombie->mNetTargetY, aMoved, aSnapped ? 1 : 0,
						aRenderRow ? 1 : 0, aDestinationOpen ? 1 : 0,
						(void*)aBody, aBody ? aBody->mAnimRate : 0.0f);
					theFailure = "Portal Combat zombie teleport was interpolated, mis-layered, or lacked a live destination";
					return false;
				}
				mPortalZombieTeleportObserved = true;
				NetTrace("SMOKE_PORTAL_ZOMBIE_TELEPORT role=%s id=%u fromRow=%d toRow=%d beforeX=%.2f afterX=%.2f targetX=%.2f moved=%.2f snap=1 render=1 destination=1 animRate=%.2f",
					theManifest.IsHost() ? "host" : "client", aPortalZombie->mNetId,
					mPortalZombieEntryRow, aPortalZombie->mRow, mPortalZombieEntryX,
					aPortalZombie->mPosX, aPortalZombie->mNetTargetX, aMoved, aBody->mAnimRate);
			}
			else if ((uint32_t)(aNow - mPlayingTick) >= 140000U)
			{
				theFailure = "Portal Combat observed no eligible stock zombie teleport within 140 seconds";
				return false;
			}
		}

		if (mPortalZombieTeleportObserved && theManifest.IsClient() && !mActionIssued &&
			(uint32_t)(aNow - mSoakLastActionTick) >= 1000U)
		{
			if (IssuePortalClientSeedPick(theApp, aPortalZombie ? aPortalZombie->mRow : 0,
				mPortalHeldPacketId, mPortalHeldPacketIndex,
				mPortalPlantColumn, mPortalPlantRow))
			{
				mActionIssued = true;
				mActionTick = aNow;
				mSoakLastActionTick = aNow;
				NetTrace("SMOKE_CHECKPOINT role=client scenario=portal_combat_client step=ACTION_ISSUED owner=client phase=pick packetId=%u index=%d",
					mPortalHeldPacketId, mPortalHeldPacketIndex);
			}
		}
		if (theManifest.IsClient() && mActionIssued && !mPortalPlantClickIssued)
		{
			if (mPortalHeldEchoMismatch)
			{
				theFailure = "Portal Combat host-confirmed held packet identity/index did not match the picked conveyor card";
				return false;
			}
			if (!mPortalHeldPacketEchoed)
			{
				if ((uint32_t)(aNow - mActionTick) >= 10000U)
				{
					theFailure = "Portal Combat received no host-confirmed held packet echo within 10 seconds";
					return false;
				}
			}
			else
			{
				CursorObject* aCursor = aBoard->mCursorObject;
				const bool aMapped = aCursor &&
					aCursor->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK &&
					aCursor->mSeedBankIndex == mPortalHeldPacketIndex &&
					mPortalHeldPacketIndex >= 0 &&
					mPortalHeldPacketIndex < aBoard->mSeedBank->mNumPackets &&
					aBoard->mSeedBank->mSeedPackets[mPortalHeldPacketIndex].mNetId ==
						mPortalHeldPacketId;
				if (!aMapped)
				{
					theFailure = "Portal Combat held packet echo did not map the local stock cursor to the same conveyor packet";
					return false;
				}
				const int aPlantX = aBoard->GridToPixelX(mPortalPlantColumn, mPortalPlantRow) + 40;
				const int aPlantY = aBoard->GridToPixelY(mPortalPlantColumn, mPortalPlantRow) + 45;
				SmokeClientClick(theApp, aPlantX, aPlantY);
				mPortalPlantClickIssued = true;
				mSoakLastActionTick = aNow;
				NetTrace("SMOKE_PORTAL_CLIENT_PLANT_INPUT packetId=%u index=%d cell=(%d,%d) echo=1",
					mPortalHeldPacketId, mPortalHeldPacketIndex,
					mPortalPlantColumn, mPortalPlantRow);
			}
		}
		if (!mActionApplied && aBoard->mPlants.mSize > mBaselinePlants)
		{
			Plant* aNewest = nullptr;
			Plant* aPlant = nullptr;
			while (aBoard->mPlants.IterateNext(aPlant))
				if (!aPlant->mDead && aPlant->mIsOnBoard && aPlant->mNetId != 0 &&
					(!aNewest || aPlant->mNetId > aNewest->mNetId))
					aNewest = aPlant;
			if (aNewest)
			{
				Reanimation* aBody = theApp->ReanimationTryToGet(aNewest->mBodyReanimID);
				if (!aBody || aBody->mDead)
				{
					theFailure = "Portal Combat client plant has no live stock presentation";
					return false;
				}
				mPortalPlantId = aNewest->mNetId;
				mPortalClientPlantObserved = true;
				mActionApplied = true;
				NetTrace("SMOKE_PORTAL_CLIENT_PLANT role=%s id=%u ghost=%d type=%d cell=(%d,%d) body=1",
					theManifest.IsHost() ? "host" : "client", aNewest->mNetId,
					aNewest->mNetGhost ? 1 : 0, (int)aNewest->mSeedType,
					aNewest->mPlantCol, aNewest->mRow);
				NetTrace("SMOKE_CHECKPOINT role=%s scenario=portal_combat_client step=ACTION_APPLIED plants=%d baseline=%d",
					theManifest.IsHost() ? "host" : "client",
					aBoard->mPlants.mSize, mBaselinePlants);
			}
		}
		if (mActionIssued && !mActionApplied && aNow - mActionTick >= 45000U)
		{
			theFailure = "Portal Combat client conveyor plant was not applied within 45 seconds";
			return false;
		}

		if (mPortalAdviceCleared && !mPortalRelocationVerified)
		{
			int aPortalCount = 0;
			GridItem* aPortal = nullptr;
			while (aBoard->mGridItems.IterateNext(aPortal))
				if (!aPortal->mDead &&
					(aPortal->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
					 aPortal->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE))
					++aPortalCount;
			if (aPortalCount == 4)
			{
				uint32_t aRelocatedHash = 0;
				if (!ValidatePortalCombatSmokeLayout(aBoard, theManifest, theFailure,
					&aRelocatedHash))
					return false;
				if (aRelocatedHash == mPortalInitialLayoutHash)
				{
					// Advice is a board event, while the relocated grid-item set is carried
					// by the following authoritative snapshot.  They can be drained in that
					// order on a client (and the host clears advice immediately before
					// MoveAPortal in the same update), so an unchanged first sample is a
					// delivery race, not evidence of failure.  Remain fail-closed after a
					// bounded window for a subsequent snapshot to expose the new layout.
					if ((uint32_t)(aNow - mPortalAdviceClearTick) >= 5000U)
					{
						theFailure = "Portal Combat relocation advice cleared but no moved portal arrived within 5 seconds";
						return false;
					}
					return true;
				}
				mPortalRelocationVerified = true;
				NetTrace("SMOKE_PORTAL_RELOCATION role=%s before=%08x after=%08x live=4 open=4 pairs=2",
					theManifest.IsHost() ? "host" : "client",
					mPortalInitialLayoutHash, aRelocatedHash);
			}
		}
		return true;
	}

	if (mScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT)
	{
		const uint32_t aNow = GetTickCount();
		if (theManifest.IsHost() && !mTdSpawnAccelerated)
		{
			// Keep the stock wave spawner and stock Zombie::Update walk path; only
			// shorten its initial wait so a visible smoke run remains bounded.
			aBoard->mZombieCountDown = 6;
			aBoard->mZombieCountDownStart = 6;
			mTdSpawnAccelerated = true;
			NetTrace("SMOKE_TD_WAVE_ACCELERATED role=host countdown=6");
		}

		Zombie* aTdZombie = nullptr;
		if (mTdZombieId != 0)
			aTdZombie = FindSmokeZombieById(aBoard, mTdZombieId);
		if (!mTdZombieEntryObserved)
		{
			Zombie* aCandidate = nullptr;
			Zombie* aZombie = nullptr;
			while (aBoard->mZombies.IterateNext(aZombie))
			{
				if (aZombie->mDead || aZombie->mFromWave < 0 ||
					aZombie->mNetId == 0 ||
					aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE ||
					(theManifest.IsClient() && !aZombie->mNetGhost))
					continue;
				if (!aCandidate || aZombie->mNetId < aCandidate->mNetId)
					aCandidate = aZombie;
			}
			if (aCandidate)
			{
				Reanimation* aBody = theApp->ReanimationTryToGet(aCandidate->mBodyReanimID);
				if (aCandidate->mPosX < 730.0f || !aBody || aBody->mAnimRate <= 0.0f)
				{
					NetTrace("SMOKE_TD_ZOMBIE_ENTRY_INVALID role=%s id=%u ghost=%d x=%.2f row=%d body=%p animRate=%.2f",
						theManifest.IsHost() ? "host" : "client", aCandidate->mNetId,
						aCandidate->mNetGhost ? 1 : 0, aCandidate->mPosX, aCandidate->mRow,
						(void*)aBody, aBody ? aBody->mAnimRate : 0.0f);
					theFailure = "Tower Defense zombie did not first appear offscreen with a live stock animation";
					return false;
				}
				mTdZombieId = aCandidate->mNetId;
				mTdZombieFirstX = aCandidate->mPosX;
				mTdZombieEntryObserved = true;
				aTdZombie = aCandidate;
				NetTrace("SMOKE_TD_ZOMBIE_ENTRY role=%s id=%u ghost=%d x=%.2f row=%d animRate=%.2f",
					theManifest.IsHost() ? "host" : "client", aCandidate->mNetId,
					aCandidate->mNetGhost ? 1 : 0, aCandidate->mPosX,
					aCandidate->mRow, aBody->mAnimRate);
			}
		}
		if (mTdZombieEntryObserved && !mTdZombieTurnObserved)
		{
			if (!aTdZombie || aTdZombie->mDead)
			{
				theFailure = "Tower Defense smoke zombie disappeared before its first top-road turn";
				return false;
			}
			Reanimation* aBody = theApp->ReanimationTryToGet(aTdZombie->mBodyReanimID);
			const float aMoved = fabsf(aTdZombie->mPosX - mTdZombieFirstX);
			if (aTdZombie->mRow == 0 && aMoved >= 12.0f && aBody && aBody->mAnimRate > 0.0f)
			{
				mTdZombieTurnObserved = true;
				NetTrace("SMOKE_TD_ZOMBIE_TURN role=%s id=%u row=0 moved=%.2f animRate=%.2f pathPos=%.2f",
					theManifest.IsHost() ? "host" : "client", aTdZombie->mNetId,
					aMoved, aBody->mAnimRate,
					theManifest.IsHost() ? aTdZombie->mTdPathPos : 0.0f);
			}
		}

		if (!mTdZombieTurnObserved)
			return true;
		if (theManifest.IsClient() && !mActionIssued &&
			(uint32_t)(aNow - mSoakLastActionTick) >= 1200U)
		{
			if (IssueSoakSeedAction(theApp, true))
			{
				mSoakPlantsIssued++;
				mActionIssued = true;
				mActionTick = aNow;
				mSoakLastActionTick = aNow;
				NetTrace("SMOKE_CHECKPOINT role=client scenario=tower_defense_client step=ACTION_ISSUED owner=client");
			}
			else if (IssueSoakSunClick(theApp, true))
			{
				mSoakSunClicks++;
				mSoakLastActionTick = aNow;
			}
		}
		if (!mActionApplied && aBoard->mPlants.mSize > mBaselinePlants)
		{
			Plant* aPlant = nullptr;
			Plant* aNewest = nullptr;
			while (aBoard->mPlants.IterateNext(aPlant))
			{
				if (aPlant->mDead || !aPlant->mIsOnBoard)
					continue;
				if (aBoard->NetTdIsPathCell(aPlant->mPlantCol, aPlant->mRow))
				{
					theFailure = "Tower Defense accepted a plant on the dug road";
					return false;
				}
				if (aPlant->mNetId != 0 && (!aNewest || aPlant->mNetId > aNewest->mNetId))
					aNewest = aPlant;
			}
			if (aNewest)
			{
				mTdPlantId = aNewest->mNetId;
				mTdClientPlantObserved = true;
				mActionApplied = true;
				NetTrace("SMOKE_TD_CLIENT_PLANT role=%s id=%u cell=(%d,%d) road=0",
					theManifest.IsHost() ? "host" : "client", aNewest->mNetId,
					aNewest->mPlantCol, aNewest->mRow);
				NetTrace("SMOKE_CHECKPOINT role=%s scenario=tower_defense_client step=ACTION_APPLIED plants=%d baseline=%d",
					theManifest.IsHost() ? "host" : "client",
					aBoard->mPlants.mSize, mBaselinePlants);
			}
		}
		if (mActionIssued && !mActionApplied && aNow - mActionTick >= 45000U)
		{
			theFailure = "Tower Defense client plant was not applied within 45 seconds";
			return false;
		}
		return true;
	}

        if (NetIsInteractiveChooserSmokeScenario(mScenario) &&
		mScenario != NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT)
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

void NetSmokeScenarioRuntime::OnPortalHeldPacketEcho(uint16_t thePacketId,
	int thePacketIndex, int theCursorType)
{
	if (mScenario != NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT || !mActionIssued ||
		theCursorType != (int)CursorType::CURSOR_TYPE_PLANT_FROM_BANK ||
		thePacketId == 0 || thePacketIndex < 0)
		return;
	if (thePacketId != mPortalHeldPacketId || thePacketIndex != mPortalHeldPacketIndex)
	{
		mPortalHeldEchoMismatch = true;
		NetTrace("SMOKE_PORTAL_HELD_ECHO_MISMATCH expectedId=%u expectedIndex=%d actualId=%u actualIndex=%d",
			mPortalHeldPacketId, mPortalHeldPacketIndex, thePacketId, thePacketIndex);
		return;
	}
	mPortalHeldPacketEchoed = true;
	NetTrace("SMOKE_PORTAL_HELD_ECHO packetId=%u index=%d cursor=plant_from_bank",
		thePacketId, thePacketIndex);
}

void NetSmokeScenarioRuntime::OnSlotMachineSound(bool theHost)
{
	if (mScenario != NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT)
		return;
	++mSlotMachineSoundCount;
	NetTrace("SMOKE_SLOT_MACHINE_SOUND role=%s count=%d",
		theHost ? "host" : "client", mSlotMachineSoundCount);
}

void NetSmokeScenarioRuntime::ObservePvPCursorSuppressed(int theLocalSlot, int theRemoteSlot)
{
	if (mScenario != NETSMOKE_SCENARIO_PVP_1V1 || mPvpCursorHiddenVerified ||
		theLocalSlot < 0 || theLocalSlot >= NET_MAX_PLAYERS || theRemoteSlot < 0 ||
		theRemoteSlot >= NET_MAX_PLAYERS || theLocalSlot == theRemoteSlot)
		return;
	mPvpCursorHiddenVerified = true;
	NetTrace("SMOKE_PVP_CURSOR_HIDDEN role=%s remoteSlot=%d remoteValid=1 overlaySuppressed=1 renderer=1",
		theLocalSlot == 0 ? "host" : "client", theRemoteSlot);
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
                         mVictoryAwardObserved &&
                         mVictoryBoardReleasedObserved && mSoakPlantsIssued > 0)) &&
		(mScenario != NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT ||
			(mSoakChooserVerified && mTdPathVerified && mTdZombieEntryObserved &&
			 mTdZombieTurnObserved && mTdClientPlantObserved)) &&
                (mScenario != NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT ||
                        (mPortalCameraVerified && mPortalCameraMovementVerified &&
			 mPortalLayoutVerified && mPortalConveyorVerified && mPortalZombieEntryObserved &&
                         mPortalZombieTeleportObserved && mPortalClientPlantObserved &&
                         mPortalAdviceShown && mPortalAdviceCleared &&
                         mPortalRelocationVerified)) &&
		(mScenario != NETSMOKE_SCENARIO_ENDLESS_ST ||
			(mEndlessModeVerified && mEndlessTimeChanged &&
			 mEndlessWaveProgressed && mEndlessRepickOpened &&
			 mEndlessRepickLogged &&
			 mEndlessRepickCompleted && mEndlessHealthScaled &&
			 mEndlessThawed)) &&
		(mScenario != NETSMOKE_SCENARIO_ENDLESS_OVERFLOW ||
			(mOverflowModeVerified && mOverflowStage255Observed &&
			 mOverflowGargObserved && mOverflowRepickOpened &&
			 mOverflowRepickLogged && mOverflowStage256Observed &&
			 mOverflowRepickCompleted && mSoakChooserVerified &&
			 mOverflowGargBodyHealth > 65535 &&
			 mOverflowGargMaxHealth > 65535)) &&
				(mScenario != NETSMOKE_SCENARIO_PVP_1V1 ||
					(mPvpRosterVerified && mPvpDeckVerified &&
					 mPvpInitialBrainsVerified && mPvpPassiveBrainsVerified &&
					 mPvpCursorHiddenVerified && mPvpZombieEntryObserved &&
					 mPvpHostPlantObserved)) &&
                (!NetIsBowlingSmokeScenario(mScenario) || mBowlingZombieObserved) &&
		theRequiredPostActionSnapshots > 0 &&
		mPostActionSamples >= theRequiredPostActionSnapshots;
}

} // namespace LawnNet
