#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetZombatar.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Widget/GameSelector.h"
#include "../Widget/ZombatarScreen.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
bool Enabled(const char* name)
{
	const char* value = getenv(name);
	return value && std::string(value) == "1";
}

LawnNet::NetZombatarAppearance Expected(int slot)
{
	LawnNet::NetZombatarAppearance a;
	a.mPresent = 1; a.mSkin = slot == 0 ? 4 : 11;
	a.mHair = slot == 0 ? 1 : 15; a.mHairColor = slot == 0 ? 5 : 16;
	a.mHat = slot == 0 ? 3 : 13; a.mHatColor = slot == 0 ? 7 : 16;
	return a;
}

bool AtomicFile(const std::string& path, const std::string& contents)
{
	const std::string temporary = path + ".tmp";
	std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
	file << contents; file.close();
	return file && MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH);
}

bool Click(LawnApp* app, Sexy::Widget* widget)
{
	if (!widget || widget->mDisabled) return false;
	int x = widget->mWidth / 2, y = widget->mHeight / 2;
	for (Sexy::WidgetContainer* parent = widget; parent; parent = parent->mParent)
	{ x += parent->mX; y += parent->mY; }
	SDL_Event e{}; e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	e.button.windowID = SDL_GetWindowID(app->mSDLWindow);
	e.button.button = SDL_BUTTON_LEFT; e.button.down = true; e.button.clicks = 1;
	e.button.x = (float)x; e.button.y = (float)y;
	if (!SDL_PushEvent(&e)) return false;
	e.type = SDL_EVENT_MOUSE_BUTTON_UP; e.button.down = false;
	return SDL_PushEvent(&e);
}

#ifdef _HAS_ZOMBATAR
LawnNet::NetZombatarAppearance Fields(ZombatarWidget* editor)
{
	const auto item = [](int value, int first) -> uint8_t { return value < 0 ? 255 : (uint8_t)(value - first); };
	LawnNet::NetZombatarAppearance a;
	a.mPresent = 1; a.mSkin = (uint8_t)editor->mCurSkinPallete;
	a.mHair = item(editor->mCurHair, ZOMBATAR_HAIR_1); a.mHairColor = (uint8_t)editor->mCurHairPallete;
	a.mTidbit = item(editor->mCurTidbit, ZOMBATAR_TIDBIT_1); a.mTidbitColor = (uint8_t)editor->mCurTidbitPallete;
	a.mEyewear = item(editor->mCurEyeWear, ZOMBATAR_EYEWEAR_1); a.mEyewearColor = (uint8_t)editor->mCurEyeWearPallete;
	a.mClothes = item(editor->mCurClothe, ZOMBATAR_CLOTHE_1);
	a.mAccessory = item(editor->mCurAccessory, ZOMBATAR_ACCESSORY_1); a.mAccessoryColor = (uint8_t)editor->mCurAccessoryPallete;
	a.mHat = item(editor->mCurHat, ZOMBATAR_HAT_1); a.mHatColor = (uint8_t)editor->mCurHatPallete;
	return a;
}
#endif

bool Model(LawnApp* app, Zombie* z, const LawnNet::NetZombatarAppearance& expected)
{
#ifdef _HAS_ZOMBATAR
	if (!z || z->mDead || z->mNetZombatar != expected || !z->mHasHead) return false;
	const ReanimationID ids[] = {z->mBodyReanimID, z->mZombatarTidbitID, z->mZombatarHairID,
		z->mZombatarHairLineID, z->mZombatarHatID, z->mZombatarHatLineID};
	for (ReanimationID id : ids)
	{
		Reanimation* r = app->ReanimationTryToGet(id);
		if (!r || r->mDead || !r->mDefinition || r->mDefinition->mTracks.count <= 0 ||
			!r->mDefinition->mTracks.tracks || !r->mTrackInstances || r->mFrameStart < 0 || r->mFrameCount <= 0 ||
			r->mFrameCount > r->mDefinition->mTracks.tracks[0].mTransforms.count - r->mFrameStart) return false;
	}
	Reanimation* hair = app->ReanimationTryToGet(z->mZombatarHairID);
	Reanimation* hat = app->ReanimationTryToGet(z->mZombatarHatID);
	const std::string hairTrack = "hair_" + std::string(expected.mHair < 10 ? "0" : "") + std::to_string(expected.mHair);
	const std::string hatTrack = "hats_" + std::string(expected.mHat < 10 ? "0" : "") + std::to_string(expected.mHat);
	return hair->TrackExists(hairTrack.c_str()) && hat->TrackExists(hatTrack.c_str()) &&
		hair->GetTrackInstanceByName(hairTrack.c_str())->mRenderGroup != RENDER_GROUP_HIDDEN &&
		hat->GetTrackInstanceByName(hatTrack.c_str())->mRenderGroup != RENDER_GROUP_HIDDEN;
#else
	return false;
#endif
}

Zombie* Find(Board* board, uint16_t id)
{
	Zombie* z = nullptr;
	while (board->mZombies.IterateNext(z)) if (!z->mDead && z->mNetId == id) return z;
	return nullptr;
}

bool Chat(LawnNet::NetManager* net, int slot, const char* text)
{
	for (const auto& line : net->mChatLines) if (line.mSlot == slot && line.mText == text) return true;
	return false;
}
}

namespace LawnNet
{
bool NetSmokeAvatarBeforeConnect(LawnApp* app)
{
	if (!Enabled("PVZ_COOP_SMOKE_SAVED_AVATARS")) return true;
	const int slot = app->mCoopSmokeManifest.IsHost() ? 0 : 1;
	NetConfig disk;
	const char* directory = getenv("PVZ_COOP_SMOKE_CAPTURE_DIR");
	if (!app->mNetManager || app->mNetManager->IsActive() || !directory || !NetLoadConfig(disk) ||
		app->mNetManager->GetMyZombatar() != Expected(slot) || disk.mZombatar != Expected(slot))
	{ app->CoopSmokeFail("saved personal avatar was not loaded before connecting"); return false; }
	const std::string proof = "{\"status\":\"pass\",\"active\":false,\"bytes\":13,\"appearance\":\"" +
		NetZombatarHex(app->mNetManager->GetMyZombatar()) + "\"}\n";
	if (!AtomicFile(std::string(directory) + "\\avatar_before_join.json", proof))
	{ app->CoopSmokeFail("before-join avatar proof could not be written"); return false; }
	return true;
}

bool NetSmokeUpdateAvatarEditor(LawnApp* app)
{
	if (!Enabled("PVZ_COOP_SMOKE_AVATAR_EDITOR")) return false;
#ifdef _HAS_ZOMBATAR
	static int stage = 0;
	static uint32_t started = 0, action = 0;
	const uint32_t now = GetTickCount();
	if (!started) started = now;
	if (now - started > 90000U) { app->CoopSmokeFail("native avatar editor SDL workflow timed out"); return true; }
	if (!app->mNetManager || app->mNetManager->IsActive())
	{ app->CoopSmokeFail("avatar editor fixture unexpectedly joined a network session"); return true; }
	GameSelector* selector = app->mGameSelector;
	if (!selector || selector->mSlideCounter) return true;
	ZombatarWidget* editor = selector->mZombatarWidget;
	const bool host = app->mCoopSmokeManifest.IsHost();
	const auto expected = Expected(host ? 0 : 1);
	const char* role = host ? "host" : "client";
	if (!stage) { selector->ShowZombatarScreen(); stage = 1; action = now; return true; }
	if (!editor || now - action < 250U) return true;
	Sexy::Widget* button = nullptr;
	switch (stage)
	{
	case 1: button = editor->mColorPalletes[expected.mSkin]; break;
	case 2: button = editor->mHairButton; break;
	case 3: button = editor->mZombatarItems[ZOMBATAR_HAIR_1 + expected.mHair]; break;
	case 4: button = editor->mColorPalletes[expected.mHairColor]; break;
	case 5: button = editor->mHatsButton; break;
	case 6: button = editor->mZombatarItems[ZOMBATAR_HAT_1 + expected.mHat]; break;
	case 7: button = editor->mColorPalletes[expected.mHatColor]; break;
	case 8:
		if (Fields(editor) != expected || !app->CoopSmokeCaptureFrame("avatar_editor_fields"))
		{ app->CoopSmokeFail("SDL editor fields did not match the selected personal appearance"); return true; }
		button = editor->mFinishButton; break;
	case 9:
	{
		NetConfig disk;
		if (selector->mDestX != 0 || app->mNetManager->GetMyZombatar() != expected ||
			!NetLoadConfig(disk) || disk.mZombatar != expected)
		{ app->CoopSmokeFail("native editor Finish did not save the exact personal INI appearance"); return true; }
		selector->ShowZombatarScreen(); stage = 10; action = now; return true;
	}
	case 10:
		if (Fields(editor) != expected || !app->CoopSmokeCaptureFrame("avatar_editor_reopened"))
		{ app->CoopSmokeFail("native editor Reset did not restore the saved personal appearance"); return true; }
		if (!AtomicFile(app->mCoopSmokeManifest.mResultPath,
			"{\"status\":\"pass\",\"test\":\"avatar-editor\",\"input\":\"SDL_queue\",\"bytes\":13,\"appearance\":\"" + NetZombatarHex(Fields(editor)) + "\"}\n"))
		{ app->CoopSmokeFail("avatar editor proof could not be written atomically"); return true; }
		NetTrace("SMOKE_AVATAR_EDITOR role=%s saved=1 reset=1 bytes=13 appearance=%s input=SDL_queue", role, NetZombatarHex(Fields(editor)).c_str());
		app->mCoopSmokeResultWritten = true; app->CloseRequestAsync(); return true;
	default: return true;
	}
	if (!Click(app, button)) { app->CoopSmokeFail("avatar editor SDL button click failed"); return true; }
	++stage; action = now;
#else
	app->CoopSmokeFail("avatar editor fixture requires a Zombatar-enabled build");
#endif
	return true;
}

bool NetSmokeUpdateSavedAvatars(LawnApp* app)
{
	static uint32_t started = 0, observed = 0, captured = 0, announced = 0, acknowledged = 0;
	static uint16_t ids[2] = {};
	static int attempts = 0;
	static bool picture = false, ack = false;
	if (!app->mBoard || !app->mNetManager || app->mGameScene != SCENE_PLAYING ||
		!app->mCoopSmokeScenarioRuntime.IsComplete(app->mCoopSmokeManifest.mMinimumSnapshots)) return false;
	const uint32_t now = GetTickCount();
	if (!started) started = now;
	if (now - started > 30000U) { app->CoopSmokeFail("saved avatar union/peer observation timed out"); return false; }
	const bool host = app->mCoopSmokeManifest.IsHost();
	const char* role = host ? "host" : "client";
	if (app->mNetManager->GetMyZombatar() != Expected(host ? 0 : 1))
	{ app->CoopSmokeFail("joining the host replaced the client's personal saved appearance"); return false; }
	if (app->mNetManager->GetPlayerZombatar(0) != Expected(0) || app->mNetManager->GetPlayerZombatar(1) != Expected(1)) return false;
	if (host && !ids[0])
	{
		// Exercise the production choice across both active personal appearances.
		// Never call SetMyZombatar or ApplyNetZombatar in the restarted pair.
		for (; attempts < 128 && (!ids[0] || !ids[1]); ++attempts)
		{
			Zombie* z = app->mBoard->AddZombieInRow(ZOMBIE_NORMAL, 2, 0);
			if (!z) { app->CoopSmokeFail("saved avatar native spawn allocation failed"); return false; }
			int owner = z->mNetZombatar == Expected(0) ? 0 : (z->mNetZombatar == Expected(1) ? 1 : -1);
			if (owner >= 0 && !ids[owner])
			{
				ids[owner] = z->mNetId; z->mPosX = (float)(500 + owner * 150); z->mX = (int)z->mPosX;
				z->mBodyHealth = z->mBodyMaxHealth = 3000; z->mVelX = 0.0f;
			}
			else z->DieNoLoot();
		}
		if (!ids[0] || !ids[1]) { app->CoopSmokeFail("natural initialization did not select both saved personal appearances within 128 attempts"); return false; }
	}
	const std::string notice = "SAVED_AVATARS " + std::to_string(ids[0]) + " " + std::to_string(ids[1]);
	if (host && ids[0] && ids[1] && !Chat(app->mNetManager, 0, notice.c_str()) &&
		now - started >= 800U && (!announced || now - announced >= 800U))
	{ app->mNetManager->SendChat(notice); announced = now; }
	if (!host && !ids[0])
		for (const auto& line : app->mNetManager->mChatLines)
			if (line.mSlot == 0 && line.mText.compare(0, 14, "SAVED_AVATARS ") == 0)
			{
				std::istringstream input(line.mText); std::string tag; int hostId = 0, clientId = 0;
				if (input >> tag >> hostId >> clientId) { ids[0] = (uint16_t)hostId; ids[1] = (uint16_t)clientId; }
			}
	if (!Model(app, Find(app->mBoard, ids[0]), Expected(0)) || !Model(app, Find(app->mBoard, ids[1]), Expected(1))) return false;
	if (!observed) observed = now;
	if (now - observed < 150U) return false;
	if (!picture)
	{
		if (!app->CoopSmokeCaptureFrame("avatar_saved_union")) { app->CoopSmokeFail("saved avatar union renderer capture failed"); return false; }
		picture = true; captured = now;
		NetTrace("SMOKE_SAVED_AVATARS role=%s hostZombie=%u clientZombie=%u hostAppearance=%s clientAppearance=%s bytes=13 attempts=%d source=natural_init",
			role, ids[0], ids[1], NetZombatarHex(Find(app->mBoard, ids[0])->mNetZombatar).c_str(),
			NetZombatarHex(Find(app->mBoard, ids[1])->mNetZombatar).c_str(), attempts);
	}
	if (!host && !ack && now - captured >= 800U)
	{
		ack = Chat(app->mNetManager, 1, "SAVED_AVATARS_ACK");
		if (!ack && (!acknowledged || now - acknowledged >= 800U))
		{ app->mNetManager->SendChat("SAVED_AVATARS_ACK"); acknowledged = now; }
	}
	if (host && !Chat(app->mNetManager, 1, "SAVED_AVATARS_ACK")) return false;
	return (host || ack) && now - captured >= 300U;
}
}
