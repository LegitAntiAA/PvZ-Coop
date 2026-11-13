#include "../../Lawn/Net/NetCommon.h"
#include "../../Lawn/Net/NetCursorTarget.h"
#include "NetCursorTargetTests.h"
#include <cstdio>

NetCursorTargetTestResult RunNetCursorTargetTests()
{
	using namespace LawnNet;
	NetCursorTargetTestResult result{};
	auto Check = [&](bool value, const char* text)
	{
		++result.mChecks;
		if (!value) { ++result.mFailures; std::printf("FAIL cursor target: %s\n",text); }
	};
	const uint8_t seedCount = 56, imitater = 48;
	NetCursorTarget screen;
	NetCursorTarget packet; packet.mKind = NETCURSOR_PACKET; packet.mPacketId = 1234;
	NetCursorTarget catalog; catalog.mKind = NETCURSOR_CHOOSER_CATALOG; catalog.mSeed = 1;
	NetCursorTarget selected; selected.mKind = NETCURSOR_CHOOSER_BANK; selected.mOwner = 3;
	selected.mSeed = imitater; selected.mImitater = 1;
	for (const NetCursorTarget& target : {screen,packet,catalog,selected})
	{
		NetBuffer buffer; NetEncodeCursorTarget(target,buffer);
		Check(buffer.Size() == NET_CURSOR_TARGET_BYTES,"fixed wire size");
		NetCursorTarget decoded;
		Check(NetDecodeCursorTarget(buffer,decoded,seedCount,imitater) && buffer.Remaining() == 0 &&
			decoded.mKind == target.mKind && decoded.mPacketId == target.mPacketId &&
			decoded.mOwner == target.mOwner && decoded.mSeed == target.mSeed &&
			decoded.mImitater == target.mImitater,"identity round trip");
		for (size_t bytes = 0; bytes < NET_CURSOR_TARGET_BYTES; ++bytes)
		{
			NetBuffer truncated; truncated.WriteU8(7);
			truncated.WriteBytes(buffer.Data(),bytes); truncated.mReadPos = 1;
			NetCursorTarget unchanged = selected;
			Check(!NetDecodeCursorTarget(truncated,unchanged,seedCount,imitater) && truncated.mReadPos == 1 &&
				unchanged.mKind == selected.mKind && unchanged.mOwner == selected.mOwner &&
				unchanged.mSeed == selected.mSeed && unchanged.mImitater == selected.mImitater,
				"truncation rolls back input and output");
		}
	}
	auto Reject = [&](NetCursorTarget invalid)
	{
		NetBuffer buffer; NetEncodeCursorTarget(invalid,buffer);
		NetCursorTarget unchanged = packet;
		Check(!NetDecodeCursorTarget(buffer,unchanged,seedCount,imitater) && buffer.mReadPos == 0 &&
			unchanged.mKind == NETCURSOR_PACKET && unchanged.mPacketId == packet.mPacketId,
			"malformed identity rejected transactionally");
	};
	NetCursorTarget invalid = screen; invalid.mKind = 4; Reject(invalid);
	invalid = packet; invalid.mPacketId = 0; Reject(invalid);
	invalid = packet; invalid.mSeed = 0; Reject(invalid);
	invalid = screen; invalid.mPacketId = 1; Reject(invalid);
	invalid = catalog; invalid.mSeed = seedCount; Reject(invalid);
	invalid = catalog; invalid.mOwner = 0; Reject(invalid);
	invalid = catalog; invalid.mImitater = 1; Reject(invalid);
	invalid = selected; invalid.mOwner = 4; Reject(invalid);
	invalid = selected; invalid.mImitater = 255; Reject(invalid);
	invalid = selected; invalid.mImitater = imitater; Reject(invalid);
	invalid = selected; invalid.mSeed = 1; Reject(invalid);
	invalid = selected; invalid.mPacketId = 9; Reject(invalid);
	return result;
}
