#include "NetCommon.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <direct.h>
#include <functional>
#include <mutex>
#include <process.h>
#include <thread>

namespace LawnNet
{

namespace
{
std::mutex gNetTraceMutex;
FILE* gNetTraceFile = nullptr;

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
	_mkdir("coop_logs");
	time_t aNow = time(nullptr);
	tm aLocal = {};
	localtime_s(&aLocal, &aNow);
	char aPath[260];
	sprintf_s(aPath, "coop_logs\\coop_%04d%02d%02d_%02d%02d%02d_%s_%d.log",
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
	case NETMSG_INTRO_STATE: return "INTRO_STATE";
	case NETMSG_SNAPSHOT: return "SNAPSHOT";
	case NETMSG_BOARD_EVENT: return "BOARD_EVENT"; case NETMSG_INPUT_MOUSE: return "INPUT_MOUSE";
	case NETMSG_INPUT_KEY: return "INPUT_KEY"; case NETMSG_CURSOR: return "CURSOR";
	case NETMSG_PAUSE: return "PAUSE"; case NETMSG_PAUSE_REQ: return "PAUSE_REQ";
	case NETMSG_CHOOSER_OPEN: return "CHOOSER_OPEN"; case NETMSG_SEED_PICK: return "SEED_PICK";
	case NETMSG_SEED_UNPICK: return "SEED_UNPICK"; case NETMSG_CHOOSER_STATE: return "CHOOSER_STATE";
	case NETMSG_CHOOSER_START: return "CHOOSER_START"; case NETMSG_PROFILE_SYNC: return "PROFILE_SYNC";
	case NETMSG_STORE_ACTION: return "STORE_ACTION"; case NETMSG_STORE_SYNC: return "STORE_SYNC";
	case NETMSG_STATS: return "STATS"; case NETMSG_RATE: return "RATE";
	case NETMSG_DEBUG_TEXT: return "DEBUG_TEXT"; default: return "UNKNOWN";
	}
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

bool NetLoadConfig(NetConfig& theConfig)
{
	theConfig.Reset();
	FILE* aFile = fopen("coop.ini", "rb");
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
	FILE* aFile = fopen("coop.ini", "wb");
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
