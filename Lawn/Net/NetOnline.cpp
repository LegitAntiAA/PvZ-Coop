#include "NetOnline.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#define RTC_STATIC
#define RTC_ENABLE_MEDIA 0
#define RTC_ENABLE_WEBSOCKET 0
#include "../../third_party/libdatachannel/rtc.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace LawnNet
{
namespace
{
using Json = nlohmann::json;
constexpr size_t kMaxFrame = 65538;
constexpr size_t kMaxSignal = 65536;
constexpr size_t kMaxQueuedBytes = 4 * 1024 * 1024;

// u8 literals are char in C++17 and char8_t in C++20; both store UTF-8 bytes.
template <class Char> std::string OnlineText(const Char* text)
{
	return reinterpret_cast<const char*>(text);
}

std::string ServiceErrorText(const std::string& code)
{
	if (code == "wrong-password") return OnlineText(u8"\u041d\u0435\u0432\u0435\u0440\u043d\u044b\u0439 \u043f\u0430\u0440\u043e\u043b\u044c \u043a\u043e\u043c\u043d\u0430\u0442\u044b. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u043f\u0430\u0440\u043e\u043b\u044c \u0438 \u043f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0441\u043d\u043e\u0432\u0430.");
	if (code == "room-full") return OnlineText(u8"\u041a\u043e\u043c\u043d\u0430\u0442\u0430 \u0437\u0430\u043f\u043e\u043b\u043d\u0435\u043d\u0430. \u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u0434\u0440\u0443\u0433\u0443\u044e \u043a\u043e\u043c\u043d\u0430\u0442\u0443 \u0438\u043b\u0438 \u043e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a.");
	if (code == "room-not-found") return OnlineText(u8"\u041a\u043e\u043c\u043d\u0430\u0442\u0430 \u0443\u0436\u0435 \u0437\u0430\u043a\u0440\u044b\u0442\u0430. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a \u043a\u043e\u043c\u043d\u0430\u0442.");
	if (code == "version-mismatch") return OnlineText(u8"\u0412\u0435\u0440\u0441\u0438\u0438 \u0438\u0433\u0440\u044b \u0440\u0430\u0437\u043b\u0438\u0447\u0430\u044e\u0442\u0441\u044f. \u0423\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u0435 \u043e\u0434\u0438\u043d\u0430\u043a\u043e\u0432\u0443\u044e \u0441\u0431\u043e\u0440\u043a\u0443 \u0441 \u0445\u043e\u0437\u044f\u0438\u043d\u043e\u043c \u043a\u043e\u043c\u043d\u0430\u0442\u044b.");
	if (code == "rate-limit" || code == "password-retry-later")
		return OnlineText(u8"\u0421\u043b\u0438\u0448\u043a\u043e\u043c \u043c\u043d\u043e\u0433\u043e \u043f\u043e\u043f\u044b\u0442\u043e\u043a. \u041d\u0435\u043c\u043d\u043e\u0433\u043e \u043f\u043e\u0434\u043e\u0436\u0434\u0438\u0442\u0435 \u0438 \u043f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0441\u043d\u043e\u0432\u0430.");
	if (code == "directory-full") return OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043a\u043e\u043c\u043d\u0430\u0442 \u043f\u0435\u0440\u0435\u0433\u0440\u0443\u0436\u0435\u043d. \u041f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0438\u0442\u044c\u0441\u044f \u043f\u043e\u0437\u0436\u0435.");
	if (code == "already-in-room") return OnlineText(u8"\u0412\u044b \u0443\u0436\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u044b \u043a \u043a\u043e\u043c\u043d\u0430\u0442\u0435. \u0421\u043d\u0430\u0447\u0430\u043b\u0430 \u0432\u044b\u0439\u0434\u0438\u0442\u0435 \u0438\u0437 \u043d\u0435\u0451.");
	if (code == "invalid-room") return OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0441\u043e\u0437\u0434\u0430\u0442\u044c \u043a\u043e\u043c\u043d\u0430\u0442\u0443. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0435\u0451 \u043d\u0430\u0437\u0432\u0430\u043d\u0438\u0435 \u0438 \u0447\u0438\u0441\u043b\u043e \u0438\u0433\u0440\u043e\u043a\u043e\u0432.");
	if (code == "hello-required" || code == "invalid-hello")
		return OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043d\u0435 \u043f\u043e\u0434\u0442\u0432\u0435\u0440\u0434\u0438\u043b \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435 \u0438\u0433\u0440\u044b. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a \u043a\u043e\u043c\u043d\u0430\u0442 \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443.");
	if (code == "host-only") return OnlineText(u8"\u042d\u0442\u043e \u0434\u0435\u0439\u0441\u0442\u0432\u0438\u0435 \u0434\u043e\u0441\u0442\u0443\u043f\u043d\u043e \u0442\u043e\u043b\u044c\u043a\u043e \u0445\u043e\u0437\u044f\u0438\u043d\u0443 \u043a\u043e\u043c\u043d\u0430\u0442\u044b.");
	if (code == "invalid-peer") return OnlineText(u8"\u0418\u0433\u0440\u043e\u043a \u0443\u0436\u0435 \u043e\u0442\u043a\u043b\u044e\u0447\u0438\u043b\u0441\u044f \u0438\u043b\u0438 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u0435\u043d. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a \u043a\u043e\u043c\u043d\u0430\u0442.");
	if (code == "invalid-signal" || code == "invalid-message" || code == "invalid-json" || code == "unknown-message")
		return OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043e\u0442\u043a\u043b\u043e\u043d\u0438\u043b \u0437\u0430\u043f\u0440\u043e\u0441 \u0438\u0433\u0440\u044b. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0438\u0433\u0440\u0443 \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443.");
	return {};
}

// Load only the packaged DLL beside this executable.  LAN/offline sessions do
// not depend on WebRTC being installed and no global DLL search path is changed.
struct RtcApi
{
	HMODULE mDll = nullptr;
#define RTC_SYMBOL(name) decltype(&name) name = nullptr;
	RTC_SYMBOL(rtcInitLogger)
	RTC_SYMBOL(rtcCleanup)
	RTC_SYMBOL(rtcCreatePeerConnection)
	RTC_SYMBOL(rtcDeletePeerConnection)
	RTC_SYMBOL(rtcDeleteDataChannel)
	RTC_SYMBOL(rtcSetUserPointer)
	RTC_SYMBOL(rtcSetLocalDescriptionCallback)
	RTC_SYMBOL(rtcSetLocalCandidateCallback)
	RTC_SYMBOL(rtcSetStateChangeCallback)
	RTC_SYMBOL(rtcSetRemoteDescription)
	RTC_SYMBOL(rtcAddRemoteCandidate)
	RTC_SYMBOL(rtcSetDataChannelCallback)
	RTC_SYMBOL(rtcCreateDataChannel)
	RTC_SYMBOL(rtcSetOpenCallback)
	RTC_SYMBOL(rtcSetClosedCallback)
	RTC_SYMBOL(rtcSetMessageCallback)
	RTC_SYMBOL(rtcGetBufferedAmount)
	RTC_SYMBOL(rtcSendMessage)
#undef RTC_SYMBOL

	bool Load()
	{
		wchar_t aPath[32768];
		const DWORD aLength = GetModuleFileNameW(nullptr, aPath, 32768);
		if (aLength == 0 || aLength >= 32768) return false;
		std::wstring aDirectory(aPath, aLength);
		const size_t aSlash = aDirectory.find_last_of(L"\\/");
		if (aSlash == std::wstring::npos) return false;
		aDirectory.resize(aSlash + 1);
		aDirectory += L"datachannel.dll";
		mDll = LoadLibraryExW(aDirectory.c_str(), nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!mDll) return false;
#define RTC_LOAD(name) name = reinterpret_cast<decltype(name)>(GetProcAddress(mDll, #name)); if (!name) return false;
		RTC_LOAD(rtcInitLogger) RTC_LOAD(rtcCleanup)
		RTC_LOAD(rtcCreatePeerConnection) RTC_LOAD(rtcDeletePeerConnection)
		RTC_LOAD(rtcDeleteDataChannel)
		RTC_LOAD(rtcSetUserPointer) RTC_LOAD(rtcSetLocalDescriptionCallback)
		RTC_LOAD(rtcSetLocalCandidateCallback) RTC_LOAD(rtcSetStateChangeCallback)
		RTC_LOAD(rtcSetRemoteDescription) RTC_LOAD(rtcAddRemoteCandidate)
		RTC_LOAD(rtcSetDataChannelCallback) RTC_LOAD(rtcCreateDataChannel)
		RTC_LOAD(rtcSetOpenCallback) RTC_LOAD(rtcSetClosedCallback)
		RTC_LOAD(rtcSetMessageCallback) RTC_LOAD(rtcGetBufferedAmount)
		RTC_LOAD(rtcSendMessage)
#undef RTC_LOAD
		rtcInitLogger(RTC_LOG_ERROR, nullptr);
		return true;
	}
	~RtcApi()
	{
		if (mDll)
		{
			if (rtcCleanup) rtcCleanup();
			FreeLibrary(mDll);
		}
	}
};

std::shared_ptr<RtcApi> LoadRtc()
{
	static std::mutex aMutex;
	static std::weak_ptr<RtcApi> aWeak;
	std::lock_guard<std::mutex> aLock(aMutex);
	if (auto aLoaded = aWeak.lock()) return aLoaded;
	auto aApi = std::make_shared<RtcApi>();
	if (!aApi->Load()) return {};
	aWeak = aApi;
	return aApi;
}

std::wstring Wide(const std::string& aText)
{
	const int aCount = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, aText.data(), (int)aText.size(), nullptr, 0);
	if (aCount <= 0) return {};
	std::wstring aResult(aCount, L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, aText.data(), (int)aText.size(), aResult.data(), aCount);
	return aResult;
}

std::string Hex(const uint8_t* theBytes, size_t theSize)
{
	static const char aDigits[] = "0123456789abcdef";
	std::string aResult(theSize * 2, '0');
	for (size_t i = 0; i < theSize; ++i)
	{
		aResult[i * 2] = aDigits[theBytes[i] >> 4];
		aResult[i * 2 + 1] = aDigits[theBytes[i] & 15];
	}
	return aResult;
}

bool PasswordProof(const std::string& thePassword, const std::string& theSalt, std::string& theProof)
{
	if (thePassword.size() > 256 || theSalt.size() != 32) return false;
	uint8_t aSalt[16];
	auto aNibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
	for (size_t i = 0; i < 16; ++i)
	{
		int aHi = aNibble(theSalt[i * 2]), aLo = aNibble(theSalt[i * 2 + 1]);
		if (aHi < 0 || aLo < 0) return false;
		aSalt[i] = (uint8_t)((aHi << 4) | aLo);
	}
	BCRYPT_ALG_HANDLE aAlgorithm = nullptr;
	uint8_t aKey[32];
	if (BCryptOpenAlgorithmProvider(&aAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
		return false;
	const NTSTATUS aStatus = BCryptDeriveKeyPBKDF2(aAlgorithm,
		(PUCHAR)thePassword.data(), (ULONG)thePassword.size(), aSalt, sizeof(aSalt), 100000,
		aKey, sizeof(aKey), 0);
	BCryptCloseAlgorithmProvider(aAlgorithm, 0);
	if (aStatus < 0) return false;
	theProof = Hex(aKey, sizeof(aKey));
	SecureZeroMemory(aKey, sizeof(aKey));
	return true;
}
}

struct NetOnlineChannel::State
{
	std::shared_ptr<RtcApi> mApi;
	std::string mPeerId;
	mutable std::mutex mMutex;
	int mDc = -1;
	std::atomic<bool> mOpen{false};
	std::atomic<bool> mCloseRequested{false};
	std::deque<std::vector<uint8_t>> mIncoming;
	size_t mIncomingBytes = 0;
};

NetOnlineChannel::NetOnlineChannel(std::shared_ptr<State> theState) : mState(std::move(theState)) {}
bool NetOnlineChannel::Send(const std::vector<uint8_t>& theFrame)
{
	if (theFrame.empty() || theFrame.size() > kMaxFrame) return false;
	std::lock_guard<std::mutex> aLock(mState->mMutex);
	if (mState->mDc < 0 || !mState->mOpen || mState->mCloseRequested) return false;
	if (mState->mApi->rtcGetBufferedAmount(mState->mDc) > (int)kMaxQueuedBytes)
	{
		mState->mCloseRequested = true;
		return false;
	}
	return mState->mApi->rtcSendMessage(mState->mDc, (const char*)theFrame.data(), (int)theFrame.size()) == 0;
}
bool NetOnlineChannel::Receive(std::vector<uint8_t>& theFrame)
{
	std::lock_guard<std::mutex> aLock(mState->mMutex);
	if (mState->mIncoming.empty()) return false;
	theFrame = std::move(mState->mIncoming.front());
	mState->mIncoming.pop_front();
	mState->mIncomingBytes -= theFrame.size();
	return true;
}
bool NetOnlineChannel::IsOpen() const { return mState->mOpen && !mState->mCloseRequested; }
void NetOnlineChannel::Close() { mState->mCloseRequested = true; }
std::string NetOnlineChannel::GetPeerId() const { return mState->mPeerId; }

struct NetOnline::Impl
{
	enum class PeerFailure { None, Initialization, Ice, Closed, Timeout, InvalidSignal, InvalidData };
	struct Peer
	{
		Impl* mOwner;
		std::string mId;
		int mPc = -1;
		std::shared_ptr<NetOnlineChannel::State> mChannel;
		bool mDelivered = false;
		bool mDirectoryDeparted = false;
		bool mRemoteDescription = false;
		std::atomic<PeerFailure> mFailure{PeerFailure::None};
		std::vector<std::pair<std::string, std::string>> mCandidates;
		std::vector<int> mExtraDataChannels; // guarded by mChannel->mMutex
		std::chrono::steady_clock::time_point mCreated = std::chrono::steady_clock::now();
	};
	std::shared_ptr<RtcApi> mApi;
	std::atomic<bool> mRunning{false}, mRegistered{false}, mConnecting{false};
	std::atomic<HINTERNET> mSocket{nullptr};
	std::thread mReceiver, mSender;
	std::mutex mSignalMutex;
	std::condition_variable mSendCv;
	std::deque<std::string> mOutgoing, mIncoming;
	mutable std::mutex mViewMutex;
	std::string mStatus, mLastErrorCode, mLastErrorMessage, mBuild, mNickname, mStun, mRoomId, mPendingRoom, mPendingPassword;
	std::vector<NetOnlineRoom> mRooms;
	bool mHosting = false;
	std::map<std::string, std::unique_ptr<Peer>> mPeers;
	std::vector<std::shared_ptr<NetOnlineChannel>> mOpened;
	std::chrono::steady_clock::time_point mLastKeepalive = std::chrono::steady_clock::now();

	void Status(const std::string& theText)
	{
		std::lock_guard<std::mutex> aLock(mViewMutex);
		mStatus = theText;
	}
	void ErrorCode(const std::string& theCode)
	{
		std::lock_guard<std::mutex> aLock(mViewMutex);
		mLastErrorCode = theCode;
		if (theCode.empty()) mLastErrorMessage.clear();
	}
	void Error(const std::string& theCode, const std::string& theMessage)
	{
		std::lock_guard<std::mutex> aLock(mViewMutex);
		mLastErrorCode = theCode; mLastErrorMessage = theMessage; mStatus = theMessage;
	}
	void Success(const std::string& theStatus)
	{
		std::lock_guard<std::mutex> aLock(mViewMutex);
		mLastErrorCode.clear(); mLastErrorMessage.clear(); mStatus = theStatus;
	}
	void SignalError(const char* stage, DWORD error, DWORD http, USHORT close, bool connected)
	{
		if (error == ERROR_WINHTTP_NAME_NOT_RESOLVED)
			Error("signaling-dns", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u043d\u0430\u0439\u0442\u0438 \u0441\u0435\u0440\u0432\u0435\u0440 \u043a\u043e\u043c\u043d\u0430\u0442. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435 \u043a \u0438\u043d\u0442\u0435\u0440\u043d\u0435\u0442\u0443 \u0438 DNS."));
		else if (error == ERROR_WINHTTP_TIMEOUT)
			Error("signaling-timeout", OnlineText(u8"\u0421\u0435\u0440\u0432\u0435\u0440 \u043a\u043e\u043c\u043d\u0430\u0442 \u043d\u0435 \u043e\u0442\u0432\u0435\u0442\u0438\u043b \u0432\u043e\u0432\u0440\u0435\u043c\u044f. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0441\u0435\u0442\u044c \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443."));
		else if (error == ERROR_WINHTTP_CANNOT_CONNECT || error == ERROR_WINHTTP_CONNECTION_ERROR)
			Error("signaling-connect", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0441\u0432\u044f\u0437\u0430\u0442\u044c\u0441\u044f \u0441 \u0441\u0435\u0440\u0432\u0435\u0440\u043e\u043c \u043a\u043e\u043c\u043d\u0430\u0442. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0438\u043d\u0442\u0435\u0440\u043d\u0435\u0442, VPN \u0438 \u0431\u0440\u0430\u043d\u0434\u043c\u0430\u0443\u044d\u0440."));
		else if (http == 407 || error == ERROR_WINHTTP_LOGIN_FAILURE)
			Error("signaling-proxy-auth", OnlineText(u8"\u041f\u0440\u043e\u043a\u0441\u0438-\u0441\u0435\u0440\u0432\u0435\u0440 \u0442\u0440\u0435\u0431\u0443\u0435\u0442 \u0430\u0432\u0442\u043e\u0440\u0438\u0437\u0430\u0446\u0438\u044e \u0438\u043b\u0438 \u043e\u0442\u043a\u043b\u043e\u043d\u0438\u043b \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u0441\u0435\u0442\u0438."));
		else if (error == ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED)
			Error("signaling-access-denied", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u0437\u0430\u043f\u0440\u043e\u0441\u0438\u043b \u0441\u0435\u0440\u0442\u0438\u0444\u0438\u043a\u0430\u0442 \u0434\u043e\u0441\u0442\u0443\u043f\u0430. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u0441\u0435\u0442\u0438 \u0438\u043b\u0438 \u043f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0434\u0440\u0443\u0433\u0443\u044e \u0441\u0435\u0442\u044c."));
		else if (error == ERROR_WINHTTP_SECURE_FAILURE || error == ERROR_WINHTTP_SECURE_CERT_DATE_INVALID ||
			error == ERROR_WINHTTP_SECURE_CERT_CN_INVALID || error == ERROR_WINHTTP_SECURE_INVALID_CA ||
			error == ERROR_WINHTTP_SECURE_INVALID_CERT || error == ERROR_WINHTTP_SECURE_CERT_REVOKED ||
			error == ERROR_WINHTTP_SECURE_CHANNEL_ERROR || error == ERROR_WINHTTP_SECURE_CERT_REV_FAILED ||
			error == ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE)
			Error("signaling-tls", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0443\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u044c \u0437\u0430\u0449\u0438\u0449\u0451\u043d\u043d\u043e\u0435 \u0441\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435 \u0441 \u0441\u0435\u0440\u0432\u0438\u0441\u043e\u043c. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0434\u0430\u0442\u0443 \u043a\u043e\u043c\u043f\u044c\u044e\u0442\u0435\u0440\u0430 \u0438 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u0441\u0435\u0442\u0438."));
		else if (http == 401 || http == 403 || close == 1008)
			Error("signaling-access-denied", OnlineText(u8"\u0421\u0435\u0440\u0432\u0435\u0440 \u043e\u0442\u043a\u043b\u043e\u043d\u0438\u043b \u0434\u043e\u0441\u0442\u0443\u043f \u043a \u043e\u043d\u043b\u0430\u0439\u043d-\u043a\u043e\u043c\u043d\u0430\u0442\u0430\u043c. \u041f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u043f\u043e\u0437\u0436\u0435 \u0438\u043b\u0438 \u0434\u0440\u0443\u0433\u0443\u044e \u0441\u0435\u0442\u044c."));
		else if (http == 429)
			Error("rate-limit", ServiceErrorText("rate-limit"));
		else if (http >= 500)
			Error("signaling-unavailable", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043a\u043e\u043c\u043d\u0430\u0442 \u0432\u0440\u0435\u043c\u0435\u043d\u043d\u043e \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u0435\u043d. \u041f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u043f\u043e\u0437\u0436\u0435."));
		else if (http && http != 101)
			Error("signaling-handshake", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043d\u0435 \u043f\u0440\u0438\u043d\u044f\u043b \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435 \u043a \u043a\u043e\u043c\u043d\u0430\u0442\u0430\u043c. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0438\u0433\u0440\u0443 \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443."));
		else if (!strcmp(stage, "protocol"))
			Error("signaling-protocol", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043a\u043e\u043c\u043d\u0430\u0442 \u043f\u0440\u0438\u0441\u043b\u0430\u043b \u043d\u0435\u043a\u043e\u0440\u0440\u0435\u043a\u0442\u043d\u044b\u0439 \u043e\u0442\u0432\u0435\u0442. \u041f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435."));
		else if (connected)
			Error("signaling-disconnected", OnlineText(u8"\u0421\u0432\u044f\u0437\u044c \u0441 \u0441\u0435\u0440\u0432\u0438\u0441\u043e\u043c \u043a\u043e\u043c\u043d\u0430\u0442 \u043f\u0440\u0435\u0440\u0432\u0430\u043b\u0430\u0441\u044c. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a \u0438 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0438\u0442\u0435\u0441\u044c \u0441\u043d\u043e\u0432\u0430."));
		else
			Error("signaling-connection-failed", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0438\u0442\u044c\u0441\u044f \u043a \u0441\u0435\u0440\u0432\u0438\u0441\u0443 \u043a\u043e\u043c\u043d\u0430\u0442. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0441\u0435\u0442\u044c \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443."));
	}
	void PeerError(PeerFailure failure, bool opened)
	{
		if (opened)
			Error("peer-disconnected", OnlineText(u8"\u0421\u0432\u044f\u0437\u044c \u0441 \u0438\u0433\u0440\u043e\u043a\u043e\u043c \u043f\u0440\u0435\u0440\u0432\u0430\u043b\u0430\u0441\u044c. \u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0441\u0435\u0442\u044c \u0438 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0438\u0442\u0435\u0441\u044c \u043a \u043a\u043e\u043c\u043d\u0430\u0442\u0435 \u0441\u043d\u043e\u0432\u0430."));
		else if (failure == PeerFailure::Initialization)
			Error("direct-connection-failed", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u043f\u043e\u0434\u0433\u043e\u0442\u043e\u0432\u0438\u0442\u044c \u043f\u0440\u044f\u043c\u043e\u0435 \u0441\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435. \u041f\u0435\u0440\u0435\u0437\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0435 \u0438\u0433\u0440\u0443 \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443."));
		else if (failure == PeerFailure::Timeout)
			Error("direct-connection-failed", OnlineText(u8"\u0418\u0441\u0442\u0435\u043a\u043b\u043e \u0432\u0440\u0435\u043c\u044f \u043e\u0436\u0438\u0434\u0430\u043d\u0438\u044f \u043f\u0440\u044f\u043c\u043e\u0433\u043e \u0441\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u044f. \u0412\u043e\u0437\u043c\u043e\u0436\u043d\u043e, NAT \u0438\u043b\u0438 \u0431\u0440\u0430\u043d\u0434\u043c\u0430\u0443\u044d\u0440 \u0431\u043b\u043e\u043a\u0438\u0440\u0443\u0435\u0442 \u0441\u0432\u044f\u0437\u044c. \u041f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0434\u0440\u0443\u0433\u0443\u044e \u0441\u0435\u0442\u044c."));
		else if (failure == PeerFailure::InvalidSignal || failure == PeerFailure::InvalidData)
			Error("direct-connection-failed", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0441\u043e\u0433\u043b\u0430\u0441\u043e\u0432\u0430\u0442\u044c \u0441\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435 \u0441 \u0438\u0433\u0440\u043e\u043a\u043e\u043c. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0438\u0433\u0440\u0443 \u0438 \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u043f\u044b\u0442\u043a\u0443."));
		else if (failure == PeerFailure::Closed)
			Error("direct-connection-failed", OnlineText(u8"\u0421\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435 \u0441 \u0438\u0433\u0440\u043e\u043a\u043e\u043c \u0437\u0430\u043a\u0440\u044b\u043b\u043e\u0441\u044c \u0434\u043e \u0437\u0430\u0432\u0435\u0440\u0448\u0435\u043d\u0438\u044f \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u044f. \u041f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0441\u043d\u043e\u0432\u0430."));
		else
			Error("direct-connection-failed", OnlineText(u8"\u041f\u0440\u044f\u043c\u043e\u0435 \u0441\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u0435 \u043d\u0435 \u0443\u0441\u0442\u0430\u043d\u043e\u0432\u043b\u0435\u043d\u043e. \u0412\u043e\u0437\u043c\u043e\u0436\u043d\u044b \u043e\u0433\u0440\u0430\u043d\u0438\u0447\u0435\u043d\u0438\u044f NAT \u0438\u043b\u0438 \u0431\u0440\u0430\u043d\u0434\u043c\u0430\u0443\u044d\u0440\u0430. \u041f\u043e\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0434\u0440\u0443\u0433\u0443\u044e \u0441\u0435\u0442\u044c."));
	}
	bool Queue(const Json& theMessage)
	{
		std::string aText = theMessage.dump();
		if (!mRunning || aText.size() > kMaxSignal) return false;
		std::lock_guard<std::mutex> aLock(mSignalMutex);
		if (mOutgoing.size() >= 256) return false;
		mOutgoing.push_back(std::move(aText));
		mSendCv.notify_one();
		return true;
	}
	void RunSignal(std::wstring theUrl)
	{
		URL_COMPONENTS aUrl{};
		aUrl.dwStructSize = sizeof(aUrl);
		aUrl.dwHostNameLength = aUrl.dwUrlPathLength = aUrl.dwExtraInfoLength = (DWORD)-1;
		if (!WinHttpCrackUrl(theUrl.c_str(), 0, 0, &aUrl))
		{
			Error("invalid-endpoint", OnlineText(u8"\u041d\u0435\u0432\u0435\u0440\u043d\u044b\u0439 \u0430\u0434\u0440\u0435\u0441 \u0441\u0435\u0440\u0432\u0438\u0441\u0430 \u043a\u043e\u043c\u043d\u0430\u0442. \u0412\u043e\u0441\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u044f."));
			mRunning = false; mConnecting = false; return;
		}
		std::wstring aHost(aUrl.lpszHostName, aUrl.dwHostNameLength);
		std::wstring aPath(aUrl.lpszUrlPath, aUrl.dwUrlPathLength);
		if (aPath.empty() || aPath == L"/") aPath = L"/signal";
		if (aUrl.dwExtraInfoLength) aPath.append(aUrl.lpszExtraInfo, aUrl.dwExtraInfoLength);
		HINTERNET aSession = WinHttpOpen(L"PvZ-Coop/Online", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		HINTERNET aConnection = nullptr, aRequest = nullptr;
		DWORD aWinHttpError = 0, aHttpStatus = 0;
		USHORT aCloseCode = 0;
		const char* aFailureStage = "session";
		auto Failed = [&](const char* theStage) { aFailureStage = theStage; aWinHttpError = GetLastError(); };
		if (!aSession) Failed("session");
		if (aSession)
		{
			WinHttpSetTimeouts(aSession, 3000, 5000, 5000, 5000);
			aConnection = WinHttpConnect(aSession, aHost.c_str(), aUrl.nPort, 0);
			if (!aConnection) Failed("connect");
		}
		if (aConnection)
		{
			aRequest = WinHttpOpenRequest(aConnection, L"GET", aPath.c_str(), nullptr,
				WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, aUrl.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
			if (!aRequest) Failed("request");
		}
		HINTERNET aSocket = nullptr;
		bool aInstalled = false;
		if (aRequest && mRunning)
		{
			if (!WinHttpSetOption(aRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) Failed("upgrade_option");
			else if (!WinHttpSendRequest(aRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) Failed("send_request");
			else if (!WinHttpReceiveResponse(aRequest, nullptr)) Failed("receive_response");
			else
			{
				DWORD aSize = sizeof(aHttpStatus);
				if (!WinHttpQueryHeaders(aRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &aHttpStatus, &aSize, WINHTTP_NO_HEADER_INDEX)) Failed("response_headers");
				else if (aHttpStatus == 101)
				{
					aSocket = WinHttpWebSocketCompleteUpgrade(aRequest, 0);
					if (!aSocket) Failed("complete_upgrade");
				}
				else aFailureStage = "http_status";
			}
		}
		if (aRequest) WinHttpCloseHandle(aRequest);
		if (aSocket && mRunning)
		{
			mSocket = aSocket;
			aInstalled = true;
			mSender = std::thread([this, aSocket]()
			{
				for (;;)
				{
					std::string aText;
					{
						std::unique_lock<std::mutex> aLock(mSignalMutex);
						mSendCv.wait(aLock, [this] { return !mRunning || !mOutgoing.empty(); });
						if (!mRunning) break;
						aText = std::move(mOutgoing.front()); mOutgoing.pop_front();
					}
					const DWORD error = WinHttpWebSocketSend(aSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
						aText.data(), (DWORD)aText.size());
					if (error != NO_ERROR) { if (mRunning) SignalError("send", error, 0, 0, true); break; }
				}
			});
			char aBuffer[8192];
			std::string aMessage;
			while (mRunning)
			{
				DWORD aRead = 0;
				WINHTTP_WEB_SOCKET_BUFFER_TYPE aType;
				const DWORD aError = WinHttpWebSocketReceive(aSocket, aBuffer, sizeof(aBuffer), &aRead, &aType);
				if (aError != NO_ERROR) { aWinHttpError = aError; aFailureStage = "receive"; break; }
				if (aType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
				{
					char aReason[123]; DWORD aReasonBytes = 0;
					aWinHttpError = WinHttpWebSocketQueryCloseStatus(aSocket, &aCloseCode, aReason, sizeof(aReason), &aReasonBytes);
					aFailureStage = "close"; break;
				}
				if ((aType != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE && aType != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) ||
					aMessage.size() + aRead > kMaxSignal) { aFailureStage = "protocol"; break; }
				aMessage.append(aBuffer, aRead);
				if (aType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
				{
					std::lock_guard<std::mutex> aLock(mSignalMutex);
					if (mIncoming.size() >= 256) { aFailureStage = "protocol"; break; }
					mIncoming.push_back(std::move(aMessage)); aMessage.clear();
				}
			}
		}
		if (mRunning) SignalError(aFailureStage, aWinHttpError, aHttpStatus, aCloseCode, aInstalled);
		mRegistered = false;
		mConnecting = false;
		mRunning = false;
		mSendCv.notify_all();
		if (mSender.joinable()) mSender.join();
		if (HINTERNET aOwned = mSocket.exchange(nullptr)) WinHttpCloseHandle(aOwned);
		else if (aSocket && !aInstalled) WinHttpCloseHandle(aSocket);
		if (aConnection) WinHttpCloseHandle(aConnection);
		if (aSession) WinHttpCloseHandle(aSession);
	}

	static void RTC_API Description(int, const char* sdp, const char* type, void* ptr)
	{
		auto* aPeer = (Peer*)ptr;
		aPeer->mOwner->Queue({{"kind", "signal"}, {"to", aPeer->mId}, {"type", "sdp"}, {"sdpType", type}, {"sdp", sdp}});
	}
	static void RTC_API Candidate(int, const char* cand, const char* mid, void* ptr)
	{
		auto* aPeer = (Peer*)ptr;
		aPeer->mOwner->Queue({{"kind", "signal"}, {"to", aPeer->mId}, {"type", "candidate"}, {"candidate", cand}, {"mid", mid}});
	}
	static void RTC_API ConnectionState(int, rtcState state, void* ptr)
	{
		auto* aPeer = (Peer*)ptr;
		if (state == RTC_FAILED || state == RTC_CLOSED)
		{
			PeerFailure expected = PeerFailure::None;
			aPeer->mFailure.compare_exchange_strong(expected, state == RTC_FAILED ? PeerFailure::Ice : PeerFailure::Closed);
			aPeer->mChannel->mCloseRequested = true;
		}
	}
	static void RTC_API Open(int, void* ptr) { ((Peer*)ptr)->mChannel->mOpen = true; }
	static void RTC_API Closed(int, void* ptr)
	{
		auto* aPeer = (Peer*)ptr;
		PeerFailure expected = PeerFailure::None;
		aPeer->mFailure.compare_exchange_strong(expected, PeerFailure::Closed);
		aPeer->mChannel->mOpen = false;
		aPeer->mChannel->mCloseRequested = true;
	}
	static void RTC_API Message(int, const char* data, int size, void* ptr)
	{
		auto* aPeer = (Peer*)ptr;
		auto aState = aPeer->mChannel;
		if (size <= 0 || (size_t)size > kMaxFrame)
		{ aPeer->mFailure = PeerFailure::InvalidData; aState->mCloseRequested = true; return; }
		std::lock_guard<std::mutex> aLock(aState->mMutex);
		if (aState->mIncoming.size() >= 512 || aState->mIncomingBytes + size > kMaxQueuedBytes)
		{
			aPeer->mFailure = PeerFailure::InvalidData; aState->mCloseRequested = true; return;
		}
		aState->mIncoming.emplace_back((const uint8_t*)data, (const uint8_t*)data + size);
		aState->mIncomingBytes += size;
	}
	static void RTC_API DataChannel(int, int dc, void* ptr)
	{
		auto* aPeer = (Peer*)ptr;
		auto aApi = aPeer->mOwner->mApi;
		{
			std::lock_guard<std::mutex> aLock(aPeer->mChannel->mMutex);
			if (aPeer->mChannel->mDc >= 0)
			{
				// Deferring deletion also avoids waiting on callbacks from inside a
				// callback.  The game thread tears down this malformed peer.
				aPeer->mExtraDataChannels.push_back(dc);
				aPeer->mFailure = PeerFailure::InvalidData;
				aPeer->mChannel->mCloseRequested = true;
				return;
			}
			aPeer->mChannel->mDc = dc;
		}
		aApi->rtcSetUserPointer(dc, aPeer);
		aApi->rtcSetMessageCallback(dc, Message);
		aApi->rtcSetClosedCallback(dc, Closed);
		aApi->rtcSetOpenCallback(dc, Open);
	}
	Peer* CreatePeer(const std::string& theId, bool theOffer)
	{
		if (theId.empty() || theId.size() > 64) return nullptr;
		if (auto aFound = mPeers.find(theId); aFound != mPeers.end())
		{
			if (!aFound->second->mDirectoryDeparted && !aFound->second->mChannel->mCloseRequested)
				return aFound->second.get();
			DeletePeer(*aFound->second); mPeers.erase(aFound);
		}
		if (mPeers.size() >= 3) return nullptr;
		auto aPeer = std::make_unique<Peer>();
		aPeer->mOwner = this; aPeer->mId = theId;
		aPeer->mChannel = std::make_shared<NetOnlineChannel::State>();
		aPeer->mChannel->mApi = mApi; aPeer->mChannel->mPeerId = theId;
		const char* aServer = mStun.c_str();
		rtcConfiguration aConfig{};
		aConfig.iceServers = &aServer; aConfig.iceServersCount = 1;
		aConfig.maxMessageSize = 262144;
		aPeer->mPc = mApi->rtcCreatePeerConnection(&aConfig);
		if (aPeer->mPc < 0) return nullptr;
		Peer* aRaw = aPeer.get();
		mPeers.emplace(theId, std::move(aPeer));
		mApi->rtcSetUserPointer(aRaw->mPc, aRaw);
		mApi->rtcSetLocalDescriptionCallback(aRaw->mPc, Description);
		mApi->rtcSetLocalCandidateCallback(aRaw->mPc, Candidate);
		mApi->rtcSetStateChangeCallback(aRaw->mPc, ConnectionState);
		mApi->rtcSetDataChannelCallback(aRaw->mPc, DataChannel);
		if (theOffer)
		{
			int aDc = mApi->rtcCreateDataChannel(aRaw->mPc, "pvz-coop-v1");
			if (aDc < 0) { aRaw->mFailure = PeerFailure::Initialization; aRaw->mChannel->mCloseRequested = true; }
			else DataChannel(aRaw->mPc, aDc, aRaw);
		}
		return aRaw;
	}
	void DeletePeer(Peer& thePeer)
	{
		thePeer.mChannel->mCloseRequested = true;
		thePeer.mChannel->mOpen = false;
		// First quiesce PC callbacks, including any in-flight OnDataChannel.
		// PC deletion does not remove C-API DataChannel registry entries or wait
		// for their callbacks, which also reference this Peer.
		mApi->rtcDeletePeerConnection(thePeer.mPc);
		int aDc = -1;
		std::vector<int> aExtra;
		{
			std::lock_guard<std::mutex> aLock(thePeer.mChannel->mMutex);
			aDc = thePeer.mChannel->mDc;
			thePeer.mChannel->mDc = -1;
			aExtra.swap(thePeer.mExtraDataChannels);
		}
		// Delete outside the mutex so a scheduled Message callback can finish.
		if (aDc >= 0) mApi->rtcDeleteDataChannel(aDc);
		for (int aExtraDc : aExtra) mApi->rtcDeleteDataChannel(aExtraDc);
	}
	void ClearPeers()
	{
		for (auto& aPair : mPeers) DeletePeer(*aPair.second);
		mPeers.clear(); mOpened.clear();
	}
	void Handle(const Json& aMessage)
	{
		const std::string aKind = aMessage.value("kind", "");
		if (aKind == "ready")
		{
			if (!mRunning) return; // A queued reply cannot revive a failed directory connection.
			mRegistered = true; mConnecting = false; Success("Online browser connected");
			Queue({{"kind", "list"}});
		}
		else if (aKind == "rooms")
		{
			std::vector<NetOnlineRoom> aRooms;
			if (!aMessage.contains("rooms") || !aMessage["rooms"].is_array() || aMessage["rooms"].size() > 64) return;
			for (const auto& aValue : aMessage["rooms"])
			{
				NetOnlineRoom aRoom;
				aRoom.mId = aValue.value("id", ""); aRoom.mName = aValue.value("name", "");
				aRoom.mHost = aValue.value("host", ""); aRoom.mBuild = aValue.value("build", "");
				aRoom.mPlayers = aValue.value("players", 0); aRoom.mMaxPlayers = aValue.value("maxPlayers", 0);
				aRoom.mHasPassword = aValue.value("password", false); aRoom.mPlaying = aValue.value("playing", false);
				if (aRoom.mBuild == mBuild && !aRoom.mId.empty() && aRoom.mId.size() <= 64 &&
					aRoom.mName.size() <= 192 && aRoom.mHost.size() <= 64 && aRoom.mMaxPlayers >= 2 &&
					aRoom.mMaxPlayers <= 4 && aRoom.mPlayers >= 1 && aRoom.mPlayers <= aRoom.mMaxPlayers)
					aRooms.push_back(std::move(aRoom));
			}
			std::lock_guard<std::mutex> aLock(mViewMutex); mRooms = std::move(aRooms);
		}
		else if (aKind == "created")
		{
			mRoomId = aMessage.value("room", ""); mHosting = true; Success("Room created");
		}
		else if (aKind == "challenge")
		{
			if (aMessage.value("room", "") != mPendingRoom) return;
			std::string aProof;
			if (aMessage.value("password", false) &&
				!PasswordProof(mPendingPassword, aMessage.value("salt", ""), aProof))
			{
				Error("invalid-password-challenge", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043f\u0440\u0438\u0441\u043b\u0430\u043b \u043d\u0435\u043a\u043e\u0440\u0440\u0435\u043a\u0442\u043d\u044b\u0439 \u0437\u0430\u043f\u0440\u043e\u0441 \u043f\u0440\u043e\u0432\u0435\u0440\u043a\u0438 \u043f\u0430\u0440\u043e\u043b\u044f. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a \u043a\u043e\u043c\u043d\u0430\u0442."));
				std::fill(mPendingPassword.begin(), mPendingPassword.end(), '\0'); mPendingPassword.clear();
				return;
			}
			Queue({{"kind", "join"}, {"room", mPendingRoom}, {"proof", aProof}});
			std::fill(mPendingPassword.begin(), mPendingPassword.end(), '\0'); mPendingPassword.clear();
		}
		else if (aKind == "joined")
		{
			mRoomId = aMessage.value("room", ""); mHosting = false;
			if (CreatePeer(aMessage.value("host", ""), false)) Status("Connecting directly to host...");
			else
			{
				PeerError(PeerFailure::Initialization, false);
				Queue({{"kind", "leave"}}); mRoomId.clear(); mPendingRoom.clear();
			}
		}
		else if (aKind == "peer-joined" && mHosting)
		{
			const std::string aPeer = aMessage.value("peer", "");
			if (!CreatePeer(aPeer, true))
			{
				PeerError(PeerFailure::Initialization, false);
				Queue({{"kind", "drop"}, {"peer", aPeer}});
			}
		}
		else if (aKind == "peer-left")
		{
			auto aPeer = mPeers.find(aMessage.value("peer", ""));
			if (aPeer != mPeers.end())
			{
				aPeer->second->mDirectoryDeparted = true;
				aPeer->second->mChannel->mCloseRequested = true;
			}
		}
		else if (aKind == "signal")
		{
			auto aFound = mPeers.find(aMessage.value("from", ""));
			if (aFound == mPeers.end()) return;
			Peer& aPeer = *aFound->second;
			if (aMessage.value("type", "") == "sdp")
			{
				std::string aSdp = aMessage.value("sdp", ""), aType = aMessage.value("sdpType", "");
				if (aSdp.empty() || aSdp.size() > 32768 || (aType != "offer" && aType != "answer") ||
					mApi->rtcSetRemoteDescription(aPeer.mPc, aSdp.c_str(), aType.c_str()) < 0)
					{ aPeer.mFailure = PeerFailure::InvalidSignal; aPeer.mChannel->mCloseRequested = true; return; }
				aPeer.mRemoteDescription = true;
				for (auto& aCandidate : aPeer.mCandidates)
					mApi->rtcAddRemoteCandidate(aPeer.mPc, aCandidate.first.c_str(), aCandidate.second.c_str());
				aPeer.mCandidates.clear();
			}
			else if (aMessage.value("type", "") == "candidate")
			{
				std::string aCandidate = aMessage.value("candidate", ""), aMid = aMessage.value("mid", "");
				if (aCandidate.empty() || aCandidate.size() > 4096 || aMid.size() > 64) return;
				if (aPeer.mRemoteDescription) mApi->rtcAddRemoteCandidate(aPeer.mPc, aCandidate.c_str(), aMid.c_str());
				else if (aPeer.mCandidates.size() < 128) aPeer.mCandidates.emplace_back(aCandidate, aMid);
			}
		}
		else if (aKind == "closed" || aKind == "kicked")
		{
			bool pending = false;
			for (const auto& aPeer : mPeers)
				pending |= !aPeer.second->mDelivered;
			ClearPeers(); mRoomId.clear(); mHosting = false;
			if (aKind == "kicked")
				Error("host-kicked", OnlineText(u8"\u0425\u043e\u0437\u044f\u0438\u043d \u0438\u0441\u043a\u043b\u044e\u0447\u0438\u043b \u0432\u0430\u0441 \u0438\u0437 \u043a\u043e\u043c\u043d\u0430\u0442\u044b. \u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u0434\u0440\u0443\u0433\u0443\u044e \u043a\u043e\u043c\u043d\u0430\u0442\u0443."));
			else
				Error(pending ? "direct-connection-failed" : "room-closed",
					OnlineText(u8"\u0425\u043e\u0437\u044f\u0438\u043d \u0437\u0430\u043a\u0440\u044b\u043b \u043a\u043e\u043c\u043d\u0430\u0442\u0443. \u041e\u0431\u043d\u043e\u0432\u0438\u0442\u0435 \u0441\u043f\u0438\u0441\u043e\u043a \u0438 \u0432\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u0434\u0440\u0443\u0433\u0443\u044e."));
		}
		else if (aKind == "error")
		{
			mConnecting = false;
			std::string aCode = aMessage.value("code", "error");
			const std::string message = ServiceErrorText(aCode);
			Error(message.empty() ? "service-error" : aCode, message.empty() ?
				OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u0432\u0435\u0440\u043d\u0443\u043b \u043d\u0435\u0438\u0437\u0432\u0435\u0441\u0442\u043d\u0443\u044e \u043e\u0448\u0438\u0431\u043a\u0443. \u041f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435.") : message);
			std::fill(mPendingPassword.begin(), mPendingPassword.end(), '\0'); mPendingPassword.clear();
		}
	}
};

NetOnline::NetOnline() : mImpl(std::make_unique<Impl>()) {}
NetOnline::~NetOnline() { Stop(); }

bool NetOnline::Connect(const std::string& theEndpoint, const std::string& theBuild,
	const std::string& theNickname, const std::string& theStun)
{
	Stop();
	if (theEndpoint.size() > 2048 || theBuild.empty() || theBuild.size() > 128 ||
		theNickname.empty() || theNickname.size() > 64 || theStun.size() > 256 ||
		(theStun.rfind("stun:", 0) != 0 && theStun.rfind("stun://", 0) != 0))
		{ mImpl->Error("invalid-settings", OnlineText(u8"\u041d\u0435\u0432\u0435\u0440\u043d\u044b\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u043e\u043d\u043b\u0430\u0439\u043d-\u0438\u0433\u0440\u044b. \u0412\u043e\u0441\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u044f.")); return false; }
	std::string aUrl = theEndpoint;
	if (aUrl.rfind("wss://", 0) == 0) aUrl.replace(0, 6, "https://");
	else if (aUrl.rfind("ws://", 0) == 0) aUrl.replace(0, 5, "http://");
	if (aUrl.rfind("https://", 0) != 0 && aUrl.rfind("http://127.0.0.1:", 0) != 0 && aUrl.rfind("http://localhost:", 0) != 0)
		{ mImpl->Error("insecure-endpoint", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043a\u043e\u043c\u043d\u0430\u0442 \u0442\u0440\u0435\u0431\u0443\u0435\u0442 \u0437\u0430\u0449\u0438\u0449\u0451\u043d\u043d\u044b\u0439 \u0430\u0434\u0440\u0435\u0441 HTTPS. \u0412\u043e\u0441\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u044f.")); return false; }
	if (aUrl.find('@') != std::string::npos || aUrl.find('#') != std::string::npos || Wide(aUrl).empty())
		{ mImpl->Error("invalid-endpoint", OnlineText(u8"\u041d\u0435\u0432\u0435\u0440\u043d\u044b\u0439 \u0430\u0434\u0440\u0435\u0441 \u0441\u0435\u0440\u0432\u0438\u0441\u0430 \u043a\u043e\u043c\u043d\u0430\u0442. \u0412\u043e\u0441\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u044f.")); return false; }
	mImpl->mApi = LoadRtc();
	if (!mImpl->mApi)
	{ mImpl->Error("runtime-missing", OnlineText(u8"\u041d\u0435 \u0443\u0434\u0430\u043b\u043e\u0441\u044c \u0437\u0430\u0433\u0440\u0443\u0437\u0438\u0442\u044c \u0441\u0435\u0442\u0435\u0432\u043e\u0439 \u043c\u043e\u0434\u0443\u043b\u044c. \u041f\u0435\u0440\u0435\u0443\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u0435 \u043f\u043e\u043b\u043d\u0443\u044e \u0441\u0431\u043e\u0440\u043a\u0443 \u0438\u0433\u0440\u044b.")); return false; }
	mImpl->mBuild = theBuild; mImpl->mNickname = theNickname; mImpl->mStun = theStun;
	mImpl->ErrorCode("");
	mImpl->mRunning = true; mImpl->mConnecting = true; mImpl->Status("Connecting to online browser...");
	mImpl->Queue({{"kind", "hello"}, {"build", theBuild}, {"nickname", theNickname}});
	mImpl->mReceiver = std::thread([this, aUrl] { mImpl->RunSignal(Wide(aUrl)); });
	return true;
}

void NetOnline::Update()
{
	std::deque<std::string> aMessages;
	{
		std::lock_guard<std::mutex> aLock(mImpl->mSignalMutex); aMessages.swap(mImpl->mIncoming);
	}
	for (const auto& aText : aMessages)
	{
		try
		{
			const Json aJson = Json::parse(aText, nullptr, false);
			if (aJson.is_object()) mImpl->Handle(aJson);
			else
			{ mImpl->mConnecting = false; mImpl->Error("signaling-protocol", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043a\u043e\u043c\u043d\u0430\u0442 \u043f\u0440\u0438\u0441\u043b\u0430\u043b \u043d\u0435\u043a\u043e\u0440\u0440\u0435\u043a\u0442\u043d\u044b\u0439 \u043e\u0442\u0432\u0435\u0442. \u041f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435.")); }
		}
		catch (const Json::exception&)
		{ mImpl->mConnecting = false; mImpl->Error("signaling-protocol", OnlineText(u8"\u0421\u0435\u0440\u0432\u0438\u0441 \u043a\u043e\u043c\u043d\u0430\u0442 \u043f\u0440\u0438\u0441\u043b\u0430\u043b \u043d\u0435\u043a\u043e\u0440\u0440\u0435\u043a\u0442\u043d\u044b\u0439 \u043e\u0442\u0432\u0435\u0442. \u041f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435 \u043f\u043e\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435.")); }
	}
	for (auto aIt = mImpl->mPeers.begin(); aIt != mImpl->mPeers.end(); )
	{
		auto& aPeer = *aIt->second;
		if (!aPeer.mDelivered && !aPeer.mChannel->mOpen &&
			std::chrono::steady_clock::now() - aPeer.mCreated > std::chrono::seconds(45))
		{
			Impl::PeerFailure expected = Impl::PeerFailure::None;
			aPeer.mFailure.compare_exchange_strong(expected, Impl::PeerFailure::Timeout);
			aPeer.mChannel->mCloseRequested = true;
		}
		if (aPeer.mChannel->mCloseRequested)
		{
			if (!aPeer.mDelivered && !aPeer.mDirectoryDeparted)
				mImpl->PeerError(aPeer.mFailure, false);
			else if (aPeer.mDelivered && !aPeer.mDirectoryDeparted && aPeer.mFailure != Impl::PeerFailure::None)
				mImpl->PeerError(aPeer.mFailure, true);
			if (!aPeer.mDirectoryDeparted)
			{
				if (mImpl->mHosting) mImpl->Queue({{"kind", "drop"}, {"peer", aPeer.mId}});
				else
				{
					mImpl->Queue({{"kind", "leave"}});
					mImpl->mRoomId.clear(); mImpl->mPendingRoom.clear();
				}
			}
			mImpl->DeletePeer(aPeer); aIt = mImpl->mPeers.erase(aIt); continue;
		}
		if (aPeer.mChannel->mOpen && !aPeer.mDelivered)
		{
			aPeer.mDelivered = true;
			mImpl->mOpened.push_back(std::make_shared<NetOnlineChannel>(aPeer.mChannel));
			mImpl->Success("WebRTC connected directly");
		}
		++aIt;
	}
	if (mImpl->mRegistered && std::chrono::steady_clock::now() - mImpl->mLastKeepalive > std::chrono::seconds(30))
	{
		mImpl->Queue({{"kind", "keepalive"}}); mImpl->mLastKeepalive = std::chrono::steady_clock::now();
	}
}
void NetOnline::RefreshRooms()
{
	if (mImpl->Queue({{"kind", "list"}})) mImpl->Success("Refreshing online rooms...");
}
bool NetOnline::CreateRoom(const std::string& theName, const std::string& thePassword, int theMaxPlayers)
{
	if (!mImpl->mRunning || theName.empty() || theName.size() > 192 || theMaxPlayers < 2 || theMaxPlayers > 4 || thePassword.size() > 256)
		return false;
	uint8_t aSalt[16];
	if (BCryptGenRandom(nullptr, aSalt, sizeof(aSalt), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
	std::string aSaltHex = Hex(aSalt, sizeof(aSalt)), aProof;
	if (!thePassword.empty() && !PasswordProof(thePassword, aSaltHex, aProof)) return false;
	const bool aQueued = mImpl->Queue({{"kind", "create"}, {"name", theName}, {"maxPlayers", theMaxPlayers},
		{"salt", thePassword.empty() ? "" : aSaltHex}, {"proof", aProof}});
	if (aQueued) mImpl->Success("Creating room...");
	return aQueued;
}
bool NetOnline::JoinRoom(const std::string& theRoomId, const std::string& thePassword)
{
	if (!mImpl->mRunning || theRoomId.empty() || theRoomId.size() > 64 || thePassword.size() > 256) return false;
	mImpl->mPendingRoom = theRoomId; mImpl->mPendingPassword = thePassword;
	const bool aQueued = mImpl->Queue({{"kind", "challenge"}, {"room", theRoomId}});
	if (aQueued) mImpl->Success("Joining room...");
	return aQueued;
}
void NetOnline::SetPlaying(bool thePlaying) { mImpl->Queue({{"kind", "playing"}, {"playing", thePlaying}}); }
void NetOnline::KickPeer(const std::string& thePeerId) { mImpl->Queue({{"kind", "kick"}, {"peer", thePeerId}}); }
void NetOnline::LeaveRoom()
{
	mImpl->ErrorCode("");
	mImpl->Queue({{"kind", "leave"}});
	mImpl->ClearPeers(); mImpl->mRoomId.clear(); mImpl->mHosting = false;
}
void NetOnline::Stop()
{
	if (!mImpl) return;
	mImpl->mRunning = false; mImpl->mRegistered = false; mImpl->mConnecting = false; mImpl->mSendCv.notify_all();
	if (HINTERNET aSocket = mImpl->mSocket.exchange(nullptr)) WinHttpCloseHandle(aSocket);
	if (mImpl->mReceiver.joinable()) mImpl->mReceiver.join();
	mImpl->ClearPeers();
	{
		std::lock_guard<std::mutex> aLock(mImpl->mSignalMutex);
		mImpl->mIncoming.clear(); mImpl->mOutgoing.clear();
	}
	mImpl->mRoomId.clear(); mImpl->mHosting = false;
	std::fill(mImpl->mPendingPassword.begin(), mImpl->mPendingPassword.end(), '\0'); mImpl->mPendingPassword.clear();
	std::lock_guard<std::mutex> aLock(mImpl->mViewMutex);
	mImpl->mRooms.clear(); mImpl->mLastErrorCode.clear(); mImpl->mLastErrorMessage.clear();
}
bool NetOnline::IsConnecting() const { return mImpl->mConnecting; }
bool NetOnline::IsConnected() const { return mImpl->mRegistered; }
bool NetOnline::IsHostingRoom() const { return mImpl->mHosting; }
std::string NetOnline::GetRoomId() const { return mImpl->mRoomId; }
std::string NetOnline::GetStatus() const { std::lock_guard<std::mutex> aLock(mImpl->mViewMutex); return mImpl->mStatus; }
std::string NetOnline::GetLastErrorCode() const { std::lock_guard<std::mutex> aLock(mImpl->mViewMutex); return mImpl->mLastErrorCode; }
std::string NetOnline::GetLastErrorMessage() const { std::lock_guard<std::mutex> aLock(mImpl->mViewMutex); return mImpl->mLastErrorMessage; }
std::vector<NetOnlineRoom> NetOnline::GetRooms() const { std::lock_guard<std::mutex> aLock(mImpl->mViewMutex); return mImpl->mRooms; }
std::vector<std::shared_ptr<NetOnlineChannel>> NetOnline::TakeOpenedChannels()
{
	std::vector<std::shared_ptr<NetOnlineChannel>> aResult; aResult.swap(mImpl->mOpened); return aResult;
}
}
