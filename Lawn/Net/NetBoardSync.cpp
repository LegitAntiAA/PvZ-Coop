#include "../Board.h"
#include "../../LawnApp.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../LawnMower.h"
#include "../GridItem.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Challenge.h"
#include "../Cutscene.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../System/PlayerInfo.h"
#include "../../GameConstants.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "NetManager.h"

using namespace LawnNet;

bool LawnNet::gNetSuppressSounds = false;

namespace LawnNet
{

// ==========================================================================
// Tower defense path (shared between host and clients; host generates,
// clients receive it with the start-game message)
// ==========================================================================
}

void Board::GenerateTowerDefensePath(std::vector<uint8_t>& thePath)
{
	thePath.clear();
	const int aMaxRow = 5; // rows 0..4 (standard lawn height)

	// Keep the three top-edge visits separated by lower-lawn sections.  This
	// makes every generated road use the full height of the board while the
	// rows between the mandatory visits remain random.
	const int aTopVisitColumns[] = { 7, 4, 1 };
	int aRow = 1 + Rand(aMaxRow - 1);
	int aCol = MAX_GRID_SIZE_X - 1;
	thePath.push_back((uint8_t)(aRow * 2 + 0));
	thePath.push_back((uint8_t)aCol);

	auto MoveToRow = [&](int theTargetRow)
	{
		while (aRow != theTargetRow)
		{
			aRow += theTargetRow > aRow ? 1 : -1;
			thePath.push_back((uint8_t)(aRow * 2 + 0));
			thePath.push_back((uint8_t)aCol);
		}
	};

	while (aCol > 0)
	{
		aCol--;
		thePath.push_back((uint8_t)(aRow * 2 + 0));
		thePath.push_back((uint8_t)aCol);

		bool aTopVisit = false;
		bool aLeaveTop = false;
		for (int aVisitCol : aTopVisitColumns)
		{
			aTopVisit |= aCol == aVisitCol;
			aLeaveTop |= aCol == aVisitCol - 1;
		}

		if (aTopVisit)
			MoveToRow(0);
		else if (aLeaveTop)
			MoveToRow(1 + Rand(aMaxRow - 1));
		else if (aRow > 0 && Rand(3) == 0)
			MoveToRow(1 + Rand(aMaxRow - 1));
	}
}

void Board::SetTowerDefensePath(const std::vector<uint8_t>& thePath)
{
	if (!gLawnApp || !gLawnApp->mBoard)
		return;
	gLawnApp->mBoard->mTDPath = thePath;
}

bool Board::NetTdIsPathCell(int theGridX, int theGridY)
{
	if (mTDPath.empty())
		return false;
	for (size_t i = 0; i + 1 < mTDPath.size(); i += 2)
	{
		uint8_t aRow = mTDPath[i] >> 1;
		uint8_t aCol = mTDPath[i + 1];
		if (aRow == theGridY && aCol == theGridX)
			return true;
	}
	return false;
}

// Map a 0..1 position along the path to a (row, col) cell
int Board::NetTdPathPosToCell(float thePos, int& theRow, int& theCol)
{
	int aCells = (int)(mTDPath.size() / 2);
	if (aCells == 0)
	{
		theRow = 0;
		theCol = MAX_GRID_SIZE_X - 1;
		return 0;
	}
	int aIndex = (int)(thePos * aCells);
	if (aIndex < 0) aIndex = 0;
	if (aIndex >= aCells) aIndex = aCells - 1;
	theRow = mTDPath[aIndex * 2] >> 1;
	theCol = mTDPath[aIndex * 2 + 1];
	return aIndex;
}

namespace LawnNet
{

// ==========================================================================
// Host: id allocation + snapshot
// ==========================================================================
}

uint16_t Board::NetAllocId()
{
	return ++mNextNetId;
}

void Board::NetHostTick()
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsHosting())
		return;

	// snapshot at adaptive rate
	uint32_t aNow = GetTickCount();
	if (mLastSnapshotTick == 0)
		mLastSnapshotTick = aNow;
	int aInterval = 1000 / max(1, aNet->GetSnapshotHz());
	if (aNow - mLastSnapshotTick >= (uint32_t)aInterval)
	{
		mLastSnapshotTick = aNow;
		NetBuffer aSnap;
		NetEncodeSnapshot(aSnap);
		aNet->SendSnapshot(aSnap);
	}

	// our cursor
	NetBuffer aCursor;
	aCursor.WriteU8((uint8_t)aNet->GetMySlot());
	aCursor.WriteI16((int16_t)mCursorObject->mX);
	aCursor.WriteI16((int16_t)mCursorObject->mY);
	aCursor.WriteU8((uint8_t)mCursorObject->mCursorType);
	aCursor.WriteU8((uint8_t)mCursorObject->mType);
	aCursor.WriteU8((uint8_t)mCursorObject->mImitaterType);
	aCursor.WriteU8(0);
	aNet->BroadcastMessage(NETMSG_CURSOR, aCursor);
}

void Board::NetEncodeSnapshot(NetBuffer& b)
{
	// ---- board core ----
	uint8_t aFlags = 0;
	if (mPaused) aFlags |= SNAPFLAG_PAUSED;
	if (mLevelComplete) aFlags |= SNAPFLAG_LEVEL_COMPLETE;
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON) aFlags |= SNAPFLAG_ZOMBIES_WON;
	if (mFogBlownCountDown > 0) aFlags |= SNAPFLAG_FOG_BLOWN;

	b.WriteU8(aFlags);
	b.WriteI32(mSunMoney);
	b.WriteU8((uint8_t)min(255, mCurrentWave));
	b.WriteU8((uint8_t)min(255, mNumWaves));
	b.WriteU16((uint16_t)max(0, mHugeWaveCountDown));
	b.WriteU16((uint16_t)max(0, mZombieCountDown));
	b.WriteU16((uint16_t)max(0, mBoardFadeOutCounter));
	b.WriteU16((uint16_t)max(0, mNextSurvivalStageCounter));
	b.WriteU16((uint16_t)(mFogOffset * 2));
	b.WriteU16((uint16_t)max(0, mFogBlownCountDown));
	b.WriteU8((uint8_t)min(255, mLevel));
	b.WriteU8((uint8_t)mTutorialState);
	b.WriteU8((uint8_t)min(255, mIceTrapCounter));
	b.WriteU8((uint8_t)min(255, mShakeCounter));

	// ice trails (visual)
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		b.WriteI16((int16_t)mIceMinX[i]);
		b.WriteU16((uint16_t)max(0, mIceTimer[i]));
	}

	// challenge essentials
	b.WriteU8((uint8_t)mChallenge->mSurvivalStage);
	b.WriteU8((uint8_t)mChallenge->mChallengeState);
	b.WriteU16((uint16_t)min(65535, max(0, mChallenge->mChallengeStateCounter)));
	b.WriteI16((int16_t)max(-32768, min(32767, mChallenge->mChallengeScore)));

	// weather (endless T)
	b.WriteU8(mNetDayPhase);
	b.WriteU16(mNetWeatherTimer);
	b.WriteU16(mNetFogTimer);
	b.WriteU16(mNetStormTimer);

	// pvp / td
	b.WriteU8((uint8_t)min(255, mNetPvPBrainScore));
	b.WriteU8((uint8_t)min(255, mNetPvPKillScore));
	b.WriteI32(mNetPvPBrainTimer);
	b.WriteU8((uint8_t)max(0, mNetTDLives));

	// ---- seed packets ----
	b.WriteU8((uint8_t)mSeedBank->mNumPackets);
	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
		b.WriteU8((uint8_t)aPacket.mPacketType);
		b.WriteU8((uint8_t)aPacket.mImitaterType);
		uint8_t aCooldown = 255;
		if (aPacket.mRefreshTime > 0)
			aCooldown = (uint8_t)(aPacket.mRefreshCounter * 255 / aPacket.mRefreshTime);
		b.WriteU8(aCooldown);
		b.WriteU8(aPacket.mNetOwnerSlot);
		b.WriteU8(aPacket.mActive ? 1 : 0);
	}

	// ---- plants ----
	uint8_t aCount = 0;
	NetBuffer aBody;
	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
	{
		if (aPlant->mDead || !aPlant->mIsOnBoard)
			continue;
		aBody.WriteU16(aPlant->mNetId);
		aBody.WriteU8((uint8_t)aPlant->mSeedType);
		aBody.WriteU8((uint8_t)aPlant->mImitaterType);
		aBody.WriteU8((uint8_t)aPlant->mRow);
		aBody.WriteU8((uint8_t)aPlant->mPlantCol);
		aBody.WriteU16((uint16_t)min(65535, max(0, aPlant->mPlantHealth)));
		aBody.WriteU16((uint16_t)min(65535, max(0, aPlant->mPlantMaxHealth)));
		aBody.WriteU8((uint8_t)aPlant->mState);
		uint8_t aPFlags = 0;
		if (aPlant->mIsAsleep) aPFlags |= 1;
		if (aPlant->mSquished) aPFlags |= 2;
		if (aPlant->mRecentlyEatenCountdown > 0) aPFlags |= 4;
		if (aPlant->mEatenFlashCountdown > 0) aPFlags |= 8;
		aBody.WriteU8(aPFlags);
		aCount++;
	}
	b.WriteU8(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- zombies ----
	aCount = 0;
	aBody.Clear();
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead)
			continue;
		aBody.WriteU16(aZombie->mNetId);
		aBody.WriteU8((uint8_t)aZombie->mZombieType);
		aBody.WriteU8((uint8_t)aZombie->mRow);
		aBody.WriteI16((int16_t)(aZombie->mPosX * 4));
		aBody.WriteI16((int16_t)(aZombie->mPosY * 4));
		aBody.WriteU16((uint16_t)min(65535, max(0, aZombie->mBodyHealth)));
		aBody.WriteU16((uint16_t)min(65535, max(0, aZombie->mBodyMaxHealth)));
		aBody.WriteU16((uint16_t)min(65535, max(0, aZombie->mHelmHealth)));
		aBody.WriteU16((uint16_t)min(65535, max(0, aZombie->mShieldHealth)));
		aBody.WriteU16((uint16_t)min(65535, max(0, aZombie->mFlyingHealth)));
		aBody.WriteU8((uint8_t)aZombie->mZombiePhase);
		aBody.WriteU8((uint8_t)aZombie->mZombieHeight);
		aBody.WriteU8((uint8_t)aZombie->mHelmType);
		aBody.WriteU8((uint8_t)aZombie->mShieldType);
		uint8_t aZFlags = 0;
		if (aZombie->mIsEating) aZFlags |= 1;
		if (aZombie->mInPool) aZFlags |= 2;
		if (aZombie->mOnHighGround) aZFlags |= 4;
		if (aZombie->mHasHead) aZFlags |= 8;
		if (aZombie->mHasArm) aZFlags |= 16;
		if (aZombie->mHasObject) aZFlags |= 32;
		if (aZombie->mVariant) aZFlags |= 64;
		if (aZombie->mMindControlled) aZFlags |= 128;
		aBody.WriteU8(aZFlags);
		aBody.WriteU8((uint8_t)min(255, aZombie->mChilledCounter));
		aBody.WriteU8((uint8_t)min(255, aZombie->mButteredCounter));
		aBody.WriteU8((uint8_t)min(255, aZombie->mIceTrapCounter));
		aBody.WriteU8((uint8_t)min(255, max(0, aZombie->mZombieFade)));
		aBody.WriteU8((uint8_t)min(255, max(0, aZombie->mJustGotShotCounter)));
		aBody.WriteF32(aZombie->mVelX);
		aBody.WriteF32(aZombie->mAltitude);
		aBody.WriteF32(aZombie->mOriginalAnimRate > 0 ? aZombie->mOriginalAnimRate : 12.0f);
		aBody.WriteU8(aZombie->mNetAnimKey);
		aCount++;
	}
	b.WriteU8(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- projectiles ----
	aCount = 0;
	aBody.Clear();
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
	{
		if (aProjectile->mDead)
			continue;
		aBody.WriteU16(aProjectile->mNetId);
		aBody.WriteU8((uint8_t)aProjectile->mProjectileType);
		aBody.WriteU8((uint8_t)aProjectile->mRow);
		aBody.WriteI16((int16_t)(aProjectile->mX * 4));
		aBody.WriteI16((int16_t)(aProjectile->mY * 4));
		aBody.WriteU16((uint16_t)min(65535, max(0, aProjectile->mRenderOrder + 32768)));
		aCount++;
	}
	b.WriteU8(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- coins ----
	aCount = 0;
	aBody.Clear();
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
	{
		if (aCoin->mDead)
			continue;
		aBody.WriteU16(aCoin->mNetId);
		aBody.WriteU8((uint8_t)aCoin->mType);
		aBody.WriteU8((uint8_t)aCoin->mCoinMotion);
		aBody.WriteI16((int16_t)(aCoin->mPosX * 4));
		aBody.WriteI16((int16_t)(aCoin->mPosY * 4));
		uint8_t aCFlags = 0;
		if (aCoin->mIsBeingCollected) aCFlags |= 1;
		if (aCoin->mHitGround) aCFlags |= 2;
		aBody.WriteU8(aCFlags);
		aCount++;
	}
	b.WriteU8(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- lawn mowers ----
	aCount = 0;
	aBody.Clear();
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
	{
		if (aMower->mDead)
			continue;
		aBody.WriteU16(aMower->mNetId);
		aBody.WriteU8((uint8_t)aMower->mRow);
		aBody.WriteU8((uint8_t)aMower->mMowerState);
		aBody.WriteI16((int16_t)(aMower->mPosX * 4));
		aCount++;
	}
	b.WriteU8(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- grid items ----
	aCount = 0;
	aBody.Clear();
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
	{
		if (aGridItem->mDead)
			continue;
		aBody.WriteU16(aGridItem->mNetId);
		aBody.WriteU8((uint8_t)aGridItem->mGridItemType);
		aBody.WriteU8((uint8_t)aGridItem->mGridItemState);
		aBody.WriteU8((uint8_t)aGridItem->mGridX);
		aBody.WriteU8((uint8_t)aGridItem->mGridY);
		aBody.WriteU16((uint16_t)min(65535, max(0, aGridItem->mGridItemCounter)));
		aCount++;
	}
	b.WriteU8(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());
}

// ==========================================================================
// Client: message drain + ghost management
// ==========================================================================

Plant* Board::NetGetGhostPlant(unsigned short theId)
{
	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
		if (aPlant->mNetId == theId && !aPlant->mDead)
			return aPlant;
	return nullptr;
}

Zombie* Board::NetGetGhostZombie(unsigned short theId)
{
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
		if (aZombie->mNetId == theId && !aZombie->mDead)
			return aZombie;
	return nullptr;
}

Projectile* Board::NetGetGhostProjectile(unsigned short theId)
{
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
		if (aProjectile->mNetId == theId && !aProjectile->mDead)
			return aProjectile;
	return nullptr;
}

Coin* Board::NetGetGhostCoin(unsigned short theId)
{
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
		if (aCoin->mNetId == theId && !aCoin->mDead)
			return aCoin;
	return nullptr;
}

GridItem* Board::NetGetGhostGridItem(unsigned short theId)
{
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
		if (aGridItem->mNetId == theId && !aGridItem->mDead)
			return aGridItem;
	return nullptr;
}

LawnMower* Board::NetGetGhostMower(unsigned short theId)
{
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
		if (aMower->mNetId == theId && !aMower->mDead)
			return aMower;
	return nullptr;
}

void Board::NetDrainMessages()
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;

	uint32_t anApplyStart = GetTickCount();

	while (!aNet->mPendingBoardMsgs.empty())
	{
		NetManager::PendingMsg aMsg = std::move(aNet->mPendingBoardMsgs.front());
		aNet->mPendingBoardMsgs.pop_front();
		if (aMsg.mKind == 0)
			NetApplyEvent(aMsg.mData);
		else
			NetApplySnapshot(aMsg.mData);
	}

	mNetLastApplyMs = (int)(GetTickCount() - anApplyStart);
}

void Board::NetApplyEvent(NetBuffer& b)
{
	uint8_t aEvent = b.ReadU8Or();
	switch (aEvent)
	{
	case BOARDEV_PLANT_ADDED:
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t anImitater = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aCol = b.ReadU8Or();
		if (!NetGetGhostPlant(anId) && mSeedBank)
		{
			gNetSuppressSounds = true;
			Plant* aPlant = NewPlant(aCol, aRow, (SeedType)aType, (SeedType)anImitater);
			gNetSuppressSounds = false;
			if (aPlant)
			{
				aPlant->mNetId = anId;
				aPlant->mNetGhost = true;
			}
		}
		break;
	}
	case BOARDEV_PLANT_REMOVED:
	{
		uint16_t anId = b.ReadU16Or();
		Plant* aPlant = NetGetGhostPlant(anId);
		if (aPlant)
			aPlant->Die();
		break;
	}
	case BOARDEV_SUN_ADDED:
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aCoinType = b.ReadU8Or();
		int aX = b.ReadI16Or() / 4;
		int aY = b.ReadI16Or() / 4;
		uint16_t aValue = b.ReadU16Or();
		if (!NetGetGhostCoin(anId))
		{
			gNetSuppressSounds = true;
			Coin* aCoin = AddCoin(aX, aY, (CoinType)aCoinType, CoinMotion::COIN_MOTION_FROM_PLANT);
			gNetSuppressSounds = false;
			if (aCoin)
			{
				aCoin->mNetId = anId;
				aCoin->mNetGhost = true;
				aCoin->mHitGround = true;
				aCoin->mPosX = (float)aX;
				aCoin->mPosY = (float)aY;
				aCoin->mGroundY = aY;
				aCoin->mNetTargetX = (float)aX;
				aCoin->mNetTargetY = (float)aY;
			}
		}
		break;
	}
	case BOARDEV_SUN_COLLECTED:
	{
		uint16_t anId = b.ReadU16Or();
		Coin* aCoin = NetGetGhostCoin(anId);
		if (aCoin && !aCoin->mIsBeingCollected)
		{
			aCoin->mIsBeingCollected = true;
			aCoin->mDead = false;
		}
		break;
	}
	case BOARDEV_ZOMBIE_ADDED:
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aVariant = b.ReadU8Or();
		uint8_t aFromWave = b.ReadU8Or();
		float aSpeed = b.ReadF32Or();
		float aPosX = b.ReadF32Or();
		if (!NetGetGhostZombie(anId))
		{
			gNetSuppressSounds = true;
			Zombie* aZombie = AddZombieInRow((ZombieType)aType, aRow, aFromWave);
			gNetSuppressSounds = false;
			if (aZombie)
			{
				aZombie->mNetId = anId;
				aZombie->mNetGhost = true;
				aZombie->mVariant = aVariant != 0;
				aZombie->mVelX = aSpeed;
				aZombie->mPosX = aPosX;
				aZombie->mX = (int)aPosX;
				aZombie->mNetAnimKey = 255; // force anim apply
			}
		}
		break;
	}
	case BOARDEV_ZOMBIE_REMOVED:
	{
		uint16_t anId = b.ReadU16Or();
		Zombie* aZombie = NetGetGhostZombie(anId);
		if (aZombie)
			aZombie->DieNoLoot();
		break;
	}
	case BOARDEV_MOWER_USED:
	{
		uint8_t aRow = b.ReadU8Or();
		LawnMower* aMower = FindLawnMowerInRow(aRow);
		if (aMower && aMower->mMowerState == MOWER_READY && !aMower->mDead)
		{
			aMower->mMowerState = MOWER_TRIGGERED;
			aMower->mDead = false;
		}
		break;
	}
	case BOARDEV_SOUND:
	{
		uint16_t aSound = b.ReadU16Or();
		if (!gNetSuppressSounds)
			mApp->PlaySample(aSound);
		break;
	}
	case BOARDEV_FOLEY:
	{
		uint16_t aFoley = b.ReadU16Or();
		uint16_t aPitchRaw = b.ReadU16Or();
		if (!gNetSuppressSounds)
		{
			if (aPitchRaw == 0x4000)
				mApp->PlayFoley((FoleyType)aFoley);
			else
				mApp->PlayFoleyPitch((FoleyType)aFoley, aPitchRaw / 4096.0f);
		}
		break;
	}
	case BOARDEV_SHAKE:
	{
		uint16_t anAmount = b.ReadU16Or();
		ShakeBoard(anAmount, anAmount / 2);
		break;
	}
	default:
		break;
	}
}

void Board::NetApplySnapshot(NetBuffer& b)
{
	// ---- board core ----
	uint8_t aFlags = b.ReadU8Or();
	bool aPaused = (aFlags & SNAPFLAG_PAUSED) != 0;
	bool aLevelComplete = (aFlags & SNAPFLAG_LEVEL_COMPLETE) != 0;
	bool aZombiesWon = (aFlags & SNAPFLAG_ZOMBIES_WON) != 0;

	mSunMoney = b.ReadI32Or();
	int aCurrentWave = b.ReadU8Or();
	int aNumWaves = b.ReadU8Or();
	mHugeWaveCountDown = b.ReadU16Or();
	mZombieCountDown = b.ReadU16Or();
	mBoardFadeOutCounter = b.ReadU16Or();
	mNextSurvivalStageCounter = b.ReadU16Or();
	mFogOffset = b.ReadU16Or() / 2.0f;
	mFogBlownCountDown = b.ReadU16Or();
	mLevel = b.ReadU8Or();
	mTutorialState = (TutorialState)b.ReadU8Or();
	mIceTrapCounter = b.ReadU8Or();
	mShakeCounter = b.ReadU8Or();

	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		mIceMinX[i] = b.ReadI16Or();
		mIceTimer[i] = b.ReadU16Or();
	}

	mChallenge->mSurvivalStage = b.ReadU8Or();
	mChallenge->mChallengeState = (ChallengeState)b.ReadU8Or();
	mChallenge->mChallengeStateCounter = b.ReadU16Or();
	mChallenge->mChallengeScore = b.ReadI16Or();

	mNetDayPhase = b.ReadU8Or();
	mNetWeatherTimer = b.ReadU16Or();
	mNetFogTimer = b.ReadU16Or();
	mNetStormTimer = b.ReadU16Or();

	mNetPvPBrainScore = b.ReadU8Or();
	mNetPvPKillScore = b.ReadU8Or();
	mNetPvPBrainTimer = b.ReadI32Or();
	mNetTDLives = b.ReadU8Or();

	// huge wave warning banner (mirrors host-side advice)
	static int aLastWave = -1;
	if (aCurrentWave != aLastWave && aLastWave != -1 && mApp->mGameScene == GameScenes::SCENE_PLAYING)
	{
		if (IsFlagWave(aCurrentWave) && mHugeWaveCountDown > 0)
			DisplayAdvice(_S("[A_HUGE_WAVE_OF_ZOMBIES_IS_APPROACHING]"), MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_NONE);
		else if (aCurrentWave == 0 || aCurrentWave > 0)
		{
			/* regular wave banner is host sound-driven */
		}
	}
	aLastWave = aCurrentWave;
	mCurrentWave = aCurrentWave;
	mNumWaves = aNumWaves;

	// pause overlay state (host controls pausing)
	mPaused = aPaused;
	mNetPausedOverlay = aPaused;
	mApp->mNetManager->mRemotePaused = aPaused;
	if (aLevelComplete)
		mLevelComplete = true;

	// zombies won: trigger the same cutscene the host shows
	if (aZombiesWon && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
	{
		mApp->mGameScene = GameScenes::SCENE_ZOMBIES_WON;
		mCutScene->StartZombiesWon();
	}

	// ---- seed packets ----
	uint8_t aNumPackets = b.ReadU8Or();
	if (mSeedBank && aNumPackets <= SEEDBANK_MAX)
	{
		for (int i = 0; i < aNumPackets; i++)
		{
			SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
			SeedType aType = (SeedType)b.ReadU8Or();
			SeedType anImitater = (SeedType)b.ReadU8Or();
			uint8_t aCooldown = b.ReadU8Or();
			uint8_t anOwner = b.ReadU8Or();
			uint8_t anActive = b.ReadU8Or();
			if (aPacket.mPacketType != aType || aPacket.mImitaterType != anImitater)
			{
				aPacket.SetPacketType(aType, anImitater);
			}
			if (aPacket.mRefreshTime > 0)
				aPacket.mRefreshCounter = aCooldown * aPacket.mRefreshTime / 255;
			aPacket.mNetOwnerSlot = anOwner;
			aPacket.mActive = anActive != 0;
		}
		mSeedBank->mNumPackets = aNumPackets;
	}

	// ---- plants ----
	uint8_t aCount = b.ReadU8Or();
	std::map<unsigned short, bool> aSeenPlants;
	for (int i = 0; i < aCount; i++)
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t anImitater = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aCol = b.ReadU8Or();
		uint16_t aHealth = b.ReadU16Or();
		uint16_t aMaxHealth = b.ReadU16Or();
		uint8_t aState = b.ReadU8Or();
		uint8_t aPFlags = b.ReadU8Or();
		aSeenPlants[anId] = true;

		Plant* aPlant = NetGetGhostPlant(anId);
		if (!aPlant)
		{
			gNetSuppressSounds = true;
			aPlant = NewPlant(aCol, aRow, (SeedType)aType, (SeedType)anImitater);
			gNetSuppressSounds = false;
			if (!aPlant)
				continue;
			aPlant->mNetId = anId;
			aPlant->mNetGhost = true;
		}
		aPlant->mRow = aRow;
		aPlant->mPlantCol = aCol;
		aPlant->mX = GridToPixelX(aCol, aRow) - (aPlant->mWidth - 80) / 2;
		aPlant->mY = GridToPixelY(aCol, aRow) - (aPlant->mHeight - 55) / 2;
		aPlant->mPlantHealth = aHealth;
		aPlant->mPlantMaxHealth = aMaxHealth;
		aPlant->mState = (PlantState)aState;
		aPlant->mIsAsleep = (aPFlags & 1) != 0;
		aPlant->mSquished = (aPFlags & 2) != 0;
		aPlant->mRecentlyEatenCountdown = (aPFlags & 4) ? 20 : 0;
		aPlant->mEatenFlashCountdown = (aPFlags & 8) ? 8 : 0;
	}
	{
		Plant* aPlant = nullptr;
		while (mPlants.IterateNext(aPlant))
			if (aPlant->mNetGhost && !aPlant->mDead && aSeenPlants.find(aPlant->mNetId) == aSeenPlants.end())
				aPlant->Die();
	}

	// ---- zombies ----
	aCount = b.ReadU8Or();
	std::map<unsigned short, bool> aSeenZombies;
	for (int i = 0; i < aCount; i++)
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		int16_t aPosX = b.ReadI16Or();
		int16_t aPosY = b.ReadI16Or();
		uint16_t aBodyHealth = b.ReadU16Or();
		uint16_t aBodyMaxHealth = b.ReadU16Or();
		uint16_t aHelmHealth = b.ReadU16Or();
		uint16_t aShieldHealth = b.ReadU16Or();
		uint16_t aFlyingHealth = b.ReadU16Or();
		uint8_t aPhase = b.ReadU8Or();
		uint8_t aHeight = b.ReadU8Or();
		uint8_t aHelmType = b.ReadU8Or();
		uint8_t aShieldType = b.ReadU8Or();
		uint8_t aZFlags = b.ReadU8Or();
		uint8_t aChilled = b.ReadU8Or();
		uint8_t aButtered = b.ReadU8Or();
		uint8_t aIceTrap = b.ReadU8Or();
		uint8_t aFade = b.ReadU8Or();
		uint8_t aJustShot = b.ReadU8Or();
		float aVelX = b.ReadF32Or();
		float anAltitude = b.ReadF32Or();
		float anAnimRate = b.ReadF32Or();
		uint8_t anAnimKey = b.ReadU8Or();
		aSeenZombies[anId] = true;

		Zombie* aZombie = NetGetGhostZombie(anId);
		if (!aZombie)
		{
			gNetSuppressSounds = true;
			aZombie = AddZombieInRow((ZombieType)aType, aRow, 0);
			gNetSuppressSounds = false;
			if (!aZombie)
				continue;
			aZombie->mNetId = anId;
			aZombie->mNetGhost = true;
			aZombie->mNetAnimKey = 255;
		}
		aZombie->mRow = aRow;
		aZombie->mNetTargetX = aPosX / 4.0f;
		aZombie->mNetTargetY = aPosY / 4.0f;
		aZombie->mBodyHealth = aBodyHealth;
		aZombie->mBodyMaxHealth = aBodyMaxHealth;
		aZombie->mHelmHealth = aHelmHealth;
		aZombie->mShieldHealth = aShieldHealth;
		aZombie->mFlyingHealth = aFlyingHealth;
		aZombie->mZombiePhase = (ZombiePhase)aPhase;
		aZombie->mZombieHeight = (ZombieHeight)aHeight;
		aZombie->mHelmType = (HelmType)aHelmType;
		aZombie->mShieldType = (ShieldType)aShieldType;
		aZombie->mIsEating = (aZFlags & 1) != 0;
		aZombie->mInPool = (aZFlags & 2) != 0;
		aZombie->mOnHighGround = (aZFlags & 4) != 0;
		aZombie->mHasHead = (aZFlags & 8) != 0;
		aZombie->mHasArm = (aZFlags & 16) != 0;
		aZombie->mHasObject = (aZFlags & 32) != 0;
		aZombie->mVariant = (aZFlags & 64) != 0;
		aZombie->mMindControlled = (aZFlags & 128) != 0;
		aZombie->mChilledCounter = aChilled;
		aZombie->mButteredCounter = aButtered;
		aZombie->mIceTrapCounter = aIceTrap;
		aZombie->mZombieFade = aFade;
		aZombie->mJustGotShotCounter = aJustShot;
		aZombie->mVelX = aVelX;
		aZombie->mAltitude = anAltitude;
		aZombie->mOriginalAnimRate = anAnimRate;

		NetGhostUpdateZombie(aZombie, anAnimKey);
	}
	{
		Zombie* aZombie = nullptr;
		while (mZombies.IterateNext(aZombie))
			if (aZombie->mNetGhost && !aZombie->mDead && aSeenZombies.find(aZombie->mNetId) == aSeenZombies.end())
				aZombie->DieNoLoot();
	}

	// ---- projectiles ----
	aCount = b.ReadU8Or();
	std::map<unsigned short, bool> aSeenProjectiles;
	for (int i = 0; i < aCount; i++)
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		int16_t aX = b.ReadI16Or();
		int16_t aY = b.ReadI16Or();
		uint16_t aRenderOrder = b.ReadU16Or();
		aSeenProjectiles[anId] = true;

		Projectile* aProjectile = NetGetGhostProjectile(anId);
		if (!aProjectile)
		{
			gNetSuppressSounds = true;
			aProjectile = AddProjectile(aX / 4, aY / 4, (int)aRenderOrder - 32768, aRow, (ProjectileType)aType);
			gNetSuppressSounds = false;
			if (!aProjectile)
				continue;
			aProjectile->mNetId = anId;
			aProjectile->mNetGhost = true;
		}
		aProjectile->mRow = aRow;
		aProjectile->mNetTargetX = aX / 4.0f;
		aProjectile->mNetTargetY = aY / 4.0f;
	}
	{
		Projectile* aProjectile = nullptr;
		while (mProjectiles.IterateNext(aProjectile))
			if (aProjectile->mNetGhost && !aProjectile->mDead && aSeenProjectiles.find(aProjectile->mNetId) == aSeenProjectiles.end())
				aProjectile->Die();
	}

	// ---- coins ----
	aCount = b.ReadU8Or();
	std::map<unsigned short, bool> aSeenCoins;
	for (int i = 0; i < aCount; i++)
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aMotion = b.ReadU8Or();
		int16_t aX = b.ReadI16Or();
		int16_t aY = b.ReadI16Or();
		uint8_t aCFlags = b.ReadU8Or();
		aSeenCoins[anId] = true;

		Coin* aCoin = NetGetGhostCoin(anId);
		if (!aCoin)
		{
			gNetSuppressSounds = true;
			aCoin = AddCoin(aX / 4, aY / 4, (CoinType)aType, (CoinMotion)aMotion);
			gNetSuppressSounds = false;
			if (!aCoin)
				continue;
			aCoin->mNetId = anId;
			aCoin->mNetGhost = true;
			aCoin->mHitGround = true;
		}
		aCoin->mNetTargetX = aX / 4.0f;
		aCoin->mNetTargetY = aY / 4.0f;
		aCoin->mIsBeingCollected = (aCFlags & 1) != 0;
		aCoin->mHitGround = (aCFlags & 2) != 0;
	}
	{
		Coin* aCoin = nullptr;
		while (mCoins.IterateNext(aCoin))
			if (aCoin->mNetGhost && !aCoin->mDead && aSeenCoins.find(aCoin->mNetId) == aSeenCoins.end())
			{
				if (aCoin->mIsBeingCollected)
					aCoin->StartFade();
				else
					aCoin->Die();
			}
	}

	// ---- lawn mowers ----
	aCount = b.ReadU8Or();
	std::map<unsigned short, bool> aSeenMowers;
	for (int i = 0; i < aCount; i++)
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aState = b.ReadU8Or();
		int16_t aX = b.ReadI16Or();
		aSeenMowers[anId] = true;

		LawnMower* aMower = NetGetGhostMower(anId);
		if (!aMower)
		{
			gNetSuppressSounds = true;
			aMower = mLawnMowers.DataArrayAlloc();
			if (!aMower)
				continue;
			aMower->LawnMowerInitialize(aRow);
			aMower->mNetId = anId;
			aMower->mNetGhost = true;
			gNetSuppressSounds = false;
		}
		aMower->mMowerState = (LawnMowerState)aState;
		aMower->mPosX = aX / 4.0f;
		aMower->mNetTargetX = aX / 4.0f;
	}
	{
		LawnMower* aMower = nullptr;
		while (mLawnMowers.IterateNext(aMower))
			if (aMower->mNetGhost && !aMower->mDead && aSeenMowers.find(aMower->mNetId) == aSeenMowers.end())
				aMower->mDead = true;
	}

	// ---- grid items ----
	aCount = b.ReadU8Or();
	std::map<unsigned short, bool> aSeenGridItems;
	for (int i = 0; i < aCount; i++)
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aState = b.ReadU8Or();
		uint8_t aX = b.ReadU8Or();
		uint8_t aY = b.ReadU8Or();
		uint16_t aCounter = b.ReadU16Or();
		aSeenGridItems[anId] = true;

		GridItem* aGridItem = NetGetGhostGridItem(anId);
		if (!aGridItem)
		{
			gNetSuppressSounds = true;
			aGridItem = mGridItems.DataArrayAlloc();
			if (!aGridItem)
				continue;
			aGridItem->mGridItemType = (GridItemType)aType;
			aGridItem->mGridItemState = (GridItemState)aState;
			aGridItem->mGridX = aX;
			aGridItem->mGridY = aY;
			aGridItem->mGridItemCounter = aCounter;
			aGridItem->mPosX = (float)GridToPixelX(aX, aY);
			aGridItem->mPosY = (float)GridToPixelY(aX, aY);
			aGridItem->mNetId = anId;
			aGridItem->mNetGhost = true;
			gNetSuppressSounds = false;
		}
		aGridItem->mGridItemState = (GridItemState)aState;
		aGridItem->mGridItemCounter = aCounter;
	}
	{
		GridItem* aGridItem = nullptr;
		while (mGridItems.IterateNext(aGridItem))
			if (aGridItem->mNetGhost && !aGridItem->mDead && aSeenGridItems.find(aGridItem->mNetId) == aSeenGridItems.end())
				aGridItem->mDead = true;
	}
}

// ==========================================================================
// Client ghost per-tick updates
// ==========================================================================

void Board::NetGhostUpdateZombie(Zombie* theZombie, uint8_t theAnimKey)
{
	// interpolate position toward the authoritative target
	theZombie->mPosX += (theZombie->mNetTargetX - theZombie->mPosX) * 0.25f;
	theZombie->mPosY += (theZombie->mNetTargetY - theZombie->mPosY) * 0.25f;
	theZombie->mX = (int)theZombie->mPosX;
	theZombie->mY = (int)theZombie->mPosY;

	if (theAnimKey != theZombie->mNetAnimKey)
	{
		theZombie->mNetAnimKey = theAnimKey;
		Reanimation* aBody = mApp->ReanimationTryToGet(theZombie->mBodyReanimID);
		if (aBody)
		{
			if (theAnimKey == 1)
			{
				aBody->SetFramesForLayer("anim_eat");
				aBody->mLoopType = ReanimLoopType::REANIM_LOOP;
			}
			else if (theAnimKey == 2)
			{
				theZombie->PlayDeathAnim(0U);
			}
			else
			{
				theZombie->StartWalkAnim(10);
			}
		}
	}

	// cosmetic counters
	if (theZombie->mJustGotShotCounter > 0)
		theZombie->mJustGotShotCounter--;
	if (theZombie->mChilledCounter > 0)
		theZombie->mChilledCounter--;
	if (theZombie->mButteredCounter > 0)
		theZombie->mButteredCounter--;
	if (theZombie->mIceTrapCounter > 0)
		theZombie->mIceTrapCounter--;

	theZombie->Animate();
}

void Board::NetGhostUpdateOthers()
{
	// projectiles: interpolate
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
	{
		if (aProjectile->mDead || !aProjectile->mNetGhost)
			continue;
		aProjectile->mX += (int)((aProjectile->mNetTargetX - aProjectile->mX) * 0.35f);
		aProjectile->mY += (int)((aProjectile->mNetTargetY - aProjectile->mY) * 0.35f);
	}

	// coins: interpolate unless being collected (then local flight anim)
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
	{
		if (aCoin->mDead || !aCoin->mNetGhost)
			continue;
		if (aCoin->mIsBeingCollected)
		{
			aCoin->Update();
			continue;
		}
		aCoin->mPosX += (aCoin->mNetTargetX - aCoin->mPosX) * 0.2f;
		aCoin->mPosY += (aCoin->mNetTargetY - aCoin->mPosY) * 0.2f;
		aCoin->mX = (int)aCoin->mPosX;
		aCoin->mY = (int)aCoin->mPosY;
	}

	// mowers: interpolate
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
	{
		if (aMower->mDead || !aMower->mNetGhost)
			continue;
		aMower->mPosX += (aMower->mNetTargetX - aMower->mPosX) * 0.3f;
	}
}

void Board::NetClientTick()
{
	// Drain and apply incoming messages first (events then snapshots in order)
	NetDrainMessages();

	// ghost micro-updates (interpolation, cosmetic counters)
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
		if (!aZombie->mDead && aZombie->mNetGhost)
			NetGhostUpdateZombie(aZombie, aZombie->mNetAnimKey);
	NetGhostUpdateOthers();

	// send our mouse position (cosmetic for the host + others)
	NetManager* aNet = mApp->mNetManager;
	if (aNet)
	{
		WidgetManager* aWM = mApp->mWidgetManager;
		aNet->SendCursor((int)mCursorObject->mCursorType, (int)mCursorObject->mType,
			(int)mCursorObject->mImitaterType, 0, aWM->mLastMouseX, aWM->mLastMouseY);
	}

	// fade counters run locally for visuals
	if (mBoardFadeOutCounter > 0)
		mBoardFadeOutCounter--;
	if (mShakeCounter > 0)
		mShakeCounter--;
	if (mIceTrapCounter > 0)
		mIceTrapCounter--;
}

// ==========================================================================
// Remote cursor rendering (board scene)
// ==========================================================================

void Board::NetDrawRemoteCursors(Graphics* g)
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsActive())
		return;

	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		if (i == aNet->GetMySlot() || !aNet->mRemoteCursors[i].mValid)
			continue;
		const NetRemoteCursor& aCursor = aNet->mRemoteCursors[i];
		const NetPlayerInfo& aPlayer = aNet->GetPlayer(i);
		if (!aPlayer.mActive)
			continue;
		int aCursorX = (int)aCursor.mRenderX;
		int aCursorY = (int)aCursor.mRenderY;

		NetColor aColor = GetRelativePlayerColor(aNet->GetMySlot(), i);

		// held plant silhouette in the owner's color
		if (aCursor.mHeldSeed != 255 && aCursor.mHeldSeed != SEED_NONE &&
			aCursor.mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
		{
			SeedType aSeedType = (SeedType)aCursor.mHeldSeed;
			SeedType anImitater = aCursor.mHeldImitater != 255 ? (SeedType)aCursor.mHeldImitater : SeedType::SEED_NONE;
			Plant::DrawSeedType(g, aSeedType, anImitater, VARIATION_NORMAL, (float)(aCursorX - 25), (float)(aCursorY - 35));
			// color overlay to mark ownership
			g->SetColor(Color(aColor.r, aColor.g, aColor.b, 90));
			g->FillRect(aCursorX - 30, aCursorY - 40, 60, 70);
		}

		// cursor pointer marker in player color
		g->SetColor(Color(aColor.r, aColor.g, aColor.b, 255));
		g->FillRect(aCursorX, aCursorY, 9, 9);

		// nick + ping label in small font
		if (aPlayer.mNick[0])
		{
			SexyString aLabel = StringToSexyString(aPlayer.mNick) + _S(" ") + StrFormat(_S("%ums"), aPlayer.mPing);
			int aLabelX = aCursorX + 14;
			int aLabelY = aCursorY + 10;
			g->SetColor(Color(0, 0, 0, 180));
			g->DrawString(aLabel, aLabelX + 1, aLabelY + 1);
			g->SetColor(Color(aColor.r, aColor.g, aColor.b, 255));
			g->DrawString(aLabel, aLabelX, aLabelY);
		}
	}
}

// ==========================================================================
// Tower defense road rendering
// ==========================================================================

void Board::NetDrawTowerDefensePath(Graphics* g)
{
	if (mTDPath.empty() || mApp->mGameMode != GameMode::GAMEMODE_COOP_TOWER_DEFENSE)
		return;

	// cut the matching cell out of the unsodded (dirt) lawn image, aligned the
	// same way DrawBackdrop places the regular background
	const int aBgX = -BOARD_OFFSET + WIDESCREEN_OFFSETX;
	const int aBgY = WIDESCREEN_OFFSETY;
	for (size_t i = 0; i + 1 < mTDPath.size(); i += 2)
	{
		int aRow = mTDPath[i] >> 1;
		int aCol = mTDPath[i + 1];
		int aX = GridToPixelX(aCol, aRow);
		int aY = GridToPixelY(aCol, aRow);

		if (Sexy::IMAGE_BACKGROUND1UNSODDED)
		{
			Rect aSrcRect(aX - aBgX, aY - aBgY, 80, 100);
			g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, aX, aY, aSrcRect);
		}
		else
		{
			g->SetColor(Color(120, 85, 55, 200));
			g->FillRect(aX, aY, 80, 100);
		}
	}
}

// ==========================================================================
// Endless T weather rendering (darkness at night, fog, storm flashes)
// ==========================================================================

void Board::DrawEndlessWeather(Graphics* g)
{
	if (!IsEndlessT())
		return;

	// night darkness
	if (mNetDayPhase == 1)
	{
		g->SetColor(Color(10, 10, 40, 110));
		g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	}

	// fog roll from the right
	if (mNetFogTimer > 0)
	{
		g->SetColor(Color(210, 215, 225, 120));
		g->FillRect(400, 0, BOARD_WIDTH - 400, BOARD_HEIGHT);
		g->SetColor(Color(190, 195, 210, 70));
		g->FillRect(300, 0, 100, BOARD_HEIGHT);
	}

	// storm: lightning flash
	if (mNetStormTimer > 0)
	{
		int aFlash = mMainCounter % 180;
		if (aFlash < 6)
		{
			g->SetColor(Color(255, 255, 255, 130 - aFlash * 15));
			g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
		}
	}
}

// ==========================================================================
// Tower defense + PvP gameplay helpers
// ==========================================================================

bool Board::IsTowerDefense()
{
	return mApp->mGameMode == GameMode::GAMEMODE_COOP_TOWER_DEFENSE;
}

bool Board::IsPvPMode()
{
	return mApp->mGameMode == GameMode::GAMEMODE_COOP_PVP;
}

void Board::AssignPvPSeedOwners()
{
	if (!IsPvPMode() || !mSeedBank || !mApp->mNetManager)
		return;

	int aPlantSlots[LawnNet::NET_MAX_PLAYERS];
	int aZombieSlots[LawnNet::NET_MAX_PLAYERS];
	int aPlantCount = 0;
	int aZombieCount = 0;
	for (int aSlot = 0; aSlot < LawnNet::NET_MAX_PLAYERS; aSlot++)
	{
		const LawnNet::NetPlayerInfo& aPlayer = mApp->mNetManager->GetPlayer(aSlot);
		if (!aPlayer.mActive)
			continue;
		if (aPlayer.mTeam == LawnNet::NETTEAM_PLANTS)
			aPlantSlots[aPlantCount++] = aSlot;
		else if (aPlayer.mTeam == LawnNet::NETTEAM_ZOMBIES)
			aZombieSlots[aZombieCount++] = aSlot;
	}

	// PreNewGame rejects an incomplete PvP lobby.  Keep safe fallbacks for
	// malformed/replayed state so every packet still has a valid owner.
	if (aPlantCount == 0)
		aPlantSlots[aPlantCount++] = 0;
	if (aZombieCount == 0)
		aZombieSlots[aZombieCount++] = 0;

	int aPlantRoundRobin = 0;
	int aZombieRoundRobin = 0;
	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
		if (aPacket.mPacketType == SeedType::SEED_NONE)
			continue;
		if (mChallenge->IsZombieSeedType(aPacket.mPacketType))
		{
			aPacket.mNetOwnerSlot = (unsigned char)aZombieSlots[aZombieRoundRobin++ % aZombieCount];
			continue;
		}

		int anOwner = aPacket.mNetOwnerSlot;
		bool anOwnerIsPlant = anOwner >= 0 && anOwner < LawnNet::NET_MAX_PLAYERS &&
			mApp->mNetManager->GetPlayer(anOwner).mActive &&
			mApp->mNetManager->GetPlayer(anOwner).mTeam == LawnNet::NETTEAM_PLANTS;
		if (!anOwnerIsPlant)
			aPacket.mNetOwnerSlot = (unsigned char)aPlantSlots[aPlantRoundRobin++ % aPlantCount];
	}
}

bool Board::CanTakeBrains(int theAmount)
{
	return mNetPvPBrainTimer >= theAmount;
}

void Board::TakeBrains(int theAmount)
{
	mNetPvPBrainTimer = max(0, mNetPvPBrainTimer - theAmount);
}

// host-side per-tick updates for the custom modes
void Board::UpdateTowerDefenseGame()
{
	if (!IsTowerDefense() || mApp->NetIsClient())
		return;

	// TD path zombies are driven by Zombie::UpdateTowerDefense
}

void Board::UpdatePvP()
{
	if (!IsPvPMode() || mApp->NetIsClient())
		return;

	// brains trickle: +50 every ~10 seconds, shared by the zombie team
	if (mNetPvPBrainTimer > 0 || mApp->mGameScene == GameScenes::SCENE_PLAYING)
	{
		mNetPvPBrainTick++;
		if (mNetPvPBrainTick >= 1000)
		{
			mNetPvPBrainTick = 0;
			mNetPvPBrainTimer += 50;
		}
	}

	// win checks
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
		return;
	if (mNetPvPBrainScore >= 5)
	{
		ZombiesWon(nullptr);
	}
	else if (mNetPvPKillScore >= 100)
	{
		FadeOutLevel();
	}
}

// a zombie crossed into the house: brain for the zombie team
void Board::NetPvPZombieScored(Zombie* theZombie)
{
	if (!IsPvPMode() || mApp->NetIsClient())
		return;
	mNetPvPBrainScore++;
	theZombie->DieNoLoot();
	mApp->PlaySample(Sexy::SOUND_CHOMP);
}

// PvP score display
void Board::NetDrawPvPScore(Graphics* g)
{
	if (!IsPvPMode())
		return;

	SexyString aScoreStr = StrFormat(_S("BRAINS %d/5    KILLS %d/100"), mNetPvPBrainScore, mNetPvPKillScore);
	TodDrawString(g, aScoreStr, BOARD_WIDTH / 2, 8, Sexy::FONT_BRIANNETOD12,
		Color(255, 240, 200), DrawStringJustification::DS_ALIGN_CENTER);
}

// ==========================================================================
// Zombie: tower defense path steering (the stock walking code supplies speed)
// ==========================================================================

void Zombie::UpdateTowerDefense(float theDistanceWalked)
{
	Board* aBoard = mBoard;
	int aCells = (int)(aBoard->mTDPath.size() / 2);
	if (aCells < 2 || mDead)
		return;

	int aStartRow = aBoard->mTDPath[0] >> 1;
	int aStartCol = aBoard->mTDPath[1];
	float aStartX = (float)aBoard->GridToPixelX(aStartCol, aStartRow);
	float aStartY = (float)aBoard->GridToPixelY(aStartCol, aStartRow);

	// Spawned zombies walk in from off-screen exactly as in the original game,
	// then enter the first road cell without a teleport.
	if (mTdPathPos <= 0.0f && mPosX > aStartX)
	{
		mRow = aStartRow;
		mPosY = aStartY;
		return;
	}

	mTdPathPos = max(0.0f, mTdPathPos + theDistanceWalked);

	float aDistanceLeft = mTdPathPos;
	for (int anIndex = 0; anIndex < aCells - 1; anIndex++)
	{
		int aRowA = aBoard->mTDPath[anIndex * 2] >> 1;
		int aColA = aBoard->mTDPath[anIndex * 2 + 1];
		int aRowB = aBoard->mTDPath[(anIndex + 1) * 2] >> 1;
		int aColB = aBoard->mTDPath[(anIndex + 1) * 2 + 1];

		float aXA = (float)aBoard->GridToPixelX(aColA, aRowA);
		float aYA = (float)aBoard->GridToPixelY(aColA, aRowA);
		float aXB = (float)aBoard->GridToPixelX(aColB, aRowB);
		float aYB = (float)aBoard->GridToPixelY(aColB, aRowB);
		float aDeltaX = aXB - aXA;
		float aDeltaY = aYB - aYA;
		float aSegmentLength = sqrtf(aDeltaX * aDeltaX + aDeltaY * aDeltaY);

		if (aDistanceLeft <= aSegmentLength)
		{
			float aT = aSegmentLength > 0.0f ? aDistanceLeft / aSegmentLength : 0.0f;
			mPosX = aXA + aDeltaX * aT;
			mPosY = aYA + aDeltaY * aT;
			mRow = aT < 0.5f ? aRowA : aRowB;
			return;
		}

		aDistanceLeft -= aSegmentLength;
	}

	// The original walking distance has reached the end of the complete road.
	aBoard->mNetTDLives--;
	mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);
	if (aBoard->mNetTDLives <= 0)
		aBoard->ZombiesWon(nullptr);
	DieNoLoot();
}
