#ifndef __NETUI_H__
#define __NETUI_H__

#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/Image.h"
#include "../../SexyAppFramework/Font.h"
#include "../../Resources.h"

namespace NetUi
{
inline Sexy::Color Ink() { return Sexy::Color(76, 47, 24); }
inline Sexy::Color MutedInk() { return Sexy::Color(119, 82, 45); }
inline Sexy::Color Cream() { return Sexy::Color(245, 230, 186); }

inline void Literal(Sexy::Graphics* g, Sexy::Font* theFont, const SexyString& theText,
    int theX, int theY, const Sexy::Color& theColor, int theAlign = -1)
{
    if (theAlign == 0) theX -= theFont->StringWidth(theText) / 2;
    else if (theAlign == 1) theX -= theFont->StringWidth(theText);
    theFont->DrawString(g, theX, theY, theText, theColor, g->mClipRect);
}

inline SexyString Fit(Sexy::Font* theFont, const SexyString& theText, int theWidth)
{
    if (theFont->StringWidth(theText) <= theWidth) return theText;
    SexyString aText = theText;
    while (!aText.empty() && theFont->StringWidth(aText + _S("...")) > theWidth)
    {
        size_t aLast = aText.size() - 1;
#ifndef _USE_WIDE_STRING
        while (aLast > 0 && (static_cast<unsigned char>(aText[aLast]) & 0xC0U) == 0x80U) --aLast;
#endif
        aText.resize(aLast);
    }
    return aText + _S("...");
}

inline void Paper(Sexy::Graphics* g, Sexy::Image* theImage, const Sexy::Rect& theRect)
{
    g->SetColor(Sexy::Color(30, 20, 10, 70));
    g->FillRect(theRect.mX + 3, theRect.mY + 3, theRect.mWidth, theRect.mHeight);
    if (theImage)
        g->DrawImage(theImage, theRect, Sexy::Rect(18, 220, 278, 238));
    else
    {
        g->SetColor(Sexy::Color(248, 209, 153));
        g->FillRect(theRect);
    }
    g->SetColor(Sexy::Color(112, 73, 34));
    g->DrawRect(theRect.mX + 1, theRect.mY + 1, theRect.mWidth - 3, theRect.mHeight - 3);
}

inline void StoneButton(Sexy::Graphics* g, const Sexy::Rect& r, const SexyString& theLabel,
    bool thePressed, bool theHover, bool theDisabled)
{
    Sexy::Image* aLeft = thePressed ? Sexy::IMAGE_BUTTON_DOWN_LEFT : Sexy::IMAGE_BUTTON_LEFT;
    Sexy::Image* aMid = thePressed ? Sexy::IMAGE_BUTTON_DOWN_MIDDLE : Sexy::IMAGE_BUTTON_MIDDLE;
    Sexy::Image* aRight = thePressed ? Sexy::IMAGE_BUTTON_DOWN_RIGHT : Sexy::IMAGE_BUTTON_RIGHT;
    const int aLeftWidth = min(aLeft->mWidth, r.mWidth / 2);
    const int aRightWidth = min(aRight->mWidth, r.mWidth - aLeftWidth);
    g->PushState();
    g->SetColorizeImages(theDisabled);
    g->SetColor(Sexy::Color(150, 150, 150));
    g->DrawImage(aLeft, Sexy::Rect(r.mX, r.mY, aLeftWidth, r.mHeight), Sexy::Rect(0, 0, aLeft->mWidth, aLeft->mHeight));
    if (r.mWidth > aLeftWidth + aRightWidth)
        g->DrawImage(aMid, Sexy::Rect(r.mX + aLeftWidth, r.mY, r.mWidth - aLeftWidth - aRightWidth, r.mHeight),
            Sexy::Rect(0, 0, aMid->mWidth, aMid->mHeight));
    g->DrawImage(aRight, Sexy::Rect(r.mX + r.mWidth - aRightWidth, r.mY, aRightWidth, r.mHeight),
        Sexy::Rect(0, 0, aRight->mWidth, aRight->mHeight));
    g->SetColorizeImages(false);
    Sexy::Font* aFont = Sexy::FONT_BRIANNETOD12;
    Literal(g, aFont, theLabel, r.mX + r.mWidth / 2 + (thePressed ? 1 : 0),
        r.mY + (r.mHeight + aFont->GetAscent()) / 2 - 2 + (thePressed ? 1 : 0),
        theDisabled ? Sexy::Color(180, 177, 168) : theHover ? Sexy::Color(190, 245, 105) : Cream(), 0);
    g->PopState();
}
}

#endif
