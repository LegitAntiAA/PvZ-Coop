#pragma once

#include <cstddef>
#include <cstdint>

namespace LawnNet
{
enum NetCursorTargetKind : uint8_t
{
	NETCURSOR_SCREEN,
	NETCURSOR_PACKET,
	NETCURSOR_CHOOSER_CATALOG,
	NETCURSOR_CHOOSER_BANK
};

// Offsets in the cursor message are relative to this identity, never to a
// viewer's array index. Chooser cards have no packet ID until the bank commits.
struct NetCursorTarget
{
	uint8_t mKind = NETCURSOR_SCREEN;
	uint16_t mPacketId = 0;
	uint8_t mOwner = 255;
	uint8_t mSeed = 255;
	uint8_t mImitater = 255;
};

static const size_t NET_CURSOR_TARGET_BYTES = 6;
static const size_t NET_CURSOR_WIRE_BYTES = 11 + NET_CURSOR_TARGET_BYTES;

inline bool NetCursorTargetValid(const NetCursorTarget& target, uint8_t seedCount, uint8_t imitaterType)
{
	if (target.mKind == NETCURSOR_SCREEN || target.mKind == NETCURSOR_PACKET)
		return (target.mKind == NETCURSOR_PACKET ? target.mPacketId != 0 : target.mPacketId == 0) &&
			target.mOwner == 255 && target.mSeed == 255 && target.mImitater == 255;
	if (target.mPacketId || target.mSeed >= seedCount) return false;
	if (target.mKind == NETCURSOR_CHOOSER_CATALOG)
		return target.mOwner == 255 && target.mImitater == 255;
	if (target.mKind != NETCURSOR_CHOOSER_BANK || target.mOwner >= 4) return false;
	return target.mSeed == imitaterType ? target.mImitater < seedCount && target.mImitater != imitaterType :
		target.mImitater == 255;
}

template<class Buffer>
inline void NetEncodeCursorTarget(const NetCursorTarget& target, Buffer& buffer)
{
	buffer.WriteU8(target.mKind); buffer.WriteU16(target.mPacketId);
	buffer.WriteU8(target.mOwner); buffer.WriteU8(target.mSeed); buffer.WriteU8(target.mImitater);
}

template<class Buffer>
inline bool NetDecodeCursorTarget(Buffer& buffer, NetCursorTarget& target, uint8_t seedCount, uint8_t imitaterType)
{
	const size_t start = buffer.mReadPos;
	NetCursorTarget candidate;
	if (!buffer.ReadU8(&candidate.mKind) || !buffer.ReadU16(&candidate.mPacketId) ||
		!buffer.ReadU8(&candidate.mOwner) || !buffer.ReadU8(&candidate.mSeed) ||
		!buffer.ReadU8(&candidate.mImitater) || !NetCursorTargetValid(candidate, seedCount, imitaterType))
	{ buffer.mReadPos = start; return false; }
	target = candidate;
	return true;
}
}
