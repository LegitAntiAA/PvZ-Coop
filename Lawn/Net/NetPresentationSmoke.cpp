#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetZombatar.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../GridItem.h"
#include "../ZenGarden.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <cstdlib>
#include <sstream>
#include <string>

namespace LawnNet { bool NetSmokeUpdateSavedAvatars(LawnApp* theApp); }

namespace
{
enum PresentationStage { NORMAL = 1, ARM, HEAD, JACK_POP, JACK_BLAST, SNAIL, REGISTRY, AVATAR, RETAINED, COMPLETE };
const char* const kNames[] = {"", "normal", "arm", "head", "jack_pop", "jack_blast", "snail", "registry", "avatar", "retained", "complete"};
struct PresentationState
{
	uint32_t mStarted = 0, mEntered = 0, mObserved = 0, mLastAck = 0;
	int mStage = 0, mSpawnAttempts = 0;
	uint16_t mEntity = 0, mNormal = 0, mJack = 0, mAvatar = 0;
	ReanimationID mSleeper = REANIMATIONID_NULL;
	float mSleepStart = 0.0f;
	bool mCaptured = false, mAcked = false, mActionIssued = false, mPopExtended = false;
};
PresentationState s;

LawnNet::NetZombatarAppearance Appearance()
{
	LawnNet::NetZombatarAppearance a;
	a.mPresent = 1; a.mSkin = 11;
	a.mHair = 15; a.mHairColor = 16; a.mTidbit = 13; a.mTidbitColor = 16;
	a.mEyewear = 15; a.mEyewearColor = 16; a.mClothes = 11;
	a.mAccessory = 14; a.mAccessoryColor = 16; a.mHat = 13; a.mHatColor = 16;
	return a;
}

bool HasChat(LawnNet::NetManager* net, int slot, const std::string& text)
{
	for (const auto& line : net->mChatLines)
		if (line.mSlot == slot && line.mText == text) return true;
	return false;
}

std::string Message(const char* prefix, int stage, uint16_t id)
{
	return std::string(prefix) + " " + std::to_string(stage) + " " + std::to_string(id);
}

Zombie* ZombieById(Board* board, uint16_t id)
{
	Zombie* z = nullptr;
	while (board->mZombies.IterateNext(z))
		if (!z->mDead && z->mNetId == id) return z;
	return nullptr;
}

GridItem* GridById(Board* board, uint16_t id)
{
	GridItem* g = nullptr;
	while (board->mGridItems.IterateNext(g))
		if (!g->mDead && g->mNetId == id) return g;
	return nullptr;
}

int Particles(LawnApp* app, ParticleEffect type)
{
	if (!app->mEffectSystem || !app->mEffectSystem->mParticleHolder) return 0;
	int count = 0;
	TodParticleSystem* p = nullptr;
	while (app->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(p))
		if (!p->mDead && p->mParticleDef && p->mEffectType == type) ++count;
	return count;
}

bool ReanimBounds(Reanimation* r)
{
	if (!r || r->mDead || !r->mDefinition || !r->mTrackInstances ||
		!r->mDefinition->mTracks.tracks || r->mDefinition->mTracks.count <= 0 ||
		r->mFrameStart < 0 || r->mFrameCount <= 0) return false;
	const int frames = r->mDefinition->mTracks.tracks[0].mTransforms.count;
	return r->mFrameStart < frames && r->mFrameCount <= frames - r->mFrameStart;
}

bool VisibleTrack(Reanimation* r, const char* name)
{
	return ReanimBounds(r) && r->TrackExists(name) &&
		r->GetTrackInstanceByName(name)->mRenderGroup != RENDER_GROUP_HIDDEN;
}

bool AvatarAssets(LawnApp* app, Zombie* z)
{
#ifdef _HAS_ZOMBATAR
	if (!z || z->mNetZombatar != Appearance()) return false;
	const ReanimationID ids[] = {z->mZombatarTidbitID, z->mZombatarHairID,
		z->mZombatarHairLineID, z->mZombatarEyeWearID, z->mZombatarEyeWearLineID,
		z->mZombatarAccessoryID, z->mZombatarHatID, z->mZombatarHatLineID};
	for (ReanimationID id : ids)
	{
		Reanimation* r = app->ReanimationTryToGet(id);
		if (!ReanimBounds(r) || r->mReanimationType != REANIM_ZOMBATAR) return false;
	}
	Reanimation* head = app->ReanimationTryToGet(z->mZombatarTidbitID);
	Reanimation* body = app->ReanimationTryToGet(z->mBodyReanimID);
	return VisibleTrack(head, "anim_head1") && VisibleTrack(head, "tidBits_13") &&
		VisibleTrack(app->ReanimationTryToGet(z->mZombatarHairID), "hair_15") &&
		VisibleTrack(app->ReanimationTryToGet(z->mZombatarEyeWearID), "eyeWear_15") &&
		VisibleTrack(app->ReanimationTryToGet(z->mZombatarAccessoryID), "accessories_08") &&
		VisibleTrack(app->ReanimationTryToGet(z->mZombatarHatID), "hats_13") &&
		VisibleTrack(body, "anim_head1") &&
		FindReanimAttachment(body->GetTrackInstanceByName("anim_head1")->mAttachmentID);
#else
	return false;
#endif
}

bool Observed(LawnApp* app)
{
	Zombie* z = ZombieById(app->mBoard, s.mEntity);
	const bool client = app->NetIsClient();
	if (s.mStage >= NORMAL && s.mStage <= HEAD)
	{
		Reanimation* body = z ? app->ReanimationTryToGet(z->mBodyReanimID) : nullptr;
		if (!z || z->mZombieType != ZOMBIE_NORMAL || z->mNetGhost != client || !ReanimBounds(body)) return false;
		if (s.mStage == NORMAL) return z->mHasArm && z->mHasHead;
		const bool arm = !z->mHasArm && body->TrackExists("Zombie_outerarm_upper") &&
			body->GetTrackInstanceByName("Zombie_outerarm_upper")->mImageOverride == Sexy::IMAGE_REANIM_ZOMBIE_OUTERARM_UPPER2;
		if (s.mStage == ARM) return arm && z->mHasHead && Particles(app, PARTICLE_ZOMBIE_ARM) > 0;
		return arm && !z->mHasHead && body->TrackExists("anim_head1") &&
			body->GetTrackInstanceByName("anim_head1")->mRenderGroup == RENDER_GROUP_HIDDEN &&
			Particles(app, PARTICLE_ZOMBIE_HEAD) > 0;
	}
	if (s.mStage == JACK_POP)
		return z && z->mNetGhost == client && z->mZombieType == ZOMBIE_JACK_IN_THE_BOX &&
			z->mZombiePhase == PHASE_JACK_IN_THE_BOX_POPPING && ReanimBounds(app->ReanimationTryToGet(z->mBodyReanimID));
	if (s.mStage == JACK_BLAST)
		return !z && Particles(app, PARTICLE_JACKEXPLODE) > 0;
	if (s.mStage == SNAIL)
	{
		GridItem* g = GridById(app->mBoard, s.mEntity);
		Reanimation* body = g ? app->ReanimationTryToGet(g->mGridItemReanimID) : nullptr;
		if (!g || g->mGridItemType != GRIDITEM_STINKY || g->mNetGhost != client ||
			g->mGridItemState != GRIDITEM_STINKY_SLEEPING || !ReanimBounds(body) ||
			body->mReanimationType != REANIM_STINKY || body->mAnimRate != 0.0f || !body->TrackExists("shell")) return false;
		Reanimation* zz = FindReanimAttachment(body->GetTrackInstanceByName("shell")->mAttachmentID);
		if (!ReanimBounds(zz) || zz->mReanimationType != REANIM_SLEEPING ||
			zz->mAnimRate != 3.0f || zz->mLoopType != REANIM_LOOP) return false;
		if (s.mSleeper == REANIMATIONID_NULL)
		{ s.mSleeper = app->ReanimationGetID(zz); s.mSleepStart = zz->mAnimTime; return false; }
		return app->ReanimationGetID(zz) == s.mSleeper && zz->mAnimTime != s.mSleepStart;
	}
	if (s.mStage == REGISTRY) return app->mNetManager->GetPlayerZombatar(1) == Appearance();
	if (s.mStage == AVATAR || s.mStage == RETAINED)
		return z && z->mNetGhost == client && AvatarAssets(app, z) &&
			(s.mStage != RETAINED || !app->mNetManager->GetPlayerZombatar(1).mPresent);
	return false;
}

bool EnterHost(LawnApp* app, int stage, uint32_t now)
{
	Board* board = app->mBoard;
	uint16_t id = 0;
	if (stage == NORMAL)
	{
		Zombie* z = board->AddZombieInRow(ZOMBIE_NORMAL, 2, 0);
		if (!z) return false;
		// Fixture only: large health leaves time for peer ACKs and real renders.
		z->mBodyHealth = z->mBodyMaxHealth = 3000;
		z->mPosX = 600.0f; z->mX = 600; z->mVelX = 0.0f;
		s.mNormal = id = z->mNetId;
	}
	else if (stage == ARM || stage == HEAD)
	{
		Zombie* z = ZombieById(board, s.mNormal);
		if (!z) return false;
		z->TakeDamage(stage == ARM ? 1001 : 1000, 0U);
		id = s.mNormal;
	}
	else if (stage == JACK_POP)
	{
		if (Zombie* z = ZombieById(board, s.mNormal)) z->DieNoLoot();
		Zombie* jack = board->AddZombieInRow(ZOMBIE_JACK_IN_THE_BOX, 2, 0);
		if (!jack) return false;
		jack->mPosX = 600.0f; jack->mX = 600;
		jack->mZombiePhase = PHASE_JACK_IN_THE_BOX_RUNNING;
		jack->mPhaseCounter = 1;
		s.mJack = id = jack->mNetId;
	}
	else if (stage == JACK_BLAST)
	{
		Zombie* jack = ZombieById(board, s.mJack);
		if (!jack || jack->mZombiePhase != PHASE_JACK_IN_THE_BOX_POPPING) return false;
		jack->mPhaseCounter = 1; // The next native update performs the explosion.
		id = s.mJack;
	}
	else if (stage == SNAIL)
	{
		GridItem* g = board->mGridItems.DataArrayAlloc();
		if (!g || !app->mZenGarden) return false;
		g->mGridItemType = GRIDITEM_STINKY; g->mPosX = g->mGoalX = 340.0f;
		g->mPosY = g->mGoalY = 390.0f; g->mNetId = board->NetAllocId();
		g->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_PLANT, 2, 360);
		Reanimation* r = app->AddReanimation(g->mPosX, g->mPosY, 0, REANIM_STINKY);
		if (!r) return false;
		r->OverrideScale(0.8f, 0.8f); g->mGridItemReanimID = app->ReanimationGetID(r);
		app->mZenGarden->StinkyFinishFallingAsleep(g, 0);
		id = g->mNetId;
	}
	else if (stage == AVATAR)
	{
		if (app->mNetManager->GetPlayerZombatar(1) != Appearance()) return false;
		// No ApplyNetZombatar call: use the production occasional selection in
		// ZombieInitialize and stop on an actual client-owned appearance.
		for (int attempt = 0; attempt < 96; ++attempt)
		{
			Zombie* z = board->AddZombieInRow(ZOMBIE_NORMAL, 2, 0);
			if (!z) return false;
			++s.mSpawnAttempts;
			if (z->mNetZombatar == Appearance())
			{
				z->mPosX = 570.0f; z->mX = 570; z->mVelX = 0.0f;
				z->mBodyHealth = z->mBodyMaxHealth = 3000;
				s.mAvatar = id = z->mNetId; break;
			}
			z->DieNoLoot();
		}
		if (!id) return false;
	}
	else if (stage == RETAINED) id = s.mAvatar;
	s.mStage = stage; s.mEntity = id; s.mEntered = now; s.mObserved = 0;
	s.mCaptured = s.mAcked = s.mActionIssued = false;
	app->mNetManager->SendChat(Message("PRESENT", stage, id));
	LawnNet::NetTrace("SMOKE_PRESENTATION_STAGE role=host stage=%s id=%u healthFixture=3000 jackTimerFixture=1", kNames[stage], id);
	return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdatePresentation(LawnApp* app)
{
	const char* saved = getenv("PVZ_COOP_SMOKE_SAVED_AVATARS");
	if (saved && std::string(saved) == "1") return NetSmokeUpdateSavedAvatars(app);
	if (!app->CoopSmokeActive() || !app->mNetManager || !app->mBoard || app->mGameScene != SCENE_PLAYING ||
		!app->mCoopSmokeScenarioRuntime.IsComplete(app->mCoopSmokeManifest.mMinimumSnapshots)) return false;
	const uint32_t now = GetTickCount();
	const bool host = app->mCoopSmokeManifest.IsHost();
	const char* role = host ? "host" : "client";
	if (!s.mStarted)
	{
		if (app->mWidth != 800 || app->mHeight != 600 || app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_ADVENTURE_1 ||
			app->mCoopSmokeManifest.mSkipIntro || !NetValidateZombatar(Appearance()))
		{ app->CoopSmokeFail("presentation fixture requires real Adventure1 intro and 800x600 peers"); return false; }
		s.mStarted = now;
		NetTrace("SMOKE_PRESENTATION_BEGIN role=%s hostFixture=1 health=3000 jackCounter=synthetic snailScope=grid_only", role);
		if (host && !EnterHost(app, NORMAL, now))
		{ app->CoopSmokeFail("presentation native normal spawn failed"); return false; }
	}
	if (now - s.mStarted > 75000U)
	{ app->CoopSmokeFail((std::string("presentation peer/render ACK timeout stage=") + std::to_string(s.mStage)).c_str()); return false; }
	if (!host)
	{
		for (const auto& line : app->mNetManager->mChatLines)
		{
			if (line.mSlot != 0 || line.mText.compare(0, 8, "PRESENT ") != 0) continue;
			std::istringstream input(line.mText); std::string prefix; int stage = 0, id = 0;
			if (!(input >> prefix >> stage >> id) || stage != s.mStage + 1 || stage > COMPLETE || id < 0 || id > 65535) continue;
			s.mStage = stage; s.mEntity = (uint16_t)id; s.mEntered = now; s.mObserved = 0;
			s.mCaptured = s.mAcked = s.mActionIssued = false;
		}
		if ((s.mStage == REGISTRY || s.mStage == RETAINED) && !s.mActionIssued)
		{
			app->mNetManager->SetMyZombatar(s.mStage == REGISTRY ? Appearance() : NetZombatarAppearance());
			s.mActionIssued = true;
		}
	}
	if (s.mStage == COMPLETE)
	{
		NetTrace("SMOKE_PRESENTATION_COMPLETE role=%s limbs=2 jack=1 snail=1 clientAvatar=1 stableAppearance=1", role);
		return true;
	}
	if (host && s.mStage == JACK_POP && !s.mPopExtended)
		if (Zombie* jack = ZombieById(app->mBoard, s.mJack))
			if (jack->mZombiePhase == PHASE_JACK_IN_THE_BOX_POPPING)
			{ jack->mPhaseCounter = 300; s.mPopExtended = true; }
	if (!s.mCaptured && Observed(app))
	{
		if (!s.mObserved) s.mObserved = now;
		if (now - s.mObserved >= 150U)
		{
			if (!app->CoopSmokeCaptureFrame((std::string("presentation_") + kNames[s.mStage]).c_str()))
			{ app->CoopSmokeFail("presentation real renderer capture failed"); return false; }
			s.mCaptured = true;
			Zombie* z = ZombieById(app->mBoard, s.mEntity);
			NetZombatarAppearance actual;
			if (s.mStage == REGISTRY) actual = app->mNetManager->GetPlayerZombatar(1);
			else if (z) actual = z->mNetZombatar;
			NetBuffer encoded;
			if (!NetEncodeZombatar(actual, encoded) || encoded.Size() != NET_ZOMBATAR_WIRE_BYTES)
			{ app->CoopSmokeFail("observed entity appearance did not encode to 13 valid bytes"); return false; }
			const std::string hex = NetZombatarHex(actual);
			int ghost = z ? (int)z->mNetGhost : -1;
			if (s.mStage == SNAIL)
			{
				GridItem* g = GridById(app->mBoard, s.mEntity);
				Reanimation* body = app->ReanimationTryToGet(g->mGridItemReanimID);
				Reanimation* zz = FindReanimAttachment(body->GetTrackInstanceByName("shell")->mAttachmentID);
				ghost = (int)g->mNetGhost;
				NetTrace("SMOKE_PRESENTATION_SNAIL role=%s id=%u state=%d bodyReanim=%u sleepingReanim=%u sleepingRate=%.1f sleepingFrame=%d count=%d start=%.6f now=%.6f bodyRate=%.1f scope=grid_only",
					role, g->mNetId, (int)g->mGridItemState, (unsigned)g->mGridItemReanimID,
					(unsigned)app->ReanimationGetID(zz), zz->mAnimRate, zz->mFrameStart, zz->mFrameCount,
					s.mSleepStart, zz->mAnimTime, body->mAnimRate);
			}
			NetTrace("SMOKE_PRESENTATION_OBSERVED role=%s stage=%s id=%u ghost=%d assetBounds=%d armParticles=%d headParticles=%d jackParticles=%d attempts=%d appearance=%s bytes=%zu arm=%d head=%d",
				role, kNames[s.mStage], s.mEntity, ghost, s.mStage == REGISTRY ? 0 : 1, Particles(app, PARTICLE_ZOMBIE_ARM),
				Particles(app, PARTICLE_ZOMBIE_HEAD), Particles(app, PARTICLE_JACKEXPLODE), s.mSpawnAttempts,
				hex.c_str(), encoded.Size(), z ? z->mHasArm : -1, z ? z->mHasHead : -1);
		}
	}
	if (!host && s.mCaptured && !s.mAcked)
	{
		const std::string ack = Message("ACK_PRESENT", s.mStage, s.mEntity);
		s.mAcked = HasChat(app->mNetManager, 1, ack);
		const uint32_t ackNow = GetTickCount();
		if (!s.mAcked && (!s.mLastAck || ackNow - s.mLastAck >= 800U))
		{
			app->mNetManager->SendChat(ack);
			s.mLastAck = GetTickCount();
		}
	}
	if (host && s.mCaptured && now - s.mEntered >= 800U &&
		HasChat(app->mNetManager, 1, Message("ACK_PRESENT", s.mStage, s.mEntity)))
	{
		if (!EnterHost(app, s.mStage + 1, now))
		{ app->CoopSmokeFail("presentation stock transition or bounded natural avatar spawn failed"); return false; }
	}
	return false;
}
}
