#include "NetConnection.h"
#include "NetOnline.h"
#include <mstcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace LawnNet
{

static int gWSARefCount = 0;
static bool gWSAOk = false;

bool NetStartupWSA()
{
	if (gWSAOk)
	{
		gWSARefCount++;
		return true;
	}
	WSADATA aData;
	int aResult = WSAStartup(MAKEWORD(2, 2), &aData);
	if (aResult != 0)
	{
		NetTrace("WSAStartup failed error=%d", aResult);
		return false;
	}
	gWSAOk = true;
	gWSARefCount = 1;
	return true;
}

void NetShutdownWSA()
{
	if (!gWSAOk)
		return;
	gWSARefCount--;
	if (gWSARefCount <= 0)
	{
		WSACleanup();
		gWSAOk = false;
		gWSARefCount = 0;
	}
}

SOCKET NetConnectTo(const char* theIP, uint16_t thePort, int theTimeoutMs)
{
	SOCKET aSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (aSocket == INVALID_SOCKET)
	{
		NetTrace("NetConnectTo socket failed error=%d", WSAGetLastError());
		return INVALID_SOCKET;
	}

	// non-blocking connect with timeout
	u_long aNonBlocking = 1;
	ioctlsocket(aSocket, FIONBIO, &aNonBlocking);

	sockaddr_in anAddr;
	memset(&anAddr, 0, sizeof(anAddr));
	anAddr.sin_family = AF_INET;
	anAddr.sin_port = htons(thePort);
	if (inet_pton(AF_INET, theIP, &anAddr.sin_addr) != 1)
	{
		NetTrace("NetConnectTo invalid IPv4 address=%s", theIP ? theIP : "(null)");
		closesocket(aSocket);
		return INVALID_SOCKET;
	}

	int aResult = connect(aSocket, (sockaddr*)&anAddr, sizeof(anAddr));
	if (aResult == SOCKET_ERROR)
	{
		int anErr = WSAGetLastError();
		if (anErr != WSAEWOULDBLOCK)
		{
			NetTrace("NetConnectTo immediate failure endpoint=%s:%u error=%d", theIP, thePort, anErr);
			closesocket(aSocket);
			return INVALID_SOCKET;
		}

		fd_set aWriteSet, anErrSet;
		FD_ZERO(&aWriteSet);
		FD_ZERO(&anErrSet);
		FD_SET(aSocket, &aWriteSet);
		FD_SET(aSocket, &anErrSet);
		timeval aTimeout = { theTimeoutMs / 1000, (theTimeoutMs % 1000) * 1000 };
		int aSelect = select(0, nullptr, &aWriteSet, &anErrSet, &aTimeout);
		if (aSelect <= 0)
		{
			NetTrace("NetConnectTo timeout/select failure endpoint=%s:%u result=%d error=%d", theIP, thePort, aSelect, WSAGetLastError());
			closesocket(aSocket);
			return INVALID_SOCKET;
		}
		int anOptVal = 0;
		int anOptLen = sizeof(anOptVal);
		getsockopt(aSocket, SOL_SOCKET, SO_ERROR, (char*)&anOptVal, &anOptLen);
		if (anOptVal != 0)
		{
			NetTrace("NetConnectTo completion failure endpoint=%s:%u error=%d", theIP, thePort, anOptVal);
			closesocket(aSocket);
			return INVALID_SOCKET;
		}
	}

	// back to blocking mode + TCP_NODELAY
	u_long aBlocking = 0;
	ioctlsocket(aSocket, FIONBIO, &aBlocking);
	BOOL aNoDelay = TRUE;
	setsockopt(aSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&aNoDelay, sizeof(BOOL));
	return aSocket;
}

// -------------------------------------------------------------------------
NetConnection::NetConnection() :
	mSocket(INVALID_SOCKET),
	mConnected(false),
	mThreadRunning(false),
	mExpectedSize(0),
	mPingSeq(1),
	mPingMs(0),
	mBytesSent(0),
	mBytesRecv(0)
{
}

NetConnection::~NetConnection()
{
	Disconnect();
}

bool NetConnection::Adopt(SOCKET theSocket)
{
	Disconnect();
	if (theSocket == INVALID_SOCKET)
		return false;

	BOOL aNoDelay = TRUE;
	setsockopt(theSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&aNoDelay, sizeof(BOOL));

	// keepalives to detect dead peers (10s idle, 3s probes)
	BOOL aKeepAlive = TRUE;
	setsockopt(theSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&aKeepAlive, sizeof(BOOL));
	DWORD aKABytes[3] = { 10000, 3000, 1 };
	DWORD aKARet = 0;
	WSAIoctl(theSocket, SIO_KEEPALIVE_VALS, &aKABytes, sizeof(aKABytes), nullptr, 0, &aKARet, nullptr, nullptr);

	mSocket = theSocket;
	mConnected = true;
	mExpectedSize = 0;
	mPartial.clear();

	mThreadRunning = true;
	mRecvThread = std::thread(&NetConnection::RecvThreadProc, this);
	NetTrace("connection adopted socket=%llu", (unsigned long long)theSocket);
	return true;
}

bool NetConnection::Adopt(std::shared_ptr<NetOnlineChannel> theChannel)
{
	Disconnect();
	if (!theChannel || !theChannel->IsOpen()) return false;
	mChannel = std::move(theChannel);
	mConnected = true;
	mBytesSent = 0; mBytesRecv = 0;
	return true;
}

std::string NetConnection::GetOnlinePeerId() const
{
	return mChannel ? mChannel->GetPeerId() : "";
}

void NetConnection::RecvThreadProc()
{
	uint8_t aChunk[8192];
	while (mThreadRunning && mConnected)
	{
		int aRecv = recv(mSocket, (char*)aChunk, sizeof(aChunk), 0);
		if (aRecv <= 0)
		{
			NetTrace("recv thread ended socket=%llu result=%d error=%d bytesRecv=%llu", (unsigned long long)mSocket, aRecv, WSAGetLastError(), (unsigned long long)mBytesRecv.load());
			mConnected = false;
			break;
		}
		mBytesRecv += aRecv;
		QueueFrame(aChunk, aRecv);
	}
	// wake up the game thread one last time so it can observe the disconnect
}

void NetConnection::QueueFrame(const uint8_t* theData, size_t theSize)
{
	size_t aPos = 0;
	while (aPos < theSize)
	{
		if (mExpectedSize == 0)
		{
			// need header (2 bytes size + 1 byte type)
			if (mPartial.size() + (theSize - aPos) < 3)
			{
				mPartial.insert(mPartial.end(), theData + aPos, theData + theSize);
				return;
			}
			size_t aNeed = 3 - mPartial.size();
			mPartial.insert(mPartial.end(), theData + aPos, theData + aPos + aNeed);
			aPos += aNeed;
			uint16_t aFrameSize;
			memcpy(&aFrameSize, mPartial.data(), 2);
			if (aFrameSize > NET_MAX_FRAME)
			{
				NetTrace("protocol violation oversized frame=%u max=%d", aFrameSize, NET_MAX_FRAME);
				mConnected = false;
				return;
			}
			mExpectedSize = aFrameSize;
		}

		size_t aRemaining = theSize - aPos;
		size_t aWant = (size_t)mExpectedSize + 3 - mPartial.size();
		size_t aTake = aRemaining < aWant ? aRemaining : aWant;
		mPartial.insert(mPartial.end(), theData + aPos, theData + aPos + aTake);
		aPos += aTake;

		if (mPartial.size() == (size_t)mExpectedSize + 3)
		{
			NetBuffer aPayload;
			uint8_t aMsgType = 0;
			if (NetDecodeFrame(mPartial.data(), mPartial.size(), &aMsgType, aPayload))
			{
				std::lock_guard<std::mutex> aLock(mRecvQueueMutex);
				mRecvTypes.push_back(aMsgType);
				mRecvQueue.push_back(std::move(aPayload));
				if (mRecvQueue.size() > 4096) // runaway protection
				{
					NetTrace("receive queue overflow; dropping oldest frame queued=%zu", mRecvQueue.size());
					mRecvQueue.pop_front();
					mRecvTypes.pop_front();
				}
			}
			else
			{
				NetTrace("frame decode failed bytes=%zu expectedPayload=%u", mPartial.size(), mExpectedSize);
			}
			mPartial.clear();
			mExpectedSize = 0;
		}
	}
}

bool NetConnection::SendMessage(uint8_t theMsgType, const NetBuffer& thePayload)
{
	if (!mConnected)
	{
		NetTrace("send rejected disconnected type=%s(%u) bytes=%zu", NetMessageName(theMsgType), theMsgType, thePayload.Size());
		return false;
	}
	NetBuffer aFrame;
	if (!NetEncodeFrame(thePayload, theMsgType, aFrame))
	{
		NetTrace("send encode failed type=%s(%u) bytes=%zu", NetMessageName(theMsgType), theMsgType, thePayload.Size());
		return false;
	}
	std::lock_guard<std::mutex> aLock(mSendMutex);
	return SendAll(aFrame.Data(), aFrame.Size());
}

bool NetConnection::SendAll(const uint8_t* theData, size_t theSize)
{
	if (mChannel)
	{
		const bool sent = mChannel->Send(std::vector<uint8_t>(theData, theData + theSize));
		if (sent) mBytesSent += theSize;
		return sent;
	}
	if (mSocket == INVALID_SOCKET)
		return false;
	size_t aSent = 0;
	while (aSent < theSize)
	{
		int aResult = send(mSocket, (const char*)(theData + aSent), (int)(theSize - aSent), 0);
		if (aResult == SOCKET_ERROR || aResult == 0)
		{
			NetTrace("send failed socket=%llu sent=%zu/%zu result=%d error=%d", (unsigned long long)mSocket, aSent, theSize, aResult, WSAGetLastError());
			mConnected = false;
			return false;
		}
		aSent += aResult;
		mBytesSent += aResult;
	}
	return true;
}

int NetConnection::PumpMessages()
{
	if (mChannel)
	{
		std::vector<uint8_t> frame;
		for (int i = 0; i < 256 && mChannel->Receive(frame); ++i)
		{
			uint8_t type = 0;
			NetBuffer payload;
			if (!NetDecodeFrame(frame.data(), frame.size(), &type, payload))
			{ mChannel->Close(); break; }
			mBytesRecv += frame.size();
			std::lock_guard<std::mutex> lock(mRecvQueueMutex);
			mRecvQueue.push_back(std::move(payload)); mRecvTypes.push_back(type);
		}
	}
	// Swap out the queue under the lock so the recv thread can keep filling.
	decltype(mRecvQueue) aQueue;
	decltype(mRecvTypes) aTypes;
	{
		std::lock_guard<std::mutex> aLock(mRecvQueueMutex);
		aQueue.swap(mRecvQueue);
		aTypes.swap(mRecvTypes);
	}

	int aCount = 0;
	while (!aQueue.empty())
	{
		uint8_t aMsgType = aTypes.front();
		aTypes.pop_front();
		NetBuffer& aPayload = aQueue.front();
		if (mHandler)
			mHandler(aMsgType, aPayload);
		aQueue.pop_front();
		aCount++;
	}
	if (mChannel && !mChannel->IsOpen()) mConnected = false;
	return aCount;
}

uint32_t NetConnection::NextPingSeq()
{
	const uint32_t aSeq = mPingSeq++;
	const uint32_t aNow = GetTickCount();
	std::lock_guard<std::mutex> aLock(mPingMutex);
	mPingTimes[aSeq] = aNow;
	// A peer that never answers must not grow this map forever.
	while (mPingTimes.size() > 32)
		mPingTimes.erase(mPingTimes.begin());
	return aSeq;
}

void NetConnection::OnPong(uint32_t theSeq)
{
	std::lock_guard<std::mutex> aLock(mPingMutex);
	auto anIt = mPingTimes.find(theSeq);
	if (anIt != mPingTimes.end())
	{
		uint32_t aNow = GetTickCount();
		mPingMs = (uint16_t)(aNow - anIt->second);
		mPingTimes.erase(anIt);
	}
}

uint32_t NetConnection::GetSeqSentTime(uint32_t theSeq)
{
	std::lock_guard<std::mutex> aLock(mPingMutex);
	auto anIt = mPingTimes.find(theSeq);
	return anIt != mPingTimes.end() ? anIt->second : 0;
}

void NetConnection::Disconnect()
{
	if (mChannel) { mChannel->Close(); mChannel.reset(); }
	if (mSocket != INVALID_SOCKET || mConnected)
		NetTrace("connection disconnect socket=%llu bytesSent=%llu bytesRecv=%llu", (unsigned long long)mSocket, (unsigned long long)mBytesSent.load(), (unsigned long long)mBytesRecv.load());
	// recv() is blocking, so close its read side before joining the worker.
	// Joining first can deadlock forever when the peer is still connected but
	// silent (for example while returning to the menu or cancelling a retry).
	mThreadRunning = false;
	mConnected = false;
	SOCKET aSocket = mSocket;
	mSocket = INVALID_SOCKET;
	if (aSocket != INVALID_SOCKET)
	{
		NetTrace("connection shutdown begin socket=%llu", (unsigned long long)aSocket);
		shutdown(aSocket, SD_BOTH);
		NetTrace("connection shutdown complete socket=%llu", (unsigned long long)aSocket);
	}

	if (mRecvThread.joinable())
	{
		NetTrace("connection recv join begin socket=%llu", (unsigned long long)aSocket);
		mRecvThread.join();
		NetTrace("connection recv join complete socket=%llu", (unsigned long long)aSocket);
	}

	if (aSocket != INVALID_SOCKET)
	{
		NetTrace("connection close begin socket=%llu", (unsigned long long)aSocket);
		closesocket(aSocket);
		NetTrace("connection close complete socket=%llu", (unsigned long long)aSocket);
	}
	{
		std::lock_guard<std::mutex> lock(mRecvQueueMutex);
		mRecvQueue.clear(); mRecvTypes.clear();
	}
	mPartial.clear(); mExpectedSize = 0;
}

} // namespace LawnNet
