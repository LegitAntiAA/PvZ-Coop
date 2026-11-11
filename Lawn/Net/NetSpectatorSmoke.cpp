#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../CutScene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct SpectatorSmokeState
{
	uint32_t mStarted = 0, mJoined = 0;
	uint32_t mShape = 0;
	uint32_t mCommsTick = 0;
	int mStage = 0;
	bool mReadOnlyIssued = false, mPingObserved = false, mVerified = false;
	bool mRawIssued = false, mSeedObserved = false, mDrawObserved = false;
	bool mChatObserved = false, mCursorObserved = false, mCommsVerified = false, mReadySent = false;
	uint16_t mPacket = 0;
	int mPromotionStage = 0, mPlantCol = -1, mPlantRow = -1;
	uint32_t mFixtureTick = 0, mBoundaryTick = 0, mChooseTick = 0, mPlantTick = 0, mCaptureTick = 0;
	uint32_t mBankEntered = 0, mBankHash = 0, mLastBankMessage = 0;
	uint16_t mFixture = 0, mPlantPacket = 0, mPlant = 0;
	bool mFixtureKilled = false, mBoundaryLogged = false, mRoundStarted = false, mQuotaLogged = false;
	bool mPickIssued = false, mPickObserved = false, mPlantIssued = false, mPlantCaptured = false;
	bool mBankVerified = false;
};
SpectatorSmokeState s;

const char* SpectatorRole()
{
	const char* enabled = getenv("PVZ_COOP_SMOKE_SPECTATOR");
	return enabled && std::string(enabled) == "1" ? getenv("PVZ_COOP_SMOKE_SPECTATOR_ROLE") : nullptr;
}

bool PromotionVariant()
{
	const char* variant = getenv("PVZ_COOP_SMOKE_SPECTATOR_PROMOTE");
	return SpectatorRole() && variant && std::string(variant) == "1";
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

bool Motion(LawnApp* app, float x, float y)
{
	SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION;
	e.motion.windowID = SDL_GetWindowID(app->mSDLWindow); e.motion.x = x; e.motion.y = y;
	return SDL_PushEvent(&e);
}

bool Key(LawnApp* app, SDL_Keycode key, SDL_Scancode scan, bool down, SDL_Keymod mods)
{
	SDL_SetModState(mods);
	SDL_Event e{}; e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	e.key.windowID = SDL_GetWindowID(app->mSDLWindow); e.key.key = key;
	e.key.scancode = scan; e.key.down = down; e.key.mod = mods;
	return SDL_PushEvent(&e);
}

bool EnterChat(LawnApp* app)
{
	if (!Key(app, SDLK_RETURN, SDL_SCANCODE_RETURN, true, SDL_KMOD_NONE) ||
		!Key(app, SDLK_RETURN, SDL_SCANCODE_RETURN, false, SDL_KMOD_NONE)) return false;
	SDL_Event e{}; e.type = SDL_EVENT_TEXT_INPUT;
	e.text.windowID = SDL_GetWindowID(app->mSDLWindow); e.text.text = "spectator communications qa";
	return SDL_PushEvent(&e) && Key(app, SDLK_RETURN, SDL_SCANCODE_RETURN, true, SDL_KMOD_NONE) &&
		Key(app, SDLK_RETURN, SDL_SCANCODE_RETURN, false, SDL_KMOD_NONE);
}

bool HasChat(LawnNet::NetManager* net, int slot, const char* text)
{
	for (const auto& line : net->mChatLines)
		if (line.mSlot == slot && line.mText == text) return true;
	return false;
}

void ObserveCommunications(LawnApp* app, const char* role, bool observer, uint32_t now)
{
	if (s.mCommsVerified) return;
	bool begin = false, end = false;
	int points = 0;
	for (const auto& a : app->mNetManager->GetAnnotations())
	{
		if (a.mSlot != 2) continue;
		if (a.mKind == LawnNet::NETANNOTATION_PING && a.mX == 610 && a.mY == 410) s.mPingObserved = true;
		if (a.mKind == LawnNet::NETANNOTATION_SEED && a.mId)
		{
			for (int i = 0; app->mBoard->mSeedBank && i < app->mBoard->mSeedBank->mNumPackets; ++i)
				if (app->mBoard->mSeedBank->mSeedPackets[i].mNetId == a.mId)
				{ s.mPacket = a.mId; s.mSeedObserved = true; }
		}
		if (a.mKind >= LawnNet::NETANNOTATION_DRAW_BEGIN && a.mKind <= LawnNet::NETANNOTATION_DRAW_END)
		{
			++points; begin |= a.mKind == LawnNet::NETANNOTATION_DRAW_BEGIN;
			end |= a.mKind == LawnNet::NETANNOTATION_DRAW_END;
		}
	}
	s.mDrawObserved |= begin && end && points >= 4;
	s.mChatObserved |= HasChat(app->mNetManager, 2, "spectator communications qa");
	if (!observer)
	{
		const auto& cursor = app->mNetManager->mRemoteCursors[2];
		s.mCursorObserved = cursor.mValid && cursor.mX == 610 && cursor.mY == 410 &&
			cursor.mHeldPacketId == 0 && cursor.mHeldSeed == 255;
	}
	if (!s.mPingObserved || !s.mSeedObserved || !s.mDrawObserved || !s.mChatObserved ||
		(!observer && !s.mCursorObserved)) return;
	if (!s.mCommsTick) s.mCommsTick = now;
	if (now - s.mCommsTick < 150U) return;
	if (!app->CoopSmokeCaptureFrame("spectator_communications"))
	{ app->CoopSmokeFail("observer communications renderer capture failed"); return; }
	s.mCommsVerified = true;
	LawnNet::NetTrace("SMOKE_SPECTATOR_COMMS role=%s chat=1 middle=1 seed=%u draw=1 cursor=%d points=%d capture=1 input=SDL_queue",
		role, s.mPacket, s.mCursorObserved ? 1 : 0, points);
}

bool HasSystemLine(LawnNet::NetManager* net, const char* text)
{
	for (const auto& line : net->mChatLines)
		if (line.mSlot == LawnNet::NET_SHARED_OWNER && line.mText.find(text) != std::string::npos) return true;
	return false;
}

bool VerifyCommittedBank(LawnApp* app, const char* role, uint32_t now)
{
	if (s.mBankVerified) return true;
	if (!s.mBankEntered) s.mBankEntered = now;
	if (now - s.mBankEntered > 10000U)
	{ app->CoopSmokeFail("promoted committed bank identity/three-peer barrier timed out"); return false; }
	LawnNet::NetManager* net = app->mNetManager;
	SeedBank* bank = app->mBoard->mSeedBank;
	const int quotas[LawnNet::NET_MAX_PLAYERS] = {4, 3, 3, 0};
	const int slot = net->GetMySlot();
	if (!bank || bank->mNumPackets != 10 || slot < 0 || slot > 2)
	{ app->CoopSmokeFail("promoted committed bank must contain all 10 chosen cards"); return false; }
	std::vector<std::vector<uint8_t>> records;
	int index = 0;
	for (int offset = 0; offset < LawnNet::NET_MAX_PLAYERS; ++offset)
	{
		const int owner = (slot + offset) % LawnNet::NET_MAX_PLAYERS;
		for (int count = 0; count < quotas[owner]; ++count, ++index)
		{
			const SeedPacket& packet = bank->mSeedPackets[index];
			if (packet.mNetOwnerSlot != owner || packet.mPacketType < SEED_PEASHOOTER ||
				packet.mPacketType >= NUM_SEEDS_IN_CHOOSER)
			{
				LawnNet::NetTrace("SMOKE_SPECTATOR_BANK_REJECT role=%s index=%d owner=%u expected=%d type=%d",
					role, index, packet.mNetOwnerSlot, owner, (int)packet.mPacketType);
				app->CoopSmokeFail("promoted committed bank lost viewer-relative owner order"); return false;
			}
			if (!packet.mNetId) return false; // Wait for the first authoritative post-commit snapshot.
			for (int previous = 0; previous < index; ++previous)
				if (bank->mSeedPackets[previous].mNetId == packet.mNetId)
				{ app->CoopSmokeFail("promoted committed bank contains duplicate packet identities"); return false; }
			LawnNet::NetBuffer record;
			record.WriteU16(packet.mNetId); record.WriteU8((uint8_t)packet.mPacketType);
			record.WriteU8((uint8_t)packet.mImitaterType); record.WriteU8(packet.mNetOwnerSlot);
			records.push_back(record.mData);
		}
	}
	std::sort(records.begin(), records.end());
	LawnNet::NetBuffer identity;
	for (const auto& record : records) identity.WriteBytes(record.data(), record.size());
	s.mBankHash = LawnNet::NetFnv1a32(identity.Data(), identity.Size());
	const std::string notice = "PROMOTE_BANK " + std::to_string(s.mBankHash);
	if (!HasChat(net, slot, notice.c_str()) && (!s.mLastBankMessage || now - s.mLastBankMessage >= 800U))
	{ net->SendChat(notice); s.mLastBankMessage = GetTickCount(); }
	for (int peer = 0; peer < 3; ++peer)
		if (!HasChat(net, peer, notice.c_str())) return false;
	if (!app->CoopSmokeCaptureFrame("spectator_committed_bank"))
	{ app->CoopSmokeFail("promoted committed bank renderer capture failed"); return false; }
	s.mBankVerified = true;
	LawnNet::NetTrace("SMOKE_SPECTATOR_COMMITTED_BANK role=%s packets=10 owners=4,3,3 ownPrefix=%d hash=%08X peers=3 capture=1 source=after_START_PLAYING",
		role, quotas[slot], s.mBankHash);
	return true;
}

bool UpdatePromotion(LawnApp* app, const char* role, uint32_t now)
{
	LawnNet::NetManager* net = app->mNetManager;
	const bool host = net->IsHosting(), observer = std::string(role) == "spectator";
	if (now - s.mStarted > 150000U)
	{ app->CoopSmokeFail("observer promotion/next-round SDL planting timed out"); return false; }
	if (host && s.mPromotionStage == 1 && app->mBoard && app->mGameScene == SCENE_PLAYING)
	{
		if (!s.mFixture)
		{
			// Compress the final-wave boundary; a native death, award collection,
			// fade and CheckForGameEnd still perform the real level transition.
			app->mBoard->RemoveAllZombies();
			app->mBoard->mCurrentWave = app->mBoard->mNumWaves;
			app->mBoard->mZombieCountDown = 5000;
			Zombie* z = app->mBoard->AddZombieInRow(ZOMBIE_NORMAL, 2, app->mBoard->mNumWaves - 1);
			if (!z) { app->CoopSmokeFail("promotion final-wave fixture spawn failed"); return false; }
			s.mFixture = z->mNetId; s.mFixtureTick = now;
			LawnNet::NetTrace("SMOKE_SPECTATOR_FINAL_WAVE role=host wave=%d zombie=%u source=boundary_fixture", app->mBoard->mCurrentWave, s.mFixture);
		}
		if (!s.mFixtureKilled && now - s.mFixtureTick >= 700U)
		{
			Zombie* z = nullptr;
			while (app->mBoard->mZombies.IterateNext(z))
				if (!z->mDead && z->mNetId == s.mFixture) { z->DieWithLoot(); s.mFixtureKilled = true; break; }
		}
		Coin* coin = nullptr;
		while (app->mBoard->mCoins.IterateNext(coin))
			if (!coin->mDead && coin->IsLevelAward() && coin->mHitGround && !coin->mIsBeingCollected)
			{
				if (!app->CoopSmokeCaptureFrame("spectator_promotion_award") ||
					!Mouse(app, true, SDL_BUTTON_LEFT, coin->mPosX + coin->mWidth / 2, coin->mPosY + coin->mHeight / 2) ||
					!Mouse(app, false, SDL_BUTTON_LEFT, coin->mPosX + coin->mWidth / 2, coin->mPosY + coin->mHeight / 2))
				{ app->CoopSmokeFail("promotion native award SDL collection failed"); return false; }
				s.mPromotionStage = 2;
				break;
			}
	}
	if (!app->mBoard && app->mChallengeScreen && !net->GetPlayer(2).mSpectator)
	{
		if (!s.mBoundaryTick) s.mBoundaryTick = now;
		if (!s.mBoundaryLogged && now - s.mBoundaryTick >= 150U)
		{
			if (net->GetPlayerCount() != 3 || net->GetReadyEligibleCount() != 3 ||
				!app->CoopSmokeCaptureFrame("spectator_promotion_boundary"))
			{ app->CoopSmokeFail("stock level boundary did not promote all three connected players"); return false; }
			s.mBoundaryLogged = true; s.mPromotionStage = 3;
			LawnNet::NetTrace("SMOKE_SPECTATOR_PROMOTED role=%s players=3 eligible=3 spectator=0 source=stock_level_end", role);
			if (!host) net->SendChat(observer ? "PROMOTE_BOUNDARY_2" : "PROMOTE_BOUNDARY_1");
		}
		if (host && s.mBoundaryLogged && !s.mRoundStarted &&
			HasChat(net, 1, "PROMOTE_BOUNDARY_1") && HasChat(net, 2, "PROMOTE_BOUNDARY_2"))
		{
			s.mRoundStarted = true; s.mPromotionStage = 4;
			app->KillChallengeScreen();
			app->PreNewGame((GameMode)app->mCoopSmokeManifest.mGameMode, false, app->mCoopSmokeManifest.mLevel);
		}
		return false;
	}
	if (!s.mBoundaryLogged || !app->mBoard) return false;
	if (app->mGameScene == SCENE_LEVEL_INTRO)
	{
		s.mRoundStarted = true; s.mPromotionStage = 4;
		SeedChooserScreen* chooser = app->mSeedChooserScreen;
		if (!chooser || !chooser->mMouseVisible || !app->mBoard->mCutScene->mSeedChoosing) return false;
		const int slot = net->GetMySlot();
		if (net->IsSpectator() || net->GetReadyEligibleCount() != 3 || net->mChooserPickQuota[2] <= 0)
		{ app->CoopSmokeFail("promoted player was omitted from the next real chooser quotas"); return false; }
		if (!s.mQuotaLogged)
		{
			s.mQuotaLogged = true;
			LawnNet::NetTrace("SMOKE_SPECTATOR_PROMOTION_QUOTA role=%s eligible=3 quotas=%d,%d,%d own=%d", role,
				net->mChooserPickQuota[0], net->mChooserPickQuota[1], net->mChooserPickQuota[2], chooser->mCoopPicksPerPlayer);
		}
		if (now - s.mChooseTick < 200U || chooser->mSeedsInFlight) return false;
		if (chooser->CoopGetLocalQuotaPickCount() < chooser->mCoopPicksPerPlayer)
		{
			for (int offset = 0; offset < 32; ++offset)
			{
				// Slot 2 needs a stock affordable card at the normal 50-sun start.
				const int index = ((slot == 2 ? (int)SEED_PUFFSHROOM : slot * 5) + offset) % 32;
				ChosenSeed& seed = chooser->mChosenSeeds[index];
				if (seed.mSeedState != SEED_IN_CHOOSER || !app->SeedTypeAvailable(seed.mSeedType) ||
					chooser->SeedNotAllowedToPick(seed.mSeedType) || chooser->SeedNotAllowedDuringTrial(seed.mSeedType) ||
					chooser->CoopIsPickedByOthers((int)seed.mSeedType)) continue;
				const float x = (float)(chooser->mX + seed.mX + 25), y = (float)(chooser->mY + seed.mY + 35);
				if (!Mouse(app, true, SDL_BUTTON_LEFT, x, y) || !Mouse(app, false, SDL_BUTTON_LEFT, x, y))
					app->CoopSmokeFail("promoted chooser SDL pick queue failed");
				s.mChooseTick = now;
				break;
			}
		}
		else if (!net->IsMyReady() && chooser->mStartButton && !chooser->mStartButton->mDisabled)
		{
			GameButton* button = chooser->mStartButton;
			if (!Mouse(app, true, SDL_BUTTON_LEFT, chooser->mX + button->mX + button->mWidth / 2,
				chooser->mY + button->mY + button->mHeight / 2) ||
				!Mouse(app, false, SDL_BUTTON_LEFT, chooser->mX + button->mX + button->mWidth / 2,
				chooser->mY + button->mY + button->mHeight / 2)) app->CoopSmokeFail("promoted chooser Ready SDL queue failed");
			s.mChooseTick = now;
		}
		return false;
	}
	if (!s.mRoundStarted || app->mGameScene != SCENE_PLAYING || !s.mQuotaLogged) return false;
	if (!VerifyCommittedBank(app, role, now)) return false;
	SeedPacket* selected = nullptr;
	for (int i = 0; app->mBoard->mSeedBank && i < app->mBoard->mSeedBank->mNumPackets; ++i)
	{
		SeedPacket& packet = app->mBoard->mSeedBank->mSeedPackets[i];
		if (packet.mNetId == s.mPlantPacket) selected = &packet;
	}
	if (observer && !s.mPickIssued && GetTickCount() - s.mLastBankMessage >= 800U)
	{
		for (int i = 0; app->mBoard->mSeedBank && !s.mPickIssued && i < app->mBoard->mSeedBank->mNumPackets; ++i)
		{
			SeedPacket& packet = app->mBoard->mSeedBank->mSeedPackets[i];
			if (packet.mNetOwnerSlot != 2 || !packet.mNetId || !packet.CanPickUp()) continue;
			for (int row = 0; row < MAX_GRID_SIZE_Y && !s.mPickIssued; ++row)
				for (int col = 0; col < 9 && !s.mPickIssued; ++col)
					if (app->mBoard->CanPlantAt(col, row, packet.mPacketType) == PLANTING_OK)
					{
						s.mPlantPacket = packet.mNetId; s.mPlantCol = col; s.mPlantRow = row; selected = &packet;
						const float x = (float)(app->mBoard->mSeedBank->mX + packet.mX + packet.mOffsetX + 20);
						const float y = (float)(app->mBoard->mSeedBank->mY + packet.mY + 20);
						if (!Mouse(app, true, SDL_BUTTON_LEFT, x, y) || !Mouse(app, false, SDL_BUTTON_LEFT, x, y))
						{ app->CoopSmokeFail("promoted SDL seed pickup failed"); return false; }
						net->SendChat("PROMOTE_PLANT " + std::to_string(s.mPlantPacket) + " " + std::to_string(col) + " " + std::to_string(row));
						s.mPickIssued = true; s.mPlantTick = now;
					}
		}
	}
	if (!observer && !s.mPlantPacket)
		for (const auto& line : net->mChatLines)
			if (line.mSlot == 2 && line.mText.compare(0, 14, "PROMOTE_PLANT ") == 0)
			{
				std::istringstream input(line.mText); std::string tag; int packet = 0;
				if (input >> tag >> packet >> s.mPlantCol >> s.mPlantRow) s.mPlantPacket = (uint16_t)packet;
			}
	if (s.mPlantPacket)
	{
		const auto& cursor = net->mRemoteCursors[2];
		if (cursor.mHeldPacketId == s.mPlantPacket) s.mPickObserved = true;
		if (observer && selected && app->mBoard->mCursorObject &&
			app->mBoard->mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK &&
			app->mBoard->mCursorObject->mSeedBankIndex == (int)(selected - app->mBoard->mSeedBank->mSeedPackets)) s.mPickObserved = true;
		if (host && app->mBoard->mNetPlayerCursors[2] &&
			app->mBoard->mNetPlayerCursors[2]->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK) s.mPickObserved = true;
	}
	if (observer && s.mPickIssued && s.mPickObserved && !s.mPlantIssued && now - s.mPlantTick >= 300U)
	{
		const float x = (float)(app->mBoard->mX + app->mBoard->GridToPixelX(s.mPlantCol, s.mPlantRow) + 40);
		const float y = (float)(app->mBoard->mY + app->mBoard->GridToPixelY(s.mPlantCol, s.mPlantRow) + 45);
		if (!Mouse(app, true, SDL_BUTTON_LEFT, x, y) || !Mouse(app, false, SDL_BUTTON_LEFT, x, y))
		{ app->CoopSmokeFail("promoted SDL planting input failed"); return false; }
		s.mPlantIssued = true;
	}
	Plant* plant = nullptr;
	while (app->mBoard->mPlants.IterateNext(plant))
		if (selected && selected->mNetOwnerSlot == 2 && (!selected->mActive || selected->mRefreshing) &&
			!plant->mDead && plant->mIsOnBoard && plant->mSeedType == selected->mPacketType &&
			plant->mPlantCol == s.mPlantCol && plant->mRow == s.mPlantRow && plant->mNetId)
			s.mPlant = plant->mNetId;
	if (s.mPickObserved && s.mPlant && !s.mPlantCaptured)
	{
		if (!s.mCaptureTick) s.mCaptureTick = now;
		if (now - s.mCaptureTick < 150U) return false;
		if (!app->CoopSmokeCaptureFrame("spectator_promoted_plant"))
		{ app->CoopSmokeFail("promoted planting renderer capture failed"); return false; }
		s.mPlantCaptured = true;
		LawnNet::NetTrace("SMOKE_SPECTATOR_PROMOTED_PLANT role=%s packet=%u plant=%u cell=%d,%d slot=2 picked=1 input=SDL_queue", role,
			s.mPlantPacket, s.mPlant, s.mPlantCol, s.mPlantRow);
		if (!observer) net->SendChat(host ? "PROMOTE_HOST_READY" : "PROMOTE_PEER_READY");
	}
	if (observer && s.mPlantCaptured && !s.mVerified && now - s.mPlantTick >= 800U &&
		HasChat(net, 0, "PROMOTE_HOST_READY") && HasChat(net, 1, "PROMOTE_PEER_READY"))
	{
		const std::string temporary = app->mCoopSmokeManifest.mResultPath + ".promotion.tmp";
		std::ofstream result(temporary, std::ios::binary | std::ios::trunc);
		result << "{\"status\":\"pass\",\"test\":\"late-spectator-promotion\",\"readonly\":true,\"signals\":true,\"promoted\":true,\"players\":3,\"bankVerified\":true,\"bankPackets\":10,\"bankHash\":"
			<< s.mBankHash << ",\"packet\":"
			<< s.mPlantPacket << ",\"plant\":" << s.mPlant << "}\n";
		result.close();
		if (!result || !MoveFileExA(temporary.c_str(), app->mCoopSmokeManifest.mResultPath.c_str(), MOVEFILE_WRITE_THROUGH))
		{ app->CoopSmokeFail("promotion proof could not be written atomically"); return false; }
		app->mCoopSmokeResultWritten = true; s.mVerified = true;
		net->SendChat("PROMOTE_DONE");
	}
	if (!observer && s.mPlantCaptured && HasChat(net, 2, "PROMOTE_DONE")) s.mVerified = true;
	if (s.mVerified) LawnNet::NetTrace("SMOKE_SPECTATOR_PROMOTION_COMPLETE role=%s players=3 picked=1 planted=1", role);
	return s.mVerified;
}
}

namespace LawnNet
{
bool NetSpectatorSmokeRunBaseline(LawnApp*)
{
	return !PromotionVariant() || s.mPromotionStage == 0;
}
}

bool LawnApp::CoopSmokeUpdateSpectator()
{
	const char* role = SpectatorRole();
	if (!role) return true;
	if (!mNetManager) return false;
	const uint32_t now = GetTickCount();
	if (PromotionVariant() && (s.mPromotionStage || (s.mCommsVerified &&
		mNetManager->GetPlayer(2).mActive && !mNetManager->GetPlayer(2).mSpectator)))
	{
		if (!s.mPromotionStage) s.mPromotionStage = 1;
		return UpdatePromotion(this, role, now);
	}
	if (!mBoard || mGameScene != SCENE_PLAYING) return false;
	const bool observer = std::string(role) == "spectator";
	if (!observer && !mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots)) return false;
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
			// Bypass only the local input convenience guard, using the public wire
			// sender, to verify the authenticated host rejects observer mutation.
			mNetManager->SendInput(true, 1, (int)x, (int)y);
			mNetManager->SendInput(false, 1, (int)x, (int)y);
			s.mRawIssued = true;
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
		else if (s.mStage == 3 && now - s.mStarted >= 900U)
		{
			const SeedPacket& packet = mBoard->mSeedBank->mSeedPackets[0];
			const float x = (float)(mBoard->mSeedBank->mX + packet.mX + packet.mOffsetX + 20);
			const float y = (float)(mBoard->mSeedBank->mY + packet.mY + 20);
			if (!Key(this, SDLK_LALT, SDL_SCANCODE_LALT, true, SDL_KMOD_LALT) ||
				!Mouse(this, true, SDL_BUTTON_LEFT, x, y) || !Mouse(this, false, SDL_BUTTON_LEFT, x, y))
			{ CoopSmokeFail("spectator SDL Alt seed ping failed"); return false; }
			s.mStage = 4;
		}
		else if (s.mStage == 4 && now - s.mStarted >= 1050U)
		{
			if (!Key(this, SDLK_LALT, SDL_SCANCODE_LALT, false, SDL_KMOD_NONE) || !EnterChat(this))
			{ CoopSmokeFail("spectator SDL chat input failed"); return false; }
			s.mStage = 5;
		}
		else if (s.mStage == 5 && now - s.mStarted >= 1300U)
		{
			if (!Key(this, SDLK_LCTRL, SDL_SCANCODE_LCTRL, true, SDL_KMOD_LCTRL) || !Motion(this, 230, 290))
			{ CoopSmokeFail("spectator SDL draw begin failed"); return false; }
			s.mStage = 6;
		}
		else if (s.mStage == 6 && now - s.mStarted >= 1450U)
		{ if (!Motion(this, 260, 310)) { CoopSmokeFail("spectator draw point failed"); return false; } s.mStage = 7; }
		else if (s.mStage == 7 && now - s.mStarted >= 1600U)
		{ if (!Motion(this, 290, 290)) { CoopSmokeFail("spectator draw point failed"); return false; } s.mStage = 8; }
		else if (s.mStage == 8 && now - s.mStarted >= 1750U)
		{ if (!Key(this, SDLK_LCTRL, SDL_SCANCODE_LCTRL, false, SDL_KMOD_NONE)) { CoopSmokeFail("spectator draw end failed"); return false; } s.mStage = 9; }
		else if (s.mStage == 9 && now - s.mStarted >= 1950U)
		{ if (!Motion(this, 610, 410)) { CoopSmokeFail("spectator cursor move failed"); return false; } s.mStage = 10; }
		if (s.mStage >= 10) ObserveCommunications(this, role, true, now);
		if (s.mStage == 10 && s.mCommsVerified && now - s.mStarted >= 2500U && BoardIdentity(mBoard) == s.mShape &&
			mBoard->mCursorObject && mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL)
		{
			LawnNet::NetTrace("SMOKE_SPECTATOR_READONLY role=spectator shape=%08X noPlantMutation=1 ping=1 input=SDL_queue", s.mShape);
			mNetManager->SendChat("SPECTATOR_READY"); s.mReadySent = true; s.mStage = 11;
		}
		return false; // the real host kick is the success condition for this process
	}
	ObserveCommunications(this, role, false, now);
	if (!mNetManager->IsHosting() && s.mCommsVerified && !s.mReadySent)
	{ mNetManager->SendChat("SPECTATOR_PEER_READY"); s.mReadySent = true; }
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
		if (s.mStage == 0 && s.mJoined && s.mCommsVerified &&
			HasChat(mNetManager, 2, "SPECTATOR_READY") && HasChat(mNetManager, 1, "SPECTATOR_PEER_READY"))
		{
			if (BoardIdentity(mBoard) != s.mShape)
			{ CoopSmokeFail("spectator SDL input mutated host plant/packet identity"); return false; }
			LawnNet::NetTrace("SMOKE_SPECTATOR_READONLY role=host shape=%08X noPlantMutation=1 ping=1 input=SDL_queue", s.mShape);
			if (PromotionVariant())
			{ s.mPromotionStage = 1; return UpdatePromotion(this, role, now); }
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
	if (PromotionVariant() && s.mVerified && mCoopSmokeResultWritten) return true;
	const char* role = SpectatorRole();
	// The ping echo and disconnect can share one TCP pump, before the next
	// smoke tick. Read the already-applied event rather than requiring a frame.
	if (mNetManager)
		for (const auto& a : mNetManager->GetAnnotations())
			if (a.mSlot == 2 && a.mKind == LawnNet::NETANNOTATION_PING && a.mX == 610 && a.mY == 410)
				s.mPingObserved = true;
	if (!role || std::string(role) != "spectator" || !s.mReadOnlyIssued || !s.mRawIssued || !s.mCommsVerified || !s.mReadySent ||
		theReason.find("kicked") == std::string::npos) return false;
	LawnNet::NetTrace("SMOKE_SPECTATOR_KICKED role=spectator reason=host_kick readonly=1 ping=1");
	const std::string temporary = mCoopSmokeManifest.mResultPath + ".spectator.tmp";
	std::ofstream result(temporary, std::ios::binary | std::ios::trunc);
	result << "{\"status\":\"pass\",\"test\":\"late-spectator\",\"readonly\":true,\"ping\":true,\"chat\":true,\"seedPing\":true,\"draw\":true,\"hostKick\":true}\n";
	result.close();
	if (!result || !MoveFileExA(temporary.c_str(), mCoopSmokeManifest.mResultPath.c_str(), MOVEFILE_WRITE_THROUGH))
	{ CoopSmokeFail("spectator kick proof could not be written atomically"); return false; }
	mCoopSmokeResultWritten = true;
	return true;
}
