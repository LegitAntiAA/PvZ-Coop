#include "NetZombatarTests.h"
#include "../../Lawn/Net/NetZombatar.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <vector>

namespace
{
int gChecks, gFailures;
void Check(bool theCondition, int theLine)
{
	++gChecks;
	if (!theCondition)
	{
		++gFailures;
		std::printf("    Zombatar check failed line=%d\n", theLine);
	}
}
#define CHECK(theCondition) Check(!!(theCondition), __LINE__)

void GoldenAndLimits()
{
	using namespace LawnNet;
	NetZombatarAppearance aDefault, aDecoded;
	NetBuffer aWire;
	CHECK(NetEncodeZombatar(aDefault, aWire));
	CHECK(aWire.Size() == 13);
	CHECK(aWire.mData == (std::vector<uint8_t>{0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}));
	CHECK(NetDecodeZombatar(aWire, aDecoded) && aDecoded == aDefault && aWire.Remaining() == 0);
	NetZombatarAppearance aMax;
	aMax.mPresent = 1; aMax.mSkin = 11;
	aMax.mHair = 15; aMax.mHairColor = 16;
	aMax.mTidbit = 13; aMax.mTidbitColor = 16;
	aMax.mEyewear = 15; aMax.mEyewearColor = 16;
	aMax.mClothes = 11; aMax.mAccessory = 14; aMax.mAccessoryColor = 16;
	aMax.mHat = 13; aMax.mHatColor = 16;
	aWire.Clear();
	CHECK(NetEncodeZombatar(aMax, aWire));
	CHECK(aWire.mData == (std::vector<uint8_t>{1,11,15,16,13,16,15,16,11,14,16,13,16}));
	CHECK(NetDecodeZombatar(aWire, aDecoded) && aDecoded == aMax);
	CHECK(NetZombatarHex(aMax) == "010B0F100D100F100B0E100D10");
	CHECK(NetDecodeZombatarHex("010b0f100d100f100b0e100d10", aDecoded) && aDecoded == aMax);
	aDefault.mPresent = 1;
	aWire.Clear();
	CHECK(NetEncodeZombatar(aDefault, aWire));
	CHECK(NetDecodeZombatar(aWire, aDecoded) && aDecoded == aDefault);
}

void TransactionalAndMalformed()
{
	using namespace LawnNet;
	const std::vector<uint8_t> aValid = {1,11,15,16,13,16,15,16,11,14,16,13,16};
	for (size_t aLength = 0; aLength < aValid.size(); ++aLength)
	{
		NetBuffer aWire;
		aWire.WriteU8(99);
		aWire.WriteBytes(aValid.data(), aLength);
		aWire.mReadPos = 1;
		NetZombatarAppearance aBefore, aDecoded = aBefore;
		CHECK(!NetDecodeZombatar(aWire, aDecoded));
		CHECK(aWire.mReadPos == 1 && aDecoded == aBefore);
	}
	const uint8_t anInvalid[] = {2,12,16,17,14,17,16,17,12,15,17,14,17};
	for (size_t i = 0; i < aValid.size(); ++i)
	{
		NetBuffer aWire;
		aWire.WriteU8(99);
		aWire.WriteBytes(aValid.data(), aValid.size());
		aWire.mData[i + 1] = anInvalid[i];
		aWire.mReadPos = 1;
		NetZombatarAppearance aBefore, aDecoded = aBefore;
		CHECK(!NetDecodeZombatar(aWire, aDecoded));
		CHECK(aWire.mReadPos == 1 && aDecoded == aBefore);
	}
	NetZombatarAppearance aBad;
	aBad.mSkin = 1; // absent payload must be canonical
	NetBuffer aWire;
	aWire.WriteU8(77);
	CHECK(!NetEncodeZombatar(aBad, aWire) && aWire.Size() == 1);
	aBad.mPresent = 1; aBad.mSkin = 12;
	CHECK(!NetEncodeZombatar(aBad, aWire) && aWire.Size() == 1);
	NetZombatarAppearance aDecoded, aBefore = aDecoded;
	CHECK(!NetDecodeZombatarHex("010B0F100D100F100B0E100D1Z", aDecoded) && aDecoded == aBefore);
	CHECK(!NetDecodeZombatarHex("010B0F100D100F100B0E100D1000", aDecoded) && aDecoded == aBefore);
	CHECK(!NetDecodeZombatarHex("010C0F100D100F100B0E100D10", aDecoded) && aDecoded == aBefore);
	CHECK(!NetDecodeZombatarHex("", aDecoded) && aDecoded == aBefore);
}
}

NetZombatarTestResult RunNetZombatarTests()
{
	gChecks = gFailures = 0;
	std::printf("[ RUN      ] Zombatar.GoldenBoundsTransactionalConfig\n");
	GoldenAndLimits();
	TransactionalAndMalformed();
	std::printf(gFailures ? "[  FAILED  ] Zombatar\n" : "[       OK ] Zombatar\n");
	return {gChecks, gFailures};
}
