#include "../Board.h"
#include "../Zombie.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "../Cutscene.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "AlmanacDialog.h"
#include "ImitaterDialog.h"
#include "../System/Music.h"
#include "../../Resources.h"
#include "../../Lawn/Plant.h"
#include "../ToolTipWidget.h"
#include "SeedChooserScreen.h"
#include "../../GameConstants.h"
#include "../System/PlayerInfo.h"
#include "../System/PopDRMComm.h"
#include "../../SexyAppFramework/Debug.h"
#include "../../SexyAppFramework/Dialog.h"
#include "../../SexyAppFramework/MTRand.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../Net/NetManager.h"
#include "../Challenge.h"

using namespace LawnNet;
//0x483380
SeedChooserScreen::SeedChooserScreen()
{
	mCoopChooserActive = false;
	mCoopPicksPerPlayer = 0;
	mCoopMyImitater = SEED_NONE;
	mCoopLastStateHash = 0xFFFFFFFFu;
	for (int i = 0; i < 4; i++)
	{
		mCoopRemotePickCount[i] = 0;
		for (int j = 0; j < 12; j++)
		{
			mCoopRemotePicks[i][j] = SEED_NONE;
			mCoopRemoteImitaters[i][j] = SEED_NONE;
		}
	}

	mApp = (LawnApp*)gSexyAppBase;
	mBoard = mApp->mBoard;
	mClip = false;
	mSeedChooserAge = 0;
	mSeedsInFlight = 0;
	mSeedsInBank = 0;
	mLastMouseX = -1;
	mLastMouseY = -1;
	mChooseState = CHOOSE_NORMAL;
	mViewLawnTime = 0;
	mToolTip = new ToolTipWidget();
	mToolTipSeed = -1;
	mIsSwiping = false;
	mSwipeStartY = -1;
	mScrollAmount = 0;
	mSwipeVelocityY = 0.0f;
	mSwipeJustReleased = false;
	mAllowBetaSeedpackets = (mApp->HasFinishedAdventure() || mApp->mPlayerInfo->mLevel > FINAL_LEVEL) && (mApp->mTodCheatKeys || mApp->mDebugKeysEnabled);

	mStartButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Start);
	mStartButton->SetLabel(_S("[LETS_ROCK_BUTTON]"));
	mStartButton->mButtonImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON;
	mStartButton->mOverImage = nullptr;
	mStartButton->mDownImage = nullptr;
	mStartButton->mDisabledImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_DISABLED;
	mStartButton->mOverOverlayImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON_GLOW;
	mStartButton->SetFont(Sexy::FONT_DWARVENTODCRAFT18YELLOW);
	mStartButton->Resize(154 + mApp->mDDInterface->mWideScreenOffsetX, 545 + mApp->mDDInterface->mWideScreenOffsetY, 156, 42);
	mStartButton->mTextOffsetY = -1;
	EnableStartButton(false);

	mMenuButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Menu);
	mMenuButton->SetLabel(_S("[MENU_BUTTON]"));
	mMenuButton->Resize(681 + mApp->mDDInterface->mWideScreenOffsetX, -10 + mApp->mDDInterface->mWideScreenOffsetY, 117, 46);
	mMenuButton->mDrawStoneButton = true;

	mRandomButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Random);
	mRandomButton->SetLabel(_S("[DEBUG_PLAY]"));
	mRandomButton->mButtonImage = Sexy::IMAGE_BLANK;
	mRandomButton->mOverImage = Sexy::IMAGE_BLANK;
	mRandomButton->mDownImage = Sexy::IMAGE_BLANK;
	mRandomButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mRandomButton->mColors[0] = Color(255, 240, 0);
	mRandomButton->mColors[1] = Color(200, 200, 255);
	mRandomButton->Resize(332 + mApp->mDDInterface->mWideScreenOffsetX, 546 + mApp->mDDInterface->mWideScreenOffsetY, 100, 30);
	if (!mApp->mTodCheatKeys)
	{
		mRandomButton->mBtnNoDraw = true;
	}
	mRandomButton->mDisabled = true;

	Color aBtnColor = Color(42, 42, 90);
	Image* aBtnImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON2;
	Image* aOverImage = Sexy::IMAGE_SEEDCHOOSER_BUTTON2_GLOW;
	int aImageWidth = aBtnImage->GetWidth();
	int aImageHeight = aOverImage->GetHeight();

	mViewLawnButton = new GameButton(SeedChooserScreen::SeedChooserScreen_ViewLawn);
	mViewLawnButton->SetLabel(_S("[VIEW_LAWN]"));
	mViewLawnButton->mButtonImage = aBtnImage;
	mViewLawnButton->mOverImage = aOverImage;
	mViewLawnButton->mDownImage = nullptr;
	mViewLawnButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mViewLawnButton->mColors[0] = aBtnColor;
	mViewLawnButton->mColors[1] = aBtnColor;
	mViewLawnButton->Resize(22, 561, aImageWidth, aImageHeight);
	mViewLawnButton->mParentWidget = this;
	mViewLawnButton->mTextOffsetY = 1;
	if (!mBoard->mCutScene->IsSurvivalRepick())
	{
		mViewLawnButton->mBtnNoDraw = true;
		mViewLawnButton->mDisabled = true;
	}

	mAlmanacButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Almanac);
	mAlmanacButton->SetLabel(_S("[ALMANAC_BUTTON]"));
	mAlmanacButton->mButtonImage = aBtnImage;
	mAlmanacButton->mOverImage = aOverImage;
	mAlmanacButton->mDownImage = nullptr;
	mAlmanacButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mAlmanacButton->mColors[0] = aBtnColor;
	mAlmanacButton->mColors[1] = aBtnColor;
	mAlmanacButton->Resize(560, 572, aImageWidth, aImageHeight);
	mAlmanacButton->mParentWidget = this;
	mAlmanacButton->mTextOffsetY = 1;

	mStoreButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Store);
	mStoreButton->SetLabel(_S("[SHOP_BUTTON]"));
	mStoreButton->mButtonImage = aBtnImage;
	mStoreButton->mOverImage = aOverImage;
	mStoreButton->mDownImage = nullptr;
	mStoreButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mStoreButton->mColors[0] = aBtnColor;
	mStoreButton->mColors[1] = aBtnColor;
	mStoreButton->Resize(680, 572, aImageWidth, aImageHeight);
	mStoreButton->mParentWidget = this;
	mStoreButton->mTextOffsetY = 1;

	mImitaterButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Imitater);
	mImitaterButton->mButtonImage = Sexy::IMAGE_BLANK;
	mImitaterButton->mOverImage = Sexy::IMAGE_BLANK;
	mImitaterButton->mDownImage = Sexy::IMAGE_BLANK;
	mImitaterButton->mDisabledImage = Sexy::IMAGE_BLANK;
	mImitaterButton->Resize(464, 515, Sexy::IMAGE_IMITATERSEED->mWidth, Sexy::IMAGE_IMITATERSEED->mHeight);
	mImitaterButton->mParentWidget = this;

	mScrollbar = new ScrollbarWidget(mApp, ScrollbarMode::VERTICAL);
	mScrollbar->mScrollRange = 70 * max(0, ceil((mApp->GetSeedsAvailable() - 49) / 8.0f));
	mScrollbar->Resize(445, 123, 8, 420);
	int offsetY = Has7Rows() || mAllowBetaSeedpackets ? 0 : 40;
	int startY = 123 + offsetY;
	mScrollbar->mViewport = Rect(22, startY, 443, 528);
	mScrollbar->mButtonNoDraw = mScrollbar->mScrollRange == 0;
	mScrollbar->mDisabled = mScrollbar->mButtonNoDraw;

	if (!mApp->CanShowAlmanac())
	{
		mAlmanacButton->mBtnNoDraw = true;
		mAlmanacButton->mDisabled = true;
	}
	if (!mApp->CanShowStore())
	{
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->mDisabled = true;
	}

	DBG_ASSERT(mApp->GetSeedsAvailable() <= NUM_SEED_TYPES);
	memset(mChosenSeeds, 0, sizeof(mChosenSeeds));
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		aChosenSeed.mSeedType = aSeedType;
		GetSeedPositionInChooser(aSeedType, aChosenSeed.mX, aChosenSeed.mY);
		aChosenSeed.mTimeStartMotion = 0;
		aChosenSeed.mTimeEndMotion = 0;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		aChosenSeed.mEndX = aChosenSeed.mX;
		aChosenSeed.mEndY = aChosenSeed.mY;
		aChosenSeed.mSeedState = aSeedType == SEED_IMITATER ? SEED_PACKET_HIDDEN : SEED_IN_CHOOSER;
		aChosenSeed.mSeedIndexInBank = 0;
		aChosenSeed.mRefreshCounter = 0;
		aChosenSeed.mRefreshing = false;
		aChosenSeed.mImitaterType = SEED_NONE;
		aChosenSeed.mCrazyDavePicked = false;
	}
	if (mBoard->mCutScene->IsSurvivalRepick())
	{
		for (int anIdx = 0; anIdx < mBoard->mSeedBank->mNumPackets; anIdx++)
		{
			SeedPacket* aSeedPacket = &mBoard->mSeedBank->mSeedPackets[anIdx];
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedPacket->mPacketType];
			aChosenSeed.mRefreshing = aSeedPacket->mRefreshing;
			aChosenSeed.mRefreshCounter = aSeedPacket->mRefreshCounter;
		}
		mBoard->mSeedBank->mNumPackets = 0;
	}
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_SEEING_STARS)
	{
		ChosenSeed& aStarFruit = mChosenSeeds[SEED_STARFRUIT];
		int aX = mBoard->GetSeedPacketPositionX(0);
		aStarFruit.mX = aX, aStarFruit.mY = 8;
		aStarFruit.mStartX = aX, aStarFruit.mStartY = 8;
		aStarFruit.mEndX = aX, aStarFruit.mEndY = 8;
		aStarFruit.mSeedState = SEED_IN_BANK;
		aStarFruit.mSeedIndexInBank = 0;
		aStarFruit.mCrazyDavePicked = true;
		mSeedsInBank++;
	}
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT)
	{
		ChosenSeed& aWallnut = mChosenSeeds[SEED_WALLNUT];
		int aX = mBoard->GetSeedPacketPositionX(0);
		aWallnut.mX = aX, aWallnut.mY = 8;
		aWallnut.mStartX = aX, aWallnut.mStartY = 8;
		aWallnut.mEndX = aX, aWallnut.mEndY = 8;
		aWallnut.mSeedState = SEED_IN_BANK;
		aWallnut.mSeedIndexInBank = 0;
		aWallnut.mCrazyDavePicked = true;
		mSeedsInBank++;
	}
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER)
	{
		{
			ChosenSeed& aStarFruit = mChosenSeeds[SEED_STARFRUIT];
			int aX = mBoard->GetSeedPacketPositionX(0);
			aStarFruit.mX = aX, aStarFruit.mY = 8;
			aStarFruit.mStartX = aX, aStarFruit.mStartY = 8;
			aStarFruit.mEndX = aX, aStarFruit.mEndY = 8;
			aStarFruit.mSeedState = SEED_IN_BANK;
			aStarFruit.mSeedIndexInBank = 0;
			aStarFruit.mCrazyDavePicked = true;
			mSeedsInBank++;
		}

		{
			ChosenSeed& aWallnut = mChosenSeeds[SEED_WALLNUT];
			int aX = mBoard->GetSeedPacketPositionX(1);
			aWallnut.mX = aX, aWallnut.mY = 8;
			aWallnut.mStartX = aX, aWallnut.mStartY = 8;
			aWallnut.mEndX = aX, aWallnut.mEndY = 8;
			aWallnut.mSeedState = SEED_IN_BANK;
			aWallnut.mSeedIndexInBank = 1;
			aWallnut.mCrazyDavePicked = true;
			mSeedsInBank++;
		}

		{
			ChosenSeed& aUmbrella = mChosenSeeds[SEED_UMBRELLA];
			int aX = mBoard->GetSeedPacketPositionX(2);
			aUmbrella.mX = aX, aUmbrella.mY = 8;
			aUmbrella.mStartX = aX, aUmbrella.mStartY = 8;
			aUmbrella.mEndX = aX, aUmbrella.mEndY = 8;
			aUmbrella.mSeedState = SEED_IN_BANK;
			aUmbrella.mSeedIndexInBank = 2;
			aUmbrella.mCrazyDavePicked = true;
			mSeedsInBank++;
		}
	}
	if (mApp->IsAdventureMode() && (!mApp->IsFirstTimeAdventureMode() || mApp->mPlayerLevelRef > 4 && mBoard->mLevel < mApp->mPlayerLevelRef))
		CrazyDavePickSeeds();
	UpdateImitaterButton();
}

template <typename T>
T SeedChooserScreen::PickFromWeightedArrayUsingSpecialRandSeed(TodWeightedArray<T>* theArray, int theCount, MTRand& theLevelRNG)
{
	int aTotalWeight = 0;
	for (int i = 0; i < theCount; i++)
		aTotalWeight += theArray[i].mWeight;
	DBG_ASSERT(aTotalWeight > 0);

	int aRndResult = theLevelRNG.Next((unsigned long)aTotalWeight);
	int aWeight = 0;
	for (int j = 0; j < theCount; j++)
	{
		aWeight += theArray[j].mWeight;
		if (aWeight > aRndResult) return theArray[j].mItem;
	}
	return T{};
	//DBG_ASSERT(false);
}

//0x483F70
void SeedChooserScreen::CrazyDavePickSeeds()
{
	TodWeightedArray<SeedType> aSeedArray[NUM_SEED_TYPES];
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		aSeedArray[aSeedType].mItem = aSeedType;
		uint aRecFlags = SeedNotRecommendedToPick(aSeedType);
		if ((aSeedType == SEED_GATLINGPEA && !mApp->mPlayerInfo->mPurchases[STORE_ITEM_PLANT_GATLINGPEA]) || !mApp->SeedTypeAvailable(aSeedType) ||
			SeedNotAllowedToPick(aSeedType) || Plant::IsUpgrade(aSeedType) || aSeedType == SEED_IMITATER || aSeedType == SEED_UMBRELLA || aSeedType == SEED_BLOVER ||
			TestBit(aRecFlags, NOT_RECOMMENDED_NEEDS_POOL) || Plant::IsNocturnal(aSeedType) && TestBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL))
		{
			aSeedArray[aSeedType].mWeight = 0;
		}
		else
		{
			aSeedArray[aSeedType].mWeight = 1;
		}
	}
	if (mBoard->mZombieAllowed[ZOMBIE_BUNGEE] || mBoard->mZombieAllowed[ZOMBIE_CATAPULT])
	{
		aSeedArray[SEED_UMBRELLA].mWeight = 1;
	}
	if (mBoard->mZombieAllowed[ZOMBIE_BALLOON] || mBoard->StageHasFog())
	{
		aSeedArray[SEED_BLOVER].mWeight = 1;
	}
	if (mBoard->StageHasRoof())
	{
		aSeedArray[SEED_TORCHWOOD].mWeight = 0;
	}
	
	MTRand aLevelRNG = MTRand(mBoard->GetLevelRandSeed());
	for (int i = 0; i < 3; i++)
	{
		SeedType aPickedSeed = (SeedType)PickFromWeightedArrayUsingSpecialRandSeed(aSeedArray, NUM_SEEDS_IN_CHOOSER, aLevelRNG);
		aSeedArray[aPickedSeed].mWeight = 0;
		ChosenSeed& aChosenSeed = mChosenSeeds[aPickedSeed];

		int aPosX = mBoard->GetSeedPacketPositionX(i);
		aChosenSeed.mX = aPosX;
		aChosenSeed.mY = 8;
		aChosenSeed.mStartX = aPosX;
		aChosenSeed.mStartY = 8;
		aChosenSeed.mEndX = aPosX;
		aChosenSeed.mEndY = 8;
		aChosenSeed.mSeedState = SEED_IN_BANK;
		aChosenSeed.mSeedIndexInBank = i;
		aChosenSeed.mCrazyDavePicked = true;
		mSeedsInBank++;
	}
}

//0x484220
bool SeedChooserScreen::Has7Rows()
{
	for (SeedType aSeedType = SEED_GATLINGPEA; aSeedType <= SEED_COBCANNON; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (aSeedType != SEED_SPIKEROCK && mApp->SeedTypeAvailable(aSeedType))
		{
			return true;
		}
	}

	return false;
}

//0x484400
void SeedChooserScreen::GetSeedPositionInChooser(int theIndex, int& x, int& y)
{
	if (theIndex == SEED_IMITATER)
	{
		x = mImitaterButton->mX;
		y = mImitaterButton->mY;
	}
	else
	{
		if (theIndex > SeedType::SEED_IMITATER) theIndex--;

		int aRow = theIndex / 8;
		int aCol = theIndex % 8;

		x = aCol * 53 + 22;
		if (Has7Rows() || mAllowBetaSeedpackets)
		{
			y = aRow * 70 + 123;
		}
		else
		{
			y = aRow * 73 + 163; // + 35 They dont know how to center (?)
		}
	}
}

//0x484480
void SeedChooserScreen::GetSeedPositionInBank(int theIndex, int& x, int& y)
{
	x = mBoard->mSeedBank->mX - mX + mBoard->GetSeedPacketPositionX(theIndex) + mApp->mDDInterface->mWideScreenOffsetX;
	y = mBoard->mSeedBank->mY - mY + 8 + mApp->mDDInterface->mWideScreenOffsetY;
}

//0x4844D0
SeedChooserScreen::~SeedChooserScreen()
{
	if (mStartButton) delete mStartButton;
	if (mRandomButton) delete mRandomButton;
	if (mViewLawnButton) delete mViewLawnButton;
	if (mAlmanacButton) delete mAlmanacButton;
	if (mImitaterButton) delete mImitaterButton;
	if (mStoreButton) delete mStoreButton;
	if (mToolTip) delete mToolTip;
	if (mMenuButton) delete mMenuButton;
	if (mScrollbar) delete mScrollbar;
}

//0x4845E0
unsigned int SeedChooserScreen::SeedNotRecommendedToPick(SeedType theSeedType)
{
	uint aRecFlags = mBoard->SeedNotRecommendedForLevel(theSeedType);
	if (TestBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL) && PickedPlantType(SEED_INSTANT_COFFEE))
		SetBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL, false);
	return aRecFlags;
}

//0x484620
bool SeedChooserScreen::SeedNotAllowedToPick(SeedType theSeedType)
{
	return mApp->IsLastStand() && (theSeedType == SEED_SUNFLOWER || theSeedType == SEED_SUNSHROOM ||
		theSeedType == SEED_TWINSUNFLOWER || theSeedType == SEED_SEASHROOM || theSeedType == SEED_PUFFSHROOM);
}

//0x484640
bool SeedChooserScreen::SeedNotAllowedDuringTrial(SeedType theSeedType)
{
	return mApp->IsTrialStageLocked() && (theSeedType == SEED_SQUASH || theSeedType == SEED_THREEPEATER);
}

//0x484690
void SeedChooserScreen::Draw(Graphics* g)
{
	if (mApp->GetDialog(DIALOG_STORE) || mApp->GetDialog(DIALOG_ALMANAC))
		return;

	g->SetLinearBlend(true);
	if (!mBoard->ChooseSeedsOnCurrentLevel() || (mBoard->mCutScene && mBoard->mCutScene->IsBeforePreloading()))
		return;

	g->DrawImage(Sexy::IMAGE_SEEDCHOOSER_BACKGROUND, 0, 87);
	if (mApp->SeedTypeAvailable(SEED_IMITATER))
	{
		g->DrawImage(Sexy::IMAGE_SEEDCHOOSER_IMITATERADDON, 459, 503);
	}
	TodDrawString(g, _S("[CHOOSE_YOUR_PLANTS]"), 229, 110, Sexy::FONT_DWARVENTODCRAFT18YELLOW, Color::White, DS_ALIGN_CENTER);

	int aNumSeeds = Has7Rows() ? 49 : 41;
	if (mAllowBetaSeedpackets) 
	{
		aNumSeeds += NUM_SEEDS_IN_CHOOSER - SEED_IMITATER;
		if (!Has7Rows())	aNumSeeds += 8;
	}
	for (SeedType aSeedShadow = SEED_PEASHOOTER; aSeedShadow < aNumSeeds - 1; aSeedShadow = (SeedType)(aSeedShadow + 1))
	{
		int x, y;
		GetSeedPositionInChooser(aSeedShadow, x, y);
		y -= mScrollbar->mScrollValue;
		if (aSeedShadow == SEED_IMITATER)
		{
			continue;
		}

		if (mApp->SeedTypeAvailable(aSeedShadow))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedShadow];
			if (aChosenSeed.mSeedState != SEED_IN_CHOOSER || CoopIsPickedByOthers(aSeedShadow))
			{
				Graphics aSeedGraphics(*g);
				const bool expandedRows = Has7Rows() || mAllowBetaSeedpackets;
				int offsetY = expandedRows ? 0 : 40;
				int seedHeight = expandedRows ? 73 : 70;
				int startY = 123 + offsetY;
				aSeedGraphics.SetClipRect(Rect(22, startY, 443, seedHeight * 5 - 68 + startY));
				
				DrawSeedPacket(&aSeedGraphics, x, y, aSeedShadow, SEED_NONE, 0, 55, true, false);
			}
		}
		else
		{
			Graphics aSeedGraphics(*g);
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedShadow];
			if (aChosenSeed.mSeedState == SEED_IN_CHOOSER)
			{
				const bool expandedRows = Has7Rows() || mAllowBetaSeedpackets;
				int offsetY = expandedRows ? 0 : 40;
				int seedHeight = expandedRows ? 73 : 70;
				int startY = 123 + offsetY;
				aSeedGraphics.SetClipRect(Rect(22, startY, 53 * 7 + 50 + 22, seedHeight * 5 - 68 + startY));
			}
			aSeedGraphics.DrawImage(Sexy::IMAGE_SEEDPACKETSILHOUETTE, x, y);
		}
	}

	int aNumSeedsInBank = min(mBoard->mSeedBank->mNumPackets, aNumSeeds);
	for (int anIndex = 0; anIndex < aNumSeedsInBank; anIndex++)
	{
		if (FindSeedInBank(anIndex) == SEED_NONE)
		{
			int x, y;
			GetSeedPositionInBank(anIndex, x, y);
			g->DrawImage(Sexy::IMAGE_SEEDPACKETSILHOUETTE, x, y);
		}
	}

	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		ChosenSeedState aSeedState = aChosenSeed.mSeedState;
		if (mApp->SeedTypeAvailable(aSeedType) && aSeedState != SEED_FLYING_TO_BANK && aSeedState != SEED_SHIFTING_IN_BANK && aSeedState != SEED_FLYING_TO_CHOOSER &&
			aSeedState != SEED_PACKET_HIDDEN && (aSeedState == SEED_IN_CHOOSER || mBoard->mCutScene->mSeedChoosing))
		{
			if (aSeedState == SEED_IN_CHOOSER && CoopIsPickedByOthers(aSeedType))
				continue;
			bool aGrayed = false;
			if (((SeedNotRecommendedToPick(aSeedType) || SeedNotAllowedToPick(aSeedType)) && aSeedState == SEED_IN_CHOOSER) ||
				SeedNotAllowedDuringTrial(aSeedType))
				aGrayed = true;
			
			int aPosX = aChosenSeed.mX;
			int aPosY = aChosenSeed.mY;
			if (aSeedState == SEED_IN_BANK)
			{
				aPosX -= mX - mApp->mDDInterface->mWideScreenOffsetX;
				aPosY -= mY - mApp->mDDInterface->mWideScreenOffsetY;
			}
			else
			{
				aPosY -= mScrollbar->mScrollValue;
			}

			Graphics aSeedGraphics(*g);
			if (aSeedState != SEED_IN_BANK)
			{
				const bool expandedRows = Has7Rows() || mAllowBetaSeedpackets;
				int offsetY = expandedRows ? 0 : 40;
				int seedHeight = expandedRows ? 73 : 70;
				int startY = 123 + offsetY;
				aSeedGraphics.SetClipRect(Rect(22, startY, 443, seedHeight * 5 - 68 + startY));
			}
			DrawSeedPacket(&aSeedGraphics, aPosX, aPosY, aChosenSeed.mSeedType, aChosenSeed.mImitaterType, 0, aGrayed ? 115 : 255, true, false);
			if (mCoopChooserActive && aSeedState == SEED_IN_BANK &&
				aChosenSeed.mCrazyDavePicked && mApp->NetIsClient())
			{
				const NetColor aColor = GetRelativePlayerColor(mApp->mNetManager->GetMySlot(), 0);
				aSeedGraphics.SetColor(Color(aColor.r, aColor.g, aColor.b, 70));
				aSeedGraphics.FillRect((int)aPosX + 1, (int)aPosY + 1, SEED_PACKET_WIDTH - 2, SEED_PACKET_HEIGHT - 2);
			}
		}
	}

	//mImitaterButton->Draw(g);
	if (mApp->SeedTypeAvailable(SEED_IMITATER))
	{
		DrawSeedPacket(g, mImitaterButton->mX, mImitaterButton->mY, SeedType::SEED_IMITATER, SEED_NONE, 0, mImitaterButton->mDisabled ? 55 : 255, false, false);
	}

	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		ChosenSeedState aSeedState = aChosenSeed.mSeedState;
		if (mApp->SeedTypeAvailable(aSeedType) && (aSeedState == SEED_FLYING_TO_BANK || aSeedState == SEED_SHIFTING_IN_BANK || aSeedState == SEED_FLYING_TO_CHOOSER))
		{
			if (aSeedState == SEED_FLYING_TO_BANK)
			{
				GetSeedPositionInChooser(aSeedType, aChosenSeed.mStartX, aChosenSeed.mStartY);
				if (aChosenSeed.mSeedType != SeedType::SEED_IMITATER)
					aChosenSeed.mStartY -= mScrollbar->mScrollValue;
			}
			else if (aSeedState == SEED_FLYING_TO_CHOOSER)
			{
				GetSeedPositionInChooser(aSeedType, aChosenSeed.mEndY, aChosenSeed.mEndY);
				if (aChosenSeed.mSeedType != SeedType::SEED_IMITATER)
					aChosenSeed.mEndY -= mScrollbar->mScrollValue;
			}

			DrawSeedPacket(g, aChosenSeed.mX, aChosenSeed.mY, aChosenSeed.mSeedType, aChosenSeed.mImitaterType, 0, 255, true, false);
		}
	}

	if (!mScrollbar->mDisabled)
	{
		mScrollbar->mButtonNoDraw = false;
		Graphics mScrollG(*g);
		mScrollG.mTransX = g->mTransX + mScrollbar->mX;
		mScrollG.mTransY = g->mTransY + mScrollbar->mY;
		mScrollbar->Draw(&mScrollG);
		mScrollbar->mButtonNoDraw = true;
	}
	g->PushState();
	g->TranslateF(-mApp->mDDInterface->mWideScreenOffsetX, -mApp->mDDInterface->mWideScreenOffsetY);
	mStartButton->Draw(g);
	if (mCoopChooserActive && mApp->mNetManager)
	{
		g->PushState();
		g->SetColor(mApp->mNetManager->IsMyReady() ? Color(40, 195, 45, 48) : Color(235, 45, 30, 48));
		g->FillRect(mStartButton->mX + 3, mStartButton->mY + 3,
			mStartButton->mWidth - 6, mStartButton->mHeight - 6);
		g->PopState();
	}
	mRandomButton->Draw(g);
	g->PopState();
	mViewLawnButton->Draw(g);
	mAlmanacButton->Draw(g);
	mStoreButton->Draw(g);
	Graphics aBoardFrameG = Graphics(*g);
	aBoardFrameG.mTransX -= mX;
	aBoardFrameG.mTransY -= mY;
	mMenuButton->Draw(&aBoardFrameG);
	mToolTip->Draw(g);

	CoopDrawRemotePicks(g);
}

//0x484C30
void SeedChooserScreen::UpdateViewLawn()
{
	if (mChooseState != CHOOSE_VIEW_LAWN) return;
	mViewLawnTime++;
	if (mViewLawnTime == 100) mBoard->DisplayAdviceAgain(_S("[CLICK_TO_CONTINUE]"), MESSAGE_STYLE_HINT_STAY, ADVICE_CLICK_TO_CONTINUE);
	else if (mViewLawnTime == 251) mViewLawnTime = 250;

	int aBoardX = BOARD_IMAGE_WIDTH_OFFSET - 800;
	int aSeedChooserY = SEED_CHOOSER_OFFSET_Y - Sexy::IMAGE_SEEDCHOOSER_BACKGROUND->mHeight;
	if (mViewLawnTime <= 100)
	{
		mBoard->Move(-TodAnimateCurve(0, 100, mViewLawnTime, aBoardX, 0, CURVE_EASE_IN_OUT), 0);
		Move(mApp->mDDInterface->mWideScreenOffsetX, TodAnimateCurve(0, 40, mViewLawnTime, aSeedChooserY, SEED_CHOOSER_OFFSET_Y - WIDESCREEN_OFFSETY, CURVE_EASE_IN_OUT) + mApp->mDDInterface->mWideScreenOffsetY);
	}
	else if (mViewLawnTime <= 250)
	{
		mBoard->Move(0, 0);
		Move(mApp->mDDInterface->mWideScreenOffsetX, SEED_CHOOSER_OFFSET_Y - WIDESCREEN_OFFSETY + mApp->mDDInterface->mWideScreenOffsetY);
	}
	else if (mViewLawnTime <= 350)
	{
		mBoard->ClearAdvice(ADVICE_CLICK_TO_CONTINUE);
		mBoard->Move(-TodAnimateCurve(250, 350, mViewLawnTime, 0, aBoardX, CURVE_EASE_IN_OUT), 0);
		Move(mApp->mDDInterface->mWideScreenOffsetX, TodAnimateCurve(310, 350, mViewLawnTime, SEED_CHOOSER_OFFSET_Y - WIDESCREEN_OFFSETY, aSeedChooserY, CURVE_EASE_IN_OUT) + mApp->mDDInterface->mWideScreenOffsetY);
	}
	else
	{
		mChooseState = CHOOSE_NORMAL;
		mViewLawnTime = 0;
		mMenuButton->mDisabled = false;
	}
}

//0x484FD0
void SeedChooserScreen::LandFlyingSeed(ChosenSeed& theChosenSeed)
{
	if (theChosenSeed.mSeedState == SEED_FLYING_TO_BANK || theChosenSeed.mSeedState == SEED_SHIFTING_IN_BANK)
	{
		theChosenSeed.mX = theChosenSeed.mEndX;
		theChosenSeed.mY = theChosenSeed.mEndY;
		theChosenSeed.mTimeStartMotion = 0;
		theChosenSeed.mTimeEndMotion = 0;
		theChosenSeed.mSeedState = SEED_IN_BANK;
		mSeedsInFlight--;
	}
	else if (theChosenSeed.mSeedState == SEED_FLYING_TO_CHOOSER)
	{
		GetSeedPositionInChooser(theChosenSeed.mSeedType, theChosenSeed.mEndX, theChosenSeed.mEndY);
		theChosenSeed.mX = theChosenSeed.mEndX;
		theChosenSeed.mY = theChosenSeed.mEndY;
		theChosenSeed.mTimeStartMotion = 0;
		theChosenSeed.mTimeEndMotion = 0;
		theChosenSeed.mSeedState = SEED_IN_CHOOSER;
		mSeedsInFlight--;
		if (theChosenSeed.mSeedType == SEED_IMITATER)
		{
			theChosenSeed.mSeedState = SEED_PACKET_HIDDEN;
			theChosenSeed.mImitaterType = SEED_NONE;
			UpdateImitaterButton();
		}
	}
}

//0x485040
void SeedChooserScreen::UpdateCursor()
{
	if (mApp->GetDialogCount() || mBoard->mCutScene->IsInShovelTutorial() || mApp->mGameMode == GAMEMODE_UPSELL || !mApp->mWidgetManager->mOverWidget) return;

	mApp->mWidgetManager->SetFocus(this);
	
	SeedType aMouseSeedType = SeedHitTest(mLastMouseX, mLastMouseY);
	if (aMouseSeedType != SEED_NONE)
	{
		ChosenSeed& aMouseChosenSeed = mChosenSeeds[aMouseSeedType];
		if (aMouseChosenSeed.mSeedState == SEED_IN_BANK && aMouseChosenSeed.mCrazyDavePicked)
			aMouseSeedType = SEED_NONE;
	}

	if (mMouseVisible && mChooseState != CHOOSE_VIEW_LAWN && (aMouseSeedType != SEED_NONE && !SeedNotAllowedToPick(aMouseSeedType)) ||
		mRandomButton->IsMouseOver() || mViewLawnButton->IsMouseOver() || mAlmanacButton->IsMouseOver() || mImitaterButton->IsMouseOver() ||
		mStoreButton->IsMouseOver() || mMenuButton->IsMouseOver() || mStartButton->IsMouseOver() || mScrollbar->isThumbOver())
	{
		mApp->SetCursor(CURSOR_HAND);
	}
	else
	{
		mApp->SetCursor(CURSOR_POINTER);
	}
}

//0x4851A0
void SeedChooserScreen::Update()
{
	CoopPollNetwork();

	Widget::Update();

	float aScrollSpeed = 1 + abs(mScrollAmount) * 0.7f;
	mScrollbar->mScrollValue = ClampFloat(mScrollbar->mScrollValue += mScrollAmount * aScrollSpeed, 0, mScrollbar->mScrollRange);

	if (mScrollAmount != 0) {
		mWidgetManager->RehupMouse();
		RemoveToolTip();
		ShowToolTip();
	}

	mScrollAmount *= 0.9f;

	mLastMouseX = mApp->mWidgetManager->mLastMouseX;
	mLastMouseY = mApp->mWidgetManager->mLastMouseY;

	mSeedChooserAge++;
	mToolTip->Update();

	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (aChosenSeed.mSeedState == SEED_FLYING_TO_BANK || aChosenSeed.mSeedState == SEED_SHIFTING_IN_BANK || aChosenSeed.mSeedState == SEED_FLYING_TO_CHOOSER)
			{
				int aTimeStart = aChosenSeed.mTimeStartMotion;
				int aTimeEnd = aChosenSeed.mTimeEndMotion;
				aChosenSeed.mX = TodAnimateCurve(aTimeStart, aTimeEnd, mSeedChooserAge, aChosenSeed.mStartX, aChosenSeed.mEndX, CURVE_EASE_IN_OUT);
				aChosenSeed.mY = TodAnimateCurve(aTimeStart, aTimeEnd, mSeedChooserAge, aChosenSeed.mStartY, aChosenSeed.mEndY, CURVE_EASE_IN_OUT);
				if (mSeedChooserAge >= aChosenSeed.mTimeEndMotion)
				{
					LandFlyingSeed(aChosenSeed);
				}
			}
		}
	}

	CoopRefreshStartButton();
	if (mCoopChooserActive && mApp->mNetManager && mApp->mNetManager->IsHosting() &&
		mApp->mNetManager->AreAllPlayersReady() && CoopAllPicksComplete())
	{
		OnStartButton();
		return;
	}
	ShowToolTip();
	mStartButton->Update();
	mRandomButton->Update();
	mViewLawnButton->Update();
	mAlmanacButton->Update();
	mImitaterButton->Update();
	mStoreButton->Update();
	mMenuButton->Update();
	mScrollbar->Update();
	UpdateViewLawn();
	UpdateCursor();
	MarkDirty();
}

//0x485340
bool SeedChooserScreen::DisplayRepickWarningDialog(const SexyChar* theMessage)
{
	return mApp->LawnMessageBox(
		Dialogs::DIALOG_CHOOSER_WARNING, 
		_S("[DIALOG_WARNING]"), 
		theMessage, 
		_S("[DIALOG_BUTTON_YES]"), 
		_S("[REPICK_BUTTON]"), 
		Dialog::BUTTONS_YES_NO
	) == Dialog::ID_YES;
}

//0x485370
bool SeedChooserScreen::FlyersAreComming()
{
	for (int aWave = 0; aWave < mBoard->mNumWaves; aWave++)
	{
		for (int anIndex = 0; anIndex < MAX_ZOMBIES_IN_WAVE; anIndex++)
		{
			ZombieType aZombieType = mBoard->mZombiesInWave[aWave][anIndex];
			if (aZombieType == ZOMBIE_INVALID)
				break;

			if (aZombieType == ZOMBIE_BALLOON)
				return true;
		}
	}
	return false;
}

//0x4853D0
bool SeedChooserScreen::FlyProtectionCurrentlyPlanted()
{
	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == SEED_CATTAIL || aPlant->mSeedType == SEED_CACTUS
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
			|| aPlant->mSeedType == SEED_STINGER || aPlant->mSeedType == SEED_BEEHIVE
#endif
			)
		{
			return true;
		}
	}
	return false;
}

//0x485420
bool SeedChooserScreen::CheckSeedUpgrade(SeedType theSeedTypeTo, SeedType theSeedTypeFrom)
{
	if (mApp->IsSurvivalMode() || !PickedPlantType(theSeedTypeTo) || PickedPlantType(theSeedTypeFrom))
		return true;

	SexyString aWarning = TodStringTranslate(_S("[SEED_CHOOSER_UPGRADE_WARNING]"));
	aWarning = TodReplaceString(aWarning, _S("{UPGRADE_TO}"), Plant::GetNameString(theSeedTypeTo));
	aWarning = TodReplaceString(aWarning, _S("{UPGRADE_FROM}"), Plant::GetNameString(theSeedTypeFrom));
	return DisplayRepickWarningDialog(aWarning.c_str());
}

//0x485600
void SeedChooserScreen::OnStartButton()
{
	if (mCoopChooserActive && mApp->mNetManager)
	{
		// The widget stays alive during the stock slide-out. Starting it again
		// would reset EndSeedChooser's cutscene position every frame.
		if (!mMouseVisible || !mBoard->mCutScene->mSeedChoosing)
			return;
		if (!mApp->mNetManager->IsHosting() || !mApp->mNetManager->AreAllPlayersReady() || !CoopAllPicksComplete())
			return;
		// An aborted stock warning must require the host to ready again.
		mApp->mNetManager->SetMyReady(false);
	}
	if (mCoopChooserActive && mApp->mNetManager && mApp->mNetManager->IsHosting())
	{
		// The host is the only start authority.  Every active eligible slot must
		// exactly satisfy the host-issued quota in every co-op mode; otherwise a
		// premature start would silently build an undersized bank.
		const bool aPvP = mApp->mGameMode == GAMEMODE_COOP_PVP;
		int aQuotaTotal = 0;
		int aPickTotal = 0;
		int anEligibleCount = 0;
		for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
		{
			const NetPlayerInfo& aPlayer = mApp->mNetManager->GetPlayer(aSlot);
			if (!aPlayer.mActive || aPlayer.mSpectator || (aPvP && aPlayer.mTeam != NETTEAM_PLANTS))
				continue;
			anEligibleCount++;
			int aCount = aSlot == mApp->mNetManager->GetMySlot() ?
				CoopGetLocalQuotaPickCount() : mCoopRemotePickCount[aSlot];
			int anExpectedCount = mApp->mNetManager->mChooserPickQuota[aSlot];
			aPickTotal += aCount;
			aQuotaTotal += anExpectedCount;
			if (aCount != anExpectedCount)
			{
				LawnNet::NetTrace("chooser start blocked slot=%d count=%d quota=%d mode=%d",
					aSlot, aCount, anExpectedCount, (int)mApp->mGameMode);
				mApp->PlaySample(Sexy::SOUND_BUZZER);
				return;
			}
		}
		if (anEligibleCount <= 0 || aQuotaTotal != mApp->mNetManager->mChooserBankCount ||
			aPickTotal != mApp->mNetManager->mChooserBankCount)
		{
			LawnNet::NetTrace("chooser start blocked roster/quota mismatch eligible=%d picks=%d quotas=%d bank=%d",
				anEligibleCount, aPickTotal, aQuotaTotal,
				mApp->mNetManager->mChooserBankCount);
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}

	if (mApp->mGameMode == GAMEMODE_CHALLENGE_SEEING_STARS && !PickedPlantType(SEED_STARFRUIT))
	{
		if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_SEEING_STARS_WARNING]")))
		{
			return;
		}
	}

	if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 11 && !PickedPlantType(SEED_PUFFSHROOM))
	{
		if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_PUFFSHROOM_WARNING]")))
		{
			return;
		}
	}
	if (!PickedPlantType(SEED_SUNFLOWER) && !PickedPlantType(SEED_TWINSUNFLOWER) && !PickedPlantType(SEED_SUNSHROOM) &&
		!mBoard->mCutScene->IsSurvivalRepick() && !mApp->IsLastStand())
	{
		if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 11)
		{
			if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_NIGHT_SUN_WARNING]")))
			{
				return;
			}
		}
		else if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_SUN_WARNING]")))
		{
			return;
		}
	}
	if (mBoard->StageHasPool() && !PickedPlantType(SEED_LILYPAD) && !PickedPlantType(SEED_SEASHROOM) && !PickedPlantType(SEED_TANGLEKELP) && !mBoard->mCutScene->IsSurvivalRepick())
	{
		if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 21)
		{
			if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_LILY_WARNING]")))
			{
				return;
			}
		}
		else if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_POOL_WARNING]")))
		{
			return;
		}
	}
	if (mBoard->StageHasRoof() && !PickedPlantType(SEED_FLOWERPOT) && mApp->SeedTypeAvailable(SEED_FLOWERPOT))
	{
		if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_ROOF_WARNING]")))
		{
			return;
		}
	}

	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT && !PickedPlantType(SEED_WALLNUT))
	{
		if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_ART_WALLNUT_WARNING]")))
		{
			return;
		}
	}
	if (mApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER && 
		(!PickedPlantType(SEED_STARFRUIT) || !PickedPlantType(SEED_UMBRELLA) || !PickedPlantType(SEED_WALLNUT)))
	{
		if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_ART_2_WARNING]")))
		{
			return;
		}
	}

	if (FlyersAreComming() && !FlyProtectionCurrentlyPlanted() && !PickedPlantType(SEED_CATTAIL) && !PickedPlantType(SEED_CACTUS) && !PickedPlantType(SEED_BLOVER) 
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
		&& !PickedPlantType(SEED_STINGER) && !PickedPlantType(SEED_BEEHIVE)
#endif
		)
	{
		if (!DisplayRepickWarningDialog(_S("[SEED_CHOOSER_FLYER_WARNING]")))
		{
			return;
		}
	}

	if (!CheckSeedUpgrade(SEED_GATLINGPEA, SEED_REPEATER) || 
		!CheckSeedUpgrade(SEED_WINTERMELON, SEED_MELONPULT) || 
		!CheckSeedUpgrade(SEED_TWINSUNFLOWER, SEED_SUNFLOWER) || 
		!CheckSeedUpgrade(SEED_SPIKEROCK, SEED_SPIKEWEED) || 
		!CheckSeedUpgrade(SEED_COBCANNON, SEED_KERNELPULT) || 
		!CheckSeedUpgrade(SEED_GOLD_MAGNET, SEED_MAGNETSHROOM) || 
		!CheckSeedUpgrade(SEED_GLOOMSHROOM, SEED_FUMESHROOM) || 
		!CheckSeedUpgrade(SEED_CATTAIL, SEED_LILYPAD))
		return;

	if (mCoopChooserActive && mApp->mNetManager && mApp->mNetManager->IsHosting())
	{
		CoopBroadcastChooserStart();
		CoopBuildBankFromLocalState();
		mBoard->mCutScene->EndSeedChooser();
		return;
	}

	CloseSeedChooser();
}

//0x4859B0
void SeedChooserScreen::PickRandomSeeds()
{
	// The host owns the final chooser result.  In particular, a client must not
	// use the debug/random shortcut to replace its local mirror of mandatory
	// cards and close the chooser before the authoritative start arrives.
	if (mApp->NetIsClient())
		return;

	for (int anIndex = mSeedsInBank; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType;
		do aSeedType = (SeedType)Rand(mApp->GetSeedsAvailable());
		while (!mApp->SeedTypeAvailable(aSeedType) || aSeedType == SEED_IMITATER || mChosenSeeds[aSeedType].mSeedState != SEED_IN_CHOOSER);
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		aChosenSeed.mTimeStartMotion = 0;
		aChosenSeed.mTimeEndMotion = 0;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		GetSeedPositionInBank(anIndex, aChosenSeed.mEndX, aChosenSeed.mEndY);
		aChosenSeed.mSeedState = SEED_IN_BANK;
		aChosenSeed.mSeedIndexInBank = anIndex;
		mSeedsInBank++;
	}
	for (SeedType aSeedFlying = SEED_PEASHOOTER; aSeedFlying < NUM_SEEDS_IN_CHOOSER; aSeedFlying = (SeedType)(aSeedFlying + 1))
		LandFlyingSeed(mChosenSeeds[aSeedFlying]);
	CloseSeedChooser();
}

//0x485BB0
void SeedChooserScreen::ButtonDepress(int theId)
{
	if (mSeedsInFlight > 0 || mChooseState == CHOOSE_VIEW_LAWN || !mMouseVisible)
		return;

	if (theId == SeedChooserScreen::SeedChooserScreen_ViewLawn)
	{
		mChooseState = CHOOSE_VIEW_LAWN;
		mMenuButton->mDisabled = true;
		mViewLawnTime = 0;
	}
	else if (theId == SeedChooserScreen::SeedChooserScreen_Almanac)
	{
		mApp->DoAlmanacDialog()->WaitForResult(true);
		mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
	}
	else if (theId == SeedChooserScreen::SeedChooserScreen_Store)
	{
		StoreScreen* aStore = mApp->ShowStoreScreen();
		aStore->mBackButton->SetLabel(_S("[STORE_BACK_TO_GAME]"));
		aStore->WaitForResult();
		if (aStore->mGoToTreeNow)
		{
			mApp->KillBoard();
			mApp->PreNewGame(GAMEMODE_TREE_OF_WISDOM, false);
		}
		else mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
	}
	else if (theId == SeedChooserScreen::SeedChooserScreen_Menu)
	{
		mMenuButton->mIsOver = false;
		mMenuButton->mIsDown = false;
		UpdateCursor();
		mApp->DoNewOptions(false);
	}
	else if (mCoopChooserActive && theId == SeedChooserScreen::SeedChooserScreen_Start)
	{
		CoopToggleReady();
	}
	else if (mApp->NetIsClient() && (theId == SeedChooserScreen::SeedChooserScreen_Start || theId == SeedChooserScreen::SeedChooserScreen_Random))
	{
		// co-op client: only the host starts the game
		mApp->PlaySample(Sexy::SOUND_BUZZER);
	}
	else if (mApp->GetSeedsAvailable() >= mBoard->mSeedBank->mNumPackets)
	{
		if (theId == SeedChooserScreen::SeedChooserScreen_Start)
			OnStartButton();
		else if (theId == SeedChooserScreen::SeedChooserScreen_Random)
			PickRandomSeeds();
	}
}

//0x485D80
SeedType SeedChooserScreen::SeedHitTest(int x, int y)
{
	x -= mApp->mDDInterface->mWideScreenOffsetX;
	y -= mApp->mDDInterface->mWideScreenOffsetY;

	if (mMouseVisible)
	{
		for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (!mApp->SeedTypeAvailable(aSeedType) || aChosenSeed.mSeedState == SEED_PACKET_HIDDEN) continue;

			Rect aRect = Rect(aChosenSeed.mX, aChosenSeed.mY, SEED_PACKET_WIDTH, SEED_PACKET_HEIGHT);
			if (aChosenSeed.mSeedState == SEED_IN_CHOOSER)	
			{
				aRect.mY -= mScrollbar->mScrollValue;

				const bool expandedRows = Has7Rows() || mAllowBetaSeedpackets;

				int offsetY = expandedRows ? 0 : 40;
				int startY = 123 + offsetY;

				if (y < startY || y > 420 + startY)
					continue;
			}
			if (aRect.Contains(x, y)) return aSeedType;
		}
	}
	return SEED_NONE;
}

//0x485E20
SeedType SeedChooserScreen::FindSeedInBank(int theIndexInBank)
{
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (aChosenSeed.mSeedState == SEED_IN_BANK && aChosenSeed.mSeedIndexInBank == theIndexInBank) return aSeedType;
		}
	}
	return SEED_NONE;
}

void SeedChooserScreen::EnableStartButton(bool theEnabled)
{
	mStartButton->SetDisabled(!theEnabled);
	if (theEnabled) mStartButton->mColors[GameButton::COLOR_LABEL] = Color::White;
	else mStartButton->mColors[GameButton::COLOR_LABEL] = Color(64, 64, 64);
}

int SeedChooserScreen::CoopCountGlobalFixedPicks() const
{
	int aCount = 0;
	for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
	{
		const ChosenSeed& aSeed = mChosenSeeds[i];
		if (aSeed.mCrazyDavePicked && aSeed.mSeedState == SEED_IN_BANK)
			aCount++;
	}
	return aCount;
}

int SeedChooserScreen::CoopExpectedGlobalFixedPicks(LawnApp* theApp, Board* theBoard)
{
	if (!theApp || !theBoard)
		return 0;
	if (theApp->mGameMode == GAMEMODE_CHALLENGE_SEEING_STARS ||
		theApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT)
		return 1;
	if (theApp->mGameMode == GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER)
		return 3;
	if (theApp->IsAdventureMode() && (!theApp->IsFirstTimeAdventureMode() ||
		theApp->mPlayerLevelRef > 4 && theBoard->mLevel < theApp->mPlayerLevelRef))
		return 3;
	return 0;
}

int SeedChooserScreen::CoopGetLocalQuotaPickCount() const
{
	int aCount = mSeedsInBank;
	// Every chooser mirrors fixed cards for presentation, but only the host
	// serializes/owns them.  Client quotas therefore count mutable picks only.
	if (mApp && mApp->NetIsClient())
		aCount -= CoopCountGlobalFixedPicks();
	return max(0, aCount);
}

bool SeedChooserScreen::CoopAllPicksComplete() const
{
    const NetManager* aNet = mApp ? mApp->mNetManager : nullptr;
    if (!aNet || !mCoopChooserActive)
        return false;
    int aTotal = 0;
    for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
    {
        const NetPlayerInfo& aPlayer = aNet->GetPlayer(aSlot);
        if (!aPlayer.mActive || aPlayer.mSpectator ||
            (mApp->mGameMode == GAMEMODE_COOP_PVP && aPlayer.mTeam != NETTEAM_PLANTS))
            continue;
        const int aCount = aSlot == aNet->GetMySlot() ?
            CoopGetLocalQuotaPickCount() : mCoopRemotePickCount[aSlot];
        if (aCount != aNet->mChooserPickQuota[aSlot])
            return false;
        aTotal += aCount;
    }
    return aTotal == aNet->mChooserBankCount && aTotal > 0;
}

void SeedChooserScreen::CoopToggleReady()
{
    NetManager* aNet = mApp ? mApp->mNetManager : nullptr;
    if (!aNet || !mCoopChooserActive || aNet->IsSpectator())
        return;
    if (CoopGetLocalQuotaPickCount() != mCoopPicksPerPlayer)
    {
        mApp->PlaySample(SOUND_BUZZER);
        return;
    }
    aNet->SetMyReady(!aNet->IsMyReady());
    CoopRefreshStartButton();
}

void SeedChooserScreen::CoopRefreshStartButton()
{
    NetManager* aNet = mApp ? mApp->mNetManager : nullptr;
    if (!mCoopChooserActive || !aNet || !aNet->IsActive())
        return;
    const bool aCanReady = !aNet->IsSpectator() &&
        CoopGetLocalQuotaPickCount() == mCoopPicksPerPlayer;
    EnableStartButton(aCanReady);
    const bool aReady = aNet->IsMyReady();
    mStartButton->SetLabel(StrFormat(_S("%d/%d ready"),
        aNet->GetReadyCount(), aNet->GetReadyEligibleCount()));
    mStartButton->SetFont(FONT_BRIANNETOD12);
    const Color aColor = aReady ? Color(115, 255, 110) : Color(255, 110, 95);
    mStartButton->mColors[GameButton::COLOR_LABEL] = aColor;
    mStartButton->mColors[GameButton::COLOR_LABEL_HILITE] = aColor;
}

//0x485E90
void SeedChooserScreen::ClickedSeedInBank(ChosenSeed& theChosenSeed)
{
	if (mApp->NetIsClient() && !mCoopChooserActive)
	{
		mApp->PlaySample(Sexy::SOUND_BUZZER);
		return;
	}

	// Keep this guard at the mutation boundary as well as in MouseDown.  It
	// protects fixed cards from future click/drag paths and from direct calls.
	if (theChosenSeed.mCrazyDavePicked)
		return;

	if (mApp->mGameMode == GAMEMODE_COOP_PVP && mApp->mNetManager &&
		mApp->mNetManager->GetMyTeam() == NETTEAM_ZOMBIES)
		return;

	for (int anIndex = theChosenSeed.mSeedIndexInBank + 1; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType = FindSeedInBank(anIndex);
		if (aSeedType != SEED_NONE)
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			aChosenSeed.mTimeStartMotion = mSeedChooserAge;
			aChosenSeed.mTimeEndMotion = mSeedChooserAge + 15;
			aChosenSeed.mStartX = aChosenSeed.mX;
			aChosenSeed.mStartY = aChosenSeed.mY;
			GetSeedPositionInBank(anIndex - 1, aChosenSeed.mEndX, aChosenSeed.mEndY);
			aChosenSeed.mSeedState = SEED_SHIFTING_IN_BANK;
			aChosenSeed.mSeedIndexInBank = anIndex - 1;
			mSeedsInFlight++;
		}
	}
	theChosenSeed.mTimeStartMotion = mSeedChooserAge;
	theChosenSeed.mTimeEndMotion = mSeedChooserAge + 25;
	theChosenSeed.mStartX = theChosenSeed.mX;
	theChosenSeed.mStartY = theChosenSeed.mY;
	GetSeedPositionInChooser(theChosenSeed.mSeedType, theChosenSeed.mEndX, theChosenSeed.mEndY);
	theChosenSeed.mSeedState = SEED_FLYING_TO_CHOOSER;
	theChosenSeed.mSeedIndexInBank = 0;
	mSeedsInBank--;
	mSeedsInFlight++;
	RemoveToolTip();
	mApp->PlaySample(Sexy::SOUND_TAP);
	CoopSendPick(theChosenSeed.mSeedType, SEED_NONE, true);
	if (mCoopChooserActive)
		CoopRefreshStartButton();
	else
		EnableStartButton(false);
}

//0x486030
void SeedChooserScreen::ClickedSeedInChooser(ChosenSeed& theChosenSeed)
{
	if (mApp->NetIsClient() && !mCoopChooserActive)
	{
		mApp->PlaySample(Sexy::SOUND_BUZZER);
		return;
	}

	if (mApp->mGameMode == GAMEMODE_COOP_PVP && mApp->mNetManager &&
		mApp->mNetManager->GetMyTeam() == NETTEAM_ZOMBIES)
	{
		mApp->PlaySample(Sexy::SOUND_BUZZER);
		return;
	}

	// Co-op quota zero is a real quota, not a sentinel for disabled co-op.
	if (mCoopChooserActive)
	{
		if (CoopGetLocalQuotaPickCount() >= mCoopPicksPerPlayer)
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
		if (CoopIsPickedByOthers(theChosenSeed.mSeedType))
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}

	if (mSeedsInBank == mBoard->mSeedBank->mNumPackets)
		return;

	theChosenSeed.mTimeStartMotion = mSeedChooserAge;
	theChosenSeed.mTimeEndMotion = mSeedChooserAge + 25;
	theChosenSeed.mStartX = theChosenSeed.mX;
	theChosenSeed.mStartY = theChosenSeed.mY;
	if (theChosenSeed.mSeedType != SeedType::SEED_IMITATER)
		theChosenSeed.mStartY -= mScrollbar->mScrollValue;
	GetSeedPositionInBank(mSeedsInBank, theChosenSeed.mEndX, theChosenSeed.mEndY);
	theChosenSeed.mSeedState = SEED_FLYING_TO_BANK;
	theChosenSeed.mSeedIndexInBank = mSeedsInBank;
	mSeedsInFlight++;
	mSeedsInBank++;

	RemoveToolTip();
	mApp->PlaySample(Sexy::SOUND_TAP);
	CoopSendPick(theChosenSeed.mSeedType, theChosenSeed.mSeedType == SEED_IMITATER ? theChosenSeed.mImitaterType : SEED_NONE, false);
	if (mCoopChooserActive)
		CoopRefreshStartButton();
	else if (mSeedsInBank == mBoard->mSeedBank->mNumPackets)
		EnableStartButton(true);
}

//0x486150
void SeedChooserScreen::ShowToolTip()
{
	if (!mApp->mWidgetManager->mMouseIn || !mApp->mActive || mApp->GetDialogCount() > 0 || mChooseState == CHOOSE_VIEW_LAWN)
	{
		RemoveToolTip();
	}
	else if (mSeedsInFlight <= 0)
	{
		if (mImitaterButton->IsMouseOver() && mMouseVisible)
		{
			mToolTip->SetLabel(Plant::GetToolTip(SEED_IMITATER));
			mToolTip->SetTitle(Plant::GetNameString(SEED_IMITATER));
			mToolTip->SetWarningText(_S(""));
			mToolTip->mX = (SEED_PACKET_WIDTH - mToolTip->mWidth) / 2 + mImitaterButton->mX;
			mToolTip->mY = mImitaterButton->mY - mToolTip->mHeight;
			mToolTip->mVisible = true;
		}
		else
		{
			SeedType aSeedType = SeedHitTest(mLastMouseX, mLastMouseY);
			if (aSeedType == SEED_NONE)
			{
				RemoveToolTip();
			}
			else if (aSeedType != mToolTipSeed)
			{
				RemoveToolTip();
				ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
				uint aRecFlags = SeedNotRecommendedToPick(aSeedType);
				if (SeedNotAllowedToPick(aSeedType))
				{
					mToolTip->SetWarningText(_S("[NOT_ALLOWED_ON_THIS_LEVEL]"));
				}
				else if (SeedNotAllowedDuringTrial(aSeedType))
				{
					mToolTip->SetWarningText(_S("[FULL_VERSION_ONLY]"));
				}
				else if (aChosenSeed.mSeedState == SEED_IN_BANK && aChosenSeed.mCrazyDavePicked)
				{
					mToolTip->SetWarningText(_S("[CRAZY_DAVE_WANTS]"));
				}
				else if (aRecFlags != 0U)
				{
					if (TestBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL))
					{
						mToolTip->SetWarningText(_S("[NOCTURNAL_WARNING]"));
					}
					else
					{
						mToolTip->SetWarningText(_S("[NOT_RECOMMENDED_FOR_LEVEL]"));
					}
				}
				else
				{
					mToolTip->SetWarningText(_S(""));
				}

				if (aSeedType == SEED_IMITATER)
				{
					mToolTip->SetTitle(Plant::GetNameString(aSeedType, aChosenSeed.mImitaterType));
					mToolTip->SetLabel(Plant::GetToolTip(aChosenSeed.mImitaterType));
				}
				else
				{
					mToolTip->SetTitle(Plant::GetNameString(aSeedType, SEED_NONE));
					mToolTip->SetLabel(Plant::GetToolTip(aSeedType));
				}

				int aSeedX, aSeedY;
				if (aChosenSeed.mSeedState == SEED_IN_BANK)
				{
					GetSeedPositionInBank(aChosenSeed.mSeedIndexInBank, aSeedX, aSeedY);
				}
				else
				{
					GetSeedPositionInChooser(aSeedType, aSeedX, aSeedY);
					aSeedY -= mScrollbar->mScrollValue;
				}

				mToolTip->mX = ClampInt((SEED_PACKET_WIDTH - mToolTip->mWidth) / 2 + aSeedX, 0, mApp->mWidth - mToolTip->mWidth);
				mToolTip->mY = aSeedY + 70;
				mToolTip->mVisible = true;
				mToolTipSeed = aSeedType;
			}
		}
	}
}

void SeedChooserScreen::RemoveToolTip()
{
	mToolTip->mVisible = false;
	mToolTipSeed = SEED_NONE;
}

//0x486600
void SeedChooserScreen::CancelLawnView()
{
	if (mChooseState == CHOOSE_VIEW_LAWN && mViewLawnTime > 100 && mViewLawnTime <= 250) mViewLawnTime = 251;
}

//0x486630
void SeedChooserScreen::MouseUp(int x, int y, int theClickCount)
{
	Widget::MouseUp(x, y);

	if (theClickCount == 1)
	{
		if (mMenuButton->IsMouseOver()) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Menu);
		else if (mStartButton->IsMouseOver()) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Start);
		else if (mAlmanacButton->IsMouseOver()) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Almanac);
		else if (mStoreButton->IsMouseOver()) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Store);
	}
}

void SeedChooserScreen::MouseMove(int theX, int theY)
{
	Widget::MouseMove(theX, theY);
}

//0x4866E0
void SeedChooserScreen::UpdateImitaterButton()
{
	if (!mApp->SeedTypeAvailable(SEED_IMITATER))
	{
		mImitaterButton->mBtnNoDraw = true;
		mImitaterButton->mDisabled = true;
	}
	else
	{
		mImitaterButton->mBtnNoDraw = false;
		mImitaterButton->mDisabled = mChosenSeeds[SEED_IMITATER].mSeedState != SEED_PACKET_HIDDEN;
	}
}

//0x486770
void SeedChooserScreen::MouseDown(int x, int y, int theClickCount)
{
	Widget::MouseDown(x, y, theClickCount);

	if (mSeedsInFlight > 0)
	{
		for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
		{
			LandFlyingSeed(mChosenSeeds[i]);
		}
	}

	if (mChooseState == CHOOSE_VIEW_LAWN)
	{
		CancelLawnView();
	}
	else if (mRandomButton->IsMouseOver())
	{
		mApp->PlaySample(Sexy::SOUND_TAP);
		ButtonDepress(SeedChooserScreen::SeedChooserScreen_Random);
	}
	else if (mViewLawnButton->IsMouseOver())
	{
		mApp->PlaySample(Sexy::SOUND_TAP);
		ButtonDepress(SeedChooserScreen::SeedChooserScreen_ViewLawn);
	}
	else if (mMenuButton->IsMouseOver())
	{
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
	}
	else if (mStartButton->IsMouseOver() || mAlmanacButton->IsMouseOver() || mStoreButton->IsMouseOver())
	{
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
	else if (mScrollbar->isThumbOver())
	{
		mScrollbar->MouseDown(x, y, theClickCount);
	}
	else if (mImitaterButton->IsMouseOver())
	{
		if (mApp->NetIsClient() && !mCoopChooserActive)
		{
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
		if (mSeedsInBank != mBoard->mSeedBank->mNumPackets)
		{
			mApp->PlaySample(Sexy::SOUND_TAP);
			ImitaterDialog* aDialog = new ImitaterDialog();
			mApp->AddDialog(aDialog->mId, aDialog);
			aDialog->Resize((mWidth - aDialog->mWidth) / 2, (mHeight - aDialog->mHeight) / 2, aDialog->mWidth, aDialog->mHeight);
			mApp->mWidgetManager->SetFocus(aDialog);
		}
	}
	else
	{
		if (!mBoard->mSeedBank->ContainsPoint(x, y) && !mAlmanacButton->IsMouseOver() && !mStoreButton->IsMouseOver() && mApp->CanShowAlmanac())
		{
			Zombie* aZombie = mBoard->ZombieHitTest(x - mBoard->mX, y - mBoard->mY);
			if (aZombie && aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE && aZombie->mZombieType < ZOMBIE_REDEYE_GARGANTUAR 
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
				&& aZombie->mZombieType != ZombieType::ZOMBIE_DOG_WALKER && aZombie->mZombieType != ZombieType::ZOMBIE_DOG && aZombie->mZombieType != ZombieType::ZOMBIE_PROPELLER
#endif
				)
			{
				mApp->PlaySample(SOUND_TAP);
				mApp->DoAlmanacDialog(SEED_NONE, aZombie->mZombieType)->WaitForResult(true);
				mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
				return;
			}
		}

		x += mApp->mDDInterface->mWideScreenOffsetX;
		y += mApp->mDDInterface->mWideScreenOffsetY;

		SeedType aSeedType = SeedHitTest(x, y);
		if (aSeedType != SEED_NONE && !SeedNotAllowedToPick(aSeedType))
		{
			if (SeedNotAllowedDuringTrial(aSeedType))
			{
				mApp->PlaySample(Sexy::SOUND_TAP);
				if (mApp->LawnMessageBox(
					DIALOG_MESSAGE, 
					_S("[GET_FULL_VERSION_TITLE]"), 
					_S("[GET_FULL_VERSION_BODY]"), 
					_S("[GET_FULL_VERSION_YES_BUTTON]"),
					_S("[GET_FULL_VERSION_NO_BUTTON]"), 
					Dialog::BUTTONS_YES_NO
				) == Dialog::ID_YES)
				{
					if (mApp->mDRM)
					{
						mApp->mDRM->BuyGame();
					}
					mApp->DoBackToMain();
				}
			}
			else
			{
				ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
				if (aChosenSeed.mSeedState == SEED_IN_BANK)
				{
					if (aChosenSeed.mCrazyDavePicked)
					{
						mApp->PlaySample(Sexy::SOUND_BUZZER);
						mToolTip->FlashWarning();
					}
					else ClickedSeedInBank(aChosenSeed);
				}
				else if (aChosenSeed.mSeedState == SEED_IN_CHOOSER)
				{
					if (theClickCount == 1)
					{
						ClickedSeedInChooser(aChosenSeed);
					}
					else if (theClickCount == -1)
					{
						mApp->PlaySample(SOUND_TAP);
						mApp->DoAlmanacDialog(aSeedType)->WaitForResult(true);
						mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
					}
				}
			}
		}
	}
}

//0x486CE0
bool SeedChooserScreen::PickedPlantType(SeedType theSeedType)
{
    if (mCoopChooserActive)
        for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
            for (int j = 0; j < mCoopRemotePickCount[aSlot] && j < 12; j++)
                if (mCoopRemotePicks[aSlot][j] == theSeedType ||
                    (mCoopRemotePicks[aSlot][j] == SEED_IMITATER &&
                     mCoopRemoteImitaters[aSlot][j] == theSeedType))
                    return true;

	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		if (aChosenSeed.mSeedState == SEED_IN_BANK)
		{
			if (aChosenSeed.mSeedType == theSeedType || (aChosenSeed.mSeedType == SEED_IMITATER && aChosenSeed.mImitaterType == theSeedType))
			{
				return true;
			}
		}
	}
	return false;
}

//0x486D20
void SeedChooserScreen::CloseSeedChooser()
{
	DBG_ASSERT(mBoard->mSeedBank->mNumPackets == mBoard->GetNumSeedsInBank());
	for (int anIndex = 0; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType = FindSeedInBank(anIndex);
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		SeedPacket& aSeedPacket = mBoard->mSeedBank->mSeedPackets[anIndex];
		aSeedPacket.SetPacketType(aSeedType, aChosenSeed.mImitaterType);
		if (aChosenSeed.mRefreshing)
		{
			aSeedPacket.mRefreshCounter = aChosenSeed.mRefreshCounter;
			aSeedPacket.mRefreshTime = Plant::GetRefreshTime(aSeedPacket.mPacketType, aSeedPacket.mImitaterType);
			aSeedPacket.mRefreshing = true;
			aSeedPacket.mActive = false;
		}
	}

	mStartButton->SetDisabled(true);
	mImitaterButton->SetDisabled(true);
	mStoreButton->SetDisabled(true);
	mAlmanacButton->SetDisabled(true);
	mMenuButton->SetDisabled(true);
	mRandomButton->SetDisabled(true);
	mBoard->mCutScene->EndSeedChooser();
}

//0x486E80
void SeedChooserScreen::KeyDown(KeyCode theKey)
{
	mBoard->DoTypingCheck(theKey);

	if (theKey == KeyCode::KEYCODE_ESCAPE && !mMenuButton->mDisabled && mBoard->mCutScene->mCutsceneTime >= 4250 + mBoard->mCutScene->mCrazyDaveTime)
	{
		mApp->DoNewOptions(false);
	}
}

//0x486EA0
void SeedChooserScreen::KeyChar(SexyChar theChar)
{
	if (mChooseState == CHOOSE_VIEW_LAWN && mViewLawnTime > 251 && (theChar == ' ' || theChar == '\r' || theChar == '\u001B'))
		CancelLawnView();
	else if (mApp->mTodCheatKeys && theChar == '\u001B' && mBoard->mCutScene->mSeedChoosing)
		PickRandomSeeds();
	else mBoard->KeyChar(theChar);
}

//0x486F10
void SeedChooserScreen::UpdateAfterPurchase()
{
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		if (aChosenSeed.mSeedState == SEED_IN_BANK)
			GetSeedPositionInBank(aChosenSeed.mSeedIndexInBank, aChosenSeed.mX, aChosenSeed.mY);
		else if (aChosenSeed.mSeedState == SEED_IN_CHOOSER)
			GetSeedPositionInChooser(aSeedType, aChosenSeed.mX, aChosenSeed.mY);
		else continue;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		aChosenSeed.mEndX = aChosenSeed.mX;
		aChosenSeed.mEndY = aChosenSeed.mY;
	}
	EnableStartButton(mSeedsInBank == mBoard->mSeedBank->mNumPackets);
	UpdateImitaterButton();
}

void SeedChooserScreen::MouseWheel(int theDelta)
{
	Widget::MouseWheel(theDelta);

	mScrollAmount -= theDelta;
	mScrollAmount -= mScrollAmount * 0.1f;
}

void SeedChooserScreen::AddedToManager(WidgetManager* theWidgetManager) {
	Widget::AddedToManager(theWidgetManager);
	this->AddWidget(mScrollbar);
}

void SeedChooserScreen::RemovedFromManager(WidgetManager* theWidgetManager) {
	Widget::RemovedFromManager(theWidgetManager);
	this->RemoveWidget(mScrollbar);
}
// ==========================================================================
// Co-op shared seed picking
// ==========================================================================

bool SeedChooserScreen::CoopIsPickedByOthers(int theSeedType) const
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < mCoopRemotePickCount[i] && j < 12; j++)
			if (mCoopRemotePicks[i][j] == theSeedType)
				return true;
	}
	return false;
}

bool SeedChooserScreen::CoopRecordRemotePick(int theSlot, int theSeedType, int theImitater, bool theUnpick)
{
	if (theSlot < 0 || theSlot >= 4)
		return false;
	if (theUnpick)
	{
		for (int j = 0; j < mCoopRemotePickCount[theSlot] && j < 12; j++)
		{
			if (mCoopRemotePicks[theSlot][j] == theSeedType)
			{
				// shift the rest left
				for (int k = j; k < mCoopRemotePickCount[theSlot] - 1; k++)
				{
					mCoopRemotePicks[theSlot][k] = mCoopRemotePicks[theSlot][k + 1];
					mCoopRemoteImitaters[theSlot][k] = mCoopRemoteImitaters[theSlot][k + 1];
				}
				mCoopRemotePicks[theSlot][mCoopRemotePickCount[theSlot] - 1] = SEED_NONE;
				mCoopRemoteImitaters[theSlot][mCoopRemotePickCount[theSlot] - 1] = SEED_NONE;
				mCoopRemotePickCount[theSlot]--;
				if (mApp->mNetManager && mApp->mNetManager->IsHosting())
					mApp->mNetManager->ResetReadyState();
				return true;
			}
		}
		return false;
	}
	else
	{
		if (mCoopRemotePickCount[theSlot] < 12)
		{
			const int anIndex = mCoopRemotePickCount[theSlot]++;
			mCoopRemotePicks[theSlot][anIndex] = theSeedType;
			mCoopRemoteImitaters[theSlot][anIndex] = theSeedType == SEED_IMITATER ? theImitater : SEED_NONE;
			if (mApp->mNetManager && mApp->mNetManager->IsHosting())
				mApp->mNetManager->ResetReadyState();
			return true;
		}
	}
	return false;
}

void SeedChooserScreen::CoopSendPick(int theSeedType, int theImitater, bool theUnpick)
{
	LawnNet::NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsActive())
		return;

	if (aNet->IsClient())
	{
		aNet->SetMyReady(false);
		aNet->SendSeedPick(theUnpick, theSeedType, theImitater);
	}
	else if (aNet->IsHosting())
	{
		int aMySlot = aNet->GetMySlot();
		CoopRecordRemotePick(aMySlot, theSeedType, theImitater, theUnpick);
		CoopBroadcastChooserState();
	}
}

void SeedChooserScreen::CoopBroadcastChooserState(bool theForce)
{
	LawnNet::NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsHosting() || !mApp->mSeedChooserScreen)
		return;

	LawnNet::NetBuffer aMsg;
	aMsg.WriteU8(4); // slots
	for (int i = 0; i < 4; i++)
	{
		SeedType aSeeds[12];
		SeedType anImitaters[12];
		int aValidCount = 0;
		if (i == aNet->GetMySlot())
		{
			// our own picks, in bank order
			for (int anIndex = 0; anIndex < mBoard->mSeedBank->mNumPackets && anIndex < mSeedsInBank; anIndex++)
			{
				SeedType aSeedType = FindSeedInBank(anIndex);
				if ((int)aSeedType >= 0 && aSeedType < SeedType::NUM_SEED_TYPES && aValidCount < 12)
				{
					aSeeds[aValidCount] = aSeedType;
					anImitaters[aValidCount] = aSeedType == SEED_IMITATER ? mChosenSeeds[aSeedType].mImitaterType : SEED_NONE;
					aValidCount++;
				}
			}
		}
		else
		{
			for (int j = 0; j < mCoopRemotePickCount[i] && j < 12; j++)
			{
				int aSeed = mCoopRemotePicks[i][j];
				if (aSeed >= 0 && aSeed < (int)SeedType::NUM_SEED_TYPES)
				{
					aSeeds[aValidCount] = (SeedType)aSeed;
					anImitaters[aValidCount] = (SeedType)mCoopRemoteImitaters[i][j];
					aValidCount++;
				}
			}
		}
		aMsg.WriteU8((uint8_t)aValidCount);
		for (int j = 0; j < aValidCount; j++)
		{
			aMsg.WriteU8((uint8_t)aSeeds[j]);
			aMsg.WriteU8((uint8_t)anImitaters[j]);
		}
	}
	// CoopPollNetwork runs every chooser frame. Send only when the serialized
	// state actually changes; the old unconditional broadcast produced hundreds
	// of duplicate packets per second and eventually overflowed client queues.
	unsigned int aHash = 2166136261u;
	for (uint8_t aByte : aMsg.mData)
		aHash = (aHash ^ aByte) * 16777619u;
	if (!theForce && aHash == mCoopLastStateHash)
		return;
	if (theForce)
		LawnNet::NetTrace("chooser forced authoritative state echo hash=%u", aHash);
	mCoopLastStateHash = aHash;
	aNet->BroadcastMessage(LawnNet::NETMSG_CHOOSER_STATE, aMsg);
}

void SeedChooserScreen::CoopDrawRemotePicks(Graphics* g)
{
    NetManager* aNet = mApp->mNetManager;
    if (!aNet || !aNet->IsActive() || !mCoopChooserActive)
        return;
    int anIndex = mSeedsInBank;
    for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
    {
        if (aSlot == aNet->GetMySlot())
            continue;
        for (int j = 0; j < mCoopRemotePickCount[aSlot] && j < 12; j++)
        {
            const SeedType aSeed = (SeedType)mCoopRemotePicks[aSlot][j];
            if (aSeed < SEED_PEASHOOTER || aSeed >= NUM_SEEDS_IN_CHOOSER ||
                mChosenSeeds[aSeed].mCrazyDavePicked || anIndex >= mBoard->mSeedBank->mNumPackets)
                continue;
            int x, y;
            GetSeedPositionInBank(anIndex++, x, y);
            const SeedType anImitater = aSeed == SEED_IMITATER ?
                (SeedType)mCoopRemoteImitaters[aSlot][j] : SEED_NONE;
            const NetColor aColor = GetRelativePlayerColor(aNet->GetMySlot(), aSlot);
            Graphics aPacketGraphics(*g);
            aPacketGraphics.SetColorizeImages(true);
            aPacketGraphics.SetColor(Color(aColor.r, aColor.g, aColor.b));
            DrawSeedPacket(&aPacketGraphics, (float)x, (float)y, aSeed, anImitater, 0, 255, true, false);
        }
    }
}

// --------------------------------------------------------------------------
// Host: broadcast the final combined pick list and start the game
// --------------------------------------------------------------------------
void SeedChooserScreen::CoopBroadcastChooserStart()
{
	LawnNet::NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsHosting())
		return;

	LawnNet::NetBuffer aMsg;
	aMsg.WriteU8((uint8_t)mCoopPicksPerPlayer);
	aMsg.WriteU8(4);
	for (int i = 0; i < 4; i++)
	{
		SeedType aSeeds[12];
		SeedType anImitaters[12];
		int aValidCount = 0;
		if (i == aNet->GetMySlot())
		{
			for (int anIndex = 0; anIndex < mBoard->mSeedBank->mNumPackets && anIndex < mSeedsInBank; anIndex++)
			{
				SeedType aSeedType = FindSeedInBank(anIndex);
				if ((int)aSeedType >= 0 && aSeedType < SeedType::NUM_SEED_TYPES && aValidCount < 12)
				{
					aSeeds[aValidCount] = aSeedType;
					anImitaters[aValidCount] = aSeedType == SEED_IMITATER ? mChosenSeeds[aSeedType].mImitaterType : SEED_NONE;
					aValidCount++;
				}
			}
		}
		else
		{
			for (int j = 0; j < mCoopRemotePickCount[i] && j < 12; j++)
			{
				int aSeed = mCoopRemotePicks[i][j];
				if (aSeed >= 0 && aSeed < (int)SeedType::NUM_SEED_TYPES)
				{
					aSeeds[aValidCount] = (SeedType)aSeed;
					anImitaters[aValidCount] = (SeedType)mCoopRemoteImitaters[i][j];
					aValidCount++;
				}
			}
		}
		aMsg.WriteU8((uint8_t)aValidCount);
		for (int j = 0; j < aValidCount; j++)
		{
			aMsg.WriteU8((uint8_t)aSeeds[j]);
			aMsg.WriteU8((uint8_t)anImitaters[j]);
		}
	}
	aNet->BroadcastMessage(LawnNet::NETMSG_CHOOSER_START, aMsg);
}

// --------------------------------------------------------------------------
// Build the in-game seed bank: my picks first, then other players' (in
// viewer-relative slot order), each packet tagged with its owner's slot.
// --------------------------------------------------------------------------
static void CoopBuildBankInternal(SeedBank* theBank, int theMySlot,
	const int thePicks[4][12], const int theCounts[4], const SeedType theImitaters[4][12])
{
	LawnNet::NetManager* aNet = gLawnApp ? gLawnApp->mNetManager : nullptr;
	const bool aPvP = gLawnApp && gLawnApp->mGameMode == GameMode::GAMEMODE_COOP_PVP;
	int aMaxPlantPackets = SEEDBANK_MAX;
	if (aPvP)
		aMaxPlantPackets = PVP_PLANT_PACKET_COUNT;

	int aTotal = 0;
	for (int i = 0; i < 4; i++)
	{
		if (aPvP && aNet && aNet->GetPlayer(i).mTeam != NETTEAM_PLANTS)
			continue;
		aTotal += min(12, theCounts[i]);
	}
	aTotal = min(aTotal, aMaxPlantPackets);

	int aPos = 0;
	for (int anOffset = 0; anOffset < 4; anOffset++)
	{
		int aSlot = (theMySlot + anOffset) % 4;
		if (aPvP && aNet && aNet->GetPlayer(aSlot).mTeam != NETTEAM_PLANTS)
			continue;
		for (int j = 0; j < theCounts[aSlot] && j < 12 && aPos < aTotal; j++)
		{
			SeedType aSeedType = (SeedType)thePicks[aSlot][j];
			if (aSeedType == SEED_NONE)
				continue;
			SeedPacket& aPacket = theBank->mSeedPackets[aPos];
			// Packet IDs belong to card identity, not to a viewer-relative index.
			if (aPacket.mPacketType != aSeedType || aPacket.mImitaterType != theImitaters[aSlot][j] ||
				aPacket.mNetOwnerSlot != (unsigned char)aSlot)
				aPacket.mNetId = 0;
			aPacket.SetPacketType(aSeedType, theImitaters[aSlot][j]);
			aPacket.mNetOwnerSlot = (unsigned char)aSlot;
			aPos++;
		}
	}
	if (aPvP)
	{
		// Plant slots remain a fixed first row even if somebody selected fewer
		// cards; the complete zombie-team palette starts after all ten slots.
		while (aPos < PVP_PLANT_PACKET_COUNT)
		{
			SeedPacket& aPacket = theBank->mSeedPackets[aPos++];
			aPacket.mNetId = 0;
			aPacket.mNetOwnerSlot = LawnNet::NET_SHARED_OWNER;
			aPacket.SetPacketType(SEED_NONE, SEED_NONE);
		}
		for (int i = 0; i < PVP_ZOMBIE_PACKET_COUNT; i++)
		{
			SeedPacket& aPacket = theBank->mSeedPackets[aPos++];
			aPacket.mNetId = 0; // AssignPvPSeedOwners sets the final team ownership below.
			aPacket.SetPacketType(Challenge::GetPvPZombieSeed(i));
		}
	}
	theBank->mNumPackets = max(aPos, 1);
	if (gLawnApp && gLawnApp->mBoard && gLawnApp->mGameMode == GameMode::GAMEMODE_COOP_PVP)
		gLawnApp->mBoard->AssignPvPSeedOwners();
	for (int i = aPos; i < SEEDBANK_CAPACITY; i++)
	{
		theBank->mSeedPackets[i].mNetId = 0;
		theBank->mSeedPackets[i].mNetOwnerSlot = LawnNet::NET_SHARED_OWNER;
		theBank->mSeedPackets[i].SetPacketType(SEED_NONE, SEED_NONE);
	}
}

void SeedChooserScreen::CoopBuildBankFromLocalState()
{
	LawnNet::NetManager* aNet = mApp->mNetManager;
	if (!aNet || !mBoard)
		return;

	int aPicks[4][12] = {};
	int aCounts[4] = {};
	SeedType anImitaters[4][12] = {};

	int aMySlot = aNet->GetMySlot();
	for (int anIndex = 0; anIndex < mBoard->mSeedBank->mNumPackets && anIndex < mSeedsInBank && anIndex < 12; anIndex++)
	{
		SeedType aSeedType = FindSeedInBank(anIndex);
		if (aSeedType == SEED_NONE)
			continue;
		aPicks[aMySlot][aCounts[aMySlot]] = aSeedType;
		anImitaters[aMySlot][aCounts[aMySlot]] = (aSeedType == SEED_IMITATER) ? mChosenSeeds[aSeedType].mImitaterType : SEED_NONE;
		aCounts[aMySlot]++;
	}
	for (int i = 0; i < 4; i++)
	{
		if (i == aMySlot)
			continue;
		for (int j = 0; j < mCoopRemotePickCount[i] && j < 12; j++)
		{
			aPicks[i][j] = mCoopRemotePicks[i][j];
			anImitaters[i][j] = (SeedType)mCoopRemoteImitaters[i][j];
			aCounts[i] = mCoopRemotePickCount[i];
		}
	}
	CoopBuildBankInternal(mBoard->mSeedBank, aMySlot, aPicks, aCounts, anImitaters);
}

bool SeedChooserScreen::CoopBuildBankFromPayload(LawnNet::NetBuffer& thePayload)
{
        if (!mBoard || thePayload.Remaining() < 2)
                return false;

        LawnNet::NetManager* aNet = mApp->mNetManager;
        int aMySlot = aNet ? aNet->GetMySlot() : 0;

        uint8_t aPicksPerPlayer = thePayload.ReadU8Or();
        uint8_t aSlotCount = thePayload.ReadU8Or();
        if (aPicksPerPlayer > 12 || aSlotCount != LawnNet::NET_MAX_PLAYERS)
                return false;

        int aPicks[4][12] = {};
        int aCounts[4] = {};
        SeedType anImitaters[4][12] = {};
        for (int i = 0; i < 4; i++)
        {
                if (thePayload.Remaining() < 1)
                        return false;
                uint8_t aCount = thePayload.ReadU8Or();
                if (aCount > 12 || thePayload.Remaining() < (size_t)aCount * 2)
                        return false;
                aCounts[i] = aCount;
                for (int j = 0; j < aCounts[i]; j++)
                {
                        uint8_t aSeed = thePayload.ReadU8Or();
                        uint8_t anImitater = thePayload.ReadU8Or();
                        if (aSeed >= (uint8_t)SeedType::NUM_SEED_TYPES ||
                                (anImitater != 255 && anImitater >= (uint8_t)SeedType::NUM_SEED_TYPES))
                                return false;
                        aPicks[i][j] = aSeed;
                        anImitaters[i][j] = anImitater == 255 ? SEED_NONE : (SeedType)anImitater;
                }
        }
        if (thePayload.Remaining() != 0)
                return false;
        CoopBuildBankInternal(mBoard->mSeedBank, aMySlot, aPicks, aCounts, anImitaters);
        return true;
}

// --------------------------------------------------------------------------
// Client: apply the host's authoritative chooser state
// --------------------------------------------------------------------------
bool SeedChooserScreen::CoopApplyChooserState(LawnNet::NetBuffer& thePayload)
{
	LawnNet::NetManager* aNet = mApp ? mApp->mNetManager : nullptr;
	if (!aNet || !aNet->IsClient() || !mCoopChooserActive || !mBoard ||
		thePayload.Remaining() < 1)
		return false;

	uint8_t aSlots = 0;
	if (!thePayload.ReadU8(&aSlots) || aSlots != NET_MAX_PLAYERS)
		return false;

	int aPicks[NET_MAX_PLAYERS][12];
	SeedType anImitaters[NET_MAX_PLAYERS][12];
	int aCounts[NET_MAX_PLAYERS] = {};
	bool aSeenSeed[NUM_SEEDS_IN_CHOOSER] = {};
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		for (int j = 0; j < 12; j++)
		{
			aPicks[i][j] = SEED_NONE;
			anImitaters[i][j] = SEED_NONE;
		}

	const bool aPvP = mApp->mGameMode == GAMEMODE_COOP_PVP;
	const int aFixedCount = CoopCountGlobalFixedPicks();
	int aFixedSeen = 0;
	int aTotalCount = 0;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		uint8_t aCount = 0;
		if (!thePayload.ReadU8(&aCount) || aCount > 12 ||
			aCount > aNet->mChooserPickQuota[i] ||
			(!aNet->GetPlayer(i).mActive && aCount != 0) ||
			(aPvP && aNet->GetPlayer(i).mTeam != NETTEAM_PLANTS && aCount != 0) ||
			thePayload.Remaining() < (size_t)aCount * 2)
			return false;
		aCounts[i] = aCount;
		aTotalCount += aCount;
		for (int j = 0; j < aCount; j++)
		{
			uint8_t aSeedWire = 0;
			uint8_t anImitaterWire = 0;
			if (!thePayload.ReadU8(&aSeedWire) || !thePayload.ReadU8(&anImitaterWire) ||
				aSeedWire >= (uint8_t)NUM_SEEDS_IN_CHOOSER || aSeenSeed[aSeedWire])
				return false;
			const SeedType aSeed = (SeedType)aSeedWire;
			SeedType anImitater = SEED_NONE;
			if (aSeed == SEED_IMITATER)
			{
				if (anImitaterWire >= (uint8_t)SEED_GATLINGPEA ||
					!mApp->SeedTypeAvailable((SeedType)anImitaterWire))
					return false;
				anImitater = (SeedType)anImitaterWire;
			}
			else if (anImitaterWire != 0xFF)
				return false;
			if (!mApp->SeedTypeAvailable(aSeed) ||
				SeedNotAllowedToPick(aSeed) || SeedNotAllowedDuringTrial(aSeed))
				return false;

			const bool aFixed = mChosenSeeds[aSeed].mCrazyDavePicked;
			if (aFixed)
			{
				if (i != 0)
					return false;
				aFixedSeen++;
			}
			aSeenSeed[aSeedWire] = true;
			aPicks[i][j] = aSeed;
			anImitaters[i][j] = anImitater;
		}
	}
	if (thePayload.Remaining() != 0 || aFixedSeen != aFixedCount ||
		aTotalCount > aNet->mChooserBankCount)
		return false;

	const int aMySlot = aNet->GetMySlot();
	if (aMySlot <= 0 || aMySlot >= NET_MAX_PLAYERS ||
		aFixedCount + aCounts[aMySlot] > mBoard->mSeedBank->mNumPackets)
		return false;

	// Fixed cards must occupy the stable prefix established by the constructor.
	// Validate that prefix before replacing any optimistic local state.
	bool aFixedIndexSeen[12] = {};
	for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
	{
		const ChosenSeed& aSeed = mChosenSeeds[i];
		if (!aSeed.mCrazyDavePicked || aSeed.mSeedState != SEED_IN_BANK)
			continue;
		if (aSeed.mSeedIndexInBank < 0 || aSeed.mSeedIndexInBank >= aFixedCount ||
			aSeed.mSeedIndexInBank >= 12 || aFixedIndexSeen[aSeed.mSeedIndexInBank])
			return false;
		aFixedIndexSeen[aSeed.mSeedIndexInBank] = true;
	}

	// Rebuild only the mutable part of our local chooser from the host echo.
	// This removes optimistic picks rejected by a simultaneous duplicate race,
	// resets any in-flight animation safely, and assigns contiguous bank indices.
	for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
	{
		ChosenSeed& aSeed = mChosenSeeds[i];
		if (aSeed.mCrazyDavePicked)
			continue;
		GetSeedPositionInChooser(i, aSeed.mX, aSeed.mY);
		aSeed.mStartX = aSeed.mEndX = aSeed.mX;
		aSeed.mStartY = aSeed.mEndY = aSeed.mY;
		aSeed.mTimeStartMotion = 0;
		aSeed.mTimeEndMotion = 0;
		aSeed.mSeedIndexInBank = 0;
		aSeed.mSeedState = i == SEED_IMITATER ? SEED_PACKET_HIDDEN : SEED_IN_CHOOSER;
		if (i == SEED_IMITATER)
			aSeed.mImitaterType = SEED_NONE;
	}
	for (int j = 0; j < aCounts[aMySlot]; j++)
	{
		ChosenSeed& aSeed = mChosenSeeds[aPicks[aMySlot][j]];
		const int aBankIndex = aFixedCount + j;
		GetSeedPositionInBank(aBankIndex, aSeed.mX, aSeed.mY);
		aSeed.mStartX = aSeed.mEndX = aSeed.mX;
		aSeed.mStartY = aSeed.mEndY = aSeed.mY;
		aSeed.mSeedState = SEED_IN_BANK;
		aSeed.mSeedIndexInBank = aBankIndex;
		aSeed.mImitaterType = anImitaters[aMySlot][j];
	}
	mSeedsInBank = aFixedCount + aCounts[aMySlot];
	mSeedsInFlight = 0;

	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		mCoopRemotePickCount[i] = 0;
		for (int j = 0; j < 12; j++)
		{
			mCoopRemotePicks[i][j] = SEED_NONE;
			mCoopRemoteImitaters[i][j] = SEED_NONE;
		}
		if (i == aMySlot)
			continue;
		mCoopRemotePickCount[i] = aCounts[i];
		for (int j = 0; j < aCounts[i]; j++)
		{
			mCoopRemotePicks[i][j] = aPicks[i][j];
			mCoopRemoteImitaters[i][j] = anImitaters[i][j];
		}
	}
	UpdateImitaterButton();
	CoopRefreshStartButton();
	if (mApp->mWidgetManager)
		mApp->mWidgetManager->MarkAllDirty();
	LawnNet::NetTrace("chooser state reconciled slot=%d mutable=%d fixed=%d total=%d",
		aMySlot, aCounts[aMySlot], aFixedCount, aTotalCount);
	return true;
}

// --------------------------------------------------------------------------
// Poll network queues (runs every chooser update on both sides)
// --------------------------------------------------------------------------
void SeedChooserScreen::CoopPollNetwork()
{
	LawnNet::NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsActive())
		return;

	// Keep zero as a real authoritative quota while the chooser generation is active.
	if (aNet->mChooserOpen)
	{
		mCoopChooserActive = true;
		mCoopPicksPerPlayer = aNet->mChooserPicksPerPlayer;
	}

	if (aNet->IsHosting())
	{
		bool aForceStateEcho = aNet->ConsumeChooserStateEchoRequest();
		bool aRefreshNeeded = aForceStateEcho;
		const bool aPvP = mApp->mGameMode == GAMEMODE_COOP_PVP;
		for (int aSlot = 1; aSlot < NET_MAX_PLAYERS; aSlot++)
		{
			const NetPlayerInfo& aPlayer = aNet->GetPlayer(aSlot);
			if (aPlayer.mActive && (!aPvP || aPlayer.mTeam == NETTEAM_PLANTS))
				continue;
			if (mCoopRemotePickCount[aSlot] <= 0)
				continue;
			LawnNet::NetTrace("chooser purged ineligible slot=%d picks=%d active=%d team=%u",
				aSlot, mCoopRemotePickCount[aSlot], aPlayer.mActive ? 1 : 0,
				aPlayer.mTeam);
			mCoopRemotePickCount[aSlot] = 0;
			for (int j = 0; j < 12; j++)
			{
				mCoopRemotePicks[aSlot][j] = SEED_NONE;
				mCoopRemoteImitaters[aSlot][j] = SEED_NONE;
			}
			aForceStateEcho = true;
			aRefreshNeeded = true;
		}
		// apply client picks queued by the network layer
		while (!aNet->mRemotePicks.empty())
		{
			aRefreshNeeded = true;
			LawnNet::NetManager::ChooserPick aPick = aNet->mRemotePicks.front();
			aNet->mRemotePicks.pop_front();
			const bool aValidSlot = aPick.mSlot >= 0 && aPick.mSlot < NET_MAX_PLAYERS;
			const bool aCanPick = aValidSlot && aNet->GetPlayer(aPick.mSlot).mActive &&
				(mApp->mGameMode != GAMEMODE_COOP_PVP ||
					aNet->GetPlayer(aPick.mSlot).mTeam == NETTEAM_PLANTS);
			const int aSlotQuota = aValidSlot ? aNet->mChooserPickQuota[aPick.mSlot] : 0;
			const bool aDuplicate = CoopIsPickedByOthers(aPick.mSeedType);
			const bool aHasQuota = aValidSlot &&
				mCoopRemotePickCount[aPick.mSlot] < aSlotQuota;
			const bool anApplied = mCoopChooserActive && aCanPick && aHasQuota &&
				!aDuplicate && CoopRecordRemotePick(aPick.mSlot, aPick.mSeedType,
					aPick.mImitater, false);
			if (!anApplied)
			{
				aForceStateEcho = true;
				LawnNet::NetTrace("chooser pick rejected at apply slot=%d seed=%u active=%d eligible=%d count=%d quota=%d duplicate=%d",
					aPick.mSlot, aPick.mSeedType, mCoopChooserActive ? 1 : 0,
					aCanPick ? 1 : 0,
					aValidSlot ? mCoopRemotePickCount[aPick.mSlot] : -1,
					aSlotQuota, aDuplicate ? 1 : 0);
			}
		}
		while (!aNet->mRemoteUnpicks.empty())
		{
			aRefreshNeeded = true;
			LawnNet::NetManager::ChooserPick aPick = aNet->mRemoteUnpicks.front();
			aNet->mRemoteUnpicks.pop_front();
			const bool aValidSlot = aPick.mSlot >= 0 && aPick.mSlot < NET_MAX_PLAYERS;
			const bool aCanUnpick = aValidSlot && aNet->GetPlayer(aPick.mSlot).mActive &&
				(mApp->mGameMode != GAMEMODE_COOP_PVP ||
					aNet->GetPlayer(aPick.mSlot).mTeam == NETTEAM_PLANTS);
			const bool anApplied = mCoopChooserActive && aCanUnpick &&
				CoopRecordRemotePick(aPick.mSlot, aPick.mSeedType, SEED_NONE, true);
			if (!anApplied)
			{
				aForceStateEcho = true;
				LawnNet::NetTrace("chooser unpick rejected at apply slot=%d seed=%u active=%d eligible=%d",
					aPick.mSlot, aPick.mSeedType, mCoopChooserActive ? 1 : 0,
					aCanUnpick ? 1 : 0);
			}
		}
		if (aRefreshNeeded)
			CoopRefreshStartButton();
		CoopBroadcastChooserState(aForceStateEcho);
	}
	else
	{
		while (!aNet->mRemoteChooserStates.empty())
		{
			LawnNet::NetBuffer aState = std::move(aNet->mRemoteChooserStates.front());
			aNet->mRemoteChooserStates.pop_front();
			if (!CoopApplyChooserState(aState))
				LawnNet::NetTrace("CHOOSER_STATE rejected by strict parser bytes=%zu", aState.Size());
		}
	}
}
