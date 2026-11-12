#include "NetEndlessWeatherTests.h"
#include "../../Lawn/Net/NetEndlessWeather.h"
#include <cstdio>
#include <vector>

namespace
{
int checks, failures;
void Check(bool condition, int line)
{
	++checks;
	if (!condition) { ++failures; std::printf("    EndlessWeather check failed line=%d\n", line); }
}
#define CHECK(condition) Check(!!(condition), __LINE__)

// Exercise the production buffer-template codec without game or Windows headers.
struct Buffer
{
	std::vector<uint8_t> mData;
	size_t mReadPos = 0;
	size_t Remaining() const { return mData.size() - mReadPos; }
	void WriteU8(uint8_t value) { mData.push_back(value); }
	void WriteU16(uint16_t value) { WriteU8((uint8_t)value); WriteU8((uint8_t)(value >> 8)); }
	bool ReadU8(uint8_t* value)
	{ if (!Remaining()) return false; *value = mData[mReadPos++]; return true; }
	bool ReadU16(uint16_t* value)
	{ if (Remaining() < 2) return false; *value = mData[mReadPos] | (uint16_t)(mData[mReadPos + 1] << 8); mReadPos += 2; return true; }
};
using namespace LawnNet;

bool Same(const NetEndlessWeatherState& a, const NetEndlessWeatherState& b)
{
	return a.mDayPhase == b.mDayPhase && a.mWeatherTimer == b.mWeatherTimer && a.mFogTimer == b.mFogTimer &&
		a.mStormTimer == b.mStormTimer && a.mNightBlend == b.mNightBlend && a.mStormFlashKind == b.mStormFlashKind &&
		a.mStormFlashCounter == b.mStormFlashCounter && a.mFogGrid == b.mFogGrid;
}

Buffer Bytes(const NetEndlessWeatherState& state)
{
	Buffer b;
	b.WriteU8(state.mDayPhase); b.WriteU16(state.mWeatherTimer); b.WriteU16(state.mFogTimer);
	b.WriteU16(state.mStormTimer); b.WriteU16(state.mNightBlend); b.WriteU8(state.mStormFlashKind);
	b.WriteU16(state.mStormFlashCounter);
	for (uint8_t alpha : state.mFogGrid) b.WriteU8(alpha);
	return b;
}

void RoundTrips()
{
	NetEndlessWeatherState state;
	state.mDayPhase = 1; state.mWeatherTimer = 6000; state.mFogTimer = 3000;
	state.mStormTimer = 2000; state.mNightBlend = 500; state.mStormFlashKind = 3; state.mStormFlashCounter = 900;
	for (size_t x = 0; x < NET_ENDLESS_FOG_COLUMNS; ++x)
		for (size_t y = 0; y < NET_ENDLESS_FOG_ROWS; ++y)
			state.mFogGrid[x * NET_ENDLESS_FOG_ROWS + y] = (uint8_t)(x * 17 + y);
	Buffer wire;
	CHECK(NetEncodeEndlessWeather(state, wire, true));
	CHECK(wire.mData.size() == 103 && NET_ENDLESS_FOG_CELLS == 91);
	CHECK((std::vector<uint8_t>(wire.mData.begin(), wire.mData.begin() + 12) ==
		std::vector<uint8_t>{1, 0x70, 0x17, 0xB8, 0x0B, 0xD0, 0x07, 0xF4, 0x01, 3, 0x84, 0x03}));
	for (size_t i = 0; i < NET_ENDLESS_FOG_CELLS; ++i) CHECK(wire.mData[12 + i] == state.mFogGrid[i]);
	NetEndlessWeatherState decoded;
	wire.WriteU8(0xA5); // Embedded block must leave the following snapshot section untouched.
	CHECK(NetDecodeEndlessWeather(wire, decoded, true) && Same(decoded, state));
	CHECK(wire.Remaining() == 1 && wire.mData[wire.mReadPos] == 0xA5);
	// Native Plantern holes/Blover clearing use all byte alphas, even outside T/ST.
	state = NetEndlessWeatherState();
	for (size_t i = 0; i < state.mFogGrid.size(); ++i) state.mFogGrid[i] = i % 3 == 0 ? 0 : i % 3 == 1 ? 200 : 255;
	wire = Buffer();
	CHECK(NetEncodeEndlessWeather(state, wire, false));
	CHECK(NetDecodeEndlessWeather(wire, decoded, false) && Same(decoded, state));
	for (uint16_t blend : {uint16_t(0), uint16_t(499), uint16_t(500), uint16_t(501), uint16_t(1000)})
		for (uint8_t phase : {uint8_t(0), uint8_t(1)})
		{
			state.mNightBlend = blend; state.mDayPhase = phase;
			wire = Buffer();
			CHECK(NetEncodeEndlessWeather(state, wire, true) && NetDecodeEndlessWeather(wire, decoded, true) && Same(decoded, state));
		}
	for (uint8_t kind = 1; kind <= 3; ++kind)
	{
		state.mStormTimer = 1; state.mStormFlashKind = kind; state.mStormFlashCounter = 0;
		wire = Buffer();
		CHECK(NetEncodeEndlessWeather(state, wire, true) && NetDecodeEndlessWeather(wire, decoded, true));
	}
}

void Rejections()
{
	NetEndlessWeatherState state;
	state.mDayPhase = 1; state.mNightBlend = 777; state.mWeatherTimer = 1234;
	state.mFogTimer = 2222; state.mStormTimer = 1111; state.mStormFlashKind = 2; state.mStormFlashCounter = 444;
	state.mFogGrid.fill(255);
	const Buffer valid = Bytes(state);
	for (size_t length = 0; length < NET_ENDLESS_WEATHER_WIRE_BYTES; ++length)
	{
		Buffer raw; raw.WriteU8(99); raw.mData.insert(raw.mData.end(), valid.mData.begin(), valid.mData.begin() + length); raw.mReadPos = 1;
		NetEndlessWeatherState decoded = state;
		CHECK(!NetDecodeEndlessWeather(raw, decoded, true) && raw.mReadPos == 1 && Same(decoded, state));
	}
	std::vector<NetEndlessWeatherState> invalid;
	auto bad = state; bad.mDayPhase = 2; invalid.push_back(bad);
	bad = state; bad.mWeatherTimer = 6001; invalid.push_back(bad);
	bad = state; bad.mFogTimer = 3001; invalid.push_back(bad);
	bad = state; bad.mStormTimer = 2001; invalid.push_back(bad);
	bad = state; bad.mNightBlend = 1001; invalid.push_back(bad);
	bad = state; bad.mStormFlashKind = 4; invalid.push_back(bad);
	bad = state; bad.mStormFlashCounter = 901; invalid.push_back(bad);
	bad = state; bad.mStormFlashKind = 0; invalid.push_back(bad);
	bad = state; bad.mStormTimer = 0; invalid.push_back(bad);
	bad = state; bad.mStormTimer = 0; bad.mStormFlashCounter = 0; invalid.push_back(bad);
	for (const auto& value : invalid)
	{
		Buffer output; output.WriteU8(99);
		CHECK(!NetEncodeEndlessWeather(value, output, true) && output.mData == std::vector<uint8_t>{99});
		Buffer raw = Bytes(value); raw.mData.insert(raw.mData.begin(), 99); raw.mReadPos = 1;
		NetEndlessWeatherState decoded = state;
		CHECK(!NetDecodeEndlessWeather(raw, decoded, true) && raw.mReadPos == 1 && Same(decoded, state));
	}
	// Each T/ST-only field is independently rejected in an ordinary native fog level.
	for (int field = 0; field < 7; ++field)
	{
		NetEndlessWeatherState weather;
		switch (field)
		{
		case 0: weather.mDayPhase = 1; break;
		case 1: weather.mWeatherTimer = 1; break;
		case 2: weather.mFogTimer = 1; break;
		case 3: weather.mStormTimer = 1; break;
		case 4: weather.mNightBlend = 1; break;
		case 5: weather.mStormTimer = 1; weather.mStormFlashKind = 1; break;
		case 6: weather.mStormTimer = 1; weather.mStormFlashKind = 1; weather.mStormFlashCounter = 1; break;
		}
		Buffer output; output.WriteU8(99);
		CHECK(!NetEncodeEndlessWeather(weather, output, false) && output.mData.size() == 1);
		Buffer raw = Bytes(weather);
		NetEndlessWeatherState decoded = state;
		CHECK(!NetDecodeEndlessWeather(raw, decoded, false) && raw.mReadPos == 0 && Same(decoded, state));
	}
}
}

NetEndlessWeatherTestResult RunNetEndlessWeatherTests()
{
	checks = failures = 0;
	std::printf("[ RUN      ] EndlessWeather.NativeFogGridSnapshot\n");
	RoundTrips(); Rejections();
	std::printf(failures ? "[  FAILED  ] EndlessWeather\n" : "[       OK ] EndlessWeather\n");
	return {checks, failures};
}

#ifdef NET_ENDLESS_WEATHER_TEST_STANDALONE
int main()
{
	const auto result = RunNetEndlessWeatherTests();
	std::printf("checks=%d failures=%d\n", result.mChecks, result.mFailures);
	return result.mFailures ? 1 : 0;
}
#endif
