#include "NetManager.h"
#include "NetBuildVersion.h"
#include "../../LawnApp.h"
#include "../Board.h"

namespace LawnNet
{
void NetManager::SendHello()
{
	if (!mHostConn) return;
	NetBuffer hello;
	hello.WriteU32(NET_PROTOCOL_VERSION);
	hello.WriteString(NET_MOD_BUILD_STRING);
	hello.WriteString(mConfig.mNick);
	mHostConn->SendMessage(NETMSG_HELLO, hello);
}

bool NetManager::ConnectOnlineDirectory(const std::string& theEndpoint)
{
	if (IsActive() || (mApp && mApp->mBoard)) return false;
	mConfig.mOnlineEndpoint = theEndpoint;
	NetSaveConfig(mConfig);
	return mOnline->Connect(theEndpoint, NET_MOD_BUILD_STRING, mConfig.mNick, mConfig.mStunServer);
}

bool NetManager::CreateOnlineRoom(const std::string& theName, const std::string& thePassword, int theMaxPlayers)
{
	if (IsActive() || (mApp && mApp->mBoard) || !mOnline->IsConnected()) return false;
	if (!mOnline->CreateRoom(theName, thePassword, theMaxPlayers)) return false;
	mOnlineIntent = 1;
	return true;
}

bool NetManager::JoinOnlineRoom(const std::string& theRoomId, const std::string& thePassword)
{
	if (IsActive() || (mApp && mApp->mBoard) || !mOnline->IsConnected()) return false;
	if (!mOnline->JoinRoom(theRoomId, thePassword)) return false;
	mOnlineIntent = 2;
	return true;
}

void NetManager::UpdateOnline()
{
	if (!mOnline) return;
	mOnline->Update();
	if (mOnlineIntent == 1 && mOnline->IsHostingRoom() && mMode == MODE_NONE)
	{
		mMode = MODE_HOST; mMySlot = 0;
		NetTraceOpen("host");
		mPlayers[0].Reset(); mPlayers[0].mActive = true;
		strcpy_s(mPlayers[0].mNick, mConfig.mNick);
		mPlayerZombatars[0] = mLocalZombatar;
		strcpy_s(mConnectStatus, "Online room created.");
		if (mApp) mApp->NetBroadcastProfile();
		NetTrace("WebRTC room hosting build=%s", NET_MOD_BUILD_STRING);
	}
	for (auto channel : mOnline->TakeOpenedChannels())
	{
		if (mOnlineIntent == 1 && mMode == MODE_HOST)
		{
			int slot = -1;
			for (int i = 1; i < NET_MAX_PLAYERS; ++i) if (!mSessions[i].mAssigned) { slot = i; break; }
			if (slot < 0) { channel->Close(); continue; }
			NetSession& session = mSessions[slot];
			if (!session.mConn.Adopt(channel)) continue;
			session.mSlot = (uint8_t)slot; session.mAssigned = true;
			session.mConn.SetMessageHandler([this,slot](uint8_t type, NetBuffer& data) { HandleMessage(&mSessions[slot],type,data); });
		}
		else if (mOnlineIntent == 2 && !mHostConn && mMode == MODE_NONE)
		{
			mMode = MODE_CLIENT;
			NetTraceOpen("client");
			mHostConn = new NetConnection();
			if (!mHostConn->Adopt(channel)) { delete mHostConn; mHostConn = nullptr; mMode = MODE_NONE; continue; }
			mHostConn->SetMessageHandler([this](uint8_t type,NetBuffer& data) { HandleMessageClient(type,data); });
			mClientConnected = true; mConnecting = false;
			strcpy_s(mConnectStatus, "Connected through WebRTC.");
			SendHello();
		}
		else channel->Close();
	}
	const bool playing = mMode == MODE_HOST && mApp && mApp->mBoard;
	if (mOnlineIntent == 1 && playing != mOnlinePlaying)
	{ mOnlinePlaying = playing; mOnline->SetPlaying(playing); }
}
}
