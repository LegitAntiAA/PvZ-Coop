#include "SDL3Font.h"
#include "../LawnApp.h"
#include "Graphics.h"
#include "TextEncoding.h"

using namespace Sexy;

std::vector<SDL3Font*> SDL3Font::gSDLFonts;

void SDL3Font::RebuildFonts(float scale)
{
	float snappedScale = floor(scale * 4.0f) / 4.0f;

	for (SDL3Font* font : gSDLFonts) {
		if (font->mScale == snappedScale || !font->mIsActive) continue;

		font->mScale = snappedScale;
		font->mScaleOffset = scale - snappedScale;
		font->Rebuild();
		font->mIsActive = true;
	}
}

void SDL3Font::Rebuild()
{
	for (auto& pair : mGlyphCache)
	{
		SDL_DestroyTexture(pair.second);
	}
	mGlyphCache.clear();
	mGlyphWidth.clear();
	mGlyphHeight.clear();
	if (mFont)
	{
		TTF_CloseFont(mFont);
		mFont = nullptr;
	}
	Init(mApp, mPathFile, mPointSize, mBold, mItalic, mUnderline);
}

void SDL3Font::SetActive(bool active)
{
	bool prevActive = mIsActive;
	mIsActive = active;
	if (prevActive != mIsActive && active) {
		float scale = 1.0f;
		int pw = 800;
		int ph = 600;
		if (LawnApp::mSDLWindow)
			SDL_GetWindowSizeInPixels(LawnApp::mSDLWindow, &pw, &ph);
		float gScale = max(static_cast<float>(pw) / 800.0f, static_cast<float>(ph) / 600.0f);
		if (pw >= 800 || ph >= 600) scale = max(floor(gScale * 4.0f) / 4.0f, 1.0f);
		float prevMScale = mScale;
		if (prevMScale != scale)
		{
			mScale = scale;
			mScaleOffset = gScale - scale;
			Rebuild();
			mIsActive = true;
		}
	}
}

void SDL3Font::Init(SexyAppBase* theApp, const std::string& theFace, int thePointSize, bool bold, bool italics, bool underline)
{
	if (!mInitializedScale) {
		mInitializedScale = true;
		float scale = 1.0f;
		int pw = 800;
		int ph = 600;
		if (LawnApp::mSDLWindow)
			SDL_GetWindowSizeInPixels(LawnApp::mSDLWindow, &pw, &ph);
		float gScale = max(static_cast<float>(pw) / 800.0f, static_cast<float>(ph) / 600.0f);
		if (pw >= 800 || ph >= 600) scale = max(floor(gScale * 4.0f) / 4.0f, 1.0f);
		mScale = scale;
		mScaleOffset = gScale - scale;
	}

	mIsActive = false;
	mApp = theApp;
	mPointSize = thePointSize;
	mPathFile = theFace;
	mBold = bold;
	mItalic = italics;
	mUnderline = underline;
	mFont = nullptr;

	if (TTF_WasInit() == 0 && !TTF_Init())
		return;

	mFont = TTF_OpenFont(mPathFile.c_str(), int(floor(thePointSize * (mScale * mDefaultScale))));
	if (!mFont)
		return;

	int style = TTF_STYLE_NORMAL;
	if (bold) style |= TTF_STYLE_BOLD;
	if (italics) style |= TTF_STYLE_ITALIC;
	if (underline) style |= TTF_STYLE_UNDERLINE;
	TTF_SetFontStyle(mFont, style);

	TTF_SetFontHinting(mFont, TTF_HINTING_LIGHT_SUBPIXEL);
	TTF_SetFontKerning(mFont, true);

	Color defaultColor(255, 255, 255, 255);
	for (char c = 32; c < 127; ++c)
		CacheGlyph(c, defaultColor);
}

void SDL3Font::CacheGlyph(Uint32 c, const Color& color)
{
	if (!mFont || !LawnApp::mSDLRenderer || mGlyphCache.find(c) != mGlyphCache.end()) return;

	const Uint32 aRenderCodePoint = ResolveGlyphCodePoint(c,
		[this](Uint32 theCodePoint) { return TTF_FontHasGlyph(mFont, theCodePoint); });
	if (aRenderCodePoint == 0U)
		return;

	SDL_Color sdlColor = { Uint8(color.mRed), Uint8(color.mGreen), Uint8(color.mBlue), Uint8(color.mAlpha) };
	SDL_Surface* surface = TTF_RenderGlyph_Blended(mFont, aRenderCodePoint, sdlColor);
	if (!surface) return;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(LawnApp::mSDLRenderer, surface);
	if (!texture)
	{
		SDL_DestroySurface(surface);
		return;
	}
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	mGlyphCache[c] = texture;
	mGlyphWidth[c] = surface->w;
	mGlyphHeight[c] = surface->h;

	SDL_DestroySurface(surface);
}

SDL3Font::SDL3Font(const std::string& theFace, int thePointSize, bool bold, bool italics, bool underline)
{
	Init(gSexyAppBase, theFace, thePointSize, bold, italics, underline);
	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), this);
	if (it == gSDLFonts.end())
		gSDLFonts.push_back(this);
}

SDL3Font::SDL3Font(SexyAppBase* theApp, const std::string& theFace, int thePointSize, bool bold, bool italics, bool underline)
{
	Init(theApp, theFace, thePointSize, bold, italics, underline);
	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), this);
	if (it == gSDLFonts.end())
		gSDLFonts.push_back(this);
}

SDL3Font::~SDL3Font()
{
	for (auto& pair : mGlyphCache)
		SDL_DestroyTexture(pair.second);
	mGlyphCache.clear();

	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), this);
	if (it != gSDLFonts.end())
		gSDLFonts.erase(it);

	if (mFont)
	{
		TTF_CloseFont(mFont);
		mFont = nullptr;
	}
}

float SDL3Font::GetRelativeScale()
{
	return (mScale - mScaleOffset) * mDefaultScale;
}

int SDL3Font::StringWidth(const SexyString& theString)
{
	if (!mFont) return 0;

	int w = 0;
	Uint32 aPrevChar = 0;
	std::wstring aWideString = StringToWString(theString);
	for (Uint32 c : aWideString)
	{
		if (aPrevChar)
		{
			int aKern = 0;
			TTF_GetGlyphKerning(mFont, aPrevChar, c, &aKern);
			w += aKern;
		}
		if (mGlyphWidth.find(c) == mGlyphWidth.end())
			CacheGlyph(c, Color::White);
		if (mGlyphWidth.find(c) != mGlyphWidth.end())
			w += mGlyphWidth[c];
		aPrevChar = c;
	}
	return w / GetRelativeScale();
}

void SDL3Font::DrawString(Graphics* g, int theX, int theY, const SexyString& theString, const Color& theColor, const Rect& theClipRect)
{
	if (!mFont || !g || !LawnApp::mSDLRenderer) return;

	SDL_Renderer* aRenderer = LawnApp::mSDLRenderer;
	SDL_Rect aPreviousClip = {};
	bool aPreviousClipEnabled = SDL_RenderClipEnabled(aRenderer);
	SDL_GetRenderClipRect(aRenderer, &aPreviousClip);

	float x = theX + g->mTransX;
	float aBaselineY = theY + g->mTransY;

	if (theClipRect.mWidth > 0 && theClipRect.mHeight > 0)
	{
		SDL_Rect clip = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
		SDL_SetRenderClipRect(aRenderer, &clip);
	}

	Uint32 prevChar = 0;
	std::wstring aWideString = StringToWString(theString);
	for (Uint32 c : aWideString)
	{
		if (prevChar)
		{
			int kern = 0;
			TTF_GetGlyphKerning(mFont, prevChar, c, &kern);
			x += kern / GetRelativeScale();
		}

		if (mGlyphCache.find(c) == mGlyphCache.end())
			CacheGlyph(c, Color::White);

		SDL_Texture* tex = mGlyphCache.count(c) ? mGlyphCache[c] : nullptr;
		if (tex)
		{
			SDL_FRect dst = { x, aBaselineY - GetAscent(), float(mGlyphWidth[c]) / GetRelativeScale(), float(mGlyphHeight[c]) / GetRelativeScale()};
			SDL_SetTextureColorMod(tex, theColor.mRed, theColor.mGreen, theColor.mBlue);
			SDL_SetTextureAlphaMod(tex, theColor.mAlpha);
			SDL_SetTextureScaleMode(tex, g->GetLinearBlend() ? SDL_ScaleMode::SDL_SCALEMODE_LINEAR : g->mIsPixelArt ? SDL_ScaleMode::SDL_SCALEMODE_PIXELART : SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
			SDL_RenderTexture(aRenderer, tex, nullptr, &dst);
			x += mGlyphWidth[c] / GetRelativeScale();
		}

		prevChar = c;
	}

	SDL_SetRenderClipRect(aRenderer, aPreviousClipEnabled ? &aPreviousClip : nullptr);
}

SDL3Font* SDL3Font::Duplicate()
{
	SDL3Font* newFont = new SDL3Font(mApp, mPathFile, mPointSize, mBold, mItalic, mUnderline);
	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), newFont);
	if (it == gSDLFonts.end())
		gSDLFonts.push_back(newFont);
	return newFont;
}

int	SDL3Font::GetAscent() { return mFont ? TTF_GetFontAscent(mFont) / GetRelativeScale() : 0; }
int	SDL3Font::GetAscentPadding() { return 0; }
int	SDL3Font::GetDescent() { return mFont ? TTF_GetFontDescent(mFont) / GetRelativeScale() : 0; }
int	SDL3Font::GetHeight() { return mFont ? TTF_GetFontHeight(mFont) / GetRelativeScale() : 0; }
int SDL3Font::GetLineSpacingOffset() { return GetHeight() + GetLineSpacing(); }
int SDL3Font::GetLineSpacing() { return mFont ? TTF_GetFontLineSkip(mFont) / GetRelativeScale() : 0; }
int SDL3Font::CharWidth(SexyChar theChar)
{
	if (!mFont) return 0;

	Uint32 aChar = (unsigned char)theChar;
	if (aChar >= 0x80)
		return max(1, mPointSize / 2); // one half of a typical two-byte Cyrillic UTF-8 sequence
	if (mGlyphWidth.find(aChar) == mGlyphWidth.end())
		CacheGlyph(aChar, Color::White);
	return mGlyphWidth[aChar] / GetRelativeScale();
}

int SDL3Font::CharWidthKern(SexyChar theChar, SexyChar thePrevChar)
{
	if (!mFont) return 0;

	Uint32 aChar = (unsigned char)theChar;
	Uint32 aPrevChar = (unsigned char)thePrevChar;
	if (aChar >= 0x80 || aPrevChar >= 0x80)
		return CharWidth(theChar);
	int aKern = 0;
	TTF_GetGlyphKerning(mFont, aPrevChar, aChar, &aKern);
	return CharWidth(theChar) + aKern / GetRelativeScale();
}
