#include "NetConnection.h"

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
	if (WSAStartup(MAKEWORD(2, 2), &aData) != 0)
		return false;
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
		return INVALID_SOCKET;

	// non-blocking connect with timeout
	u_long aNonBlocking = 1;
	ioctlsocket(aSocket, FIONBIO, &aNonBlocking);

	sockaddr_in anAddr;
	memset(&anAddr, 0, sizeof(anAddr));
	anAddr.sin_family = AF_INET;
	anAddr.sin_port = htons(thePort);
	inet_pton(AF_INET, theIP, &anAddr.sin_addr);

	int aResult = connect(aSocket, (sockaddr*)&anAddr, sizeof(anAddr));
	if (aResult == SOCKET_ERROR)
	{
		int anErr = WSAGetLastError();
		if (anErr != WSAEWOULDBLOCK)
		{
			closesocket(aSocket);
			return INVALID_SOCKET;
		}

		fd_set aWriteSet, anErrSet;
		FD_ZERO(&aWriteSet);
		FD_ZERO(&anErrSet);
		FD_SET(aSocket, &aWriteSet);
		FD_SET(aSocket, &anErrSet);
		timeval aTimeout = { theTimeoutMs / 1000, (theTimeoutMs % 1000) * 1000 };
		if (select(0, nullptr, &aWriteSet, &anErrSet, &aTimeout) <= 0)
		{
			closesocket(aSocket);
			return INVALID_SOCKET;
		}
		int anOptVal = 0;
		int anOptLen = sizeof(anOptVal);
		getsockopt(aSocket, SOL_SOCKET, SO_ERROR, (char*)&anOptVal, &anOptLen);
		if (anOptVal != 0)
		{
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
	return true;
}

void NetConnection::RecvThreadProc()
{
	uint8_t aChunk[8192];
	while (mThreadRunning && mConnected)
	{
		int aRecv = recv(mSocket, (char*)aChunk, sizeof(aChunk), 0);
		if (aRecv <= 0)
		{
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
					mRecvQueue.pop_front();
					mRecvTypes.pop_front();
				}
			}
			mPartial.clear();
			mExpectedSize = 0;
		}
	}
}

bool NetConnection::SendMessage(uint8_t theMsgType, const NetBuffer& thePayload)
{
	if (!mConnected)
		return false;
	NetBuffer aFrame;
	if (!NetEncodeFrame(thePayload, theMsgType, aFrame))
		return false;
	std::lock_guard<std::mutex> aLock(mSendMutex);
	return SendAll(aFrame.Data(), aFrame.Size());
}

bool NetConnection::SendAll(const uint8_t* theData, size_t theSize)
{
	if (mSocket == INVALID_SOCKET)
		return false;
	size_t aSent = 0;
	while (aSent < theSize)
	{
		int aResult = send(mSocket, (const char*)(theData + aSent), (int)(theSize - aSent), 0);
		if (aResult == SOCKET_ERROR)
		{
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
	// Swap out the queue under the lock so the recv thread can keep filling.
	decltype(mRecvQueue) aQueue;
	decltype(mRecvTypes) aTypes;
	{
		std::lock_guard<std::mutex> aLock(mRecvQueueMutex);
		aQueue.swap(mRecvQueue);
		aTypes.swap(mRecvTypes);
	}

	int aCount = 0;
	while (!aQueue.empty() && mConnected)
	{
		uint8_t aMsgType = aTypes.front();
		aTypes.pop_front();
		NetBuffer& aPayload = aQueue.front();
		if (mHandler)
			mHandler(aMsgType, aPayload);
		aQueue.pop_front();
		aCount++;
	}
	return aCount;
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
	if (mThreadRunning)
	{
		mThreadRunning = false;
		mConnected = false;
		if (mRecvThread.joinable())
			mRecvThread.join();
	}
	if (mSocket != INVALID_SOCKET)
	{
		shutdown(mSocket, SD_BOTH);
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
	mConnected = false;
}

} // namespace LawnNet
