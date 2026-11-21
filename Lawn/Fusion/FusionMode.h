#pragma once

#include <cstddef>
#include <cstdint>

namespace LawnFusion
{

// Retained for a future mixed mode. Public Fusion uses its own campaign.
inline constexpr bool kFusionClassicCampaignEnabled = false;

// Content selection is independent of the adventure/challenge GameMode enum.
enum class ContentMode : uint8_t
{
	Classic = 0,
	Fusion = 1
};

inline bool IsValidContentMode(ContentMode theMode)
{
	return theMode == ContentMode::Classic || theMode == ContentMode::Fusion;
}

inline const char* ContentModeName(ContentMode theMode)
{
	switch (theMode)
	{
	case ContentMode::Classic: return "Classic";
	case ContentMode::Fusion: return "Fusion";
	default: return "Unknown";
	}
}

template <typename Buffer>
bool EncodeContentMode(Buffer& theBuffer, ContentMode theMode)
{
	if (!IsValidContentMode(theMode)) return false;
	theBuffer.WriteU8(static_cast<uint8_t>(theMode));
	return true;
}

template <typename Buffer>
bool DecodeContentMode(Buffer& theBuffer, ContentMode& theMode)
{
	const size_t aStart = theBuffer.mReadPos;
	uint8_t aWire = 0;
	if (!theBuffer.ReadU8(&aWire) || !IsValidContentMode(static_cast<ContentMode>(aWire)))
	{
		theBuffer.mReadPos = aStart;
		return false;
	}
	theMode = static_cast<ContentMode>(aWire);
	return true;
}

}
