#include "../../Lawn/Net/NetCommon.h"
#include "LocalizedTextTests.h"
#include "NetChallengeRuntimeTests.h"
#include "NetEndlessWireTests.h"
#include "NetMowerSnapshotTests.h"
#include "NetZombieAnimTests.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include <process.h>
#include <windows.h>

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
                int aRemoteIndex = 0;
                for (int anOwner = 0; anOwner < LawnNet::NET_MAX_PLAYERS; ++anOwner)
                {
                        if (anOwner == aViewer)
                                continue;
                        static const uint8_t aExpected[3][3] =
                        {
                                { 70U, 130U, 255U },
                                { 255U, 70U, 70U },
                                { 80U, 200U, 80U }
                        };
                        CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(aViewer, anOwner),
                                aExpected[aRemoteIndex][0], aExpected[aRemoteIndex][1],
                                aExpected[aRemoteIndex][2]));
                        ++aRemoteIndex;
                }
        }

        CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(0, 1), 70U, 130U, 255U));
        CHECK(ColorEquals(LawnNet::GetRelativePlayerColor(1, 0), 70U, 130U, 255U));

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
        CHECK(aConfig.mHeldPlantTintAlpha == 150);
        CHECK(aConfig.mHotbarTintAlpha == 110);
        CHECK(aConfig.mHeldPlantOutlineAlpha == 180);

	std::strcpy(aConfig.mNick, "Changed");
	aConfig.mUnlockAllMinigames = true;
	aConfig.mEnableCoop = false;
	aConfig.mBaseSnapshotHz = 29;
	aConfig.mWindowWidth = 1920;
        aConfig.mWindowHeight = 1080;
        aConfig.mBorderlessFullscreen = false;
        aConfig.mHeldPlantTintAlpha = 1;
        aConfig.mHotbarTintAlpha = 2;
        aConfig.mHeldPlantOutlineAlpha = 3;
	aConfig.Reset();
	CHECK(std::strcmp(aConfig.mNick, "Player") == 0);
	CHECK(!aConfig.mUnlockAllMinigames);
	CHECK(aConfig.mEnableCoop);
	CHECK(aConfig.mBaseSnapshotHz == LawnNet::NET_SNAPSHOT_HZ_DEFAULT);
	CHECK(aConfig.mWindowWidth == 0);
        CHECK(aConfig.mWindowHeight == 0);
        CHECK(aConfig.mBorderlessFullscreen);
        CHECK(aConfig.mHeldPlantTintAlpha == 150);
        CHECK(aConfig.mHotbarTintAlpha == 110);
        CHECK(aConfig.mHeldPlantOutlineAlpha == 180);
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

bool WriteSmokeTestFile(const std::filesystem::path& thePath, const std::string& theText)
{
	FILE* aFile = nullptr;
	if (fopen_s(&aFile, thePath.string().c_str(), "wb") != 0 || !aFile)
		return false;
	const bool aWritten = std::fwrite(theText.data(), 1U, theText.size(), aFile) == theText.size();
	const bool aClosed = std::fclose(aFile) == 0;
	return aWritten && aClosed;
}

class ScopedSmokeManifestTree
{
public:
	ScopedSmokeManifestTree() : mReady(false), mEnvironmentTouched(false), mHadEnvironmentRole(false)
	{
		std::error_code anError;
		const long long aNonce = std::chrono::steady_clock::now().time_since_epoch().count();
		mRoot = std::filesystem::temp_directory_path(anError) /
			("PvZSmokeManifestTests_" + std::to_string(_getpid()) + "_" + std::to_string(aNonce));
		mRoleRoot = mRoot / "host";
		mAppData = mRoleRoot / "appdata";
		mLogs = mRoleRoot / "logs";
		mConfig = mRoleRoot / "coop.ini";
		mManifest = mRoleRoot / "SmokeManifest.ini";
		mResult = mRoleRoot / "result.json";
		mEscape = mRoot / "escape";
		if (anError || !std::filesystem::create_directories(mAppData, anError) || anError ||
			!std::filesystem::create_directories(mLogs, anError) || anError ||
			!std::filesystem::create_directories(mEscape, anError) || anError ||
			!WriteSmokeTestFile(mConfig, "nickname=SmokeHost\nenable_coop=1\n"))
			return;

		char anOldRole[64] = {};
		DWORD anOldLength = GetEnvironmentVariableA("PVZ_COOP_SMOKE_ROLE", anOldRole, sizeof(anOldRole));
		if (anOldLength >= sizeof(anOldRole))
			return;
		if (anOldLength > 0)
		{
			mHadEnvironmentRole = true;
			mOldEnvironmentRole.assign(anOldRole, anOldLength);
		}
		SetEnvironmentVariableA("PVZ_COOP_SMOKE_ROLE", "host");
		mEnvironmentTouched = true;
		mReady = true;
	}

	~ScopedSmokeManifestTree()
	{
		if (mEnvironmentTouched)
		{
			if (mHadEnvironmentRole)
				SetEnvironmentVariableA("PVZ_COOP_SMOKE_ROLE", mOldEnvironmentRole.c_str());
			else
				SetEnvironmentVariableA("PVZ_COOP_SMOKE_ROLE", nullptr);
		}

		std::error_code anIgnored;
		std::filesystem::remove(mResult, anIgnored);
		std::filesystem::remove(mManifest, anIgnored);
		std::filesystem::remove(mConfig, anIgnored);
		std::filesystem::remove(mAppData, anIgnored);
		std::filesystem::remove(mLogs, anIgnored);
		std::filesystem::remove(mEscape, anIgnored);
		std::filesystem::remove(mRoleRoot, anIgnored);
		std::filesystem::remove(mRoot, anIgnored);
	}

	bool IsReady() const { return mReady; }

	std::string MakeManifest(const std::string& theAppData, const std::string& theResult,
		const std::string& theMode = "adventure", int theLevel = 1, int theSkipIntro = 1) const
	{
		return
			"schema=1\n"
			"role=host\n"
			"endpoint=127.0.0.1\n"
			"port=27713\n"
			"nickname=SmokeHost\n"
			"mode=" + theMode + "\n"
			"level=" + std::to_string(theLevel) + "\n"
			"seed=424242\n"
			"appdata_dir=" + theAppData + "\n"
			"config_path=" + mConfig.string() + "\n"
			"log_dir=" + mLogs.string() + "\n"
			"result_path=" + theResult + "\n"
			"timeout_ms=120000\n"
			"minimum_snapshots=3\n"
			"skip_intro=" + std::to_string(theSkipIntro) + "\n"
			"close_on_result=0\n";
	}

	bool Parse(const std::string& theText, LawnNet::NetSmokeManifest& theManifest,
		std::string& theError) const
	{
		if (!WriteSmokeTestFile(mManifest, theText))
			return false;
		return LawnNet::NetLoadSmokeManifest(mManifest.string().c_str(), theManifest, theError);
	}

	std::filesystem::path mRoot, mRoleRoot, mAppData, mLogs, mConfig, mManifest, mResult, mEscape;

private:
	bool mReady;
	bool mEnvironmentTouched;
	bool mHadEnvironmentRole;
	std::string mOldEnvironmentRole;
};

void TestSmokeManifestAndHashValidation()
{
	static const char aHello[] = "hello";
	CHECK(LawnNet::NetFnv1a32(nullptr, 0) == 2166136261u);
	CHECK(LawnNet::NetFnv1a32(aHello, sizeof(aHello) - 1) == 0x4F9F2CABu);

	ScopedSmokeManifestTree aTree;
	if (!aTree.IsReady())
	{
		Skip("could not construct isolated smoke manifest tree");
		return;
	}

	LawnNet::NetSmokeManifest aManifest;
	std::string anError;
	const std::string aValid = aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string());
	CHECK(aTree.Parse(aValid, aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.IsHost());
	CHECK(aManifest.mLevel == 1);
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_1);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_ADVENTURE);
	CHECK(aManifest.mSeed == 424242u);
	CHECK(aManifest.mMinimumSnapshots == 3u);
	CHECK(aManifest.mSkipIntro);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"adventure", 5, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_ADVENTURE);
	CHECK(!aManifest.mSkipIntro);
	const LawnNet::NetSmokeScenarioDefinition* aHostBowling =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING);
	CHECK(aHostBowling != nullptr);
	CHECK(aHostBowling && !aHostBowling->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"adventure_bowling_client", 5, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_ADVENTURE);
	const LawnNet::NetSmokeScenarioDefinition* aClientBowling =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT);
	CHECK(aClientBowling != nullptr);
	CHECK(aClientBowling && aClientBowling->mActionIsClientOwned);
	CHECK(LawnNet::NetIsBowlingSmokeScenario(aManifest.mScenario));

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"adventure_whack_client", 15, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_ADVENTURE);
	const LawnNet::NetSmokeScenarioDefinition* aClientWhack =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT);
	CHECK(aClientWhack != nullptr);
	CHECK(aClientWhack && aClientWhack->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"slot_machine_client", 1, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE);
	const LawnNet::NetSmokeScenarioDefinition* aClientSlotMachine =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT);
	CHECK(aClientSlotMachine != nullptr);
	CHECK(aClientSlotMachine && aClientSlotMachine->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"tower_defense_client", 1, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_COOP_TOWER_DEFENSE);
	const LawnNet::NetSmokeScenarioDefinition* aClientTowerDefense =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT);
	CHECK(aClientTowerDefense != nullptr);
	CHECK(aClientTowerDefense && aClientTowerDefense->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"portal_combat_client", 1, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT);
	const LawnNet::NetSmokeScenarioDefinition* aClientPortalCombat =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT);
	CHECK(aClientPortalCombat != nullptr);
	CHECK(aClientPortalCombat && aClientPortalCombat->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"pvp_1v1", 1, 1), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_PVP_1V1);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_COOP_PVP);
	const LawnNet::NetSmokeScenarioDefinition* aPvP1v1 =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_PVP_1V1);
	CHECK(aPvP1v1 != nullptr);
	CHECK(aPvP1v1 && aPvP1v1->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"endless_st", 1, 0), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ENDLESS_ST);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_COOP_ENDLESS_ST);
	CHECK(LawnNet::NetIsInteractiveChooserSmokeScenario(aManifest.mScenario));
	const LawnNet::NetSmokeScenarioDefinition* anEndlessST =
		LawnNet::NetGetSmokeScenario(LawnNet::NETSMOKE_SCENARIO_ENDLESS_ST);
	CHECK(anEndlessST != nullptr);
	CHECK(anEndlessST && !anEndlessST->mActionIsClientOwned);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"vasebreaker", 1, 1), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_VASEBREAKER);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_CHALLENGE_VASEBREAKER);

	anError.clear();
	CHECK(aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"beghouled", 1, 1), aManifest, anError));
	CHECK(anError.empty());
	CHECK(aManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_BEGHOULED);
	CHECK(aManifest.mGameMode == (uint8_t)GameMode::GAMEMODE_CHALLENGE_BEGHOULED);

	anError.clear();
	CHECK(!aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"adventure", 2, 1), aManifest, anError));
	CHECK(!anError.empty());

	anError.clear();
	CHECK(!aTree.Parse(aTree.MakeManifest(aTree.mAppData.string(), aTree.mResult.string(),
		"vasebreaker", 1, 2), aManifest, anError));
	CHECK(!anError.empty());

	std::string anUnknown = aValid;
	const size_t aCloseField = anUnknown.find("close_on_result=0");
	CHECK(aCloseField != std::string::npos);
	if (aCloseField != std::string::npos)
		anUnknown.replace(aCloseField, strlen("close_on_result=0"), "unknown_field=0");
	anError.clear();
	CHECK(!aTree.Parse(anUnknown, aManifest, anError));
	CHECK(!anError.empty());

	anError.clear();
	CHECK(!aTree.Parse(aValid + "seed=9\n", aManifest, anError));
	CHECK(!anError.empty());

	anError.clear();
	CHECK(!aTree.Parse(aTree.MakeManifest("relative-appdata", aTree.mResult.string()),
		aManifest, anError));
	CHECK(!anError.empty());

	anError.clear();
	CHECK(!aTree.Parse(aTree.MakeManifest(aTree.mEscape.string(), aTree.mResult.string()),
		aManifest, anError));
	CHECK(!anError.empty());

	CHECK(WriteSmokeTestFile(aTree.mResult, "{}\n"));
	anError.clear();
	CHECK(!aTree.Parse(aValid, aManifest, anError));
	CHECK(!anError.empty());
}

void TestPresentationApproachIsMonotonicAndConvergent()
{
	CHECK(LawnNet::NetApproachPresentationInt(0, 100) == 35);
	CHECK(LawnNet::NetApproachPresentationInt(100, 0) == 65);
	CHECK(LawnNet::NetApproachPresentationInt(9, 10) == 10);
	CHECK(LawnNet::NetApproachPresentationInt(11, 10) == 10);
	CHECK(LawnNet::NetApproachPresentationInt(10, 10) == 10);

	int aForward = -250;
	for (int i = 0; i < 128 && aForward != 400; ++i)
	{
		const int aNext = LawnNet::NetApproachPresentationInt(aForward, 400);
		CHECK(aNext > aForward);
		CHECK(aNext <= 400);
		aForward = aNext;
	}
	CHECK(aForward == 400);

	int aBackward = 400;
	for (int i = 0; i < 128 && aBackward != -250; ++i)
	{
		const int aNext = LawnNet::NetApproachPresentationInt(aBackward, -250);
		CHECK(aNext < aBackward);
		CHECK(aNext >= -250);
		aBackward = aNext;
	}
	CHECK(aBackward == -250);

	// With network targets arriving only every seventh render tick, presentation
	// still advances on the six frames between authoritative samples.
	int aRendered = 0;
	int aChangesBetweenSamples = 0;
	for (int aTick = 0; aTick < 35; ++aTick)
	{
		const int aTarget = (aTick / 7 + 1) * 100;
		const int aNext = LawnNet::NetApproachPresentationInt(aRendered, aTarget);
		if (aTick % 7 != 0 && aNext != aRendered)
			++aChangesBetweenSamples;
		aRendered = aNext;
	}
	CHECK(aChangesBetweenSamples > 0);
}

void TestCoinEventPayloadSizesAreExact()
{
	LawnNet::NetBuffer anAdded;
	for (size_t i = 0; i < LawnNet::NET_SUN_ADDED_PAYLOAD_BYTES - 1U; ++i)
		anAdded.WriteU8(0);
	CHECK(!LawnNet::NetPayloadHasExactBytes(anAdded, LawnNet::NET_SUN_ADDED_PAYLOAD_BYTES));
	anAdded.WriteU8(0);
	CHECK(LawnNet::NetPayloadHasExactBytes(anAdded, LawnNet::NET_SUN_ADDED_PAYLOAD_BYTES));
	anAdded.WriteU8(0);
	CHECK(!LawnNet::NetPayloadHasExactBytes(anAdded, LawnNet::NET_SUN_ADDED_PAYLOAD_BYTES));

	LawnNet::NetBuffer aCollected;
	aCollected.WriteU8(0);
	CHECK(!LawnNet::NetPayloadHasExactBytes(aCollected, LawnNet::NET_SUN_COLLECTED_PAYLOAD_BYTES));
	aCollected.WriteU8(0);
	CHECK(LawnNet::NetPayloadHasExactBytes(aCollected, LawnNet::NET_SUN_COLLECTED_PAYLOAD_BYTES));
	aCollected.WriteU8(0);
	CHECK(!LawnNet::NetPayloadHasExactBytes(aCollected, LawnNet::NET_SUN_COLLECTED_PAYLOAD_BYTES));
}

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
        aSaved.mHeldPlantTintAlpha = 201;
        aSaved.mHotbarTintAlpha = 99;
        aSaved.mHeldPlantOutlineAlpha = 231;
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
        CHECK(aLoaded.mHeldPlantTintAlpha == 201);
        CHECK(aLoaded.mHotbarTintAlpha == 99);
        CHECK(aLoaded.mHeldPlantOutlineAlpha == 231);
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
	RunTest("Smoke.ManifestAndHashValidation", TestSmokeManifestAndHashValidation);
	RunTest("Presentation.Approach", TestPresentationApproachIsMonotonicAndConvergent);
	RunTest("Events.CoinPayloadSizes", TestCoinEventPayloadSizesAreExact);
	const LocalizedTextTestResult aTextResult = RunLocalizedTextTests();
	gChecks += aTextResult.mChecks;
	gFailures += aTextResult.mFailures;
	gSkipped += aTextResult.mSkipped;
	const NetZombieAnimTestResult aZombieAnimResult = RunNetZombieAnimTests();
	gChecks += aZombieAnimResult.mChecks;
	gFailures += aZombieAnimResult.mFailures;
	const NetChallengeRuntimeTestResult aChallengeRuntimeResult =
		RunNetChallengeRuntimeTests();
	gChecks += aChallengeRuntimeResult.mChecks;
	gFailures += aChallengeRuntimeResult.mFailures;
	const NetEndlessWireTestResult anEndlessWireResult =
		RunNetEndlessWireTests();
	gChecks += anEndlessWireResult.mChecks;
	gFailures += anEndlessWireResult.mFailures;
	const NetMowerSnapshotTestResult aMowerSnapshotResult =
		RunNetMowerSnapshotTests();
	gChecks += aMowerSnapshotResult.mChecks;
	gFailures += aMowerSnapshotResult.mFailures;

	std::printf("\nchecks=%d failures=%d skipped=%d\n", gChecks, gFailures, gSkipped);
	return gFailures == 0 ? 0 : 1;
}
