#include "NetMysteryTests.h"
#include "../../Lawn/Net/NetMystery.h"
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
int checks, failures;
void Check(bool condition, int line)
{
	++checks;
	if (!condition) { ++failures; std::printf("    Mystery check failed line=%d\n", line); }
}
#define CHECK(condition) Check(!!(condition), __LINE__)

// The production codecs are buffer templates so their validation can run
// without game, renderer or Windows headers. NetBuffer has this same I/O API.
struct Buffer
{
	std::vector<uint8_t> mData;
	size_t mReadPos = 0;
	size_t Remaining() const { return mData.size() - mReadPos; }
	void WriteBytes(const void* value, size_t size)
	{ const uint8_t* bytes = (const uint8_t*)value; mData.insert(mData.end(), bytes, bytes + size); }
	bool ReadBytes(void* value, size_t size)
	{ if (size > Remaining()) return false; std::memcpy(value, mData.data() + mReadPos, size); mReadPos += size; return true; }
	void WriteU8(uint8_t value) { WriteBytes(&value, 1); }
	void WriteU16(uint16_t value) { WriteBytes(&value, 2); }
	void WriteI16(int16_t value) { WriteBytes(&value, 2); }
	void WriteF32(float value) { WriteBytes(&value, 4); }
	bool ReadU8(uint8_t* value) { return ReadBytes(value, 1); }
	bool ReadU16(uint16_t* value) { return ReadBytes(value, 2); }
	bool ReadI16(int16_t* value) { return ReadBytes(value, 2); }
	bool ReadF32(float* value) { return ReadBytes(value, 4); }
};
using namespace LawnNet;
const NetMysteryLimits limits = {49, 48, 40};

bool SameCard(const NetMysteryCard& a, const NetMysteryCard& b)
{ return a.mType == b.mType && a.mImitater == b.mImitater && a.mId == b.mId && a.mOriginLevel == b.mOriginLevel; }

bool SameStart(const NetMysteryStart& a, const NetMysteryStart& b)
{
	if (a.mBoardX != b.mBoardX || a.mBoardY != b.mBoardY || a.mTreeX != b.mTreeX || a.mPoleX != b.mPoleX || a.mReveals.size() != b.mReveals.size()) return false;
	for (size_t i = 0; i < a.mReveals.size(); ++i)
		if (a.mReveals[i].mId != b.mReveals[i].mId || a.mReveals[i].mType != b.mReveals[i].mType || a.mReveals[i].mImitater != b.mReveals[i].mImitater) return false;
	return true;
}

Buffer SnapshotBytes(const NetMysterySeedSnapshot& state)
{
	Buffer b;
	b.WriteU16(state.mId); b.WriteU8(state.mType); b.WriteU8(state.mImitater); b.WriteU8(state.mCooldown);
	b.WriteU8(state.mOwner); b.WriteU8(state.mActive); b.WriteI16(state.mOffsetX); b.WriteU16(state.mSlotCountdown);
	b.WriteU8(state.mSlotNextSeed); b.WriteF32(state.mSlotPosition); b.WriteU8(state.mOriginLevel);
	b.WriteU16(state.mRefreshTime); b.WriteU8(state.mRefreshing);
	return b;
}

bool DecodeSnapshot(Buffer& wire, NetMysterySeedSnapshot& state)
{
	return NetDecodeMysterySeedSnapshot(wire, state, limits, 53,
		[](uint8_t value) { return value == 255 || value < 80; },
		[](uint8_t value) { return value == 255 || value < 4; });
}

void CommitCards()
{
	NetMysteryCard manual;
	manual.mType = 7; manual.mId = 0x1234;
	Buffer wire;
	CHECK(NetEncodeMysteryCard(manual, wire, limits));
	CHECK(wire.mData == (std::vector<uint8_t>{7,255,0x34,0x12,0}));
	NetMysteryCard decoded;
	CHECK(NetDecodeMysteryCard(wire, decoded, limits) && SameCard(manual, decoded) && wire.Remaining() == 0);
	NetMysteryCard pending;
	pending.mId = 65535; pending.mOriginLevel = 10;
	wire = Buffer();
	CHECK(NetEncodeMysteryCard(pending, wire, limits) && wire.mData == (std::vector<uint8_t>{255,255,255,255,10}));
	CHECK(NetDecodeMysteryCard(wire, decoded, limits) && SameCard(pending, decoded));
	for (size_t length = 0; length < NET_MYSTERY_CARD_BYTES; ++length)
	{
		Buffer truncated; truncated.WriteU8(99); truncated.WriteBytes(wire.mData.data(), length); truncated.mReadPos = 1;
		decoded = manual;
		CHECK(!NetDecodeMysteryCard(truncated, decoded, limits) && truncated.mReadPos == 1 && SameCard(decoded, manual));
	}
	std::vector<NetMysteryCard> invalid;
	NetMysteryCard bad = manual; bad.mId = 0; invalid.push_back(bad);
	bad = manual; bad.mType = 49; invalid.push_back(bad);
	bad = manual; bad.mImitater = 0; invalid.push_back(bad);
	bad = manual; bad.mOriginLevel = 1; invalid.push_back(bad);
	bad = pending; bad.mOriginLevel = 11; invalid.push_back(bad);
	bad = pending; bad.mOriginLevel = 0; invalid.push_back(bad);
	bad = pending; bad.mImitater = 7; invalid.push_back(bad);
	for (const auto& card : invalid)
	{
		Buffer output; output.WriteU8(99);
		CHECK(!NetEncodeMysteryCard(card, output, limits) && output.mData.size() == 1);
		Buffer raw; raw.WriteU8(99); raw.WriteU8(card.mType); raw.WriteU8(card.mImitater); raw.WriteU16(card.mId); raw.WriteU8(card.mOriginLevel); raw.mReadPos = 1;
		decoded = manual;
		CHECK(!NetDecodeMysteryCard(raw, decoded, limits) && raw.mReadPos == 1 && SameCard(decoded, manual));
	}
	manual.mType = 48; manual.mImitater = 39;
	CHECK(NetMysteryCardValid(manual, limits));
	manual.mImitater = 40;
	CHECK(!NetMysteryCardValid(manual, limits));
}

void RevealBarrier()
{
	NetMysteryStart state;
	Buffer ordinary;
	CHECK(NetEncodeMysteryStart(state, ordinary, limits) && ordinary.mData == std::vector<uint8_t>(9, 0));
	state.mBoardX = -2048; state.mBoardY = -1024; state.mTreeX = -4096; state.mPoleX = 4096;
	NetMysteryReveal base; base.mId = 0x1234; base.mType = 7;
	NetMysteryReveal imitater; imitater.mId = 0x3456; imitater.mType = 48; imitater.mImitater = 7;
	state.mReveals = {base, imitater};
	Buffer wire;
	CHECK(NetEncodeMysteryStart(state, wire, limits) && wire.mData.size() == 17);
	CHECK(std::vector<uint8_t>(wire.mData.begin() + 8, wire.mData.end()) == (std::vector<uint8_t>{2,0x34,0x12,7,255,0x56,0x34,48,7}));
	NetMysteryStart decoded;
	CHECK(NetDecodeMysteryStart(wire, decoded, limits) && SameStart(decoded, state) && wire.Remaining() == 0);
	CHECK(NetMysteryRevealIdsMatch({0x3456,0x1234}, state.mReveals));
	CHECK(!NetMysteryRevealIdsMatch({0x1234}, state.mReveals));
	CHECK(!NetMysteryRevealIdsMatch({0x1234,0x1234}, state.mReveals));
	CHECK(!NetMysteryRevealIdsMatch({0,0x1234}, state.mReveals));
	CHECK(!NetMysteryRevealIdsMatch({0x1234,77}, state.mReveals));
	for (size_t length = 0; length < wire.mData.size(); ++length)
	{
		Buffer raw; raw.WriteU8(99); raw.WriteBytes(wire.mData.data(), length); raw.mReadPos = 1;
		decoded = state;
		CHECK(!NetDecodeMysteryStart(raw, decoded, limits) && raw.mReadPos == 1 && SameStart(decoded, state));
	}
	std::vector<std::vector<uint8_t>> invalid;
	auto bytes = wire.mData; bytes.push_back(0); invalid.push_back(bytes);
	bytes = wire.mData; bytes[8] = 33; invalid.push_back(bytes);
	bytes = wire.mData; bytes[0] = 255; bytes[1] = 247; invalid.push_back(bytes); // -2049
	bytes = wire.mData; bytes[9] = bytes[10] = 0; invalid.push_back(bytes);
	bytes = wire.mData; bytes[11] = 255; invalid.push_back(bytes);
	bytes = wire.mData; bytes[16] = 40; invalid.push_back(bytes);
	bytes = wire.mData; bytes[13] = bytes[9]; bytes[14] = bytes[10]; invalid.push_back(bytes);
	for (const auto& rawBytes : invalid)
	{
		Buffer raw; raw.WriteU8(99); raw.WriteBytes(rawBytes.data(), rawBytes.size()); raw.mReadPos = 1;
		decoded = state;
		CHECK(!NetDecodeMysteryStart(raw, decoded, limits) && raw.mReadPos == 1 && SameStart(decoded, state));
	}
	state.mReveals.resize(33, base);
	Buffer output; output.WriteU8(99);
	CHECK(!NetEncodeMysteryStart(state, output, limits) && output.mData.size() == 1);
}

void SnapshotAndSave()
{
	NetMysterySeedSnapshot state;
	state.mId = 65535; state.mOwner = 3; state.mOriginLevel = 10; state.mOffsetX = 1024;
	Buffer wire = SnapshotBytes(state);
	CHECK(wire.mData.size() == NET_MYSTERY_SEED_SNAPSHOT_BYTES);
	NetMysterySeedSnapshot decoded;
	CHECK(DecodeSnapshot(wire, decoded) && SnapshotBytes(decoded).mData == SnapshotBytes(state).mData);
	for (size_t length = 0; length < wire.mData.size(); ++length)
	{
		Buffer raw; raw.WriteU8(99); raw.WriteBytes(wire.mData.data(), length); raw.mReadPos = 1;
		decoded = state;
		CHECK(!DecodeSnapshot(raw, decoded) && raw.mReadPos == 1 && SnapshotBytes(decoded).mData == SnapshotBytes(state).mData);
	}
	std::vector<NetMysterySeedSnapshot> invalid;
	auto bad = state; bad.mOriginLevel = 11; invalid.push_back(bad);
	bad = state; bad.mId = 0; invalid.push_back(bad);
	bad = state; bad.mImitater = 7; invalid.push_back(bad);
	bad = state; bad.mActive = 1; invalid.push_back(bad);
	bad = state; bad.mCooldown = 254; invalid.push_back(bad);
	bad = state; bad.mSlotCountdown = 1; invalid.push_back(bad);
	bad = state; bad.mSlotNextSeed = 0; invalid.push_back(bad);
	bad = state; bad.mSlotPosition = 0.5f; invalid.push_back(bad);
	bad = state; bad.mSlotPosition = std::numeric_limits<float>::quiet_NaN(); invalid.push_back(bad);
	bad = state; bad.mOwner = 4; invalid.push_back(bad);
	bad = state; bad.mOffsetX = -1; invalid.push_back(bad);
	bad = state; bad.mOffsetX = 1025; invalid.push_back(bad);
	bad = state; bad.mType = 48; bad.mImitater = 40; invalid.push_back(bad);
	bad = state; bad.mRefreshTime = 750; invalid.push_back(bad);
	bad = state; bad.mRefreshing = 1; invalid.push_back(bad);
	bad = state; bad.mType = 7; bad.mRefreshTime = 60001; invalid.push_back(bad);
	bad = state; bad.mType = 7; bad.mRefreshing = 2; invalid.push_back(bad);
	bad = state; bad.mType = 7; bad.mRefreshing = 1; invalid.push_back(bad);
	for (const auto& value : invalid)
	{
		Buffer raw; raw.WriteU8(99); const Buffer payload = SnapshotBytes(value); raw.WriteBytes(payload.mData.data(), payload.mData.size()); raw.mReadPos = 1;
		decoded = state;
		CHECK(!DecodeSnapshot(raw, decoded) && raw.mReadPos == 1 && SnapshotBytes(decoded).mData == SnapshotBytes(state).mData);
	}
	state.mType = 48; state.mImitater = 39;
	wire = SnapshotBytes(state);
	CHECK(DecodeSnapshot(wire, decoded) && decoded.mOriginLevel == 10);
	state = NetMysterySeedSnapshot(); state.mActive = 1;
	wire = SnapshotBytes(state);
	CHECK(DecodeSnapshot(wire, decoded) && decoded.mId == 0 && decoded.mOriginLevel == 0);
	state.mRefreshTime = 750;
	wire = SnapshotBytes(state);
	CHECK(!DecodeSnapshot(wire, decoded)); // An empty slot never carries another card's cooldown.
	state.mId = 42; state.mType = 0; state.mOwner = 1; state.mOriginLevel = 10;
	state.mActive = 0; state.mCooldown = 85; state.mRefreshing = 1;
	wire = SnapshotBytes(state);
	CHECK(DecodeSnapshot(wire, decoded) && decoded.mRefreshTime == 750 && decoded.mRefreshing == 1);
	CHECK(decoded.mCooldown * decoded.mRefreshTime / 255 == 250); // First snapshot after a real use, not initial SetPacketType state.
	state.mActive = 1;
	wire = SnapshotBytes(state);
	CHECK(DecodeSnapshot(wire, decoded)); // Preserve native active/refresh combinations rather than inventing a stricter state machine.
	state.mRefreshTime = 60000;
	wire = SnapshotBytes(state);
	CHECK(DecodeSnapshot(wire, decoded) && decoded.mRefreshTime == 60000);
	// PvP MouseDown keeps the packet active; WasPlanted sets refreshing with
	// Plant::GetRefreshTime(zombie)==0. Canonical wire omits this inert flag.
	CHECK(NetSeedRefreshWireFlag(0, true) == 0 && NetSeedRefreshWireFlag(0, false) == 0);
	CHECK(NetSeedRefreshWireFlag(750, true) == 1 && NetSeedRefreshWireFlag(750, false) == 0);
	CHECK(NetSeedRefreshWireFlag(60000, true) == 1);
	Buffer zombieBank;
	for (uint8_t type = 58; type <= 75; ++type)
	{
		NetMysterySeedSnapshot zombie;
		zombie.mId = (uint16_t)(100 + type); zombie.mType = type; zombie.mOwner = 1;
		zombie.mRefreshing = NetSeedRefreshWireFlag(zombie.mRefreshTime, true);
		for (uint8_t active = 0; active <= 1; ++active)
		{
			zombie.mActive = active;
			wire = SnapshotBytes(zombie);
			CHECK(DecodeSnapshot(wire, decoded) && decoded.mType == type && decoded.mActive == active &&
				decoded.mRefreshing == 0 && decoded.mRefreshTime == 0);
		}
		const Buffer payload = SnapshotBytes(zombie);
		zombieBank.WriteBytes(payload.mData.data(), payload.mData.size());
	}
	for (uint8_t type = 58; type <= 75; ++type)
		CHECK(DecodeSnapshot(zombieBank, decoded) && decoded.mType == type && decoded.mRefreshTime == 0);
	CHECK(zombieBank.Remaining() == 0);
	for (uint8_t type : {uint8_t(0), uint8_t(48), uint8_t(54), uint8_t(55), uint8_t(56), uint8_t(57), uint8_t(255)})
	{
		NetMysterySeedSnapshot invalidClock;
		invalidClock.mId = type == 255 ? 0 : 42; invalidClock.mType = type; invalidClock.mRefreshing = 1;
		if (type == 48) invalidClock.mImitater = 0;
		Buffer raw; raw.WriteU8(99); const Buffer payload = SnapshotBytes(invalidClock);
		raw.WriteBytes(payload.mData.data(), payload.mData.size()); raw.mReadPos = 1;
		decoded = state;
		CHECK(!DecodeSnapshot(raw, decoded) && raw.mReadPos == 1 &&
			SnapshotBytes(decoded).mData == SnapshotBytes(state).mData);
	}
	for (int i = 0; i < 256; ++i)
	{
		uint8_t origin = (uint8_t)i;
		CHECK(NetMigrateMysteryOrigin(2, origin) && origin == 0);
		origin = (uint8_t)i;
		CHECK(NetMigrateMysteryOrigin(3, origin) == (i <= 10) && origin == (uint8_t)i);
	}
	uint8_t origin = 10;
	CHECK(!NetMigrateMysteryOrigin(1, origin) && origin == 10);
}
}

NetMysteryTestResult RunNetMysteryTests()
{
	checks = failures = 0;
	std::printf("[ RUN      ] Mystery.CommitRevealSnapshotSaveMigration\n");
	CommitCards(); RevealBarrier(); SnapshotAndSave();
	std::printf(failures ? "[  FAILED  ] Mystery\n" : "[       OK ] Mystery\n");
	return {checks, failures};
}

#ifdef NET_MYSTERY_TEST_STANDALONE
int main()
{
	const auto result = RunNetMysteryTests();
	std::printf("checks=%d failures=%d\n", result.mChecks, result.mFailures);
	return result.mFailures ? 1 : 0;
}
#endif
