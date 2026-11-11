#include "../../Lawn/Net/NetOnline.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>

using namespace LawnNet;

int main(int argc, char** argv)
{
	const std::string aEndpoint = argc > 1 ? argv[1] : "http://127.0.0.1:8787";
	const std::string aRun = std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
	const std::string aBuild = "test-native-" + aRun;
	NetOnline aHost, aClient, aWrongBuild, aObserver;
	NetOnline* aPeers[] = { &aHost, &aClient, &aWrongBuild, &aObserver };
	int aPassed = 0, aFailed = 0;
	auto Check = [&](bool ok, const char* label) {
		std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); std::fflush(stdout);
		if (ok) ++aPassed; else ++aFailed;
	};
	auto ReadableError = [](const NetOnline& peer) {
		const std::string text = peer.GetLastErrorMessage();
		return !text.empty() && text.size() <= 512 && (uint8_t)text[0] >= 0xC0 &&
			text.find("://") == std::string::npos && text.find("stage=") == std::string::npos &&
			text.find("error=") == std::string::npos && text.find("candidate") == std::string::npos &&
			text.find("password-123") == std::string::npos;
	};
	auto Wait = [&](const std::function<bool()>& predicate, int seconds = 15) {
		const auto aUntil = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
		do {
			for (auto* aPeer : aPeers) aPeer->Update();
			if (predicate()) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} while (std::chrono::steady_clock::now() < aUntil);
		return false;
	};
	Check(!aHost.Connect("http://example.com", "test", "Host"), "reject insecure remote signaling");
	Check(aHost.GetLastErrorCode() == "insecure-endpoint" && ReadableError(aHost),
		"insecure signaling has a safe Russian reason");
	Check(!aHost.Connect("https://fixture-secret@example.com", "test", "Host") &&
		aHost.GetLastErrorCode() == "invalid-endpoint" && ReadableError(aHost) &&
		aHost.GetLastErrorMessage().find("fixture-secret") == std::string::npos,
		"invalid endpoint never echoes its credentials");
	Check(!aHost.Connect("https://example.com", "", "Host") &&
		aHost.GetLastErrorCode() == "invalid-settings" && ReadableError(aHost),
		"invalid online settings expose a bounded localized reason");
	Check(!aHost.IsConnecting() && !aHost.IsConnected(), "rejected directory attempt clears pending state");
	const bool aHostAccepted = aHost.Connect(aEndpoint, aBuild, "Host");
	Check(aHostAccepted && aHost.GetLastErrorCode().empty() && aHost.GetLastErrorMessage().empty(),
		"new directory attempt clears the previous error message");
	Check(aHostAccepted && aHost.IsConnecting() && !aHost.IsConnected(), "directory pending before registration is pumped");
	if (!aHostAccepted ||
		!aClient.Connect(aEndpoint, aBuild, "Client") ||
		!aWrongBuild.Connect(aEndpoint, aBuild + "-other", "Other") ||
		!aObserver.Connect(aEndpoint, aBuild, "Observer") ||
		!Wait([&] { return aHost.IsConnected() && aClient.IsConnected() && aWrongBuild.IsConnected() && aObserver.IsConnected(); }))
	{
		Check(false, "connect native WinHTTP signaling clients");
		for (auto* aPeer : aPeers) std::printf("STATUS %s\n", aPeer->GetStatus().c_str());
		return 1;
	}
	Check(true, "connect native WinHTTP signaling clients");
	Check(!aHost.IsConnecting() && !aClient.IsConnecting() && !aWrongBuild.IsConnecting() && !aObserver.IsConnecting(),
		"directory registration clears pending before peer ICE");
	const auto aIdleUntil = std::chrono::steady_clock::now() + std::chrono::seconds(8);
	const bool aIdle = Wait([&] {
		for (auto* aPeer : aPeers) if (!aPeer->IsConnected()) return true;
		return std::chrono::steady_clock::now() >= aIdleUntil;
	}, 10) && aHost.IsConnected() && aClient.IsConnected() && aWrongBuild.IsConnected() && aObserver.IsConnected();
	Check(aIdle, "directory stays registered across eight-second wait");
	if (!aIdle)
	{
		for (auto* aPeer : aPeers) std::printf("STATUS %s\n", aPeer->GetStatus().c_str());
		return 1;
	}
	bool aPendingJoin = aHost.CreateRoom("Pending join " + aRun, "", 2) &&
		Wait([&] { return !aHost.GetRoomId().empty(); });
	if (aPendingJoin)
	{
		aPendingJoin = aClient.JoinRoom(aHost.GetRoomId(), "");
		const auto aUntilJoined = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		// Keep the host's peer-joined event queued: the client has joined the
		// directory, but no offer or DataChannel can be produced yet.
		while (aPendingJoin && aClient.GetRoomId().empty() && std::chrono::steady_clock::now() < aUntilJoined)
		{
			aClient.Update(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		aPendingJoin = aPendingJoin && !aClient.GetRoomId().empty();
	}
	aHost.LeaveRoom();
	Check(aPendingJoin && Wait([&] { return aClient.GetLastErrorCode() == "direct-connection-failed"; }) &&
		aClient.GetRoomId().empty() && aClient.IsConnected() && ReadableError(aClient),
		"pending join host loss returns a retryable localized directory error");
	Check(aHost.CreateRoom("Native test " + aRun, "password-123", 2) && Wait([&] { return !aHost.GetRoomId().empty(); }), "create password room");
	const std::string aRoomId = aHost.GetRoomId();
	aClient.RefreshRooms(); aWrongBuild.RefreshRooms();
	Check(Wait([&] { return aClient.GetRooms().size() == 1; }), "same-build room discovery");
	Check(aWrongBuild.GetRooms().empty(), "hide other-build rooms");
	aWrongBuild.JoinRoom(aRoomId, "password-123");
	const bool aMismatch = Wait([&] { return aWrongBuild.GetLastErrorCode() == "version-mismatch"; });
	Check(aMismatch && ReadableError(aWrongBuild), "different-build rejection has a safe Russian reason");
	aWrongBuild.LeaveRoom();
	Check(aMismatch && aWrongBuild.GetLastErrorCode().empty() && aWrongBuild.GetLastErrorMessage().empty(),
		"reject direct different-build join and clear leave error");
	aObserver.JoinRoom(aRoomId, "wrong-password");
	Check(Wait([&] { return aObserver.GetLastErrorCode() == "wrong-password"; }), "native PBKDF2 structured wrong-password rejection");
	Check(ReadableError(aObserver), "wrong-password rejection shows a localized reason without password material");
	const bool aRetryQueued = aClient.JoinRoom(aRoomId, "password-123") && aClient.GetLastErrorCode().empty();
	std::shared_ptr<NetOnlineChannel> aHostChannel, aClientChannel;
	const bool aConnected = aRetryQueued && Wait([&] {
		for (auto& aChannel : aHost.TakeOpenedChannels()) aHostChannel = aChannel;
		for (auto& aChannel : aClient.TakeOpenedChannels()) aClientChannel = aChannel;
		return aHostChannel && aClientChannel && aHostChannel->IsOpen() && aClientChannel->IsOpen();
	}, 45);
	Check(aConnected, "real local ICE + DTLS + SCTP ordered DataChannel");
	Check(aConnected && aHost.GetLastErrorMessage().empty() && aClient.GetLastErrorMessage().empty(),
		"successful channel adoption clears connection errors");
	if (!aConnected)
	{
		std::printf("HOST %s CLIENT %s\n", aHost.GetStatus().c_str(), aClient.GetStatus().c_str());
		return 1;
	}
	std::vector<uint8_t> aMaximum(65538);
	for (size_t i = 0; i < aMaximum.size(); ++i) aMaximum[i] = (uint8_t)(i * 31);
	std::vector<uint8_t> aReceived;
	Check(aHostChannel->Send(aMaximum) && Wait([&] { return aClientChannel->Receive(aReceived); }) && aReceived == aMaximum,
		"maximum wire frame preserved byte-for-byte");
	bool aOrdered = true;
	for (int i = 0; i < 32; ++i) aOrdered = aOrdered && aClientChannel->Send({ (uint8_t)i, 0, 255 });
	for (int i = 0; i < 32; ++i)
		aOrdered = aOrdered && Wait([&] { return aHostChannel->Receive(aReceived); }) && aReceived == std::vector<uint8_t>({ (uint8_t)i, 0, 255 });
	Check(aOrdered, "32 ordered binary messages preserved");
	aHost.SetPlaying(true); aObserver.RefreshRooms();
	Check(Wait([&] { auto rooms = aObserver.GetRooms(); return rooms.size() == 1 && rooms[0].mPlaying && rooms[0].mPlayers == 2; }), "active-game room occupancy published");
	const bool aJoinQueued = aObserver.JoinRoom(aRoomId, "password-123") && aObserver.GetLastErrorCode().empty();
	Check(aJoinQueued && aObserver.GetLastErrorMessage().empty(), "room retry clears its previous localized error");
	Check(aJoinQueued && Wait([&] { return aObserver.GetLastErrorCode() == "room-full"; }) && ReadableError(aObserver),
		"full room rejected with a localized reason before ICE");
	aHost.KickPeer(aHostChannel->GetPeerId());
	Check(Wait([&] { return !aClientChannel->IsOpen() && aClient.GetLastErrorCode() == "host-kicked"; }) &&
		ReadableError(aClient), "host kick closes adopted transport with a localized reason");
	aHost.LeaveRoom(); aObserver.RefreshRooms();
	Check(Wait([&] { return aObserver.GetRooms().empty(); }), "leave removes directory room");
	const auto aBeforeStop = std::chrono::steady_clock::now();
	for (auto* aPeer : aPeers) aPeer->Stop();
	Check(std::chrono::steady_clock::now() - aBeforeStop < std::chrono::seconds(10), "stop joins signaling threads");
	Check(!aHost.IsConnecting() && !aHost.IsConnected() && !aClient.IsConnecting() && !aClient.IsConnected() &&
		aWrongBuild.GetLastErrorCode().empty() && aObserver.GetLastErrorCode().empty() &&
		aClient.GetLastErrorMessage().empty() && aObserver.GetLastErrorMessage().empty(),
		"stopped directories clear connected and pending states");
	NetOnline aUnavailable;
	Check(aUnavailable.Connect("http://127.0.0.1:1", aBuild, "Unavailable") &&
		Wait([&] { return !aUnavailable.IsConnecting(); }) && !aUnavailable.IsConnected() &&
		aUnavailable.GetLastErrorCode().find("signaling-") == 0 && ReadableError(aUnavailable),
		"failed directory connection clears pending state and explains the failed network stage");
	aUnavailable.Stop();
	std::printf("ONLINE_TESTS passed=%d failed=%d\n", aPassed, aFailed);
	return aFailed ? 1 : 0;
}
