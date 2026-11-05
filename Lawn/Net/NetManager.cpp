#include "NetManager.h"
#include "../../LawnApp.h"
#include "../Board.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Challenge.h"
#include "../CutScene.h"
#include "../Widget/StoreScreen.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../SexyAppFramework/Common.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../GameConstants.h"

namespace
{
static const size_t NET_MOUSE_INPUT_BYTES = 21;

int NetCanonicalX(LawnApp* theApp, int theLocalX)
{
	return theApp && theApp->mDDInterface ? theLocalX - theApp->mDDInterface->mWideScreenOffsetX : theLocalX;
}

int NetCanonicalY(LawnApp* theApp, int theLocalY)
{
	return theApp && theApp->mDDInterface ? theLocalY - theApp->mDDInterface->mWideScreenOffsetY : theLocalY;
}

int NetLocalX(LawnApp* theApp, int theCanonicalX)
{
	return theApp && theApp->mDDInterface ? theCanonicalX + theApp->mDDInterface->mWideScreenOffsetX : theCanonicalX;
}

int NetLocalY(LawnApp* theApp, int theCanonicalY)
{
	return theApp && theApp->mDDInterface ? theCanonicalY + theApp->mDDInterface->mWideScreenOffsetY : theCanonicalY;
}

bool NetValidMouseButton(int theButton)
{
	return theButton == -1 || theButton == 1 || theButton == 3;
}

// Returns the host/client seed packet under canonical 800x600 board
// coordinates.  This is intentionally presentation-free: it is used only to
// attach a stable semantic identity to a raw click and to reject stale bank
// positions before Board::MouseDown can select the wrong card.
int NetSeedPacketAt(Board* theBoard, int theCanonicalX, int theCanonicalY)
{
	if (!theBoard || !theBoard->mSeedBank)
		return -1;
	SeedBank* aBank = theBoard->mSeedBank;
	if (theCanonicalX - aBank->mX > aBank->mWidth - 5 || aBank->mNumPackets <= 0)
		return -1;

	const int aBankX = theCanonicalX - aBank->mX;
	const int aBankY = theCanonicalY - aBank->mY;
	for (int i = 0; i < aBank->mNumPackets && i < SEEDBANK_CAPACITY; i++)
	{
		SeedPacket& aPacket = aBank->mSeedPackets[i];
		if (aPacket.mSlotMachineCountDown > 0 || aPacket.mPacketType == SeedType::SEED_NONE)
			continue;
		if (aBankX >= aPacket.mX + aPacket.mOffsetX &&
			aBankX < aPacket.mX + aPacket.mOffsetX + aPacket.mWidth &&
			aBankY >= aPacket.mY && aBankY < aPacket.mY + aPacket.mHeight)
			return i;
	}
	return -1;
}
}

namespace LawnNet
{

static bool NetIsValidHeldSeed(uint8_t theSeed)
{
	// Remote plant silhouettes are rendered by Plant::DrawSeedType, which only
	// accepts real plant seed ids. Zombie-card ids and tool cursor payloads must
	// never reach that renderer (PvP hides cursors entirely anyway).
	return theSeed == 255 || theSeed < (uint8_t)SeedType::NUM_SEED_TYPES;
}

// NETMSG_INPUT_KEY intentionally exposes one semantic key only.  Keeping the
// wire value independent of the platform KeyCode enum prevents a peer from
// smuggling pause/menu/debug keys through this gameplay-only channel.
static const uint16_t NET_HEAT_WAVE_Q_WIRE = (uint16_t)'Q';
static const size_t NET_HEAT_WAVE_KEY_INPUT_BYTES = sizeof(uint32_t) * 2 +
	sizeof(uint16_t) + sizeof(uint8_t);
// A missed focus/key-up event must not leave a remote shout latched forever.
// One valid press is still long enough to fill Heat Wave's 100-tick meter.
static const uint32_t NET_HEAT_WAVE_Q_MAX_HOLD_MS = 2000U;

NetManager* gNetManager = nullptr;

// -------------------------------------------------------------------------
NetManager::NetManager() :
	mMode(MODE_NONE),
	mApp(nullptr),
	mListenerSocket(INVALID_SOCKET),
	mAcceptRunning(false),
	mConnecting(false),
	mClientConnected(false),
	mHostConn(nullptr),
	mMySlot(0),
	mActingPlayerSlot(-1),
	mInputEpoch(1),
	mNextInputSequence(1),
	mLastInputMoveTick(0),
	mClientHeatWaveQDown(false),
	mInputPhase(NETINPUT_MENU),
	mAdventureStartFxGeneration(0),
	mLastAdventureStartFxGeneration(0),
	mSnapshotHz(NET_SNAPSHOT_HZ_DEFAULT),
	mAdaptiveGoodStreak(0),
	mAdaptiveBadStreak(0),
	mLastPingTick(0),
	mLastStatsTick(0),
	mChooserOpen(false),
	mChooserPicksPerPlayer(0),
	mChooserBankCount(0)
{
	mConnectStatus[0] = 0;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		mPlayers[i].Reset();
		mChooserPickQuota[i] = 0;
		mDeferredDrop[i] = false;
		mPendingInputCount[i] = 0;
		mLastInputSequence[i] = 0;
		mRemoteHeatWaveQDown[i] = false;
		mRemoteHeatWaveQDownTick[i] = 0;
	}
	mChooserStateEchoRequested.store(false, std::memory_order_relaxed);
	mRemotePaused = false;
}

NetManager::~NetManager()
{
	Shutdown();
}

void NetManager::Init(LawnApp* theApp)
{
	mApp = theApp;
	NetLoadConfig(mConfig);
	mSnapshotHz = mConfig.mBaseSnapshotHz;
	NetStartupWSA();
}

void NetManager::Shutdown()
{
	NetTrace("manager shutdown requested mode=%d", (int)mMode);
	Disconnect();
	NetShutdownWSA();
	NetTraceClose();
}

// -------------------------------------------------------------------------
// Session control
// -------------------------------------------------------------------------
bool NetManager::StartHost(uint16_t thePort)
{
	Disconnect();
	NetTraceOpen("host");
	NetTrace("host start requested port=%u nick=%s snapshotHz=%d", thePort, mConfig.mNick, mSnapshotHz);

	if (!NetStartupWSA())
	{
		NetTrace("host start failed: WSA startup");
		return false;
	}

	SOCKET aListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (aListener == INVALID_SOCKET)
	{
		NetTrace("host start failed: socket error=%d", WSAGetLastError());
		return false;
	}

	BOOL aReuse = TRUE;
	setsockopt(aListener, SOL_SOCKET, SO_REUSEADDR, (const char*)&aReuse, sizeof(BOOL));

	sockaddr_in anAddr;
	memset(&anAddr, 0, sizeof(anAddr));
	anAddr.sin_family = AF_INET;
	anAddr.sin_port = htons(thePort);
	anAddr.sin_addr.s_addr = INADDR_ANY;

	if (::bind(aListener, (sockaddr*)&anAddr, sizeof(anAddr)) == SOCKET_ERROR ||
		listen(aListener, 4) == SOCKET_ERROR)
	{
		NetTrace("host start failed: bind/listen error=%d", WSAGetLastError());
		closesocket(aListener);
		return false;
	}

	mListenerSocket = aListener;
	mMode = MODE_HOST;
	mMySlot = 0;
	mInputEpoch = 1;
	mNextInputSequence = 1;
	mLastInputMoveTick = 0;
	mClientHeatWaveQDown = false;
	mInputPhase = NETINPUT_MENU;
	mAdventureStartFxGeneration = 0;
	mLastAdventureStartFxGeneration = 0;

	mPlayers[0].Reset();
	mPlayers[0].mActive = true;
	mPlayers[0].mSlot = 0;
	strncpy_s(mPlayers[0].mNick, mConfig.mNick, NET_MAX_NICK);

	mAcceptRunning = true;
	mAcceptThread = std::thread(&NetManager::AcceptThreadProc, this);

	strcpy_s(mConnectStatus, "Hosting on port ");
	_itoa_s(thePort, mConnectStatus + strlen(mConnectStatus), 32, 10);
	NetTrace("host listening port=%u", thePort);
	return true;
}

void NetManager::AcceptThreadProc()
{
	while (mAcceptRunning)
	{
		sockaddr_in aClientAddr;
		int aAddrLen = sizeof(aClientAddr);
		SOCKET aClient = accept(mListenerSocket, (sockaddr*)&aClientAddr, &aAddrLen);
		if (aClient == INVALID_SOCKET)
		{
			if (!mAcceptRunning)
				break;
			Sleep(50);
			continue;
		}
		NetTrace("accepted TCP peer address=%s port=%u", inet_ntoa(aClientAddr.sin_addr), ntohs(aClientAddr.sin_port));
		// queue for the game thread to adopt
		std::lock_guard<std::mutex> aLock(mPendingConnMutex);
		mPendingConns.push_back(aClient);
	}
}

bool NetManager::StartClient(const char* theIP, uint16_t thePort)
{
	Disconnect();
	NetTraceOpen("client");
	NetTrace("client connect requested endpoint=%s:%u nick=%s snapshotHz=%d", theIP ? theIP : "(null)", thePort, mConfig.mNick, mSnapshotHz);
	if (!NetStartupWSA())
	{
		NetTrace("client start failed: WSA startup");
		return false;
	}

	mMode = MODE_CLIENT;
	mInputEpoch = 1;
	mNextInputSequence = 1;
	mLastInputMoveTick = 0;
	mClientHeatWaveQDown = false;
	mInputPhase = NETINPUT_MENU;
	mAdventureStartFxGeneration = 0;
	mLastAdventureStartFxGeneration = 0;
	mConnecting = true;
	mClientConnected = false;
	strcpy_s(mConnectStatus, "Connecting...");

	char anIP[64];
	strncpy_s(anIP, theIP, 63);
	anIP[63] = 0;

	mConnectThread = std::thread([this, anIP, thePort]()
	{
		// try several times before giving up
		const int aMaxAttempts = 3;
		SOCKET aSock = INVALID_SOCKET;
		for (int anAttempt = 1; anAttempt <= aMaxAttempts; anAttempt++)
		{
			NetTrace("client connection attempt=%d/%d endpoint=%s:%u", anAttempt, aMaxAttempts, anIP, thePort);
			sprintf_s(mConnectStatus, "Attempt %d/%d...", anAttempt, aMaxAttempts);
			aSock = NetConnectTo(anIP, thePort, 5000);
			if (aSock != INVALID_SOCKET)
				break;
			if (!mConnecting) // cancelled by the user
				return;
			Sleep(500);
		}

		if (aSock == INVALID_SOCKET)
		{
			NetTrace("client connection failed endpoint=%s:%u lastError=%d", anIP, thePort, WSAGetLastError());
			strcpy_s(mConnectStatus, "Failed to connect (check IP + Radmin VPN)");
			mConnecting = false;
			return;
		}
		NetTrace("client TCP connected endpoint=%s:%u", anIP, thePort);

		// hand to the manager under lock
		{
			std::lock_guard<std::mutex> aLock(mPendingConnMutex);
			mPendingConns.push_back(aSock);
		}
		strcpy_s(mConnectStatus, "Connected, waiting for host...");
		mConnecting = false;
	});

	return true;
}

void NetManager::CancelConnect()
{
	Disconnect();
}

void NetManager::Disconnect()
{
	if (mMode != MODE_NONE || mConnecting || mClientConnected)
		NetTrace("disconnect begin mode=%d connecting=%d connected=%d players=%d", (int)mMode, mConnecting ? 1 : 0, mClientConnected ? 1 : 0, GetPlayerCount());
	// Stop the retry loop before waiting for its worker.  An in-progress select
	// may still take up to its current timeout, but no additional retry starts.
	mConnecting = false;
	if (mAcceptRunning)
	{
		mAcceptRunning = false;
		if (mListenerSocket != INVALID_SOCKET)
		{
			closesocket(mListenerSocket);
			mListenerSocket = INVALID_SOCKET;
		}
		if (mAcceptThread.joinable())
			mAcceptThread.join();
	}
	if (mConnectThread.joinable())
		mConnectThread.join();

	{
		std::lock_guard<std::mutex> aLock(mPendingConnMutex);
		for (SOCKET s : mPendingConns)
			closesocket(s);
		mPendingConns.clear();
	}

	for (int i = 1; i < NET_MAX_PLAYERS; i++)
		DropSession(i, false);

	if (mHostConn)
	{
		mHostConn->Disconnect();
		delete mHostConn;
		mHostConn = nullptr;
	}
	mClientConnected = false;
	mConnecting = false;
	mMode = MODE_NONE;
	mMySlot = 0;
	mActingPlayerSlot = -1;
	mInputEpoch = 1;
	mNextInputSequence = 1;
	mLastInputMoveTick = 0;
	mClientHeatWaveQDown = false;
	mInputPhase = NETINPUT_MENU;
	mAdventureStartFxGeneration = 0;
	mLastAdventureStartFxGeneration = 0;
	mRemotePaused = false;
	mAdaptiveGoodStreak = 0;
	mAdaptiveBadStreak = 0;
	mLastPingTick = 0;
	mLastStatsTick = 0;
	mRemotePicks.clear();
	mRemoteUnpicks.clear();
	mRemoteChooserStates.clear();
	mPendingChooserStarts.clear();
	mPendingBoardMsgs.clear();
	mChooserStateEchoRequested.store(false, std::memory_order_relaxed);
	PurgePendingInputs("DISCONNECT");
	mChooserOpen = false;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		mRemoteCursors[i].Clear();
		mChooserPickQuota[i] = 0;
		mDeferredDrop[i] = false;
		mLastInputSequence[i] = 0;
		mRemoteHeatWaveQDown[i] = false;
		mRemoteHeatWaveQDownTick[i] = 0;
	}
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		mPlayers[i].Reset();
	NetTrace("disconnect complete");
}

void NetManager::DropSession(int theSlot, bool theNotify)
{
	if (theSlot <= 0 || theSlot >= NET_MAX_PLAYERS)
		return;
	NetSession& aSession = mSessions[theSlot];
	if (!aSession.mAssigned)
		return;
	NetTrace("dropping peer slot=%d nick=%s notify=%d", theSlot, mPlayers[theSlot].mNick, theNotify ? 1 : 0);

	// A disconnected player can no longer finish or cancel a held bank packet.
	// Release that exact reservation before marking the player inactive, then
	// clear the host-authoritative cursor so later conveyor compaction cannot
	// mistake the stale index for another player's held packet.
	if (mApp && mApp->mBoard && theSlot >= 0 && theSlot < NET_MAX_PLAYERS)
	{
		Board* aBoard = mApp->mBoard;
		CursorObject* aCursor = aBoard->mNetPlayerCursors[theSlot];
		if (aCursor)
		{
			if (aCursor->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK &&
				aBoard->mSeedBank && aCursor->mSeedBankIndex >= 0 &&
				aCursor->mSeedBankIndex < aBoard->mSeedBank->mNumPackets)
			{
				SeedPacket& aPacket = aBoard->mSeedBank->mSeedPackets[aCursor->mSeedBankIndex];
				if (aPacket.mPacketType != SeedType::SEED_NONE)
				{
					aPacket.Activate();
					NetTrace("released held seed packet slot=%d index=%d type=%d",
						theSlot, aCursor->mSeedBankIndex, (int)aPacket.mPacketType);
				}
			}

			aCursor->mType = SeedType::SEED_NONE;
			aCursor->mImitaterType = SeedType::SEED_NONE;
			aCursor->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
			aCursor->mSeedBankIndex = -1;
			aCursor->mCoinID = CoinID::COINID_NULL;
			aCursor->mDuplicatorPlantID = PlantID::PLANTID_NULL;
			aCursor->mCobCannonPlantID = PlantID::PLANTID_NULL;
			aCursor->mGlovePlantID = PlantID::PLANTID_NULL;
		}
	}
	NetTrace("drop peer slot=%d checkpoint=disconnect_begin", theSlot);
	aSession.mConn.Disconnect();
	NetTrace("drop peer slot=%d checkpoint=disconnect_complete", theSlot);
	aSession.mAssigned = false;
	mDeferredDrop[theSlot] = false;
	mLastInputSequence[theSlot] = 0;
	NetTrace("drop peer slot=%d checkpoint=purge_inputs_begin", theSlot);
	PurgePendingInputs("PEER_DISCONNECT", theSlot);
	NetTrace("drop peer slot=%d checkpoint=purge_inputs_complete", theSlot);
	ClearHeatWaveShoutKeys("PEER_DISCONNECT", theSlot);
	NetTrace("drop peer slot=%d checkpoint=clear_keys_complete", theSlot);
	mPlayers[theSlot].mActive = false;
	mRemoteCursors[theSlot].Clear();
	RequestChooserStateEcho("peer_disconnect", theSlot, -1);
	if (theNotify)
	{
		NetTrace("drop peer slot=%d checkpoint=lobby_notify_begin", theSlot);
		SendLobbyState();
		NetTrace("drop peer slot=%d checkpoint=lobby_notify_complete", theSlot);
	}
	NetTrace("drop peer slot=%d complete", theSlot);
}

// -------------------------------------------------------------------------
// Per-frame pump
// -------------------------------------------------------------------------
int NetManager::GetPlayerCount() const
{
	int aCount = 0;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		if (mPlayers[i].mActive)
			aCount++;
	return aCount;
}

void NetManager::Update()
{
	if (mMode == MODE_NONE)
		return;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		if (mRemoteCursors[i].mValid)
			mRemoteCursors[i].Interpolate();

	// adopt pending connections
	{
		std::lock_guard<std::mutex> aLock(mPendingConnMutex);
		for (SOCKET aSock : mPendingConns)
		{
			if (mMode == MODE_HOST)
			{
				// find a free slot
				int aFreeSlot = -1;
				for (int i = 1; i < NET_MAX_PLAYERS; i++)
					if (!mSessions[i].mAssigned)
					{
						aFreeSlot = i;
						break;
					}
				if (aFreeSlot == -1)
				{
					closesocket(aSock); // lobby full; they will be dropped
				}
				else
				{
					NetSession& aSession = mSessions[aFreeSlot];
					aSession.mSlot = (uint8_t)aFreeSlot;
					aSession.mAssigned = true;
					aSession.mConn.Adopt(aSock);
					aSession.mConn.SetMessageHandler([this, aFreeSlot](uint8_t aType, NetBuffer& aPayload)
					{
						HandleMessage(&mSessions[aFreeSlot], aType, aPayload);
					});
				}
			}
			else // client: the connect thread's socket
			{
				if (!mHostConn)
				{
					mHostConn = new NetConnection();
					mHostConn->Adopt(aSock);
					mHostConn->SetMessageHandler([this](uint8_t aType, NetBuffer& aPayload)
					{
						HandleMessageClient(aType, aPayload);
					});
					// say hello
					NetBuffer aHello;
					aHello.WriteU32(NET_PROTOCOL_VERSION);
					aHello.WriteString(mConfig.mNick);
					mHostConn->SendMessage(NETMSG_HELLO, aHello);
					mClientConnected = true;
					strcpy_s(mConnectStatus, "Connected, waiting for host...");
				}
			}
		}
		mPendingConns.clear();
	}

	UpdatePings();

	if (mMode == MODE_HOST)
	{
		bool aDropAfterPump[NET_MAX_PLAYERS] = {};
		for (int i = 1; i < NET_MAX_PLAYERS; i++)
		{
			if (mSessions[i].mAssigned)
			{
				mSessions[i].mConn.PumpMessages();
				aDropAfterPump[i] = mDeferredDrop[i] || !mSessions[i].mConn.IsConnected();
			}
		}
		// PumpMessages invokes user callbacks.  Session storage must stay alive
		// until every callback in this pass has unwound.
		for (int i = 1; i < NET_MAX_PLAYERS; i++)
		{
			if (!aDropAfterPump[i])
				continue;
			DropSession(i, true);
			if (mApp && mApp->mBoard)
				mApp->mBoard->mPaused = true;
		}
	}
	else if (mMode == MODE_CLIENT && mHostConn)
	{
		mHostConn->PumpMessages();
		if (!mHostConn->IsConnected())
		{
			mClientConnected = false;
			// host lost: notify the app (returns to menu with a message)
			if (mApp)
				mApp->NetOnHostLost();
		}
	}

	// rate adjustments
	if (mMode == MODE_HOST)
		CheckAdaptiveRate();
}

void NetManager::UpdatePings()
{
	uint32_t aNow = GetTickCount();
	if (mLastPingTick == 0)
		mLastPingTick = aNow;
	if (aNow - mLastPingTick < 2000)
		return;
	mLastPingTick = aNow;

	if (mMode == MODE_HOST)
	{
		for (int i = 1; i < NET_MAX_PLAYERS; i++)
		{
			if (!mSessions[i].mAssigned)
				continue;
			uint32_t aSeq = mSessions[i].mConn.NextPingSeq();
			NetBuffer aPing;
			aPing.WriteU32(aSeq);
			mSessions[i].mConn.SendMessage(NETMSG_PING, aPing);
			NetBuffer aTmp;
			aTmp.WriteU32(aSeq);
			mPlayers[i].mPing = mSessions[i].mConn.GetPing();
		}
		// Keep every lobby's nick/ping list live, not just the host's copy.
		SendLobbyState();
	}
	else if (mMode == MODE_CLIENT && mHostConn && mClientConnected)
	{
		uint32_t aSeq = mHostConn->NextPingSeq();
		NetBuffer aPing;
		aPing.WriteU32(aSeq);
		mHostConn->SendMessage(NETMSG_PING, aPing);

		if (mLastStatsTick == 0 || aNow - mLastStatsTick >= 2000)
		{
			mLastStatsTick = aNow;
			NetBuffer aStats;
			aStats.WriteU16(mHostConn->GetPing());
			int anApplyMs = mApp && mApp->mBoard ? mApp->mBoard->mNetLastApplyMs : 0;
			if (anApplyMs < 0) anApplyMs = 0;
			if (anApplyMs > 65535) anApplyMs = 65535;
			aStats.WriteU16((uint16_t)anApplyMs);
			mHostConn->SendMessage(NETMSG_STATS, aStats);
		}
	}
}

// -------------------------------------------------------------------------
// Host-side message handling
// -------------------------------------------------------------------------
void NetManager::HandleMessage(NetSession* theSession, uint8_t theMsgType, NetBuffer& thePayload)
{
	if (!theSession || theSession->mSlot == 0 || theSession->mSlot >= NET_MAX_PLAYERS ||
		!theSession->mAssigned)
	{
		NetTrace("RX host ignored type=%s(%u) reason=invalid_session",
			NetMessageName(theMsgType), theMsgType);
		return;
	}
	if (mDeferredDrop[theSession->mSlot])
	{
		// A protocol failure is disconnected after PumpMessages unwinds.  Ignore
		// every remaining frame from that peer without invalidating callback-owned
		// session storage in the middle of the receive loop.
		NetTrace("RX host ignored slot=%u type=%s(%u) reason=deferred_drop",
			theSession->mSlot, NetMessageName(theMsgType), theMsgType);
		return;
	}
	if (theMsgType != NETMSG_CURSOR && theMsgType != NETMSG_PING && theMsgType != NETMSG_PONG)
		NetTrace("RX host slot=%u type=%s(%u) bytes=%zu", theSession ? theSession->mSlot : 255, NetMessageName(theMsgType), theMsgType, thePayload.Size());
	switch (theMsgType)
	{
	case NETMSG_PING:
	{
		uint32_t aSeq = thePayload.ReadU32Or();
		NetBuffer anEcho;
		anEcho.WriteU32(aSeq);
		theSession->mConn.SendMessage(NETMSG_PONG, anEcho);
		break;
	}
	case NETMSG_PONG:
		theSession->mConn.OnPong(thePayload.ReadU32Or());
		mPlayers[theSession->mSlot].mPing = theSession->mConn.GetPing();
		break;
	case NETMSG_HELLO:
		OnHello(theSession, thePayload);
		break;
	case NETMSG_INPUT_MOUSE:
		OnClientInput(theSession, thePayload);
		break;
	case NETMSG_INPUT_KEY:
		OnClientKeyInput(theSession, thePayload);
		break;
	case NETMSG_CURSOR:
		OnClientCursor(theSession, thePayload);
		break;
	case NETMSG_SEED_PICK:
		OnClientSeedPick(theSession, thePayload, false);
		break;
	case NETMSG_SEED_UNPICK:
		OnClientSeedPick(theSession, thePayload, true);
		break;
	case NETMSG_TEAM_SELECT:
		OnClientTeamSelect(theSession, thePayload);
		break;
	case NETMSG_STORE_ACTION:
		OnStoreAction(theSession, thePayload);
		break;
	case NETMSG_STATS:
		OnClientStats(theSession, thePayload);
		break;
	case NETMSG_PAUSE_REQ:
		// clients cannot pause (host-only control) — ignored
		break;
	default:
		break;
	}
}

void NetManager::OnHello(NetSession* theSession, NetBuffer& thePayload)
{
	uint32_t aVersion = thePayload.ReadU32Or();
	NetTrace("HELLO slot=%u protocol=%u expected=%u", theSession->mSlot, aVersion, NET_PROTOCOL_VERSION);
	if (aVersion != NET_PROTOCOL_VERSION)
	{
		theSession->mConn.SendMessage(NETMSG_VERSION_MISMATCH);
		mDeferredDrop[theSession->mSlot] = true;
		return;
	}

	NetPlayerInfo& aPlayer = mPlayers[theSession->mSlot];
	aPlayer.mActive = true;
	aPlayer.mSlot = theSession->mSlot;
	aPlayer.mTeam = NETTEAM_NONE;
	aPlayer.mPing = 0;
	thePayload.ReadString(aPlayer.mNick, NET_MAX_NICK + 1);
	if (!aPlayer.mNick[0])
		strcpy_s(aPlayer.mNick, "Player");
	NetTrace("peer authenticated slot=%u nick=%s", theSession->mSlot, aPlayer.mNick);

	NetBuffer aWelcome;
	aWelcome.WriteU8(theSession->mSlot);
	aWelcome.WriteString(mPlayers[0].mNick);
	theSession->mConn.SendMessage(NETMSG_WELCOME, aWelcome);
	// A late peer must learn the current lifetime token on the same ordered TCP
	// stream before it can observe any subsequent board/chooser barrier.
	SendInputEpoch(theSession);
	NetBuffer aRate;
	aRate.WriteU16((uint16_t)mSnapshotHz);
	theSession->mConn.SendMessage(NETMSG_RATE, aRate);

	// NetHostStart() may run several seconds before a peer connects.  The
	// broadcast made at host startup is therefore not a reliable way to give a
	// late peer the host profile.  Queue a fresh copy on the same TCP stream as
	// WELCOME, before that peer can receive START_GAME/CHOOSER_OPEN.  This is
	// gameplay-critical: ChooseSeedsOnCurrentLevel() and available seed packets
	// both depend on profile progress.
	if (mApp)
	{
		NetTrace("PROFILE_SYNC resend for authenticated slot=%u", theSession->mSlot);
		mApp->NetBroadcastProfile();
	}

	SendLobbyState();
}

bool NetManager::ConsumeExpectedInputSequence(uint8_t theSlot, uint32_t theEpoch,
	uint32_t theSequence, const char* theKind)
{
	const char* aKind = theKind ? theKind : "input";
	if (theEpoch != mInputEpoch)
	{
		NetTrace("%s reject slot=%u reason=epoch packetEpoch=%u hostEpoch=%u seq=%u",
			aKind, theSlot, theEpoch, mInputEpoch, theSequence);
		return false;
	}

	uint32_t anExpectedSequence = 0;
	bool aSequenceAccepted = false;
	{
		std::lock_guard<std::mutex> aLock(mSessionsMutex);
		anExpectedSequence = mLastInputSequence[theSlot] + 1U;
		if (theSequence != 0 && theSequence == anExpectedSequence)
		{
			// Mouse and key actions share one ordered stream.  Consume a decoded
			// packet before semantic/context validation so rejection at N can never
			// turn every valid N+1 action into a permanent sequence gap.
			mLastInputSequence[theSlot] = theSequence;
			aSequenceAccepted = true;
		}
	}
	if (!aSequenceAccepted)
	{
		NetTrace("%s reject slot=%u reason=sequence got=%u expected=%u epoch=%u",
			aKind, theSlot, theSequence, anExpectedSequence, theEpoch);
		return false;
	}
	NetTrace("%s sequence consumed slot=%u epoch=%u seq=%u before semantic validation",
		aKind, theSlot, theEpoch, theSequence);
	return true;
}

void NetManager::OnClientInput(NetSession* theSession, NetBuffer& thePayload)
{
	const uint8_t aSlot = theSession ? theSession->mSlot : 255;
	if (thePayload.Remaining() != NET_MOUSE_INPUT_BYTES)
	{
		NetTrace("mouse reject slot=%u reason=size expected=%zu actual=%zu", aSlot,
			NET_MOUSE_INPUT_BYTES, thePayload.Remaining());
		return;
	}

	uint32_t anEpoch = 0;
	uint32_t aSequence = 0;
	uint8_t anAction = 0;
	uint8_t aButtonWire = 0;
	int16_t aCanonicalX = 0;
	int16_t aCanonicalY = 0;
	int16_t aSeedIndex = -1;
	uint16_t aSeedPacketId = 0;
	uint8_t aSeedType = 255;
	uint8_t anImitater = 255;
	uint8_t anOwner = 255;
	const bool aDecoded = thePayload.ReadU32(&anEpoch) && thePayload.ReadU32(&aSequence) &&
		thePayload.ReadU8(&anAction) && thePayload.ReadU8(&aButtonWire) &&
		thePayload.ReadI16(&aCanonicalX) && thePayload.ReadI16(&aCanonicalY) &&
		thePayload.ReadI16(&aSeedIndex) && thePayload.ReadU16(&aSeedPacketId) &&
		thePayload.ReadU8(&aSeedType) &&
		thePayload.ReadU8(&anImitater) && thePayload.ReadU8(&anOwner) &&
		thePayload.Remaining() == 0;
	const int aButton = (int)(int8_t)aButtonWire;
	if (!aDecoded || aSlot == 0 || aSlot >= NET_MAX_PLAYERS ||
		!mPlayers[aSlot].mActive || !theSession->mAssigned)
	{
		NetTrace("mouse reject slot=%u reason=decode_or_session", aSlot);
		return;
	}

	if (!ConsumeExpectedInputSequence(aSlot, anEpoch, aSequence, "mouse"))
		return;

	Board* aBoard = mApp ? mApp->mBoard : nullptr;
	if (mInputPhase != NETINPUT_PLAYING || !aBoard ||
		!mApp || mApp->mGameScene != GameScenes::SCENE_PLAYING ||
		mApp->mSeedChooserScreen || mApp->GetDialogCount() > 0 || aBoard->mPaused)
	{
		NetTrace("mouse reject slot=%u reason=context packetEpoch=%u hostEpoch=%u phase=%u board=%p scene=%d chooser=%p dialogs=%d paused=%d",
			aSlot, anEpoch, mInputEpoch, (unsigned)mInputPhase, (void*)aBoard,
			mApp ? (int)mApp->mGameScene : -1,
			mApp ? (void*)mApp->mSeedChooserScreen : nullptr,
			mApp ? mApp->GetDialogCount() : -1, aBoard && aBoard->mPaused ? 1 : 0);
		return;
	}

	const bool aButtonValid = anAction == NETINPUT_MOUSE_MOVE ?
		(aButton == 0 || NetValidMouseButton(aButton)) : NetValidMouseButton(aButton);
	if (anAction > NETINPUT_MOUSE_MOVE || !aButtonValid ||
		aCanonicalX < -4096 || aCanonicalX > 4096 ||
		aCanonicalY < -2048 || aCanonicalY > 2048)
	{
		NetTrace("mouse reject slot=%u reason=fields action=%u button=%d canonical=(%d,%d)",
			aSlot, anAction, aButton, aCanonicalX, aCanonicalY);
		return;
	}

	const bool aHasSeedIdentity = aSeedIndex >= 0;
	const bool aCleanNoSeedIdentity = aSeedIndex == -1 && aSeedPacketId == 0 && aSeedType == 255 &&
		anImitater == 255 && anOwner == 255;
	if ((!aHasSeedIdentity && !aCleanNoSeedIdentity) ||
		(aHasSeedIdentity && (aSeedIndex >= SEEDBANK_CAPACITY ||
			aSeedPacketId == 0 || aSeedType == 255 ||
			(anImitater != 255 && anImitater >= (uint8_t)SeedType::NUM_SEED_TYPES) ||
			(anOwner >= NET_MAX_PLAYERS && anOwner != NET_SHARED_OWNER))))
	{
		NetTrace("mouse reject slot=%u reason=seed_identity index=%d packetId=%u type=%u imitater=%u owner=%u",
			aSlot, aSeedIndex, aSeedPacketId, aSeedType, anImitater, anOwner);
		return;
	}

	int aHostSeedHit = NetSeedPacketAt(aBoard, aCanonicalX, aCanonicalY);
	if (anAction == NETINPUT_MOUSE_DOWN && aHasSeedIdentity)
	{
		SeedBank* aHostBank = aBoard->mSeedBank;
		if (!aHostBank || aHostBank->mNumPackets < 0 ||
			aHostBank->mNumPackets > SEEDBANK_CAPACITY)
		{
			NetTrace("mouse reject slot=%u reason=host_seed_bank localIndex=%d count=%d", aSlot,
				aSeedIndex, aHostBank ? aHostBank->mNumPackets : -1);
			return;
		}

		// The sender's index is viewer-relative and therefore diagnostic only.  The
		// host-assigned packet id survives conveyor compaction and disambiguates two
		// identical Wall-nuts without trusting client-side layout.  The semantic
		// fields are still checked so a stale/replayed id cannot change card meaning.
		int aHostSeedIndex = -1;
		int aMatchCount = 0;
		for (int i = 0; i < aHostBank->mNumPackets; i++)
		{
			const SeedPacket& aCandidate = aHostBank->mSeedPackets[i];
			if (aCandidate.mNetId == aSeedPacketId &&
				(uint8_t)aCandidate.mPacketType == aSeedType &&
				(uint8_t)aCandidate.mImitaterType == anImitater &&
				aCandidate.mNetOwnerSlot == anOwner)
			{
				aHostSeedIndex = i;
				aMatchCount++;
			}
		}
		if (aMatchCount != 1)
		{
			NetTrace("mouse reject slot=%u reason=seed_packet_id_%s localIndex=%d packetId=%u identity=(%u,%u,%u) matches=%d",
				aSlot, aMatchCount == 0 ? "missing" : "ambiguous", aSeedIndex,
				aSeedPacketId, aSeedType, anImitater, anOwner, aMatchCount);
			return;
		}

		SeedPacket& aPacket = aHostBank->mSeedPackets[aHostSeedIndex];
		NetTrace("mouse seed remap slot=%u localIndex=%d hostIndex=%d packetId=%u identity=(%u,%u,%u)",
			aSlot, aSeedIndex, aHostSeedIndex, aSeedPacketId, aSeedType, anImitater, anOwner);
		// Packet motion between the client's last snapshot and this ordered click
		// must not select a neighbour.  Resolve the validated semantic identity to
		// the host packet's current centre while preserving Board's normal path.
		aCanonicalX = (int16_t)(aHostBank->mX + aPacket.mX + aPacket.mOffsetX + aPacket.mWidth / 2);
		aCanonicalY = (int16_t)(aHostBank->mY + aPacket.mY + aPacket.mHeight / 2);
	}
	else if (anAction == NETINPUT_MOUSE_DOWN && aHostSeedHit >= 0)
	{
		NetTrace("mouse reject slot=%u reason=missing_seed_identity hostIndex=%d", aSlot, aHostSeedHit);
		return;
	}
	else if (anAction != NETINPUT_MOUSE_DOWN && !aCleanNoSeedIdentity)
	{
		NetTrace("mouse reject slot=%u reason=seed_identity_on_non_down action=%u", aSlot, anAction);
		return;
	}

	const int aLocalX = NetLocalX(mApp, aCanonicalX);
	const int aLocalY = NetLocalY(mApp, aCanonicalY);
	if (aLocalX < -32768 || aLocalX > 32767 || aLocalY < -32768 || aLocalY > 32767)
	{
		NetTrace("mouse reject slot=%u reason=local_range local=(%d,%d)", aSlot, aLocalX, aLocalY);
		return;
	}

	NetRemoteInput anInput;
	anInput.mSlot = aSlot;
	anInput.mEpoch = anEpoch;
	anInput.mSequence = aSequence;
	anInput.mAction = anAction;
	anInput.mButton = (int8_t)aButton;
	anInput.mX = (int16_t)aLocalX;
	anInput.mY = (int16_t)aLocalY;
	anInput.mSeedPacketId = aSeedPacketId;
	anInput.mSeedType = aSeedType;
	anInput.mSeedImitater = anImitater;
	anInput.mSeedOwner = anOwner;

	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	if (mPendingInputCount[aSlot] >= NET_INPUT_QUEUE_PER_PLAYER ||
		mPendingInputs.size() >= NET_INPUT_QUEUE_TOTAL)
	{
		NetTrace("mouse drop slot=%u reason=queue_cap slotPending=%zu total=%zu seq=%u",
			aSlot, mPendingInputCount[aSlot], mPendingInputs.size(), aSequence);
		return;
	}
	mPendingInputs.push_back(anInput);
	mPendingInputCount[aSlot]++;
	NetTrace("mouse queued slot=%u epoch=%u seq=%u action=%u button=%d local=(%d,%d) pendingSlot=%zu pendingTotal=%zu",
		aSlot, anEpoch, aSequence, anAction, aButton, anInput.mX, anInput.mY,
		mPendingInputCount[aSlot], mPendingInputs.size());
}

void NetManager::OnClientKeyInput(NetSession* theSession, NetBuffer& thePayload)
{
	const uint8_t aSlot = theSession ? theSession->mSlot : 255;
	if (thePayload.Remaining() != NET_HEAT_WAVE_KEY_INPUT_BYTES)
	{
		NetTrace("key reject slot=%u reason=size expected=%zu actual=%zu", aSlot,
			NET_HEAT_WAVE_KEY_INPUT_BYTES, thePayload.Remaining());
		return;
	}

	uint32_t anEpoch = 0;
	uint32_t aSequence = 0;
	uint16_t aKey = 0;
	uint8_t aDownWire = 0;
	const bool aDecoded = thePayload.ReadU32(&anEpoch) &&
		thePayload.ReadU32(&aSequence) && thePayload.ReadU16(&aKey) &&
		thePayload.ReadU8(&aDownWire) && thePayload.Remaining() == 0;
	if (!aDecoded || aSlot == 0 || aSlot >= NET_MAX_PLAYERS || !theSession ||
		!theSession->mAssigned || !mPlayers[aSlot].mActive)
	{
		NetTrace("key reject slot=%u reason=decode_or_session", aSlot);
		return;
	}
	if (!ConsumeExpectedInputSequence(aSlot, anEpoch, aSequence, "key"))
		return;

	// Consume the sequence first, then reject every semantic value except Q and
	// a canonical 0/1 edge.  This protects the shared mouse/key ordering without
	// ever dispatching an arbitrary remote KeyDown into the widget tree.
	if (aKey != NET_HEAT_WAVE_Q_WIRE || aDownWire > 1)
	{
		NetTrace("key reject slot=%u reason=fields key=%u down=%u epoch=%u seq=%u",
			aSlot, aKey, aDownWire, anEpoch, aSequence);
		return;
	}

	Board* aBoard = mApp ? mApp->mBoard : nullptr;
	const bool aHeatWavePlaying = mInputPhase == NETINPUT_PLAYING && mApp &&
		aBoard && mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HEAT_WAVE &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING && !mApp->mSeedChooserScreen;
	if (!aHeatWavePlaying)
	{
		ClearHeatWaveShoutKeys("KEY_CONTEXT", aSlot);
		NetTrace("key reject slot=%u reason=context down=%u phase=%u mode=%d scene=%d board=%p chooser=%p",
			aSlot, aDownWire, (unsigned)mInputPhase,
			mApp ? (int)mApp->mGameMode : -1,
			mApp ? (int)mApp->mGameScene : -1, (void*)aBoard,
			mApp ? (void*)mApp->mSeedChooserScreen : nullptr);
		return;
	}

	const bool aDown = aDownWire != 0;
	// A release is always allowed while this board/epoch remains current, even
	// if a pause dialog or a Heat Wave substate appeared after the down edge.
	// This is what prevents a legitimate key-up from latching the slot.
	if (aDown && (aBoard->mPaused || mApp->GetDialogCount() > 0))
	{
		NetTrace("key reject slot=%u reason=blocked_down paused=%d dialogs=%d epoch=%u seq=%u",
			aSlot, aBoard->mPaused ? 1 : 0, mApp->GetDialogCount(), anEpoch, aSequence);
		return;
	}

	const uint32_t aNow = GetTickCount();
	bool aChanged = false;
	{
		std::lock_guard<std::mutex> aLock(mSessionsMutex);
		if (aDown)
		{
			// OS key-repeat must not refresh the safety timeout.  Only a real
			// up->down edge starts a fresh shout interval.
			if (!mRemoteHeatWaveQDown[aSlot])
			{
				mRemoteHeatWaveQDown[aSlot] = true;
				mRemoteHeatWaveQDownTick[aSlot] = aNow;
				aChanged = true;
			}
		}
		else
		{
			aChanged = mRemoteHeatWaveQDown[aSlot];
			mRemoteHeatWaveQDown[aSlot] = false;
			mRemoteHeatWaveQDownTick[aSlot] = 0;
		}
	}
	NetTrace("heat-wave Q slot=%u down=%d changed=%d epoch=%u seq=%u",
		aSlot, aDown ? 1 : 0, aChanged ? 1 : 0, anEpoch, aSequence);
}

void NetManager::OnClientCursor(NetSession* theSession, NetBuffer& thePayload)
{
	// The connection owns the slot.  Never trust the slot claimed in a packet.
	thePayload.ReadU8Or();
	const uint8_t aSlot = theSession->mSlot;
	if (aSlot >= NET_MAX_PLAYERS || !mPlayers[aSlot].mActive)
		return;
	NetRemoteCursor& aCursor = mRemoteCursors[aSlot];
	int16_t aCanonicalX = thePayload.ReadI16Or();
	int16_t aCanonicalY = thePayload.ReadI16Or();
	aCursor.SetTarget((int16_t)NetLocalX(mApp, aCanonicalX), (int16_t)NetLocalY(mApp, aCanonicalY));
	if (mApp && mApp->mWidgetManager)
		mApp->mWidgetManager->MarkAllDirty();
	uint8_t aClaimedCursorType = thePayload.ReadU8Or();
	uint8_t aClaimedSeed = thePayload.ReadU8Or();
	uint8_t aClaimedImitater = thePayload.ReadU8Or();
	aCursor.mFlags = thePayload.ReadU8Or();

	// During a game the host's per-player cursor is authoritative.  Clients
	// provide only pointer coordinates, never the plant/tool they claim to hold.
	CursorObject* anAuthoritativeCursor = mApp && mApp->mBoard ? mApp->mBoard->mNetPlayerCursors[aSlot] : nullptr;
	if (anAuthoritativeCursor)
	{
		aCursor.mCursorType = (uint8_t)anAuthoritativeCursor->mCursorType;
		aCursor.mHeldSeed = (uint8_t)anAuthoritativeCursor->mType;
		aCursor.mHeldImitater = (uint8_t)anAuthoritativeCursor->mImitaterType;
	}
	else
	{
		aCursor.mCursorType = aClaimedCursorType;
		aCursor.mHeldSeed = aClaimedSeed;
		aCursor.mHeldImitater = aClaimedImitater;
	}
	if (!NetIsValidHeldSeed(aCursor.mHeldSeed))
		aCursor.mHeldSeed = 255;
	if (aCursor.mHeldImitater != 255 && aCursor.mHeldImitater >= (uint8_t)SeedType::NUM_SEED_TYPES)
		aCursor.mHeldImitater = 255;

	// Relay the sanitized state to every client.  The sender uses the echo to
	// update its local held silhouette; other peers see the remote cursor.
	NetBuffer aRelay;
	aRelay.WriteU8(aSlot);
	aRelay.WriteI16(aCanonicalX);
	aRelay.WriteI16(aCanonicalY);
	aRelay.WriteU8(aCursor.mCursorType);
	aRelay.WriteU8(aCursor.mHeldSeed);
	aRelay.WriteU8(aCursor.mHeldImitater);
	aRelay.WriteU8(aCursor.mFlags);
	BroadcastMessage(NETMSG_CURSOR, aRelay);
}

void NetManager::OnClientSeedPick(NetSession* theSession, NetBuffer& thePayload, bool theUnpick)
{
	const size_t anExpectedBytes = theUnpick ? 1U : 2U;
	if (thePayload.Remaining() != anExpectedBytes)
	{
		NetTrace("chooser %s rejected: expected=%zu bytes actual=%zu",
			theUnpick ? "unpick" : "pick", anExpectedBytes, thePayload.Remaining());
		RequestChooserStateEcho("payload_size", theSession ? theSession->mSlot : -1, -1);
		return;
	}

	const uint8_t aSeedType = thePayload.ReadU8Or();
	const uint8_t anImitater = theUnpick ? 0xFF : thePayload.ReadU8Or();
	const int aSlot = theSession ? theSession->mSlot : -1;
	const bool aPvP = mApp && mApp->mGameMode == GameMode::GAMEMODE_COOP_PVP;
	const bool anAuthenticatedPlayer = theSession && aSlot > 0 && aSlot < NET_MAX_PLAYERS &&
		theSession->mAssigned && mPlayers[aSlot].mActive && mPlayers[aSlot].mSlot == aSlot;
	const bool anEligiblePlayer = anAuthenticatedPlayer &&
		(!aPvP || mPlayers[aSlot].mTeam == NETTEAM_PLANTS);
	const bool aChooserContext = mChooserOpen && mApp && mApp->mBoard &&
		mApp->mBoard->mCutScene && mApp->mSeedChooserScreen &&
		mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO;
	if (!anEligiblePlayer || !aChooserContext)
	{
		NetTrace("chooser %s rejected: slot=%d authenticated=%d eligible=%d open=%d app=%p board=%p intro=%p screen=%p scene=%d",
			theUnpick ? "unpick" : "pick", aSlot, anAuthenticatedPlayer ? 1 : 0,
			anEligiblePlayer ? 1 : 0, mChooserOpen ? 1 : 0, (void*)mApp,
			mApp ? (void*)mApp->mBoard : nullptr,
			mApp && mApp->mBoard ? (void*)mApp->mBoard->mCutScene : nullptr,
			mApp ? (void*)mApp->mSeedChooserScreen : nullptr,
			mApp ? (int)mApp->mGameScene : -1);
		RequestChooserStateEcho(!anEligiblePlayer ? "slot_or_team" : "context",
			aSlot, aSeedType);
		return;
	}

	SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;
	const bool aValidSeed = aSeedType < (uint8_t)NUM_SEEDS_IN_CHOOSER &&
		mApp->SeedTypeAvailable((SeedType)aSeedType) &&
		!aChooser->SeedNotAllowedToPick((SeedType)aSeedType) &&
		!aChooser->SeedNotAllowedDuringTrial((SeedType)aSeedType);
	bool aValidImitater = true;
	if (!theUnpick)
	{
		if (aSeedType == (uint8_t)SeedType::SEED_IMITATER)
		{
			// Match ImitaterDialog::SeedHitTest: only unlocked base plants may
			// be copied, and level-specific chooser restrictions still apply.
			aValidImitater = anImitater < (uint8_t)SeedType::SEED_GATLINGPEA &&
				mApp->SeedTypeAvailable((SeedType)anImitater) &&
				!aChooser->SeedNotAllowedToPick((SeedType)anImitater);
		}
		else
		{
			aValidImitater = anImitater == 0xFF;
		}
	}

	// Crazy Dave's locked cards are global preselection, not a remote
	// player's mutable pick.  Ordinary fixed challenge cards are likewise
	// absent from that player's remote list, so an unpick cannot remove them.
	const bool aLockedDaveSeed = aValidSeed &&
		aChooser->mChosenSeeds[aSeedType].mCrazyDavePicked;
	if (!aValidSeed || !aValidImitater || aLockedDaveSeed)
	{
		NetTrace("chooser %s rejected: slot=%d seed=%u imitater=%u validSeed=%d validImitater=%d locked=%d",
			theUnpick ? "unpick" : "pick", aSlot, aSeedType, anImitater,
			aValidSeed ? 1 : 0, aValidImitater ? 1 : 0, aLockedDaveSeed ? 1 : 0);
		RequestChooserStateEcho(!aValidSeed ? "seed" :
			(!aValidImitater ? "imitater" : "fixed_seed"), aSlot, aSeedType);
		return;
	}

	// Forward only fully validated chooser actions.  The chooser applies the
	// authoritative per-slot quota and duplicate rules in queue order.
	ChooserPick aPick;
	aPick.mSeedType = aSeedType;
	aPick.mImitater = anImitater;
	aPick.mSlot = aSlot;
	if (theUnpick)
		mRemoteUnpicks.push_back(aPick);
	else
		mRemotePicks.push_back(aPick);
}

void NetManager::OnClientTeamSelect(NetSession* theSession, NetBuffer& thePayload)
{
	uint8_t aTeam = thePayload.ReadU8Or();
	if (aTeam > NETTEAM_ZOMBIES || (mApp && mApp->mBoard))
		return;
	mPlayers[theSession->mSlot].mTeam = aTeam;
	SendLobbyState();
}

void NetManager::OnClientStats(NetSession* theSession, NetBuffer& thePayload)
{
	// ping ms (already measured), snapshot apply time
	uint16_t aPing = thePayload.ReadU16Or();
	uint16_t anApplyMs = thePayload.ReadU16Or();
	NetTrace("client stats slot=%u pingMs=%u snapshotApplyMs=%u rateHz=%d", theSession->mSlot, aPing, anApplyMs, mSnapshotHz);
	mPlayers[theSession->mSlot].mPing = aPing;

	if (aPing < 60 && anApplyMs < 8)
	{
		mAdaptiveGoodStreak++;
		mAdaptiveBadStreak = 0;
	}
	else if (aPing > 150 || anApplyMs > 20)
	{
		mAdaptiveGoodStreak = 0;
		mAdaptiveBadStreak++;
	}
	else
	{
		mAdaptiveGoodStreak = 0;
		mAdaptiveBadStreak = 0;
	}
}

void NetManager::CheckAdaptiveRate()
{
	if (mAdaptiveGoodStreak >= 3 && mSnapshotHz < NET_SNAPSHOT_HZ_MAX)
	{
		SetSnapshotHz(mSnapshotHz + 1);
		mAdaptiveGoodStreak = 0;
	}
	else if (mAdaptiveBadStreak >= 2 && mSnapshotHz > NET_SNAPSHOT_HZ_MIN)
	{
		SetSnapshotHz(mSnapshotHz - 1);
		mAdaptiveBadStreak = 0;
	}
}

void NetManager::SetSnapshotHz(int theHz)
{
	if (theHz < NET_SNAPSHOT_HZ_MIN) theHz = NET_SNAPSHOT_HZ_MIN;
	if (theHz > NET_SNAPSHOT_HZ_MAX) theHz = NET_SNAPSHOT_HZ_MAX;
	if (theHz == mSnapshotHz)
		return;
	mSnapshotHz = theHz;
	NetBuffer aRate;
	aRate.WriteU16((uint16_t)theHz);
	BroadcastMessage(NETMSG_RATE, aRate);
}

// -------------------------------------------------------------------------
// Client-side message handling
// -------------------------------------------------------------------------
void NetManager::HandleMessageClient(uint8_t theMsgType, NetBuffer& thePayload)
{
	if (theMsgType != NETMSG_CURSOR && theMsgType != NETMSG_SNAPSHOT && theMsgType != NETMSG_DAVE_STATE &&
		theMsgType != NETMSG_PING && theMsgType != NETMSG_PONG)
		NetTrace("RX client type=%s(%u) bytes=%zu", NetMessageName(theMsgType), theMsgType, thePayload.Size());
	switch (theMsgType)
	{
	case NETMSG_PING:
	{
		uint32_t aSeq = thePayload.ReadU32Or();
		NetBuffer anEcho;
		anEcho.WriteU32(aSeq);
		mHostConn->SendMessage(NETMSG_PONG, anEcho);
		break;
	}
	case NETMSG_PONG:
		mHostConn->OnPong(thePayload.ReadU32Or());
		break;
	case NETMSG_WELCOME:
		OnWelcome(thePayload);
		break;
	case NETMSG_LOBBY_FULL:
		strcpy_s(mConnectStatus, "Lobby is full (4 players max)");
		break;
	case NETMSG_VERSION_MISMATCH:
		strcpy_s(mConnectStatus, "Version mismatch with host");
		break;
	case NETMSG_LOBBY_STATE:
		OnLobbyState(thePayload);
		break;
	case NETMSG_START_GAME:
		OnStartGame(thePayload);
		break;
	case NETMSG_SNAPSHOT:
		OnSnapshot(thePayload);
		break;
	case NETMSG_BOARD_EVENT:
		OnBoardEvent(thePayload);
		break;
	case NETMSG_CURSOR:
		OnCursor(thePayload);
		break;
	case NETMSG_PAUSE:
		mRemotePaused = thePayload.ReadU8Or() != 0;
		break;
	case NETMSG_INPUT_EPOCH:
		OnInputEpoch(thePayload);
		break;
	case NETMSG_CHOOSER_OPEN:
		OnChooserOpen(thePayload);
		break;
	case NETMSG_CHOOSER_STATE:
		OnChooserState(thePayload);
		break;
	case NETMSG_CHOOSER_START:
		OnChooserStart(thePayload);
		break;
	case NETMSG_RATE:
		mSnapshotHz = thePayload.ReadU16Or();
		break;
	case NETMSG_BACK_TO_MENU:
		ClearPendingBoardMessages("BACK_TO_MENU");
		if (mApp)
			mApp->NetOnBackToMenu();
		break;
	case NETMSG_START_PLAYING:
		if (mApp && mApp->mBoard && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			NetTrace("client applying START_PLAYING scene=%d seedChooser=%p", (int)mApp->mGameScene, (void*)mApp->mSeedChooserScreen);
			if (!mPendingChooserStarts.empty() && mApp->mSeedChooserScreen)
			{
				NetBuffer aFinalBank = std::move(mPendingChooserStarts.back());
				if (!mApp->mSeedChooserScreen->CoopBuildBankFromPayload(aFinalBank))
				{
					NetTrace("START_PLAYING rejected: malformed final chooser payload");
					mPendingChooserStarts.clear();
					break;
				}
				NetTrace("final chooser bank applied packets=%d", mApp->mBoard->mSeedBank->mNumPackets);
			}
			else if (mApp->mBoard->ChooseSeedsOnCurrentLevel() &&
				mApp->mBoard->mCutScene && mApp->mBoard->mCutScene->mSeedChoosing)
			{
				NetTrace("START_PLAYING deferred: chooser level has no final bank payload");
				break;
			}
			mPendingChooserStarts.clear();
			mRemoteChooserStates.clear();
			mChooserOpen = false;
			mApp->StartPlaying();
		}
		break;
	case NETMSG_INTRO_STATE:
		if (mApp && mApp->mBoard && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			if (mApp->mBoard->mCutScene && !mApp->mBoard->mCutScene->mPreloaded)
				mApp->mBoard->mCutScene->PreloadResources();
			mApp->mBoard->NetApplyIntroState(thePayload);
		}
		break;
	case NETMSG_DAVE_STATE:
		if (mApp && mApp->mBoard && mApp->mGameScene == GameScenes::SCENE_PLAYING)
			mApp->mBoard->NetApplyDaveState(thePayload);
		else
			NetTrace("DAVE_STATE ignored without an active playing board");
		break;
	case NETMSG_SCENE_CMD:
		OnSceneCmd(thePayload);
		break;
	case NETMSG_PROFILE_SYNC:
		if (mApp)
			mApp->NetOnProfileSync(thePayload);
		break;
	default:
		break;
	}
}

void NetManager::OnSceneCmd(NetBuffer& thePayload)
{
	static const size_t SCENE_CMD_BYTES = sizeof(uint8_t) + sizeof(uint32_t);
	if (mMode != MODE_CLIENT || !mApp)
	{
		NetTrace("SCENE_CMD rejected: invalid receiver mode=%d app=%p", (int)mMode, (void*)mApp);
		return;
	}
	if (thePayload.Remaining() != SCENE_CMD_BYTES)
	{
		NetTrace("SCENE_CMD rejected: expected=%zu bytes actual=%zu", SCENE_CMD_BYTES, thePayload.Remaining());
		return;
	}

	uint8_t aCmd = 0;
	uint32_t aParam = 0;
	if (!thePayload.ReadU8(&aCmd) || !thePayload.ReadU32(&aParam) || thePayload.Remaining() != 0)
	{
		NetTrace("SCENE_CMD rejected: truncated or trailing payload");
		return;
	}
	if (aCmd > SCENECMD_ADVENTURE_START_FX)
	{
		NetTrace("SCENE_CMD rejected: command out of range cmd=%u", (unsigned)aCmd);
		return;
	}

	bool aParamValid = false;
	switch (aCmd)
	{
	case SCENECMD_SHOW_CHALLENGE_PAGE:
		aParamValid = aParam < (uint32_t)MAX_CHALLANGE_PAGES;
		break;
	case SCENECMD_SHOW_AWARD:
		aParamValid = aParam <= (uint32_t)AWARD_PRECREDITS_ZOMBIENOTE;
		break;
	case SCENECMD_ADVENTURE_START_FX:
		aParamValid = aParam != 0;
		break;
	default:
		aParamValid = aParam == 0;
		break;
	}
	if (!aParamValid)
	{
		NetTrace("SCENE_CMD rejected: invalid param cmd=%u param=%u", (unsigned)aCmd, aParam);
		return;
	}

	if (aCmd == SCENECMD_ADVENTURE_START_FX)
	{
		if (mApp->mGameScene != GameScenes::SCENE_MENU || !mApp->mGameSelector || mApp->mBoard)
		{
			NetTrace("Adventure start FX rejected: context scene=%d selector=%p board=%p generation=%u",
				(int)mApp->mGameScene, (void*)mApp->mGameSelector, (void*)mApp->mBoard, aParam);
			return;
		}
		if (mLastAdventureStartFxGeneration != 0 &&
			(int32_t)(aParam - mLastAdventureStartFxGeneration) <= 0)
		{
			NetTrace("Adventure start FX rejected: stale generation=%u last=%u",
				aParam, mLastAdventureStartFxGeneration);
			return;
		}
	}

	if (!mApp->NetOnSceneCmd(aCmd, aParam))
	{
		NetTrace("SCENE_CMD rejected by app cmd=%u param=%u", (unsigned)aCmd, aParam);
		return;
	}

	if (aCmd == SCENECMD_ADVENTURE_START_FX)
		mLastAdventureStartFxGeneration = aParam;
	NetTrace("SCENE_CMD applied cmd=%u param=%u", (unsigned)aCmd, aParam);
}

void NetManager::OnWelcome(NetBuffer& thePayload)
{
	mMySlot = thePayload.ReadU8Or();
	char aHostNick[NET_MAX_NICK + 1];
	thePayload.ReadString(aHostNick, NET_MAX_NICK + 1);
	mPlayers[0].Reset();
	mPlayers[0].mActive = true;
	mPlayers[0].mSlot = 0;
	strncpy_s(mPlayers[0].mNick, aHostNick, NET_MAX_NICK);
	NetTrace("WELCOME assignedSlot=%u hostNick=%s", mMySlot, aHostNick);
}

void NetManager::EncodeLobbyState(NetBuffer& theBuffer)
{
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		const NetPlayerInfo& aP = mPlayers[i];
		theBuffer.WriteU8(aP.mActive ? 1 : 0);
		theBuffer.WriteU8(i);
		theBuffer.WriteString(aP.mNick);
		theBuffer.WriteU16(aP.mPing);
		theBuffer.WriteU8(aP.mTeam);
		theBuffer.WriteU8(aP.mReady);
	}
}

void NetManager::OnLobbyState(NetBuffer& thePayload)
{
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		NetPlayerInfo& aP = mPlayers[i];
		bool aWasActive = aP.mActive;
		uint8_t aActive = thePayload.ReadU8Or();
		uint8_t aSlot = thePayload.ReadU8Or();
		aP.mActive = aActive != 0;
		aP.mSlot = aSlot;
		thePayload.ReadString(aP.mNick, NET_MAX_NICK + 1);
		aP.mPing = thePayload.ReadU16Or();
		aP.mTeam = thePayload.ReadU8Or();
		aP.mReady = thePayload.ReadU8Or();
	}
}

void NetManager::OnStartGame(NetBuffer& thePayload)
{
	// START_GAME is the lifetime barrier between two boards.  Nothing queued
	// for the previous board may survive long enough to reach the new one.
	ClearPendingBoardMessages("START_GAME");

	// hand over to the app: it will create the board in client mode
	if (mApp)
		mApp->NetOnStartGame(thePayload);
}

void NetManager::OnSnapshot(NetBuffer& thePayload)
{
	if (mApp && mApp->CoopSmokeActive())
		mApp->CoopSmokeOnSnapshotRx(NetFnv1a32(thePayload.Data(), thePayload.Size()),
			thePayload.Size());
	static uint32_t aSnapshotCount = 0;
	aSnapshotCount++;
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	if (aSnapshotCount == 1 || aSnapshotCount % 30 == 0)
		NetTrace("snapshot received count=%u bytes=%zu pendingBefore=%zu", aSnapshotCount, thePayload.Size(), mPendingBoardMsgs.size());

	// A snapshot is a complete authoritative state.  Keep only the newest
	// pending snapshot, but preserve every reliable board event and its order.
	// The former total-queue cap could silently discard a BOARDEV packet.
	size_t aRemovedSnapshots = 0;
	for (std::deque<PendingMsg>::iterator anIt = mPendingBoardMsgs.begin(); anIt != mPendingBoardMsgs.end(); )
	{
		if (anIt->mKind == 1)
		{
			anIt = mPendingBoardMsgs.erase(anIt);
			aRemovedSnapshots++;
		}
		else
			++anIt;
	}
	if (aRemovedSnapshots > 0)
		NetTrace("snapshot coalesced older=%zu eventsPreserved=%zu", aRemovedSnapshots, mPendingBoardMsgs.size());

	PendingMsg aMsg;
	aMsg.mKind = 1;
	aMsg.mData = std::move(thePayload);
	mPendingBoardMsgs.push_back(std::move(aMsg));
}

void NetManager::OnBoardEvent(NetBuffer& thePayload)
{
	uint8_t anEvent = thePayload.Size() ? thePayload.mData[0] : 255;
	NetTrace("board event received type=%u bytes=%zu", anEvent, thePayload.Size());
	// Intro preview entities are created locally by CutScene on every peer.
	// Applying the host's preview events as well would duplicate them and could
	// access reanimation definitions before the client's preload has completed.
	if (mApp && mApp->mBoard && mApp->mGameScene != GameScenes::SCENE_PLAYING)
	{
		NetTrace("board event dropped outside gameplay type=%u scene=%d", anEvent, (int)mApp->mGameScene);
		return;
	}
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	PendingMsg aMsg;
	aMsg.mKind = 0;
	aMsg.mData = std::move(thePayload);
	mPendingBoardMsgs.push_back(std::move(aMsg));
}

void NetManager::ClearPendingBoardMessages(const char* theReason)
{
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	if (mPendingBoardMsgs.empty())
		return;

	size_t anEventCount = 0;
	size_t aSnapshotCount = 0;
	for (std::deque<PendingMsg>::const_iterator anIt = mPendingBoardMsgs.begin(); anIt != mPendingBoardMsgs.end(); ++anIt)
	{
		if (anIt->mKind == 0)
			anEventCount++;
		else
			aSnapshotCount++;
	}
	NetTrace("pending board queue cleared reason=%s events=%zu snapshots=%zu",
		theReason ? theReason : "unspecified", anEventCount, aSnapshotCount);
	mPendingBoardMsgs.clear();
}

void NetManager::PurgePendingInputs(const char* theReason, int theSlot)
{
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	size_t aRemoved = 0;
	if (theSlot < 0)
	{
		aRemoved = mPendingInputs.size();
		mPendingInputs.clear();
		for (int i = 0; i < NET_MAX_PLAYERS; i++)
			mPendingInputCount[i] = 0;
	}
	else if (theSlot < NET_MAX_PLAYERS)
	{
		for (std::deque<NetRemoteInput>::iterator anIt = mPendingInputs.begin();
			anIt != mPendingInputs.end(); )
		{
			if (anIt->mSlot == theSlot)
			{
				anIt = mPendingInputs.erase(anIt);
				aRemoved++;
			}
			else
				++anIt;
		}
		mPendingInputCount[theSlot] = 0;
	}
	if (aRemoved > 0)
		NetTrace("pending input queue cleared reason=%s slot=%d removed=%zu remaining=%zu",
			theReason ? theReason : "unspecified", theSlot, aRemoved, mPendingInputs.size());
}

void NetManager::ClearHeatWaveShoutKeys(const char* theReason, int theSlot)
{
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	const int aFirst = theSlot < 0 ? 0 : theSlot;
	const int anEnd = theSlot < 0 ? NET_MAX_PLAYERS : theSlot + 1;
	bool aChanged = false;
	for (int i = aFirst; i < anEnd && i < NET_MAX_PLAYERS; i++)
	{
		if (i < 0)
			continue;
		aChanged = aChanged || mRemoteHeatWaveQDown[i];
		mRemoteHeatWaveQDown[i] = false;
		mRemoteHeatWaveQDownTick[i] = 0;
	}
	if (aChanged)
		NetTrace("heat-wave Q cleared reason=%s slot=%d",
			theReason ? theReason : "unspecified", theSlot);
}

bool NetManager::IsRemoteHeatWaveShoutKeyActive()
{
	if (mMode != MODE_HOST || mInputPhase != NETINPUT_PLAYING || !mApp ||
		!mApp->mBoard || mApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_HEAT_WAVE ||
		mApp->mGameScene != GameScenes::SCENE_PLAYING || mApp->mBoard->mPaused)
		return false;

	const uint32_t aNow = GetTickCount();
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	for (int i = 1; i < NET_MAX_PLAYERS; i++)
	{
		if (!mRemoteHeatWaveQDown[i])
			continue;
		if (!mPlayers[i].mActive ||
			aNow - mRemoteHeatWaveQDownTick[i] > NET_HEAT_WAVE_Q_MAX_HOLD_MS)
		{
			mRemoteHeatWaveQDown[i] = false;
			mRemoteHeatWaveQDownTick[i] = 0;
			continue;
		}
		return true;
	}
	return false;
}

bool NetManager::PopPendingInput(NetRemoteInput& theInput)
{
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	if (mPendingInputs.empty())
		return false;
	theInput = mPendingInputs.front();
	mPendingInputs.pop_front();
	if (theInput.mSlot < NET_MAX_PLAYERS && mPendingInputCount[theInput.mSlot] > 0)
		mPendingInputCount[theInput.mSlot]--;
	return true;
}

void NetManager::SendInputEpoch(NetSession* theSession)
{
	if (mMode != MODE_HOST)
		return;
	NetBuffer aMsg;
	aMsg.WriteU32(mInputEpoch);
	aMsg.WriteU8((uint8_t)mInputPhase);
	if (theSession)
	{
		NetTrace("input epoch TX slot=%u epoch=%u phase=%u", theSession->mSlot,
			mInputEpoch, (unsigned)mInputPhase);
		theSession->mConn.SendMessage(NETMSG_INPUT_EPOCH, aMsg);
	}
	else
	{
		NetTrace("input epoch TX broadcast epoch=%u phase=%u", mInputEpoch, (unsigned)mInputPhase);
		BroadcastMessage(NETMSG_INPUT_EPOCH, aMsg);
	}
}

void NetManager::AdvanceInputPhase(NetInputPhase thePhase, const char* theReason)
{
	if (mMode != MODE_HOST || thePhase > NETINPUT_PLAYING)
		return;
	uint32_t aNextEpoch = mInputEpoch + 1U;
	if (aNextEpoch == 0)
		aNextEpoch = 1;
	mInputEpoch = aNextEpoch;
	mInputPhase = thePhase;
	mChooserOpen = thePhase == NETINPUT_CHOOSER;
	PurgePendingInputs(theReason ? theReason : "INPUT_PHASE");
	ClearHeatWaveShoutKeys(theReason ? theReason : "INPUT_PHASE");
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		mLastInputSequence[i] = 0;
	NetTrace("input phase advanced epoch=%u phase=%u reason=%s", mInputEpoch,
		(unsigned)mInputPhase, theReason ? theReason : "unspecified");
	SendInputEpoch(nullptr);
}

void NetManager::OnInputEpoch(NetBuffer& thePayload)
{
	if (thePayload.Remaining() != 5)
	{
		NetTrace("input epoch reject reason=size expected=5 actual=%zu", thePayload.Remaining());
		return;
	}
	uint32_t anEpoch = 0;
	uint8_t aPhaseWire = 0;
	if (!thePayload.ReadU32(&anEpoch) || !thePayload.ReadU8(&aPhaseWire) ||
		thePayload.Remaining() != 0 || anEpoch == 0 || aPhaseWire > NETINPUT_PLAYING)
	{
		NetTrace("input epoch reject reason=fields epoch=%u phase=%u trailing=%zu",
			anEpoch, aPhaseWire, thePayload.Remaining());
		return;
	}
	NetInputPhase aPhase = (NetInputPhase)aPhaseWire;
	if (anEpoch < mInputEpoch || (anEpoch == mInputEpoch && aPhase != mInputPhase))
	{
		NetTrace("input epoch reject reason=rollback got=(%u,%u) current=(%u,%u)",
			anEpoch, aPhaseWire, mInputEpoch, (unsigned)mInputPhase);
		return;
	}
	if (anEpoch == mInputEpoch && aPhase == mInputPhase)
	{
		NetTrace("input epoch duplicate epoch=%u phase=%u", anEpoch, aPhaseWire);
		return;
	}
	mInputEpoch = anEpoch;
	mInputPhase = aPhase;
	mNextInputSequence = 1;
	mClientHeatWaveQDown = false;
	mChooserOpen = aPhase == NETINPUT_CHOOSER;
	NetTrace("input epoch applied epoch=%u phase=%u", mInputEpoch, (unsigned)mInputPhase);
}

void NetManager::OnCursor(NetBuffer& thePayload)
{
	uint8_t aSlot = thePayload.ReadU8Or();
	if (aSlot >= NET_MAX_PLAYERS)
		return;
	int16_t aX = thePayload.ReadI16Or();
	int16_t aY = thePayload.ReadI16Or();
	uint8_t aCursorType = thePayload.ReadU8Or();
	uint8_t aHeldSeed = thePayload.ReadU8Or();
	uint8_t aHeldImitater = thePayload.ReadU8Or();
	uint8_t aFlags = thePayload.ReadU8Or();

	// Echoes for our own slot carry the host-confirmed held seed.  Apply only
	// that semantic state locally; local mouse coordinates remain immediate.
	if (aSlot == mMySlot)
	{
		if (mApp && mApp->mBoard)
		{
			CursorObject* aLocalCursor = mApp->mBoard->mCursorObject;
			aLocalCursor->mCursorType = (CursorType)aCursorType;
			aLocalCursor->mType = NetIsValidHeldSeed(aHeldSeed) && aHeldSeed != 255 ? (SeedType)aHeldSeed : SeedType::SEED_NONE;
			aLocalCursor->mImitaterType = aHeldImitater < (uint8_t)SeedType::NUM_SEED_TYPES ? (SeedType)aHeldImitater : SeedType::SEED_NONE;
			if (aLocalCursor->mCursorType == CursorType::CURSOR_TYPE_NORMAL)
				aLocalCursor->mSeedBankIndex = -1;
		}
		return;
	}

	NetRemoteCursor& aCursor = mRemoteCursors[aSlot];
	aCursor.SetTarget((int16_t)NetLocalX(mApp, aX), (int16_t)NetLocalY(mApp, aY));
	aCursor.mCursorType = aCursorType;
	aCursor.mHeldSeed = aHeldSeed;
	aCursor.mHeldImitater = aHeldImitater;
	aCursor.mFlags = aFlags;
	if (mApp && mApp->mWidgetManager)
		mApp->mWidgetManager->MarkAllDirty();
}

void NetManager::OnChooserOpen(NetBuffer& thePayload)
{
	static const size_t CHOOSER_OPEN_BYTES = 1 + NET_MAX_PLAYERS;
	if (thePayload.Remaining() != CHOOSER_OPEN_BYTES)
	{
		NetTrace("CHOOSER_OPEN rejected: expected=%zu bytes actual=%zu",
			CHOOSER_OPEN_BYTES, thePayload.Remaining());
		return;
	}

	const uint8_t aBankCount = thePayload.ReadU8Or();
	uint8_t aQuotas[NET_MAX_PLAYERS] = {};
	int aQuotaTotal = 0;
	bool aQuotaInRange = aBankCount <= SEEDBANK_CAPACITY;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		aQuotas[i] = thePayload.ReadU8Or();
		aQuotaTotal += aQuotas[i];
		if (aQuotas[i] > SEEDBANK_CAPACITY)
			aQuotaInRange = false;
	}

	// Validate the complete generation before ShowSeedChooserScreen or any
	// manager field is changed.  The client derives the same bank size and
	// deterministic slot split as the host sender; accepting merely a matching
	// sum would let a stale/malformed packet assign another player's quota.
	const bool aContextValid = IsClient() && mApp && mApp->mBoard &&
		mApp->mBoard->mCutScene && mApp->mWidgetManager &&
		mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO &&
		mMySlot < NET_MAX_PLAYERS && mPlayers[mMySlot].mActive &&
		mPlayers[mMySlot].mSlot == mMySlot;
	const bool aPvP = aContextValid && mApp->mGameMode == GameMode::GAMEMODE_COOP_PVP;
	const int anExpectedBankCount = aContextValid ?
		(aPvP ? PVP_PLANT_PACKET_COUNT : mApp->mBoard->GetNumSeedsInBank()) : -1;
	const int anExpectedFixedCount = aContextValid ?
		SeedChooserScreen::CoopExpectedGlobalFixedPicks(mApp, mApp->mBoard) : -1;
	int anExpectedQuotas[NET_MAX_PLAYERS] = {};
	int anEligibleCount = 0;
	const bool anExpectedSplitValid = aContextValid &&
		anExpectedBankCount >= 0 && anExpectedBankCount <= SEEDBANK_CAPACITY &&
		anExpectedFixedCount >= 0 && anExpectedFixedCount <= anExpectedBankCount &&
		ComputeChooserQuotas(anExpectedBankCount, anExpectedFixedCount, aPvP,
			anExpectedQuotas, &anEligibleCount);

	bool aSplitMatches = anExpectedSplitValid && aBankCount == anExpectedBankCount &&
		aQuotaTotal == aBankCount;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		if ((int)aQuotas[i] != anExpectedQuotas[i])
			aSplitMatches = false;
	}
	if (thePayload.Remaining() != 0 || !aQuotaInRange || !aContextValid ||
		anExpectedBankCount < 0 || anExpectedBankCount > SEEDBANK_CAPACITY ||
		!anExpectedSplitValid || !aSplitMatches)
	{
		NetTrace("CHOOSER_OPEN rejected: bank=%u expectedBank=%d fixed=%d quotas=[%u,%u,%u,%u] expected=[%d,%d,%d,%d] total=%d eligible=%d slot=%u context=%d range=%d splitValid=%d trailing=%zu",
			aBankCount, anExpectedBankCount, anExpectedFixedCount,
			aQuotas[0], aQuotas[1], aQuotas[2], aQuotas[3],
			anExpectedQuotas[0], anExpectedQuotas[1], anExpectedQuotas[2], anExpectedQuotas[3],
			aQuotaTotal, anEligibleCount, mMySlot, aContextValid ? 1 : 0,
			aQuotaInRange ? 1 : 0, anExpectedSplitValid ? 1 : 0,
			thePayload.Remaining());
		return;
	}

	// Survival repick / any mid-game chooser reopen: create the inert client
	// widget first, then commit the host-authoritative generation below.
	if (mApp && mApp->mBoard && !mApp->mSeedChooserScreen)
		mApp->ShowSeedChooserScreen();
	if (!mApp->mSeedChooserScreen)
	{
		NetTrace("CHOOSER_OPEN rejected after widget creation: screen is unavailable");
		return;
	}
	const int anActualFixedCount = mApp->mSeedChooserScreen->CoopCountGlobalFixedPicks();
	if (anActualFixedCount != anExpectedFixedCount)
	{
		NetTrace("CHOOSER_OPEN rejected after widget creation: fixed cards actual=%d expected=%d",
			anActualFixedCount, anExpectedFixedCount);
		return;
	}

	// A valid OPEN is the ordered generation barrier.  Discard only chooser
	// traffic belonging to the preceding generation; later STATE/START packets
	// on this TCP stream remain ordered behind this message.
	mRemotePicks.clear();
	mRemoteUnpicks.clear();
	mRemoteChooserStates.clear();
	mPendingChooserStarts.clear();
	mChooserOpen = true;
	mChooserBankCount = aBankCount;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		mChooserPickQuota[i] = aQuotas[i];
	mChooserPicksPerPlayer = aQuotas[mMySlot];
	if (mApp->mSeedChooserScreen)
	{
		mApp->mSeedChooserScreen->mCoopChooserActive = true;
		mApp->mSeedChooserScreen->mCoopPicksPerPlayer = mChooserPicksPerPlayer;
		mApp->mSeedChooserScreen->mVisible = true;
		mApp->mSeedChooserScreen->CoopRefreshStartButton();
		if (mApp->mWidgetManager)
			mApp->mWidgetManager->MarkAllDirty();
	}

	const bool aLocalChooserLevel = mApp && mApp->mBoard && mApp->mBoard->ChooseSeedsOnCurrentLevel();
	const bool aSeedChoosing = mApp && mApp->mBoard && mApp->mBoard->mCutScene &&
		mApp->mBoard->mCutScene->mSeedChoosing;
	NetTrace("CHOOSER_OPEN applied bank=%u fixed=%d mutable=%d quotas=[%u,%u,%u,%u] slot=%u localQuota=%d localChooserLevel=%d screen=%p visible=%d mouse=%d seedChoosing=%d focus=%p",
		aBankCount, anExpectedFixedCount, aBankCount - anExpectedFixedCount,
		aQuotas[0], aQuotas[1], aQuotas[2], aQuotas[3], mMySlot,
		mChooserPicksPerPlayer, aLocalChooserLevel ? 1 : 0,
		mApp ? (void*)mApp->mSeedChooserScreen : nullptr,
		mApp && mApp->mSeedChooserScreen && mApp->mSeedChooserScreen->mVisible ? 1 : 0,
		mApp && mApp->mSeedChooserScreen && mApp->mSeedChooserScreen->mMouseVisible ? 1 : 0,
		aSeedChoosing ? 1 : 0,
		mApp && mApp->mWidgetManager ? (void*)mApp->mWidgetManager->mFocusWidget : nullptr);
}

void NetManager::OnChooserState(NetBuffer& thePayload)
{
	mRemoteChooserStates.push_back(std::move(thePayload));
	if (mRemoteChooserStates.size() > 32)
		mRemoteChooserStates.pop_front();
}

void NetManager::OnChooserStart(NetBuffer& thePayload)
{
	mPendingChooserStarts.push_back(std::move(thePayload));
}

// -------------------------------------------------------------------------
// Send helpers
// -------------------------------------------------------------------------
void NetManager::SendLobbyState()
{
	if (mMode != MODE_HOST)
		return;
	NetBuffer aState;
	EncodeLobbyState(aState);
	BroadcastMessage(NETMSG_LOBBY_STATE, aState);
}

void NetManager::SetMyTeam(uint8_t theTeam)
{
	if (theTeam > NETTEAM_ZOMBIES || (mApp && mApp->mBoard))
		return;
	if (mMode == MODE_HOST)
	{
		mPlayers[0].mTeam = theTeam;
		SendLobbyState();
	}
	else if (mMode == MODE_CLIENT && mHostConn)
	{
		NetBuffer aMsg;
		aMsg.WriteU8(theTeam);
		mHostConn->SendMessage(NETMSG_TEAM_SELECT, aMsg);
	}
}

uint8_t NetManager::GetMyTeam() const
{
	return mPlayers[mMySlot].mTeam;
}

int NetManager::GetTeamCount(uint8_t theTeam) const
{
	int aCount = 0;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		if (mPlayers[i].mActive && mPlayers[i].mTeam == theTeam)
			aCount++;
	return aCount;
}

void NetManager::RequestChooserStateEcho(const char* theReason, int theSlot, int theSeedType)
{
	if (mMode != MODE_HOST || !mChooserOpen)
		return;
	mChooserStateEchoRequested.store(true, std::memory_order_release);
	NetTrace("chooser authoritative echo requested reason=%s slot=%d seed=%d",
		theReason ? theReason : "unknown", theSlot, theSeedType);
}

bool NetManager::ConsumeChooserStateEchoRequest()
{
	return mChooserStateEchoRequested.exchange(false, std::memory_order_acq_rel);
}

bool NetManager::ComputeChooserQuotas(int theBankCount, int theFixedHostCount,
	bool thePvP, int theQuotas[NET_MAX_PLAYERS], int* theEligibleCount) const
{
	if (!theQuotas)
		return false;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		theQuotas[i] = 0;
	if (theEligibleCount)
		*theEligibleCount = 0;
	if (theBankCount < 0 || theBankCount > SEEDBANK_CAPACITY ||
		theFixedHostCount < 0 || theFixedHostCount > theBankCount)
		return false;

	int anEligibleCount = 0;
	for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
	{
		const NetPlayerInfo& aPlayer = mPlayers[aSlot];
		if (aPlayer.mActive && (!thePvP || aPlayer.mTeam == NETTEAM_PLANTS))
			anEligibleCount++;
	}
	if (theEligibleCount)
		*theEligibleCount = anEligibleCount;
	if (anEligibleCount <= 0)
		return false;

	// Fixed Dave/challenge cards are one global preselection.  Divide only the
	// mutable capacity, then account for those fixed cards exactly once in the
	// host's quota (the host serializes them in its slot).
	const int aMutableCount = theBankCount - theFixedHostCount;
	const int aBaseQuota = aMutableCount / anEligibleCount;
	int aRemainder = aMutableCount % anEligibleCount;
	for (int aSlot = 0; aSlot < NET_MAX_PLAYERS; aSlot++)
	{
		const NetPlayerInfo& aPlayer = mPlayers[aSlot];
		if (!aPlayer.mActive || (thePvP && aPlayer.mTeam != NETTEAM_PLANTS))
			continue;
		theQuotas[aSlot] = aBaseQuota;
		if (aRemainder > 0)
		{
			theQuotas[aSlot]++;
			aRemainder--;
		}
	}

	if (theFixedHostCount > 0)
	{
		const NetPlayerInfo& aHost = mPlayers[0];
		if (!aHost.mActive || (thePvP && aHost.mTeam != NETTEAM_PLANTS))
			return false;
		theQuotas[0] += theFixedHostCount;
	}

	int aTotal = 0;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		if (theQuotas[i] < 0 || theQuotas[i] > SEEDBANK_CAPACITY)
			return false;
		aTotal += theQuotas[i];
	}
	return aTotal == theBankCount;
}

void NetManager::BroadcastMessage(uint8_t theMsgType, const NetBuffer& thePayload)
{
	if (mMode != MODE_HOST)
		return;
	if (theMsgType != NETMSG_CURSOR && theMsgType != NETMSG_SNAPSHOT && theMsgType != NETMSG_DAVE_STATE &&
		theMsgType != NETMSG_PING && theMsgType != NETMSG_PONG)
		NetTrace("TX broadcast type=%s(%u) bytes=%zu", NetMessageName(theMsgType), theMsgType, thePayload.Size());
	for (int i = 1; i < NET_MAX_PLAYERS; i++)
		if (mSessions[i].mAssigned)
			mSessions[i].mConn.SendMessage(theMsgType, thePayload);
}

void NetManager::BroadcastSceneCmd(uint8_t theCmd, int theParam)
{
	NetBuffer aMsg;
	aMsg.WriteU8(theCmd);
	aMsg.WriteI32(theParam);
	BroadcastMessage(NETMSG_SCENE_CMD, aMsg);
}

uint32_t NetManager::BroadcastAdventureStartFx()
{
	if (mMode != MODE_HOST)
	{
		NetTrace("Adventure start FX broadcast rejected: manager is not hosting");
		return 0;
	}
	uint32_t aGeneration = ++mAdventureStartFxGeneration;
	if (aGeneration == 0)
		aGeneration = ++mAdventureStartFxGeneration;
	NetBuffer aMsg;
	aMsg.WriteU8(SCENECMD_ADVENTURE_START_FX);
	aMsg.WriteU32(aGeneration);
	if (aMsg.Size() != sizeof(uint8_t) + sizeof(uint32_t))
	{
		NetTrace("Adventure start FX broadcast rejected: encoded size=%zu", aMsg.Size());
		return 0;
	}

	BroadcastMessage(NETMSG_SCENE_CMD, aMsg);
	NetTrace("Adventure start FX broadcast generation=%u bytes=%zu", aGeneration, aMsg.Size());
	return aGeneration;
}

void NetManager::BroadcastStartGame(int theGameMode, int theLevel, uint32_t theBoardSeed)
{
	// Ordered immediately before START_GAME.  This invalidates every click from
	// the previous board even if it was already buffered by TCP.
	AdvanceInputPhase(NETINPUT_INTRO, "START_GAME");
	NetTrace("start game broadcast mode=%d level=%d players=%d seed=%u tdPathCells=%zu", theGameMode, theLevel, GetPlayerCount(), theBoardSeed, mStartTDPath.size());
	NetBuffer aMsg;
	aMsg.WriteU8((uint8_t)theGameMode);
	aMsg.WriteI32(theLevel);
	aMsg.WriteU8((uint8_t)GetPlayerCount());
	aMsg.WriteU32(theBoardSeed);
	// TD path
	aMsg.WriteU8((uint8_t)mStartTDPath.size());
	for (uint8_t aCell : mStartTDPath)
		aMsg.WriteU8(aCell);
	BroadcastMessage(NETMSG_START_GAME, aMsg);
}

void NetManager::SendStoreAction(const NetBuffer& thePayload)
{
	if (mMode != MODE_CLIENT || !mHostConn)
		return;
	mHostConn->SendMessage(NETMSG_STORE_ACTION, thePayload);
}

void NetManager::OnStoreAction(NetSession* theSession, NetBuffer& thePayload)
{
	if (!mApp || !mApp->NetIsHost())
		return;
	uint8_t anItem = thePayload.ReadU8Or();
	StoreScreen* aStore = (StoreScreen*)mApp->GetDialog(Dialogs::DIALOG_STORE);
	if (aStore)
		aStore->NetApplyPurchase((StoreItem)anItem);
}

void NetManager::SendInput(bool theDown, int theClickCount, int theX, int theY)
{
	SendMouseInputAction(theDown ? NETINPUT_MOUSE_DOWN : NETINPUT_MOUSE_UP,
		theClickCount, theX, theY);
}

void NetManager::SendMouseMove(int theDragButton, int theX, int theY)
{
	const uint32_t aNow = GetTickCount();
	if (aNow - mLastInputMoveTick < 1000U / NET_INPUT_MOVE_HZ)
		return;
	mLastInputMoveTick = aNow;
	SendMouseInputAction(NETINPUT_MOUSE_MOVE, theDragButton, theX, theY);
}

void NetManager::SendHeatWaveShoutKey(bool theDown)
{
	Board* aBoard = mApp ? mApp->mBoard : nullptr;
	const bool aBaseContext = mMode == MODE_CLIENT && mHostConn && mClientConnected &&
		mInputPhase == NETINPUT_PLAYING && mInputEpoch != 0 && mApp && aBoard &&
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_HEAT_WAVE &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING && !mApp->mSeedChooserScreen;
	if (!aBaseContext || (theDown && (aBoard->mPaused || mRemotePaused ||
		mApp->GetDialogCount() > 0)) || theDown == mClientHeatWaveQDown)
		return;

	const uint32_t aSequence = mNextInputSequence;
	if (aSequence == 0)
	{
		NetTrace("key send blocked reason=sequence_exhausted epoch=%u", mInputEpoch);
		return;
	}

	NetBuffer aMsg;
	aMsg.WriteU32(mInputEpoch);
	aMsg.WriteU32(aSequence);
	aMsg.WriteU16(NET_HEAT_WAVE_Q_WIRE);
	aMsg.WriteU8(theDown ? 1 : 0);
	mHostConn->SendMessage(NETMSG_INPUT_KEY, aMsg);
	mNextInputSequence++;
	mClientHeatWaveQDown = theDown;
	NetTrace("key send epoch=%u seq=%u key=Q down=%d", mInputEpoch,
		aSequence, theDown ? 1 : 0);
}

void NetManager::SendMouseInputAction(uint8_t theAction, int theButton, int theX, int theY)
{
	Board* aBoard = mApp ? mApp->mBoard : nullptr;
	const bool aButtonValid = theAction == NETINPUT_MOUSE_MOVE ?
		(theButton == 0 || NetValidMouseButton(theButton)) : NetValidMouseButton(theButton);
	if (mMode != MODE_CLIENT || !mHostConn || !mClientConnected ||
		mInputPhase != NETINPUT_PLAYING || mInputEpoch == 0 || !aBoard ||
		!mApp || mApp->mGameScene != GameScenes::SCENE_PLAYING ||
		mApp->mSeedChooserScreen || mApp->GetDialogCount() > 0 ||
		aBoard->mPaused || mRemotePaused || theAction > NETINPUT_MOUSE_MOVE || !aButtonValid)
	{
		NetTrace("mouse send blocked action=%u button=%d epoch=%u phase=%u board=%p scene=%d chooser=%p dialogs=%d paused=%d remotePaused=%d",
			theAction, theButton, mInputEpoch, (unsigned)mInputPhase,
			(void*)aBoard, mApp ? (int)mApp->mGameScene : -1,
			mApp ? (void*)mApp->mSeedChooserScreen : nullptr,
			mApp ? mApp->GetDialogCount() : -1, aBoard && aBoard->mPaused ? 1 : 0,
			mRemotePaused ? 1 : 0);
		return;
	}
	const int aCanonicalX = NetCanonicalX(mApp, theX);
	const int aCanonicalY = NetCanonicalY(mApp, theY);
	if (aCanonicalX < -32768 || aCanonicalX > 32767 ||
		aCanonicalY < -32768 || aCanonicalY > 32767)
	{
		NetTrace("mouse send blocked reason=canonical_range local=(%d,%d) canonical=(%d,%d)",
			theX, theY, aCanonicalX, aCanonicalY);
		return;
	}

	int16_t aSeedIndex = -1;
	uint16_t aSeedPacketId = 0;
	uint8_t aSeedType = 255;
	uint8_t anImitater = 255;
	uint8_t anOwner = 255;
	if (theAction == NETINPUT_MOUSE_DOWN)
	{
		const int aHitIndex = NetSeedPacketAt(aBoard, aCanonicalX, aCanonicalY);
		if (aHitIndex >= 0 && aHitIndex < aBoard->mSeedBank->mNumPackets &&
			aHitIndex < SEEDBANK_CAPACITY)
		{
			SeedPacket& aPacket = aBoard->mSeedBank->mSeedPackets[aHitIndex];
			aSeedIndex = (int16_t)aHitIndex;
			aSeedPacketId = aPacket.mNetId;
			aSeedType = (uint8_t)aPacket.mPacketType;
			anImitater = (uint8_t)aPacket.mImitaterType;
			anOwner = aPacket.mNetOwnerSlot;
		}
	}

	uint32_t aSequence = mNextInputSequence;
	if (aSequence == 0)
	{
		NetTrace("mouse send blocked reason=sequence_exhausted epoch=%u", mInputEpoch);
		return;
	}
	NetTrace("mouse send epoch=%u seq=%u action=%u button=%d local=(%d,%d) canonical=(%d,%d) seed=(%d,%u,%u,%u,%u)",
		mInputEpoch, aSequence, theAction, theButton, theX, theY, aCanonicalX, aCanonicalY,
		aSeedIndex, aSeedPacketId, aSeedType, anImitater, anOwner);
	NetBuffer aMsg;
	aMsg.WriteU32(mInputEpoch);
	aMsg.WriteU32(aSequence);
	aMsg.WriteU8(theAction);
	aMsg.WriteU8((uint8_t)theButton);
	aMsg.WriteI16((int16_t)aCanonicalX);
	aMsg.WriteI16((int16_t)aCanonicalY);
	aMsg.WriteI16(aSeedIndex);
	aMsg.WriteU16(aSeedPacketId);
	aMsg.WriteU8(aSeedType);
	aMsg.WriteU8(anImitater);
	aMsg.WriteU8(anOwner);
	mHostConn->SendMessage(NETMSG_INPUT_MOUSE, aMsg);
	mNextInputSequence++;
}

void NetManager::SendSeedPick(bool theUnpick, int theSeedType, int theImitater)
{
	if (mMode != MODE_CLIENT || !mHostConn)
		return;
	NetBuffer aMsg;
	aMsg.WriteU8((uint8_t)theSeedType);
	if (!theUnpick)
		aMsg.WriteU8((uint8_t)theImitater);
	mHostConn->SendMessage(theUnpick ? NETMSG_SEED_UNPICK : NETMSG_SEED_PICK, aMsg);
}

void NetManager::SendCursor(int theCursorType, int theHeldSeed, int theHeldImitater, uint8_t theFlags, int theX, int theY)
{
	uint32_t aNow = GetTickCount();
	static uint32_t sLastCursorSend = 0;
	if (aNow - sLastCursorSend < 1000 / NET_CURSOR_HZ)
		return;
	sLastCursorSend = aNow;

	NetBuffer aMsg;
	aMsg.WriteU8((uint8_t)mMySlot);
	aMsg.WriteI16((int16_t)NetCanonicalX(mApp, theX));
	aMsg.WriteI16((int16_t)NetCanonicalY(mApp, theY));
	aMsg.WriteU8((uint8_t)theCursorType);
	aMsg.WriteU8((uint8_t)theHeldSeed);
	aMsg.WriteU8((uint8_t)theHeldImitater);
	aMsg.WriteU8(theFlags);

	if (mMode == MODE_HOST)
		BroadcastMessage(NETMSG_CURSOR, aMsg);
	else if (mHostConn)
		mHostConn->SendMessage(NETMSG_CURSOR, aMsg);
}

void NetManager::SendPauseState(bool thePaused)
{
	NetBuffer aMsg;
	aMsg.WriteU8(thePaused ? 1 : 0);
	BroadcastMessage(NETMSG_PAUSE, aMsg);
}

void NetManager::SendBoardEvent(const NetBuffer& thePayload, uint8_t theEventType)
{
	NetBuffer aMsg;
	aMsg.WriteU8(theEventType);
	aMsg.WriteBytes(thePayload.Data(), thePayload.Size());
	BroadcastMessage(NETMSG_BOARD_EVENT, aMsg);
}

void NetManager::SendSnapshot(const NetBuffer& thePayload)
{
	BroadcastMessage(NETMSG_SNAPSHOT, thePayload);
	if (mApp && mApp->CoopSmokeActive())
		mApp->CoopSmokeOnSnapshotTx(NetFnv1a32(thePayload.Data(), thePayload.Size()),
			thePayload.Size());
}

uint64_t NetManager::GetBytesSent() const
{
	if (mMode == MODE_HOST)
	{
		uint64_t aTotal = 0;
		for (int i = 1; i < NET_MAX_PLAYERS; i++)
			if (mSessions[i].mAssigned)
				aTotal += mSessions[i].mConn.GetBytesSent();
		return aTotal;
	}
	return mHostConn ? mHostConn->GetBytesSent() : 0;
}

uint64_t NetManager::GetBytesRecv() const
{
	if (mMode == MODE_HOST)
	{
		uint64_t aTotal = 0;
		for (int i = 1; i < NET_MAX_PLAYERS; i++)
			if (mSessions[i].mAssigned)
				aTotal += mSessions[i].mConn.GetBytesRecv();
		return aTotal;
	}
	return mHostConn ? mHostConn->GetBytesRecv() : 0;
}

} // namespace LawnNet
