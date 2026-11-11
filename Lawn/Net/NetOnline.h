#ifndef __NETONLINE_H__
#define __NETONLINE_H__

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace LawnNet
{

struct NetOnlineRoom
{
	std::string mId;
	std::string mName;
	std::string mHost;
	std::string mBuild;
	int mPlayers = 0;
	int mMaxPlayers = 4;
	bool mHasPassword = false;
	bool mPlaying = false;
};

// A reliable, ordered WebRTC DataChannel.  Each message contains one complete
// existing PvZ length-prefixed frame; the transport never interprets gameplay.
class NetOnlineChannel
{
public:
	struct State;
	explicit NetOnlineChannel(std::shared_ptr<State> theState);
	bool Send(const std::vector<uint8_t>& theFrame);
	bool Receive(std::vector<uint8_t>& theFrame);
	bool IsOpen() const;
	void Close();
	std::string GetPeerId() const;

private:
	std::shared_ptr<State> mState;
};

// The game thread calls Update(), then adopts TakeOpenedChannels().  Signaling
// and ICE callbacks run on worker threads and never touch LawnApp or Board.
class NetOnline
{
public:
	NetOnline();
	~NetOnline();
	NetOnline(const NetOnline&) = delete;
	NetOnline& operator=(const NetOnline&) = delete;

	bool Connect(const std::string& theEndpoint, const std::string& theBuild,
		const std::string& theNickname, const std::string& theStun = "stun:stun.cloudflare.com:3478");
	void Update();
	void RefreshRooms();
	bool CreateRoom(const std::string& theName, const std::string& thePassword, int theMaxPlayers);
	bool JoinRoom(const std::string& theRoomId, const std::string& thePassword);
	void SetPlaying(bool thePlaying);
	void KickPeer(const std::string& thePeerId);
	void LeaveRoom();
	void Stop();
	bool IsConnected() const;
	bool IsHostingRoom() const;
	std::string GetRoomId() const;
	std::string GetStatus() const;
	std::vector<NetOnlineRoom> GetRooms() const;
	std::vector<std::shared_ptr<NetOnlineChannel>> TakeOpenedChannels();

private:
	struct Impl;
	std::unique_ptr<Impl> mImpl;
};

}
#endif
