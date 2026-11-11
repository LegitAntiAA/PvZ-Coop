#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetOnline.h"
#include <cstdlib>

bool LawnApp::CoopSmokeUpdateOnline()
{
	const char* anEndpoint = getenv("PVZ_COOP_SMOKE_ONLINE_ENDPOINT");
	if (!anEndpoint || !*anEndpoint) return true;
	if (!mNetManager || !mCoopSmokeLifecycleStarted) return false;
	LawnNet::NetOnline* anOnline = mNetManager->GetOnline();
	if (!anOnline) { CoopSmokeFail("online smoke has no directory client"); return false; }
	const std::string aStatus = anOnline->GetStatus();
	if (aStatus.rfind("Online:", 0) == 0 || aStatus.rfind("Signaling connection failed", 0) == 0 ||
		aStatus.rfind("Signaling disconnected", 0) == 0 || aStatus.rfind("Direct connection failed", 0) == 0)
	{
		CoopSmokeFail(aStatus.c_str()); return false;
	}
	if (!anOnline->IsConnected()) return false;

	// Each isolated smoke process owns exactly one LawnApp and one run ID.
	// No normal game UI or transport mode is changed by this opt-in controller.
	static bool aRequested = false;
	static bool aAdoptionLogged = false;
	static uint32_t aLastRefresh = 0;
	const char* aRun = getenv("PVZ_COOP_SMOKE_RUN_ID");
	if (!aRun || strlen(aRun) != 32) { CoopSmokeFail("online smoke requires a bounded run ID"); return false; }
	const std::string aRoomName = std::string("smoke-") + aRun;
	if (mCoopSmokeManifest.IsHost())
	{
		if (!aRequested)
		{
			if (!mNetManager->CreateOnlineRoom(aRoomName, "test", 2))
				{ CoopSmokeFail("online smoke failed to request its room"); return false; }
			aRequested = true;
		}
		if (!mNetManager->IsHosting() || mNetManager->GetPlayerCount() != 2) return false;
	}
	else
	{
		if (!aRequested)
		{
			const uint32_t aNow = GetTickCount();
			if (aLastRefresh == 0 || aNow - aLastRefresh >= 500U)
			{
				anOnline->RefreshRooms(); aLastRefresh = aNow;
			}
			for (const auto& aRoom : anOnline->GetRooms())
			{
				if (aRoom.mName != aRoomName) continue;
				if (!mNetManager->JoinOnlineRoom(aRoom.mId, "test"))
					{ CoopSmokeFail("online smoke failed to request joining its room"); return false; }
				aRequested = true;
				break;
			}
		}
		if (!mNetManager->IsConnectedToHost()) return false;
	}
	if (!aAdoptionLogged)
	{
		aAdoptionLogged = true;
		LawnNet::NetTrace("SMOKE_ONLINE_CHANNEL role=%s adopted=1 transport=webrtc",
			mCoopSmokeManifest.IsHost() ? "host" : "client");
	}
	return true;
}
