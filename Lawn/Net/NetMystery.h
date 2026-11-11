#ifndef __NETMYSTERY_H__
#define __NETMYSTERY_H__

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>

namespace LawnNet
{
static const size_t NET_MYSTERY_MAX_CARDS = 32;
static const size_t NET_MYSTERY_CARD_BYTES = 5;
static const size_t NET_MYSTERY_REVEAL_BYTES = 4;
static const size_t NET_MYSTERY_SEED_SNAPSHOT_BYTES = 20;

struct NetMysteryLimits
{
	uint8_t mSeedCount, mImitaterType, mImitaterTargetCount;
};

struct NetMysteryCard
{
	uint8_t mType = 255, mImitater = 255;
	uint16_t mId = 0;
	uint8_t mOriginLevel = 0;
};

inline bool NetMysteryPlantValid(uint8_t type, uint8_t imitater, const NetMysteryLimits& limits)
{
	return type < limits.mSeedCount && (type == limits.mImitaterType ?
		imitater < limits.mImitaterTargetCount : imitater == 255);
}

inline bool NetMysteryCardValid(const NetMysteryCard& card, const NetMysteryLimits& limits)
{
	if (!card.mId || card.mOriginLevel > 10) return false;
	if (card.mType == 255) return card.mImitater == 255 && card.mOriginLevel > 0;
	return card.mOriginLevel == 0 && NetMysteryPlantValid(card.mType, card.mImitater, limits);
}

template <class Buffer>
inline bool NetEncodeMysteryCard(const NetMysteryCard& card, Buffer& buffer, const NetMysteryLimits& limits)
{
	if (!NetMysteryCardValid(card, limits)) return false;
	buffer.WriteU8(card.mType); buffer.WriteU8(card.mImitater);
	buffer.WriteU16(card.mId); buffer.WriteU8(card.mOriginLevel);
	return true;
}

template <class Buffer>
inline bool NetDecodeMysteryCard(Buffer& buffer, NetMysteryCard& card, const NetMysteryLimits& limits)
{
	const size_t start = buffer.mReadPos;
	NetMysteryCard candidate;
	if (!buffer.ReadU8(&candidate.mType) || !buffer.ReadU8(&candidate.mImitater) ||
		!buffer.ReadU16(&candidate.mId) || !buffer.ReadU8(&candidate.mOriginLevel) ||
		!NetMysteryCardValid(candidate, limits))
	{ buffer.mReadPos = start; return false; }
	card = candidate;
	return true;
}

struct NetMysteryReveal
{
	uint16_t mId = 0;
	uint8_t mType = 255, mImitater = 255;
};

struct NetMysteryStart
{
	int16_t mBoardX = 0, mBoardY = 0, mTreeX = 0, mPoleX = 0;
	std::vector<NetMysteryReveal> mReveals;
};

inline bool NetMysteryStartValid(const NetMysteryStart& state, const NetMysteryLimits& limits)
{
	if (state.mBoardX < -2048 || state.mBoardX > 2048 || state.mBoardY < -1024 || state.mBoardY > 1024 ||
		state.mTreeX < -4096 || state.mTreeX > 4096 || state.mPoleX < -4096 || state.mPoleX > 4096 ||
		state.mReveals.size() > NET_MYSTERY_MAX_CARDS) return false;
	for (size_t i = 0; i < state.mReveals.size(); ++i)
	{
		const NetMysteryReveal& reveal = state.mReveals[i];
		if (!reveal.mId || !NetMysteryPlantValid(reveal.mType, reveal.mImitater, limits)) return false;
		for (size_t j = 0; j < i; ++j)
			if (state.mReveals[j].mId == reveal.mId) return false;
	}
	return true;
}

template <class Buffer>
inline bool NetEncodeMysteryStart(const NetMysteryStart& state, Buffer& buffer, const NetMysteryLimits& limits)
{
	if (!NetMysteryStartValid(state, limits)) return false;
	buffer.WriteI16(state.mBoardX); buffer.WriteI16(state.mBoardY);
	buffer.WriteI16(state.mTreeX); buffer.WriteI16(state.mPoleX);
	buffer.WriteU8((uint8_t)state.mReveals.size());
	for (const NetMysteryReveal& reveal : state.mReveals)
	{
		buffer.WriteU16(reveal.mId); buffer.WriteU8(reveal.mType); buffer.WriteU8(reveal.mImitater);
	}
	return true;
}

template <class Buffer>
inline bool NetDecodeMysteryStart(Buffer& buffer, NetMysteryStart& state, const NetMysteryLimits& limits)
{
	const size_t start = buffer.mReadPos;
	NetMysteryStart candidate;
	uint8_t count = 0;
	if (!buffer.ReadI16(&candidate.mBoardX) || !buffer.ReadI16(&candidate.mBoardY) ||
		!buffer.ReadI16(&candidate.mTreeX) || !buffer.ReadI16(&candidate.mPoleX) ||
		!buffer.ReadU8(&count) || count > NET_MYSTERY_MAX_CARDS ||
		buffer.Remaining() != (size_t)count * NET_MYSTERY_REVEAL_BYTES)
	{ buffer.mReadPos = start; return false; }
	for (uint8_t i = 0; i < count; ++i)
	{
		NetMysteryReveal reveal;
		if (!buffer.ReadU16(&reveal.mId) || !buffer.ReadU8(&reveal.mType) || !buffer.ReadU8(&reveal.mImitater))
		{ buffer.mReadPos = start; return false; }
		candidate.mReveals.push_back(reveal);
	}
	if (!NetMysteryStartValid(candidate, limits)) { buffer.mReadPos = start; return false; }
	state = candidate;
	return true;
}

inline bool NetMysteryRevealIdsMatch(const std::vector<uint16_t>& pending, const std::vector<NetMysteryReveal>& reveals)
{
	if (pending.size() != reveals.size() || pending.size() > NET_MYSTERY_MAX_CARDS) return false;
	for (size_t i = 0; i < pending.size(); ++i)
	{
		if (!pending[i]) return false;
		for (size_t j = 0; j < i; ++j) if (pending[i] == pending[j]) return false;
		size_t matches = 0;
		for (const NetMysteryReveal& reveal : reveals) if (reveal.mId == pending[i]) ++matches;
		if (matches != 1) return false;
	}
	return true;
}

struct NetMysterySeedSnapshot
{
	uint16_t mId = 0;
	uint8_t mType = 255, mImitater = 255, mCooldown = 255, mOwner = 255, mActive = 0;
	int16_t mOffsetX = 0;
	uint16_t mSlotCountdown = 0;
	uint8_t mSlotNextSeed = 255;
	float mSlotPosition = 0.0f;
	uint8_t mOriginLevel = 0;
	uint16_t mRefreshTime = 0;
	uint8_t mRefreshing = 0;
};

inline uint8_t NetSeedRefreshWireFlag(uint16_t refreshTime, bool refreshing)
{
	// Native zombie purchases can keep refreshing set with no recharge duration.
	return refreshTime > 0 && refreshing ? 1 : 0;
}

template <class Buffer, class PacketValidator, class SlotValidator>
inline bool NetDecodeMysterySeedSnapshot(Buffer& buffer, NetMysterySeedSnapshot& state,
	const NetMysteryLimits& limits, uint8_t ordinarySeedCount, PacketValidator packetValid, SlotValidator slotValid)
{
	const size_t start = buffer.mReadPos;
	NetMysterySeedSnapshot candidate;
	bool valid = buffer.ReadU16(&candidate.mId) && buffer.ReadU8(&candidate.mType) &&
		buffer.ReadU8(&candidate.mImitater) && buffer.ReadU8(&candidate.mCooldown) &&
		buffer.ReadU8(&candidate.mOwner) && buffer.ReadU8(&candidate.mActive) &&
		buffer.ReadI16(&candidate.mOffsetX) && buffer.ReadU16(&candidate.mSlotCountdown) &&
		buffer.ReadU8(&candidate.mSlotNextSeed) && buffer.ReadF32(&candidate.mSlotPosition) &&
		buffer.ReadU8(&candidate.mOriginLevel) && buffer.ReadU16(&candidate.mRefreshTime) &&
		buffer.ReadU8(&candidate.mRefreshing);
	valid = valid && packetValid(candidate.mType) && candidate.mOriginLevel <= 10 &&
		(candidate.mImitater == 255 || candidate.mImitater < ordinarySeedCount) &&
		(candidate.mOwner == 255 || candidate.mOwner < 4) && candidate.mActive <= 1 &&
		candidate.mOffsetX >= 0 && candidate.mOffsetX <= 1024 && candidate.mSlotCountdown <= 400 &&
		candidate.mRefreshTime <= 60000 && candidate.mRefreshing <= 1 &&
		(!candidate.mRefreshing || candidate.mRefreshTime > 0) &&
		slotValid(candidate.mSlotNextSeed) && std::isfinite(candidate.mSlotPosition) &&
		candidate.mSlotPosition >= 0.0f && candidate.mSlotPosition <= 1.0f;
	if (candidate.mType == 255)
	{
		valid = valid && candidate.mImitater == 255;
		valid = valid && candidate.mRefreshTime == 0 && candidate.mRefreshing == 0;
		if (candidate.mOriginLevel)
			valid = valid && candidate.mId != 0 && candidate.mActive == 0 && candidate.mCooldown == 255 &&
				candidate.mSlotCountdown == 0 && candidate.mSlotNextSeed == 255 && candidate.mSlotPosition == 0.0f;
		else valid = valid && candidate.mId == 0;
	}
	else
	{
		valid = valid && candidate.mId != 0;
		if (candidate.mOriginLevel) valid = valid && NetMysteryPlantValid(candidate.mType, candidate.mImitater, limits);
	}
	if (!valid) { buffer.mReadPos = start; return false; }
	state = candidate;
	return true;
}

inline bool NetMigrateMysteryOrigin(uint32_t saveVersion, uint8_t& origin)
{
	if (saveVersion == 2) { origin = 0; return true; }
	return saveVersion == 3 && origin <= 10;
}
}
#endif
