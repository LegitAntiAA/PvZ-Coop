#include "../../Lawn/Net/NetCommon.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include <process.h>

namespace
{

int gChecks = 0;
int gFailures = 0;
int gSkipped = 0;
bool gCurrentTestFailed = false;

void Check(bool theCondition, const char* theExpression, const char* theFile, int theLine)
{
	++gChecks;
	if (theCondition)
		return;

	++gFailures;
	gCurrentTestFailed = true;
	std::printf("    CHECK failed: %s (%s:%d)\n", theExpression, theFile, theLine);
}

#define CHECK(theExpression) Check(!!(theExpression), #theExpression, __FILE__, __LINE__)

void Skip(const char* theReason)
{
	++gSkipped;
	std::printf("    SKIP: %s\n", theReason);
}

template <typename TestFunction>
void RunTest(const char* theName, TestFunction theFunction)
{
	std::printf("[ RUN      ] %s\n", theName);
	gCurrentTestFailed = false;
	try
	{
		theFunction();
	}
	catch (const std::exception& anException)
	{
		++gFailures;
		gCurrentTestFailed = true;
		std::printf("    unexpected exception: %s\n", anException.what());
	}
	catch (...)
	{
		++gFailures;
		gCurrentTestFailed = true;
		std::printf("    unexpected non-standard exception\n");
	}

	std::printf(gCurrentTestFailed ? "[  FAILED  ] %s\n" : "[       OK ] %s\n", theName);
}

void TestNetBufferNumericRoundTrip()
{
	LawnNet::NetBuffer aBuffer;
	aBuffer.WriteU8(0xA5U);
	aBuffer.WriteU16(0xBEEFU);
	aBuffer.WriteU32(0x89ABCDEFU);
	aBuffer.WriteI16(-12345);
	aBuffer.WriteI32(-123456789);
	aBuffer.WriteF32(123.25f);

	CHECK(aBuffer.Size() == 17U);
	CHECK(aBuffer.Remaining() == 17U);

	uint8_t aU8 = 0;
	uint16_t aU16 = 0;
	uint32_t aU32 = 0;
	int16_t anI16 = 0;
	int32_t anI32 = 0;
	float aF32 = 0.0f;
	CHECK(aBuffer.ReadU8(&aU8));
	CHECK(aU8 == 0xA5U);
	CHECK(aBuffer.ReadU16(&aU16));
	CHECK(aU16 == 0xBEEFU);
	CHECK(aBuffer.ReadU32(&aU32));
	CHECK(aU32 == 0x89ABCDEFU);
	CHECK(aBuffer.ReadI16(&anI16));
	CHECK(anI16 == -12345);
	CHECK(aBuffer.ReadI32(&anI32));
	CHECK(anI32 == -123456789);
	CHECK(aBuffer.ReadF32(&aF32));
	CHECK(aF32 == 123.25f);
	CHECK(aBuffer.Remaining() == 0U);
}

void TestNetBufferTruncationIsBounded()
{
	LawnNet::NetBuffer aBuffer;
	aBuffer.WriteU8(0x11U);
	aBuffer.WriteU8(0x22U);

	uint8_t aFirst = 0;
	CHECK(aBuffer.ReadU8(&aFirst));
	CHECK(aFirst == 0x11U);
	CHECK(aBuffer.Remaining() == 1U);

	const size_t aReadPositionBeforeFailure = aBuffer.mReadPos;
	uint16_t aTooWide = 0xCAFEU;
	CHECK(!aBuffer.ReadU16(&aTooWide));
	CHECK(aBuffer.mReadPos == aReadPositionBeforeFailure);
	CHECK(aBuffer.Remaining() == 1U);
	CHECK(aTooWide == 0xCAFEU);

	uint8_t aLast = 0;
	CHECK(aBuffer.ReadU8(&aLast));
	CHECK(aLast == 0x22U);
	CHECK(aBuffer.Remaining() == 0U);
	CHECK(!aBuffer.ReadU8(&aLast));
	CHECK(aBuffer.Remaining() == 0U);
}

void TestNetBufferStrings()
{
	LawnNet::NetBuffer aBuffer;
	aBuffer.WriteString("Peashooter");
	char aString[32] = {};
	CHECK(aBuffer.ReadString(aString, static_cast<int>(sizeof(aString))));
	CHECK(std::strcmp(aString, "Peashooter") == 0);
	CHECK(aBuffer.Remaining() == 0U);

	std::string aLongString(300U, 'z');
	LawnNet::NetBuffer aLongBuffer;
	aLongBuffer.WriteString(aLongString.c_str());
	CHECK(aLongBuffer.Size() == 256U);
	CHECK(aLongBuffer.mData[0] == 255U);

	char aLongOutput[301] = {};
	CHECK(aLongBuffer.ReadString(aLongOutput, static_cast<int>(sizeof(aLongOutput))));
	CHECK(std::strlen(aLongOutput) == 255U);
	CHECK(std::string(aLongOutput) == aLongString.substr(0U, 255U));
	CHECK(aLongBuffer.Remaining() == 0U);

	LawnNet::NetBuffer aSmallOutputBuffer;
	aSmallOutputBuffer.WriteString("sunflower");
	char aSmallOutput[5] = {};
	CHECK(aSmallOutputBuffer.ReadString(aSmallOutput, static_cast<int>(sizeof(aSmallOutput))));
	CHECK(std::strcmp(aSmallOutput, "sunf") == 0);
	CHECK(aSmallOutputBuffer.Remaining() == 0U);
}

void TestFrameGoldenRoundTrip()
{
	LawnNet::NetBuffer aPayload;
	aPayload.WriteU8(0x11U);
	aPayload.WriteU8(0x22U);

	LawnNet::NetBuffer aFrame;
	CHECK(LawnNet::NetEncodeFrame(aPayload, 0x7FU, aFrame));
	const std::vector<uint8_t> anExpected = { 0x02U, 0x00U, 0x7FU, 0x11U, 0x22U };
	CHECK(aFrame.mData == anExpected);

	uint8_t aType = 0;
	LawnNet::NetBuffer aDecoded;
	CHECK(LawnNet::NetDecodeFrame(aFrame.Data(), aFrame.Size(), &aType, aDecoded));
	CHECK(aType == 0x7FU);
	CHECK(aDecoded.mData == aPayload.mData);
	CHECK(aDecoded.mReadPos == 0U);
	CHECK(aDecoded.Remaining() == 2U);
}

void TestFrameZeroPayload()
{
	LawnNet::NetBuffer anEmptyPayload;
	LawnNet::NetBuffer aFrame;
	CHECK(LawnNet::NetEncodeFrame(anEmptyPayload, LawnNet::NETMSG_PING, aFrame));
	const std::vector<uint8_t> anExpected = { 0x00U, 0x00U, LawnNet::NETMSG_PING };
	CHECK(aFrame.mData == anExpected);

	uint8_t aType = 0;
	LawnNet::NetBuffer aDecoded;
	CHECK(LawnNet::NetDecodeFrame(aFrame.Data(), aFrame.Size(), &aType, aDecoded));
	CHECK(aType == LawnNet::NETMSG_PING);
	CHECK(aDecoded.Size() == 0U);
	CHECK(aDecoded.Remaining() == 0U);
}

void TestFrameRejectsMalformedSizes()
{
	uint8_t aType = 0xCCU;
	LawnNet::NetBuffer aDecoded;
	aDecoded.WriteU8(0xAAU);

	const uint8_t aZeroBytes[1] = { 0U };
	const uint8_t aOneByte[1] = { 0U };
	const uint8_t aTwoBytes[2] = { 0U, 0U };
	const uint8_t aTruncatedPayload[4] = { 2U, 0U, 9U, 0x44U };
	const uint8_t aTrailingByte[5] = { 1U, 0U, 9U, 0x44U, 0x55U };
	const uint8_t anExtraOnEmpty[4] = { 0U, 0U, 9U, 0x44U };

	CHECK(!LawnNet::NetDecodeFrame(aZeroBytes, 0U, &aType, aDecoded));
	CHECK(!LawnNet::NetDecodeFrame(aOneByte, 1U, &aType, aDecoded));
	CHECK(!LawnNet::NetDecodeFrame(aTwoBytes, 2U, &aType, aDecoded));
	CHECK(!LawnNet::NetDecodeFrame(aTruncatedPayload, sizeof(aTruncatedPayload), &aType, aDecoded));
	CHECK(!LawnNet::NetDecodeFrame(aTrailingByte, sizeof(aTrailingByte), &aType, aDecoded));
	CHECK(!LawnNet::NetDecodeFrame(anExtraOnEmpty, sizeof(anExtraOnEmpty), &aType, aDecoded));

	// A rejected frame must not expose a partial payload or overwrite the type.
	CHECK(aType == 0xCCU);
	CHECK(aDecoded.Size() == 1U);
	CHECK(aDecoded.mData[0] == 0xAAU);
}

void TestFrameMaximumPayload()
{
	std::vector<uint8_t> aBytes(static_cast<size_t>(LawnNet::NET_MAX_FRAME));
	for (size_t i = 0; i < aBytes.size(); ++i)
		aBytes[i] = static_cast<uint8_t>((i * 37U) & 0xFFU);

	LawnNet::NetBuffer aPayload;
	aPayload.WriteBytes(aBytes.data(), aBytes.size());
	LawnNet::NetBuffer aFrame;
	CHECK(LawnNet::NetEncodeFrame(aPayload, LawnNet::NETMSG_SNAPSHOT, aFrame));
	CHECK(aFrame.Size() == aBytes.size() + 3U);
	CHECK(aFrame.mData[0] == 0xFFU);
	CHECK(aFrame.mData[1] == 0xFFU);

	uint8_t aType = 0;
	LawnNet::NetBuffer aDecoded;
	CHECK(LawnNet::NetDecodeFrame(aFrame.Data(), aFrame.Size(), &aType, aDecoded));
	CHECK(aType == LawnNet::NETMSG_SNAPSHOT);
	CHECK(aDecoded.mData == aBytes);

	LawnNet::NetBuffer anOversizedPayload;
	std::vector<uint8_t> anOversizedBytes(static_cast<size_t>(LawnNet::NET_MAX_FRAME) + 1U, 0x5AU);
	anOversizedPayload.WriteBytes(anOversizedBytes.data(), anOversizedBytes.size());
	LawnNet::NetBuffer anOutput;
	anOutput.WriteU8(0xA5U);
	CHECK(!LawnNet::NetEncodeFrame(anOversizedPayload, LawnNet::NETMSG_SNAPSHOT, anOutput));
	CHECK(anOutput.Size() == 1U);
	CHECK(anOutput.mData[0] == 0xA5U);
}

bool ColorEquals(const LawnNet::NetColor& theColor, uint8_t theRed, uint8_t theGreen, uint8_t theBlue)
{
	return theColor.r == theRed && theColor.g == theGreen && theColor.b == theBlue;
}

void TestRelativePlayerColors()
{
	for (int aViewer = 0; aViewer < LawnNet::NET_MAX_PLAYERS; ++aViewer)
	{
		CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(aViewer, aViewer), 255U, 255U, 255U));
		CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(aViewer, (aViewer + 1) % LawnNet::NET_MAX_PLAYERS), 70U, 130U, 255U));
		CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(aViewer, (aViewer + 2) % LawnNet::NET_MAX_PLAYERS), 255U, 70U, 70U));
		CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(aViewer, (aViewer + 3) % LawnNet::NET_MAX_PLAYERS), 80U, 200U, 80U));
	}

	CHECK(ColorEquals(LawnNet::GetPlayerColorBySlot(0), 255U, 255U, 255U));
	CHECK(ColorEquals(LawnNet::GetPlayerColorBySlot(1), 70U, 130U, 255U));
	CHECK(ColorEquals(LawnNet::GetPlayerColorBySlot(2), 255U, 70U, 70U));
	CHECK(ColorEquals(LawnNet::GetPlayerColorBySlot(3), 80U, 200U, 80U));
}

void TestConfigDefaults()
{
	LawnNet::NetConfig aConfig;
	CHECK(std::strcmp(aConfig.mNick, "Player") == 0);
	CHECK(!aConfig.mUnlockAllMinigames);
	CHECK(aConfig.mEnableCoop);
	CHECK(aConfig.mBaseSnapshotHz == LawnNet::NET_SNAPSHOT_HZ_DEFAULT);
	CHECK(aConfig.mWindowWidth == 0);
	CHECK(aConfig.mWindowHeight == 0);
	CHECK(aConfig.mBorderlessFullscreen);

	std::strcpy(aConfig.mNick, "Changed");
	aConfig.mUnlockAllMinigames = true;
	aConfig.mEnableCoop = false;
	aConfig.mBaseSnapshotHz = 29;
	aConfig.mWindowWidth = 1920;
	aConfig.mWindowHeight = 1080;
	aConfig.mBorderlessFullscreen = false;
	aConfig.Reset();
	CHECK(std::strcmp(aConfig.mNick, "Player") == 0);
	CHECK(!aConfig.mUnlockAllMinigames);
	CHECK(aConfig.mEnableCoop);
	CHECK(aConfig.mBaseSnapshotHz == LawnNet::NET_SNAPSHOT_HZ_DEFAULT);
	CHECK(aConfig.mWindowWidth == 0);
	CHECK(aConfig.mWindowHeight == 0);
	CHECK(aConfig.mBorderlessFullscreen);
}

class ScopedTemporaryWorkingDirectory
{
public:
	ScopedTemporaryWorkingDirectory() : mCreatedDirectory(false), mChangedDirectory(false), mIsolated(false)
	{
		std::error_code anError;
		mOriginal = std::filesystem::current_path(anError);
		if (anError)
			return;

		const long long aNonce = std::chrono::steady_clock::now().time_since_epoch().count();
		mTemporary = std::filesystem::temp_directory_path(anError) /
			("PvZNetProtocolTests_" + std::to_string(_getpid()) + "_" + std::to_string(aNonce));
		if (anError || !std::filesystem::create_directory(mTemporary, anError) || anError)
			return;
		mCreatedDirectory = true;

		std::filesystem::current_path(mTemporary, anError);
		if (anError)
			return;
		mChangedDirectory = true;

		std::error_code aCurrentError;
		const std::filesystem::path aCurrent = std::filesystem::current_path(aCurrentError);
		std::error_code anEquivalentError;
		mIsolated = !aCurrentError && std::filesystem::equivalent(aCurrent, mTemporary, anEquivalentError) && !anEquivalentError;
	}

	~ScopedTemporaryWorkingDirectory()
	{
		std::error_code anIgnoredError;
		if (mChangedDirectory)
			std::filesystem::current_path(mOriginal, anIgnoredError);
		if (mCreatedDirectory)
		{
			std::filesystem::remove(mTemporary / "coop.ini", anIgnoredError);
			std::filesystem::remove(mTemporary, anIgnoredError);
		}
	}

	bool IsIsolated() const { return mIsolated; }

private:
	std::filesystem::path mOriginal;
	std::filesystem::path mTemporary;
	bool mCreatedDirectory;
	bool mChangedDirectory;
	bool mIsolated;
};

void TestConfigRoundTripInIsolatedDirectory()
{
	ScopedTemporaryWorkingDirectory aWorkingDirectory;
	if (!aWorkingDirectory.IsIsolated())
	{
		Skip("could not prove that the temporary working directory is isolated");
		return;
	}

	LawnNet::NetConfig aSaved;
	std::strcpy(aSaved.mNick, "UnitTester");
	aSaved.mUnlockAllMinigames = true;
	aSaved.mEnableCoop = false;
	aSaved.mBaseSnapshotHz = 29;
	aSaved.mWindowWidth = 1920;
	aSaved.mWindowHeight = 1080;
	aSaved.mBorderlessFullscreen = false;
	CHECK(LawnNet::NetSaveConfig(aSaved));
	CHECK(std::filesystem::is_regular_file("coop.ini"));

	LawnNet::NetConfig aLoaded;
	CHECK(LawnNet::NetLoadConfig(aLoaded));
	CHECK(std::strcmp(aLoaded.mNick, "UnitTester") == 0);
	CHECK(aLoaded.mUnlockAllMinigames);
	CHECK(!aLoaded.mEnableCoop);
	CHECK(aLoaded.mBaseSnapshotHz == 29);
	CHECK(aLoaded.mWindowWidth == 1920);
	CHECK(aLoaded.mWindowHeight == 1080);
	CHECK(!aLoaded.mBorderlessFullscreen);
}

} // namespace

int main()
{
	std::printf("PvZ NetProtocolTests (protocol %u)\n", LawnNet::NET_PROTOCOL_VERSION);

	RunTest("NetBuffer.NumericRoundTrip", TestNetBufferNumericRoundTrip);
	RunTest("NetBuffer.TruncationIsBounded", TestNetBufferTruncationIsBounded);
	RunTest("NetBuffer.Strings", TestNetBufferStrings);
	RunTest("Frame.GoldenRoundTrip", TestFrameGoldenRoundTrip);
	RunTest("Frame.ZeroPayload", TestFrameZeroPayload);
	RunTest("Frame.RejectsMalformedSizes", TestFrameRejectsMalformedSizes);
	RunTest("Frame.MaximumPayload", TestFrameMaximumPayload);
	RunTest("Players.RelativeColors", TestRelativePlayerColors);
	RunTest("Config.Defaults", TestConfigDefaults);
	RunTest("Config.IsolatedRoundTrip", TestConfigRoundTripInIsolatedDirectory);

	std::printf("\nchecks=%d failures=%d skipped=%d\n", gChecks, gFailures, gSkipped);
	return gFailures == 0 ? 0 : 1;
}
