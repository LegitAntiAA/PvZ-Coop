#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../SeedPacket.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct Card
{
	uint16_t id = 0;
	int type = -1, imitater = -1, owner = -1, origin = 0, cost = -1;
	int time = 0, cooldown = 255;
	bool refreshing = false, active = false;
};
struct ObserverState
{
	uint32_t started = 0, lastSend = 0, introTick = 0, seenTick = 0, transferTick = 0, purchaseTick = 0, pickTick = 0;
	uint32_t shape = 0, runtime = 0, introShape = 0;
	bool waitLogged = false, pauseIssued = false, resumeIssued = false, introCaptured = false, introDone = false;
	bool seenCaptured = false, seenVerified = false, rawIssued = false, armed = false, kickIssued = false, transferCaptured = false;
	bool pickIssued = false, plantIssued = false, purchaseCaptured = false, complete = false;
	uint16_t target = 0, plant = 0;
	int targetCost = 0, targetTime = 0, targetCooldown = 255, targetOrigin = 0, targetRefreshing = 0, targetActive = 0;
	int col = -1, row = -1, sunBefore = 0;
	std::vector<Card> before, introCards;
};
ObserverState s;

const char* Variant()
{
	const char* value = getenv("PVZ_COOP_SMOKE_MYSTERY_OBSERVER");
	return value && (!strcmp(value, "IntroJoin") || !strcmp(value, "PlayingKick")) ? value : nullptr;
}
const char* Role()
{
	const char* value = getenv("PVZ_COOP_SMOKE_MYSTERY_OBSERVER_ROLE");
	return value ? value : "";
}
bool Intro() { return Variant() && !strcmp(Variant(), "IntroJoin"); }
bool Fail(LawnApp* app, const char* reason) { app->CoopSmokeFail(reason); return false; }
bool Context(LawnApp* app)
{
	if (!app || !app->CoopSmokeActive() || !Variant()) return false;
	if (!s.started) s.started = GetTickCount();
	if (app->mCoopSmokeManifest.mScenario != LawnNet::NETSMOKE_SCENARIO_ADVENTURE_8_SOAK ||
		app->mCoopSmokeManifest.mSkipIntro || app->mWidth != 800 || app->mHeight != 600)
		return Fail(app, "mystery observer requires isolated full-intro Adventure8Soak800");
	if (GetTickCount() - s.started > 180000U) return Fail(app, "mystery observer checkpoint timed out");
	return true;
}
bool Chat(LawnApp* app, int slot, const std::string& text)
{
	for (const auto& line : app->mNetManager->mChatLines)
		if (line.mSlot == slot && line.mText == text) return true;
	return false;
}
bool Publish(LawnApp* app, const std::string& text)
{
	if (Chat(app, app->mNetManager->GetMySlot(), text)) return true;
	const uint32_t now = GetTickCount();
	if (!s.lastSend || now - s.lastSend >= 800U)
	{ app->mNetManager->SendChat(text); s.lastSend = GetTickCount(); }
	return false;
}
bool HostCommand(LawnApp* app, const char* prefix, std::string& text)
{
	bool found = false;
	for (const auto& line : app->mNetManager->mChatLines)
		if (line.mSlot == 0 && line.mText.compare(0, strlen(prefix), prefix) == 0)
		{ text = line.mText; found = true; }
	return found;
}
bool Escape(LawnApp* app)
{
	SDL_SetModState(SDL_KMOD_NONE);
	SDL_Event e{}; e.type = SDL_EVENT_KEY_DOWN; e.key.windowID = SDL_GetWindowID(app->mSDLWindow);
	e.key.key = SDLK_ESCAPE; e.key.scancode = SDL_SCANCODE_ESCAPE; e.key.down = true;
	if (!SDL_PushEvent(&e)) return false;
	e.type = SDL_EVENT_KEY_UP; e.key.down = false;
	return SDL_PushEvent(&e);
}
bool Click(LawnApp* app, int x, int y)
{
	if (app->mWidgetManager->mLastMouseX != x || app->mWidgetManager->mLastMouseY != y)
	{
		SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
		motion.motion.x = (float)x; motion.motion.y = (float)y; SDL_PushEvent(&motion); return false;
	}
	SDL_SetModState(SDL_KMOD_NONE);
	SDL_Event e{}; e.type = SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.windowID = SDL_GetWindowID(app->mSDLWindow);
	e.button.button = SDL_BUTTON_LEFT; e.button.down = true; e.button.clicks = 1; e.button.x = (float)x; e.button.y = (float)y;
	if (!SDL_PushEvent(&e)) return false;
	e.type = SDL_EVENT_MOUSE_BUTTON_UP; e.button.down = false;
	return SDL_PushEvent(&e);
}
bool Capture(LawnApp* app, const char* name, bool& captured, uint32_t& observed)
{
	if (captured) return true;
	if (!observed) observed = GetTickCount();
	if (GetTickCount() - observed < 150U) return false;
	if (!app->CoopSmokeCaptureFrame(name)) return Fail(app, "mystery observer renderer capture failed");
	captured = true; return true;
}
SeedPacket* Packet(LawnApp* app, uint16_t id)
{
	if (!app->mBoard || !app->mBoard->mSeedBank) return nullptr;
	for (int i = 0; i < app->mBoard->mSeedBank->mNumPackets; ++i)
		if (app->mBoard->mSeedBank->mSeedPackets[i].mNetId == id) return &app->mBoard->mSeedBank->mSeedPackets[i];
	return nullptr;
}
bool Cards(LawnApp* app, bool pending, int owner, std::vector<Card>& cards)
{
	if (!app->mBoard || !app->mBoard->mSeedBank || app->mBoard->mSeedBank->mNumPackets != 10) return false;
	int random = 0;
	for (int i = 0; i < 10; ++i)
	{
		const SeedPacket& packet = app->mBoard->mSeedBank->mSeedPackets[i];
		if (!packet.mNetId) return false;
		Card card; card.id = packet.mNetId; card.type = packet.mPacketType; card.imitater = packet.mImitaterType;
		card.owner = packet.mNetOwnerSlot; card.origin = packet.mRandomDiscountLevel;
		card.time = packet.mRefreshTime; card.cooldown = card.time ? packet.mRefreshCounter * 255 / card.time : 255;
		card.refreshing = packet.mRefreshing; card.active = packet.mActive;
		if (card.origin)
		{
			++random;
			if (card.origin < 1 || card.origin > 10 || card.owner != owner || packet.IsMysteryPending() != pending) return false;
			if (pending && (card.type != SEED_NONE || card.imitater != SEED_NONE || card.active || card.time || card.refreshing)) return false;
		}
		if (!pending || !card.origin) card.cost = app->mBoard->GetSeedPacketCost(packet);
		cards.push_back(card);
	}
	std::sort(cards.begin(), cards.end(), [](const Card& a, const Card& b) { return a.id < b.id; });
	for (size_t i = 1; i < cards.size(); ++i) if (cards[i - 1].id == cards[i].id) return false;
	return random == 5;
}
uint32_t Hash(const std::vector<Card>& cards, bool runtime)
{
	LawnNet::NetBuffer b;
	for (const Card& card : cards)
	{
		b.WriteU16(card.id); b.WriteU8((uint8_t)card.type); b.WriteU8((uint8_t)card.imitater);
		b.WriteU8((uint8_t)card.owner); b.WriteU8((uint8_t)card.origin);
		if (runtime) { b.WriteI32(card.time); b.WriteU8(card.refreshing ? 1 : 0); b.WriteU8(card.active ? 1 : 0); b.WriteI32(card.cost); }
	}
	return LawnNet::NetFnv1a32(b.Data(), b.Size());
}
bool Proof(LawnApp* app, const char* stem, const std::string& fields)
{
	const std::string result = app->mCoopSmokeManifest.mResultPath;
	const std::string path = result.substr(0, result.find_last_of("/\\") + 1) + stem + ".json";
	std::ofstream file(path + ".tmp", std::ios::binary | std::ios::trunc);
	file << "{\"status\":\"pass\",\"variant\":\"" << Variant() << "\",\"role\":\"" << Role() << "\"," << fields << "}\n";
	file.close();
	return file && MoveFileExA((path + ".tmp").c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) ? true : Fail(app, "mystery observer atomic proof failed");
}
bool Seen(LawnApp* app, bool observer)
{
	if (s.seenVerified) return true;
	std::vector<Card> cards;
	if (!Cards(app, false, 1, cards)) return false;
	const uint32_t shape = Hash(cards, false), runtime = Hash(cards, true);
	if (app->mNetManager->IsHosting())
	{
		s.shape = shape; s.runtime = runtime;
		if (!Publish(app, "MO_SPENT " + std::to_string(shape) + " " + std::to_string(runtime))) return false;
	}
	else
	{
		std::string command, tag; uint32_t expected = 0, expectedRuntime = 0;
		if (!HostCommand(app, "MO_SPENT ", command)) return false;
		std::istringstream input(command); input >> tag >> expected >> expectedRuntime;
		if (!input || shape != expected || runtime != expectedRuntime) return false;
		s.shape = expected; s.runtime = expectedRuntime;
	}
	if (!Capture(app, "mystery_observer_reveal", s.seenCaptured, s.seenTick)) return false;
	if (s.before.empty())
	{
		s.before = cards;
		LawnNet::NetTrace("SMOKE_MO_SEEN role=%s shape=%08X cooldown=%08X originCards=5 owner=1 phase=playing capture=1", Role(), shape, runtime);
	}
	const std::string notice = "MO_SEEN " + std::to_string(shape) + " " + std::to_string(runtime);
	if (!Publish(app, notice)) return false;
	for (int slot = 0; slot < 3; ++slot) if (!Chat(app, slot, notice)) return false;
	if (observer && !s.rawIssued && !app->mBoard->mPaused)
	{
		const SeedPacket& packet = app->mBoard->mSeedBank->mSeedPackets[0];
		const int x = app->mBoard->mSeedBank->mX + packet.mX + 20, y = app->mBoard->mSeedBank->mY + packet.mY + 20;
		if (!Click(app, x, y)) return false;
		// Exactly two authenticated wire frames test the server role guard.
		app->mNetManager->SendInput(true, 1, x, y); app->mNetManager->SendInput(false, 1, x, y);
		s.rawIssued = true;
		LawnNet::NetTrace("SMOKE_MO_READONLY role=observer slot=2 owned=0 rawFrames=2 input=SDL_queue+authenticated_sender");
	}
	s.seenVerified = true; return true;
}
bool IntroCheckpoint(LawnApp* app)
{
	if (s.introDone) return true;
	if (app->mNetManager->IsHosting() && !s.pauseIssued) { app->DoNewOptions(false); s.pauseIssued = true; }
	if (!app->mBoard || !app->mBoard->mPaused) return false;
	std::vector<Card> cards;
	if (!Cards(app, true, 1, cards)) return Fail(app, "intro observer checkpoint lost its five pending cards");
	s.introShape = Hash(cards, false);
	if (!s.waitLogged)
	{
		s.waitLogged = true;
		LawnNet::NetTrace("SMOKE_MO_WAIT role=%s phase=intro_pending pause=1 variant=IntroJoin fixture=native_options_pause", Role());
	}
	if (!app->mNetManager->IsHosting()) Publish(app, "MO_INTRO_PEER " + std::to_string(s.introShape));
	if (!Chat(app, 2, "MO_INTRO " + std::to_string(s.introShape)) ||
		!Chat(app, 1, "MO_INTRO_PEER " + std::to_string(s.introShape))) return false;
	if (app->mNetManager->IsHosting() && !s.resumeIssued)
	{ if (!Escape(app)) return Fail(app, "intro observer SDL resume failed"); s.resumeIssued = true; }
	return false; // The next call observes the authoritative unpause below.
}
bool Transfer(LawnApp* app)
{
	std::vector<Card> cards;
	if (!Cards(app, false, 0, cards)) return false;
	for (const Card& old : s.before)
	{
		if (!old.origin) continue;
		SeedPacket* packet = Packet(app, old.id);
		if (!packet || packet->mNetOwnerSlot != 0 || packet->mPacketType != old.type || packet->mImitaterType != old.imitater ||
			packet->mRandomDiscountLevel != old.origin || app->mBoard->GetSeedPacketCost(*packet) != old.cost)
			return Fail(app, "active-owner kick changed a card identity, origin or discounted price");
	}
	SeedPacket* packet = Packet(app, s.target);
	if (!packet || packet->mRefreshTime != s.targetTime || packet->mRefreshing != (s.targetRefreshing != 0) || packet->mActive != (s.targetActive != 0))
		return Fail(app, "active-owner kick reset the authoritative cooldown state");
	const int fraction = packet->mRefreshTime ? packet->mRefreshCounter * 255 / packet->mRefreshTime : 255;
	if (abs(fraction - s.targetCooldown) > 2) return Fail(app, "paused kick changed cooldown progress");
	if (!Capture(app, "mystery_observer_transfer", s.transferCaptured, s.transferTick)) return false;
	LawnNet::NetTrace("SMOKE_MO_TRANSFER role=%s id=%u origin=%d beforeOwner=1 owner=0 cooldownTime=%d refreshing=%d preserved=1 price=%d capture=1",
		Role(), s.target, s.targetOrigin, packet->mRefreshTime, packet->mRefreshing ? 1 : 0, s.targetCost);
	return true;
}
bool Purchase(LawnApp* app, bool observer)
{
	if (s.purchaseCaptured) return true;
	SeedPacket* packet = Packet(app, s.target);
	if (!packet) return false;
	if (!observer && !s.pickIssued)
	{
		if (!packet->CanPickUp() || app->GetDialogCount() || app->mBoard->mSunMoney < s.targetCost) return false;
		if (s.col < 0)
			for (int row = 0; row < MAX_GRID_SIZE_Y && s.col < 0; ++row)
				for (int col = 0; col < 9 && s.col < 0; ++col)
					if (app->mBoard->CanPlantAt(col, row, packet->mPacketType) == PLANTING_OK)
					{
						const int x = app->mBoard->GridToPixelX(col, row) + 40, y = app->mBoard->GridToPixelY(col, row) + 40;
						bool clear = true; Coin* coin = nullptr;
						while (app->mBoard->mCoins.IterateNext(coin))
							if (!coin->mDead && abs((int)coin->mPosX - x) < 70 && abs((int)coin->mPosY - y) < 150) clear = false;
						if (clear) { s.col = col; s.row = row; s.sunBefore = app->mBoard->mSunMoney; }
					}
		if (s.col < 0) return Fail(app, "no native plantable cell for the transferred mystery card");
		const std::string notice = "MO_PLANT " + std::to_string(s.target) + " " + std::to_string(s.col) + " " + std::to_string(s.row) + " " + std::to_string(s.targetCost) + " " + std::to_string(s.sunBefore);
		if (!Publish(app, notice)) return false;
		if (!Click(app, app->mBoard->mSeedBank->mX + packet->mX + packet->mOffsetX + 25, app->mBoard->mSeedBank->mY + packet->mY + 35)) return false;
		s.pickIssued = true; s.pickTick = GetTickCount();
	}
	if (!observer && s.pickIssued && !s.plantIssued && app->mBoard->mCursorObject->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK && GetTickCount() - s.pickTick >= 300U)
	{
		if (!Click(app, app->mBoard->mX + app->mBoard->GridToPixelX(s.col, s.row) + 40, app->mBoard->mY + app->mBoard->GridToPixelY(s.col, s.row) + 40)) return false;
		s.plantIssued = true;
	}
	if (observer && s.col < 0)
	{
		std::string command, tag; int id = 0;
		if (!HostCommand(app, "MO_PLANT ", command)) return false;
		std::istringstream input(command); input >> tag >> id >> s.col >> s.row >> s.targetCost >> s.sunBefore;
		if (!input || id != s.target) return Fail(app, "transferred purchase command identity differs");
	}
	Plant* plant = nullptr;
	while (app->mBoard->mPlants.IterateNext(plant))
		if (!plant->mDead && plant->mIsOnBoard && plant->mSeedType == packet->mPacketType && plant->mPlantCol == s.col && plant->mRow == s.row) s.plant = plant->mNetId;
	if (!s.plant || app->mBoard->mSunMoney != s.sunBefore - s.targetCost || !packet->mRefreshing ||
		packet->mRefreshTime != Plant::GetRefreshTime(packet->mPacketType, packet->mImitaterType)) return false;
	if (!Capture(app, "mystery_observer_purchase", s.purchaseCaptured, s.purchaseTick)) return false;
	LawnNet::NetTrace("SMOKE_MO_PURCHASE role=%s packet=%u plant=%u cell=%d,%d displayed=%d spent=%d origin=%d sdl=1",
		Role(), s.target, s.plant, s.col, s.row, s.targetCost, s.sunBefore - app->mBoard->mSunMoney, s.targetOrigin);
	return true;
}
bool KickCheckpoint(LawnApp* app, bool observer)
{
	LawnNet::NetManager* net = app->mNetManager;
	const bool host = net->IsHosting();
	if (host && !s.pauseIssued) { app->DoNewOptions(false); s.pauseIssued = true; }
	if (!s.transferCaptured && !app->mBoard->mPaused) return false;
	if (host && !s.waitLogged)
	{ s.waitLogged = true; LawnNet::NetTrace("SMOKE_MO_WAIT role=host phase=playing pause=1 variant=PlayingKick fixture=native_options_pause"); }
	if (!s.seenVerified && (net->GetPlayerCount() != 3 || !net->GetPlayer(2).mActive || !net->GetPlayer(2).mSpectator))
		return false;
	if (!s.seenVerified && !Seen(app, observer)) return false;
	if (!s.target)
	{
		if (host)
		{
			for (const Card& card : s.before)
				if (card.origin && card.type != SEED_IMITATER && card.cost > 0 && card.refreshing && !card.active && card.cooldown > 2)
				{
					s.target = card.id; s.targetCost = card.cost; s.targetTime = card.time; s.targetCooldown = card.cooldown;
					s.targetOrigin = card.origin; s.targetRefreshing = 1; s.targetActive = 0; break;
				}
			if (!s.target) return Fail(app, "no positively priced refreshing mystery packet for native kick retention");
		}
		else
		{
			std::string command, tag; int id = 0;
			if (!HostCommand(app, "MO_KICK ", command)) return false;
			std::istringstream input(command); input >> tag >> id >> s.targetCost >> s.targetTime >> s.targetCooldown >> s.targetOrigin >> s.targetRefreshing >> s.targetActive;
			if (!input || id <= 0 || id > 65535) return Fail(app, "invalid mystery owner-kick command");
			s.target = (uint16_t)id;
		}
	}
	const std::string command = "MO_KICK " + std::to_string(s.target) + " " + std::to_string(s.targetCost) + " " + std::to_string(s.targetTime) + " " + std::to_string(s.targetCooldown) + " " + std::to_string(s.targetOrigin) + " " + std::to_string(s.targetRefreshing) + " " + std::to_string(s.targetActive);
	if (host && !Publish(app, command)) return false;
	if (!host && net->GetPlayer(1).mActive)
	{
		s.armed = true;
		if (!Publish(app, "MO_ARM_" + std::to_string(net->GetMySlot()) + " " + std::to_string(s.target))) return false;
	}
	if (!observer && !host) return false; // The active owner completes only on the authenticated kick reason.
	if (host && !s.kickIssued)
	{
		if (!Chat(app, 1, "MO_ARM_1 " + std::to_string(s.target)) || !Chat(app, 2, "MO_ARM_2 " + std::to_string(s.target))) return false;
		if (!Click(app, 196, 214)) return false;
		s.kickIssued = true;
	}
	if (net->GetPlayer(1).mActive) return false;
	if (!s.transferCaptured && !Transfer(app)) return false;
	const std::string transferred = "MO_TRANSFER " + std::to_string(s.target);
	if (!Publish(app, transferred) || !Chat(app, host ? 2 : 0, transferred)) return false;
	if (host && !s.resumeIssued) { if (!Escape(app)) return Fail(app, "post-kick SDL resume failed"); s.resumeIssued = true; }
	if (app->mBoard->mPaused) return false;
	if (observer && !s.rawIssued)
	{
		const SeedPacket& first = app->mBoard->mSeedBank->mSeedPackets[0];
		const int x = app->mBoard->mSeedBank->mX + first.mX + 20, y = app->mBoard->mSeedBank->mY + first.mY + 20;
		if (!Click(app, x, y)) return false;
		net->SendInput(true, 1, x, y); net->SendInput(false, 1, x, y); s.rawIssued = true;
		LawnNet::NetTrace("SMOKE_MO_READONLY role=observer slot=2 owned=0 rawFrames=2 input=SDL_queue+authenticated_sender");
	}
	return Purchase(app, observer);
}
bool FinishObserver(LawnApp* app)
{
	const std::string done = std::string("MO_DONE ") + Variant();
	if (!Publish(app, done)) return false;
	if (!s.complete)
	{
		if (!Proof(app, "mystery_observer", "\"readonly\":true,\"introJoin\":" + std::string(Intro() ? "true" : "false") +
			",\"rawFrames\":2,\"shape\":" + std::to_string(s.shape) + ",\"cooldownHash\":" + std::to_string(s.runtime) +
			",\"packet\":" + std::to_string(s.target) + ",\"plant\":" + std::to_string(s.plant))) return false;
		app->CoopSmokeWriteResult(true, nullptr); s.complete = true;
	}
	return true;
}
}

namespace LawnNet
{
bool NetSmokeMysteryObserverCheckpoint(LawnApp* app, const char* point)
{
	if (!Variant()) return true;
	if (!Context(app) || !app->mNetManager || !app->mBoard) return false;
	if (!strcmp(point, "intro_pending"))
	{
		if (!Intro()) return true;
		if (s.resumeIssued || (Chat(app, 2, "MO_INTRO " + std::to_string(s.introShape)) && !app->mBoard->mPaused))
			if (!app->mBoard->mPaused) { s.introDone = true; return true; }
		return IntroCheckpoint(app);
	}
	if (!strcmp(point, "spent"))
	{
		if (Intro()) return Seen(app, false);
		return KickCheckpoint(app, false);
	}
	if (!strcmp(point, "final")) return Chat(app, 2, std::string("MO_DONE ") + Variant());
	return Fail(app, "unknown mystery observer checkpoint");
}

bool NetSmokeUpdateMysteryObserver(LawnApp* app)
{
	if (!Variant() || strcmp(Role(), "observer")) return false;
	if (!Context(app) || app->mCoopSmokeResultWritten || !app->mNetManager || !app->mBoard) return true;
	NetManager* net = app->mNetManager;
	const bool afterKick = !Intro() && s.armed && !net->GetPlayer(1).mActive && net->GetPlayerCount() == 2;
	if (net->GetMySlot() != 2 || !net->IsSpectator() || (net->GetPlayerCount() != 3 && !afterKick))
	{ Fail(app, "mystery late peer is not read-only observer slot2"); return true; }
	if (Intro() && !s.introCaptured)
	{
		if (app->mGameScene != SCENE_LEVEL_INTRO || !app->mBoard->mPaused)
		{ Fail(app, "IntroJoin did not bootstrap inside the native paused intro"); return true; }
		std::vector<Card> cards;
		if (!Cards(app, true, 1, cards)) return true;
		s.introShape = Hash(cards, false); s.introCards = cards;
		if (!Capture(app, "mystery_observer_intro", s.introCaptured, s.introTick)) return true;
		NetTrace("SMOKE_MO_INTRO role=observer scene=intro pending=5 realTypes=0 hash=%08X capture=1 fixturePause=1", s.introShape);
	}
	if (Intro() && !Chat(app, 2, "MO_INTRO " + std::to_string(s.introShape)))
	{ Publish(app, "MO_INTRO " + std::to_string(s.introShape)); return true; }
	if (app->mGameScene != SCENE_PLAYING) return true;
	if (Intro())
	{
		if (!s.introCards.empty())
			for (const Card& old : s.introCards)
			{
				SeedPacket* packet = Packet(app, old.id);
				if (!packet || packet->mNetOwnerSlot != old.owner || packet->mRandomDiscountLevel != old.origin || packet->IsMysteryPending())
				{ Fail(app, "observer reveal did not preserve pending card identity/origin"); return true; }
			}
		if (!Seen(app, true) || !s.rawIssued) return true;
		FinishObserver(app); return true;
	}
	if (KickCheckpoint(app, true) && s.rawIssued) FinishObserver(app);
	return true;
}

bool NetSmokeMysteryObserverHostLost(LawnApp* app, const std::string& reason)
{
	if (!Variant() || Intro() || strcmp(Role(), "client") || !s.armed || !app->mNetManager || app->mNetManager->GetMySlot() != 1 ||
		reason.find("kicked by the host") == std::string::npos) return false;
	if (Proof(app, "mystery_observer_owner_kick", "\"hostKick\":true,\"slot\":1,\"packet\":" + std::to_string(s.target) + ",\"origin\":" + std::to_string(s.targetOrigin)))
	{
		NetTrace("SMOKE_MO_KICKED role=client slot=1 reason=host_kick id=%u origin=%d armed=1", s.target, s.targetOrigin);
		app->CoopSmokeWriteResult(true, nullptr);
	}
	return true;
}
}
