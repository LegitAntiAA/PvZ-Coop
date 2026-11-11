#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../CursorObject.h"
#include "../SeedPacket.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <cstdlib>
#include <cmath>

using namespace LawnNet;
namespace
{
struct ModesState
{
	int mStage = 0;
	uint32_t mTick = 0;
	uint16_t mPlant = 0;
	int mBaseWaves = 0, mWaves = 0, mBasePoints = 0, mPoints = 0;
	int mFade = -1;
	bool mDifficultySet = false, mDone = false;
	bool mDifficultyVerified = false, mCursorIssued = false;
	uint16_t mHoverPacket = 0;
	int mHeldStage = 0, mHeldIndex = -1;
	uint32_t mHeldTick = 0, mGoalSeenTick = 0;
	uint16_t mHeldPacket = 0;
	SeedType mHeldSeed = SEED_NONE;
	bool mGoal99Captured = false;
	std::shared_ptr<SDL_Surface> mGoalFrame;
};
ModesState& State() { static ModesState aState; return aState; }
bool Enabled()
{
	const char* aFlag = getenv("PVZ_COOP_SMOKE_MODES");
	return aFlag && std::string(aFlag) == "1";
}
bool SawChat(NetManager* theNet, int theSlot, const char* theText)
{
	for (const auto& aLine : theNet->mChatLines)
		if (aLine.mSlot == theSlot && aLine.mText == theText) return true;
	return false;
}
Plant* FindPlant(Board* theBoard, uint16_t theId)
{
	Plant* aPlant = nullptr;
	while (theBoard->mPlants.IterateNext(aPlant))
		if (!aPlant->mDead && aPlant->mNetId == theId) return aPlant;
	return nullptr;
}
Plant* FirstPlant(Board* theBoard)
{
	Plant* aPlant = nullptr;
	while (theBoard->mPlants.IterateNext(aPlant))
		if (!aPlant->mDead && aPlant->mIsOnBoard && aPlant->mNetId) return aPlant;
	return nullptr;
}
bool SdlClick(LawnApp* theApp, int theX, int theY, uint8_t theButton = SDL_BUTTON_LEFT)
{
	for (bool aDown : {true, false})
	{
		SDL_Event aEvent{};
		aEvent.type = aDown ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
		aEvent.button.windowID = SDL_GetWindowID(theApp->mSDLWindow);
		aEvent.button.button = theButton; aEvent.button.down = aDown; aEvent.button.clicks = 1;
		aEvent.button.x = (float)theX; aEvent.button.y = (float)theY;
		if (!SDL_PushEvent(&aEvent)) return false;
	}
	return true;
}
void RawClick(LawnApp* theApp, int theX, int theY)
{
	theX += theApp->mDDInterface->mWideScreenOffsetX;
	theY += theApp->mDDInterface->mWideScreenOffsetY;
	theApp->mNetManager->SendInput(true, 1, theX, theY);
	theApp->mNetManager->SendInput(false, 1, theX, theY);
}
}

void LawnApp::CoopSmokeRecordModeWavePlan(int theBaseWaves, int theWaves, int theBasePoints, int thePoints)
{
	if (!Enabled()) return;
	auto& s = State();
	s.mBaseWaves = theBaseWaves; s.mWaves = theWaves;
	s.mBasePoints = theBasePoints; s.mPoints = thePoints;
}

bool LawnApp::CoopSmokeUpdateModes()
{
	if (!Enabled()) return true;
	auto& s = State();
	if (s.mDone) return true;
	if (!mNetManager) return false;
	const bool aHost = mCoopSmokeManifest.IsHost();
	const char* aRole = aHost ? "host" : "client";
	const bool aTower = mCoopSmokeManifest.mScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT;
	if (aTower && aHost && !mBoard && !mCoopSmokeGameStarted && mNetManager->IsHosting() && !s.mDifficultySet)
	{
		mNetManager->SetDifficulty(10); s.mDifficultySet = true;
	}
	if (!mBoard || mGameScene != SCENE_PLAYING ||
		!mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots)) return false;
	Board* b = mBoard;
	const uint32_t aNow = GetTickCount();
	if (aTower)
	{
		if (!s.mDifficultyVerified)
		{
			int aZombies = 0;
			for (int aWave = 0; aWave < b->mNumWaves; ++aWave)
				for (int i = 0; i < MAX_ZOMBIES_IN_WAVE && b->mZombiesInWave[aWave][i] != ZOMBIE_INVALID; ++i) ++aZombies;
			if (mNetManager->GetDifficulty() != 10 || s.mBaseWaves <= 0 || s.mWaves <= s.mBaseWaves ||
				s.mPoints <= s.mBasePoints || b->mNumWaves != s.mWaves || aZombies <= s.mBaseWaves ||
				!b->mZombieAllowed[ZOMBIE_GARGANTUAR] || !b->mZombieAllowed[ZOMBIE_FOOTBALL])
				{ CoopSmokeFail("difficulty10 did not expand its real wave plan and roster"); return false; }
			if (aHost)
			{
				for (ZombieType aType : { ZOMBIE_NORMAL, ZOMBIE_PAIL })
				{
					Zombie aStock;
					aStock.ZombieInitialize(2, aType, false, nullptr, 0);
					Zombie* aScaled = b->AddZombieInRow(aType, 2, 0);
					const bool aSame = aScaled && aScaled->mBodyHealth == aStock.mBodyHealth &&
						aScaled->mBodyMaxHealth == aStock.mBodyMaxHealth && aScaled->mHelmHealth == aStock.mHelmHealth;
					aStock.DieNoLoot();
					if (!aSame) { CoopSmokeFail("difficulty changed stock zombie health"); return false; }
				}
			}
			NetTrace("SMOKE_MODES_DIFFICULTY role=%s difficulty=10 baseWaves=%d waves=%d basePoints=%d points=%d zombies=%d garg=1 football=1 hpUnchanged=%d",
				aRole, s.mBaseWaves, s.mWaves, s.mBasePoints, s.mPoints, aZombies, aHost ? 1 : 0);
			s.mDifficultyVerified = true; s.mTick = aNow;
		}
		if (!b->mSeedBank || b->mSeedBank->mNumPackets != 10)
			{ CoopSmokeFail("relative cursor fixture requires the real10-card split bank"); return false; }
		if (aNow - s.mTick > 15000U)
			{ CoopSmokeFail("semantic seed cursor did not cross different HUD layouts"); return false; }
		if (!aHost)
		{
			SeedPacket& aFirst = b->mSeedBank->mSeedPackets[0];
			if (aFirst.mNetOwnerSlot != 1 || !aFirst.mNetId)
				{ CoopSmokeFail("client's first relative card is not locally owned"); return false; }
			if (!s.mCursorIssued)
			{
				SDL_Event aMotion{}; aMotion.type = SDL_EVENT_MOUSE_MOTION;
				aMotion.motion.windowID = SDL_GetWindowID(mSDLWindow);
				aMotion.motion.x = (float)(b->mSeedBank->mX + aFirst.mX + aFirst.mOffsetX + aFirst.mWidth / 2);
				aMotion.motion.y = (float)(b->mSeedBank->mY + aFirst.mY + aFirst.mHeight / 2);
				if (!SDL_PushEvent(&aMotion)) { CoopSmokeFail("SDL relative cursor motion was rejected"); return false; }
				s.mHoverPacket = aFirst.mNetId; s.mCursorIssued = true;
			}
			const std::string aAck = "modes-cursor-ok:" + std::to_string(s.mHoverPacket);
			if (SawChat(mNetManager, 0, aAck.c_str()))
			{
				if (!CoopSmokeCaptureFrame("modes_td_own_cursor")) return false;
				NetTrace("SMOKE_MODES_CURSOR role=client packetId=%u localIndex=0 mapped=1 pointInside=1 renderSettled=1", s.mHoverPacket);
				s.mDone = true;
			}
		}
		else
		{
			const NetRemoteCursor& aCursor = mNetManager->mRemoteCursors[1];
			for (int i = 0; i < b->mSeedBank->mNumPackets; ++i)
			{
				const SeedPacket& aPacket = b->mSeedBank->mSeedPackets[i];
				if (!aCursor.mValid || !aCursor.mHoverPacketId || aPacket.mNetId != aCursor.mHoverPacketId) continue;
				if (i < 5 || aPacket.mNetOwnerSlot != 1)
					{ CoopSmokeFail("remote cursor did not identify the other player's reordered card"); return false; }
				const int aLeft = b->mSeedBank->mX + aPacket.mX + aPacket.mOffsetX + mDDInterface->mWideScreenOffsetX;
				const int aTop = b->mSeedBank->mY + aPacket.mY + mDDInterface->mWideScreenOffsetY;
				if (aCursor.mX < aLeft || aCursor.mX >= aLeft + aPacket.mWidth ||
					aCursor.mY < aTop || aCursor.mY >= aTop + aPacket.mHeight ||
					std::abs(aCursor.mRenderX - aCursor.mX) > 1.0f || std::abs(aCursor.mRenderY - aCursor.mY) > 1.0f)
					return false;
				if (!CoopSmokeCaptureFrame("modes_td_relative_cursor")) return false;
				NetTrace("SMOKE_MODES_CURSOR role=host packetId=%u localIndex=%d remoteIndex=0 mapped=1 pointInside=1 renderSettled=1",
					aPacket.mNetId, i);
				mNetManager->SendChat("modes-cursor-ok:" + std::to_string(aPacket.mNetId));
				s.mDone = true; break;
			}
		}
		return s.mDone;
	}
	if (!b->IsPvPMode()) { CoopSmokeFail("VerifyModes unsupported scenario"); return false; }
	if (!aHost && s.mHeldStage < 4)
	{
		const int aLawnX = b->GridToPixelX(6, 2) + 40;
		const int aLawnY = b->GridToPixelY(6, 2) + 45;
		if (s.mHeldStage == 0)
		{
			for (int i = PVP_PLANT_PACKET_COUNT; i < b->mSeedBank->mNumPackets; ++i)
			{
				SeedPacket& aPacket = b->mSeedBank->mSeedPackets[i];
				if (!aPacket.CanPickUp()) continue;
				if (!SdlClick(this, b->mSeedBank->mX + aPacket.mX + aPacket.mOffsetX + aPacket.mWidth / 2,
					b->mSeedBank->mY + aPacket.mY + aPacket.mHeight / 2))
					{ CoopSmokeFail("SDL zombie card pick was rejected"); return false; }
				s.mHeldPacket = aPacket.mNetId; s.mHeldSeed = aPacket.mPacketType; s.mHeldIndex = i;
				s.mHeldStage = 1; s.mHeldTick = aNow;
				NetTrace("SMOKE_MODES_ZOMBIE_HELD_INPUT packetId=%u index=%d type=%d", s.mHeldPacket, i, (int)s.mHeldSeed);
				break;
			}
			return false;
		}
		if (aNow - s.mHeldTick > 10000U)
			{ CoopSmokeFail("host-confirmed zombie held silhouette did not become visible or cancel"); return false; }
		const bool aHeld = b->mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK &&
			b->mCursorObject->mSeedBankIndex == s.mHeldIndex && b->mCursorObject->mType == s.mHeldSeed &&
			b->mCursorObject->mImitaterType == b->mSeedBank->mSeedPackets[s.mHeldIndex].mImitaterType &&
			b->mSeedBank->mSeedPackets[s.mHeldIndex].mNetId == s.mHeldPacket;
		if (s.mHeldStage == 1 && aHeld)
		{
			SDL_Event aMotion{}; aMotion.type = SDL_EVENT_MOUSE_MOTION;
			aMotion.motion.windowID = SDL_GetWindowID(mSDLWindow);
			aMotion.motion.x = (float)aLawnX; aMotion.motion.y = (float)aLawnY;
			if (!SDL_PushEvent(&aMotion)) { CoopSmokeFail("SDL zombie hand motion was rejected"); return false; }
			s.mHeldStage = 2; s.mHeldTick = aNow; s.mGoalFrame = mCoopSmokeFrame;
		}
		else if (s.mHeldStage == 2 && aNow - s.mHeldTick >= 250U)
		{
			if (mCoopSmokeFrame == s.mGoalFrame) return false;
			if (!aHeld || !b->mCursorObject->mVisible || !b->mCursorPreview->mVisible ||
				b->mCursorPreview->mGridY != 2 || b->mCursorPreview->mGridX != 6 ||
				std::abs(b->mCursorObject->mX - (aLawnX - 25)) > 1 || std::abs(b->mCursorObject->mY - (aLawnY - 35)) > 1)
				{ CoopSmokeFail("zombie held packet, hand, or row2 preview was lost after host echo"); return false; }
			if (!CoopSmokeCaptureFrame("modes_zombie_held")) return false;
			NetTrace("SMOKE_MODES_ZOMBIE_HELD role=client packetId=%u index=%d type=%d hostEcho=1 handVisible=1 previewVisible=1 row=2 col=6",
				s.mHeldPacket, s.mHeldIndex, (int)s.mHeldSeed);
			if (!SdlClick(this, aLawnX, aLawnY, SDL_BUTTON_RIGHT))
				{ CoopSmokeFail("SDL zombie hand cancel was rejected"); return false; }
			s.mHeldStage = 3; s.mHeldTick = aNow;
		}
		else if (s.mHeldStage == 3 && b->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL)
			s.mHeldStage = 4;
		return false;
	}
	if (!s.mPlant)
	{
		Plant* aPlant = FirstPlant(b);
		if (!aPlant) return false;
		s.mPlant = aPlant->mNetId;
	}
	const Rect aShovel = b->GetShovelButtonRect();
	// Use the bottom of the real shovel hitbox, below the compact zombie
	// cards. Thus an attack cannot be remapped into a legitimate seed pick.
	const int aShovelX = aShovel.mX + aShovel.mWidth / 2, aShovelY = aShovel.mY + aShovel.mHeight - 3;
	if (!aHost)
	{
		Plant* aTarget = FindPlant(b, s.mPlant);
		if ((s.mStage <= 1 || (s.mStage == 2 && !SawChat(mNetManager, 0, "modes-raw-ok"))) && !aTarget)
			{ CoopSmokeFail("zombie-side shovel request removed the plant"); return false; }
		if (s.mStage == 0)
		{
			if (!b->mShowShovel || b->CanUsePlantTools(1) ||
				!SdlClick(this, aShovelX, aShovelY) || !SdlClick(this, aTarget->mX + 40, aTarget->mY + 40))
				{ CoopSmokeFail("PvP SDL shovel fixture could not issue its input"); return false; }
			mNetManager->SendChat("modes-sdl-issued"); s.mStage = 1;
		}
		else if (s.mStage == 1 && SawChat(mNetManager, 0, "modes-sdl-ok"))
		{
			if (!CoopSmokeCaptureFrame("modes_zombie_hud")) return false;
			RawClick(this, aShovelX, aShovelY); RawClick(this, aTarget->mX + 40, aTarget->mY + 40);
			mNetManager->SendChat("modes-raw-issued"); s.mStage = 2;
		}
		else if (s.mStage == 2 && SawChat(mNetManager, 0, "modes-raw-ok"))
		{
			NetTrace("SMOKE_MODES_SHOVEL role=client sdl=blocked raw=blocked plantPreserved=1 zombieTool=0");
			s.mStage = 3;
		}
		if (s.mStage == 3 && b->mNetPvPKillScore == 99)
		{
			if (b->mLevelAwardSpawned || b->mBoardFadeOutCounter >= 0 || b->mLevelComplete)
				{ CoopSmokeFail("PvP ended before100 real fixture kills"); return false; }
			if (!s.mGoalSeenTick) { s.mGoalSeenTick = aNow; s.mGoalFrame = mCoopSmokeFrame; }
			if (aNow - s.mGoalSeenTick < 100U || mCoopSmokeFrame == s.mGoalFrame ||
				!CoopSmokeCaptureFrame("modes_goal99")) return false;
			NetTrace("SMOKE_MODES_GOAL role=client kills=99 award=0 fade=-1 complete=0 fixtureSeed=98 actualDeaths=1");
			mNetManager->SendChat("modes-goal99-seen"); s.mStage = 4;
		}
		if (s.mStage == 4 && b->mNetPvPKillScore == 100 && b->mBoardFadeOutCounter >= 0)
		{
			if (s.mFade < 0) { s.mFade = b->mBoardFadeOutCounter; s.mGoalFrame = mCoopSmokeFrame; }
			else if (b->mBoardFadeOutCounter < s.mFade && mCoopSmokeFrame != s.mGoalFrame)
			{
				if (!CoopSmokeCaptureFrame("modes_goal100")) return false;
				NetTrace("SMOKE_MODES_GOAL role=client kills=100 fadeEdge=1 counter=%d previous=%d fixtureSeed=98 actualDeaths=2",
					b->mBoardFadeOutCounter, s.mFade);
				mNetManager->SendChat("modes-goal100-seen"); s.mDone = true;
			}
		}
		return s.mDone;
	}

	Plant* aTarget = FindPlant(b, s.mPlant);
	if (s.mStage < 3 && !aTarget) { CoopSmokeFail("host lost target after zombie shovel attempt"); return false; }
	if (s.mStage == 0 && SawChat(mNetManager, 1, "modes-sdl-issued")) { s.mStage = 1; s.mTick = aNow; }
	if (s.mStage == 1 && aNow - s.mTick >= 1000U)
	{
		if (!b->CanUsePlantTools(0) || b->CanUsePlantTools(1) || b->mNetPlayerCursors[1]->mCursorType == CURSOR_TYPE_SHOVEL)
			{ CoopSmokeFail("zombie SDL click acquired plant-side tool authority"); return false; }
		mNetManager->SendChat("modes-sdl-ok"); s.mStage = 2;
	}
	if (s.mStage == 2 && SawChat(mNetManager, 1, "modes-raw-issued")) { s.mStage = 3; s.mTick = aNow; }
	if (s.mStage == 3 && aNow - s.mTick >= 1000U)
	{
		if (!CoopSmokeCaptureFrame("modes_plant_hud")) return false;
		aTarget = FindPlant(b, s.mPlant);
		if (!aTarget || b->mNetPlayerCursors[1]->mCursorType == CURSOR_TYPE_SHOVEL)
			{ CoopSmokeFail("raw zombie shovel request bypassed host authorization"); return false; }
		CursorObject* aSavedCursor = b->mCursorObject;
		const int aSavedSlot = mNetManager->GetRawActingPlayerSlot();
		b->mCursorObject = b->mNetPlayerCursors[1]; mNetManager->SetActingPlayerSlot(1);
		b->mCursorObject->mCursorType = CURSOR_TYPE_SHOVEL;
		b->MouseDownWithTool(aTarget->mX + 40, aTarget->mY + 40, 1, CURSOR_TYPE_SHOVEL);
		b->mCursorObject = aSavedCursor; mNetManager->SetActingPlayerSlot(aSavedSlot);
		if (!FindPlant(b, s.mPlant)) { CoopSmokeFail("forged zombie tool cursor bypassed final server guard"); return false; }
		mNetManager->SendChat("modes-raw-ok");
		// A valid plant-side shovel must still work through the shared path.
		b->PickUpTool(OBJECT_TYPE_SHOVEL);
		b->MouseDownWithTool(aTarget->mX + 40, aTarget->mY + 40, 1, CURSOR_TYPE_SHOVEL);
		if (FindPlant(b, s.mPlant)) { CoopSmokeFail("plant-side shovel was disabled by PvP guard"); return false; }
		NetTrace("SMOKE_MODES_SHOVEL role=host sdl=blocked raw=blocked forgedCursor=blocked plantTool=working");
		Zombie* aZombie = nullptr;
		while (b->mZombies.IterateNext(aZombie)) if (!aZombie->mDead) { aZombie->mNetKillCounted = true; aZombie->DieNoLoot(); }
		b->mNetPvPKillScore = 98;
		Zombie* aFirst = b->AddZombieInRow(ZOMBIE_NORMAL, 2, 0);
		if (!aFirst) { CoopSmokeFail("PvP could not create first goal fixture zombie"); return false; }
		aFirst->DieWithLoot();
		if (b->mNetPvPKillScore != 99 || b->mLevelAwardSpawned || b->mBoardFadeOutCounter >= 0 || b->mLevelComplete)
			{ CoopSmokeFail("PvP last-zombie stock award or missed death counter at99"); return false; }
		NetTrace("SMOKE_MODES_GOAL role=host kills=99 award=0 fade=-1 complete=0 fixtureSeed=98 actualDeaths=1");
		s.mStage = 4; s.mTick = aNow; s.mGoalFrame = mCoopSmokeFrame;
	}
	if (s.mStage == 4 && !s.mGoal99Captured && aNow - s.mTick >= 100U && mCoopSmokeFrame != s.mGoalFrame)
		s.mGoal99Captured = CoopSmokeCaptureFrame("modes_goal99");
	if (s.mStage == 4 && aNow - s.mTick >= 1000U && SawChat(mNetManager, 1, "modes-goal99-seen"))
	{
		if (!s.mGoal99Captured) return false;
		if (b->mNetPvPKillScore != 99 || b->mBoardFadeOutCounter >= 0 || b->mLevelAwardSpawned)
			{ CoopSmokeFail("PvP did not hold the99-kill nonterminal state"); return false; }
		Zombie* aLast = b->AddZombieInRow(ZOMBIE_NORMAL, 2, 0);
		if (!aLast) { CoopSmokeFail("PvP could not create final goal fixture zombie"); return false; }
		aLast->DieWithLoot();
		if (b->mNetPvPKillScore != 100) { CoopSmokeFail("PvP death edge did not count100"); return false; }
		s.mStage = 5;
	}
	if (s.mStage == 5 && b->mBoardFadeOutCounter >= 0)
	{
		if (s.mFade < 0) { s.mFade = b->mBoardFadeOutCounter; s.mGoalFrame = mCoopSmokeFrame; }
		else if (b->mBoardFadeOutCounter < s.mFade && mCoopSmokeFrame != s.mGoalFrame)
		{
			if (!CoopSmokeCaptureFrame("modes_goal100")) return false;
			NetTrace("SMOKE_MODES_GOAL role=host kills=100 fadeEdge=1 counter=%d previous=%d fixtureSeed=98 actualDeaths=2",
				b->mBoardFadeOutCounter, s.mFade);
			s.mStage = 6;
		}
	}
	if (s.mStage == 6 && SawChat(mNetManager, 1, "modes-goal100-seen")) s.mDone = true;
	return s.mDone;
}
