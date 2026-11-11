#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include <cstdlib>
#include <string>
#include <vector>

namespace LawnNet { bool NetSmokeUpdatePresentation(LawnApp* theApp); }

namespace
{
struct InteractionSmokeState
{
	uint32_t mStarted = 0;
	int mStage = 0;
	int mPlantCount = 0;
	bool mPingVerified = false, mStrokeVerified = false, mSeedVerified = false;
	bool mHoldVerified = false, mFadeVerified = false, mClearVerified = false;
	uint16_t mPacket = 0;
};

int LivePlants(Board* theBoard)
{
	int aCount = 0;
	Plant* aPlant = nullptr;
	while (theBoard->mPlants.IterateNext(aPlant))
		if (!aPlant->mDead && aPlant->mIsOnBoard) ++aCount;
	return aCount;
}

bool PushMouse(LawnApp* theApp, bool theDown, uint8_t theButton, float theX, float theY)
{
	SDL_Event aEvent{};
	aEvent.type = theDown ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
	aEvent.button.windowID = SDL_GetWindowID(theApp->mSDLWindow);
	aEvent.button.button = theButton;
	aEvent.button.down = theDown;
	aEvent.button.clicks = 1;
	aEvent.button.x = theX; aEvent.button.y = theY;
	return SDL_PushEvent(&aEvent);
}

bool PushMotion(LawnApp* theApp, float theX, float theY)
{
	SDL_Event aEvent{};
	aEvent.type = SDL_EVENT_MOUSE_MOTION;
	aEvent.motion.windowID = SDL_GetWindowID(theApp->mSDLWindow);
	aEvent.motion.x = theX; aEvent.motion.y = theY;
	return SDL_PushEvent(&aEvent);
}

bool PushModifier(LawnApp* theApp, SDL_Keycode theKey, SDL_Scancode theScan, bool theDown, SDL_Keymod theMods)
{
	// SDL_PushEvent does not update SDL's device state. Model the same local
	// modifier state the real keyboard handler reads, without OS input/focus.
	SDL_SetModState(theMods);
	SDL_Event aEvent{};
	aEvent.type = theDown ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	aEvent.key.windowID = SDL_GetWindowID(theApp->mSDLWindow);
	aEvent.key.key = theKey; aEvent.key.scancode = theScan;
	aEvent.key.down = theDown; aEvent.key.mod = theMods;
	return SDL_PushEvent(&aEvent);
}

bool CaptureFrame(LawnApp* theApp, const char* theName)
{
	return theApp->CoopSmokeCaptureFrame((std::string("interactions_") + theName).c_str());
}
}

bool LawnApp::CoopSmokeUpdateInteractions()
{
	const char* aPresentation = getenv("PVZ_COOP_SMOKE_PRESENTATION");
	if (aPresentation && std::string(aPresentation) == "1")
		return LawnNet::NetSmokeUpdatePresentation(this);
	const char* anEnabled = getenv("PVZ_COOP_SMOKE_INTERACTIONS");
	if (!anEnabled || std::string(anEnabled) != "1") return true;
	static InteractionSmokeState s;
	if (!CoopSmokeActive() || !mNetManager || !mBoard || mGameScene != SCENE_PLAYING ||
		!mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots)) return false;
	const char* aRole = mCoopSmokeManifest.IsHost() ? "host" : "client";
	const uint32_t aNow = GetTickCount();
	if (!s.mStarted)
	{
		if (mWidth != 800 || mHeight != 600 || mCoopSmokeManifest.mScenario != LawnNet::NETSMOKE_SCENARIO_ADVENTURE_1)
		{
			CoopSmokeFail("interaction smoke requires Adventure1 at 800x600 on both peers");
			return false;
		}
		s.mStarted = aNow;
		s.mPlantCount = LivePlants(mBoard);
		LawnNet::NetTrace("SMOKE_INTERACTIONS_BEGIN role=%s plants=%d input=SDL_queue foreground=0", aRole, s.mPlantCount);
	}
	const uint32_t anElapsed = aNow - s.mStarted;
	if (anElapsed > 30000U)
	{
		CoopSmokeFail("interaction SDL regression timed out waiting for peer/render evidence");
		return false;
	}
	if (mCoopSmokeManifest.IsClient())
	{
		bool aQueued = true;
		switch (s.mStage)
		{
		case 0: if (anElapsed >= 750U) { aQueued = PushMouse(this, true, SDL_BUTTON_MIDDLE, 620, 420) && PushMouse(this, false, SDL_BUTTON_MIDDLE, 620, 420); ++s.mStage; } break;
		case 1: if (anElapsed >= 850U) { aQueued = PushMouse(this, true, SDL_BUTTON_MIDDLE, 630, 420) && PushMouse(this, false, SDL_BUTTON_MIDDLE, 630, 420); ++s.mStage; } break;
		case 2: if (anElapsed >= 1400U) { aQueued = PushMouse(this, true, SDL_BUTTON_MIDDLE, 640, 420) && PushMouse(this, false, SDL_BUTTON_MIDDLE, 640, 420); ++s.mStage; } break;
		case 3: if (anElapsed >= 2300U) { aQueued = PushModifier(this, SDLK_LCTRL, SDL_SCANCODE_LCTRL, true, SDL_KMOD_LCTRL) && PushMotion(this, 230, 290); ++s.mStage; } break;
		case 4: if (anElapsed >= 2420U) { aQueued = PushMotion(this, 260, 310); ++s.mStage; } break;
		case 5: if (anElapsed >= 2540U) { aQueued = PushMotion(this, 290, 290); ++s.mStage; } break;
		case 6: if (anElapsed >= 2660U) { aQueued = PushMotion(this, 320, 310); ++s.mStage; } break;
		case 7: if (anElapsed >= 2800U) { aQueued = PushModifier(this, SDLK_LCTRL, SDL_SCANCODE_LCTRL, false, SDL_KMOD_NONE); ++s.mStage; } break;
		case 8:
			if (anElapsed >= 3400U)
			{
				SeedPacket* aPacket = nullptr;
				for (int i = 0; mBoard->mSeedBank && i < mBoard->mSeedBank->mNumPackets; ++i)
					if (mBoard->mSeedBank->mSeedPackets[i].mNetId && mBoard->IsSeedPacketVisibleToSlot(i, mNetManager->GetMySlot()))
					{ aPacket = &mBoard->mSeedBank->mSeedPackets[i]; break; }
				if (!aPacket) { CoopSmokeFail("interaction smoke has no visible authoritative packet"); return false; }
				s.mPacket = aPacket->mNetId;
				const float aX = (float)(mBoard->mSeedBank->mX + aPacket->mX + aPacket->mOffsetX + aPacket->mWidth / 2);
				const float aY = (float)(mBoard->mSeedBank->mY + aPacket->mY + aPacket->mHeight / 2);
				aQueued = PushModifier(this, SDLK_LALT, SDL_SCANCODE_LALT, true, SDL_KMOD_LALT) &&
					PushMouse(this, true, SDL_BUTTON_LEFT, aX, aY) && PushMouse(this, false, SDL_BUTTON_LEFT, aX, aY);
				++s.mStage;
			}
			break;
		case 9: if (anElapsed >= 3550U) { aQueued = PushModifier(this, SDLK_LALT, SDL_SCANCODE_LALT, false, SDL_KMOD_NONE); ++s.mStage; } break;
		default: break;
		}
		if (!aQueued) { CoopSmokeFail("SDL rejected a process-local interaction event"); return false; }
	}

	int aPingCount = 0, aSeedCount = 0;
	std::vector<LawnNet::NetAnnotation> aStroke;
	uint16_t aPacketId = 0;
	uint32_t aStrokeEnd = 0;
	bool aBegin = false, anEnd = false;
	for (const auto& a : mNetManager->GetAnnotations())
	{
		if (a.mSlot != 1) continue;
		if (a.mKind == LawnNet::NETANNOTATION_PING && a.mY == 420 && a.mX >= 620 && a.mX <= 640) ++aPingCount;
		else if (a.mKind == LawnNet::NETANNOTATION_SEED) { ++aSeedCount; aPacketId = a.mId; }
		else if (a.mKind >= LawnNet::NETANNOTATION_DRAW_BEGIN && a.mKind <= LawnNet::NETANNOTATION_DRAW_END)
		{
			aStroke.push_back(a);
			aBegin |= a.mKind == LawnNet::NETANNOTATION_DRAW_BEGIN;
			anEnd |= a.mKind == LawnNet::NETANNOTATION_DRAW_END;
			aStrokeEnd = a.mTick;
		}
	}
	if (!s.mPingVerified && anElapsed >= 2100U && aPingCount == 2)
	{
		s.mPingVerified = true;
		LawnNet::NetTrace("SMOKE_INTERACTIONS_PING role=%s accepted=2 attempts=3 cooldownMs=500 authority=received", aRole);
	}
	if (aPingCount > 2) { CoopSmokeFail("ping cooldown admitted the within-500ms SDL click"); return false; }
	if (!s.mStrokeVerified && aBegin && anEnd && aStroke.size() >= 4)
	{
		s.mStrokeVerified = true;
		LawnNet::NetTrace("SMOKE_INTERACTIONS_STROKE role=%s points=%zu begin=1 end=1 authority=received", aRole, aStroke.size());
	}
	if (!s.mSeedVerified && aSeedCount == 1 && aPacketId)
	{
		SeedPacket* aPacket = nullptr;
		for (int i = 0; mBoard->mSeedBank && i < mBoard->mSeedBank->mNumPackets; ++i)
			if (mBoard->mSeedBank->mSeedPackets[i].mNetId == aPacketId) aPacket = &mBoard->mSeedBank->mSeedPackets[i];
		if (aPacket)
		{
			const std::string aName = SexyStringToString(Plant::GetNameString(aPacket->mPacketType, aPacket->mImitaterType));
			for (const auto& aChat : mNetManager->mChatLines)
				if (aChat.mSlot == 1 && aChat.mText.find(aName) != std::string::npos && aChat.mText.find("s)") != std::string::npos)
				{
					if (mCoopSmokeManifest.IsClient() && s.mPacket != aPacketId) { CoopSmokeFail("Alt click ping changed packet identity"); return false; }
					s.mSeedVerified = true;
					if (!CaptureFrame(this, "seed")) { CoopSmokeFail("interaction packet frame capture failed"); return false; }
					LawnNet::NetTrace("SMOKE_INTERACTIONS_SEED role=%s id=%u name=1 cooldownText=1 authority=received", aRole, aPacketId);
					break;
				}
		}
	}
	if (aSeedCount > 1) { CoopSmokeFail("one Alt-click produced duplicate packet pings"); return false; }
	if (s.mStrokeVerified && aStrokeEnd && aStroke.size() >= 4)
	{
		const uint32_t anAge = aNow - aStrokeEnd;
		const int aFirstAlpha = LawnNet::NetStrokeAlpha(anAge, 0, (int)aStroke.size());
		const int aLastAlpha = LawnNet::NetStrokeAlpha(anAge, (int)aStroke.size() - 1, (int)aStroke.size());
		if (!s.mHoldVerified && anAge >= 7500U && anAge < 8000U && aFirstAlpha == 255 && aLastAlpha == 255)
		{
			if (!CaptureFrame(this, "hold")) { CoopSmokeFail("interaction hold frame capture failed"); return false; }
			s.mHoldVerified = true;
			LawnNet::NetTrace("SMOKE_INTERACTIONS_HOLD role=%s age=%u first=%d last=%d", aRole, anAge, aFirstAlpha, aLastAlpha);
		}
		if (!s.mFadeVerified && anAge >= 8250U && anAge < 9000U && aFirstAlpha > aLastAlpha && aLastAlpha > 0)
		{
			if (!CaptureFrame(this, "fade")) { CoopSmokeFail("interaction fade frame capture failed"); return false; }
			s.mFadeVerified = true;
			LawnNet::NetTrace("SMOKE_INTERACTIONS_FADE role=%s age=%u first=%d last=%d direction=end_to_start", aRole, anAge, aFirstAlpha, aLastAlpha);
		}
		if (!s.mClearVerified && anAge >= 10100U && aFirstAlpha == 0 && aLastAlpha == 0)
		{
			if (!CaptureFrame(this, "clear")) { CoopSmokeFail("interaction clear frame capture failed"); return false; }
			s.mClearVerified = true;
			LawnNet::NetTrace("SMOKE_INTERACTIONS_CLEAR role=%s age=%u first=0 last=0", aRole, anAge);
		}
	}
	if (s.mPingVerified && s.mStrokeVerified && s.mSeedVerified && s.mHoldVerified && s.mFadeVerified && s.mClearVerified)
	{
		if (LivePlants(mBoard) != s.mPlantCount || (mBoard->mCursorObject &&
			mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK))
		{ CoopSmokeFail("annotation input leaked into planting or held-seed gameplay"); return false; }
		LawnNet::NetTrace("SMOKE_INTERACTIONS_COMPLETE role=%s ping=1 stroke=1 seed=1 fade=1 noPlantMutation=1", aRole);
		return true;
	}
	return false;
}
