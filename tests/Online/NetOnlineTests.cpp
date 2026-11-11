#include "../../Lawn/Net/NetOnline.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>

using namespace LawnNet;

int main(int argc, char** argv)
{
	const std::string aEndpoint = argc > 1 ? argv[1] : "http://127.0.0.1:8787";
	NetOnline aHost, aClient, aWrongBuild, aObserver;
	NetOnline* aPeers[] = { &aHost, &aClient, &aWrongBuild, &aObserver };
	int aPassed = 0, aFailed = 0;
	auto Check = [&](bool ok, const char* label) {
		std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); std::fflush(stdout);
		if (ok) ++aPassed; else ++aFailed;
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
	if (!aHost.Connect(aEndpoint, "test-native-1", "Host") ||
		!aClient.Connect(aEndpoint, "test-native-1", "Client") ||
		!aWrongBuild.Connect(aEndpoint, "test-native-2", "Other") ||
		!aObserver.Connect(aEndpoint, "test-native-1", "Observer") ||
		!Wait([&] { return aHost.IsConnected() && aClient.IsConnected() && aWrongBuild.IsConnected() && aObserver.IsConnected(); }))
	{
		Check(false, "connect native WinHTTP signaling clients");
		for (auto* aPeer : aPeers) std::printf("STATUS %s\n", aPeer->GetStatus().c_str());
		return 1;
	}
	Check(true, "connect native WinHTTP signaling clients");
	Check(aHost.CreateRoom("Native loopback test", "password-123", 2) && Wait([&] { return !aHost.GetRoomId().empty(); }), "create password room");
	const std::string aRoomId = aHost.GetRoomId();
	aClient.RefreshRooms(); aWrongBuild.RefreshRooms();
	Check(Wait([&] { return aClient.GetRooms().size() == 1; }), "same-build room discovery");
	Check(aWrongBuild.GetRooms().empty(), "hide other-build rooms");
	aWrongBuild.JoinRoom(aRoomId, "password-123");
	Check(Wait([&] { return aWrongBuild.GetStatus().find("version-mismatch") != std::string::npos; }), "reject direct different-build join");
	aObserver.JoinRoom(aRoomId, "wrong-password");
	Check(Wait([&] { return aObserver.GetStatus().find("wrong-password") != std::string::npos; }), "native PBKDF2 wrong-password rejection");
	aClient.JoinRoom(aRoomId, "password-123");
	std::shared_ptr<NetOnlineChannel> aHostChannel, aClientChannel;
	const bool aConnected = Wait([&] {
		for (auto& aChannel : aHost.TakeOpenedChannels()) aHostChannel = aChannel;
		for (auto& aChannel : aClient.TakeOpenedChannels()) aClientChannel = aChannel;
		return aHostChannel && aClientChannel && aHostChannel->IsOpen() && aClientChannel->IsOpen();
	}, 45);
	Check(aConnected, "real local ICE + DTLS + SCTP ordered DataChannel");
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
	aObserver.JoinRoom(aRoomId, "password-123");
	Check(Wait([&] { return aObserver.GetStatus().find("room-full") != std::string::npos; }), "full room rejected before ICE");
	aHost.KickPeer(aHostChannel->GetPeerId());
	Check(Wait([&] { return !aClientChannel->IsOpen() && aClient.GetStatus().find("kicked") != std::string::npos; }), "host kick closes adopted transport");
	aHost.LeaveRoom(); aObserver.RefreshRooms();
	Check(Wait([&] { return aObserver.GetRooms().empty(); }), "leave removes directory room");
	const auto aBeforeStop = std::chrono::steady_clock::now();
	for (auto* aPeer : aPeers) aPeer->Stop();
	Check(std::chrono::steady_clock::now() - aBeforeStop < std::chrono::seconds(10), "stop joins signaling threads");
	std::printf("ONLINE_TESTS passed=%d failed=%d\n", aPassed, aFailed);
	return aFailed ? 1 : 0;
}
