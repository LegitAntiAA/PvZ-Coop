#ifndef __TEXTENCODING_H__
#define __TEXTENCODING_H__

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <windows.h>

namespace Sexy
{
	enum class TextEncoding
	{
		UTF8,
		WINDOWS_1251
	};

	inline bool TryDecodeTextBytes(const std::string& theBytes, TextEncoding theEncoding, std::wstring& theResult)
	{
		theResult.clear();
		if (theBytes.empty())
			return true;
		if (theBytes.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
			return false;

		const UINT aCodePage = theEncoding == TextEncoding::UTF8 ? CP_UTF8 : 1251U;
		const DWORD aFlags = theEncoding == TextEncoding::UTF8 ? MB_ERR_INVALID_CHARS : 0U;
		const int aLength = MultiByteToWideChar(aCodePage, aFlags, theBytes.data(),
			static_cast<int>(theBytes.size()), nullptr, 0);
		if (aLength <= 0)
			return false;

		theResult.resize(static_cast<size_t>(aLength));
		return MultiByteToWideChar(aCodePage, aFlags, theBytes.data(),
			static_cast<int>(theBytes.size()), &theResult[0], aLength) == aLength;
	}

	inline TextEncoding DetectTextEncoding(const std::string& theBytes)
	{
		std::wstring aDiscardedResult;
		return TryDecodeTextBytes(theBytes, TextEncoding::UTF8, aDiscardedResult) ?
			TextEncoding::UTF8 : TextEncoding::WINDOWS_1251;
	}

	inline std::wstring DecodeTextBytes(const std::string& theBytes, TextEncoding theEncoding)
	{
		std::wstring aResult;
		if (TryDecodeTextBytes(theBytes, theEncoding, aResult))
			return aResult;

		// Windows-1251 should decode every byte.  Preserve data rather than
		// returning an empty UI string if a platform conversion unexpectedly fails.
		aResult.reserve(theBytes.size());
		for (unsigned char aByte : theBytes)
			aResult.push_back(static_cast<wchar_t>(aByte));
		return aResult;
	}

	inline std::wstring DecodeUtf8OrWindows1251(const std::string& theBytes)
	{
		return DecodeTextBytes(theBytes, DetectTextEncoding(theBytes));
	}

	inline size_t Utf8CodePointByteLength(const std::string& theText, size_t theOffset)
	{
		if (theOffset >= theText.size())
			return 0U;

		const unsigned char aLead = static_cast<unsigned char>(theText[theOffset]);
		if (aLead < 0x80U)
			return 1U;

		size_t aLength = 0U;
		unsigned char aMinSecond = 0x80U;
		unsigned char aMaxSecond = 0xBFU;
		if (aLead >= 0xC2U && aLead <= 0xDFU)
			aLength = 2U;
		else if (aLead >= 0xE0U && aLead <= 0xEFU)
		{
			aLength = 3U;
			if (aLead == 0xE0U)
				aMinSecond = 0xA0U; // reject overlong encodings
			else if (aLead == 0xEDU)
				aMaxSecond = 0x9FU; // reject UTF-16 surrogates
		}
		else if (aLead >= 0xF0U && aLead <= 0xF4U)
		{
			aLength = 4U;
			if (aLead == 0xF0U)
				aMinSecond = 0x90U; // reject overlong encodings
			else if (aLead == 0xF4U)
				aMaxSecond = 0x8FU; // reject values above U+10FFFF
		}
		else
			return 1U;

		if (theOffset + aLength > theText.size())
			return 1U;
		const unsigned char aSecond = static_cast<unsigned char>(theText[theOffset + 1U]);
		if (aSecond < aMinSecond || aSecond > aMaxSecond)
			return 1U;
		for (size_t i = 2U; i < aLength; ++i)
		{
			const unsigned char aByte = static_cast<unsigned char>(theText[theOffset + i]);
			if (aByte < 0x80U || aByte > 0xBFU)
				return 1U;
		}
		return aLength;
	}

	template <typename GlyphPredicate>
	inline uint32_t ResolveGlyphCodePoint(uint32_t theRequestedCodePoint, GlyphPredicate theHasGlyph)
	{
		if (theHasGlyph(theRequestedCodePoint))
			return theRequestedCodePoint;
		if (theRequestedCodePoint != 0xFFFDU && theHasGlyph(0xFFFDU))
			return 0xFFFDU;
		if (theRequestedCodePoint != static_cast<uint32_t>('?') && theHasGlyph(static_cast<uint32_t>('?')))
			return static_cast<uint32_t>('?');
		return 0U;
	}
}

#endif // __TEXTENCODING_H__
