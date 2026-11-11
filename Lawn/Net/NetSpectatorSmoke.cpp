#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace
{
struct SpectatorSmokeState
{
	uint32_t mStarted = 0, mJoined = 0;
	uint32_t mShape = 0;
	int mStage = 0;
	bool mReadOnlyIssued = false, mPingObserved = false, mVerified = false;
};
SpectatorSmokeState s;

const char* SpectatorRole()
{
	const char* enabled = getenv("PVZ_COOP_SMOKE_SPECTATOR");
	return enabled && std::string(enabled) == "1" ? getenv("PVZ_COOP_SMOKE_SPECTATOR_ROLE") : nullptr;
}

uint32_t BoardIdentity(Board* board)
{
	std::vector<std::vector<uint8_t>> plants;
	Plant* plant = nullptr;
	while (board->mPlants.IterateNext(plant))
	{
		if (plant->mDead || !plant->mIsOnBoard) continue;
		LawnNet::NetBuffer record;
		record.WriteU16(plant->mNetId); record.WriteU8((uint8_t)plant->mSeedType);
		record.WriteU8((uint8_t)plant->mPlantCol); record.WriteU8((uint8_t)plant->mRow);
		plants.push_back(record.mData);
	}
	std::sort(plants.begin(), plants.end());
	LawnNet::NetBuffer identity;
	for (const auto& record : plants) identity.WriteBytes(record.data(), record.size());
	for (int i = 0; board->mSeedBank && i < board->mSeedBank->mNumPackets; ++i)
	{
		const SeedPacket& packet = board->mSeedBank->mSeedPackets[i];
		identity.WriteU16(packet.mNetId); identity.WriteU8((uint8_t)packet.mPacketType);
		identity.WriteU8(packet.mNetOwnerSlot);
	}
	return LawnNet::NetFnv1a32(identity.Data(), identity.Size());
}

bool Mouse(LawnApp* app, bool down, uint8_t button, float x, float y)
{
	SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
	e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = button;
	e.button.down = down; e.button.clicks = 1; e.button.x = x; e.button.y = y;
	return SDL_PushEvent(&e);
}

bool Escape(LawnApp* app)
{
	SDL_Event e{}; e.type = SDL_EVENT_KEY_DOWN; e.key.windowID = SDL_GetWindowID(app->mSDLWindow);
	e.key.key = SDLK_ESCAPE; e.key.scancode = SDL_SCANCODE_ESCAPE; e.key.down = true;
	if (!SDL_PushEvent(&e)) return false;
	e.type = SDL_EVENT_KEY_UP; e.key.down = false;
	return SDL_PushEvent(&e);
}

bool HasSystemLine(LawnNet::NetManager* net, const char* text)
{
	for (const auto& line : net->mChatLines)
		if (line.mSlot == LawnNet::NET_SHARED_OWNER && line.mText.find(text) != std::string::npos) return true;
	return false;
}
}

bool LawnApp::CoopSmokeUpdateSpectator()
{
	const char* role = SpectatorRole();
	if (!role) return true;
	if (!mNetManager || !mBoard || mGameScene != SCENE_PLAYING) return false;
	const bool observer = std::string(role) == "spectator";
	if (!observer && !mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots)) return false;
	const uint32_t now = GetTickCount();
	if (!s.mStarted)
	{
		s.mStarted = now; s.mShape = BoardIdentity(mBoard);
		if (mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT)
		{
			int owners[LawnNet::NET_MAX_PLAYERS] = {};
			for (int i = 0; mBoard->mSeedBank && i < mBoard->mSeedBank->mNumPackets; ++i)
			{
				const int owner = mBoard->mSeedBank->mSeedPackets[i].mNetOwnerSlot;
				if (owner < LawnNet::NET_MAX_PLAYERS) ++owners[owner];
			}
			if (!mBoard->mSeedBank || mBoard->mSeedBank->mNumPackets != 10 || owners[0] != 5 || owners[1] != 5 || owners[2])
			{ CoopSmokeFail("spectator fixture has no native 5+5 mixed-owner bank"); return false; }
			LawnNet::NetTrace("SMOKE_SPECTATOR_MIXED_BANK role=%s packets=10 host=5 client=5 spectator=0", role);
		}
		LawnNet::NetTrace("SMOKE_SPECTATOR_READY role=%s shape=%08X", role, s.mShape);
	}
	if (now - s.mStarted > 90000U)
	{ CoopSmokeFail("late spectator join/ping/pause kick timed out"); return false; }
	if (observer)
	{
		if (!mNetManager->IsSpectator() || mNetManager->GetMySlot() != 2 || mNetManager->GetPlayerCount() != 3)
		{ CoopSmokeFail("late client was not assigned read-only spectator slot 2"); return false; }
		if (s.mStage == 0)
		{
			for (int i = 0; mBoard->mSeedBank && i < mBoard->mSeedBank->mNumPackets; ++i)
				if (mBoard->mSeedBank->mSeedPackets[i].mNetOwnerSlot == 2)
				{ CoopSmokeFail("spectator received gameplay seed ownership"); return false; }
			LawnNet::NetTrace("SMOKE_SPECTATOR_BOOTSTRAP role=spectator shape=%08X readonly=1 scene=playing", s.mShape);
			SDL_SetModState(SDL_KMOD_NONE);
			if (!mBoard->mSeedBank || mBoard->mSeedBank->mNumPackets < 1)
			{ CoopSmokeFail("spectator bootstrap omitted the seed bank"); return false; }
			const SeedPacket& packet = mBoard->mSeedBank->mSeedPackets[0];
			const float x = (float)(mBoard->mSeedBank->mX + packet.mX + packet.mOffsetX + 20);
			const float y = (float)(mBoard->mSeedBank->mY + packet.mY + 20);
			if (!Mouse(this, true, SDL_BUTTON_LEFT, x, y) || !Mouse(this, false, SDL_BUTTON_LEFT, x, y))
			{ CoopSmokeFail("spectator SDL packet click queue failed"); return false; }
			s.mReadOnlyIssued = true; s.mStage = 1;
		}
		else if (s.mStage == 1 && now - s.mStarted >= 150U)
		{
			if (!Mouse(this, true, SDL_BUTTON_LEFT, 400, 400) || !Mouse(this, false, SDL_BUTTON_LEFT, 400, 400))
			{ CoopSmokeFail("spectator SDL planting attempt queue failed"); return false; }
			s.mStage = 2;
		}
		else if (s.mStage == 2 && now - s.mStarted >= 350U)
		{
			if (!Mouse(this, true, SDL_BUTTON_MIDDLE, 610, 410) || !Mouse(this, false, SDL_BUTTON_MIDDLE, 610, 410))
			{ CoopSmokeFail("spectator SDL ping queue failed"); return false; }
			s.mStage = 3;
		}
		for (const auto& a : mNetManager->GetAnnotations())
			if (a.mSlot == 2 && a.mKind == LawnNet::NETANNOTATION_PING && a.mX == 610 && a.mY == 410)
				s.mPingObserved = true;
		if (s.mStage == 3 && s.mPingObserved && BoardIdentity(mBoard) == s.mShape &&
			mBoard->mCursorObject && mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL)
		{
			LawnNet::NetTrace("SMOKE_SPECTATOR_READONLY role=spectator shape=%08X noPlantMutation=1 ping=1 input=SDL_queue", s.mShape);
			s.mStage = 4;
		}
		return false; // the real host kick is the success condition for this process
	}
	if (mNetManager->IsHosting())
	{
		if (mNetManager->GetPlayer(2).mActive && !s.mJoined)
		{
			s.mJoined = now;
			if (!mNetManager->GetPlayer(2).mSpectator || !HasSystemLine(mNetManager, "joined as a spectator"))
			{ CoopSmokeFail("host omitted spectator authority or join system chat"); return false; }
			LawnNet::NetTrace("SMOKE_SPECTATOR_BOOTSTRAP role=host shape=%08X readonly=1 scene=playing", BoardIdentity(mBoard));
		}
		for (const auto& a : mNetManager->GetAnnotations())
			if (a.mSlot == 2 && a.mKind == LawnNet::NETANNOTATION_PING && a.mX == 610 && a.mY == 410)
				s.mPingObserved = true;
		if (s.mStage == 0 && s.mJoined && s.mPingObserved && now - s.mJoined > 1200U)
		{
			if (BoardIdentity(mBoard) != s.mShape)
			{ CoopSmokeFail("spectator SDL input mutated host plant/packet identity"); return false; }
			LawnNet::NetTrace("SMOKE_SPECTATOR_READONLY role=host shape=%08X noPlantMutation=1 ping=1 input=SDL_queue", s.mShape);
			if (!Escape(this)) { CoopSmokeFail("host SDL pause queue failed"); return false; }
			s.mStage = 1;
		}
		else if (s.mStage == 1 && mBoard->mPaused)
		{
			// These are the production pause-player-list bounds, slot 2.
			if (!Mouse(this, true, SDL_BUTTON_LEFT, 196, 236) || !Mouse(this, false, SDL_BUTTON_LEFT, 196, 236))
			{ CoopSmokeFail("host SDL spectator kick queue failed"); return false; }
			s.mStage = 2;
		}
		else if (s.mStage == 2 && !mNetManager->GetPlayer(2).mActive && HasSystemLine(mNetManager, "was kicked by the host"))
		{
			LawnNet::NetTrace("SMOKE_SPECTATOR_KICK role=host inactive=1 systemChat=1 pauseButton=1");
			if (!Escape(this)) { CoopSmokeFail("host SDL resume queue failed"); return false; }
			s.mStage = 3;
		}
		if (s.mStage == 3 && !mBoard->mPaused) s.mVerified = true;
	}
	else
	{
		if (HasSystemLine(mNetManager, "joined as a spectator"))
		{ if (!s.mJoined) s.mJoined = now; s.mStage = 1; }
		if (mNetManager->GetPlayer(2).mActive)
		{
			if (!s.mJoined) s.mJoined = now;
		}
		if (s.mStage == 1 && s.mJoined && !mNetManager->GetPlayer(2).mActive &&
			HasSystemLine(mNetManager, "was kicked by the host") && !mBoard->mPaused)
		{
			s.mVerified = true;
			LawnNet::NetTrace("SMOKE_SPECTATOR_KICK role=client inactive=1 systemChat=1 existingSession=1");
		}
	}
	if (s.mVerified)
		LawnNet::NetTrace("SMOKE_SPECTATOR_COMPLETE role=%s originalPeerConnected=1", role);
	return s.mVerified;
}

bool LawnApp::CoopSmokeExpectedSpectatorKick(const std::string& theReason)
{
	const char* role = SpectatorRole();
	// The ping echo and disconnect can share one TCP pump, before the next
	// smoke tick. Read the already-applied event rather than requiring a frame.
	if (mNetManager)
		for (const auto& a : mNetManager->GetAnnotations())
			if (a.mSlot == 2 && a.mKind == LawnNet::NETANNOTATION_PING && a.mX == 610 && a.mY == 410)
				s.mPingObserved = true;
	if (!role || std::string(role) != "spectator" || !s.mReadOnlyIssued || !s.mPingObserved ||
		theReason.find("kicked") == std::string::npos) return false;
	LawnNet::NetTrace("SMOKE_SPECTATOR_KICKED role=spectator reason=host_kick readonly=1 ping=1");
	const std::string temporary = mCoopSmokeManifest.mResultPath + ".spectator.tmp";
	std::ofstream result(temporary, std::ios::binary | std::ios::trunc);
	result << "{\"status\":\"pass\",\"test\":\"late-spectator\",\"readonly\":true,\"ping\":true,\"hostKick\":true}\n";
	result.close();
	if (!result || !MoveFileExA(temporary.c_str(), mCoopSmokeManifest.mResultPath.c_str(), MOVEFILE_WRITE_THROUGH))
	{ CoopSmokeFail("spectator kick proof could not be written atomically"); return false; }
	mCoopSmokeResultWritten = true;
	return true;
}
