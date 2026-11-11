#include "NetZombatar.h"
#include "NetCommon.h"

namespace LawnNet
{
bool NetZombatarAppearance::operator==(const NetZombatarAppearance& a) const
{
	return mPresent == a.mPresent && mSkin == a.mSkin &&
		mHair == a.mHair && mHairColor == a.mHairColor &&
		mTidbit == a.mTidbit && mTidbitColor == a.mTidbitColor &&
		mEyewear == a.mEyewear && mEyewearColor == a.mEyewearColor &&
		mClothes == a.mClothes && mAccessory == a.mAccessory &&
		mAccessoryColor == a.mAccessoryColor && mHat == a.mHat && mHatColor == a.mHatColor;
}

bool NetValidateZombatar(const NetZombatarAppearance& a)
{
	if (a.mPresent == 0)
		return a == NetZombatarAppearance();
	if (a.mPresent != 1 || a.mSkin >= 12)
		return false;
	auto OptionalIndex = [](uint8_t theValue, uint8_t theCount)
	{
		return theValue == 255 || theValue < theCount;
	};
	return OptionalIndex(a.mHair, 16) && OptionalIndex(a.mTidbit, 14) &&
		OptionalIndex(a.mEyewear, 16) && OptionalIndex(a.mClothes, 12) &&
		OptionalIndex(a.mAccessory, 15) && OptionalIndex(a.mHat, 14) &&
		OptionalIndex(a.mHairColor, 17) && OptionalIndex(a.mTidbitColor, 17) &&
		OptionalIndex(a.mEyewearColor, 17) && OptionalIndex(a.mAccessoryColor, 17) &&
		OptionalIndex(a.mHatColor, 17);
}

bool NetEncodeZombatar(const NetZombatarAppearance& a, NetBuffer& b)
{
	if (!NetValidateZombatar(a))
		return false;
	b.WriteU8(a.mPresent); b.WriteU8(a.mSkin);
	b.WriteU8(a.mHair); b.WriteU8(a.mHairColor);
	b.WriteU8(a.mTidbit); b.WriteU8(a.mTidbitColor);
	b.WriteU8(a.mEyewear); b.WriteU8(a.mEyewearColor);
	b.WriteU8(a.mClothes);
	b.WriteU8(a.mAccessory); b.WriteU8(a.mAccessoryColor);
	b.WriteU8(a.mHat); b.WriteU8(a.mHatColor);
	return true;
}

bool NetDecodeZombatar(NetBuffer& b, NetZombatarAppearance& a)
{
	if (b.Remaining() < NET_ZOMBATAR_WIRE_BYTES)
		return false;
	const size_t aReadPos = b.mReadPos;
	NetZombatarAppearance aDecoded;
	aDecoded.mPresent = b.ReadU8Or(); aDecoded.mSkin = b.ReadU8Or();
	aDecoded.mHair = b.ReadU8Or(); aDecoded.mHairColor = b.ReadU8Or();
	aDecoded.mTidbit = b.ReadU8Or(); aDecoded.mTidbitColor = b.ReadU8Or();
	aDecoded.mEyewear = b.ReadU8Or(); aDecoded.mEyewearColor = b.ReadU8Or();
	aDecoded.mClothes = b.ReadU8Or();
	aDecoded.mAccessory = b.ReadU8Or(); aDecoded.mAccessoryColor = b.ReadU8Or();
	aDecoded.mHat = b.ReadU8Or(); aDecoded.mHatColor = b.ReadU8Or();
	if (!NetValidateZombatar(aDecoded))
	{
		b.mReadPos = aReadPos;
		return false;
	}
	a = aDecoded;
	return true;
}

std::string NetZombatarHex(const NetZombatarAppearance& a)
{
	NetBuffer aWire;
	if (!NetEncodeZombatar(a, aWire))
		return std::string();
	static const char aHex[] = "0123456789ABCDEF";
	std::string aResult;
	for (uint8_t aByte : aWire.mData)
	{
		aResult += aHex[aByte >> 4];
		aResult += aHex[aByte & 15];
	}
	return aResult;
}

bool NetDecodeZombatarHex(const std::string& theHex, NetZombatarAppearance& a)
{
	if (theHex.size() != NET_ZOMBATAR_WIRE_BYTES * 2)
		return false;
	auto Digit = [](char theChar) -> int
	{
		if (theChar >= '0' && theChar <= '9') return theChar - '0';
		if (theChar >= 'A' && theChar <= 'F') return theChar - 'A' + 10;
		if (theChar >= 'a' && theChar <= 'f') return theChar - 'a' + 10;
		return -1;
	};
	NetBuffer aWire;
	for (size_t i = 0; i < theHex.size(); i += 2)
	{
		const int aHigh = Digit(theHex[i]), aLow = Digit(theHex[i + 1]);
		if (aHigh < 0 || aLow < 0)
			return false;
		aWire.WriteU8((uint8_t)((aHigh << 4) | aLow));
	}
	return NetDecodeZombatar(aWire, a);
}
}
