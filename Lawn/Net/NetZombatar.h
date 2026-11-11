#ifndef __NETZOMBATAR_H__
#define __NETZOMBATAR_H__

#include <cstdint>
#include <cstddef>
#include <string>

namespace LawnNet
{
class NetBuffer;

// Category-relative indexes, never resource handles or file names. 255 means
// no item/palette. Each zombie retains its own value after a player leaves.
struct NetZombatarAppearance
{
	uint8_t mPresent = 0;
	uint8_t mSkin = 0;
	uint8_t mHair = 255, mHairColor = 255;
	uint8_t mTidbit = 255, mTidbitColor = 255;
	uint8_t mEyewear = 255, mEyewearColor = 255;
	uint8_t mClothes = 255;
	uint8_t mAccessory = 255, mAccessoryColor = 255;
	uint8_t mHat = 255, mHatColor = 255;

	bool operator==(const NetZombatarAppearance& theOther) const;
	bool operator!=(const NetZombatarAppearance& theOther) const { return !(*this == theOther); }
};

static const size_t NET_ZOMBATAR_WIRE_BYTES = 13;
bool NetValidateZombatar(const NetZombatarAppearance& theAppearance);
bool NetEncodeZombatar(const NetZombatarAppearance& theAppearance, NetBuffer& theBuffer);
bool NetDecodeZombatar(NetBuffer& theBuffer, NetZombatarAppearance& theAppearance);
std::string NetZombatarHex(const NetZombatarAppearance& theAppearance);
bool NetDecodeZombatarHex(const std::string& theHex, NetZombatarAppearance& theAppearance);
}

#endif
