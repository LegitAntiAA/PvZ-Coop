#ifndef __NETMANAGER_H__
#define __NETMANAGER_H__

#include "NetCommon.h"
#include "NetConnection.h"
#include "NetOnline.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <atomic>

class LawnApp;
class Board;
namespace Sexy { class Graphics; }

namespace LawnNet
{
enum NetAnnotationKind : uint8_t { NETANNOTATION_PING, NETANNOTATION_DRAW_BEGIN,
	NETANNOTATION_DRAW_POINT, NETANNOTATION_DRAW_END, NETANNOTATION_SEED };
struct NetAnnotation
{
	uint8_t mSlot, mKind;
	int16_t mX, mY;
	uint16_t mId;
	uint32_t mTick;
};

// A connected remote player's session (host side)
struct NetSession
{
	NetConnection mConn;
	uint8_t mSlot;
	bool mAssigned;
	NetSession() : mSlot(0), mAssigned(false) {}
};

// Remote cursor state (both directions, cosmetic)
struct NetRemoteCursor
{
	bool mValid;
	int16_t mX, mY;		// latest network target
	float mRenderX, mRenderY;	// interpolated display position
	uint8_t mCursorType;	// CursorType
	uint8_t mHeldSeed;		// SeedType held in hand (for silhouette rendering)
	uint8_t mHeldImitater;
	uint8_t mFlags;
	uint16_t mHeldPacketId;
	uint16_t mHoverPacketId;
	NetRemoteCursor() { Clear(); }
	void Clear() { mValid = false; mX = 0; mY = 0; mRenderX = 0; mRenderY = 0; mCursorType = 0; mHeldSeed = 255; mHeldImitater = 255; mFlags = 0; mHeldPacketId = 0; mHoverPacketId = 0; }
	void SetTarget(int16_t theX, int16_t theY)
	{
		if (!mValid) { mRenderX = (float)theX; mRenderY = (float)theY; }
		mX = theX; mY = theY; mValid = true;
	}
	void Interpolate()
	{
		mRenderX += ((float)mX - mRenderX) * 0.35f;
		mRenderY += ((float)mY - mRenderY) * 0.35f;
		float aDx = (float)mX - mRenderX;
		float aDy = (float)mY - mRenderY;
		if (aDx > -0.15f && aDx < 0.15f) mRenderX = (float)mX;
		if (aDy > -0.15f && aDy < 0.15f) mRenderY = (float)mY;
	}
};

struct NetRemoteInput
{
	uint8_t mSlot;
	uint32_t mEpoch;
	uint32_t mSequence;
	uint8_t mAction;
	int8_t mButton;
	int16_t mX, mY;
	uint16_t mSeedPacketId;
	uint8_t mSeedType;
	uint8_t mSeedImitater;
	uint8_t mSeedOwner;
};

class NetManager
{
public:
	NetManager();
	~NetManager();

	void Init(LawnApp* theApp);
	void Shutdown();

	// Per-frame pump (from LawnApp::UpdateFrames)
	void Update();

	// ---- session control ----
	bool StartHost(uint16_t thePort = NET_DEFAULT_PORT);	// begin listening
	bool StartClient(const char* theIP, uint16_t thePort = NET_DEFAULT_PORT);		// begin connecting (async)
	void CancelConnect();
	void Disconnect();
	bool IsHosting() const { return mMode == MODE_HOST; }
	bool IsClient() const { return mMode == MODE_CLIENT; }
	bool IsActive() const { return mMode != MODE_NONE; }
	bool IsConnecting() const { return mConnecting; }
	bool IsConnectedToHost() const { return mMode == MODE_CLIENT && mClientConnected; }
	const char* GetConnectStatus() const { return mConnectStatus; }
	const std::string& GetDisconnectReason() const { return mDisconnectReason; }

	// ---- players / lobby ----
	int GetMySlot() const { return mMySlot; }
	int GetPlayerCount() const;
	const NetPlayerInfo& GetPlayer(int theSlot) const { return mPlayers[theSlot]; }
	NetPlayerInfo& GetPlayerMutable(int theSlot) { return mPlayers[theSlot]; }
	bool IsPlayerActive(int theSlot) const { return mPlayers[theSlot].mActive; }
	void SetMyTeam(uint8_t theTeam);			// client: request; host: apply
	uint8_t GetMyTeam() const;
	int GetActingPlayerSlot() const { return mActingPlayerSlot >= 0 ? mActingPlayerSlot : mMySlot; }
	int GetRawActingPlayerSlot() const { return mActingPlayerSlot; }
	void SetActingPlayerSlot(int theSlot) { mActingPlayerSlot = theSlot; }
	int GetTeamCount(uint8_t theTeam) const;	// count players on a team
	bool ComputeChooserQuotas(int theBankCount, int theFixedHostCount, bool thePvP,
		int theQuotas[NET_MAX_PLAYERS], int* theEligibleCount = nullptr) const;
	void RequestChooserStateEcho(const char* theReason, int theSlot, int theSeedType);
	bool ConsumeChooserStateEchoRequest();
	void SendLobbyState();						// host: broadcast lobby
	void SetMyReady(bool theReady);
	bool IsMyReady() const { return mPlayers[mMySlot].mReady != 0; }
	int GetReadyCount() const;
	int GetReadyEligibleCount() const;
	bool AreAllPlayersReady() const;
	void ResetReadyState();
	void PromoteSpectatorsAtLevelBoundary(const char* theReason);
	int GetDifficulty() const { return mDifficulty; }
	void SetDifficulty(int theDifficulty);
	void KickPlayer(int theSlot);
	bool IsSpectator() const { return mPlayers[mMySlot].mSpectator; }
	NetOnline* GetOnline() { return mOnline.get(); }
	bool ConnectOnlineDirectory();
	bool CreateOnlineRoom(const std::string& theName, const std::string& thePassword, int theMaxPlayers);
	bool JoinOnlineRoom(const std::string& theRoomId, const std::string& thePassword);
	void SendSystemChat(const std::string& theText);
	void SendAnnotation(uint8_t theKind, int theX, int theY, uint16_t theId = 0);
	void DrawAnnotations(Sexy::Graphics* theGraphics);
	bool HandlePausePlayerClick(int theX, int theY);
	const std::deque<NetAnnotation>& GetAnnotations() const { return mAnnotations; }
	void SendLateJoinState(NetSession* theSession);
	const NetZombatarAppearance& GetPlayerZombatar(int theSlot) const;
	const NetZombatarAppearance& GetMyZombatar() const;
	void SetMyZombatar(const NetZombatarAppearance& theAppearance);
	void BroadcastZombatars();
	void SendChat(const std::string& theUtf8Text);
	struct ChatLine
	{
		uint8_t mSlot;
		std::string mText;
		uint32_t mReceivedTick;
		std::string mNick;
	};
	std::deque<ChatLine> mChatLines;

	// ---- game start (host) ----
	void BroadcastStartGame(int theGameMode, int theLevel, uint32_t theBoardSeed);
	// TD path data for the current game (host fills before broadcast)
	std::vector<uint8_t> mStartTDPath;			// pairs (row, col)

	// ---- outgoing helpers ----
	void SendInput(bool theDown, int theClickCount, int theX, int theY); // client -> host
	void SendMouseMove(int theDragButton, int theX, int theY);           // client -> host (coalesced)
	void SendHeatWaveShoutKey(bool theDown);                             // client -> host (Q only)
	void SendSeedPick(bool theUnpick, int theSeedType, int theImitater); // client -> host chooser
	void SendStoreAction(const NetBuffer& thePayload);                  // client -> host buy request
	void SendCursor(int theCursorType, int theHeldSeed, int theHeldImitater, uint8_t theFlags, int theX, int theY);
	void SendPauseState(bool thePaused);		// host
	void SendBoardEvent(const NetBuffer& thePayload, uint8_t theEventType);
	void SendSnapshot(const NetBuffer& thePayload);
	void BroadcastSceneCmd(uint8_t theCmd, int theParam);
	uint32_t BroadcastAdventureStartFx();
	void ClearPendingBoardMessages(const char* theReason);
	void AdvanceInputPhase(NetInputPhase thePhase, const char* theReason); // host barrier
	void PurgePendingInputs(const char* theReason, int theSlot = -1);
	bool PopPendingInput(NetRemoteInput& theInput);
	bool IsRemoteHeatWaveShoutKeyActive();
	uint32_t GetInputEpoch() const { return mInputEpoch; }
	NetInputPhase GetInputPhase() const { return mInputPhase; }

	// ---- incoming queues (polled by LawnApp / Board) ----
	struct PendingMsg
	{
		uint8_t mKind;		// 0 = board event, 1 = snapshot
		NetBuffer mData;
	};
	std::deque<PendingMsg> mPendingBoardMsgs;	// client: events+snapshots in arrival order
	NetRemoteCursor mRemoteCursors[NET_MAX_PLAYERS];
	bool mRemotePaused;							// client: is the game paused by host

	// ---- adaptive rate ----
	int GetSnapshotHz() const { return mSnapshotHz; }
	void SetSnapshotHz(int theHz);

	// connection info for UI
	uint16_t GetPingToHost() const { return mMode == MODE_CLIENT && mHostConn ? mHostConn->GetPing() : 0; }
	uint64_t GetBytesSent() const;
	uint64_t GetBytesRecv() const;

	NetConfig mConfig;

private:
	enum Mode { MODE_NONE, MODE_HOST, MODE_CLIENT };

	void HandleMessage(NetSession* theSession, uint8_t theMsgType, NetBuffer& thePayload);
	void HandleMessageClient(uint8_t theMsgType, NetBuffer& thePayload);

	// host-side message handlers
	void OnHello(NetSession* theSession, NetBuffer& thePayload);
	void OnClientInput(NetSession* theSession, NetBuffer& thePayload);
	void OnClientKeyInput(NetSession* theSession, NetBuffer& thePayload);
	void SendMouseInputAction(uint8_t theAction, int theButton, int theX, int theY);
	bool ConsumeExpectedInputSequence(uint8_t theSlot, uint32_t theEpoch,
		uint32_t theSequence, const char* theKind);
	void ClearHeatWaveShoutKeys(const char* theReason, int theSlot = -1);
	void OnClientCursor(NetSession* theSession, NetBuffer& thePayload);
	void OnClientSeedPick(NetSession* theSession, NetBuffer& thePayload, bool theUnpick);
	void OnClientTeamSelect(NetSession* theSession, NetBuffer& thePayload);
	void OnClientStats(NetSession* theSession, NetBuffer& thePayload);
	void OnStoreAction(NetSession* theSession, NetBuffer& thePayload);
	void OnClientChat(NetSession* theSession, NetBuffer& thePayload);

	// client-side message handlers
	void OnLobbyState(NetBuffer& thePayload);
	void OnWelcome(NetBuffer& thePayload);
	void OnStartGame(NetBuffer& thePayload);
	void OnSnapshot(NetBuffer& thePayload);
	void OnBoardEvent(NetBuffer& thePayload);
	void OnCursor(NetBuffer& thePayload);
	void OnChooserOpen(NetBuffer& thePayload);
	void OnChooserState(NetBuffer& thePayload);
	void OnChooserStart(NetBuffer& thePayload);
	void OnInputEpoch(NetBuffer& thePayload);
	void OnSceneCmd(NetBuffer& thePayload);
	void OnChatBroadcast(NetBuffer& thePayload);
	void AddChatLine(uint8_t theSlot, const std::string& theText);
	void OnAnnotation(NetSession* theSession, NetBuffer& thePayload);
	void ApplyAnnotation(uint8_t theSlot, uint8_t theKind, int16_t theX, int16_t theY, uint16_t theId);
	void RedistributeSeeds(int theDepartedSlot);
	void OnZombatar(NetSession* theSession, NetBuffer& thePayload);
	void UpdateOnline();
	void SendHello();

	void UpdatePings();
	void AcceptThreadProc();
	void ConnectThreadProc(const char* theIP);
	void DropSession(int theSlot, bool theNotify);
	void CheckAdaptiveRate();
	void SendInputEpoch(NetSession* theSession);

	// serialize/parse lobby
	void EncodeLobbyState(NetBuffer& theBuffer);

public:
	void BroadcastMessage(uint8_t theMsgType, const NetBuffer& thePayload);

	// chooser state shared between players (mirrored from host authority)
	struct ChooserPick { uint8_t mSeedType; uint8_t mImitater; int mSlot; };
	std::deque<ChooserPick> mRemotePicks;		// client: picks from others to display
	std::deque<ChooserPick> mRemoteUnpicks;		// client: unpicks from others
	std::deque<NetBuffer> mPendingChooserStarts;	// client: final seed list
	std::deque<NetBuffer> mRemoteChooserStates;		// client: authoritative chooser pick state
	bool mChooserOpen;							// client: chooser should be open
	int mChooserPicksPerPlayer;					// from host
	int mChooserBankCount;
	int mChooserPickQuota[NET_MAX_PLAYERS];		// authoritative quota for every slot

private:
	Mode mMode;
	std::unique_ptr<NetOnline> mOnline;
	uint8_t mOnlineIntent = 0;
	bool mOnlinePlaying = false;
	NetBuffer mCurrentStart;
	LawnApp* mApp;

	// host state
	SOCKET mListenerSocket;
	std::thread mAcceptThread;
	std::atomic<bool> mAcceptRunning;
	NetSession mSessions[NET_MAX_PLAYERS];		// indexed by slot; slot 0 = host (unused)
	std::mutex mSessionsMutex;
	bool mDeferredDrop[NET_MAX_PLAYERS];		// never tear a session down inside its callback
	std::deque<NetRemoteInput> mPendingInputs;
	size_t mPendingInputCount[NET_MAX_PLAYERS];
	uint32_t mLastInputSequence[NET_MAX_PLAYERS];
	bool mRemoteHeatWaveQDown[NET_MAX_PLAYERS];
	uint32_t mRemoteHeatWaveQDownTick[NET_MAX_PLAYERS];
	uint32_t mLastChatTick[NET_MAX_PLAYERS];
	std::atomic<bool> mChooserStateEchoRequested;

	// client state
	std::thread mConnectThread;
	std::atomic<bool> mConnecting;
	std::atomic<bool> mClientConnected;
	NetConnection* mHostConn;
	char mConnectStatus[128];
	std::string mDisconnectReason;
	bool mKicked[NET_MAX_PLAYERS] = {};
	uint32_t mKickDeadline[NET_MAX_PLAYERS] = {};

	// lobby
	NetPlayerInfo mPlayers[NET_MAX_PLAYERS];
	uint8_t mMySlot;
	int mActingPlayerSlot;					// host: slot whose queued input is being applied
	uint32_t mInputEpoch;
	uint32_t mNextInputSequence;			// client: non-zero sequence within mInputEpoch
	uint32_t mLastInputMoveTick;			// client: bounds authoritative motion traffic
	bool mClientHeatWaveQDown;			// client: suppress repeat downs and guarantee one release
	NetInputPhase mInputPhase;
	uint32_t mAdventureStartFxGeneration;	// host: next presentation serial
	uint32_t mLastAdventureStartFxGeneration;	// client: last applied presentation serial

	// rate
	int mSnapshotHz;
	int mAdaptiveGoodStreak;
	int mAdaptiveBadStreak;

	// ping timer
	uint32_t mLastPingTick;
	uint32_t mLastStatsTick;
	int mDifficulty;
	uint32_t mLastAnnotationTick[NET_MAX_PLAYERS] = {};
	uint32_t mLastDrawTick[NET_MAX_PLAYERS] = {};
	std::deque<NetAnnotation> mAnnotations;
	NetZombatarAppearance mLocalZombatar;
	NetZombatarAppearance mPlayerZombatars[NET_MAX_PLAYERS];

	// pending connection to adopt from accept thread (host)
	std::mutex mPendingConnMutex;
	std::vector<SOCKET> mPendingConns;
};

extern NetManager* gNetManager;

} // namespace LawnNet

#endif // __NETMANAGER_H__
