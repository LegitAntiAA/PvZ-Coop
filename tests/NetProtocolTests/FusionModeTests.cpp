#include "../../Lawn/Net/NetCommon.h"
#include "FusionModeTests.h"
#include <cstdio>

FusionModeTestResult RunFusionModeTests()
{
	using namespace LawnFusion;
	using namespace LawnNet;
	FusionModeTestResult result{};
	auto Check = [&](bool value, const char* text)
	{
		++result.mChecks;
		if (!value) { ++result.mFailures; std::printf("FAIL content mode: %s\n", text); }
	};
	Check(IsValidContentMode(ContentMode::Classic) && IsValidContentMode(ContentMode::Fusion), "known modes");
	Check(std::strcmp(ContentModeName(ContentMode::Classic), "Classic") == 0 &&
		std::strcmp(ContentModeName(ContentMode::Fusion), "Fusion") == 0, "mode names");
	for (int value = 0; value < 256; ++value)
	{
		NetBuffer buffer;
		buffer.WriteU8(99); buffer.WriteU8((uint8_t)value); buffer.mReadPos = 1;
		ContentMode mode = ContentMode::Fusion;
		const bool accepted = DecodeContentMode(buffer, mode);
		Check(accepted == (value <= 1), "only two wire mode values");
		Check(accepted ? (buffer.mReadPos == 2 && mode == (ContentMode)value) :
			(buffer.mReadPos == 1 && mode == ContentMode::Fusion), "mode decode commits atomically");
	}
	{
		NetBuffer buffer;
		ContentMode mode = ContentMode::Fusion;
		Check(!DecodeContentMode(buffer, mode) && buffer.mReadPos == 0 && mode == ContentMode::Fusion,
			"missing mode does not become Classic");
		buffer.WriteU8(77);
		Check(!EncodeContentMode(buffer, (ContentMode)255) && buffer.Size() == 1, "invalid mode not encoded");
	}

	for (ContentMode mode : {ContentMode::Classic, ContentMode::Fusion})
	{
		NetWelcomeState welcome;
		welcome.mSlot = 3; welcome.mHostNick = "Host"; welcome.mContentMode = mode;
		NetBuffer wire;
		Check(NetEncodeWelcomeState(welcome, wire), "welcome encodes");
		NetWelcomeState decoded;
		Check(NetDecodeWelcomeState(wire, decoded) && wire.Remaining() == 0 && decoded.mSlot == 3 &&
			decoded.mHostNick == "Host" && decoded.mContentMode == mode, "welcome round trip");
		auto RejectWelcome = [&](NetBuffer bad)
		{
			bad.mReadPos = 0;
			NetBuffer offset; offset.WriteU8(77);
			if (bad.Size()) offset.WriteBytes(bad.Data(), bad.Size());
			offset.mReadPos = 1;
			NetWelcomeState unchanged;
			unchanged.mSlot = 2; unchanged.mHostNick = "Unchanged"; unchanged.mContentMode = ContentMode::Fusion;
			Check(!NetDecodeWelcomeState(offset, unchanged) && offset.mReadPos == 1 && unchanged.mSlot == 2 &&
				unchanged.mHostNick == "Unchanged" && unchanged.mContentMode == ContentMode::Fusion,
				"invalid welcome preserves input and output");
		};
		for (size_t bytes = 0; bytes < wire.Size(); ++bytes)
		{
			NetBuffer truncated; truncated.WriteBytes(wire.Data(), bytes); RejectWelcome(truncated);
		}
		NetBuffer bad = wire; bad.WriteU8(0); RejectWelcome(bad);
		bad = wire; bad.mData.back() = 255; RejectWelcome(bad);
		bad = wire; bad.mData[0] = 0; RejectWelcome(bad);
		bad = wire; bad.mData[0] = NET_MAX_PLAYERS; RejectWelcome(bad);
		bad = wire; bad.mData[3] = 0; RejectWelcome(bad); // embedded NUL inside the encoded name
		bad.Clear(); bad.WriteU8(1); bad.WriteString(""); EncodeContentMode(bad, mode); RejectWelcome(bad);
		bad.Clear(); bad.WriteU8(1); bad.WriteString("NameLongerThanSixteenCharacters");
		EncodeContentMode(bad, mode); RejectWelcome(bad);
		NetWelcomeState invalid = welcome; invalid.mSlot = 0;
		NetBuffer output; output.WriteU8(66);
		Check(!NetEncodeWelcomeState(invalid, output) && output.Size() == 1, "invalid welcome encode rolls back");
	}

	for (ContentMode mode : {ContentMode::Classic, ContentMode::Fusion})
	{
		NetLobbyState lobby;
		for (int i = 0; i < NET_MAX_PLAYERS; ++i) lobby.mPlayers[i].mSlot = (uint8_t)i;
		lobby.mPlayers[0].mActive = true; strcpy_s(lobby.mPlayers[0].mNick, "Host");
		lobby.mPlayers[0].mTeam = NETTEAM_PLANTS; lobby.mPlayers[0].mReady = 1;
		lobby.mPlayers[1].mActive = true; strcpy_s(lobby.mPlayers[1].mNick, "Player");
		lobby.mPlayers[1].mSpectator = true; lobby.mPlayers[1].mPing = 123;
		lobby.mDifficulty = 10; lobby.mContentMode = mode;
		NetBuffer wire;
		Check(NetEncodeLobbyState(lobby, wire), "lobby encodes");
		NetLobbyState decoded;
		Check(NetDecodeLobbyState(wire, decoded) && wire.Remaining() == 0 && decoded.mContentMode == mode &&
			decoded.mDifficulty == 10 && decoded.mPlayers[0].mReady == 1 && decoded.mPlayers[1].mSpectator &&
			decoded.mPlayers[1].mPing == 123 && !decoded.mPlayers[2].mActive, "lobby round trip preserves co-op settings");
		auto RejectLobby = [&](NetBuffer bad)
		{
			NetBuffer offset; offset.WriteU8(77);
			if (bad.Size()) offset.WriteBytes(bad.Data(), bad.Size());
			offset.mReadPos = 1;
			NetLobbyState unchanged;
			unchanged.mDifficulty = 7; unchanged.mContentMode = ContentMode::Fusion;
			strcpy_s(unchanged.mPlayers[0].mNick, "Unchanged");
			Check(!NetDecodeLobbyState(offset, unchanged) && offset.mReadPos == 1 && unchanged.mDifficulty == 7 &&
				unchanged.mContentMode == ContentMode::Fusion && strcmp(unchanged.mPlayers[0].mNick, "Unchanged") == 0,
				"invalid lobby preserves roster/difficulty/mode");
		};
		for (size_t bytes = 0; bytes < wire.Size(); ++bytes)
		{
			NetBuffer truncated; truncated.WriteBytes(wire.Data(), bytes); RejectLobby(truncated);
		}
		NetBuffer bad = wire; bad.WriteU8(0); RejectLobby(bad);
		bad = wire; bad.mData.back() = 2; RejectLobby(bad);
		bad = wire; bad.mData[bad.Size() - 2] = 0; RejectLobby(bad);
		bad = wire; bad.mData[bad.Size() - 2] = 11; RejectLobby(bad);
		bad = wire; bad.mData[0] = 2; RejectLobby(bad);
		bad = wire; bad.mData[1] = 1; RejectLobby(bad);
		const size_t teamOffset = 3 + strlen("Host") + sizeof(uint16_t);
		bad = wire; bad.mData[teamOffset] = 255; RejectLobby(bad);
		bad = wire; bad.mData[teamOffset + 1] = 2; RejectLobby(bad);
		bad = wire; bad.mData[teamOffset + 2] = 2; RejectLobby(bad);
		NetBuffer output; output.WriteU8(66); lobby.mContentMode = (ContentMode)2;
		Check(!NetEncodeLobbyState(lobby, output) && output.Size() == 1, "invalid lobby encode rolls back");
	}

	const int modeCount = 10, towerMode = 7;
	for (ContentMode content : {ContentMode::Classic, ContentMode::Fusion})
		for (bool tower : {false, true})
		{
			NetStartGameState start;
			start.mContentMode = content; start.mGameMode = tower ? towerMode : 0;
			start.mContentEpoch = content == ContentMode::Fusion ? 123 : 0;
			start.mLevel = 49; start.mPlayerCount = 4; start.mBoardSeed = 0x12345678;
			if (tower) start.mTDPath = {2, 8, 2, 7};
			NetBuffer wire;
			Check(NetEncodeStartGame(start, wire, modeCount, towerMode), "start encodes");
			Check(wire.Size() == 12 + start.mTDPath.size() + (content == ContentMode::Fusion ? 5 : 0) && wire.mData[0] == (uint8_t)content,
				"start has explicit content mode prefix");
			NetStartGameState decoded;
			Check(NetDecodeStartGame(wire, decoded, modeCount, towerMode) && wire.Remaining() == 0 &&
				decoded.mContentMode == content && decoded.mContentEpoch == start.mContentEpoch && decoded.mGameMode == start.mGameMode && decoded.mLevel == 49 &&
				decoded.mPlayerCount == 4 && decoded.mBoardSeed == 0x12345678 && decoded.mTDPath == start.mTDPath,
				"start round trip preserves seed/roster/path");
			auto RejectStart = [&](NetBuffer bad)
			{
				NetBuffer offset; offset.WriteU8(77);
				if (bad.Size()) offset.WriteBytes(bad.Data(), bad.Size());
				offset.mReadPos = 1;
				NetStartGameState unchanged;
				unchanged.mContentMode = ContentMode::Fusion; unchanged.mLevel = 999; unchanged.mTDPath = {1, 2};
				Check(!NetDecodeStartGame(offset, unchanged, modeCount, towerMode) && offset.mReadPos == 1 &&
					unchanged.mContentMode == ContentMode::Fusion && unchanged.mLevel == 999 &&
					unchanged.mTDPath == std::vector<uint8_t>({1, 2}), "invalid start preserves descriptor/read position");
			};
			for (size_t bytes = 0; bytes < wire.Size(); ++bytes)
			{
				NetBuffer truncated; truncated.WriteBytes(wire.Data(), bytes); RejectStart(truncated);
			}
			NetBuffer bad = wire; bad.WriteU8(0); RejectStart(bad);
			bad = wire; bad.mData[0] = 255; RejectStart(bad);
			bad = wire; bad.mData[1] = modeCount; RejectStart(bad);
			bad = wire; bad.mData[6] = 0; RejectStart(bad);
			bad = wire; bad.mData[6] = NET_MAX_PLAYERS + 1; RejectStart(bad);
			bad = wire; bad.mData[1] = tower ? 0 : towerMode; RejectStart(bad);
			bad = wire; bad.mData[11] = 255; RejectStart(bad);
			NetBuffer output; output.WriteU8(66); start.mPlayerCount = 0;
			Check(!NetEncodeStartGame(start, output, modeCount, towerMode) && output.Size() == 1,
				"invalid start encode rolls back");
		}
	return result;
}
