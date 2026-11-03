#ifndef __NETMANAGER_H__
#define __NETMANAGER_H__

#include "NetCommon.h"
#include "NetConnection.h"

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

namespace LawnNet
{

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
	int16_t mX, mY;
	uint8_t mCursorType;	// CursorType
	uint8_t mHeldSeed;		// SeedType held in hand (for silhouette rendering)
	uint8_t mHeldImitater;
	uint8_t mFlags;
	NetRemoteCursor() { Clear(); }
	void Clear() { mValid = false; mX = 0; mY = 0; mCursorType = 0; mHeldSeed = 255; mHeldImitater = 255; mFlags = 0; }
};

struct NetRemoteInput
{
	bool mDown;
	int8_t mClickCount;
	int16_t mX, mY;
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

	// ---- players / lobby ----
	int GetMySlot() const { return mMySlot; }
	int GetPlayerCount() const;
	const NetPlayerInfo& GetPlayer(int theSlot) const { return mPlayers[theSlot]; }
	NetPlayerInfo& GetPlayerMutable(int theSlot) { return mPlayers[theSlot]; }
	bool IsPlayerActive(int theSlot) const { return mPlayers[theSlot].mActive; }
	void SetMyTeam(uint8_t theTeam);			// client: request; host: apply
	uint8_t GetMyTeam() const;
	int GetTeamCount(uint8_t theTeam) const;	// count players on a team
	void SendLobbyState();						// host: broadcast lobby

	// ---- game start (host) ----
	void BroadcastStartGame(int theGameMode, int theLevel, uint32_t theBoardSeed);
	// TD path data for the current game (host fills before broadcast)
	std::vector<uint8_t> mStartTDPath;			// pairs (row, col)

	// ---- outgoing helpers ----
	void SendInput(bool theDown, int theClickCount, int theX, int theY); // client -> host
	void SendStoreAction(const NetBuffer& thePayload);                  // client -> host buy request
	void SendCursor(int theCursorType, int theHeldSeed, int theHeldImitater, uint8_t theFlags, int theX, int theY);
	void SendPauseState(bool thePaused);		// host
	void SendBoardEvent(const NetBuffer& thePayload, uint8_t theEventType);
	void SendSnapshot(const NetBuffer& thePayload);
	void BroadcastSceneCmd(uint8_t theCmd, int theParam);

	// ---- incoming queues (polled by LawnApp / Board) ----
	struct PendingMsg
	{
		uint8_t mKind;		// 0 = board event, 1 = snapshot
		NetBuffer mData;
	};
	std::deque<PendingMsg> mPendingBoardMsgs;	// client: events+snapshots in arrival order
	std::deque<NetRemoteInput> mPendingInputs;	// host: remote input events
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
	void OnClientCursor(NetSession* theSession, NetBuffer& thePayload);
	void OnClientSeedPick(NetSession* theSession, NetBuffer& thePayload, bool theUnpick);
	void OnClientTeamSelect(NetSession* theSession, NetBuffer& thePayload);
	void OnClientStats(NetSession* theSession, NetBuffer& thePayload);
	void OnStoreAction(NetSession* theSession, NetBuffer& thePayload);

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

	void UpdatePings();
	void AcceptThreadProc();
	void ConnectThreadProc(const char* theIP);
	void DropSession(int theSlot, bool theNotify);
	void CheckAdaptiveRate();

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

private:
	Mode mMode;
	LawnApp* mApp;

	// host state
	SOCKET mListenerSocket;
	std::thread mAcceptThread;
	std::atomic<bool> mAcceptRunning;
	NetSession mSessions[NET_MAX_PLAYERS];		// indexed by slot; slot 0 = host (unused)
	std::mutex mSessionsMutex;

	// client state
	std::thread mConnectThread;
	std::atomic<bool> mConnecting;
	std::atomic<bool> mClientConnected;
	NetConnection* mHostConn;
	char mConnectStatus[128];

	// lobby
	NetPlayerInfo mPlayers[NET_MAX_PLAYERS];
	uint8_t mMySlot;

	// rate
	int mSnapshotHz;
	int mAdaptiveGoodStreak;

	// ping timer
	uint32_t mLastPingTick;
	uint32_t mLastStatsTick;

	// pending connection to adopt from accept thread (host)
	std::mutex mPendingConnMutex;
	std::vector<SOCKET> mPendingConns;
};

extern NetManager* gNetManager;

} // namespace LawnNet

#endif // __NETMANAGER_H__
