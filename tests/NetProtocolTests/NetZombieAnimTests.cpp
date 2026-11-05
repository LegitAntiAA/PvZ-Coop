#include "NetZombieAnimTests.h"
#include "../../Lawn/Net/NetCommon.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int gChecks = 0;
int gFailures = 0;

void Check(bool theCondition, const char* theExpression, int theLine)
{
	++gChecks;
	if (theCondition)
		return;
	++gFailures;
	std::printf("    CHECK failed: %s (NetZombieAnimTests.cpp:%d)\n", theExpression, theLine);
}

#define CHECK(theExpression) Check(!!(theExpression), #theExpression, __LINE__)

void TestGoldenVectorAndRoundTrip()
{
	LawnNet::NetZombieAnimState aState;
	aState.mPresent = true;
	aState.mFrameStart = 123;
	aState.mFrameCount = 45;
	aState.mAnimTime = 0.5f;
	aState.mAnimRate = -24.0f;
	aState.mLoopType = 3;
	aState.mLoopCount = 1;

	LawnNet::NetBuffer aWire;
	CHECK(LawnNet::NetEncodeZombieAnimState(aState, aWire));
	const std::vector<uint8_t> anExpected =
	{
		0x7BU, 0x00U, 0x2DU, 0x00U, 0x00U, 0x80U,
		0x00U, 0xFDU, 0x03U, 0x01U, 0x01U
	};
	CHECK(aWire.mData == anExpected);
	CHECK(aWire.Size() == LawnNet::NET_ZOMBIE_ANIM_WIRE_BYTES);

	LawnNet::NetZombieAnimState aDecoded;
	CHECK(LawnNet::NetDecodeZombieAnimState(aWire, aDecoded));
	CHECK(aWire.Remaining() == 0U);
	CHECK(aDecoded.mPresent);
	CHECK(!aDecoded.mDead);
	CHECK(aDecoded.mFrameStart == 123U);
	CHECK(aDecoded.mFrameCount == 45U);
	CHECK(std::fabs(aDecoded.mAnimTime - 0.5f) <= 1.0f / 65535.0f);
	CHECK(aDecoded.mAnimRate == -24.0f);
	CHECK(aDecoded.mLoopType == 3U);
	CHECK(aDecoded.mLoopCount == 1U);
}

void TestBoundedQuantizationAndAbsence()
{
	LawnNet::NetZombieAnimState aState;
	aState.mPresent = true;
	aState.mDead = true;
	aState.mFrameCount = 1;
	aState.mAnimTime = 2.0f;
	aState.mAnimRate = 2000.0f;
	aState.mLoopType = 5;
	aState.mLoopCount = 255;

	LawnNet::NetBuffer aWire;
	CHECK(LawnNet::NetEncodeZombieAnimState(aState, aWire));
	LawnNet::NetZombieAnimState aDecoded;
	CHECK(LawnNet::NetDecodeZombieAnimState(aWire, aDecoded));
	CHECK(aDecoded.mDead);
	CHECK(aDecoded.mAnimTime == 1.0f);
	CHECK(aDecoded.mAnimRate == 32767.0f / 32.0f);

	aState.mAnimTime = -3.0f;
	aState.mAnimRate = -2000.0f;
	aWire.Clear();
	CHECK(LawnNet::NetEncodeZombieAnimState(aState, aWire));
	CHECK(LawnNet::NetDecodeZombieAnimState(aWire, aDecoded));
	CHECK(aDecoded.mAnimTime == 0.0f);
	CHECK(aDecoded.mAnimRate == -1024.0f);

	LawnNet::NetZombieAnimState anAbsent;
	aWire.Clear();
	CHECK(LawnNet::NetEncodeZombieAnimState(anAbsent, aWire));
	CHECK(aWire.mData == std::vector<uint8_t>(LawnNet::NET_ZOMBIE_ANIM_WIRE_BYTES, 0U));
	CHECK(LawnNet::NetDecodeZombieAnimState(aWire, aDecoded));
	CHECK(!aDecoded.mPresent);
	CHECK(!aDecoded.mDead);
}

bool DecodeRejectedWithoutMutation(const std::vector<uint8_t>& theBytes)
{
	LawnNet::NetBuffer aWire;
	if (!theBytes.empty())
		aWire.WriteBytes(theBytes.data(), theBytes.size());
	const size_t aReadPosition = aWire.mReadPos;
	LawnNet::NetZombieAnimState aState;
	aState.mPresent = true;
	aState.mDead = true;
	aState.mFrameStart = 7;
	aState.mFrameCount = 9;
	aState.mAnimTime = 0.25f;
	aState.mAnimRate = 18.0f;
	aState.mLoopType = 2;
	aState.mLoopCount = 4;
	const bool aDecoded = LawnNet::NetDecodeZombieAnimState(aWire, aState);
	return !aDecoded && aWire.mReadPos == aReadPosition && aState.mPresent &&
		aState.mDead && aState.mFrameStart == 7U && aState.mFrameCount == 9U &&
		aState.mAnimTime == 0.25f && aState.mAnimRate == 18.0f &&
		aState.mLoopType == 2U && aState.mLoopCount == 4U;
}

void TestMalformedRecordsAreRejectedTransactionally()
{
	CHECK(DecodeRejectedWithoutMutation(std::vector<uint8_t>(10U, 0U)));

	std::vector<uint8_t> aReservedFlags(11U, 0U);
	aReservedFlags[10] = 0x80U;
	CHECK(DecodeRejectedWithoutMutation(aReservedFlags));

	std::vector<uint8_t> aNonCanonicalAbsent(11U, 0U);
	aNonCanonicalAbsent[0] = 1U;
	CHECK(DecodeRejectedWithoutMutation(aNonCanonicalAbsent));

	std::vector<uint8_t> aMissingFrames(11U, 0U);
	aMissingFrames[10] = LawnNet::NET_ZOMBIE_ANIM_PRESENT;
	CHECK(DecodeRejectedWithoutMutation(aMissingFrames));

	std::vector<uint8_t> anOverflowingRange(11U, 0U);
	anOverflowingRange[0] = 0xFFU;
	anOverflowingRange[1] = 0xFFU;
	anOverflowingRange[2] = 0x02U;
	anOverflowingRange[10] = LawnNet::NET_ZOMBIE_ANIM_PRESENT;
	CHECK(DecodeRejectedWithoutMutation(anOverflowingRange));

	std::vector<uint8_t> anInvalidLoop(11U, 0U);
	anInvalidLoop[2] = 1U;
	anInvalidLoop[8] = 6U;
	anInvalidLoop[10] = LawnNet::NET_ZOMBIE_ANIM_PRESENT;
	CHECK(DecodeRejectedWithoutMutation(anInvalidLoop));
}

void TestAssetFrameBounds()
{
	LawnNet::NetZombieAnimState aState;
	CHECK(LawnNet::NetZombieAnimStateFitsFrames(aState, 0U));
	aState.mPresent = true;
	aState.mFrameStart = 10;
	aState.mFrameCount = 5;
	CHECK(LawnNet::NetZombieAnimStateFitsFrames(aState, 15U));
	CHECK(!LawnNet::NetZombieAnimStateFitsFrames(aState, 14U));
	aState.mFrameCount = 0;
	CHECK(!LawnNet::NetZombieAnimStateFitsFrames(aState, 100U));
}
}

NetZombieAnimTestResult RunNetZombieAnimTests()
{
	gChecks = 0;
	gFailures = 0;
	std::printf("[ RUN      ] ZombieAnim.GoldenVectorAndRoundTrip\n");
	TestGoldenVectorAndRoundTrip();
	std::printf("[ RUN      ] ZombieAnim.BoundedQuantizationAndAbsence\n");
	TestBoundedQuantizationAndAbsence();
	std::printf("[ RUN      ] ZombieAnim.MalformedTransactionalDecode\n");
	TestMalformedRecordsAreRejectedTransactionally();
	std::printf("[ RUN      ] ZombieAnim.AssetFrameBounds\n");
	TestAssetFrameBounds();
	std::printf(gFailures == 0 ? "[       OK ] ZombieAnim\n" : "[  FAILED  ] ZombieAnim\n");
	return { gChecks, gFailures };
}
