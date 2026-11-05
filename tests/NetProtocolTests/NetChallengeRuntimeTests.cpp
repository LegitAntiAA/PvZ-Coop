#include "../../Lawn/Net/NetCommon.h"
#include "NetChallengeRuntimeTests.h"

#include <cstdio>
#include <cstring>
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
	std::printf("    CHECK failed: %s (NetChallengeRuntimeTests.cpp:%d)\n",
		theExpression, theLine);
}

#define CHECK(theExpression) Check(!!(theExpression), #theExpression, __LINE__)

LawnNet::NetChallengeRuntimeState MakeValidState()
{
	LawnNet::NetChallengeRuntimeState aState;
	aState.mProgressMeterWidth = 150;
	aState.mChallengePoints = 12345678;
	aState.mBombsLeft = 25;
	aState.mSlotMachineRollCount = 321;
	aState.mTreeOfWisdomTalkIndex = 1100;
	aState.mCurrentChallengeRecord = 1234;
	aState.SetBeghouledEaten(0, 0, true);
	aState.SetBeghouledEaten(0, 5, true);
	aState.SetBeghouledEaten(1, 0, true);
	aState.SetBeghouledEaten(8, 5, true);
	return aState;
}

void TestGoldenVectorAndRoundTrip()
{
	LawnNet::NetChallengeRuntimeState aState = MakeValidState();
	LawnNet::NetBuffer aWire;
	CHECK(LawnNet::NetEncodeChallengeRuntimeState(aState, aWire));
	const std::vector<uint8_t> anExpected =
	{
		0x96U, 0x00U, 0x4EU, 0x61U, 0xBCU, 0x00U,
		0x19U, 0x00U, 0x41U, 0x01U, 0x4CU, 0x04U,
		0xD2U, 0x04U, 0x00U, 0x00U,
		0x61U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x20U
	};
	CHECK(aWire.mData == anExpected);
	CHECK(aWire.Size() == LawnNet::NET_CHALLENGE_RUNTIME_WIRE_BYTES);

	LawnNet::NetChallengeRuntimeState aDecoded;
	CHECK(LawnNet::NetDecodeChallengeRuntimeState(aWire, aDecoded));
	CHECK(aWire.Remaining() == 0U);
	CHECK(aDecoded.mProgressMeterWidth == 150U);
	CHECK(aDecoded.mChallengePoints == 12345678);
	CHECK(aDecoded.mBombsLeft == 25);
	CHECK(aDecoded.mSlotMachineRollCount == 321U);
	CHECK(aDecoded.mTreeOfWisdomTalkIndex == 1100U);
	CHECK(aDecoded.mCurrentChallengeRecord == 1234);
	CHECK(aDecoded.IsBeghouledEaten(0, 0));
	CHECK(aDecoded.IsBeghouledEaten(0, 5));
	CHECK(aDecoded.IsBeghouledEaten(1, 0));
	CHECK(aDecoded.IsBeghouledEaten(8, 5));
	CHECK(!aDecoded.IsBeghouledEaten(8, 4));
	CHECK(!aDecoded.IsBeghouledEaten(-1, 0));
	CHECK(!aDecoded.IsBeghouledEaten(9, 0));
}

bool DecodeRejectedWithoutMutation(const std::vector<uint8_t>& theBytes)
{
	LawnNet::NetBuffer aWire;
	if (!theBytes.empty())
		aWire.WriteBytes(theBytes.data(), theBytes.size());
	const size_t aReadPosition = aWire.mReadPos;
	LawnNet::NetChallengeRuntimeState aState;
	aState.mProgressMeterWidth = 7;
	aState.mChallengePoints = 8;
	aState.mBombsLeft = 9;
	aState.mSlotMachineRollCount = 10;
	aState.mTreeOfWisdomTalkIndex = 11;
	aState.mCurrentChallengeRecord = 12;
	aState.mBeghouledEatenBits = 13;
	const bool aDecoded = LawnNet::NetDecodeChallengeRuntimeState(aWire, aState);
	return !aDecoded && aWire.mReadPos == aReadPosition &&
		aState.mProgressMeterWidth == 7 && aState.mChallengePoints == 8 &&
		aState.mBombsLeft == 9 && aState.mSlotMachineRollCount == 10 &&
		aState.mTreeOfWisdomTalkIndex == 11 &&
		aState.mCurrentChallengeRecord == 12 &&
		aState.mBeghouledEatenBits == 13;
}

template <typename T>
void ReplaceWireValue(std::vector<uint8_t>& theBytes, size_t theOffset, T theValue)
{
	std::memcpy(theBytes.data() + theOffset, &theValue, sizeof(theValue));
}

void TestMalformedRecordsAreRejectedTransactionally()
{
	LawnNet::NetBuffer aValidWire;
	CHECK(LawnNet::NetEncodeChallengeRuntimeState(MakeValidState(), aValidWire));
	CHECK(DecodeRejectedWithoutMutation(std::vector<uint8_t>(
		aValidWire.mData.begin(), aValidWire.mData.end() - 1)));

	std::vector<uint8_t> aBad = aValidWire.mData;
	ReplaceWireValue<uint16_t>(aBad, 0, 151);
	CHECK(DecodeRejectedWithoutMutation(aBad));

	aBad = aValidWire.mData;
	ReplaceWireValue<int32_t>(aBad, 2, -1);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<int32_t>(aBad, 2, LawnNet::NET_CHALLENGE_POINTS_MAX + 1);
	CHECK(DecodeRejectedWithoutMutation(aBad));

	aBad = aValidWire.mData;
	ReplaceWireValue<int16_t>(aBad, 6, -2);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<int16_t>(aBad, 6, 1001);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<uint16_t>(aBad, 8,
		LawnNet::NET_CHALLENGE_SLOT_ROLL_MAX + 1);
	CHECK(DecodeRejectedWithoutMutation(aBad));

	aBad = aValidWire.mData;
	ReplaceWireValue<uint16_t>(aBad, 10, 1101);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<int32_t>(aBad, 12, -2);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<int32_t>(aBad, 12,
		LawnNet::NET_CHALLENGE_RECORD_MAX + 1);
	CHECK(DecodeRejectedWithoutMutation(aBad));

	aBad = aValidWire.mData;
	aBad[22] |= 0x40U;
	CHECK(DecodeRejectedWithoutMutation(aBad));
}

void TestEncoderBoundsAndCanonicalMask()
{
	LawnNet::NetChallengeRuntimeState aState = MakeValidState();
	LawnNet::NetBuffer aWire;
	aWire.WriteU8(0xAAU);
	aState.mProgressMeterWidth = 151;
	CHECK(!LawnNet::NetEncodeChallengeRuntimeState(aState, aWire));
	CHECK(aWire.Size() == 1U && aWire.mData[0] == 0xAAU);

	aState = MakeValidState();
	aState.mBeghouledEatenBits |= UINT64_C(1) << 54;
	CHECK(!LawnNet::NetEncodeChallengeRuntimeState(aState, aWire));
	CHECK(aWire.Size() == 1U);

	LawnNet::NetChallengeRuntimeState aSentinels;
	CHECK(LawnNet::NetEncodeChallengeRuntimeState(aSentinels, aWire));
	CHECK(aWire.Size() == 1U + LawnNet::NET_CHALLENGE_RUNTIME_WIRE_BYTES);
}
}

NetChallengeRuntimeTestResult RunNetChallengeRuntimeTests()
{
	gChecks = 0;
	gFailures = 0;
	std::printf("[ RUN      ] ChallengeRuntime.GoldenVectorAndRoundTrip\n");
	TestGoldenVectorAndRoundTrip();
	std::printf("[ RUN      ] ChallengeRuntime.MalformedTransactionalDecode\n");
	TestMalformedRecordsAreRejectedTransactionally();
	std::printf("[ RUN      ] ChallengeRuntime.EncoderBoundsAndCanonicalMask\n");
	TestEncoderBoundsAndCanonicalMask();
	std::printf(gFailures == 0 ? "[       OK ] ChallengeRuntime\n" :
		"[  FAILED  ] ChallengeRuntime\n");
	return { gChecks, gFailures };
}
