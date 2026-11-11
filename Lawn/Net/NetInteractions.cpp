#include "NetManager.h"
#include "../../LawnApp.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Plant.h"
#include "../CutScene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <map>

namespace LawnNet
{
void NetManager::SetMyReady(bool theReady)
{
	if (!IsActive() || IsSpectator() || !mApp || (!mChooserOpen && !mApp->NetIsChoosingPvpTeams())) return;
	if (theReady && mChooserOpen &&
		(!mApp->mSeedChooserScreen || !mApp->mSeedChooserScreen->CoopCanReady())) return;
	if (IsMyReady() == theReady) return;
	if (IsHosting()) { mPlayers[mMySlot].mReady = theReady; SendLobbyState(); }
	else if (mHostConn) { NetBuffer b; b.WriteU8(theReady); mHostConn->SendMessage(NETMSG_READY, b); }
}

int NetManager::GetReadyEligibleCount() const
{
	int n = 0;
	for (const auto& p : mPlayers) if (p.mActive && !p.mSpectator) ++n;
	return n;
}

int NetManager::GetReadyCount() const
{
	int n = 0;
	for (const auto& p : mPlayers) if (p.mActive && !p.mSpectator && p.mReady) ++n;
	return n;
}

bool NetManager::AreAllPlayersReady() const
{
	return GetReadyEligibleCount() > 0 && GetReadyCount() == GetReadyEligibleCount();
}

void NetManager::ResetReadyState()
{
	if (!IsHosting()) return;
	bool changed = false;
	for (auto& p : mPlayers) { changed |= p.mReady != 0; p.mReady = 0; }
	if (changed) SendLobbyState();
}

void NetManager::PromoteSpectatorsAtLevelBoundary(const char* theReason)
{
	if (!IsHosting()) return;
	bool changed = false;
	for (const auto& player : mPlayers) changed |= player.mActive && player.mSpectator;
	if (!changed) return;
	// Reject input queued while the sender was still an observer. Seamless
	// stages retain their current input phase rather than entering an intro.
	AdvanceInputPhase(mInputPhase, "SPECTATOR_PROMOTION");
	for (auto& player : mPlayers)
	{
		if (player.mActive) player.mSpectator = false;
		player.mReady = 0;
	}
	Board* board = mApp ? mApp->mBoard : nullptr;
	SeedBank* bank = board ? board->mSeedBank : nullptr;
	if (bank)
	{
		int owned = 0, quotas[NET_MAX_PLAYERS] = {};
		for (int i = 0; i < bank->mNumPackets; ++i)
			if ((bank->mSeedPackets[i].mPacketType != SeedType::SEED_NONE || bank->mSeedPackets[i].IsMysteryPending()) &&
				bank->mSeedPackets[i].mNetOwnerSlot != NET_SHARED_OWNER) ++owned;
		const int fixed = min(owned, SeedChooserScreen::CoopExpectedGlobalFixedPicks(mApp, board));
		if (ComputeChooserQuotas(owned, fixed, board->IsPvPMode(), quotas))
			for (int i = 0; i < bank->mNumPackets; ++i)
			{
				SeedPacket& packet = bank->mSeedPackets[i];
				if ((packet.mPacketType == SeedType::SEED_NONE && !packet.IsMysteryPending()) || packet.mNetOwnerSlot == NET_SHARED_OWNER) continue;
				int owner = packet.mNetOwnerSlot;
				if (owner >= NET_MAX_PLAYERS || quotas[owner] == 0)
				{
					owner = 0;
					for (int slot = 1; slot < NET_MAX_PLAYERS; ++slot)
						if (quotas[slot] > quotas[owner]) owner = slot;
					// A hand from the old stage must not keep using a reassigned card.
					for (int slot = 0; slot < NET_MAX_PLAYERS; ++slot)
					{
						CursorObject* cursor = board->mNetPlayerCursors[slot];
						if (!cursor || cursor->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_BANK || cursor->mSeedBankIndex != i) continue;
						packet.Activate();
						cursor->mType = cursor->mImitaterType = SeedType::SEED_NONE;
						cursor->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
						cursor->mSeedBankIndex = -1;
					}
					packet.mNetOwnerSlot = (uint8_t)owner;
				}
				--quotas[owner]; // identity and cooldown stay with the same card
			}
	}
	SendLobbyState();
	NetTrace("spectators promoted reason=%s eligible=%d", theReason ? theReason : "level boundary", GetReadyEligibleCount());
	SendSystemChat("The level has ended. Spectators can now play.");
}

void NetManager::SetDifficulty(int theDifficulty)
{
	if (!IsHosting() || (mApp && mApp->mBoard) || theDifficulty < 1 || theDifficulty > 10) return;
	mDifficulty = theDifficulty;
	SendLobbyState();
}

void NetManager::SendSystemChat(const std::string& theText)
{
	if (!IsHosting()) return;
	std::string text;
	if (!NetNormalizeUtf8(theText, NET_MAX_CHAT_BYTES, NET_MAX_CHAT_CHARS, true, text)) return;
	AddChatLine(NET_SHARED_OWNER, text);
	NetBuffer b; b.WriteU8(NET_SHARED_OWNER); b.WriteString(text.c_str());
	BroadcastMessage(NETMSG_CHAT_BROADCAST, b);
}

void NetManager::KickPlayer(int theSlot)
{
	if (!IsHosting() || theSlot <= 0 || theSlot >= NET_MAX_PLAYERS || !mPlayers[theSlot].mActive || mKicked[theSlot]) return;
	mKicked[theSlot] = true;
	mKickDeadline[theSlot] = GetTickCount() + 500U;
	mPlayers[theSlot].mReady = 0;
	mSessions[theSlot].mConn.SendMessage(NETMSG_KICKED);
}

void NetManager::RedistributeSeeds(int theDepartedSlot)
{
	if (!mApp || !mApp->mBoard || !mApp->mBoard->mSeedBank) return;
	Board* board = mApp->mBoard;
	SeedBank* bank = board->mSeedBank;
	int counts[NET_MAX_PLAYERS] = {};
	for (int i = 0; i < bank->mNumPackets; ++i)
	{
		const int owner = bank->mSeedPackets[i].mNetOwnerSlot;
		if (owner < NET_MAX_PLAYERS && owner != theDepartedSlot) ++counts[owner];
	}
	for (int i = 0; i < bank->mNumPackets; ++i)
	{
		SeedPacket& packet = bank->mSeedPackets[i];
		if (packet.mNetOwnerSlot != theDepartedSlot) continue;
		int least = -1;
		for (int slot = 0; slot < NET_MAX_PLAYERS; ++slot)
			if (slot != theDepartedSlot && mPlayers[slot].mActive && !mPlayers[slot].mSpectator &&
				(!board->IsPvPMode() || mPlayers[slot].mTeam == NETTEAM_PLANTS) &&
				(least < 0 || counts[slot] < counts[least])) least = slot;
		packet.mNetOwnerSlot = least < 0 ? NET_SHARED_OWNER : (uint8_t)least;
		if (least >= 0) ++counts[least];
	}
	int owners[NET_MAX_PLAYERS] = {}, orphaned = 0;
	for (int i=0;i<bank->mNumPackets;++i)
	{
		const auto& packet = bank->mSeedPackets[i];
		if (packet.mPacketType == SeedType::SEED_NONE && !packet.IsMysteryPending()) continue;
		if (packet.mNetOwnerSlot < NET_MAX_PLAYERS) ++owners[packet.mNetOwnerSlot];
		if (packet.mNetOwnerSlot == theDepartedSlot) ++orphaned;
	}
	NetTrace("seed redistribution slot=%d packets=%d owners=%d,%d,%d,%d orphaned=%d",
		theDepartedSlot,bank->mNumPackets,owners[0],owners[1],owners[2],owners[3],orphaned);
}

void NetManager::SendLateJoinState(NetSession* theSession)
{
	if (!theSession || !mApp || !mApp->mBoard) return;
	Board* board = mApp->mBoard;
	// Custom modes can use an internal board level unrelated to their start
	// descriptor. Reuse exactly what created this board, changing only roster.
	NetBuffer start = mCurrentStart;
	uint8_t mode = 0, previousCount = 0;
	int32_t level = 0;
	bool recorded = start.ReadU8(&mode) && start.ReadI32(&level);
	const size_t rosterPosition = start.mReadPos;
	recorded = recorded && start.ReadU8(&previousCount);
	if (recorded)
	{
		start.mData[rosterPosition] = (uint8_t)GetPlayerCount();
		start.mReadPos = 0;
	}
	else
	{
		start.Clear();
		mode = (uint8_t)mApp->mGameMode; level = board->mLevel;
		start.WriteU8(mode); start.WriteI32(level);
		start.WriteU8((uint8_t)GetPlayerCount()); start.WriteU32(mApp->mNetBoardSeed);
		start.WriteU8((uint8_t)mStartTDPath.size());
		for (uint8_t cell : mStartTDPath) start.WriteU8(cell);
	}
	NetTrace("late join start descriptor mode=%u level=%d players=%d recorded=%d",mode,level,GetPlayerCount(),recorded?1:0);
	theSession->mConn.SendMessage(NETMSG_START_GAME, start);
	// Apply one complete authoritative bank while the new board is still in
	// INTRO; only then cross the playing barrier. Existing peers never restart.
	NetBuffer snapshot;
	board->NetEncodeSnapshot(snapshot);
	theSession->mConn.SendMessage(NETMSG_SNAPSHOT, snapshot);
	NetBuffer camera;
	camera.WriteU8((uint8_t)mApp->mGameScene);
	board->NetEncodeCameraBootstrap(camera);
	theSession->mConn.SendMessage(NETMSG_SPECTATOR_START, camera);
	NetTrace("late join state sent slot=%u scene=%d readonly=1", theSession->mSlot, (int)mApp->mGameScene);
}

void NetManager::SendAnnotation(uint8_t theKind, int theX, int theY, uint16_t theId)
{
	if (!IsActive() || !mApp || !mApp->mBoard) return;
	const int ox = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0;
	const int oy = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0;
	int x = theX - ox, y = theY - oy;
	if (theKind == NETANNOTATION_SEED && theId == 0 && mApp->mBoard->mSeedBank)
	{
		SeedBank* bank = mApp->mBoard->mSeedBank;
		for (int i = 0; i < bank->mNumPackets; ++i)
		{
			SeedPacket& packet = bank->mSeedPackets[i];
			if (mApp->mBoard->IsSeedPacketVisibleToSlot(i, mMySlot) &&
				x >= bank->mX + packet.mX + packet.mOffsetX && x < bank->mX + packet.mX + packet.mOffsetX + packet.mWidth &&
				y >= bank->mY + packet.mY && y < bank->mY + packet.mY + packet.mHeight)
			{ theId = packet.mNetId; break; }
		}
		if (!theId) return;
	}
	NetBuffer b; b.WriteU8(mMySlot); b.WriteU32(mInputEpoch); b.WriteU8(theKind);
	b.WriteI16((int16_t)x); b.WriteI16((int16_t)y); b.WriteU16(theId);
	if (IsHosting()) OnAnnotation(nullptr, b);
	else if (mHostConn) mHostConn->SendMessage(NETMSG_ANNOTATION, b);
}

void NetManager::OnAnnotation(NetSession* theSession, NetBuffer& thePayload)
{
	if (!mApp || !mApp->mBoard || mApp->mGameScene != SCENE_PLAYING || thePayload.Remaining() != 12) return;
	uint8_t slot = thePayload.ReadU8Or();
	const uint32_t epoch = thePayload.ReadU32Or();
	const uint8_t kind = thePayload.ReadU8Or();
	const int16_t x = thePayload.ReadI16Or(), y = thePayload.ReadI16Or();
	const uint16_t id = thePayload.ReadU16Or();
	if (IsHosting()) slot = theSession ? theSession->mSlot : mMySlot;
	if (slot >= NET_MAX_PLAYERS || !mPlayers[slot].mActive || epoch != mInputEpoch || kind > NETANNOTATION_SEED ||
		x < 0 || x > 800 || y < 0 || y > 600) return;
	SeedPacket* seed = nullptr;
	if (kind == NETANNOTATION_SEED)
	{
		SeedBank* bank = mApp->mBoard->mSeedBank;
		for (int i = 0; bank && i < bank->mNumPackets; ++i)
			if (id && bank->mSeedPackets[i].mNetId == id && mApp->mBoard->IsSeedPacketVisibleToSlot(i, slot)) seed = &bank->mSeedPackets[i];
		if (!seed) return;
	}
	const uint32_t now = GetTickCount();
	const auto canView = [&](int peer)
	{
		if (!mApp->mBoard->IsPvPMode() || mPlayers[peer].mSpectator) return true;
		if (!mPlayers[slot].mSpectator) return mPlayers[peer].mTeam == mPlayers[slot].mTeam;
		if (!seed) return true; // neutral observer pings and drawings are public
		const uint8_t team = mApp->mBoard->mChallenge->IsZombieSeedType(seed->mPacketType) ? NETTEAM_ZOMBIES : NETTEAM_PLANTS;
		return mPlayers[peer].mTeam == team;
	};
	const bool aVisibleHere = canView(mMySlot);
	if (IsHosting())
	{
		uint32_t& last = kind == NETANNOTATION_PING || kind == NETANNOTATION_SEED ? mLastAnnotationTick[slot] : mLastDrawTick[slot];
		const uint32_t cooldown = kind == NETANNOTATION_PING || kind == NETANNOTATION_SEED ? 500U : 40U;
		if (last && now - last < cooldown && kind != NETANNOTATION_DRAW_END) return;
		last = now;
		NetBuffer b; b.WriteU8(slot); b.WriteU32(epoch); b.WriteU8(kind); b.WriteI16(x); b.WriteI16(y); b.WriteU16(id);
		for (int peer = 1; peer < NET_MAX_PLAYERS; ++peer)
			if (mPlayers[peer].mActive && canView(peer))
				mSessions[peer].mConn.SendMessage(NETMSG_ANNOTATION, b);
		if (seed)
		{
			const int seconds = seed->mRefreshing ? max(0, seed->mRefreshTime - seed->mRefreshCounter + 99) / 100 : 0;
			const std::string text = SexyStringToString(Plant::GetNameString(seed->mPacketType, seed->mImitaterType)) +
				" (" + std::to_string(seconds) + "s)";
			if (aVisibleHere) AddChatLine(slot, text);
			NetBuffer chat; chat.WriteU8(slot); chat.WriteString(text.c_str());
			for (int peer = 1; peer < NET_MAX_PLAYERS; ++peer)
				if (mPlayers[peer].mActive && canView(peer))
					mSessions[peer].mConn.SendMessage(NETMSG_CHAT_BROADCAST, chat);
		}
	}
	if (aVisibleHere) ApplyAnnotation(slot, kind, x, y, id);
}

void NetManager::ApplyAnnotation(uint8_t theSlot, uint8_t theKind, int16_t theX, int16_t theY, uint16_t theId)
{
	NetTrace("annotation accepted slot=%u kind=%u id=%u x=%d y=%d", theSlot,theKind,theId,theX,theY);
	if (theKind == NETANNOTATION_PING || theKind == NETANNOTATION_SEED)
	{
		mApp->SexyAppBase::PlaySample(Sexy::SOUND_TAP); // local playback; annotation is already relayed
		NetTrace("annotation playback slot=%u kind=%u", theSlot,theKind);
	}
	mAnnotations.push_back({theSlot, theKind, theX, theY, theId, GetTickCount()});
	while (mAnnotations.size() > 1024) mAnnotations.pop_front();
}

void NetManager::DrawAnnotations(Sexy::Graphics* g)
{
	if (!mApp || !mApp->mBoard || !g) return;
	const uint32_t now = GetTickCount();
	while (!mAnnotations.empty() && now - mAnnotations.front().mTick > 12000U) mAnnotations.pop_front();
	const int ox = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0;
	const int oy = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0;
	if (IsHosting() && mApp->mBoard->mPaused)
	{
		g->SetColor(Color(35,30,20,235)); g->FillRect(ox+8,oy+150,218,140);
		TodDrawString(g,_S("Players"),ox+18,oy+171,Sexy::FONT_BRIANNETOD12,Color(255,235,170),DS_ALIGN_LEFT);
		for (int slot=0;slot<NET_MAX_PLAYERS;++slot) if (mPlayers[slot].mActive)
		{
			const int y = oy+194+slot*24;
			Sexy::FONT_BRIANNETOD12->DrawString(g,ox+18,y,StringToSexyString(mPlayers[slot].mNick),Color(255,255,255),g->mClipRect);
			if (slot) { g->SetColor(Color(140,45,35)); g->FillRect(ox+180,y-16,34,20); TodDrawString(g,_S("X"),ox+197,y,Sexy::FONT_BRIANNETOD12,Color(255,255,255),DS_ALIGN_CENTER); }
		}
	}
	std::map<std::pair<int,int>, uint32_t> ends;
	std::map<std::pair<int,int>, int> counts, indices;
	std::map<std::pair<int,int>, NetAnnotation> previous;
	for (const auto& a : mAnnotations) if (a.mKind >= NETANNOTATION_DRAW_BEGIN && a.mKind <= NETANNOTATION_DRAW_END)
	{ auto key = std::make_pair(a.mSlot,a.mId); ends[key] = a.mTick; ++counts[key]; }
	for (const auto& a : mAnnotations)
	{
		const auto color = GetRelativePlayerColor(mMySlot, a.mSlot);
		const uint32_t age = now - a.mTick;
		if (a.mKind == NETANNOTATION_PING && age < 2000U)
		{
			g->SetColor(Color(color.r,color.g,color.b,(2000U-age)*255U/2000U));
			const int r = 8 + age/70;
			g->DrawRect(a.mX+ox-r,a.mY+oy-r,r*2,r*2);
			g->DrawLine(a.mX+ox-8,a.mY+oy,a.mX+ox+8,a.mY+oy);
			g->DrawLine(a.mX+ox,a.mY+oy-8,a.mX+ox,a.mY+oy+8);
		}
		else if (a.mKind == NETANNOTATION_SEED && age < 2500U && (age/140U)%2 == 0)
		{
			SeedBank* bank = mApp->mBoard->mSeedBank;
			for (int i = 0; bank && i < bank->mNumPackets; ++i)
				if (bank->mSeedPackets[i].mNetId == a.mId && mApp->mBoard->IsSeedPacketVisibleToSlot(i,mMySlot))
				{ const auto& p = bank->mSeedPackets[i]; g->SetColor(Color(255,255,255,125)); g->FillRect(ox+bank->mX+p.mX+p.mOffsetX,oy+bank->mY+p.mY,p.mWidth,p.mHeight); }
		}
		else if (a.mKind >= NETANNOTATION_DRAW_BEGIN && a.mKind <= NETANNOTATION_DRAW_END)
		{
			auto key = std::make_pair(a.mSlot,a.mId);
			const int index = indices[key]++;
			const int alpha = NetStrokeAlpha(now-ends[key],index,counts[key]);
			auto last = previous.find(key);
			if (a.mKind != NETANNOTATION_DRAW_BEGIN && last != previous.end())
			{ g->SetColor(Color(color.r,color.g,color.b,alpha)); g->DrawLine(last->second.mX+ox,last->second.mY+oy,a.mX+ox,a.mY+oy); }
			previous[key] = a;
		}
	}
}

bool NetManager::HandlePausePlayerClick(int theX, int theY)
{
	if (!IsHosting() || !mApp || !mApp->mBoard || !mApp->mBoard->mPaused) return false;
	const int x = theX - (mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0);
	const int y = theY - (mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0);
	if (x < 8 || x >= 226 || y < 150 || y >= 290) return false;
	for (int slot=1;slot<NET_MAX_PLAYERS;++slot)
		if (x >= 180 && x < 214 && y >= 178+slot*24 && y < 198+slot*24) KickPlayer(slot);
	return true;
}
}

bool LawnApp::NetHandlePointerInput(int theAction, int theButton, int theX, int theY)
{
	using namespace LawnNet;
	if (!mNetManager || !mNetManager->IsActive() || !mBoard || mGameScene != SCENE_PLAYING || mNetChatTyping) return false;
	if (theAction == NETINPUT_MOUSE_DOWN && theButton == 1 && mNetManager->HandlePausePlayerClick(theX,theY)) return true;
	if (mBoard->mPaused || GetDialogCount() != 0) return false;
	const SDL_Keymod mods = SDL_GetModState();
	if (mNetDrawing && (theAction == NETINPUT_MOUSE_UP || !(mods & SDL_KMOD_CTRL)))
	{
		mNetManager->SendAnnotation(NETANNOTATION_DRAW_END,mNetLastDrawX,mNetLastDrawY,mNetDrawingId);
		mNetDrawing = false;
		if (theAction == NETINPUT_MOUSE_UP) return true;
	}
	if (theAction == NETINPUT_MOUSE_MOVE && (mods & SDL_KMOD_CTRL))
	{
		if (!mNetDrawing) { ++mNetDrawingId; if (!mNetDrawingId) ++mNetDrawingId; }
		mNetManager->SendAnnotation(mNetDrawing ? NETANNOTATION_DRAW_POINT : NETANNOTATION_DRAW_BEGIN,theX,theY,mNetDrawingId);
		mNetDrawing = true; mNetLastDrawX = theX; mNetLastDrawY = theY;
		return true;
	}
	if (theButton == 3)
	{
		if (theAction == NETINPUT_MOUSE_DOWN) mNetManager->SendAnnotation(NETANNOTATION_PING,theX,theY);
		return true;
	}
	if (theButton == 1 && (mods & SDL_KMOD_ALT))
	{
		if (theAction == NETINPUT_MOUSE_DOWN) mNetManager->SendAnnotation(NETANNOTATION_SEED,theX,theY);
		return true;
	}
	return (mods & SDL_KMOD_CTRL) || (mNetManager->IsSpectator() && theAction != NETINPUT_MOUSE_MOVE);
}
