#include "NetCommon.h"

#include <cmath>
#include <cstdarg>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <direct.h>
#include <functional>
#include <mutex>
#include <process.h>
#include <thread>
#include <windows.h>

namespace LawnNet
{

namespace
{
std::mutex gNetTraceMutex;
FILE* gNetTraceFile = nullptr;
std::string gNetConfigPath("coop.ini");
std::string gNetLogDirectory("coop_logs");

void NetTraceTimestamp(char* theBuffer, size_t theSize)
{
	time_t aNow = time(nullptr);
	tm aLocal = {};
	localtime_s(&aLocal, &aNow);
	strftime(theBuffer, theSize, "%Y-%m-%d %H:%M:%S", &aLocal);
}
}

void NetTraceOpen(const char* theRole)
{
	std::lock_guard<std::mutex> aLock(gNetTraceMutex);
	if (gNetTraceFile)
	{
		fprintf(gNetTraceFile, "--- trace replaced by new session ---\n");
		fclose(gNetTraceFile);
		gNetTraceFile = nullptr;
	}
	_mkdir(gNetLogDirectory.c_str());
	time_t aNow = time(nullptr);
	tm aLocal = {};
	localtime_s(&aLocal, &aNow);
	char aPath[512];
	sprintf_s(aPath, "%s\\coop_%04d%02d%02d_%02d%02d%02d_%s_%d.log", gNetLogDirectory.c_str(),
		aLocal.tm_year + 1900, aLocal.tm_mon + 1, aLocal.tm_mday,
		aLocal.tm_hour, aLocal.tm_min, aLocal.tm_sec,
		theRole ? theRole : "unknown", _getpid());
	gNetTraceFile = fopen(aPath, "wb");
	if (!gNetTraceFile)
		return;
	setvbuf(gNetTraceFile, nullptr, _IOLBF, 4096);
	fprintf(gNetTraceFile, "PvZ Co-op diagnostic trace\nrole=%s protocol=%u pid=%d\n",
		theRole ? theRole : "unknown", NET_PROTOCOL_VERSION, _getpid());
}

void NetTraceClose()
{
	std::lock_guard<std::mutex> aLock(gNetTraceMutex);
	if (!gNetTraceFile)
		return;
	fprintf(gNetTraceFile, "--- trace closed ---\n");
	fclose(gNetTraceFile);
	gNetTraceFile = nullptr;
}

void NetTrace(const char* theFormat, ...)
{
	std::lock_guard<std::mutex> aLock(gNetTraceMutex);
	if (!gNetTraceFile)
		return;
	char aTime[32];
	NetTraceTimestamp(aTime, sizeof(aTime));
	size_t aThread = std::hash<std::thread::id>{}(std::this_thread::get_id());
	fprintf(gNetTraceFile, "[%s] [tid=%zu] ", aTime, aThread);
	va_list anArgs;
	va_start(anArgs, theFormat);
	vfprintf(gNetTraceFile, theFormat, anArgs);
	va_end(anArgs);
	fputc('\n', gNetTraceFile);
	fflush(gNetTraceFile);
}

const char* NetMessageName(uint8_t theMsgType)
{
	switch (theMsgType)
	{
	case NETMSG_PING: return "PING"; case NETMSG_PONG: return "PONG";
	case NETMSG_HELLO: return "HELLO"; case NETMSG_WELCOME: return "WELCOME";
	case NETMSG_LOBBY_FULL: return "LOBBY_FULL"; case NETMSG_VERSION_MISMATCH: return "VERSION_MISMATCH";
	case NETMSG_LOBBY_STATE: return "LOBBY_STATE"; case NETMSG_TEAM_SELECT: return "TEAM_SELECT";
	case NETMSG_START_GAME: return "START_GAME"; case NETMSG_SCENE_CMD: return "SCENE_CMD";
	case NETMSG_BACK_TO_MENU: return "BACK_TO_MENU"; case NETMSG_START_PLAYING: return "START_PLAYING";
	case NETMSG_INTRO_STATE: return "INTRO_STATE"; case NETMSG_DAVE_STATE: return "DAVE_STATE";
	case NETMSG_SNAPSHOT: return "SNAPSHOT";
	case NETMSG_BOARD_EVENT: return "BOARD_EVENT"; case NETMSG_INPUT_MOUSE: return "INPUT_MOUSE";
	case NETMSG_INPUT_KEY: return "INPUT_KEY"; case NETMSG_CURSOR: return "CURSOR";
	case NETMSG_PAUSE: return "PAUSE"; case NETMSG_PAUSE_REQ: return "PAUSE_REQ";
	case NETMSG_INPUT_EPOCH: return "INPUT_EPOCH";
	case NETMSG_CHOOSER_OPEN: return "CHOOSER_OPEN"; case NETMSG_SEED_PICK: return "SEED_PICK";
	case NETMSG_SEED_UNPICK: return "SEED_UNPICK"; case NETMSG_CHOOSER_STATE: return "CHOOSER_STATE";
	case NETMSG_CHOOSER_START: return "CHOOSER_START"; case NETMSG_PROFILE_SYNC: return "PROFILE_SYNC";
	case NETMSG_STORE_ACTION: return "STORE_ACTION"; case NETMSG_STORE_SYNC: return "STORE_SYNC";
	case NETMSG_STATS: return "STATS"; case NETMSG_RATE: return "RATE";
	case NETMSG_DEBUG_TEXT: return "DEBUG_TEXT"; default: return "UNKNOWN";
	}
}

int NetApproachPresentationInt(int theCurrent, int theTarget)
{
	const int64_t aDelta = (int64_t)theTarget - (int64_t)theCurrent;
	if (aDelta >= -1 && aDelta <= 1)
		return theTarget;

	int64_t aStep = aDelta * 35 / 100;
	if (aStep == 0)
		aStep = aDelta > 0 ? 1 : -1;

	const int64_t aNext = (int64_t)theCurrent + aStep;
	if ((aDelta > 0 && aNext > theTarget) || (aDelta < 0 && aNext < theTarget))
		return theTarget;
	return (int)aNext;
}

bool NetZombieAnimStateFitsFrames(const NetZombieAnimState& theState, uint32_t theTotalFrames)
{
	if (!theState.mPresent)
		return true;
	if (theState.mFrameCount == 0 || theTotalFrames == 0)
		return false;
	const uint32_t anEndFrame = (uint32_t)theState.mFrameStart + (uint32_t)theState.mFrameCount;
	return anEndFrame <= theTotalFrames;
}

bool NetEncodeZombieAnimState(const NetZombieAnimState& theState, NetBuffer& theBuffer)
{
	NetBuffer aWire;
	if (!theState.mPresent)
	{
		// Keep absence canonical.  This makes stale/corrupt payloads observable
		// instead of silently interpreting their unused bytes later.
		aWire.WriteU16(0);
		aWire.WriteU16(0);
		aWire.WriteU16(0);
		aWire.WriteI16(0);
		aWire.WriteU8(0);
		aWire.WriteU8(0);
		aWire.WriteU8(0);
		theBuffer.WriteBytes(aWire.Data(), aWire.Size());
		return true;
	}

	const uint32_t anEndFrame = (uint32_t)theState.mFrameStart + (uint32_t)theState.mFrameCount;
	if (theState.mFrameCount == 0 || anEndFrame > 65536U ||
		!std::isfinite(theState.mAnimTime) || !std::isfinite(theState.mAnimRate) ||
		theState.mLoopType > 5)
		return false;

	float anAnimTime = theState.mAnimTime;
	if (anAnimTime < 0.0f) anAnimTime = 0.0f;
	if (anAnimTime > 1.0f) anAnimTime = 1.0f;
	const uint16_t anAnimTimeQ = (uint16_t)(anAnimTime * 65535.0f + 0.5f);

	float anAnimRate = theState.mAnimRate;
	if (anAnimRate < -1024.0f) anAnimRate = -1024.0f;
	const float aMaxRate = 32767.0f / 32.0f;
	if (anAnimRate > aMaxRate) anAnimRate = aMaxRate;
	int anAnimRateQ32 = (int)(anAnimRate * 32.0f + (anAnimRate >= 0.0f ? 0.5f : -0.5f));
	if (anAnimRateQ32 < -32768) anAnimRateQ32 = -32768;
	if (anAnimRateQ32 > 32767) anAnimRateQ32 = 32767;

	aWire.WriteU16(theState.mFrameStart);
	aWire.WriteU16(theState.mFrameCount);
	aWire.WriteU16(anAnimTimeQ);
	aWire.WriteI16((int16_t)anAnimRateQ32);
	aWire.WriteU8(theState.mLoopType);
	aWire.WriteU8(theState.mLoopCount);
	aWire.WriteU8((uint8_t)(NET_ZOMBIE_ANIM_PRESENT |
		(theState.mDead ? NET_ZOMBIE_ANIM_DEAD : 0)));
	if (aWire.Size() != NET_ZOMBIE_ANIM_WIRE_BYTES)
		return false;
	theBuffer.WriteBytes(aWire.Data(), aWire.Size());
	return true;
}

bool NetDecodeZombieAnimState(NetBuffer& theBuffer, NetZombieAnimState& theState)
{
	// Decode into locals and roll the cursor back on failure. Copying the whole
	// snapshot buffer for every zombie would make this section quadratic.
	const size_t aStartReadPos = theBuffer.mReadPos;
	if (theBuffer.Remaining() < NET_ZOMBIE_ANIM_WIRE_BYTES)
		return false;

	const uint16_t aFrameStart = theBuffer.ReadU16Or();
	const uint16_t aFrameCount = theBuffer.ReadU16Or();
	const uint16_t anAnimTimeQ = theBuffer.ReadU16Or();
	const int16_t anAnimRateQ = theBuffer.ReadI16Or();
	const uint8_t aLoopType = theBuffer.ReadU8Or();
	const uint8_t aLoopCount = theBuffer.ReadU8Or();
	const uint8_t aFlags = theBuffer.ReadU8Or();
	if ((aFlags & ~(NET_ZOMBIE_ANIM_PRESENT | NET_ZOMBIE_ANIM_DEAD)) != 0)
	{
		theBuffer.mReadPos = aStartReadPos;
		return false;
	}

	const bool aPresent = (aFlags & NET_ZOMBIE_ANIM_PRESENT) != 0;
	const bool aDead = (aFlags & NET_ZOMBIE_ANIM_DEAD) != 0;
	if (!aPresent)
	{
		if (aDead || aFrameStart != 0 || aFrameCount != 0 || anAnimTimeQ != 0 ||
			anAnimRateQ != 0 || aLoopType != 0 || aLoopCount != 0)
		{
			theBuffer.mReadPos = aStartReadPos;
			return false;
		}
	}
	else
	{
		const uint32_t anEndFrame = (uint32_t)aFrameStart + (uint32_t)aFrameCount;
		if (aFrameCount == 0 || anEndFrame > 65536U || aLoopType > 5)
		{
			theBuffer.mReadPos = aStartReadPos;
			return false;
		}
	}

	NetZombieAnimState aDecoded;
	aDecoded.mPresent = aPresent;
	aDecoded.mDead = aDead;
	aDecoded.mFrameStart = aFrameStart;
	aDecoded.mFrameCount = aFrameCount;
	aDecoded.mAnimTime = anAnimTimeQ / 65535.0f;
	aDecoded.mAnimRate = anAnimRateQ / 32.0f;
	aDecoded.mLoopType = aLoopType;
	aDecoded.mLoopCount = aLoopCount;
	theState = aDecoded;
	return true;
}

bool NetEncodeChallengeRuntimeState(const NetChallengeRuntimeState& theState,
	NetBuffer& theBuffer)
{
	if (theState.mProgressMeterWidth > NET_CHALLENGE_PROGRESS_MAX ||
		theState.mChallengePoints < 0 ||
		theState.mChallengePoints > NET_CHALLENGE_POINTS_MAX ||
		theState.mBombsLeft < NET_CHALLENGE_BOMBS_MIN ||
		theState.mBombsLeft > NET_CHALLENGE_BOMBS_MAX ||
		theState.mSlotMachineRollCount > NET_CHALLENGE_SLOT_ROLL_MAX ||
		theState.mTreeOfWisdomTalkIndex > NET_CHALLENGE_TREE_TALK_MAX ||
		theState.mCurrentChallengeRecord < NET_CHALLENGE_RECORD_NONE ||
		theState.mCurrentChallengeRecord > NET_CHALLENGE_RECORD_MAX ||
		(theState.mBeghouledEatenBits & ~NET_CHALLENGE_BEGHOULDED_MASK) != 0)
		return false;

	NetBuffer aWire;
	aWire.WriteU16(theState.mProgressMeterWidth);
	aWire.WriteI32(theState.mChallengePoints);
	aWire.WriteI16(theState.mBombsLeft);
	aWire.WriteU16(theState.mSlotMachineRollCount);
	aWire.WriteU16(theState.mTreeOfWisdomTalkIndex);
	aWire.WriteI32(theState.mCurrentChallengeRecord);
	for (int aByte = 0; aByte < 7; ++aByte)
		aWire.WriteU8((uint8_t)(theState.mBeghouledEatenBits >> (aByte * 8)));
	if (aWire.Size() != NET_CHALLENGE_RUNTIME_WIRE_BYTES)
		return false;
	theBuffer.WriteBytes(aWire.Data(), aWire.Size());
	return true;
}

bool NetDecodeChallengeRuntimeState(NetBuffer& theBuffer,
	NetChallengeRuntimeState& theState)
{
	const size_t aStartReadPos = theBuffer.mReadPos;
	if (theBuffer.Remaining() < NET_CHALLENGE_RUNTIME_WIRE_BYTES)
		return false;

	NetChallengeRuntimeState aDecoded;
	aDecoded.mProgressMeterWidth = theBuffer.ReadU16Or();
	aDecoded.mChallengePoints = theBuffer.ReadI32Or();
	aDecoded.mBombsLeft = theBuffer.ReadI16Or();
	aDecoded.mSlotMachineRollCount = theBuffer.ReadU16Or();
	aDecoded.mTreeOfWisdomTalkIndex = theBuffer.ReadU16Or();
	aDecoded.mCurrentChallengeRecord = theBuffer.ReadI32Or();
	aDecoded.mBeghouledEatenBits = 0;
	for (int aByte = 0; aByte < 7; ++aByte)
		aDecoded.mBeghouledEatenBits |=
			(uint64_t)theBuffer.ReadU8Or() << (aByte * 8);

	if (aDecoded.mProgressMeterWidth > NET_CHALLENGE_PROGRESS_MAX ||
		aDecoded.mChallengePoints < 0 ||
		aDecoded.mChallengePoints > NET_CHALLENGE_POINTS_MAX ||
		aDecoded.mBombsLeft < NET_CHALLENGE_BOMBS_MIN ||
		aDecoded.mBombsLeft > NET_CHALLENGE_BOMBS_MAX ||
		aDecoded.mSlotMachineRollCount > NET_CHALLENGE_SLOT_ROLL_MAX ||
		aDecoded.mTreeOfWisdomTalkIndex > NET_CHALLENGE_TREE_TALK_MAX ||
		aDecoded.mCurrentChallengeRecord < NET_CHALLENGE_RECORD_NONE ||
		aDecoded.mCurrentChallengeRecord > NET_CHALLENGE_RECORD_MAX ||
		(aDecoded.mBeghouledEatenBits & ~NET_CHALLENGE_BEGHOULDED_MASK) != 0)
	{
		theBuffer.mReadPos = aStartReadPos;
		return false;
	}

	theState = aDecoded;
	return true;
}

// -------------------------------------------------------------------------
// Frame encoding
// -------------------------------------------------------------------------
bool NetEncodeFrame(const NetBuffer& thePayload, uint8_t theMsgType, NetBuffer& theFrame)
{
	if (thePayload.Size() > NET_MAX_FRAME || thePayload.Size() > 0xFFFFU)
		return false;
	theFrame.Clear();
	theFrame.Reserve(thePayload.Size() + 3);
	uint16_t aSize = (uint16_t)thePayload.Size();
	theFrame.WriteU16(aSize);
	theFrame.WriteU8(theMsgType);
	theFrame.WriteBytes(thePayload.Data(), thePayload.Size());
	return true;
}

bool NetDecodeFrame(const uint8_t* theData, size_t theSize, uint8_t* theMsgType, NetBuffer& thePayload)
{
	if (theSize < 3)
		return false;
	uint16_t aSize;
	memcpy(&aSize, theData, 2);
	if ((size_t)aSize + 3 != theSize)
		return false;
	*theMsgType = theData[2];
	thePayload.Clear();
	thePayload.WriteBytes(theData + 3, aSize);
	return true;
}

// -------------------------------------------------------------------------
// Config file: simple "key=value" INI parser
// -------------------------------------------------------------------------
static std::string TrimStr(const std::string& theStr)
{
	size_t aStart = theStr.find_first_not_of(" \t\r\n");
	if (aStart == std::string::npos) return "";
	size_t anEnd = theStr.find_last_not_of(" \t\r\n");
	return theStr.substr(aStart, anEnd - aStart + 1);
}

uint32_t NetFnv1a32(const void* theData, size_t theSize)
{
	const uint8_t* aBytes = static_cast<const uint8_t*>(theData);
	uint32_t aHash = 2166136261u;
	for (size_t i = 0; i < theSize; ++i)
	{
		aHash ^= aBytes[i];
		aHash *= 16777619u;
	}
	return aHash;
}

void NetSetRuntimePaths(const char* theConfigPath, const char* theLogDirectory)
{
	if (theConfigPath && theConfigPath[0])
		gNetConfigPath = theConfigPath;
	if (theLogDirectory && theLogDirectory[0])
		gNetLogDirectory = theLogDirectory;
}

namespace
{
bool SmokeError(std::string& theError, const std::string& theMessage)
{
	theError = theMessage;
	return false;
}

bool ParseU32Strict(const std::string& theValue, uint32_t* theOut)
{
	if (!theOut || theValue.empty())
		return false;
	uint64_t aValue = 0;
	for (size_t i = 0; i < theValue.size(); ++i)
	{
		unsigned char aChar = (unsigned char)theValue[i];
		if (!isdigit(aChar))
			return false;
		aValue = aValue * 10u + (uint64_t)(aChar - '0');
		if (aValue > 0xFFFFFFFFULL)
			return false;
	}
	*theOut = (uint32_t)aValue;
	return true;
}

bool IsDriveAbsolutePath(const std::string& thePath)
{
	return thePath.size() >= 3 && isalpha((unsigned char)thePath[0]) &&
		thePath[1] == ':' && (thePath[2] == '\\' || thePath[2] == '/');
}

bool NormalizeSmokePath(const std::string& theValue, std::string& thePath, std::string& theError,
	const char* theField)
{
	if (!IsDriveAbsolutePath(theValue))
		return SmokeError(theError, std::string(theField) + " must be an absolute drive path");
	for (size_t i = 0; i < theValue.size(); ++i)
	{
		unsigned char aChar = (unsigned char)theValue[i];
		if (aChar < 32 || aChar == '"' || aChar == '*' || aChar == '?' ||
			aChar == '<' || aChar == '>' || aChar == '|')
			return SmokeError(theError, std::string(theField) + " contains an unsafe character");
		if (aChar == ':' && i != 1)
			return SmokeError(theError, std::string(theField) + " contains an invalid colon");
	}

	char aFullPath[MAX_PATH];
	DWORD aLength = GetFullPathNameA(theValue.c_str(), MAX_PATH, aFullPath, nullptr);
	if (aLength == 0 || aLength >= MAX_PATH)
		return SmokeError(theError, std::string(theField) + " cannot be canonicalized within MAX_PATH");
	thePath.assign(aFullPath, aLength);
	for (size_t i = 0; i < thePath.size(); ++i)
		if (thePath[i] == '/') thePath[i] = '\\';
	while (thePath.size() > 3 && thePath[thePath.size() - 1] == '\\')
		thePath.erase(thePath.size() - 1);
	return true;
}

std::string SmokeParentPath(const std::string& thePath)
{
	size_t aSlash = thePath.find_last_of("\\/");
	if (aSlash == std::string::npos)
		return "";
	if (aSlash == 2)
		return thePath.substr(0, 3);
	return thePath.substr(0, aSlash);
}

bool SmokePathIsDescendant(const std::string& theRoot, const std::string& thePath)
{
	if (thePath.size() <= theRoot.size() + 1 || _strnicmp(theRoot.c_str(), thePath.c_str(), theRoot.size()) != 0)
		return false;
	return thePath[theRoot.size()] == '\\';
}

bool SmokeExistingDirectory(const std::string& thePath)
{
	DWORD anAttributes = GetFileAttributesA(thePath.c_str());
	return anAttributes != INVALID_FILE_ATTRIBUTES && (anAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
		(anAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool SmokeExistingFile(const std::string& thePath)
{
	DWORD anAttributes = GetFileAttributesA(thePath.c_str());
	return anAttributes != INVALID_FILE_ATTRIBUTES && (anAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
		(anAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool SmokeHasReparseBelowRoot(const std::string& theRoot, const std::string& thePath,
	bool theFinalMayBeMissing)
{
	std::string aCurrent = theFinalMayBeMissing ? SmokeParentPath(thePath) : thePath;
	for (;;)
	{
		DWORD anAttributes = GetFileAttributesA(aCurrent.c_str());
		if (anAttributes == INVALID_FILE_ATTRIBUTES ||
			(anAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			return true;
		if (_stricmp(aCurrent.c_str(), theRoot.c_str()) == 0)
			return false;
		if (!SmokePathIsDescendant(theRoot, aCurrent))
			return true;
		aCurrent = SmokeParentPath(aCurrent);
	}
}

bool SmokeSameOrDescendant(const std::string& theRoot, const std::string& thePath)
{
	return _stricmp(theRoot.c_str(), thePath.c_str()) == 0 ||
		SmokePathIsDescendant(theRoot, thePath);
}

bool SmokeParserSelfTest()
{
	uint32_t aValue = 0;
	static const char aHello[] = "hello";
	return ParseU32Strict("0", &aValue) && aValue == 0 &&
		ParseU32Strict("4294967295", &aValue) && aValue == 0xFFFFFFFFu &&
		!ParseU32Strict("-1", &aValue) && !ParseU32Strict("1x", &aValue) &&
		!ParseU32Strict("4294967296", &aValue) &&
		NetFnv1a32(aHello, sizeof(aHello) - 1) == 0x4F9F2CABu;
}
}

bool NetLoadSmokeManifest(const char* theAbsolutePath, NetSmokeManifest& theManifest, std::string& theError)
{
	theManifest.Reset();
	theError.clear();
	if (!SmokeParserSelfTest())
		return SmokeError(theError, "internal smoke parser self-test failed");
	if (!theAbsolutePath || !theAbsolutePath[0])
		return SmokeError(theError, "smoke manifest path is empty");

	std::string aManifestPath;
	if (!NormalizeSmokePath(theAbsolutePath, aManifestPath, theError, "manifest path"))
		return false;
	if (!SmokeExistingFile(aManifestPath))
		return SmokeError(theError, "smoke manifest is not an existing regular file");

	FILE* aFile = nullptr;
	if (fopen_s(&aFile, aManifestPath.c_str(), "rb") != 0 || !aFile)
		return SmokeError(theError, "smoke manifest cannot be opened");

	std::map<std::string, std::string> aValues;
	char aLine[2048];
	int aLineNumber = 0;
	bool aParseOk = true;
	while (fgets(aLine, sizeof(aLine), aFile))
	{
		++aLineNumber;
		size_t aLineLength = strlen(aLine);
		if (aLineLength == sizeof(aLine) - 1 && aLine[aLineLength - 1] != '\n' && !feof(aFile))
		{
			theError = "smoke manifest line is too long at line " + std::to_string(aLineNumber);
			aParseOk = false;
			break;
		}
		std::string aText(aLine, aLineLength);
		if (aLineNumber == 1 && aText.size() >= 3 &&
			(uint8_t)aText[0] == 0xEF && (uint8_t)aText[1] == 0xBB && (uint8_t)aText[2] == 0xBF)
			aText.erase(0, 3);
		aText = TrimStr(aText);
		if (aText.empty() || aText[0] == '#' || aText[0] == ';')
			continue;
		if (aText[0] == '[')
		{
			theError = "smoke manifest sections are not allowed at line " + std::to_string(aLineNumber);
			aParseOk = false;
			break;
		}
		size_t anEquals = aText.find('=');
		if (anEquals == std::string::npos)
		{
			theError = "smoke manifest entry lacks '=' at line " + std::to_string(aLineNumber);
			aParseOk = false;
			break;
		}
		std::string aKey = TrimStr(aText.substr(0, anEquals));
		std::string aValue = TrimStr(aText.substr(anEquals + 1));
		if (aKey.empty() || aValue.empty())
		{
			theError = "smoke manifest key/value is empty at line " + std::to_string(aLineNumber);
			aParseOk = false;
			break;
		}
		if (aValues.find(aKey) != aValues.end())
		{
			theError = "duplicate smoke manifest key '" + aKey + "'";
			aParseOk = false;
			break;
		}
		aValues[aKey] = aValue;
		if (aValues.size() > 16)
		{
			theError = "smoke manifest contains too many fields";
			aParseOk = false;
			break;
		}
	}
	if (ferror(aFile))
	{
		theError = "error while reading smoke manifest";
		aParseOk = false;
	}
	fclose(aFile);
	if (!aParseOk)
		return false;

	static const char* aRequiredKeys[] = {
		"schema", "role", "endpoint", "port", "nickname", "mode", "level", "seed",
		"appdata_dir", "config_path", "log_dir", "result_path", "timeout_ms",
		"minimum_snapshots", "skip_intro", "close_on_result"
	};
	// The schema contains sixteen exact fields.  Unknown, missing and duplicate
	// entries are all fatal so a typo can never fall back to a normal profile.
	if (aValues.size() != sizeof(aRequiredKeys) / sizeof(aRequiredKeys[0]))
		return SmokeError(theError, "smoke manifest must contain exactly 16 fields");
	for (std::map<std::string, std::string>::const_iterator anIt = aValues.begin(); anIt != aValues.end(); ++anIt)
	{
		bool aKnown = false;
		for (size_t i = 0; i < sizeof(aRequiredKeys) / sizeof(aRequiredKeys[0]); ++i)
			if (anIt->first == aRequiredKeys[i]) { aKnown = true; break; }
		if (!aKnown)
			return SmokeError(theError, "unknown smoke manifest key '" + anIt->first + "'");
	}
	for (size_t i = 0; i < sizeof(aRequiredKeys) / sizeof(aRequiredKeys[0]); ++i)
		if (aValues.find(aRequiredKeys[i]) == aValues.end())
			return SmokeError(theError, std::string("smoke manifest is missing '") + aRequiredKeys[i] + "'");

	uint32_t aSchema = 0, aPort = 0, aLevel = 0, aSeed = 0, aTimeout = 0,
		aMinimumSnapshots = 0, aSkipIntro = 0, aCloseOnResult = 0;
	if (!ParseU32Strict(aValues["schema"], &aSchema) || aSchema != 1)
		return SmokeError(theError, "schema must equal 1");
	if (aValues["role"] == "host") theManifest.mRole = NETSMOKE_HOST;
	else if (aValues["role"] == "client") theManifest.mRole = NETSMOKE_CLIENT;
	else return SmokeError(theError, "role must be exactly host or client");
	if (aValues["endpoint"] != "127.0.0.1")
		return SmokeError(theError, "endpoint must be exactly 127.0.0.1");
	if (!ParseU32Strict(aValues["port"], &aPort) || aPort == 0 || aPort > 65535)
		return SmokeError(theError, "port must be in 1..65535");
	const std::string& aNickname = aValues["nickname"];
	if (aNickname.empty() || aNickname.size() > NET_MAX_NICK)
		return SmokeError(theError, "nickname length is invalid");
	for (size_t i = 0; i < aNickname.size(); ++i)
		if (!isalnum((unsigned char)aNickname[i]) && aNickname[i] != '_' && aNickname[i] != '-')
			return SmokeError(theError, "nickname contains an invalid character");
	if (!ParseU32Strict(aValues["level"], &aLevel) || aLevel == 0 || aLevel > 999)
		return SmokeError(theError, "level must be in 1..999");
	NetSmokeScenarioDefinition aScenario;
	if (!NetResolveSmokeScenario(aValues["mode"], (int)aLevel, aScenario, theError))
		return false;
	if (!ParseU32Strict(aValues["seed"], &aSeed))
		return SmokeError(theError, "seed must be an unsigned 32-bit integer");
	if (!ParseU32Strict(aValues["timeout_ms"], &aTimeout) || aTimeout < 10000 || aTimeout > 3600000)
		return SmokeError(theError, "timeout_ms must be in 10000..3600000");
	if (!ParseU32Strict(aValues["minimum_snapshots"], &aMinimumSnapshots) ||
		aMinimumSnapshots == 0 || aMinimumSnapshots > 1000)
		return SmokeError(theError, "minimum_snapshots must be in 1..1000");
	if (!ParseU32Strict(aValues["skip_intro"], &aSkipIntro) || aSkipIntro > 1)
		return SmokeError(theError, "skip_intro must be 0 or 1");
	if (!ParseU32Strict(aValues["close_on_result"], &aCloseOnResult) || aCloseOnResult > 1)
		return SmokeError(theError, "close_on_result must be 0 or 1");

	std::string anAppDataDir, aConfigPath, aLogDir, aResultPath;
	if (!NormalizeSmokePath(aValues["appdata_dir"], anAppDataDir, theError, "appdata_dir") ||
		!NormalizeSmokePath(aValues["config_path"], aConfigPath, theError, "config_path") ||
		!NormalizeSmokePath(aValues["log_dir"], aLogDir, theError, "log_dir") ||
		!NormalizeSmokePath(aValues["result_path"], aResultPath, theError, "result_path"))
		return false;

	std::string aRunRoleRoot = SmokeParentPath(aManifestPath);
	if (aRunRoleRoot.empty() || !SmokeExistingDirectory(aRunRoleRoot))
		return SmokeError(theError, "manifest parent directory is unavailable");
	if (!SmokePathIsDescendant(aRunRoleRoot, anAppDataDir) ||
		!SmokePathIsDescendant(aRunRoleRoot, aConfigPath) ||
		!SmokePathIsDescendant(aRunRoleRoot, aLogDir) ||
		!SmokePathIsDescendant(aRunRoleRoot, aResultPath))
		return SmokeError(theError, "all smoke paths must stay below the manifest directory");
	if (!SmokeExistingDirectory(anAppDataDir) || !SmokeExistingDirectory(aLogDir))
		return SmokeError(theError, "appdata_dir and log_dir must be existing regular directories");
	if (!SmokeExistingFile(aConfigPath))
		return SmokeError(theError, "config_path must be an existing regular file");
	if (!SmokeExistingDirectory(SmokeParentPath(aResultPath)))
		return SmokeError(theError, "result_path parent must be an existing regular directory");
	if (SmokeHasReparseBelowRoot(aRunRoleRoot, anAppDataDir, false) ||
		SmokeHasReparseBelowRoot(aRunRoleRoot, aLogDir, false) ||
		SmokeHasReparseBelowRoot(aRunRoleRoot, aConfigPath, false) ||
		SmokeHasReparseBelowRoot(aRunRoleRoot, aResultPath, true))
		return SmokeError(theError, "smoke paths may not traverse reparse points");
	if (aResultPath.size() > MAX_PATH - 32)
		return SmokeError(theError, "result_path is too long for atomic temporary publication");
	if (SmokeSameOrDescendant(anAppDataDir, aLogDir) ||
		SmokeSameOrDescendant(aLogDir, anAppDataDir) ||
		SmokeSameOrDescendant(anAppDataDir, aConfigPath) ||
		SmokeSameOrDescendant(anAppDataDir, aResultPath) ||
		SmokeSameOrDescendant(aLogDir, aConfigPath) ||
		SmokeSameOrDescendant(aLogDir, aResultPath) ||
		_stricmp(aConfigPath.c_str(), aResultPath.c_str()) == 0 ||
		_stricmp(aManifestPath.c_str(), aResultPath.c_str()) == 0)
		return SmokeError(theError, "smoke paths overlap");
	DWORD aResultAttributes = GetFileAttributesA(aResultPath.c_str());
	DWORD aResultError = aResultAttributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
	if (aResultAttributes != INVALID_FILE_ATTRIBUTES || aResultError != ERROR_FILE_NOT_FOUND)
		return SmokeError(theError, "result_path must not already exist");

	char anEnvironmentRole[32] = {};
	DWORD anEnvironmentRoleLength = GetEnvironmentVariableA("PVZ_COOP_SMOKE_ROLE", anEnvironmentRole, sizeof(anEnvironmentRole));
	if (anEnvironmentRoleLength > 0 && (anEnvironmentRoleLength >= sizeof(anEnvironmentRole) ||
		aValues["role"] != anEnvironmentRole))
		return SmokeError(theError, "PVZ_COOP_SMOKE_ROLE does not match manifest role");

	theManifest.mEnabled = true;
	theManifest.mManifestPath = aManifestPath;
	theManifest.mEndpoint = aValues["endpoint"];
	theManifest.mPort = (uint16_t)aPort;
	theManifest.mNickname = aNickname;
	theManifest.mMode = aValues["mode"];
	theManifest.mLevel = (int)aLevel;
	theManifest.mScenario = aScenario.mId;
	theManifest.mGameMode = aScenario.mGameMode;
	theManifest.mSeed = aSeed;
	theManifest.mAppDataDir = anAppDataDir;
	theManifest.mConfigPath = aConfigPath;
	theManifest.mLogDir = aLogDir;
	theManifest.mResultPath = aResultPath;
	theManifest.mTimeoutMs = aTimeout;
	theManifest.mMinimumSnapshots = aMinimumSnapshots;
	theManifest.mSkipIntro = aSkipIntro != 0;
	theManifest.mCloseOnResult = aCloseOnResult != 0;
	return true;
}

bool NetLoadConfig(NetConfig& theConfig)
{
	theConfig.Reset();
	FILE* aFile = fopen(gNetConfigPath.c_str(), "rb");
	if (!aFile)
		return false;

	char aLine[256];
	while (fgets(aLine, sizeof(aLine), aFile))
	{
		std::string aStr = TrimStr(aLine);
		if (aStr.empty() || aStr[0] == '#' || aStr[0] == ';')
			continue;
		size_t anEq = aStr.find('=');
		if (anEq == std::string::npos)
			continue;
		std::string aKey = TrimStr(aStr.substr(0, anEq));
		std::string aValue = TrimStr(aStr.substr(anEq + 1));

		if (aKey == "nickname" && !aValue.empty())
		{
			strncpy_s(theConfig.mNick, aValue.c_str(), NET_MAX_NICK);
			theConfig.mNick[NET_MAX_NICK] = 0;
		}
		else if (aKey == "unlock_all_minigames")
			theConfig.mUnlockAllMinigames = (aValue == "1" || aValue == "true" || aValue == "yes");
		else if (aKey == "enable_coop")
			theConfig.mEnableCoop = (aValue == "1" || aValue == "true" || aValue == "yes");
		else if (aKey == "base_snapshot_rate")
			theConfig.mBaseSnapshotHz = atoi(aValue.c_str());
		else if (aKey == "window_width")
			theConfig.mWindowWidth = atoi(aValue.c_str());
		else if (aKey == "window_height")
			theConfig.mWindowHeight = atoi(aValue.c_str());
		else if (aKey == "borderless_fullscreen")
			theConfig.mBorderlessFullscreen = (aValue == "1" || aValue == "true" || aValue == "yes");
	}
	fclose(aFile);

	if (theConfig.mBaseSnapshotHz < NET_SNAPSHOT_HZ_MIN) theConfig.mBaseSnapshotHz = NET_SNAPSHOT_HZ_MIN;
	if (theConfig.mBaseSnapshotHz > NET_SNAPSHOT_HZ_MAX) theConfig.mBaseSnapshotHz = NET_SNAPSHOT_HZ_MAX;
	return true;
}

bool NetSaveConfig(const NetConfig& theConfig)
{
	FILE* aFile = fopen(gNetConfigPath.c_str(), "wb");
	if (!aFile)
		return false;
	fprintf(aFile, "# PvZ Co-op configuration\n");
	fprintf(aFile, "# nickname - your in-game name (max %d chars)\n", NET_MAX_NICK);
	fprintf(aFile, "# unlock_all_minigames - 1 = all minigames/modes unlocked without adventure progress\n");
	fprintf(aFile, "# base_snapshot_rate - initial sync rate (15..30), adapts automatically\n\n");
	fprintf(aFile, "nickname=%s\n", theConfig.mNick);
	fprintf(aFile, "unlock_all_minigames=%d\n", theConfig.mUnlockAllMinigames ? 1 : 0);
	fprintf(aFile, "enable_coop=%d\n", theConfig.mEnableCoop ? 1 : 0);
	fprintf(aFile, "base_snapshot_rate=%d\n", theConfig.mBaseSnapshotHz);
	fprintf(aFile, "window_width=%d\n", theConfig.mWindowWidth);
	fprintf(aFile, "window_height=%d\n", theConfig.mWindowHeight);
	fprintf(aFile, "borderless_fullscreen=%d\n", theConfig.mBorderlessFullscreen ? 1 : 0);
	fclose(aFile);
	return true;
}

} // namespace LawnNet
