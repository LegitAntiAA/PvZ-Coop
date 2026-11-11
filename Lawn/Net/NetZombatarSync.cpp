#include "NetManager.h"
#include "NetZombatar.h"

namespace LawnNet
{
const NetZombatarAppearance& NetManager::GetPlayerZombatar(int theSlot) const
{
	static const NetZombatarAppearance anAbsent;
	return theSlot >= 0 && theSlot < NET_MAX_PLAYERS ? mPlayerZombatars[theSlot] : anAbsent;
}

const NetZombatarAppearance& NetManager::GetMyZombatar() const
{
	return mLocalZombatar;
}

void NetManager::SetMyZombatar(const NetZombatarAppearance& theAppearance)
{
	if (!NetValidateZombatar(theAppearance) || theAppearance == mLocalZombatar)
		return;
	mLocalZombatar = theAppearance;
	mConfig.mZombatar = theAppearance;
	if (!NetSaveConfig(mConfig))
		NetTrace("Zombatar local configuration save failed");
	if (IsHosting())
	{
		mPlayerZombatars[mMySlot] = theAppearance;
		BroadcastZombatars();
	}
	else if (IsConnectedToHost() && mHostConn)
	{
		NetBuffer aWire;
		NetEncodeZombatar(theAppearance, aWire);
		mHostConn->SendMessage(NETMSG_ZOMBATAR, aWire);
	}
}

void NetManager::OnZombatar(NetSession* theSession, NetBuffer& thePayload)
{
	if (IsHosting())
	{
		if (!theSession || !theSession->mAssigned || theSession->mSlot == 0 ||
			theSession->mSlot >= NET_MAX_PLAYERS || !mPlayers[theSession->mSlot].mActive ||
			thePayload.Remaining() != NET_ZOMBATAR_WIRE_BYTES)
			return;
		NetZombatarAppearance anAppearance;
		if (!NetDecodeZombatar(thePayload, anAppearance))
		{
			NetTrace("Zombatar request rejected slot=%u", theSession->mSlot);
			return;
		}
		if (mPlayerZombatars[theSession->mSlot] != anAppearance)
		{
			mPlayerZombatars[theSession->mSlot] = anAppearance;
			BroadcastZombatars();
		}
	}
	else if (IsClient())
	{
		if (thePayload.Remaining() != NET_ZOMBATAR_WIRE_BYTES + 1)
			return;
		const uint8_t aSlot = thePayload.ReadU8Or();
		NetZombatarAppearance anAppearance;
		if (aSlot >= NET_MAX_PLAYERS || !NetDecodeZombatar(thePayload, anAppearance))
		{
			NetTrace("Zombatar state rejected slot=%u", aSlot);
			return;
		}
		mPlayerZombatars[aSlot] = anAppearance;
	}
}

void NetManager::BroadcastZombatars()
{
	if (!IsHosting())
		return;
	mPlayerZombatars[mMySlot] = mLocalZombatar;
	for (int i = 0; i < NET_MAX_PLAYERS; ++i)
	{
		if (!mPlayers[i].mActive)
			mPlayerZombatars[i] = NetZombatarAppearance();
		NetBuffer aWire;
		aWire.WriteU8((uint8_t)i);
		if (NetEncodeZombatar(mPlayerZombatars[i], aWire))
			BroadcastMessage(NETMSG_ZOMBATAR, aWire);
	}
}
}
