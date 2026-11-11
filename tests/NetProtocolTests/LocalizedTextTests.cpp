#include "LocalizedTextTests.h"

#include "../../PakLib/PakInterface.h"
#include "../../SexyAppFramework/TextEncoding.h"
#include "../../Lawn/Net/NetCommon.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

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

#define LOCAL_CHECK(theExpression) Check(!!(theExpression), #theExpression, __FILE__, __LINE__)

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

void TestLocalizedTextDecoding()
{
	const std::wstring anExpectedMusic = L"\u041C\u0443\u0437\u044B\u043A\u0430";
	const std::string anAscii = "OPTIONS_MUSIC_LABEL";
	LOCAL_CHECK(Sexy::DetectTextEncoding(anAscii) == Sexy::TextEncoding::UTF8);
	LOCAL_CHECK(Sexy::DecodeUtf8OrWindows1251(anAscii) == L"OPTIONS_MUSIC_LABEL");

	const char aUtf8Bytes[] = {
		static_cast<char>(0xD0), static_cast<char>(0x9C),
		static_cast<char>(0xD1), static_cast<char>(0x83),
		static_cast<char>(0xD0), static_cast<char>(0xB7),
		static_cast<char>(0xD1), static_cast<char>(0x8B),
		static_cast<char>(0xD0), static_cast<char>(0xBA),
		static_cast<char>(0xD0), static_cast<char>(0xB0)
	};
	const std::string aUtf8(aUtf8Bytes, sizeof(aUtf8Bytes));
	LOCAL_CHECK(Sexy::DetectTextEncoding(aUtf8) == Sexy::TextEncoding::UTF8);
	LOCAL_CHECK(Sexy::DecodeUtf8OrWindows1251(aUtf8) == anExpectedMusic);

	const char aCp1251Bytes[] = {
		static_cast<char>(0xCC), static_cast<char>(0xF3),
		static_cast<char>(0xE7), static_cast<char>(0xFB),
		static_cast<char>(0xEA), static_cast<char>(0xE0)
	};
	const std::string aCp1251(aCp1251Bytes, sizeof(aCp1251Bytes));
	LOCAL_CHECK(Sexy::DetectTextEncoding(aCp1251) == Sexy::TextEncoding::WINDOWS_1251);
	LOCAL_CHECK(Sexy::DecodeTextBytes(aCp1251, Sexy::TextEncoding::WINDOWS_1251) == anExpectedMusic);

	// D0 B0 is both valid UTF-8 for U+0430 and valid CP1251 for U+0420 U+00B0.
	// It proves why a legacy string-list must retain the encoding detected from
	// the complete file instead of re-detecting each short value independently.
	const char anAmbiguousCp1251Bytes[] = { static_cast<char>(0xD0), static_cast<char>(0xB0) };
	const std::string anAmbiguousCp1251Value(anAmbiguousCp1251Bytes, sizeof(anAmbiguousCp1251Bytes));
	LOCAL_CHECK(Sexy::DecodeUtf8OrWindows1251(anAmbiguousCp1251Value) == L"\u0430");
	const std::string aCp1251File = aCp1251 + anAmbiguousCp1251Value;
	const Sexy::TextEncoding aFileEncoding = Sexy::DetectTextEncoding(aCp1251File);
	LOCAL_CHECK(aFileEncoding == Sexy::TextEncoding::WINDOWS_1251);
	LOCAL_CHECK(Sexy::DecodeTextBytes(anAmbiguousCp1251Value, aFileEncoding) == L"\u0420\u00B0");

	const char anOverlongUtf8Bytes[] = { static_cast<char>(0xC0), static_cast<char>(0xAF) };
	const std::string anOverlongUtf8(anOverlongUtf8Bytes, sizeof(anOverlongUtf8Bytes));
	LOCAL_CHECK(Sexy::DetectTextEncoding(anOverlongUtf8) == Sexy::TextEncoding::WINDOWS_1251);
}

void TestUtf8CodePointBoundaries()
{
	const char aBytes[] = {
		'A',
		static_cast<char>(0xD0), static_cast<char>(0x9C),
		static_cast<char>(0xF0), static_cast<char>(0x9F),
		static_cast<char>(0x8C), static_cast<char>(0xBB)
	};
	const std::string aText(aBytes, sizeof(aBytes));
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aText, 0U) == 1U);
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aText, 1U) == 2U);
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aText, 3U) == 4U);
	const std::string aWrappedText = aText + "\n" + aText;
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aWrappedText, aText.size()) == 1U);
	LOCAL_CHECK(aWrappedText[aText.size()] == '\n');

	size_t anOffset = 0U;
	while (anOffset < aText.size())
	{
		const size_t aLength = Sexy::Utf8CodePointByteLength(aText, anOffset);
		LOCAL_CHECK(aLength >= 1U && anOffset + aLength <= aText.size());
		std::wstring aDecodedCodePoint;
		LOCAL_CHECK(Sexy::TryDecodeTextBytes(aText.substr(anOffset, aLength), Sexy::TextEncoding::UTF8, aDecodedCodePoint));
		anOffset += aLength;
	}
	LOCAL_CHECK(anOffset == aText.size());

	const char aMalformedBytes[] = { static_cast<char>(0xE2), '(', static_cast<char>(0xA1) };
	const std::string aMalformed(aMalformedBytes, sizeof(aMalformedBytes));
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aMalformed, 0U) == 1U);
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aMalformed, 1U) == 1U);
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aMalformed, 2U) == 1U);
	LOCAL_CHECK(Sexy::Utf8CodePointByteLength(aMalformed, aMalformed.size()) == 0U);
}

void TestMissingGlyphResolution()
{
	LOCAL_CHECK(Sexy::ResolveGlyphCodePoint(0x041CU,
		[](uint32_t theCodePoint) { return theCodePoint == 0x041CU; }) == 0x041CU);
	LOCAL_CHECK(Sexy::ResolveGlyphCodePoint(0x041CU,
		[](uint32_t theCodePoint) { return theCodePoint == 0xFFFDU; }) == 0xFFFDU);
	LOCAL_CHECK(Sexy::ResolveGlyphCodePoint(0x041CU,
		[](uint32_t theCodePoint) { return theCodePoint == static_cast<uint32_t>('?'); }) == static_cast<uint32_t>('?'));
	LOCAL_CHECK(Sexy::ResolveGlyphCodePoint(0x041CU,
		[](uint32_t) { return false; }) == 0U);
}

void TestNetworkUtf8Normalization()
{
	const std::string aRussianNick = "\xD0\x91\xD1\x80\xD0\xB0\xD1\x82";
	std::string anOutput;
	LOCAL_CHECK(LawnNet::NetNormalizeUtf8(aRussianNick, LawnNet::NET_MAX_NICK_BYTES,
		LawnNet::NET_MAX_NICK_CHARS, false, anOutput));
	LOCAL_CHECK(anOutput == aRussianNick);
	LOCAL_CHECK(LawnNet::NetNormalizeUtf8(aRussianNick, 5U,
		LawnNet::NET_MAX_NICK_CHARS, false, anOutput));
	LOCAL_CHECK(anOutput == "\xD0\x91\xD1\x80");
	const std::string aMalformed("\xD0\x20", 2U);
	LOCAL_CHECK(!LawnNet::NetNormalizeUtf8(aMalformed, 64U, 16U, false, anOutput));
	LOCAL_CHECK(!LawnNet::NetNormalizeUtf8("Bad\nNick", 64U, 16U, false, anOutput));
	const std::string aControl("Bad\x01Nick", 8U);
	LOCAL_CHECK(!LawnNet::NetNormalizeUtf8(aControl, 64U, 16U, false, anOutput));
	LOCAL_CHECK(LawnNet::NetNormalizeUtf8("  Sun Flower  ", 64U, 16U, true, anOutput));
	LOCAL_CHECK(anOutput == "Sun Flower");
	LOCAL_CHECK(!LawnNet::NetNormalizeUtf8("Sun Flower", 64U, 16U, false, anOutput));
	LOCAL_CHECK(LawnNet::NetNormalizeUtf8("Sun ", 64U, 16U, true, anOutput, false));
	LOCAL_CHECK(anOutput == "Sun ");
	LOCAL_CHECK(LawnNet::NetNormalizeUtf8(" ", 64U, 16U, true, anOutput, false));
	LOCAL_CHECK(anOutput == " ");
}

std::filesystem::path FindMainPak()
{
	std::error_code anError;
	std::filesystem::path aDirectory = std::filesystem::current_path(anError);
	if (anError)
		return {};

	for (int aDepth = 0; aDepth < 8; ++aDepth)
	{
		const std::filesystem::path aCandidates[] = {
			aDirectory / "main.pak",
			aDirectory / "bin" / "main.pak",
			aDirectory / "build" / "ReleaseGOTY_Win32" / "bin" / "main.pak"
		};
		for (const std::filesystem::path& aCandidate : aCandidates)
		{
			if (std::filesystem::is_regular_file(aCandidate, anError) && !anError)
				return aCandidate;
			anError.clear();
		}

		const std::filesystem::path aParent = aDirectory.parent_path();
		if (aParent == aDirectory || aParent.empty())
			break;
		aDirectory = aParent;
	}
	return {};
}

bool IsStringListWhitespace(char theChar)
{
	return theChar == ' ' || theChar == '\t' || theChar == '\r' || theChar == '\n';
}

bool ExtractRawStringListValue(const std::string& theFileText, const char* theKey, std::string& theValue)
{
	const std::string aMarker = std::string("[") + theKey + "]";
	size_t aStart = theFileText.find(aMarker);
	if (aStart == std::string::npos)
		return false;
	aStart += aMarker.size();
	size_t anEnd = theFileText.find('[', aStart);
	if (anEnd == std::string::npos)
		anEnd = theFileText.size();
	while (aStart < anEnd && IsStringListWhitespace(theFileText[aStart]))
		++aStart;
	while (anEnd > aStart && IsStringListWhitespace(theFileText[anEnd - 1U]))
		--anEnd;
	theValue.assign(theFileText, aStart, anEnd - aStart);
	theValue.erase(std::remove(theValue.begin(), theValue.end(), '\r'), theValue.end());
	return true;
}

void TestRussianGotyLawnStringsFromPak()
{
	const std::filesystem::path aPakPath = FindMainPak();
	if (aPakPath.empty())
	{
		Skip("main.pak not found; set the test working directory inside the PvZ tree");
		return;
	}

	LOCAL_CHECK(gPakInterface != nullptr);
	std::error_code aRelativeError;
	const std::filesystem::path aCurrentDirectory = std::filesystem::current_path(aRelativeError);
	std::filesystem::path aPakArgument;
	if (!aRelativeError)
		aPakArgument = std::filesystem::relative(aPakPath, aCurrentDirectory, aRelativeError);
	if (aRelativeError || aPakArgument.empty())
		aPakArgument = aPakPath;
	if (gPakInterface == nullptr || !gPakInterface->AddPakFile(aPakArgument.string()))
	{
		LOCAL_CHECK(false);
		return;
	}

	PFILE* aFile = gPakInterface->FOpen("properties\\LawnStrings.txt", "rb");
	LOCAL_CHECK(aFile != nullptr);
	if (aFile == nullptr)
		return;
	gPakInterface->FSeek(aFile, 0, SEEK_END);
	const int aFileSize = gPakInterface->FTell(aFile);
	gPakInterface->FSeek(aFile, 0, SEEK_SET);
	LOCAL_CHECK(aFileSize > 0);
	if (aFileSize <= 0)
	{
		gPakInterface->FClose(aFile);
		return;
	}

	std::string aFileText(static_cast<size_t>(aFileSize), '\0');
	const size_t aReadCount = gPakInterface->FRead(&aFileText[0], 1, aFileSize, aFile);
	gPakInterface->FClose(aFile);
	LOCAL_CHECK(aReadCount == static_cast<size_t>(aFileSize));

	const Sexy::TextEncoding anEncoding = Sexy::DetectTextEncoding(aFileText);
	LOCAL_CHECK(anEncoding == Sexy::TextEncoding::WINDOWS_1251);
	const std::wstring aDecodedFile = Sexy::DecodeTextBytes(aFileText, anEncoding);
	const size_t aCyrillicCount = static_cast<size_t>(std::count_if(aDecodedFile.begin(), aDecodedFile.end(),
		[](wchar_t theChar) { return theChar >= 0x0400 && theChar <= 0x04FF; }));
	LOCAL_CHECK(aCyrillicCount > 40000U);
	std::printf("    LawnStrings.txt via PakInterface: %d bytes, CP1251, %zu Cyrillic code points\n",
		aFileSize, aCyrillicCount);

	struct ExpectedValue
	{
		const char* mKey;
		const wchar_t* mValue;
	};
	const ExpectedValue anExpectedValues[] = {
		{ "OPTIONS_MUSIC_LABEL", L"\u041C\u0443\u0437\u044B\u043A\u0430" },
		{ "OPTIONS_SOUNDFX", L"\u0417\u0432\u0443\u043A\u0438" },
		{ "OPTIONS_FULL_SCREEN", L"\u041F\u043E\u043B\u043D\u044B\u0439 \u044D\u043A\u0440\u0430\u043D" },
		{ "SUBURBAN_ALMANAC_INDEX", L"\u0410\u043B\u044C\u043C\u0430\u043D\u0430\u0445 \u0421\u0430\u0434\u043E\u0432\u043E\u0434\u0430 - \u041A\u0430\u0442\u0430\u043B\u043E\u0433" }
	};
	for (const ExpectedValue& anExpected : anExpectedValues)
	{
		std::string aRawValue;
		LOCAL_CHECK(ExtractRawStringListValue(aFileText, anExpected.mKey, aRawValue));
		LOCAL_CHECK(Sexy::DecodeTextBytes(aRawValue, anEncoding) == anExpected.mValue);
	}
}

} // namespace

LocalizedTextTestResult RunLocalizedTextTests()
{
	gChecks = 0;
	gFailures = 0;
	gSkipped = 0;
	RunTest("Text.LocalizedDecoding", TestLocalizedTextDecoding);
	RunTest("Text.Utf8CodePointBoundaries", TestUtf8CodePointBoundaries);
	RunTest("Text.MissingGlyphResolution", TestMissingGlyphResolution);
	RunTest("Text.NetworkUtf8Normalization", TestNetworkUtf8Normalization);
	RunTest("Text.RussianGotyLawnStringsFromPak", TestRussianGotyLawnStringsFromPak);
	return { gChecks, gFailures, gSkipped };
}
