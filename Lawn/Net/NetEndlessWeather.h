#ifndef __NETENDLESSWEATHER_H__
#define __NETENDLESSWEATHER_H__

#include <array>
#include <cstddef>
#include <cstdint>

namespace LawnNet
{
constexpr size_t NET_ENDLESS_FOG_COLUMNS = 13;
constexpr size_t NET_ENDLESS_FOG_ROWS = 7;
constexpr size_t NET_ENDLESS_FOG_CELLS = NET_ENDLESS_FOG_COLUMNS * NET_ENDLESS_FOG_ROWS;
constexpr size_t NET_ENDLESS_WEATHER_WIRE_BYTES = 12 + NET_ENDLESS_FOG_CELLS;

struct NetEndlessWeatherState
{
	uint8_t mDayPhase = 0;
	uint16_t mWeatherTimer = 0;
	uint16_t mFogTimer = 0;
	uint16_t mStormTimer = 0;
	uint16_t mNightBlend = 0;
	uint8_t mStormFlashKind = 0;
	uint16_t mStormFlashCounter = 0;
	// Native [column][row] order, including the four extra columns and bottom row.
	std::array<uint8_t, NET_ENDLESS_FOG_CELLS> mFogGrid{};
};

inline bool NetEndlessWeatherValid(const NetEndlessWeatherState& state, bool endlessT)
{
	if (state.mDayPhase > 1 || state.mWeatherTimer > 6000 || state.mFogTimer > 3000 ||
		state.mStormTimer > 2000 || state.mNightBlend > 1000 || state.mStormFlashKind > 3 ||
		state.mStormFlashCounter > 900 || (!state.mStormFlashKind && state.mStormFlashCounter) ||
		(!state.mStormTimer && (state.mStormFlashKind || state.mStormFlashCounter)))
		return false;
	// Ordinary fog levels still carry their native alpha grid, without T/ST weather.
	return endlessT || (!state.mDayPhase && !state.mWeatherTimer && !state.mFogTimer &&
		!state.mStormTimer && !state.mNightBlend && !state.mStormFlashKind && !state.mStormFlashCounter);
}

template <class Buffer>
inline bool NetEncodeEndlessWeather(const NetEndlessWeatherState& state, Buffer& buffer, bool endlessT)
{
	if (!NetEndlessWeatherValid(state, endlessT)) return false;
	buffer.WriteU8(state.mDayPhase);
	buffer.WriteU16(state.mWeatherTimer);
	buffer.WriteU16(state.mFogTimer);
	buffer.WriteU16(state.mStormTimer);
	buffer.WriteU16(state.mNightBlend);
	buffer.WriteU8(state.mStormFlashKind);
	buffer.WriteU16(state.mStormFlashCounter);
	for (uint8_t alpha : state.mFogGrid) buffer.WriteU8(alpha);
	return true;
}

template <class Buffer>
inline bool NetDecodeEndlessWeather(Buffer& buffer, NetEndlessWeatherState& state, bool endlessT)
{
	const size_t start = buffer.mReadPos;
	NetEndlessWeatherState candidate;
	bool valid = buffer.ReadU8(&candidate.mDayPhase) && buffer.ReadU16(&candidate.mWeatherTimer) &&
		buffer.ReadU16(&candidate.mFogTimer) && buffer.ReadU16(&candidate.mStormTimer) &&
		buffer.ReadU16(&candidate.mNightBlend) && buffer.ReadU8(&candidate.mStormFlashKind) &&
		buffer.ReadU16(&candidate.mStormFlashCounter);
	for (uint8_t& alpha : candidate.mFogGrid)
		if (valid) valid = buffer.ReadU8(&alpha);
	if (!valid || !NetEndlessWeatherValid(candidate, endlessT))
	{
		buffer.mReadPos = start;
		return false;
	}
	state = candidate;
	return true;
}
}
#endif
