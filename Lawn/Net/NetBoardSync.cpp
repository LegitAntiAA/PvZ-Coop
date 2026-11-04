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
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../System/PlayerInfo.h"
#include "../../GameConstants.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "NetManager.h"
#include <float.h>
#include <limits.h>

using namespace LawnNet;

bool LawnNet::gNetSuppressSounds = false;

namespace
{
enum NetDaveAnimKey : uint8_t
{
	NET_DAVE_ANIM_NONE = 0,
	NET_DAVE_ANIM_ENTER,
	NET_DAVE_ANIM_ENTER_UP,
	NET_DAVE_ANIM_LEAVE,
	NET_DAVE_ANIM_GRAB,
	NET_DAVE_ANIM_IDLE,
	NET_DAVE_ANIM_IDLE_HANDING,
	NET_DAVE_ANIM_TALK_HANDING,
	NET_DAVE_ANIM_TALK_CRAZY,
	NET_DAVE_ANIM_TALK_SMALL,
	NET_DAVE_ANIM_TALK_MEDIUM,
	NET_DAVE_ANIM_TALK_BLAH,
	NET_DAVE_ANIM_COUNT
};

struct NetDaveAnimSpec
{
	const char* mTrackName;
	ReanimLoopType mLoopType;
	float mAnimRate;
};

const NetDaveAnimSpec gNetDaveAnimSpecs[NET_DAVE_ANIM_COUNT] =
{
	{ nullptr,             ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD,  0.0f },
	{ "anim_enter",        ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 24.0f },
	{ "anim_enterup",      ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_leave",        ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 24.0f },
	{ "anim_grab",         ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 18.0f },
	{ "anim_idle",         ReanimLoopType::REANIM_LOOP,               12.0f },
	{ "anim_idle_handing", ReanimLoopType::REANIM_LOOP,               12.0f },
	{ "anim_talk_handing", ReanimLoopType::REANIM_LOOP,               12.0f },
	{ "anim_crazy",        ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_smalltalk",    ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_mediumtalk",   ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_blahblah",     ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f }
};

static const size_t NET_DAVE_STATE_BYTES = 12;
// Seed packet: type, imitater, cooldown, owner, active, signed horizontal offset,
// plus the authoritative slot-machine countdown, next card and scroll phase.
static const size_t NET_SEED_PACKET_WIRE_BYTES = 14;
// Projectile: id(2), type/row(2), x/y/z/shadow(16), order(4),
// rotation(4), motion/frame/high-ground(3).
static const size_t NET_PROJECTILE_WIRE_BYTES = 31;
// Coin: id/type/motion(4), x/y(8), order(4), scale/collect(4),
// flags/seed/fade(3), disappear(2), potted visual seed/garden/facing/variation/age(5).
static const size_t NET_COIN_WIRE_BYTES = 30;

bool NetIsValidSeedPacketType(uint8_t theType)
{
	if (theType == 255 || theType < (uint8_t)SeedType::NUM_SEED_TYPES)
		return true;

	return theType == (uint8_t)SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE ||
		theType == (uint8_t)SeedType::SEED_BEGHOULED_BUTTON_CRATER ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_SUN ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_DIAMOND ||
		theType == (uint8_t)SeedType::SEED_ZOMBIQUARIUM_SNORKLE ||
		theType == (uint8_t)SeedType::SEED_ZOMBIQUARIUM_TROPHY ||
		(theType >= (uint8_t)SeedType::SEED_ZOMBIE_NORMAL &&
		 theType <= (uint8_t)SeedType::SEED_ZOMBIE_NEWSPAPER);
}

bool NetIsValidSlotMachineSeed(uint8_t theType)
{
	return theType == 255 ||
		theType == (uint8_t)SeedType::SEED_SUNFLOWER ||
		theType == (uint8_t)SeedType::SEED_PEASHOOTER ||
		theType == (uint8_t)SeedType::SEED_SNOWPEA ||
		theType == (uint8_t)SeedType::SEED_WALLNUT ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_SUN ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_DIAMOND;
}

struct NetDaveWireState
{
	uint8_t mState;
	uint8_t mAnimKey;
	int mMessageIndex;
	uint16_t mAnimTimeQ;
	int mX;
	int mY;
};

uint8_t NetGetDaveAnimKey(Reanimation* theReanim)
{
	if (!theReanim || theReanim->mDead)
		return NET_DAVE_ANIM_NONE;

	// Check the exceptional tracks before their more common siblings.  Some
	// Dave animations share base-pose frames, while IsAnimPlaying compares the
	// complete frame range selected by the named layer.
	const uint8_t aPriority[] =
	{
		NET_DAVE_ANIM_ENTER_UP, NET_DAVE_ANIM_GRAB, NET_DAVE_ANIM_LEAVE,
		NET_DAVE_ANIM_ENTER, NET_DAVE_ANIM_TALK_HANDING,
		NET_DAVE_ANIM_IDLE_HANDING, NET_DAVE_ANIM_TALK_CRAZY,
		NET_DAVE_ANIM_TALK_SMALL, NET_DAVE_ANIM_TALK_MEDIUM,
		NET_DAVE_ANIM_TALK_BLAH, NET_DAVE_ANIM_IDLE
	};
	for (uint8_t aKey : aPriority)
	{
		if (theReanim->IsAnimPlaying(gNetDaveAnimSpecs[aKey].mTrackName))
			return aKey;
	}
	return NET_DAVE_ANIM_NONE;
}

uint8_t NetGetFallbackDaveAnimKey(CrazyDaveState theState)
{
	switch (theState)
	{
	case CrazyDaveState::CRAZY_DAVE_ENTERING:        return NET_DAVE_ANIM_ENTER;
	case CrazyDaveState::CRAZY_DAVE_LEAVING:         return NET_DAVE_ANIM_LEAVE;
	case CrazyDaveState::CRAZY_DAVE_IDLING:          return NET_DAVE_ANIM_IDLE;
	case CrazyDaveState::CRAZY_DAVE_TALKING:         return NET_DAVE_ANIM_TALK_MEDIUM;
	case CrazyDaveState::CRAZY_DAVE_HANDING_TALKING: return NET_DAVE_ANIM_TALK_HANDING;
	case CrazyDaveState::CRAZY_DAVE_HANDING_IDLING:  return NET_DAVE_ANIM_IDLE_HANDING;
	default:                                          return NET_DAVE_ANIM_NONE;
	}
}

NetDaveWireState NetCaptureDaveWireState(LawnApp* theApp)
{
	NetDaveWireState aState = {};
	aState.mState = (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF;
	aState.mAnimKey = NET_DAVE_ANIM_NONE;
	aState.mMessageIndex = -1;

	Reanimation* aDaveReanim = theApp->ReanimationTryToGet(theApp->mCrazyDaveReanimID);
	const int aRawDaveState = (int)theApp->mCrazyDaveState;
	if (!aDaveReanim || aDaveReanim->mDead ||
		aRawDaveState <= (int)CrazyDaveState::CRAZY_DAVE_OFF ||
		aRawDaveState > (int)CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		return aState;
	}

	aState.mAnimKey = NetGetDaveAnimKey(aDaveReanim);
	if (aState.mAnimKey == NET_DAVE_ANIM_NONE)
		aState.mAnimKey = NetGetFallbackDaveAnimKey(theApp->mCrazyDaveState);
	if (aState.mAnimKey == NET_DAVE_ANIM_NONE)
		return aState;

	aState.mState = (uint8_t)aRawDaveState;
	if (theApp->mCrazyDaveMessageIndex >= 0 && theApp->mCrazyDaveMessageIndex <= 9999)
		aState.mMessageIndex = theApp->mCrazyDaveMessageIndex;
	int aTimeQ = (int)(aDaveReanim->mAnimTime * 65535.0f + 0.5f);
	aState.mAnimTimeQ = (uint16_t)max(0, min(65535, aTimeQ));
	aState.mX = max(-32768, min(32767, (int)aDaveReanim->mOverlayMatrix.m02));
	aState.mY = max(-32768, min(32767, (int)aDaveReanim->mOverlayMatrix.m12));
	return aState;
}

void NetWriteDaveWireState(NetBuffer& theBuffer, const NetDaveWireState& theState)
{
	theBuffer.WriteU8(theState.mState);
	theBuffer.WriteU8(theState.mAnimKey);
	theBuffer.WriteI32(theState.mMessageIndex);
	theBuffer.WriteU16(theState.mAnimTimeQ);
	theBuffer.WriteI16((int16_t)theState.mX);
	theBuffer.WriteI16((int16_t)theState.mY);
}

bool NetValidateDaveWireState(const NetDaveWireState& theState)
{
	const bool aStateValid = theState.mState <= (uint8_t)CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
	const bool anAnimValid = theState.mAnimKey < NET_DAVE_ANIM_COUNT;
	const bool aMessageValid = theState.mMessageIndex == -1 ||
		(theState.mMessageIndex >= 0 && theState.mMessageIndex <= 9999);
	const bool anOffCanonical = theState.mState != (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF ||
		(theState.mAnimKey == NET_DAVE_ANIM_NONE && theState.mMessageIndex == -1 &&
		 theState.mAnimTimeQ == 0 && theState.mX == 0 && theState.mY == 0);
	const bool anOnCanonical = theState.mState == (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF ||
		theState.mAnimKey != NET_DAVE_ANIM_NONE;
	return aStateValid && anAnimValid && aMessageValid && anOffCanonical && anOnCanonical;
}

bool NetReadDaveWireState(NetBuffer& theBuffer, NetDaveWireState& theState)
{
	if (theBuffer.Remaining() < NET_DAVE_STATE_BYTES)
		return false;
	theState.mState = theBuffer.ReadU8Or();
	theState.mAnimKey = theBuffer.ReadU8Or();
	theState.mMessageIndex = theBuffer.ReadI32Or();
	theState.mAnimTimeQ = theBuffer.ReadU16Or();
	theState.mX = theBuffer.ReadI16Or();
	theState.mY = theBuffer.ReadI16Or();
	return NetValidateDaveWireState(theState);
}

void NetApplyDavePresentation(Board* theBoard, LawnApp* anApp, const NetDaveWireState& theState)
{
	if (theState.mState == (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF)
	{
		theBoard->NetCleanupCrazyDavePresentation();
		return;
	}

	Reanimation* aDaveReanim = anApp->ReanimationTryToGet(anApp->mCrazyDaveReanimID);
	if (!aDaveReanim || aDaveReanim->mDead)
	{
		theBoard->NetCleanupCrazyDavePresentation();
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CRAZY_DAVE, true);
		aDaveReanim = anApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
		if (aDaveReanim)
		{
			aDaveReanim->mIsAttachment = true;
			aDaveReanim->SetBasePoseFromAnim("anim_idle_handing");
			anApp->mCrazyDaveReanimID = anApp->ReanimationGetID(aDaveReanim);
			anApp->mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
			anApp->mCrazyDaveMessageIndex = -1;
			anApp->mCrazyDaveMessageText.clear();
			anApp->mCrazyDaveBlinkCounter = INT_MAX;
			if (anApp->IsStormyNightLevel())
				aDaveReanim->mColorOverride = Color(64, 64, 64);
			NetTrace("DAVE_STATE materialized client root=%u", (unsigned)anApp->mCrazyDaveReanimID);
		}
	}

	if (!aDaveReanim)
		return;

	if (theBoard->mNetDaveAppliedMessageIndex != theState.mMessageIndex)
	{
		if (theState.mMessageIndex >= 0)
		{
			// CrazyDaveTalkIndex may create a hand prop and local foley, so it is
			// strictly edge-triggered and never advances dialog authority here.
			SexyString aDaveMessage = anApp->GetCrazyDaveText(theState.mMessageIndex);
			const bool aReplacesHandProp =
				aDaveMessage.find(_S("{SHOW_WALLNUT}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_HAMMER}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_FERTILIZER}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_TREE_FOOD}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_MONEYBAG}")) != SexyString::npos;
			if (aReplacesHandProp && aDaveReanim->TrackExists("Dave_handinghand"))
			{
				ReanimatorTrackInstance* aHandTrack =
					aDaveReanim->GetTrackInstanceByName("Dave_handinghand");
				if (aHandTrack)
					AttachmentDie(aHandTrack->mAttachmentID);
			}
			// PlayFoley/PlayFoleyPitch only use gNetSuppressSounds as a
			// host rebroadcast guard.  Mute this synchronous client-side
			// presentation call so the authoritative BOARDEV_FOLEY is
			// heard once, then restore the pre-existing cutscene mute.
			const bool aWasMutedForCutscene = anApp->mMuteSoundsForCutscene;
			anApp->mMuteSoundsForCutscene = true;
			anApp->CrazyDaveTalkIndex(theState.mMessageIndex);
			anApp->mMuteSoundsForCutscene = aWasMutedForCutscene;
		}
		else
		{
			aDaveReanim->SetImageOverride("Dave_mouths", nullptr);
			if (theState.mState != (uint8_t)CrazyDaveState::CRAZY_DAVE_HANDING_TALKING &&
				theState.mState != (uint8_t)CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
			{
				if (aDaveReanim->TrackExists("Dave_handinghand"))
				{
					ReanimatorTrackInstance* aHandTrack =
						aDaveReanim->GetTrackInstanceByName("Dave_handinghand");
					if (aHandTrack)
						AttachmentDie(aHandTrack->mAttachmentID);
				}
			}
			anApp->mCrazyDaveMessageIndex = -1;
			anApp->mCrazyDaveMessageText.clear();
			anApp->CrazyDaveStopSound();
		}
		theBoard->mNetDaveAppliedMessageIndex = theState.mMessageIndex;
		NetTrace("DAVE_STATE applied message edge index=%d", theState.mMessageIndex);
	}

	if (theBoard->mNetDaveAppliedAnimKey != theState.mAnimKey)
	{
		const NetDaveAnimSpec& aSpec = gNetDaveAnimSpecs[theState.mAnimKey];
		aDaveReanim->PlayReanim(aSpec.mTrackName, aSpec.mLoopType, 0, aSpec.mAnimRate);
		theBoard->mNetDaveAppliedAnimKey = theState.mAnimKey;
		NetTrace("DAVE_STATE applied anim edge key=%u track=%s", theState.mAnimKey, aSpec.mTrackName);
	}

	const float aDaveAnimTime = theState.mAnimTimeQ / 65535.0f;
	aDaveReanim->mAnimTime = aDaveAnimTime;
	aDaveReanim->mLastFrameTime = aDaveAnimTime;
	aDaveReanim->mLoopCount = 0;
	aDaveReanim->SetPosition((float)theState.mX, (float)theState.mY);
	anApp->mCrazyDaveState = (CrazyDaveState)theState.mState;
}

bool NetGridStateMatchesType(uint8_t theType, uint8_t theState)
{
	switch ((GridItemType)theType)
	{
	case GRIDITEM_GRAVESTONE:		return theState <= GRIDITEM_STATE_GRAVESTONE_SPECIAL;
	case GRIDITEM_CRATER:
	case GRIDITEM_LADDER:
	case GRIDITEM_BRAIN:			return theState == GRIDITEM_STATE_NORMAL;
	case GRIDITEM_PORTAL_CIRCLE:
	case GRIDITEM_PORTAL_SQUARE:	return theState == GRIDITEM_STATE_NORMAL || theState == GRIDITEM_STATE_PORTAL_CLOSED;
	case GRIDITEM_SCARY_POT:		return theState >= GRIDITEM_STATE_SCARY_POT_QUESTION && theState <= GRIDITEM_STATE_SCARY_POT_ZOMBIE;
	case GRIDITEM_SQUIRREL:		return theState >= GRIDITEM_STATE_SQUIRREL_WAITING && theState <= GRIDITEM_STATE_SQUIRREL_ZOMBIE;
	case GRIDITEM_ZEN_TOOL:		return theState >= GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN && theState <= GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN;
	case GRIDITEM_STINKY:			return theState >= GRIDITEM_STINKY_WALKING_LEFT && theState <= GRIDITEM_STINKY_WAKING_UP;
	case GRIDITEM_RAKE:			return theState >= GRIDITEM_STATE_RAKE_ATTRACTING && theState <= GRIDITEM_STATE_RAKE_TRIGGERED;
	case GRIDITEM_IZOMBIE_BRAIN:	return theState == GRIDITEM_STATE_NORMAL || theState == GRIDITEM_STATE_BRAIN_SQUISHED;
	default:						return false;
	}
}

bool NetProjectileFrameIsSafe(uint8_t theType, uint8_t theFrame)
{
	Image* anImage = nullptr;
	switch ((ProjectileType)theType)
	{
	case ProjectileType::PROJECTILE_PEA:
	case ProjectileType::PROJECTILE_ZOMBIE_PEA:
		anImage = IMAGE_PROJECTILEPEA;
		break;
	case ProjectileType::PROJECTILE_SNOWPEA:
		anImage = IMAGE_PROJECTILESNOWPEA;
		break;
	case ProjectileType::PROJECTILE_FIREBALL:
		// FIREBALL is drawn entirely by its REANIM_FIRE_PEA attachment.
		return theFrame == 0;
	case ProjectileType::PROJECTILE_SPIKE:
#ifdef _PIERCING_CACTUS
	case ProjectileType::PROJECTILE_PIERCE_SPIKE:
#endif
		anImage = IMAGE_PROJECTILECACTUS;
		break;
	case ProjectileType::PROJECTILE_STAR:
		anImage = IMAGE_PROJECTILE_STAR;
		break;
	case ProjectileType::PROJECTILE_PUFF:
		anImage = IMAGE_PUFFSHROOM_PUFF1;
		break;
	case ProjectileType::PROJECTILE_BASKETBALL:
		anImage = IMAGE_REANIM_ZOMBIE_CATAPULT_BASKETBALL;
		break;
	case ProjectileType::PROJECTILE_CABBAGE:
		anImage = IMAGE_REANIM_CABBAGEPULT_CABBAGE;
		break;
	case ProjectileType::PROJECTILE_KERNEL:
		anImage = IMAGE_REANIM_CORNPULT_KERNAL;
		break;
	case ProjectileType::PROJECTILE_BUTTER:
		anImage = IMAGE_REANIM_CORNPULT_BUTTER;
		break;
	case ProjectileType::PROJECTILE_MELON:
		anImage = IMAGE_REANIM_MELONPULT_MELON;
		break;
	case ProjectileType::PROJECTILE_WINTERMELON:
		anImage = IMAGE_REANIM_WINTERMELON_PROJECTILE;
		break;
	case ProjectileType::PROJECTILE_COBBIG:
		anImage = IMAGE_REANIM_COBCANNON_COB;
		break;
	default:
		anImage = IMAGE_PROJECTILE;
		break;
	}
	return anImage && theFrame < anImage->mNumCols;
}

void NetEnsureGridItemVisual(GridItem* theGridItem, GridItemState thePreviousState)
{
	Reanimation* aReanim = theGridItem->mApp->ReanimationTryToGet(theGridItem->mGridItemReanimID);
	bool aCreatedVisual = false;

	if (theGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
		theGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE)
	{
		if (!aReanim)
		{
			theGridItem->OpenPortal();
			aReanim = theGridItem->mApp->ReanimationTryToGet(theGridItem->mGridItemReanimID);
			aCreatedVisual = true;
		}
		if (theGridItem->mGridItemState == GridItemState::GRIDITEM_STATE_PORTAL_CLOSED &&
			(aCreatedVisual || thePreviousState != theGridItem->mGridItemState))
		{
			theGridItem->ClosePortal();
		}
		else if (!aCreatedVisual && thePreviousState == GridItemState::GRIDITEM_STATE_PORTAL_CLOSED &&
			theGridItem->mGridItemState == GridItemState::GRIDITEM_STATE_NORMAL)
		{
			theGridItem->OpenPortal();
		}
		return;
	}

	if (theGridItem->mGridItemType == GridItemType::GRIDITEM_RAKE)
	{
		if (!aReanim)
		{
			aReanim = theGridItem->mBoard->CreateRakeReanim(
				(float)theGridItem->mGridX, (float)theGridItem->mGridY, 0);
			theGridItem->mGridItemReanimID = theGridItem->mApp->ReanimationGetID(aReanim);
		}
		if (aReanim)
			aReanim->mAnimRate = theGridItem->mGridItemState == GridItemState::GRIDITEM_STATE_RAKE_TRIGGERED ? 20.0f : 0.0f;
		return;
	}

	if (theGridItem->mGridItemType != GridItemType::GRIDITEM_STINKY)
		return;

	if (!aReanim)
	{
		aReanim = theGridItem->mApp->AddReanimation(
			theGridItem->mPosX, theGridItem->mPosY, 0, ReanimationType::REANIM_STINKY);
		theGridItem->mGridItemReanimID = theGridItem->mApp->ReanimationGetID(aReanim);
		aCreatedVisual = true;
	}
	if (!aReanim)
		return;

	if (aCreatedVisual || thePreviousState != theGridItem->mGridItemState)
	{
		theGridItem->mMotionTrailCount = 0;
		switch (theGridItem->mGridItemState)
		{
		case GridItemState::GRIDITEM_STINKY_WALKING_LEFT:
		case GridItemState::GRIDITEM_STINKY_WALKING_RIGHT:
			aReanim->PlayReanim("anim_crawl", ReanimLoopType::REANIM_LOOP, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_TURNING_LEFT:
		case GridItemState::GRIDITEM_STINKY_TURNING_RIGHT:
			aReanim->PlayReanim("turn", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_FALLING_ASLEEP:
			aReanim->PlayReanim("anim_in", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_WAKING_UP:
			aReanim->PlayReanim("anim_out", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_SLEEPING:
			aReanim->PlayReanim("anim_out", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);
			break;
		default:
			break;
		}
	}

	if (theGridItem->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT ||
		theGridItem->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_LEFT)
	{
		aReanim->OverrideScale(-0.8f, 0.8f);
		aReanim->SetPosition(theGridItem->mPosX + 69.0f, theGridItem->mPosY);
	}
	else
	{
		aReanim->OverrideScale(0.8f, 0.8f);
		aReanim->SetPosition(theGridItem->mPosX, theGridItem->mPosY);
	}
}
}

void Board::NetCleanupCrazyDavePresentation()
{
	Reanimation* aBlinkReanim = mApp->ReanimationTryToGet(mApp->mCrazyDaveBlinkReanimID);
	if (aBlinkReanim)
		aBlinkReanim->ReanimationDie();

	Reanimation* aDaveReanim = mApp->ReanimationTryToGet(mApp->mCrazyDaveReanimID);
	if (aDaveReanim)
		aDaveReanim->ReanimationDie(); // also owns every hand/body attachment

	const bool aHadDavePresentation = aBlinkReanim || aDaveReanim ||
		mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF ||
		mApp->mCrazyDaveMessageIndex != -1 || !mApp->mCrazyDaveMessageText.empty();
	mApp->mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mApp->mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mApp->mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mApp->mCrazyDaveBlinkCounter = 0;
	mApp->mCrazyDaveMessageIndex = -1;
	mApp->mCrazyDaveMessageText.clear();
	if (aHadDavePresentation)
		mApp->CrazyDaveStopSound();

	mNetDaveAppliedAnimKey = 255;
	mNetDaveAppliedMessageIndex = INT_MIN;
}

void Board::NetEncodeDaveState(NetBuffer& theBuffer)
{
	NetWriteDaveWireState(theBuffer, NetCaptureDaveWireState(mApp));
}

void Board::NetApplyDaveState(NetBuffer& theBuffer)
{
	if (theBuffer.Remaining() != NET_DAVE_STATE_BYTES)
	{
		NetTrace("DAVE_STATE rejected: bytes=%zu expected=%zu",
			theBuffer.Remaining(), NET_DAVE_STATE_BYTES);
		return;
	}

	NetDaveWireState aState = {};
	if (!NetReadDaveWireState(theBuffer, aState) || theBuffer.Remaining() != 0)
	{
		NetTrace("DAVE_STATE rejected: state=(%u,%u,%d,%u,%d,%d) remaining=%zu",
			aState.mState, aState.mAnimKey, aState.mMessageIndex, aState.mAnimTimeQ,
			aState.mX, aState.mY, theBuffer.Remaining());
		return;
	}

	NetApplyDavePresentation(this, mApp, aState);
}

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
	// Zero is the wire sentinel for "no authoritative identity".  Endless
	// sessions can wrap the 16-bit counter, so also avoid every still-live id.
	auto IsLiveId = [&](uint16_t theId)
	{
		Plant* aPlant = nullptr;
		while (mPlants.IterateNext(aPlant)) if (!aPlant->mDead && aPlant->mNetId == theId) return true;
		Zombie* aZombie = nullptr;
		while (mZombies.IterateNext(aZombie)) if (!aZombie->mDead && aZombie->mNetId == theId) return true;
		Projectile* aProjectile = nullptr;
		while (mProjectiles.IterateNext(aProjectile)) if (!aProjectile->mDead && aProjectile->mNetId == theId) return true;
		Coin* aCoin = nullptr;
		while (mCoins.IterateNext(aCoin)) if (!aCoin->mDead && aCoin->mNetId == theId) return true;
		LawnMower* aMower = nullptr;
		while (mLawnMowers.IterateNext(aMower)) if (!aMower->mDead && aMower->mNetId == theId) return true;
		GridItem* aGridItem = nullptr;
		while (mGridItems.IterateNext(aGridItem)) if (!aGridItem->mDead && aGridItem->mNetId == theId) return true;
		return false;
	};

	for (uint32_t anAttempt = 0; anAttempt < 0xFFFFU; ++anAttempt)
	{
		do
		{
			++mNextNetId;
		} while (mNextNetId == 0);
		if (!IsLiveId(mNextNetId))
			return mNextNetId;
	}
	NetTrace("fatal: exhausted all authoritative entity ids");
	return 0;
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
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			NetBuffer anIntro;
			NetEncodeIntroState(anIntro);
			aNet->BroadcastMessage(NETMSG_INTRO_STATE, anIntro);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
		{
			NetBuffer aSnap;
			NetEncodeSnapshot(aSnap);
			aNet->SendSnapshot(aSnap);

			// Dave can enter after the intro (Scary Potter, Zen Garden and Tree of
			// Wisdom).  Keep his presentation separate from the large snapshot and
			// send canonical OFF as well so clients remove him at dialog end.
			NetBuffer aDaveState;
			NetEncodeDaveState(aDaveState);
			aNet->BroadcastMessage(NETMSG_DAVE_STATE, aDaveState);
		}
	}

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
	// -1 means that no fade is active.  Zero is an active, fully opaque fade,
	// so clamping the sentinel to zero covered every client with a white quad.
	b.WriteU16((uint16_t)min(65535, max(0, mBoardFadeOutCounter + 1)));
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
		b.WriteI16((int16_t)max(0, min(1024, aPacket.mOffsetX)));
		b.WriteU16((uint16_t)max(0, min(SLOT_MACHINE_TIME, aPacket.mSlotMachineCountDown)));
		uint8_t aNextSlotSeed = (uint8_t)aPacket.mSlotMachiningNextSeed;
		b.WriteU8(NetIsValidSlotMachineSeed(aNextSlotSeed) ? aNextSlotSeed : 255);
		float aSlotPosition = aPacket.mSlotMachiningPosition;
		if (!_finite(aSlotPosition) || aSlotPosition < 0.0f || aSlotPosition > 1.0f)
			aSlotPosition = 0.0f;
		b.WriteF32(aSlotPosition);
	}

	// ---- plants ----
	uint16_t aCount = 0;
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
	b.WriteU16(aCount);
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
		// Zombie drawing uses -1 for fully visible and 0 for fully transparent.
		aBody.WriteU8(aZombie->mZombieFade < 0 ? 255 : (uint8_t)min(254, aZombie->mZombieFade));
		aBody.WriteU8((uint8_t)min(255, max(0, aZombie->mJustGotShotCounter)));
		aBody.WriteF32(aZombie->mVelX);
		aBody.WriteF32(aZombie->mAltitude);
		aBody.WriteF32(aZombie->mOriginalAnimRate > 0 ? aZombie->mOriginalAnimRate : 12.0f);
		uint8_t anAnimKey = aZombie->mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ? 2 :
			(aZombie->mIsEating ? 1 : 0);
		aBody.WriteU8(anAnimKey);
		aCount++;
	}
	b.WriteU16(aCount);
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
                aBody.WriteF32(aProjectile->mPosX);
                aBody.WriteF32(aProjectile->mPosY);
                aBody.WriteF32(aProjectile->mPosZ);
                aBody.WriteF32(aProjectile->mShadowY);
                aBody.WriteI32(aProjectile->mRenderOrder);
                aBody.WriteF32(aProjectile->mRotation);
                aBody.WriteU8((uint8_t)aProjectile->mMotionType);
                aBody.WriteU8((uint8_t)max(0, min(255, aProjectile->mFrame)));
                aBody.WriteU8(aProjectile->mOnHighGround ? 1 : 0);
                aCount++;
        }
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_PROJECTILE_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- coins ----
	aCount = 0;
	aBody.Clear();
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mNetId == 0)
			aCoin->mNetId = NetAllocId();
                aBody.WriteU16(aCoin->mNetId);
                aBody.WriteU8((uint8_t)aCoin->mType);
                aBody.WriteU8((uint8_t)aCoin->mCoinMotion);
                aBody.WriteF32(aCoin->mPosX);
                aBody.WriteF32(aCoin->mPosY);
                aBody.WriteI32(aCoin->mRenderOrder);
                aBody.WriteU16((uint16_t)min(16384, max(0, (int)(aCoin->mScale * 4096.0f + 0.5f))));
                aBody.WriteU16((uint16_t)min(8192, max(0, (int)(aCoin->mCollectionDistance * 4.0f + 0.5f))));
                uint8_t aCFlags = 0;
                if (aCoin->mIsBeingCollected) aCFlags |= 1;
                if (aCoin->mHitGround) aCFlags |= 2;
                aBody.WriteU8(aCFlags);
                aBody.WriteU8((uint8_t)aCoin->mUsableSeedType);
                aBody.WriteU8((uint8_t)min(255, max(0, aCoin->mFadeCount)));
                aBody.WriteU16((uint16_t)min(65535, max(0, aCoin->mDisappearCounter)));
                const bool aPottedPresent = aCoin->mType == CoinType::COIN_PRESENT_PLANT ||
                        aCoin->mType == CoinType::COIN_AWARD_PRESENT;
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mSeedType : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mWhichZenGarden : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mFacing : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mDrawVariation : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mPlantAge : 255);
                aCount++;
        }
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_COIN_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- lawn mowers ----
	aCount = 0;
	aBody.Clear();
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
	{
		if (aMower->mDead)
			continue;
		if (aMower->mNetId == 0)
			aMower->mNetId = NetAllocId();
		aBody.WriteU16(aMower->mNetId);
		aBody.WriteU8((uint8_t)aMower->mRow);
		aBody.WriteU8((uint8_t)aMower->mMowerState);
		aBody.WriteU8((uint8_t)aMower->mMowerType);
		aBody.WriteU8(aMower->mVisible ? 1 : 0);
		aBody.WriteI16((int16_t)(aMower->mPosX * 4));
		aBody.WriteF32(aMower->mAltitude);
		aBody.WriteU8((uint8_t)aMower->mMowerHeight);
		aCount++;
	}
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- grid items ----
	aCount = 0;
	aBody.Clear();
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
	{
		if (aGridItem->mDead)
			continue;
		// Several stock modes (notably Vasebreaker) allocate grid items directly,
		// bypassing the helpers that normally assign network identities.
		if (aGridItem->mNetId == 0)
			aGridItem->mNetId = NetAllocId();
		aBody.WriteU16(aGridItem->mNetId);
		aBody.WriteU8((uint8_t)aGridItem->mGridItemType);
		aBody.WriteU8((uint8_t)aGridItem->mGridItemState);
		aBody.WriteU8((uint8_t)aGridItem->mGridX);
		aBody.WriteU8((uint8_t)aGridItem->mGridY);
		aBody.WriteI32(aGridItem->mGridItemCounter);
		aBody.WriteI32(aGridItem->mRenderOrder);
		aBody.WriteF32(aGridItem->mPosX);
		aBody.WriteF32(aGridItem->mPosY);
		aBody.WriteF32(aGridItem->mGoalX);
		aBody.WriteF32(aGridItem->mGoalY);
		aBody.WriteU8(aGridItem->mZombieType == ZombieType::ZOMBIE_INVALID ? 255 : (uint8_t)aGridItem->mZombieType);
		aBody.WriteU8(aGridItem->mSeedType == SeedType::SEED_NONE ? 255 : (uint8_t)aGridItem->mSeedType);
		aBody.WriteU8((uint8_t)aGridItem->mScaryPotType);
		aBody.WriteU16((uint16_t)min(65535, max(0, aGridItem->mTransparentCounter)));
		aBody.WriteU8((uint8_t)min(255, max(0, aGridItem->mSunCount)));
		aBody.WriteU8(aGridItem->mHighlighted ? 1 : 0);
		aCount++;
	}
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());
}

// ==========================================================================
// Client: message drain + ghost management
// ==========================================================================

Plant* Board::NetGetGhostPlant(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
		if (aPlant->mNetGhost && aPlant->mNetId == theId && !aPlant->mDead)
			return aPlant;
	return nullptr;
}

Zombie* Board::NetGetGhostZombie(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
		if (aZombie->mNetGhost && aZombie->mNetId == theId && !aZombie->mDead)
			return aZombie;
	return nullptr;
}

Projectile* Board::NetGetGhostProjectile(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
		if (aProjectile->mNetGhost && aProjectile->mNetId == theId && !aProjectile->mDead)
			return aProjectile;
	return nullptr;
}

Coin* Board::NetGetGhostCoin(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
		if (aCoin->mNetGhost && aCoin->mNetId == theId && !aCoin->mDead)
			return aCoin;
	return nullptr;
}

GridItem* Board::NetGetGhostGridItem(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
		if (aGridItem->mNetGhost && aGridItem->mNetId == theId && !aGridItem->mDead)
			return aGridItem;
	return nullptr;
}

LawnMower* Board::NetGetGhostMower(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
		if (aMower->mNetGhost && aMower->mNetId == theId && !aMower->mDead)
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
		SeedType anImitaterType = anImitater == 255 ? SeedType::SEED_NONE : (SeedType)anImitater;
		if (anId == 0 || aType >= (uint8_t)SeedType::NUM_SEED_TYPES ||
			(anImitater != 255 && anImitater >= (uint8_t)SeedType::NUM_SEED_TYPES) ||
			aRow >= MAX_GRID_SIZE_Y || aCol >= MAX_GRID_SIZE_X)
		{
			NetTrace("PLANT_ADDED rejected id=%u type=%u imitater=%u cell=(%u,%u)", anId, aType, anImitater, aCol, aRow);
			break;
		}
		if (!NetGetGhostPlant(anId))
		{
			gNetSuppressSounds = true;
			Plant* aPlant = NewPlant(aCol, aRow, (SeedType)aType, anImitaterType);
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
		if (b.Remaining() != 16)
		{
			NetTrace("SUN_ADDED rejected: invalid payload size=%zu expected=16", b.Remaining());
			break;
		}
		uint16_t anId = b.ReadU16Or();
		uint8_t aCoinType = b.ReadU8Or();
		uint8_t aMotion = b.ReadU8Or();
		int aX = b.ReadI16Or() / 4;
		int aY = b.ReadI16Or() / 4;
		uint16_t aValue = b.ReadU16Or();
		uint8_t aUsableSeed = b.ReadU8Or();
		uint8_t aPottedSeed = b.ReadU8Or();
		uint8_t aPottedGarden = b.ReadU8Or();
		uint8_t aPottedFacing = b.ReadU8Or();
		uint8_t aPottedVariation = b.ReadU8Or();
		uint8_t aPottedAge = b.ReadU8Or();
		bool aUsablePacket = aCoinType == (uint8_t)CoinType::COIN_USABLE_SEED_PACKET;
		bool aPottedPresent = aCoinType == (uint8_t)CoinType::COIN_PRESENT_PLANT ||
			aCoinType == (uint8_t)CoinType::COIN_AWARD_PRESENT;
		bool aValidPottedVisual = aPottedPresent ?
			(aPottedSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
			 aPottedGarden <= (uint8_t)GardenType::GARDEN_AQUARIUM &&
			 aPottedFacing <= (uint8_t)PottedPlant::FacingDirection::FACING_LEFT &&
			 aPottedVariation < (uint8_t)DrawVariation::NUM_VARIATIONS &&
			 aPottedAge <= (uint8_t)PottedPlantAge::PLANTAGE_FULL) :
			(aPottedSeed == 255 && aPottedGarden == 255 && aPottedFacing == 255 &&
			 aPottedVariation == 255 && aPottedAge == 255);
		if (anId == 0 || aCoinType < (uint8_t)CoinType::COIN_SILVER ||
			aCoinType > (uint8_t)CoinType::COIN_PRESENT_SURVIVAL_MODE ||
			aMotion > (uint8_t)CoinMotion::COIN_MOTION_FROM_BOSS ||
			(aUsablePacket && aUsableSeed >= (uint8_t)SeedType::NUM_SEED_TYPES) ||
			(!aUsablePacket && aUsableSeed != 255) || !aValidPottedVisual)
		{
			NetTrace("SUN_ADDED rejected: id=%u coinType=%u seed=%u potted=(%u,%u,%u,%u,%u) value=%u",
				anId, aCoinType, aUsableSeed, aPottedSeed, aPottedGarden,
				aPottedFacing, aPottedVariation, aPottedAge, aValue);
			break;
		}
		PottedPlant aPottedVisual = {};
		const PottedPlant* aPottedVisualPtr = nullptr;
		if (aPottedPresent)
		{
			aPottedVisual.mSeedType = (SeedType)aPottedSeed;
			aPottedVisual.mWhichZenGarden = (GardenType)aPottedGarden;
			aPottedVisual.mFacing = (PottedPlant::FacingDirection)aPottedFacing;
			aPottedVisual.mDrawVariation = (DrawVariation)aPottedVariation;
			aPottedVisual.mPlantAge = (PottedPlantAge)aPottedAge;
			aPottedVisualPtr = &aPottedVisual;
		}
		Coin* aCoin = NetGetGhostCoin(anId);
		if (aCoin && aCoin->mType != (CoinType)aCoinType)
		{
			NetTrace("SUN_ADDED rejected: id=%u changed coin type old=%d new=%u", anId, (int)aCoin->mType, aCoinType);
			break;
		}
		if (!aCoin)
		{
			aCoin = mCoins.DataArrayAlloc();
			if (aCoin)
			{
				aCoin->CoinInitializeNetGhost(aX, aY, (CoinType)aCoinType,
					(CoinMotion)aMotion, aPottedVisualPtr);
				aCoin->mUsableSeedType = aUsableSeed == 255 ? SeedType::SEED_NONE : (SeedType)aUsableSeed;
			}
		}
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
			aCoin->mCoinMotion = (CoinMotion)aMotion;
			aCoin->mUsableSeedType = aUsableSeed == 255 ? SeedType::SEED_NONE : (SeedType)aUsableSeed;
			if (aPottedPresent)
			{
				aCoin->mPottedPlantSpec.mSeedType = aPottedVisual.mSeedType;
				aCoin->mPottedPlantSpec.mWhichZenGarden = aPottedVisual.mWhichZenGarden;
				aCoin->mPottedPlantSpec.mFacing = aPottedVisual.mFacing;
				aCoin->mPottedPlantSpec.mDrawVariation = aPottedVisual.mDrawVariation;
				aCoin->mPottedPlantSpec.mPlantAge = aPottedVisual.mPlantAge;
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
	mBoardFadeOutCounter = (int)b.ReadU16Or() - 1;
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
	if (b.Remaining() < 1)
	{
		NetTrace("snapshot seed section truncated before count");
		return;
	}
	uint8_t aNumPackets = b.ReadU8Or();
	if (!mSeedBank || aNumPackets > SEEDBANK_CAPACITY ||
		b.Remaining() < (size_t)aNumPackets * NET_SEED_PACKET_WIRE_BYTES)
	{
		NetTrace("snapshot seed section rejected count=%u remaining=%zu bank=%d",
			aNumPackets, b.Remaining(), mSeedBank ? 1 : 0);
		return;
	}

	struct NetSeedPacketWire
	{
		uint8_t mType;
		uint8_t mImitater;
		uint8_t mCooldown;
		uint8_t mOwner;
		uint8_t mActive;
		int16_t mOffsetX;
		uint16_t mSlotCountdown;
		uint8_t mSlotNextSeed;
		float mSlotPosition;
	};
	std::vector<NetSeedPacketWire> aSeedPackets;
	aSeedPackets.reserve(aNumPackets);
	for (int i = 0; i < aNumPackets; i++)
	{
		NetSeedPacketWire aWire = {};
		aWire.mType = b.ReadU8Or();
		aWire.mImitater = b.ReadU8Or();
		aWire.mCooldown = b.ReadU8Or();
		aWire.mOwner = b.ReadU8Or();
		aWire.mActive = b.ReadU8Or();
		aWire.mOffsetX = b.ReadI16Or();
		aWire.mSlotCountdown = b.ReadU16Or();
		aWire.mSlotNextSeed = b.ReadU8Or();
		aWire.mSlotPosition = b.ReadF32Or();

		bool aTypeValid = NetIsValidSeedPacketType(aWire.mType);
		bool anImitaterValid = aWire.mImitater == 255 ||
			aWire.mImitater < (uint8_t)SeedType::NUM_SEED_TYPES;
		bool anOwnerValid = aWire.mOwner == LawnNet::NET_SHARED_OWNER ||
			aWire.mOwner < LawnNet::NET_MAX_PLAYERS;
		if (!aTypeValid || !anImitaterValid || !anOwnerValid ||
			aWire.mActive > 1 || aWire.mOffsetX < 0 || aWire.mOffsetX > 1024 ||
			aWire.mSlotCountdown > SLOT_MACHINE_TIME ||
			!NetIsValidSlotMachineSeed(aWire.mSlotNextSeed) ||
			!_finite(aWire.mSlotPosition) ||
			aWire.mSlotPosition < 0.0f || aWire.mSlotPosition > 1.0f)
		{
			NetTrace("snapshot seed rejected index=%d type=%u imitater=%u owner=%u active=%u offset=%d slot=(%u,%u,%.3f)",
				i, aWire.mType, aWire.mImitater, aWire.mOwner, aWire.mActive,
				aWire.mOffsetX, aWire.mSlotCountdown, aWire.mSlotNextSeed, aWire.mSlotPosition);
			return;
		}
		aSeedPackets.push_back(aWire);
	}

	// Apply only after the complete seed section has passed structural and value
	// validation; a malformed packet cannot leave a partially updated seed bank.
	for (int i = 0; i < aNumPackets; i++)
	{
		const NetSeedPacketWire& aWire = aSeedPackets[i];
		SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
		SeedType aType = aWire.mType == 255 ? SeedType::SEED_NONE : (SeedType)aWire.mType;
		SeedType anImitater = aWire.mImitater == 255 ? SeedType::SEED_NONE : (SeedType)aWire.mImitater;
		if (aPacket.mPacketType != aType || aPacket.mImitaterType != anImitater)
		{
			// SetPacketType queries plant definitions.  Empty and challenge-only
			// packet ids deliberately live outside that table and use a raw reset.
			if (aType == SeedType::SEED_NONE ||
				(int)aType >= (int)SeedType::NUM_SEED_TYPES)
			{
				aPacket.mPacketType = aType;
				aPacket.mImitaterType = anImitater;
				aPacket.mRefreshCounter = 0;
				aPacket.mRefreshTime = 0;
				aPacket.mRefreshing = false;
				aPacket.mActive = true;
			}
			else
			{
				aPacket.SetPacketType(aType, anImitater);
			}
		}
		if (aPacket.mRefreshTime > 0)
			aPacket.mRefreshCounter = aWire.mCooldown * aPacket.mRefreshTime / 255;
		aPacket.mNetOwnerSlot = aWire.mOwner;
		aPacket.mActive = aWire.mActive != 0;
		aPacket.mOffsetX = aWire.mOffsetX;
		aPacket.mSlotMachineCountDown = aWire.mSlotCountdown;
		aPacket.mSlotMachiningNextSeed = aWire.mSlotNextSeed == 255 ?
			SeedType::SEED_NONE : (SeedType)aWire.mSlotNextSeed;
		aPacket.mSlotMachiningPosition = aWire.mSlotPosition;
	}
	mSeedBank->mNumPackets = aNumPackets;

	// ---- plants ----
	uint16_t aCount = b.ReadU16Or();
	std::map<unsigned short, bool> aSeenPlants;
	for (int i = 0; i < aCount; i++)
	{
		if (b.Remaining() < 12)
		{
			NetTrace("snapshot plant section truncated index=%d count=%u remaining=%zu", i, aCount, b.Remaining());
			return;
		}
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t anImitater = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aCol = b.ReadU8Or();
		uint16_t aHealth = b.ReadU16Or();
		uint16_t aMaxHealth = b.ReadU16Or();
		uint8_t aState = b.ReadU8Or();
		uint8_t aPFlags = b.ReadU8Or();
		SeedType anImitaterType = anImitater == 255 ? SeedType::SEED_NONE : (SeedType)anImitater;
		if (anId == 0 || aType >= (uint8_t)SeedType::NUM_SEED_TYPES ||
			(anImitater != 255 && anImitater >= (uint8_t)SeedType::NUM_SEED_TYPES) ||
			aRow >= MAX_GRID_SIZE_Y || aCol >= MAX_GRID_SIZE_X || (aPFlags & ~15) != 0 ||
			aSeenPlants.find(anId) != aSeenPlants.end())
		{
			NetTrace("snapshot plant rejected id=%u type=%u imitater=%u cell=(%u,%u) flags=%u",
				anId, aType, anImitater, aCol, aRow, aPFlags);
			continue;
		}
		aSeenPlants[anId] = true;

		Plant* aPlant = NetGetGhostPlant(anId);
		if (!aPlant)
		{
			// Intro/challenge setup can create the same stock plant locally before
			// the first authoritative snapshot.  Adopt it instead of drawing two
			// plants in the same cell.
			Plant* aLocalPlant = nullptr;
			while (mPlants.IterateNext(aLocalPlant))
			{
				if (!aLocalPlant->mDead && !aLocalPlant->mNetGhost &&
					aLocalPlant->mRow == aRow && aLocalPlant->mPlantCol == aCol &&
					aLocalPlant->mSeedType == (SeedType)aType &&
					aLocalPlant->mImitaterType == anImitaterType)
				{
					aPlant = aLocalPlant;
					break;
				}
			}
		}
		if (!aPlant)
		{
			gNetSuppressSounds = true;
			aPlant = NewPlant(aCol, aRow, (SeedType)aType, anImitaterType);
			gNetSuppressSounds = false;
			if (!aPlant)
				continue;
		}
		aPlant->mNetId = anId;
		aPlant->mNetGhost = true;
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
			if (!aPlant->mDead &&
				((aPlant->mNetGhost && aSeenPlants.find(aPlant->mNetId) == aSeenPlants.end()) || !aPlant->mNetGhost))
				aPlant->Die();
	}

	// ---- zombies ----
	aCount = b.ReadU16Or();
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
		aZombie->mZombieFade = aFade == 255 ? -1 : aFade;
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
	if (b.Remaining() < 2)
	{
		NetTrace("snapshot projectile section missing count bytes=%zu", b.Remaining());
		return;
	}
	aCount = b.ReadU16Or();
	if (aCount > 1024 || (size_t)aCount > b.Remaining() / NET_PROJECTILE_WIRE_BYTES)
	{
		NetTrace("snapshot projectile section truncated count=%u remaining=%zu record=%zu",
			aCount, b.Remaining(), NET_PROJECTILE_WIRE_BYTES);
		return;
	}
	struct NetProjectileWire
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mRow;
		float mPosX;
		float mPosY;
		float mPosZ;
		float mShadowY;
		int32_t mRenderOrder;
		float mRotation;
		uint8_t mMotion;
		uint8_t mFrame;
		uint8_t mFlags;
	};
	std::vector<NetProjectileWire> aProjectileWires;
	aProjectileWires.reserve(aCount);
	std::map<unsigned short, bool> aSeenProjectiles;
	for (int i = 0; i < aCount; i++)
	{
		NetProjectileWire aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mRow = b.ReadU8Or();
		aWire.mPosX = b.ReadF32Or();
		aWire.mPosY = b.ReadF32Or();
		aWire.mPosZ = b.ReadF32Or();
		aWire.mShadowY = b.ReadF32Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mRotation = b.ReadF32Or();
		aWire.mMotion = b.ReadU8Or();
		aWire.mFrame = b.ReadU8Or();
		aWire.mFlags = b.ReadU8Or();

		const bool aFrameValid = NetProjectileFrameIsSafe(aWire.mType, aWire.mFrame);
		const bool aValid = aWire.mId != 0 &&
			aWire.mType < (uint8_t)ProjectileType::NUM_PROJECTILES &&
			aWire.mRow < MAX_GRID_SIZE_Y &&
			aWire.mMotion <= (uint8_t)ProjectileMotion::MOTION_HOMING &&
			(aWire.mFlags & ~1) == 0 && aFrameValid &&
			_finite(aWire.mPosX) && _finite(aWire.mPosY) &&
			_finite(aWire.mPosZ) && _finite(aWire.mShadowY) && _finite(aWire.mRotation) &&
			aWire.mPosX >= -1024.0f && aWire.mPosX <= 2048.0f &&
			aWire.mPosY >= -1024.0f && aWire.mPosY <= 2048.0f &&
			aWire.mPosZ >= -4096.0f && aWire.mPosZ <= 4096.0f &&
			aWire.mShadowY >= -1024.0f && aWire.mShadowY <= 2048.0f &&
			aWire.mRotation >= -100000.0f && aWire.mRotation <= 100000.0f &&
			aWire.mRenderOrder >= -1000000 && aWire.mRenderOrder <= 2000000 &&
			aSeenProjectiles.find(aWire.mId) == aSeenProjectiles.end();
		if (!aValid)
		{
			NetTrace("snapshot projectile section rejected index=%d id=%u type=%u row=%u motion=%u frame=%u flags=%u",
				i, aWire.mId, aWire.mType, aWire.mRow, aWire.mMotion, aWire.mFrame, aWire.mFlags);
			return;
		}
		aSeenProjectiles[aWire.mId] = true;
		aProjectileWires.push_back(aWire);
	}

	// The entire variable-length section is valid before any projectile is
	// created, replaced, moved, or deleted.
	for (const NetProjectileWire& aWire : aProjectileWires)
	{
		Projectile* aProjectile = NetGetGhostProjectile(aWire.mId);
		if (aProjectile && aProjectile->mProjectileType != (ProjectileType)aWire.mType)
		{
			// Torchwood conversion changes PEA into FIREBALL without changing the
			// network identity.  Recreate the presentation object so FIREBALL gets
			// its required REANIM_FIRE_PEA attachment and the old trail dies once.
			aProjectile->Die();
			aProjectile = nullptr;
		}
		bool aCreated = false;
		if (!aProjectile)
		{
			aProjectile = mProjectiles.DataArrayAlloc();
			if (aProjectile)
				aProjectile->ProjectileInitializeNetGhost((int)aWire.mPosX, (int)aWire.mPosY,
					aWire.mRenderOrder, aWire.mRow, (ProjectileType)aWire.mType);
			if (!aProjectile)
				continue;
			aCreated = true;
			aProjectile->mNetId = aWire.mId;
			aProjectile->mNetGhost = true;
		}
		aProjectile->mRow = aWire.mRow;
		aProjectile->mNetTargetX = aWire.mPosX;
		aProjectile->mNetTargetY = aWire.mPosY;
		aProjectile->mNetTargetZ = aWire.mPosZ;
		aProjectile->mNetTargetRotation = aWire.mRotation;
		if (aCreated)
		{
			aProjectile->mPosX = aWire.mPosX;
			aProjectile->mPosY = aWire.mPosY;
			aProjectile->mPosZ = aWire.mPosZ;
			aProjectile->mRotation = aWire.mRotation;
			aProjectile->mX = (int)aWire.mPosX;
			aProjectile->mY = (int)(aWire.mPosY + aWire.mPosZ);
		}
		aProjectile->mShadowY = aWire.mShadowY;
		aProjectile->mRenderOrder = aWire.mRenderOrder;
		aProjectile->mMotionType = (ProjectileMotion)aWire.mMotion;
		aProjectile->mFrame = aWire.mFrame;
		aProjectile->mOnHighGround = (aWire.mFlags & 1) != 0;
		if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_FIREBALL)
		{
			const bool aBackwards = aProjectile->mMotionType == ProjectileMotion::MOTION_BACKWARDS;
			Reanimation* aFirePeaReanim = FindReanimAttachment(aProjectile->mAttachmentID);
			if (aFirePeaReanim)
				aFirePeaReanim->OverrideScale(aBackwards ? -1.0f : 1.0f, 1.0f);
			AttachEffect* aFirePeaEffect = FindFirstAttachment(aProjectile->mAttachmentID);
			if (aFirePeaEffect && aFirePeaEffect->mEffectType == EffectType::EFFECT_REANIM)
			{
				aFirePeaEffect->mOffset.m02 = aBackwards ? 55.0f : -25.0f;
				aFirePeaEffect->mOffset.m12 = -25.0f;
			}
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (mProjectiles.IterateNext(aProjectile))
			if (aProjectile->mNetGhost && !aProjectile->mDead && aSeenProjectiles.find(aProjectile->mNetId) == aSeenProjectiles.end())
				aProjectile->Die();
	}

	// ---- coins ----
	if (b.Remaining() < 2)
	{
		NetTrace("snapshot coin section missing count bytes=%zu", b.Remaining());
		return;
	}
	aCount = b.ReadU16Or();
	if (aCount > 1024 || (size_t)aCount > b.Remaining() / NET_COIN_WIRE_BYTES)
	{
		NetTrace("snapshot coin section truncated count=%u remaining=%zu record=%zu",
			aCount, b.Remaining(), NET_COIN_WIRE_BYTES);
		return;
	}
	struct NetCoinWire
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mMotion;
		float mPosX;
		float mPosY;
		int32_t mRenderOrder;
		uint16_t mScaleQ;
		uint16_t mCollectionDistanceQ;
		uint8_t mFlags;
		uint8_t mUsableSeed;
		uint8_t mFadeCount;
		uint16_t mDisappearCounter;
		uint8_t mPottedSeed;
		uint8_t mPottedGarden;
		uint8_t mPottedFacing;
		uint8_t mPottedVariation;
		uint8_t mPottedAge;
	};
	std::vector<NetCoinWire> aCoinWires;
	aCoinWires.reserve(aCount);
	std::map<unsigned short, bool> aSeenCoins;
	for (int i = 0; i < aCount; i++)
	{
		NetCoinWire aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mMotion = b.ReadU8Or();
		aWire.mPosX = b.ReadF32Or();
		aWire.mPosY = b.ReadF32Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mScaleQ = b.ReadU16Or();
		aWire.mCollectionDistanceQ = b.ReadU16Or();
		aWire.mFlags = b.ReadU8Or();
		aWire.mUsableSeed = b.ReadU8Or();
		aWire.mFadeCount = b.ReadU8Or();
		aWire.mDisappearCounter = b.ReadU16Or();
		const bool aUsablePacket = aWire.mType == (uint8_t)CoinType::COIN_USABLE_SEED_PACKET;
		aWire.mPottedSeed = b.ReadU8Or();
		aWire.mPottedGarden = b.ReadU8Or();
		aWire.mPottedFacing = b.ReadU8Or();
		aWire.mPottedVariation = b.ReadU8Or();
		aWire.mPottedAge = b.ReadU8Or();
		const bool aPottedPresent = aWire.mType == (uint8_t)CoinType::COIN_PRESENT_PLANT ||
			aWire.mType == (uint8_t)CoinType::COIN_AWARD_PRESENT;
		const bool aValidPottedVisual = aPottedPresent ?
			(aWire.mPottedSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
			 aWire.mPottedGarden <= (uint8_t)GardenType::GARDEN_AQUARIUM &&
			 aWire.mPottedFacing <= (uint8_t)PottedPlant::FacingDirection::FACING_LEFT &&
			 aWire.mPottedVariation < (uint8_t)DrawVariation::NUM_VARIATIONS &&
			 aWire.mPottedAge <= (uint8_t)PottedPlantAge::PLANTAGE_FULL) :
			(aWire.mPottedSeed == 255 && aWire.mPottedGarden == 255 &&
			 aWire.mPottedFacing == 255 && aWire.mPottedVariation == 255 &&
			 aWire.mPottedAge == 255);
		const bool aValid = aWire.mId != 0 &&
			aWire.mType >= (uint8_t)CoinType::COIN_SILVER &&
			aWire.mType <= (uint8_t)CoinType::COIN_PRESENT_SURVIVAL_MODE &&
			aWire.mMotion <= (uint8_t)CoinMotion::COIN_MOTION_FROM_BOSS &&
			(aWire.mFlags & ~3) == 0 &&
			(aUsablePacket ? aWire.mUsableSeed < (uint8_t)SeedType::NUM_SEED_TYPES : aWire.mUsableSeed == 255) &&
			_finite(aWire.mPosX) && _finite(aWire.mPosY) &&
			aWire.mPosX >= -1024.0f && aWire.mPosX <= 2048.0f &&
			aWire.mPosY >= -1024.0f && aWire.mPosY <= 2048.0f &&
			aWire.mRenderOrder >= -1000000 && aWire.mRenderOrder <= 2000000 &&
			aWire.mScaleQ <= 16384 && aWire.mCollectionDistanceQ <= 8192 &&
			aWire.mFadeCount <= 15 &&
			aValidPottedVisual &&
			aSeenCoins.find(aWire.mId) == aSeenCoins.end();
		if (!aValid)
		{
			NetTrace("snapshot coin section rejected index=%d id=%u type=%u motion=%u flags=%u seed=%u potted=(%u,%u,%u,%u,%u) scale=%u fade=%u",
				i, aWire.mId, aWire.mType, aWire.mMotion, aWire.mFlags,
				aWire.mUsableSeed, aWire.mPottedSeed, aWire.mPottedGarden,
				aWire.mPottedFacing, aWire.mPottedVariation, aWire.mPottedAge,
				aWire.mScaleQ, aWire.mFadeCount);
			return;
		}
		aSeenCoins[aWire.mId] = true;
		aCoinWires.push_back(aWire);
	}

	for (const NetCoinWire& aWire : aCoinWires)
	{
		const bool aPottedPresent = aWire.mType == (uint8_t)CoinType::COIN_PRESENT_PLANT ||
			aWire.mType == (uint8_t)CoinType::COIN_AWARD_PRESENT;
		PottedPlant aPottedVisual = {};
		const PottedPlant* aPottedVisualPtr = nullptr;
		if (aPottedPresent)
		{
			aPottedVisual.mSeedType = (SeedType)aWire.mPottedSeed;
			aPottedVisual.mWhichZenGarden = (GardenType)aWire.mPottedGarden;
			aPottedVisual.mFacing = (PottedPlant::FacingDirection)aWire.mPottedFacing;
			aPottedVisual.mDrawVariation = (DrawVariation)aWire.mPottedVariation;
			aPottedVisual.mPlantAge = (PottedPlantAge)aWire.mPottedAge;
			aPottedVisualPtr = &aPottedVisual;
		}
		Coin* aCoin = NetGetGhostCoin(aWire.mId);
		if (aCoin && aCoin->mType != (CoinType)aWire.mType)
		{
			unsigned int aDataId = mCoins.DataArrayGetID(aCoin);
			if (mCursorObject && mCursorObject->mCoinID == (CoinID)aDataId)
			{
				mCursorObject->mType = SeedType::SEED_NONE;
				mCursorObject->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
				mCursorObject->mCoinID = CoinID::COINID_NULL;
				mApp->SetCursor(CURSOR_POINTER);
			}
			aCoin->Die();
			aCoin = nullptr;
		}
		bool aCreated = false;
		if (!aCoin)
		{
			aCoin = mCoins.DataArrayAlloc();
			if (aCoin)
			{
				// This factory creates stock attachments but never Collects, scores,
				// mutates a profile/board, plays sound, or consumes gameplay RNG.
				aCoin->CoinInitializeNetGhost((int)aWire.mPosX, (int)aWire.mPosY,
					(CoinType)aWire.mType, (CoinMotion)aWire.mMotion, aPottedVisualPtr);
				aCoin->mUsableSeedType = aWire.mUsableSeed == 255 ?
					SeedType::SEED_NONE : (SeedType)aWire.mUsableSeed;
			}
			if (!aCoin)
				continue;
			aCreated = true;
			aCoin->mNetId = aWire.mId;
			aCoin->mNetGhost = true;
		}
		aCoin->mNetTargetX = aWire.mPosX;
		aCoin->mNetTargetY = aWire.mPosY;
		if (aCreated)
		{
			aCoin->mPosX = aWire.mPosX;
			aCoin->mPosY = aWire.mPosY;
		}
		// Coin::Draw uses absolute mPos coordinates; mX/mY must stay at the
		// GameObject origin or the renderer translates the coin twice.
		aCoin->mX = 0;
		aCoin->mY = 0;
		aCoin->mRenderOrder = aWire.mRenderOrder;
		aCoin->mScale = aWire.mScaleQ / 4096.0f;
		aCoin->mCollectionDistance = aWire.mCollectionDistanceQ / 4.0f;
		aCoin->mCoinMotion = (CoinMotion)aWire.mMotion;
		aCoin->mIsBeingCollected = (aWire.mFlags & 1) != 0;
		aCoin->mHitGround = (aWire.mFlags & 2) != 0;
		aCoin->mUsableSeedType = aWire.mUsableSeed == 255 ? SeedType::SEED_NONE : (SeedType)aWire.mUsableSeed;
		if (aPottedPresent)
		{
			aCoin->mPottedPlantSpec.mSeedType = aPottedVisual.mSeedType;
			aCoin->mPottedPlantSpec.mWhichZenGarden = aPottedVisual.mWhichZenGarden;
			aCoin->mPottedPlantSpec.mFacing = aPottedVisual.mFacing;
			aCoin->mPottedPlantSpec.mDrawVariation = aPottedVisual.mDrawVariation;
			aCoin->mPottedPlantSpec.mPlantAge = aPottedVisual.mPlantAge;
		}
		aCoin->mFadeCount = aWire.mFadeCount;
		aCoin->mDisappearCounter = aWire.mDisappearCounter;
	}
	{
		Coin* aCoin = nullptr;
		while (mCoins.IterateNext(aCoin))
			if (aCoin->mNetGhost && !aCoin->mDead && aSeenCoins.find(aCoin->mNetId) == aSeenCoins.end())
			{
				// Once the host omits a coin it is authoritative-dead.  Do not restart
				// StartFade on every snapshot and never let Coin::Update reach ScoreCoin.
				unsigned int aDataId = mCoins.DataArrayGetID(aCoin);
				if (mCursorObject && mCursorObject->mCoinID == (CoinID)aDataId)
				{
					mCursorObject->mType = SeedType::SEED_NONE;
					mCursorObject->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
					mCursorObject->mCoinID = CoinID::COINID_NULL;
					mApp->SetCursor(CURSOR_POINTER);
				}
				aCoin->Die();
			}
	}

	// ---- lawn mowers ----
	aCount = b.ReadU16Or();
	std::map<unsigned short, bool> aSeenMowers;
	for (int i = 0; i < aCount; i++)
	{
		static const size_t MOWER_WIRE_BYTES = 13;
		if (b.Remaining() < MOWER_WIRE_BYTES)
		{
			NetTrace("snapshot mower section truncated index=%d count=%u remaining=%zu", i, aCount, b.Remaining());
			return;
		}
		uint16_t anId = b.ReadU16Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aState = b.ReadU8Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aVisible = b.ReadU8Or();
		int16_t aX = b.ReadI16Or();
		float anAltitude = b.ReadF32Or();
		uint8_t aHeight = b.ReadU8Or();
		if (anId == 0 || aRow >= MAX_GRID_SIZE_Y || aState > (uint8_t)LawnMowerState::MOWER_SQUISHED ||
			aType > (uint8_t)LawnMowerType::LAWNMOWER_ROOF || aVisible > 1 ||
			aHeight > (uint8_t)MowerHeight::MOWER_HEIGHT_UP_TO_LAND || !_finite(anAltitude) ||
			aSeenMowers.find(anId) != aSeenMowers.end())
		{
			NetTrace("snapshot mower rejected id=%u row=%u state=%u type=%u visible=%u height=%u",
				anId, aRow, aState, aType, aVisible, aHeight);
			continue;
		}
		aSeenMowers[anId] = true;

		LawnMower* aMower = NetGetGhostMower(anId);
		if (!aMower)
		{
			LawnMower* aLocalMower = nullptr;
			while (mLawnMowers.IterateNext(aLocalMower))
			{
				if (!aLocalMower->mDead && !aLocalMower->mNetGhost && aLocalMower->mRow == aRow)
				{
					aMower = aLocalMower;
					break;
				}
			}
		}
		if (!aMower)
		{
			gNetSuppressSounds = true;
			aMower = mLawnMowers.DataArrayAlloc();
			if (!aMower)
				continue;
			aMower->LawnMowerInitialize(aRow, (LawnMowerType)aType);
			gNetSuppressSounds = false;
		}
		else if (aMower->mMowerType != (LawnMowerType)aType)
		{
			Reanimation* anOldReanim = mApp->ReanimationTryToGet(aMower->mReanimID);
			if (anOldReanim)
				anOldReanim->ReanimationDie();
			aMower->LawnMowerInitialize(aRow, (LawnMowerType)aType);
		}
		MowerHeight aPreviousHeight = aMower->mMowerHeight;
		aMower->mNetId = anId;
		aMower->mNetGhost = true;
		aMower->mMowerState = (LawnMowerState)aState;
		aMower->mVisible = aVisible != 0;
		aMower->mPosX = aX / 4.0f;
		aMower->mNetTargetX = aX / 4.0f;
		aMower->mAltitude = anAltitude;
		aMower->mMowerHeight = (MowerHeight)aHeight;
		Reanimation* aMowerReanim = mApp->ReanimationTryToGet(aMower->mReanimID);
		if (aMowerReanim)
		{
			if (aMower->mMowerType == LawnMowerType::LAWNMOWER_POOL &&
				aMower->mMowerHeight != aPreviousHeight)
			{
				if (aMower->mMowerHeight == MowerHeight::MOWER_HEIGHT_IN_POOL)
					aMowerReanim->PlayReanim("anim_water", ReanimLoopType::REANIM_LOOP, 0, 35.0f);
				else
					aMowerReanim->PlayReanim("anim_land", ReanimLoopType::REANIM_LOOP, 0, 35.0f);
			}
			if (aMower->mMowerState == LawnMowerState::MOWER_TRIGGERED)
				aMowerReanim->mAnimRate = aMower->mMowerType == LawnMowerType::LAWNMOWER_POOL ? 35.0f : 70.0f;
			else if (aMower->mMowerState == LawnMowerState::MOWER_SQUISHED)
			{
				aMowerReanim->OverrideScale(0.85f, 0.22f);
				aMowerReanim->SetPosition(-11.0f, 65.0f);
			}
			else
				aMowerReanim->mAnimRate = 0.0f;
		}
	}
	{
		LawnMower* aMower = nullptr;
		while (mLawnMowers.IterateNext(aMower))
		{
			if (!aMower->mDead &&
				((aMower->mNetGhost && aSeenMowers.find(aMower->mNetId) == aSeenMowers.end()) || !aMower->mNetGhost))
			{
				Reanimation* aReanim = mApp->ReanimationTryToGet(aMower->mReanimID);
				if (aReanim)
					aReanim->ReanimationDie();
				aMower->mDead = true;
			}
		}
	}

	// ---- grid items ----
	aCount = b.ReadU16Or();
	std::map<unsigned short, bool> aSeenGridItems;
	for (int i = 0; i < aCount; i++)
	{
		static const size_t GRID_ITEM_WIRE_BYTES = 37;
		if (b.Remaining() < GRID_ITEM_WIRE_BYTES)
		{
			NetTrace("snapshot grid section truncated index=%d count=%u remaining=%zu", i, aCount, b.Remaining());
			return;
		}
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aState = b.ReadU8Or();
		uint8_t aX = b.ReadU8Or();
		uint8_t aY = b.ReadU8Or();
		int32_t aCounter = b.ReadI32Or();
		int32_t aRenderOrder = b.ReadI32Or();
		float aPosX = b.ReadF32Or();
		float aPosY = b.ReadF32Or();
		float aGoalX = b.ReadF32Or();
		float aGoalY = b.ReadF32Or();
		uint8_t aZombieType = b.ReadU8Or();
		uint8_t aSeedType = b.ReadU8Or();
		uint8_t aScaryPotType = b.ReadU8Or();
		uint16_t aTransparentCounter = b.ReadU16Or();
		uint8_t aSunCount = b.ReadU8Or();
		uint8_t aFlags = b.ReadU8Or();
		bool aPortal = aType == (uint8_t)GridItemType::GRIDITEM_PORTAL_CIRCLE ||
			aType == (uint8_t)GridItemType::GRIDITEM_PORTAL_SQUARE;
		bool aValidCell = (aX < MAX_GRID_SIZE_X || (aPortal && aX == MAX_GRID_SIZE_X)) && aY < MAX_GRID_SIZE_Y;
		bool aValid = anId != 0 && aType >= (uint8_t)GridItemType::GRIDITEM_GRAVESTONE &&
			aType <= (uint8_t)GridItemType::GRIDITEM_IZOMBIE_BRAIN &&
			NetGridStateMatchesType(aType, aState) && aValidCell &&
			(aZombieType == 255 || aZombieType < (uint8_t)ZombieType::NUM_ZOMBIE_TYPES) &&
			(aSeedType == 255 || aSeedType < (uint8_t)SeedType::NUM_SEED_TYPES) &&
			aScaryPotType <= (uint8_t)ScaryPotType::SCARYPOT_SUN &&
			_finite(aPosX) && _finite(aPosY) && _finite(aGoalX) && _finite(aGoalY);
		if (!aValid)
		{
			NetTrace("snapshot grid rejected id=%u type=%u state=%u cell=(%u,%u) zombie=%u seed=%u pot=%u",
				anId, aType, aState, aX, aY, aZombieType, aSeedType, aScaryPotType);
			continue;
		}
		bool aSeedPot = aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_SEED;
		bool aZombiePot = aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_ZOMBIE;
		bool aSunPot = aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_SUN;
		if (aType == (uint8_t)GridItemType::GRIDITEM_SCARY_POT &&
			(aY >= MAX_GRID_SIZE_Y - 1 || aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_NONE ||
			 (aSeedPot && (aSeedType == 255 || aZombieType != 255 || aSunCount != 0)) ||
			 (aZombiePot && (aZombieType == 255 || aSeedType != 255 || aSunCount != 0)) ||
			 (aSunPot && (aZombieType != 255 || aSeedType != 255 || aSunCount < 1 || aSunCount > 3)) ||
			 (aState == (uint8_t)GridItemState::GRIDITEM_STATE_SCARY_POT_LEAF && !aSeedPot) ||
			 (aState == (uint8_t)GridItemState::GRIDITEM_STATE_SCARY_POT_ZOMBIE && !aZombiePot) ||
			 aTransparentCounter > 50))
		{
			NetTrace("snapshot scary pot rejected id=%u state=%u content=%u zombie=%u seed=%u",
				anId, aState, aScaryPotType, aZombieType, aSeedType);
			continue;
		}
		if (aSeenGridItems.find(anId) != aSeenGridItems.end())
		{
			NetTrace("snapshot grid rejected duplicate id=%u", anId);
			continue;
		}
		aSeenGridItems[anId] = true;

		GridItem* aGridItem = NetGetGhostGridItem(anId);
		if (aGridItem && aGridItem->mGridItemType != (GridItemType)aType)
		{
			NetTrace("snapshot grid rejected: id=%u changed type old=%d new=%u", anId, (int)aGridItem->mGridItemType, aType);
			continue;
		}
		if (!aGridItem)
		{
			// Adopt the stock client-side item created during local resource/intro
			// setup when possible.  Its random contents are overwritten below.
			GridItem* aLocalItem = nullptr;
			while (mGridItems.IterateNext(aLocalItem))
			{
				if (!aLocalItem->mDead && !aLocalItem->mNetGhost &&
					aLocalItem->mGridItemType == (GridItemType)aType &&
					aLocalItem->mGridX == aX && aLocalItem->mGridY == aY)
				{
					aGridItem = aLocalItem;
					break;
				}
			}
		}
		GridItemState aPreviousState = aGridItem ? aGridItem->mGridItemState : (GridItemState)aState;
		if (!aGridItem)
		{
			gNetSuppressSounds = true;
			aGridItem = mGridItems.DataArrayAlloc();
			if (!aGridItem)
				continue;
			gNetSuppressSounds = false;
		}
		aGridItem->mGridItemType = (GridItemType)aType;
		aGridItem->mGridItemState = (GridItemState)aState;
		aGridItem->mGridX = aX;
		aGridItem->mGridY = aY;
		aGridItem->mGridItemCounter = aCounter;
		aGridItem->mRenderOrder = aRenderOrder;
		aGridItem->mPosX = aPosX;
		aGridItem->mPosY = aPosY;
		aGridItem->mGoalX = aGoalX;
		aGridItem->mGoalY = aGoalY;
		aGridItem->mZombieType = aZombieType == 255 ? ZombieType::ZOMBIE_INVALID : (ZombieType)aZombieType;
		aGridItem->mSeedType = aSeedType == 255 ? SeedType::SEED_NONE : (SeedType)aSeedType;
		aGridItem->mScaryPotType = (ScaryPotType)aScaryPotType;
		aGridItem->mTransparentCounter = aTransparentCounter;
		aGridItem->mSunCount = aSunCount;
		aGridItem->mHighlighted = (aFlags & 1) != 0;
		NetEnsureGridItemVisual(aGridItem, aPreviousState);
		aGridItem->mNetId = anId;
		aGridItem->mNetGhost = true;
	}
	{
		GridItem* aGridItem = nullptr;
		while (mGridItems.IterateNext(aGridItem))
			if (!aGridItem->mDead &&
				((aGridItem->mNetGhost && aSeenGridItems.find(aGridItem->mNetId) == aSeenGridItems.end()) ||
				 (!aGridItem->mNetGhost && aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)))
				aGridItem->GridItemDie();
	}
	if (b.Remaining() != 0)
		NetTrace("snapshot ignored trailing bytes=%zu", b.Remaining());
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
			float aHostVelocity = theZombie->mVelX;
			if (theAnimKey == 1)
			{
				theZombie->mIsEating = false;
				theZombie->StartEating();
			}
			else if (theAnimKey == 2)
			{
				theZombie->PlayDeathAnim(0U);
			}
			else
			{
				if (theZombie->mIsEating)
					theZombie->StopEating();
				else
					theZombie->StartWalkAnim(10);
			}
			theZombie->mVelX = aHostVelocity;
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

	// Do not call Zombie::Animate() on a client ghost: chew callbacks mutate
	// health and can trigger hypno/death gameplay.  Only advance its attached
	// visual tree and keep that tree positioned on the interpolated entity.
	AttachmentUpdateAndMove(theZombie->mAttachmentID, theZombie->mPosX, theZombie->mPosY);
	theZombie->UpdateReanim();
}

void Board::NetGhostUpdateOthers()
{
	// Host pause freezes all client-side presentation clocks and interpolation.
	if (mPaused)
		return;

	// Projectiles: interpolate every float used by Draw, keep the integer
	// GameObject origin coherent, and advance only the attached visual tree.
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
	{
		if (aProjectile->mDead || !aProjectile->mNetGhost)
			continue;
		aProjectile->mPosX += (aProjectile->mNetTargetX - aProjectile->mPosX) * 0.35f;
		aProjectile->mPosY += (aProjectile->mNetTargetY - aProjectile->mPosY) * 0.35f;
		aProjectile->mPosZ += (aProjectile->mNetTargetZ - aProjectile->mPosZ) * 0.35f;
		aProjectile->mRotation += (aProjectile->mNetTargetRotation - aProjectile->mRotation) * 0.35f;
		aProjectile->mX = (int)aProjectile->mPosX;
		aProjectile->mY = (int)(aProjectile->mPosY + aProjectile->mPosZ);
		AttachmentUpdateAndMove(aProjectile->mAttachmentID,
			aProjectile->mPosX, aProjectile->mPosY + aProjectile->mPosZ);
	}

	// Coins: host position is authoritative both while falling and while flying
	// into the bank.  Never call Coin::Update here: it can reach ScoreCoin and
	// mutate sun/profile/gameplay state on a client.
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
	{
		if (aCoin->mDead || !aCoin->mNetGhost)
			continue;
		aCoin->mPosX += (aCoin->mNetTargetX - aCoin->mPosX) * 0.35f;
		aCoin->mPosY += (aCoin->mNetTargetY - aCoin->mPosY) * 0.35f;
		aCoin->mX = 0;
		aCoin->mY = 0;
		if (aCoin->mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
		{
			float anOffsetX = 0.0f;
			float anOffsetY = 0.0f;
			if (aCoin->mType == CoinType::COIN_DIAMOND)
			{
				anOffsetX = 18.0f - 18.0f * aCoin->mScale;
				anOffsetY = 13.0f - 13.0f * aCoin->mScale;
			}
			AttachmentUpdateAndMove(aCoin->mAttachmentID,
				aCoin->mPosX + anOffsetX, aCoin->mPosY + anOffsetY);
			AttachmentOverrideColor(aCoin->mAttachmentID, aCoin->GetColor());
			AttachmentOverrideScale(aCoin->mAttachmentID, aCoin->mScale);
		}
	}

	// mowers: interpolate
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
	{
		if (aMower->mDead || !aMower->mNetGhost)
			continue;
		aMower->mPosX += (aMower->mNetTargetX - aMower->mPosX) * 0.3f;
		Reanimation* aMowerReanim = mApp->ReanimationTryToGet(aMower->mReanimID);
		if (aMowerReanim && aMowerReanim->mIsAttachment)
			aMowerReanim->Update();
	}

	// Grid-item visuals created from snapshots are attachments in the stock
	// code, so EffectSystem deliberately does not tick them.
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
	{
		if (aGridItem->mDead || !aGridItem->mNetGhost)
			continue;
		Reanimation* aGridReanim = mApp->ReanimationTryToGet(aGridItem->mGridItemReanimID);
		if (!aGridReanim || !aGridReanim->mIsAttachment)
			continue;
		aGridReanim->Update();
		if ((aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
			aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE) &&
			aGridItem->mGridItemState != GridItemState::GRIDITEM_STATE_PORTAL_CLOSED &&
			aGridReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aGridReanim->mLoopCount > 0)
		{
			aGridReanim->PlayReanim("anim_pulse", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
		}
	}
}

void Board::NetEncodeIntroState(NetBuffer& b)
{
	const int aWideX = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0;
	const int aWideY = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0;
	SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;

	b.WriteU32(mApp->mNetBoardSeed);
	b.WriteI32(mCutScene->mCutsceneTime);
	b.WriteI16((int16_t)mX);
	b.WriteI16((int16_t)mY);
	b.WriteI16((int16_t)mTreeX);
	b.WriteI16((int16_t)mPoleX);
	b.WriteI16((int16_t)(aChooser ? aChooser->mX - aWideX : 0));
	b.WriteI16((int16_t)(aChooser ? aChooser->mY - aWideY : 0));
	b.WriteI16((int16_t)(aChooser && aChooser->mMenuButton ? aChooser->mMenuButton->mY - aWideY : 0));
	b.WriteI16((int16_t)mSeedBank->mX);
	b.WriteI16((int16_t)mSeedBank->mY);
	b.WriteU16((uint16_t)max(0, min(1000, mSodPosition)));
	b.WriteI16((int16_t)(mFogOffset * 2.0f));
	b.WriteU16((uint16_t)min(65535, max(0, mFogBlownCountDown)));
	b.WriteU8((uint8_t)max(0, min(255, mSeedBank->mCutSceneDarken)));

	uint8_t aFlags = 0;
	if (mCutScene->mSeedChoosing) aFlags |= 1;
	if (mMenuButton && mMenuButton->mBtnNoDraw) aFlags |= 2;
	if (aChooser && aChooser->mMenuButton && aChooser->mMenuButton->mBtnNoDraw) aFlags |= 4;
	if (aChooser && aChooser->mMenuButton && aChooser->mMenuButton->mDisabled) aFlags |= 8;
	if (mEnableGraveStones) aFlags |= 16;
	if (mShowShovel) aFlags |= 32;
	if (aChooser && aChooser->mMouseVisible) aFlags |= 64;
	b.WriteU8(aFlags);

	// Use the same canonical 12-byte presentation payload as the playing-scene
	// DAVE_STATE message; INTRO_STATE applies it only after validating all data.
	NetEncodeDaveState(b);

	uint16_t aCount = 0;
	NetBuffer aPreview;
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead || aZombie->mFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE)
			continue;
		if (aZombie->mNetId == 0)
			aZombie->mNetId = NetAllocId();
		if (aZombie->mNetId == 0 || aCount == 0xFFFF)
			continue;
		aPreview.WriteU16(aZombie->mNetId);
		aPreview.WriteU8((uint8_t)aZombie->mZombieType);
		aPreview.WriteU8((uint8_t)max(0, min(MAX_GRID_SIZE_Y - 1, aZombie->mRow)));
		aPreview.WriteI16((int16_t)(aZombie->mPosX * 4.0f));
		aPreview.WriteI16((int16_t)(aZombie->mPosY * 4.0f));
		aPreview.WriteI32(aZombie->mRenderOrder);
		aPreview.WriteU8(aZombie->mVariant ? 1 : 0);
		aCount++;
	}
	b.WriteU16(aCount);
	b.WriteBytes(aPreview.Data(), aPreview.Size());
}

void Board::NetApplyIntroState(NetBuffer& b)
{
	static const size_t INTRO_FIXED_BYTES = 48;
	static const size_t INTRO_ZOMBIE_BYTES = 13;
	if (b.Remaining() < INTRO_FIXED_BYTES)
	{
		NetTrace("INTRO_STATE rejected: truncated fixed payload bytes=%zu", b.Remaining());
		return;
	}

	uint32_t aBoardSeed = b.ReadU32Or();
	int aCutsceneTime = b.ReadI32Or();
	int aBoardX = b.ReadI16Or();
	int aBoardY = b.ReadI16Or();
	int aTreeX = b.ReadI16Or();
	int aPoleX = b.ReadI16Or();
	int aChooserX = b.ReadI16Or();
	int aChooserY = b.ReadI16Or();
	int aChooserMenuY = b.ReadI16Or();
	int aSeedBankX = b.ReadI16Or();
	int aSeedBankY = b.ReadI16Or();
	int aSodPosition = b.ReadU16Or();
	int aFogOffset2 = b.ReadI16Or();
	int aFogCountdown = b.ReadU16Or();
	int aSeedBankDarken = b.ReadU8Or();
	uint8_t aFlags = b.ReadU8Or();
	NetDaveWireState aDaveState = {};
	const bool aDaveValid = NetReadDaveWireState(b, aDaveState);
	uint16_t aZombieCount = b.ReadU16Or();

	if (aBoardSeed != mApp->mNetBoardSeed || aSodPosition > 1000 || (aFlags & ~127) != 0 ||
		!aDaveValid ||
		b.Remaining() != (size_t)aZombieCount * INTRO_ZOMBIE_BYTES)
	{
		NetTrace("INTRO_STATE rejected: seed=%u expected=%u sod=%d flags=%u dave=(%u,%u,%d,%u,%d,%d) zombies=%u remaining=%zu",
			aBoardSeed, mApp->mNetBoardSeed, aSodPosition, aFlags,
			aDaveState.mState, aDaveState.mAnimKey, aDaveState.mMessageIndex,
			aDaveState.mAnimTimeQ, aDaveState.mX, aDaveState.mY,
			aZombieCount, b.Remaining());
		return;
	}

	struct PreviewZombie
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mRow;
		int16_t mX4;
		int16_t mY4;
		int32_t mRenderOrder;
		bool mVariant;
	};
	std::vector<PreviewZombie> aPreview;
	aPreview.reserve(aZombieCount);
	std::map<unsigned short, bool> aSeen;
	for (uint16_t i = 0; i < aZombieCount; i++)
	{
		PreviewZombie aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mRow = b.ReadU8Or();
		aWire.mX4 = b.ReadI16Or();
		aWire.mY4 = b.ReadI16Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mVariant = b.ReadU8Or() != 0;
		if (aWire.mId == 0 || aWire.mType >= (uint8_t)ZombieType::NUM_ZOMBIE_TYPES ||
			aWire.mRow >= MAX_GRID_SIZE_Y || aSeen.find(aWire.mId) != aSeen.end())
		{
			NetTrace("INTRO_STATE rejected zombie id=%u type=%u row=%u", aWire.mId, aWire.mType, aWire.mRow);
			return;
		}
		aSeen[aWire.mId] = true;
		aPreview.push_back(aWire);
	}

	// All bytes and preview entities have now been validated.  From this point
	// on mutations are presentation-only; the client never advances Dave's
	// dialog state machine or consumes gameplay RNG.
	NetApplyDavePresentation(this, mApp, aDaveState);

	mCutScene->mCutsceneTime = max(0, aCutsceneTime);
	Move(aBoardX, aBoardY);
	mTreeX = aTreeX;
	mPoleX = aPoleX;
	mSeedBank->Move(aSeedBankX, aSeedBankY);
	mSeedBank->mCutSceneDarken = aSeedBankDarken;
	mSodPosition = aSodPosition;
	mFogOffset = aFogOffset2 / 2.0f;
	mFogBlownCountDown = aFogCountdown;
	mEnableGraveStones = (aFlags & 16) != 0;
	mShowShovel = (aFlags & 32) != 0;
	if (mMenuButton)
		mMenuButton->mBtnNoDraw = (aFlags & 2) != 0;

	SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;
	if (aChooser)
	{
		const int aWideX = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0;
		const int aWideY = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0;
		aChooser->Move(aChooserX + aWideX, aChooserY + aWideY);
		aChooser->mMouseVisible = (aFlags & 64) != 0;
		if (aChooser->mMenuButton)
		{
			aChooser->mMenuButton->mY = aChooserMenuY + aWideY;
			aChooser->mMenuButton->mBtnNoDraw = (aFlags & 4) != 0;
			aChooser->mMenuButton->mDisabled = (aFlags & 8) != 0;
		}
		bool aSeedChoosing = (aFlags & 1) != 0;
		if (aSeedChoosing && !mCutScene->mSeedChoosing)
			mCutScene->StartSeedChooser();
		else if (!aSeedChoosing && mCutScene->mSeedChoosing)
		{
			mCutScene->mSeedChoosing = false;
			aChooser->mMouseVisible = false;
			mApp->mWidgetManager->SetFocus(this);
		}
	}

	for (const PreviewZombie& aWire : aPreview)
	{
		Zombie* aZombie = NetGetGhostZombie(aWire.mId);
		if (!aZombie)
		{
			gNetSuppressSounds = true;
			aZombie = AddZombieInRow((ZombieType)aWire.mType, aWire.mRow, Zombie::ZOMBIE_WAVE_CUTSCENE);
			gNetSuppressSounds = false;
			if (!aZombie)
				continue;
			aZombie->mNetId = aWire.mId;
			aZombie->mNetGhost = true;
			aZombie->mNetAnimKey = 255;
		}
		aZombie->mFromWave = Zombie::ZOMBIE_WAVE_CUTSCENE;
		aZombie->mRow = aWire.mRow;
		aZombie->mVariant = aWire.mVariant;
		aZombie->mPosX = aWire.mX4 / 4.0f;
		aZombie->mPosY = aWire.mY4 / 4.0f;
		aZombie->mNetTargetX = aZombie->mPosX;
		aZombie->mNetTargetY = aZombie->mPosY;
		aZombie->mX = (int)aZombie->mPosX;
		aZombie->mY = (int)aZombie->mPosY;
		aZombie->mRenderOrder = aWire.mRenderOrder;
	}

	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (!aZombie->mDead && aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE &&
			(!aZombie->mNetGhost || aSeen.find(aZombie->mNetId) == aSeen.end()))
		{
			gNetSuppressSounds = true;
			aZombie->DieNoLoot();
			gNetSuppressSounds = false;
		}
	}
	mCutScene->mPlacedZombies = true;
}

void Board::NetClientTick()
{
	// Drain and apply incoming messages first (events then snapshots in order)
	NetDrainMessages();

	// ghost micro-updates (interpolation, cosmetic counters)
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mNetGhost)
		{
			NetGhostUpdateZombie(aZombie, aZombie->mNetAnimKey);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			// Preview zombies are created locally by the stock cutscene.  They do
			// not simulate on a client, but their attached reanimations must tick.
			AttachmentUpdateAndMove(aZombie->mAttachmentID, aZombie->mPosX, aZombie->mPosY);
			aZombie->UpdateReanim();
		}
	}

	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
	{
		if (!aPlant->mDead && aPlant->mIsOnBoard)
		{
			aPlant->Animate();
			aPlant->UpdateReanim();
		}
	}
	NetGhostUpdateOthers();

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
		if (aCursor.mHeldSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
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
	int aPlantCount = 0;
	for (int aSlot = 0; aSlot < LawnNet::NET_MAX_PLAYERS; aSlot++)
	{
		const LawnNet::NetPlayerInfo& aPlayer = mApp->mNetManager->GetPlayer(aSlot);
		if (!aPlayer.mActive)
			continue;
		if (aPlayer.mTeam == LawnNet::NETTEAM_PLANTS)
			aPlantSlots[aPlantCount++] = aSlot;
	}

	// PreNewGame rejects an incomplete PvP lobby.  Keep safe fallbacks for
	// malformed/replayed state so every packet still has a valid owner.
	if (aPlantCount == 0)
		aPlantSlots[aPlantCount++] = 0;

	int aPlantRoundRobin = 0;
	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
		if (aPacket.mPacketType == SeedType::SEED_NONE)
			continue;
		if (mChallenge->IsZombieSeedType(aPacket.mPacketType))
		{
			// Every zombie player can use every zombie card.  Brains and cooldowns
			// live on this one host-authoritative shared packet.
			aPacket.mNetOwnerSlot = LawnNet::NET_SHARED_OWNER;
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
