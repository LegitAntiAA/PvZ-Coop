#ifndef __NETCONNECTION_H__
#define __NETCONNECTION_H__

#include "NetCommon.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

namespace LawnNet
{

// One TCP session. Owns the socket, a reader thread and a send mutex.
// Complete frames are pushed into mRecvQueue; the game thread pops them.
class NetConnection
{
public:
	typedef std::function<void(uint8_t, NetBuffer&)> MessageHandler;

public:
	NetConnection();
	~NetConnection();

	// Adopt an accepted socket (host side) or a freshly connected one.
	bool Adopt(SOCKET theSocket);

	void SetMessageHandler(MessageHandler theHandler) { mHandler = theHandler; }

	bool IsConnected() const { return mConnected; }

	// Thread-safe send of one message (frame-encodes internally).
	bool SendMessage(uint8_t theMsgType, const NetBuffer& thePayload);
	bool SendMessage(uint8_t theMsgType)
	{
		NetBuffer anEmpty;
		return SendMessage(theMsgType, anEmpty);
	}

	// Called from the game thread: fires the handler for each queued message.
	// Returns the number of messages processed.
	int PumpMessages();

	void Disconnect();

	// Ping bookkeeping (called by NetManager)
	uint32_t NextPingSeq() { return mPingSeq++; }
	void OnPong(uint32_t theSeq);
	uint32_t GetSeqSentTime(uint32_t theSeq);
	uint16_t GetPing() const { return mPingMs; }
	void SetPing(uint16_t theMs) { mPingMs = theMs; }

	uint64_t GetBytesSent() const { return mBytesSent; }
	uint64_t GetBytesRecv() const { return mBytesRecv; }

private:
	void RecvThreadProc();
	bool SendAll(const uint8_t* theData, size_t theSize);
	void QueueFrame(const uint8_t* theData, size_t theSize);

private:
	SOCKET mSocket;
	std::atomic<bool> mConnected;
	std::thread mRecvThread;
	std::atomic<bool> mThreadRunning;

	std::mutex mSendMutex;

	std::mutex mRecvQueueMutex;
	std::deque<NetBuffer> mRecvQueue;
	std::deque<uint8_t> mRecvTypes;

	MessageHandler mHandler;

	// partial frame assembly (owned by recv thread)
	std::vector<uint8_t> mPartial;
	uint16_t mExpectedSize;

	// ping bookkeeping
	std::mutex mPingMutex;
	std::map<uint32_t, uint32_t> mPingTimes;
	uint32_t mPingSeq;
	uint16_t mPingMs;

	std::atomic<uint64_t> mBytesSent;
	std::atomic<uint64_t> mBytesRecv;
};

// Winsock startup/cleanup (ref-counted)
bool NetStartupWSA();
void NetShutdownWSA();

// Connect with timeout (returns valid socket or INVALID_SOCKET)
SOCKET NetConnectTo(const char* theIP, uint16_t thePort, int theTimeoutMs);

} // namespace LawnNet

#endif // __NETCONNECTION_H__
