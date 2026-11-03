#include "NetCommon.h"

namespace LawnNet
{

// -------------------------------------------------------------------------
// Frame encoding
// -------------------------------------------------------------------------
bool NetEncodeFrame(const NetBuffer& thePayload, uint8_t theMsgType, NetBuffer& theFrame)
{
	if (thePayload.Size() > NET_MAX_FRAME)
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
	fclose(aFile);
	return true;
}

} // namespace LawnNet
