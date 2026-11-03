#include "NetManager.h"
#include "../LawnApp.h"
#include "../../SexyAppFramework/Common.h"

namespace LawnNet
{

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
	mSnapshotHz(NET_SNAPSHOT_HZ_DEFAULT),
	mAdaptiveGoodStreak(0),
	mLastPingTick(0),
	mLastStatsTick(0),
	mChooserOpen(false),
	mChooserPicksPerPlayer(0),
	mChooserBankCount(0)
{
	mConnectStatus[0] = 0;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		mPlayers[i].Reset();
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
	NetStartupWSA();
}

void NetManager::Shutdown()
{
	Disconnect();
	NetShutdownWSA();
}

// -------------------------------------------------------------------------
// Session control
// -------------------------------------------------------------------------
bool NetManager::StartHost()
{
	Disconnect();

	if (!NetStartupWSA())
		return false;

	SOCKET aListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (aListener == INVALID_SOCKET)
		return false;

	BOOL aReuse = TRUE;
	setsockopt(aListener, SOL_SOCKET, SO_REUSEADDR, (const char*)&aReuse, sizeof(BOOL));

	sockaddr_in anAddr;
	memset(&anAddr, 0, sizeof(anAddr));
	anAddr.sin_family = AF_INET;
	anAddr.sin_port = htons(NET_DEFAULT_PORT);
	anAddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(aListener, (sockaddr*)&anAddr, sizeof(anAddr)) == SOCKET_ERROR ||
		listen(aListener, 4) == SOCKET_ERROR)
	{
		closesocket(aListener);
		return false;
	}

	mListenerSocket = aListener;
	mMode = MODE_HOST;
	mMySlot = 0;

	mPlayers[0].Reset();
	mPlayers[0].mActive = true;
	mPlayers[0].mSlot = 0;
	strncpy_s(mPlayers[0].mNick, mConfig.mNick, NET_MAX_NICK);

	mAcceptRunning = true;
	mAcceptThread = std::thread(&NetManager::AcceptThreadProc, this);

	strcpy_s(mConnectStatus, "Hosting on port ");
	_itoa_s(NET_DEFAULT_PORT, mConnectStatus + strlen(mConnectStatus), 32, 10);
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
		// queue for the game thread to adopt
		std::lock_guard<std::mutex> aLock(mPendingConnMutex);
		mPendingConns.push_back(aClient);
	}
}

bool NetManager::StartClient(const char* theIP)
{
	Disconnect();
	if (!NetStartupWSA())
		return false;

	mMode = MODE_CLIENT;
	mConnecting = true;
	mClientConnected = false;
	strcpy_s(mConnectStatus, "Connecting...");

	char anIP[64];
	strncpy_s(anIP, theIP, 63);
	anIP[63] = 0;

	mConnectThread = std::thread([this, anIP]()
	{
		SOCKET aSock = NetConnectTo(anIP, NET_DEFAULT_PORT, 5000);
		if (aSock == INVALID_SOCKET)
		{
			strcpy_s(mConnectStatus, "Failed to connect (check IP + Radmin VPN)");
			mConnecting = false;
			return;
		}

		// hand to the manager under lock
		{
			std::lock_guard<std::mutex> aLock(mPendingConnMutex);
			mPendingConns.push_back(aSock);
		}
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
	mRemotePaused = false;
	mRemotePicks.clear();
	mRemoteUnpicks.clear();
	mPendingChooserStarts.clear();
	mPendingBoardMsgs.clear();
	mPendingInputs.clear();
	mChooserOpen = false;
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		mRemoteCursors[i].Clear();
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		mPlayers[i].Reset();
}

void NetManager::DropSession(int theSlot, bool theNotify)
{
	NetSession& aSession = mSessions[theSlot];
	if (!aSession.mAssigned)
		return;
	aSession.mConn.Disconnect();
	aSession.mAssigned = false;
	mPlayers[theSlot].mActive = false;
	if (theNotify)
		SendLobbyState();
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
		for (int i = 1; i < NET_MAX_PLAYERS; i++)
		{
			if (mSessions[i].mAssigned)
			{
				mSessions[i].mConn.PumpMessages();
				if (!mSessions[i].mConn.IsConnected())
				{
					DropSession(i, true);
					// if we were in a game, pause so the host notices
					if (mApp && mApp->mBoard)
						mApp->mBoard->mPaused = true;
				}
			}
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
	}
	else if (mMode == MODE_CLIENT && mHostConn && mClientConnected)
	{
		uint32_t aSeq = mHostConn->NextPingSeq();
		NetBuffer aPing;
		aPing.WriteU32(aSeq);
		mHostConn->SendMessage(NETMSG_PING, aPing);
	}
}

// -------------------------------------------------------------------------
// Host-side message handling
// -------------------------------------------------------------------------
void NetManager::HandleMessage(NetSession* theSession, uint8_t theMsgType, NetBuffer& thePayload)
{
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
	{
		// key events are unused for now (host-only pause/menu), kept for future
		break;
	}
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
	if (aVersion != NET_PROTOCOL_VERSION)
	{
		theSession->mConn.SendMessage(NETMSG_VERSION_MISMATCH);
		DropSession(theSession->mSlot, false);
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

	NetBuffer aWelcome;
	aWelcome.WriteU8(theSession->mSlot);
	aWelcome.WriteString(mPlayers[0].mNick);
	theSession->mConn.SendMessage(NETMSG_WELCOME, aWelcome);

	SendLobbyState();
}

void NetManager::OnClientInput(NetSession* theSession, NetBuffer& thePayload)
{
	NetRemoteInput anInput;
	anInput.mDown = thePayload.ReadU8Or() != 0;
	anInput.mClickCount = (int8_t)thePayload.ReadU8Or();
	anInput.mX = thePayload.ReadI16Or();
	anInput.mY = thePayload.ReadI16Or();
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	mPendingInputs.push_back(anInput);
}

void NetManager::OnClientCursor(NetSession* theSession, NetBuffer& thePayload)
{
	uint8_t aSlot = thePayload.ReadU8Or();
	if (aSlot >= NET_MAX_PLAYERS)
		return;
	NetRemoteCursor& aCursor = mRemoteCursors[aSlot];
	aCursor.mValid = true;
	aCursor.mX = thePayload.ReadI16Or();
	aCursor.mY = thePayload.ReadI16Or();
	aCursor.mCursorType = thePayload.ReadU8Or();
	aCursor.mHeldSeed = thePayload.ReadU8Or();
	aCursor.mHeldImitater = thePayload.ReadU8Or();
	aCursor.mFlags = thePayload.ReadU8Or();
}

void NetManager::OnClientSeedPick(NetSession* theSession, NetBuffer& thePayload, bool theUnpick)
{
	// forward to the app (seed chooser screen picks it up on the host)
	ChooserPick aPick;
	aPick.mSeedType = thePayload.ReadU8Or();
	aPick.mImitater = theUnpick ? 0xFF : thePayload.ReadU8Or();
	if (theUnpick)
		mRemoteUnpicks.push_back(aPick);
	else
		mRemotePicks.push_back(aPick);
}

void NetManager::OnClientTeamSelect(NetSession* theSession, NetBuffer& thePayload)
{
	uint8_t aTeam = thePayload.ReadU8Or();
	if (aTeam > NETTEAM_ZOMBIES)
		return;
	mPlayers[theSession->mSlot].mTeam = aTeam;
	SendLobbyState();
}

void NetManager::OnClientStats(NetSession* theSession, NetBuffer& thePayload)
{
	// ping ms (already measured), snapshot apply time
	uint16_t aPing = thePayload.ReadU16Or();
	uint16_t anApplyMs = thePayload.ReadU16Or();
	mPlayers[theSession->mSlot].mPing = aPing;

	if (aPing < 60 && anApplyMs < 8)
		mAdaptiveGoodStreak++;
	else if (aPing > 150 || anApplyMs > 20)
		mAdaptiveGoodStreak = 0;
}

void NetManager::CheckAdaptiveRate()
{
	if (mAdaptiveGoodStreak >= 3 && mSnapshotHz < NET_SNAPSHOT_HZ_MAX)
	{
		SetSnapshotHz(mSnapshotHz + 1);
		mAdaptiveGoodStreak = 0;
	}
	else if (mAdaptiveGoodStreak == 0 && mSnapshotHz > NET_SNAPSHOT_HZ_MIN)
	{
		// decay rate only after repeated bad reports (streak stays 0)
		static int aBadTicks = 0;
		aBadTicks++;
		if (aBadTicks >= 6)
		{
			SetSnapshotHz(mSnapshotHz - 1);
			aBadTicks = 0;
		}
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
		if (mApp)
			mApp->NetOnBackToMenu();
		break;
	case NETMSG_SCENE_CMD:
		if (mApp)
			mApp->NetOnSceneCmd(thePayload);
		break;
	case NETMSG_PROFILE_SYNC:
		if (mApp)
			mApp->NetOnProfileSync(thePayload);
		break;
	default:
		break;
	}
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
	// hand over to the app: it will create the board in client mode
	if (mApp)
		mApp->NetOnStartGame(thePayload);
}

void NetManager::OnSnapshot(NetBuffer& thePayload)
{
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	PendingMsg aMsg;
	aMsg.mKind = 1;
	aMsg.mData = std::move(thePayload);
	mPendingBoardMsgs.push_back(std::move(aMsg));
	if (mPendingBoardMsgs.size() > 8)
		mPendingBoardMsgs.pop_front(); // never fall behind
}

void NetManager::OnBoardEvent(NetBuffer& thePayload)
{
	std::lock_guard<std::mutex> aLock(mSessionsMutex);
	PendingMsg aMsg;
	aMsg.mKind = 0;
	aMsg.mData = std::move(thePayload);
	mPendingBoardMsgs.push_back(std::move(aMsg));
	if (mPendingBoardMsgs.size() > 128)
		mPendingBoardMsgs.pop_front();
}

void NetManager::OnCursor(NetBuffer& thePayload)
{
	uint8_t aSlot = thePayload.ReadU8Or();
	if (aSlot >= NET_MAX_PLAYERS || aSlot == mMySlot)
		return;
	NetRemoteCursor& aCursor = mRemoteCursors[aSlot];
	aCursor.mValid = true;
	aCursor.mX = thePayload.ReadI16Or();
	aCursor.mY = thePayload.ReadI16Or();
	aCursor.mCursorType = thePayload.ReadU8Or();
	aCursor.mHeldSeed = thePayload.ReadU8Or();
	aCursor.mHeldImitater = thePayload.ReadU8Or();
	aCursor.mFlags = thePayload.ReadU8Or();
}

void NetManager::OnChooserOpen(NetBuffer& thePayload)
{
	mChooserOpen = true;
	mChooserPicksPerPlayer = thePayload.ReadU8Or();
	mChooserBankCount = thePayload.ReadU8Or();
	mRemotePicks.clear();
	mRemoteUnpicks.clear();
}

void NetManager::OnChooserState(NetBuffer& thePayload)
{
	uint8_t aCount = thePayload.ReadU8Or();
	bool anUnpick = false;
	for (int i = 0; i < aCount; i++)
	{
		ChooserPick aPick;
		aPick.mSeedType = thePayload.ReadU8Or();
		aPick.mImitater = thePayload.ReadU8Or();
		if (aPick.mImitater == 0xFF)
			anUnpick = true;
		mRemotePicks.push_back(aPick);
	}
	(void)anUnpick;
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

void NetManager::BroadcastMessage(uint8_t theMsgType, const NetBuffer& thePayload)
{
	if (mMode != MODE_HOST)
		return;
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

void NetManager::BroadcastStartGame(int theGameMode, int theLevel)
{
	NetBuffer aMsg;
	aMsg.WriteU8((uint8_t)theGameMode);
	aMsg.WriteI32(theLevel);
	aMsg.WriteU8((uint8_t)GetPlayerCount());
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
		aMsg.WriteU8(mPlayers[i].mTeam);
	// TD path
	aMsg.WriteU8((uint8_t)mStartTDPath.size());
	for (uint8_t aCell : mStartTDPath)
		aMsg.WriteU8(aCell);
	BroadcastMessage(NETMSG_START_GAME, aMsg);
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
	aMsg.WriteI16((int16_t)theX);
	aMsg.WriteI16((int16_t)theY);
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
