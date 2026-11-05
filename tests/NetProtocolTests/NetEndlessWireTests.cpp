#include "../../Lawn/Net/NetCommon.h"
#include "NetEndlessWireTests.h"

#include <cstdio>
#include <cstring>

namespace
{
int gChecks = 0;
int gFailures = 0;

void Check(bool theCondition, const char* theExpression, int theLine)
{
	gChecks++;
	if (theCondition)
		return;
	gFailures++;
	std::printf("    CHECK failed: %s (NetEndlessWireTests.cpp:%d)\n",
		theExpression, theLine);
}

#define CHECK(theExpression) Check(!!(theExpression), #theExpression, __LINE__)

bool SameHealth(const LawnNet::NetZombieHealthState& a,
	const LawnNet::NetZombieHealthState& b)
{
	return a.mBody == b.mBody && a.mBodyMax == b.mBodyMax &&
		a.mHelm == b.mHelm && a.mHelmMax == b.mHelmMax &&
		a.mShield == b.mShield && a.mShieldMax == b.mShieldMax &&
		a.mFlying == b.mFlying && a.mFlyingMax == b.mFlyingMax;
}

void TestRepickStagesDoNotWrap()
{
	const uint32_t aStages[] = { 0U, 255U, 256U, LawnNet::NET_ENDLESS_STAGE_MAX };
	for (uint32_t aStage : aStages)
	{
		LawnNet::NetBuffer aWire;
		CHECK(LawnNet::NetEncodeEndlessRepickEvent(aStage, aWire));
		CHECK(aWire.Size() == LawnNet::NET_ENDLESS_REPICK_WIRE_BYTES);
		uint32_t aDecoded = 17U;
		CHECK(LawnNet::NetDecodeEndlessRepickEvent(aWire, aDecoded));
		CHECK(aDecoded == aStage);
		CHECK(aWire.Remaining() == 0U);
	}
}

void TestRepickDecodeIsExactAndTransactional()
{
	LawnNet::NetBuffer aValid;
	CHECK(LawnNet::NetEncodeEndlessRepickEvent(256U, aValid));

	LawnNet::NetBuffer aTruncated = aValid;
	aTruncated.mData.pop_back();
	uint32_t aStage = 777U;
	const size_t aStart = aTruncated.mReadPos;
	CHECK(!LawnNet::NetDecodeEndlessRepickEvent(aTruncated, aStage));
	CHECK(aTruncated.mReadPos == aStart);
	CHECK(aStage == 777U);

	LawnNet::NetBuffer aTrailing = aValid;
	aTrailing.WriteU8(0xAAU);
	CHECK(!LawnNet::NetDecodeEndlessRepickEvent(aTrailing, aStage));
	CHECK(aTrailing.mReadPos == 0U);
	CHECK(aStage == 777U);

	LawnNet::NetBuffer aWrongEvent = aValid;
	aWrongEvent.mData[0] = 255U;
	CHECK(!LawnNet::NetDecodeEndlessRepickEvent(aWrongEvent, aStage));
	CHECK(aWrongEvent.mReadPos == 0U);
	CHECK(aStage == 777U);

	LawnNet::NetBuffer anOutOfRange;
	anOutOfRange.WriteU8(LawnNet::BOARDEV_ENDLESS_REPICK);
	anOutOfRange.WriteU32(LawnNet::NET_ENDLESS_STAGE_MAX + 1U);
	CHECK(!LawnNet::NetDecodeEndlessRepickEvent(anOutOfRange, aStage));
	CHECK(anOutOfRange.mReadPos == 0U);
	CHECK(aStage == 777U);

	LawnNet::NetBuffer anOutput;
	anOutput.WriteU8(0x55U);
	CHECK(!LawnNet::NetEncodeEndlessRepickEvent(
		LawnNet::NET_ENDLESS_STAGE_MAX + 1U, anOutput));
	CHECK(anOutput.Size() == 1U && anOutput.mData[0] == 0x55U);
}

LawnNet::NetZombieHealthState MakeWideHealth()
{
	LawnNet::NetZombieHealthState aState;
	aState.mBody = 94800U;
	aState.mBodyMax = 120000U;
	aState.mHelm = 65536U;
	aState.mHelmMax = 131072U;
	aState.mShield = 65535U;
	aState.mShieldMax = 70000U;
	aState.mFlying = 90000U;
	aState.mFlyingMax = LawnNet::NET_ZOMBIE_HEALTH_MAX;
	return aState;
}

void TestWideHealthRoundTrip()
{
	const LawnNet::NetZombieHealthState aState = MakeWideHealth();
	LawnNet::NetBuffer aWire;
	CHECK(LawnNet::NetEncodeZombieHealthState(aState, aWire));
	CHECK(aWire.Size() == LawnNet::NET_ZOMBIE_HEALTH_WIRE_BYTES);
	LawnNet::NetZombieHealthState aDecoded;
	CHECK(LawnNet::NetDecodeZombieHealthState(aWire, aDecoded));
	CHECK(SameHealth(aState, aDecoded));
	CHECK(aWire.Remaining() == 0U);
}

void TestHealthDecodeIsTransactionalAndBounded()
{
	const LawnNet::NetZombieHealthState aState = MakeWideHealth();
	LawnNet::NetBuffer aValid;
	CHECK(LawnNet::NetEncodeZombieHealthState(aState, aValid));

	LawnNet::NetZombieHealthState aSentinel;
	aSentinel.mBody = 1U;
	aSentinel.mBodyMax = 2U;
	aSentinel.mHelm = 3U;
	aSentinel.mHelmMax = 4U;
	aSentinel.mShield = 5U;
	aSentinel.mShieldMax = 6U;
	aSentinel.mFlying = 7U;
	aSentinel.mFlyingMax = 8U;
	LawnNet::NetZombieHealthState anOutput = aSentinel;

	LawnNet::NetBuffer aTruncated = aValid;
	aTruncated.mData.pop_back();
	CHECK(!LawnNet::NetDecodeZombieHealthState(aTruncated, anOutput));
	CHECK(aTruncated.mReadPos == 0U);
	CHECK(SameHealth(anOutput, aSentinel));

	LawnNet::NetBuffer anOutOfRange = aValid;
	const uint32_t anInvalid = LawnNet::NET_ZOMBIE_HEALTH_MAX + 1U;
	std::memcpy(&anOutOfRange.mData[0], &anInvalid, sizeof(anInvalid));
	CHECK(!LawnNet::NetDecodeZombieHealthState(anOutOfRange, anOutput));
	CHECK(anOutOfRange.mReadPos == 0U);
	CHECK(SameHealth(anOutput, aSentinel));

	LawnNet::NetZombieHealthState anInvalidState = aState;
	anInvalidState.mBodyMax = LawnNet::NET_ZOMBIE_HEALTH_MAX + 1U;
	LawnNet::NetBuffer anAppendTarget;
	anAppendTarget.WriteU8(0xCCU);
	CHECK(!LawnNet::NetEncodeZombieHealthState(anInvalidState, anAppendTarget));
	CHECK(anAppendTarget.Size() == 1U && anAppendTarget.mData[0] == 0xCCU);
}
}

NetEndlessWireTestResult RunNetEndlessWireTests()
{
	gChecks = 0;
	gFailures = 0;
	std::printf("[ RUN      ] EndlessWire.RepickStagesDoNotWrap\n");
	TestRepickStagesDoNotWrap();
	std::printf("[ RUN      ] EndlessWire.RepickExactTransactionalDecode\n");
	TestRepickDecodeIsExactAndTransactional();
	std::printf("[ RUN      ] EndlessWire.WideZombieHealthRoundTrip\n");
	TestWideHealthRoundTrip();
	std::printf("[ RUN      ] EndlessWire.HealthTransactionalBounds\n");
	TestHealthDecodeIsTransactionalAndBounded();
	std::printf(gFailures == 0 ? "[       OK ] EndlessWire\n" :
		"[  FAILED  ] EndlessWire\n");
	return { gChecks, gFailures };
}
