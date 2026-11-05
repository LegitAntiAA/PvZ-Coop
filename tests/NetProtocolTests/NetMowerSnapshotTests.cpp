#include "../../Lawn/Net/NetCommon.h"
#include "NetMowerSnapshotTests.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
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
	std::printf("    CHECK failed: %s (NetMowerSnapshotTests.cpp:%d)\n",
		theExpression, theLine);
}

#define CHECK(theExpression) Check(!!(theExpression), #theExpression, __LINE__)

LawnNet::NetMowerSnapshotState MakeValidState(uint16_t theId = 0x1234U,
	uint8_t theType = 1U)
{
	LawnNet::NetMowerSnapshotState aState;
	aState.mId = theId;
	aState.mRow = 4;
	aState.mState = 2;
	aState.mType = theType;
	aState.mVisible = true;
	aState.mPosX = 1.25f;
	aState.mPosY = -1.5f;
	aState.mRenderOrder = 0x00020304;
	aState.mAltitude = -28.0f;
	aState.mHeight = 2;
	aState.mReanim.mPresent = true;
	aState.mReanim.mDead = false;
	aState.mReanim.mFrameStart = 1;
	aState.mReanim.mFrameCount = 2;
	aState.mReanim.mAnimTime = 0.5f;
	aState.mReanim.mAnimRate = 35.0f;
	aState.mReanim.mLoopType = 0;
	aState.mReanim.mLoopCount = 7;
	return aState;
}

void TestGoldenVectorAndRoundTrip()
{
	LawnNet::NetBuffer aWire;
	CHECK(LawnNet::NetEncodeMowerSnapshotState(MakeValidState(), aWire));
	const std::vector<uint8_t> anExpected =
	{
		0x34U, 0x12U, 0x04U, 0x02U, 0x01U, 0x01U,
		0x05U, 0x00U, 0xFAU, 0xFFU,
		0x04U, 0x03U, 0x02U, 0x00U,
		0x00U, 0x00U, 0xE0U, 0xC1U, 0x02U,
		0x01U, 0x00U, 0x02U, 0x00U, 0x00U, 0x80U,
		0x60U, 0x04U, 0x00U, 0x07U, 0x01U
	};
	CHECK(aWire.mData == anExpected);
	CHECK(aWire.Size() == LawnNet::NET_MOWER_WIRE_BYTES);

	LawnNet::NetMowerSnapshotState aDecoded;
	CHECK(LawnNet::NetDecodeMowerSnapshotState(aWire, aDecoded));
	CHECK(aWire.Remaining() == 0U);
	CHECK(aDecoded.mId == 0x1234U);
	CHECK(aDecoded.mRow == 4U);
	CHECK(aDecoded.mState == 2U);
	CHECK(aDecoded.mType == 1U);
	CHECK(aDecoded.mVisible);
	CHECK(aDecoded.mPosX == 1.25f);
	CHECK(aDecoded.mPosY == -1.5f);
	CHECK(aDecoded.mRenderOrder == 0x00020304);
	CHECK(aDecoded.mAltitude == -28.0f);
	CHECK(aDecoded.mHeight == 2U);
	CHECK(aDecoded.mReanim.mPresent);
	CHECK(!aDecoded.mReanim.mDead);
	CHECK(aDecoded.mReanim.mFrameStart == 1U);
	CHECK(aDecoded.mReanim.mFrameCount == 2U);
	CHECK(std::fabs(aDecoded.mReanim.mAnimTime - 0.5f) <=
		1.0f / 65535.0f);
	CHECK(aDecoded.mReanim.mAnimRate == 35.0f);
	CHECK(aDecoded.mReanim.mLoopType == 0U);
	CHECK(aDecoded.mReanim.mLoopCount == 7U);
}

bool IsSentinelState(const LawnNet::NetMowerSnapshotState& theState)
{
	return theState.mId == 77U && theState.mRow == 5U &&
		theState.mState == 3U && theState.mType == 2U &&
		!theState.mVisible && theState.mPosX == 9.0f &&
		theState.mPosY == 10.0f && theState.mRenderOrder == 11 &&
		theState.mAltitude == 12.0f && theState.mHeight == 3U &&
		theState.mReanim.mPresent && theState.mReanim.mDead &&
		theState.mReanim.mFrameStart == 13U &&
		theState.mReanim.mFrameCount == 14U;
}

LawnNet::NetMowerSnapshotState MakeSentinelState()
{
	LawnNet::NetMowerSnapshotState aState;
	aState.mId = 77;
	aState.mRow = 5;
	aState.mState = 3;
	aState.mType = 2;
	aState.mVisible = false;
	aState.mPosX = 9.0f;
	aState.mPosY = 10.0f;
	aState.mRenderOrder = 11;
	aState.mAltitude = 12.0f;
	aState.mHeight = 3;
	aState.mReanim.mPresent = true;
	aState.mReanim.mDead = true;
	aState.mReanim.mFrameStart = 13;
	aState.mReanim.mFrameCount = 14;
	return aState;
}

bool DecodeRejectedWithoutMutation(const std::vector<uint8_t>& theBytes)
{
	LawnNet::NetBuffer aWire;
	if (!theBytes.empty())
		aWire.WriteBytes(theBytes.data(), theBytes.size());
	const size_t aReadPosition = aWire.mReadPos;
	LawnNet::NetMowerSnapshotState aState = MakeSentinelState();
	const bool aDecoded = LawnNet::NetDecodeMowerSnapshotState(aWire, aState);
	return !aDecoded && aWire.mReadPos == aReadPosition &&
		IsSentinelState(aState);
}

template <typename T>
void ReplaceWireValue(std::vector<uint8_t>& theBytes, size_t theOffset,
	T theValue)
{
	std::memcpy(theBytes.data() + theOffset, &theValue, sizeof(theValue));
}

void TestMalformedRecordsAreRejectedTransactionally()
{
	LawnNet::NetBuffer aValidWire;
	CHECK(LawnNet::NetEncodeMowerSnapshotState(MakeValidState(), aValidWire));
	CHECK(DecodeRejectedWithoutMutation(std::vector<uint8_t>(
		aValidWire.mData.begin(), aValidWire.mData.end() - 1)));

	std::vector<uint8_t> aBad = aValidWire.mData;
	ReplaceWireValue<uint16_t>(aBad, 0, 0);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	aBad[2] = 6;
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	aBad[3] = 4;
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	aBad[4] = 3;
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	aBad[5] = 2;
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<int32_t>(aBad, 10, -1);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<int32_t>(aBad, 10,
		LawnNet::NET_MOWER_RENDER_ORDER_MAX + 1);
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	ReplaceWireValue<float>(aBad, 14,
		std::numeric_limits<float>::quiet_NaN());
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	aBad[18] = 4;
	CHECK(DecodeRejectedWithoutMutation(aBad));
	aBad = aValidWire.mData;
	aBad[29] = 0x80U;
	CHECK(DecodeRejectedWithoutMutation(aBad));
}

bool EncodeRejectedWithoutAppend(LawnNet::NetMowerSnapshotState theState)
{
	LawnNet::NetBuffer aWire;
	aWire.WriteU8(0xAAU);
	return !LawnNet::NetEncodeMowerSnapshotState(theState, aWire) &&
		aWire.Size() == 1U && aWire.mData[0] == 0xAAU;
}

void TestEncoderBoundsAndCleanerTypes()
{
	LawnNet::NetMowerSnapshotState aState = MakeValidState();
	aState.mPosX = LawnNet::NET_MOWER_POSITION_MIN - 0.25f;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mPosY = LawnNet::NET_MOWER_POSITION_MAX + 0.25f;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mPosX = std::numeric_limits<float>::infinity();
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mAltitude = LawnNet::NET_MOWER_ALTITUDE_MIN - 1.0f;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mRenderOrder = -1;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mRow = LawnNet::NET_MOWER_ROW_COUNT;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mState = LawnNet::NET_MOWER_STATE_MAX + 1;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mType = LawnNet::NET_MOWER_TYPE_MAX + 1;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mHeight = LawnNet::NET_MOWER_HEIGHT_MAX + 1;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mReanim.mLoopType = 6;
	CHECK(EncodeRejectedWithoutAppend(aState));
	aState = MakeValidState();
	aState.mReanim.mFrameCount = 0;
	CHECK(EncodeRejectedWithoutAppend(aState));

	for (uint8_t aType = 0; aType <= LawnNet::NET_MOWER_TYPE_MAX; ++aType)
	{
		LawnNet::NetBuffer aWire;
		CHECK(LawnNet::NetEncodeMowerSnapshotState(
			MakeValidState((uint16_t)(100 + aType), aType), aWire));
		LawnNet::NetMowerSnapshotState aDecoded;
		CHECK(LawnNet::NetDecodeMowerSnapshotState(aWire, aDecoded));
		CHECK(aDecoded.mType == aType);
	}
}

LawnNet::NetBuffer MakeSection(const std::vector<LawnNet::NetMowerSnapshotState>& theStates)
{
	LawnNet::NetBuffer aSection;
	aSection.WriteU16((uint16_t)theStates.size());
	for (const LawnNet::NetMowerSnapshotState& aState : theStates)
		CHECK(LawnNet::NetEncodeMowerSnapshotState(aState, aSection));
	return aSection;
}

void TestWholeSectionIsTransactionalAndBounded()
{
	std::vector<LawnNet::NetMowerSnapshotState> aStates;
	aStates.push_back(MakeValidState(1, 0));
	aStates.push_back(MakeValidState(2, 2));
	LawnNet::NetBuffer aSection = MakeSection(aStates);
	aSection.WriteU8(0xCCU);
	std::vector<LawnNet::NetMowerSnapshotState> aDecoded;
	CHECK(LawnNet::NetDecodeMowerSnapshotSection(aSection, aDecoded));
	CHECK(aDecoded.size() == 2U);
	CHECK(aDecoded[0].mId == 1U && aDecoded[1].mId == 2U);
	CHECK(aSection.Remaining() == 1U);
	CHECK(aSection.ReadU8Or() == 0xCCU);

	std::vector<LawnNet::NetMowerSnapshotState> aDuplicateStates;
	aDuplicateStates.push_back(MakeValidState(3, 0));
	aDuplicateStates.push_back(MakeValidState(3, 1));
	LawnNet::NetBuffer aDuplicate = MakeSection(aDuplicateStates);
	std::vector<LawnNet::NetMowerSnapshotState> anOutput(1,
		MakeSentinelState());
	const size_t aDuplicateStart = aDuplicate.mReadPos;
	CHECK(!LawnNet::NetDecodeMowerSnapshotSection(aDuplicate, anOutput));
	CHECK(aDuplicate.mReadPos == aDuplicateStart);
	CHECK(anOutput.size() == 1U && IsSentinelState(anOutput[0]));

	LawnNet::NetBuffer aTruncated = MakeSection(aStates);
	aTruncated.mData.pop_back();
	const size_t aTruncatedStart = aTruncated.mReadPos;
	CHECK(!LawnNet::NetDecodeMowerSnapshotSection(aTruncated, anOutput));
	CHECK(aTruncated.mReadPos == aTruncatedStart);
	CHECK(anOutput.size() == 1U && IsSentinelState(anOutput[0]));

	LawnNet::NetBuffer aTooMany;
	aTooMany.WriteU16((uint16_t)(LawnNet::NET_MOWER_MAX_COUNT + 1));
	const size_t aTooManyStart = aTooMany.mReadPos;
	CHECK(!LawnNet::NetDecodeMowerSnapshotSection(aTooMany, anOutput));
	CHECK(aTooMany.mReadPos == aTooManyStart);
	CHECK(anOutput.size() == 1U && IsSentinelState(anOutput[0]));
}
}

NetMowerSnapshotTestResult RunNetMowerSnapshotTests()
{
	gChecks = 0;
	gFailures = 0;
	std::printf("[ RUN      ] MowerSnapshot.GoldenVectorAndRoundTrip\n");
	TestGoldenVectorAndRoundTrip();
	std::printf("[ RUN      ] MowerSnapshot.MalformedTransactionalDecode\n");
	TestMalformedRecordsAreRejectedTransactionally();
	std::printf("[ RUN      ] MowerSnapshot.EncoderBoundsAndCleanerTypes\n");
	TestEncoderBoundsAndCleanerTypes();
	std::printf("[ RUN      ] MowerSnapshot.WholeSectionTransactionalBounds\n");
	TestWholeSectionIsTransactionalAndBounded();
	std::printf(gFailures == 0 ? "[       OK ] MowerSnapshot\n" :
		"[  FAILED  ] MowerSnapshot\n");
	return { gChecks, gFailures };
}
