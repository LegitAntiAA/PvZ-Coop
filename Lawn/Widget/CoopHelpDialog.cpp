#include "CoopHelpDialog.h"
#include "LawnDialog.h"
#include "GameSelector.h"
#include "NewOptionsDialog.h"
#include "GameButton.h"
#include "NetOnlineDialog.h"
#include "NetUi.h"
#include "../../LawnApp.h"
#include "../Net/NetManager.h"
#include "../Net/NetLobbyPanel.h"
#include "../SeedPacket.h"
#include "../Zombie.h"
#include "../../SexyAppFramework/DDImage.h"
#include "../../SexyAppFramework/Font.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../SexyAppFramework/SDL3Font.h"
#include "../../SexyAppFramework/SoundManager.h"
#include "../../SexyAppFramework/SoundInstance.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include <sstream>
#include <cmath>

using namespace Sexy;
using namespace LawnNet;

namespace
{
std::vector<SexyString> WrapSpeech(const SexyString& text, Font* font, int width)
{
    // Measure complete UTF-8 strings: per-byte glyph widths lose Cyrillic text.
    std::basic_istringstream<SexyChar> words(text);
    std::vector<SexyString> lines;
    SexyString word, line;
    while (words >> word)
    {
        const SexyString candidate = line.empty() ? word : line + _S(" ") + word;
        if (!line.empty() && font->StringWidth(candidate) > width)
        { lines.push_back(line); line = word; }
        else line = candidate;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

int SpeechLineHeight(Font* font)
{ return max(font->GetLineSpacing(), font->GetHeight() + 2); }

std::vector<size_t> CodepointEnds(const SexyString& text)
{
    std::vector<size_t> ends;
    for (size_t i = 0; i < text.size();)
    {
        ++i;
        if (sizeof(SexyChar) == 1)
            while (i < text.size() && ((unsigned char)text[i] & 0xc0) == 0x80) ++i;
        else if (text[i - 1] >= 0xd800 && text[i - 1] <= 0xdbff &&
            i < text.size() && text[i] >= 0xdc00 && text[i] <= 0xdfff) ++i;
        ends.push_back(i);
    }
    return ends;
}

bool InsideTriangle(double x, double y, const Point (&points)[3])
{
    bool positive = false, negative = false;
    for (int i = 0; i < 3; ++i)
    {
        const Point& a = points[i]; const Point& b = points[(i + 1) % 3];
        const double cross = (x - a.mX) * (b.mY - a.mY) - (y - a.mY) * (b.mX - a.mX);
        positive |= cross > 0; negative |= cross < 0;
    }
    return !positive || !negative;
}

double TriangleDistance(double x, double y, const Point (&points)[3])
{
    double distanceSquared = 1e20;
    for (int i = 0; i < 3; ++i)
    {
        const Point& a = points[i]; const Point& b = points[(i + 1) % 3];
        const double dx = b.mX - a.mX, dy = b.mY - a.mY;
        const double along = max(0.0, min(1.0, ((x - a.mX) * dx + (y - a.mY) * dy) / (dx * dx + dy * dy)));
        const double ex = x - a.mX - along * dx, ey = y - a.mY - along * dy;
        distanceSquared = min(distanceSquared, ex * ex + ey * ey);
    }
    return std::sqrt(distanceSquared) * (InsideTriangle(x, y, points) ? -1.0 : 1.0);
}

void SpeechTail(const Rect& bubble, const Rect& zombie, Point (&points)[3])
{
    const Point head(zombie.mX + 42, zombie.mY + 35);
    const int cx = bubble.mX + bubble.mWidth / 2, cy = bubble.mY + bubble.mHeight / 2;
    const double direction = std::atan2((head.mY - cy) * 2.0 / bubble.mHeight, (head.mX - cx) * 2.0 / bubble.mWidth);
    points[0] = Point(cx + (int)(std::cos(direction - 0.11) * bubble.mWidth / 2), cy + (int)(std::sin(direction - 0.11) * bubble.mHeight / 2));
    points[1] = Point(cx + (int)(std::cos(direction + 0.11) * bubble.mWidth / 2), cy + (int)(std::sin(direction + 0.11) * bubble.mHeight / 2));
    points[2] = head;
}

const char* const kTitles[] = {
    "\xd0\x98\xd0\x93\xd0\xa0\xd0\x90\xd0\x95\xd0\x9c\x20\xd0\x92\xd0\x9c\xd0\x95\xd0\xa1\xd0\xa2\xd0\x95", "\xd0\x94\xd0\xa0\xd0\xa3\xd0\x97\xd0\xac\xd0\xaf\x20\xd0\x92\x20\xd0\x9b\xd0\x9e\xd0\x9a\xd0\x90\xd0\x9b\xd0\xac\xd0\x9d\xd0\x9e\xd0\x99\x20\xd0\xa1\xd0\x95\xd0\xa2\xd0\x98", "\xd0\x9a\xd0\x9e\xd0\x9c\xd0\x9d\xd0\x90\xd0\xa2\xd0\xab\x20\xd0\x92\x20\xd0\x98\xd0\x9d\xd0\xa2\xd0\x95\xd0\xa0\xd0\x9d\xd0\x95\xd0\xa2\xd0\x95", "\xd0\x9d\xd0\x90\xd0\xa1\xd0\xa2\xd0\xa0\xd0\x9e\xd0\x99\xd0\x9a\xd0\x98",
    "\xd0\x9e\xd0\x91\xd0\xa9\xd0\x90\xd0\x99\xd0\xa2\xd0\x95\xd0\xa1\xd0\xac\x20\xd0\x92\x20\xd0\xa7\xd0\x90\xd0\xa2\xd0\x95", "\xd0\x9f\xd0\x9e\xd0\x9a\xd0\x90\xd0\x96\xd0\x98\x20\xd0\xa1\xd0\x92\xd0\x9e\xd0\x95\xd0\x99\x20\xd0\x9a\xd0\x9e\xd0\x9c\xd0\x90\xd0\x9d\xd0\x94\xd0\x95", "\xd0\x9c\xd0\x9e\xd0\x96\xd0\x9d\xd0\x9e\x20\xd0\xa1\xd0\x90\xd0\x96\xd0\x90\xd0\xa2\xd0\xac\x21"
};
const char* const kSpeech[] = {
    "\xd0\x9c\xd0\xbe\xd0\xb7\xd0\xb3\xd0\xb8\x2d\xd0\xb8\x2e\x2e\x2e\x20\xd0\xb8\x20\xd0\xb4\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f\x21\x20\xd0\x9a\xd0\xbd\xd0\xbe\xd0\xbf\xd0\xba\xd0\xb0\x20\x50\x4c\x41\x59\x20\x54\x4f\x47\x45\x54\x48\x45\x52\x20\xd1\x80\xd0\xb0\xd0\xb7\xd0\xb2\xd0\xbe\xd1\x80\xd0\xb0\xd1\x87\xd0\xb8\xd0\xb2\xd0\xb0\xd0\xb5\xd1\x82\x20\xd0\xbf\xd0\xb0\xd0\xbd\xd0\xb5\xd0\xbb\xd1\x8c\x20\xd0\xba\xd0\xbe\xd0\xbe\xd0\xbf\xd0\xb5\xd1\x80\xd0\xb0\xd1\x82\xd0\xb8\xd0\xb2\xd0\xb0\x2e\x20\xd0\x97\xd0\xb4\xd0\xb5\xd1\x81\xd1\x8c\x20\xd0\xbd\xd0\xb0\xd1\x85\xd0\xbe\xd0\xb4\xd1\x8f\xd1\x82\xd1\x81\xd1\x8f\x20\xd0\xb2\xd0\xb0\xd1\x88\x20\xd0\xbd\xd0\xb8\xd0\xba\x20\xd0\xb8\x20\xd1\x81\xd0\xbf\xd0\xbe\xd1\x81\xd0\xbe\xd0\xb1\xd1\x8b\x20\xd0\xbf\xd0\xbe\xd0\xb4\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87\xd0\xb5\xd0\xbd\xd0\xb8\xd1\x8f\x2e",
    "\xd0\x9e\xd1\x82\xd0\xba\xd1\x80\xd0\xbe\xd0\xb9\xd1\x82\xd0\xb5\x20\x4c\x4f\x43\x41\x4c\x20\x4e\x45\x54\x57\x4f\x52\x4b\x2e\x20\xd0\xa5\xd0\xbe\xd0\xb7\xd1\x8f\xd0\xb8\xd0\xbd\x20\xd0\xbd\xd0\xb0\xd0\xb6\xd0\xb8\xd0\xbc\xd0\xb0\xd0\xb5\xd1\x82\x20\x48\x4f\x53\x54\x20\x4c\x41\x4e\x2e\x20\xd0\x94\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f\x20\xd0\xb2\xd0\xb2\xd0\xbe\xd0\xb4\xd1\x8f\xd1\x82\x20\xd0\xb5\xd0\xb3\xd0\xbe\x20\x49\x50\x20\xd0\xb8\x20\xd1\x82\xd0\xbe\xd1\x82\x20\xd0\xb6\xd0\xb5\x20\xd0\xbf\xd0\xbe\xd1\x80\xd1\x82\x2c\x20\xd0\xb7\xd0\xb0\xd1\x82\xd0\xb5\xd0\xbc\x20\x4a\x4f\x49\x4e\x20\x4c\x41\x4e\x2e\x20\xd0\x9c\xd0\xbe\xd0\xb6\xd0\xbd\xd0\xbe\x20\xd0\xb8\xd0\xb3\xd1\x80\xd0\xb0\xd1\x82\xd1\x8c\x20\xd0\xb8\x20\xd1\x87\xd0\xb5\xd1\x80\xd0\xb5\xd0\xb7\x20\x56\x50\x4e\x2e",
    "\x4f\x4e\x4c\x49\x4e\x45\x20\x50\x4c\x41\x59\x20\xd0\xbe\xd1\x82\xd0\xba\xd1\x80\xd1\x8b\xd0\xb2\xd0\xb0\xd0\xb5\xd1\x82\x20\xd1\x8d\xd1\x82\xd0\xbe\xd1\x82\x20\xd0\xb1\xd1\x80\xd0\xb0\xd1\x83\xd0\xb7\xd0\xb5\xd1\x80\x2e\x20\x52\x45\x46\x52\x45\x53\x48\x20\xd0\xbe\xd0\xb1\xd0\xbd\xd0\xbe\xd0\xb2\xd0\xbb\xd1\x8f\xd0\xb5\xd1\x82\x20\xd0\xba\xd0\xbe\xd0\xbc\xd0\xbd\xd0\xb0\xd1\x82\xd1\x8b\x2c\x20\x4a\x4f\x49\x4e\x20\x52\x4f\x4f\x4d\x20\xd0\xbf\xd0\xbe\xd0\xb4\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87\xd0\xb0\xd0\xb5\xd1\x82\x20\xd0\xba\x20\xd0\xb2\xd1\x8b\xd0\xb1\xd1\x80\xd0\xb0\xd0\xbd\xd0\xbd\xd0\xbe\xd0\xb9\x2e\x20\xd0\x98\xd0\xbb\xd0\xb8\x20\xd1\x81\xd0\xbe\xd0\xb7\xd0\xb4\xd0\xb0\xd0\xb9\xd1\x82\xd0\xb5\x20\xd1\x81\xd0\xb2\xd0\xbe\xd1\x8e\x20\xd1\x87\xd0\xb5\xd1\x80\xd0\xb5\xd0\xb7\x20\x43\x52\x45\x41\x54\x45\x2e\x20\xd0\x95\xd1\x81\xd0\xbb\xd0\xb8\x20\xd0\xbd\xd1\x83\xd0\xb6\xd0\xb5\xd0\xbd\x20\xd0\xbf\xd0\xb0\xd1\x80\xd0\xbe\xd0\xbb\xd1\x8c\x2c\x20\xd0\xb2\xd0\xb2\xd0\xb5\xd0\xb4\xd0\xb8\xd1\x82\xd0\xb5\x20\xd0\xb5\xd0\xb3\xd0\xbe\x20\xd0\xbf\xd0\xb5\xd1\x80\xd0\xb5\xd0\xb4\x20\xd0\xb2\xd1\x85\xd0\xbe\xd0\xb4\xd0\xbe\xd0\xbc\x2e",
    "\xd0\x92\x20\xd0\xbc\xd0\xb5\xd0\xbd\xd1\x8e\x20\x4f\x50\x54\x49\x4f\x4e\x53\x20\xd0\xbd\xd0\xb0\xd0\xb6\xd0\xbc\xd0\xb8\xd1\x82\xd0\xb5\x20\x43\x4f\x2d\x4f\x50\x2e\x20\xd0\x97\xd0\xb4\xd0\xb5\xd1\x81\xd1\x8c\x20\xd0\xbc\xd0\xbe\xd0\xb6\xd0\xbd\xd0\xbe\x20\xd0\xbd\xd0\xb0\xd1\x81\xd1\x82\xd1\x80\xd0\xbe\xd0\xb8\xd1\x82\xd1\x8c\x20\xd1\x86\xd0\xb2\xd0\xb5\xd1\x82\x20\xd0\xb8\x20\xd0\xbe\xd0\xb1\xd0\xb2\xd0\xbe\xd0\xb4\xd0\xba\xd1\x83\x20\xd1\x80\xd0\xb0\xd1\x81\xd1\x82\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb9\x20\xd0\xb2\x20\xd1\x80\xd1\x83\xd0\xba\xd0\xb5\x20\xd0\xb8\x20\xd0\xb8\xd1\x85\x20\xd1\x81\xd0\xbb\xd0\xbe\xd1\x82\xd0\xbe\xd0\xb2\x2c\x20\xd1\x87\xd1\x82\xd0\xbe\xd0\xb1\xd1\x8b\x20\xd0\xbb\xd1\x83\xd1\x87\xd1\x88\xd0\xb5\x20\xd1\x80\xd0\xb0\xd0\xb7\xd0\xbb\xd0\xb8\xd1\x87\xd0\xb0\xd1\x82\xd1\x8c\x20\xd0\xb8\xd0\xb3\xd1\x80\xd0\xbe\xd0\xba\xd0\xbe\xd0\xb2\x2e",
    "\xd0\x92\x20\xd0\xba\xd0\xbe\xd0\xbe\xd0\xbf\xd0\xb5\xd1\x80\xd0\xb0\xd1\x82\xd0\xb8\xd0\xb2\xd0\xb5\x20\xd0\xbd\xd0\xb0\xd0\xb6\xd0\xbc\xd0\xb8\xd1\x82\xd0\xb5\x20\x45\x4e\x54\x45\x52\x2c\x20\xd1\x87\xd1\x82\xd0\xbe\xd0\xb1\xd1\x8b\x20\xd0\xbe\xd1\x82\xd0\xba\xd1\x80\xd1\x8b\xd1\x82\xd1\x8c\x20\xd1\x87\xd0\xb0\xd1\x82\x2e\x20\xd0\x9d\xd0\xb0\xd0\xbf\xd0\xb8\xd1\x88\xd0\xb8\xd1\x82\xd0\xb5\x20\xd1\x81\xd0\xbe\xd0\xbe\xd0\xb1\xd1\x89\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5\x20\xd0\xb8\x20\xd1\x81\xd0\xbd\xd0\xbe\xd0\xb2\xd0\xb0\x20\xd0\xbd\xd0\xb0\xd0\xb6\xd0\xbc\xd0\xb8\xd1\x82\xd0\xb5\x20\x45\x4e\x54\x45\x52\x20\xd0\xb4\xd0\xbb\xd1\x8f\x20\xd0\xbe\xd1\x82\xd0\xbf\xd1\x80\xd0\xb0\xd0\xb2\xd0\xba\xd0\xb8\x2e\x20\x45\x53\x43\x20\xd0\xbe\xd1\x82\xd0\xbc\xd0\xb5\xd0\xbd\xd1\x8f\xd0\xb5\xd1\x82\x20\xd0\xb2\xd0\xb2\xd0\xbe\xd0\xb4\x2e\x20\xd0\x92\x20\xd1\x87\xd0\xb0\xd1\x82\x20\xd1\x82\xd0\xb0\xd0\xba\xd0\xb6\xd0\xb5\x20\xd0\xbf\xd1\x80\xd0\xb8\xd1\x85\xd0\xbe\xd0\xb4\xd1\x8f\xd1\x82\x20\xd0\xbd\xd0\xb0\xd0\xb7\xd0\xb2\xd0\xb0\xd0\xbd\xd0\xb8\xd1\x8f\x20\xd0\xbe\xd1\x82\xd0\xbc\xd0\xb5\xd1\x87\xd0\xb5\xd0\xbd\xd0\xbd\xd1\x8b\xd1\x85\x20\xd1\x80\xd0\xb0\xd1\x81\xd1\x82\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb9\x2e",
    "\xd0\x9d\xd0\xb0\xd0\xb6\xd0\xbc\xd0\xb8\xd1\x82\xd0\xb5\x20\xd0\xbd\xd0\xb0\x20\xd0\xba\xd0\xbe\xd0\xbb\xd1\x91\xd1\x81\xd0\xb8\xd0\xba\xd0\xbe\x3a\x20\xd0\xbf\xd0\xbe\xd1\x8f\xd0\xb2\xd0\xb8\xd1\x82\xd1\x81\xd1\x8f\x20\xd1\x81\xd0\xb8\xd0\xbd\xd0\xb8\xd0\xb9\x20\xd0\xbf\xd0\xb8\xd0\xbd\xd0\xb3\x2e\x20\xd0\x94\xd0\xb5\xd1\x80\xd0\xb6\xd0\xb8\xd1\x82\xd0\xb5\x20\x43\x54\x52\x4c\x20\xd0\xb8\x20\xd0\xb2\xd0\xb5\xd0\xb4\xd0\xb8\xd1\x82\xd0\xb5\x20\xd0\xbc\xd1\x8b\xd1\x88\xd1\x8c\x3a\x20\xd0\xbb\xd0\xb8\xd0\xbd\xd0\xb8\xd1\x8f\x20\xd0\xbf\xd0\xbe\xd0\xb2\xd1\x82\xd0\xbe\xd1\x80\xd1\x8f\xd0\xb5\xd1\x82\x20\xd0\xb4\xd0\xb2\xd0\xb8\xd0\xb6\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5\x2e\x20\x41\x4c\x54\x20\x2b\x20\xd0\x9b\xd0\x9a\xd0\x9c\x20\xd0\xbf\xd0\xbe\x20\xd1\x81\xd0\xbb\xd0\xbe\xd1\x82\xd1\x83\x20\xd0\xbf\xd0\xbe\xd0\xb4\xd1\x81\xd0\xb2\xd0\xb5\xd1\x82\xd0\xb8\xd1\x82\x20\xd1\x80\xd0\xb0\xd1\x81\xd1\x82\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5\x20\xd0\xb8\x20\xd1\x81\xd0\xbe\xd0\xbe\xd0\xb1\xd1\x89\xd0\xb8\xd1\x82\x20\xd0\xb5\xd0\xb3\xd0\xbe\x20\xd0\xbf\xd0\xb5\xd1\x80\xd0\xb5\xd0\xb7\xd0\xb0\xd1\x80\xd1\x8f\xd0\xb4\xd0\xba\xd1\x83\x2e\x20\xd0\x92\xd0\xbe\x20\xd0\xb2\xd1\x80\xd0\xb5\xd0\xbc\xd1\x8f\x20\xd0\xb2\xd1\x8b\xd0\xb1\xd0\xbe\xd1\x80\xd0\xb0\x20\xd1\x80\xd0\xb0\xd1\x81\xd1\x82\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb9\x20\xd0\xb8\xd1\x81\xd0\xbf\xd0\xbe\xd0\xbb\xd1\x8c\xd0\xb7\xd1\x83\xd0\xb9\xd1\x82\xd0\xb5\x20\x41\x4c\x54\x20\x2b\x20\xd0\x9f\xd0\x9a\xd0\x9c\x2e",
    "\xd0\x9a\xd0\xbb\xd0\xb0\xd0\xb2\xd0\xb8\xd1\x88\xd0\xb8\x20\x31\xe2\x80\x93\x39\x20\xd0\xb2\xd1\x8b\xd0\xb1\xd0\xb8\xd1\x80\xd0\xb0\xd1\x8e\xd1\x82\x20\xd0\xb2\xd0\xb0\xd1\x88\xd0\xb8\x20\xd1\x81\xd0\xbb\xd0\xbe\xd1\x82\xd1\x8b\x2c\x20\xd0\xb0\x20\x30\x20\xe2\x80\x94\x20\xd0\xb4\xd0\xb5\xd1\x81\xd1\x8f\xd1\x82\xd1\x8b\xd0\xb9\x2e\x20\x51\x20\xd0\xb1\xd0\xb5\xd1\x80\xd1\x91\xd1\x82\x20\xd0\xbb\xd0\xbe\xd0\xbf\xd0\xb0\xd1\x82\xd1\x83\x2e\x20\xd0\x9d\xd0\xb0\x20\xd0\xba\xd0\xbe\xd0\xbd\xd0\xb2\xd0\xb5\xd0\xb9\xd0\xb5\xd1\x80\xd0\xb5\x20\xd1\x8d\xd1\x82\xd0\xbe\x20\xd1\x82\xd0\xbe\xd0\xb6\xd0\xb5\x20\xd1\x80\xd0\xb0\xd0\xb1\xd0\xbe\xd1\x82\xd0\xb0\xd0\xb5\xd1\x82\x2e\x20\xd0\x98\xd1\x81\xd0\xbf\xd0\xbe\xd0\xbb\xd1\x8c\xd0\xb7\xd0\xbe\xd0\xb2\xd0\xb0\xd1\x82\xd1\x8c\x20\xd0\xbc\xd0\xbe\xd0\xb6\xd0\xbd\xd0\xbe\x20\xd1\x81\xd0\xb2\xd0\xbe\xd0\xb8\x20\xd0\xb8\x20\xd0\xbe\xd0\xb1\xd1\x89\xd0\xb8\xd0\xb5\x20\xd1\x80\xd0\xb0\xd1\x81\xd1\x82\xd0\xb5\xd0\xbd\xd0\xb8\xd1\x8f\x2e\x20\xd0\x9c\xd0\xbe\xd0\xb7\xd0\xb3\xd0\xb8\x2d\xd0\xb8\x2e\x2e\x2e\x20\xd1\x82\xd0\xb5\xd0\xbf\xd0\xb5\xd1\x80\xd1\x8c\x20\xd0\xb2\xd1\x8b\x20\xd0\xb3\xd0\xbe\xd1\x82\xd0\xbe\xd0\xb2\xd1\x8b\x21"
};
}

CoopHelpDialog::CoopHelpDialog(LawnApp* theApp) :
    Dialog(nullptr, nullptr, DIALOG_COOP_HELP, true, _S(""), _S(""), _S(""), BUTTONS_NONE),
    mApp(theApp), mZombie(new ReanimationWidget()),
    mPreviewCursor(theApp->GetImage(_S("Cursor13"))),
    mPreviewPaper(theApp->GetImage(_S("images/Almanac_PlantCard"))), mPreviewStarted(GetTickCount()),
    mWasCollapsed(theApp->mNetLobbyPanel->IsCollapsed()),
    mWasLanExpanded(theApp->mNetLobbyPanel->IsLanExpanded())
{
    mZOrder = 10000;
    mWantsFocus = true;
    Resize(0, 0, mApp->mWidth, mApp->mHeight);
    mZombie->AddReanimation(0, 0, REANIM_ZOMBIE);
    Zombie::SetupReanimLayers(mZombie->mReanim, ZOMBIE_NORMAL);
    mZombie->mReanim->PlayReanim("anim_idle", REANIM_LOOP, 0, 12.0f);
    mZombie->mReanim->OverrideScale(1.2f, 1.2f);
    wchar_t windows[MAX_PATH];
    const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    if (length && length < MAX_PATH)
    {
        const std::wstring fonts = std::wstring(windows, length) + L"\\Fonts\\";
        mSpeechFont = new SDL3Font(mApp, WStringToString(fonts + L"trebuc.ttf"), 14);
        mTitleFont = new SDL3Font(mApp, WStringToString(fonts + L"trebucbd.ttf"), 17);
        if (!mSpeechFont->mFont || !mTitleFont->mFont)
        { delete mSpeechFont; delete mTitleFont; mSpeechFont = mTitleFont = nullptr; }
        else { mSpeechFont->SetActive(true); mTitleFont->SetActive(true); }
    }
    NetTrace("COOP_HELP_FONT face=%s body=14 headerBold=17", mSpeechFont ? "TrebuchetMS" : "nativeFallback");
    SetStep(0);
}

CoopHelpDialog::~CoopHelpDialog()
{
    StopTypingSound();
    delete mBubbleImage;
    delete mSpeechFont;
    delete mTitleFont;
    delete mZombie;
    delete mPreviewCursor;
    delete mPreviewPaper;
}

void CoopHelpDialog::ClosePreview()
{
    if (!mPreview) return;
    if (mApp->GetDialog(mPreviewId) == mPreview)
        mApp->KillDialog(mPreviewId);
    mPreview = nullptr;
    mPreviewId = -1;
}

void CoopHelpDialog::SetStep(int theStep)
{
    StopTypingSound();
    delete mBubbleImage; mBubbleImage = nullptr;
    ClosePreview();
    mStep = max(0, min(STEP_COUNT - 1, theStep));
    mPreviewStarted = GetTickCount();
    mSpeechStarted = mLastTypingTick = mPreviewStarted;
    mSpeech = StringToSexyStringFast(kSpeech[mStep]);
    mSpeechEnds = CodepointEnds(mSpeech);
    mVisibleCharacters = 0;
    mApp->mNetLobbyPanel->SetTutorialLayout(mStep == 0, mStep == 1);
    if (mStep == 2)
    {
        mPreview = new NetOnlineDialog(mApp, true);
        mPreviewId = DIALOG_COOP_ONLINE;
        mPreview->mIsModal = false;
        mApp->AddDialog(mPreview);
    }
    else if (mStep == 3)
    {
        NewOptionsDialog* aOptions = new NewOptionsDialog(mApp, true);
        LawnApp::CenterDialog(aOptions, IMAGE_OPTIONS_MENUBACK->mWidth,
            aOptions->GetPreferredHeight(IMAGE_OPTIONS_MENUBACK->mWidth));
        aOptions->mIsModal = false;
        mPreview = aOptions;
        mPreviewId = DIALOG_NEWOPTIONS;
        mApp->AddDialog(mPreview);
    }
    if (mWidgetManager) mWidgetManager->SetFocus(this);
    NetTrace("COOP_HELP step=%d", mStep + 1);
    MarkDirty();
}

Rect CoopHelpDialog::GetSpeechBubbleRect() const
{
    if (mStep == 2) return Rect(mWidth - 772, mHeight - 290, min(560, mWidth - 214), 218);
    if (mStep == 3) return Rect(mWidth - 264, 100, 250, 290);
    if (mStep == 5) return Rect(mWidth - 474, 50, 460, 240);
    return Rect(mWidth - 474, 150, 460, 240);
}

Rect CoopHelpDialog::GetZombieBounds() const
{
    return Rect(mWidth - 158, mHeight - 198, 144, 186);
}

Point CoopHelpDialog::GetSpeechTailProbe() const
{
    Point tail[3]; SpeechTail(GetSpeechBubbleRect(), GetZombieBounds(), tail);
    return Point((tail[0].mX + tail[1].mX + tail[2].mX * 2) / 4,
        (tail[0].mY + tail[1].mY + tail[2].mY * 2) / 4);
}

Point CoopHelpDialog::GetSpeechTailJoinProbe() const
{
    Point tail[3]; SpeechTail(GetSpeechBubbleRect(), GetZombieBounds(), tail);
    return Point((tail[0].mX + tail[1].mX) / 2, (tail[0].mY + tail[1].mY) / 2);
}

void CoopHelpDialog::BuildBubbleImage()
{
    const Rect bubble = GetSpeechBubbleRect();
    Point tail[3]; SpeechTail(bubble, GetZombieBounds(), tail);
    const int left = min(bubble.mX, tail[2].mX) - 8, top = min(bubble.mY, tail[2].mY) - 8;
    const int right = max(bubble.mX + bubble.mWidth, tail[2].mX) + 8;
    const int bottom = max(bubble.mY + bubble.mHeight, tail[2].mY) + 8;
    mBubbleImageBounds = Rect(left, top, right - left, bottom - top);
    const double rx = bubble.mWidth / 2.0, ry = bubble.mHeight / 2.0;
    const double cx = bubble.mX + rx, cy = bubble.mY + ry;
    const auto inside = [&](double x, double y)
    {
        const double nx = (x - cx) / rx, ny = (y - cy) / ry;
        return nx * nx + ny * ny <= 1.0 || InsideTriangle(x, y, tail);
    };
    const auto distance = [&](double x, double y)
    {
        const double nx = (x - cx) / rx, ny = (y - cy) / ry;
        const double gradient = 2.0 * std::sqrt(nx * nx / (rx * rx) + ny * ny / (ry * ry));
        const double ellipse = gradient > 0.0001 ? (nx * nx + ny * ny - 1.0) / gradient : -min(rx, ry);
        return min(ellipse, TriangleDistance(x, y, tail));
    };
    // Erode the union, rather than outlining its two shapes separately: the
    // tail joins the body without an internal stroke. Cache 4x edge coverage.
    const double inset[8][2] = { {1.25, 0}, {-1.25, 0}, {0, 1.25}, {0, -1.25},
        {0.884, 0.884}, {-0.884, 0.884}, {0.884, -0.884}, {-0.884, -0.884} };
    std::vector<ulong> bits(mBubbleImageBounds.mWidth * mBubbleImageBounds.mHeight);
    for (int y = 0; y < mBubbleImageBounds.mHeight; ++y)
        for (int x = 0; x < mBubbleImageBounds.mWidth; ++x)
        {
            const double px = left + x, py = top + y;
            const double edge = distance(px + 0.5, py + 0.5);
            double outer = 0, inner = 0;
            if (edge < -2.0) outer = inner = 1;
            else if (edge < 1.0)
            {
                int outerSamples = 0, innerSamples = 0;
                for (int sy = 0; sy < 4; ++sy)
                    for (int sx = 0; sx < 4; ++sx)
                    {
                        const double ax = px + (sx + 0.5) / 4.0, ay = py + (sy + 0.5) / 4.0;
                        if (!inside(ax, ay)) continue;
                        ++outerSamples;
                        bool filled = true;
                        for (const auto& offset : inset)
                            if (!inside(ax + offset[0], ay + offset[1])) { filled = false; break; }
                        if (filled) ++innerSamples;
                    }
                outer = outerSamples / 16.0; inner = innerSamples / 16.0;
            }
            const double shadow = max(0.0, min(1.0, (4.5 - distance(px - 1.5, py - 2.5)) / 4.5)) * 0.13 * (1.0 - outer);
            const double alpha = outer + shadow;
            if (alpha <= 0) continue;
            const double stroke = outer - inner;
            const int red = (int)((249 * inner + 167 * stroke + 60 * shadow) / alpha + 0.5);
            const int green = (int)((247 * inner + 153 * stroke + 50 * shadow) / alpha + 0.5);
            const int blue = (int)((242 * inner + 133 * stroke + 40 * shadow) / alpha + 0.5);
            bits[y * mBubbleImageBounds.mWidth + x] = ((ulong)(alpha * 255 + 0.5) << 24) |
                ((ulong)red << 16) | ((ulong)green << 8) | (ulong)blue;
        }
    mBubbleImage = new MemoryImage(mApp);
    mBubbleImage->SetBits(bits.data(), mBubbleImageBounds.mWidth, mBubbleImageBounds.mHeight, true);
}

Font* CoopHelpDialog::BodyFont() const
{ return mSpeechFont ? static_cast<Font*>(mSpeechFont) : FONT_BRIANNETOD12; }

Font* CoopHelpDialog::HeaderFont() const
{ return mTitleFont ? static_cast<Font*>(mTitleFont) : FONT_BRIANNETOD16; }

int CoopHelpDialog::GetSpeechFontHeight() const
{ return BodyFont()->GetHeight(); }

Rect CoopHelpDialog::GetButtonRect(int theButton) const
{
    const int width = theButton == 2 ? 94 : 86;
    return Rect(mWidth - 456 + theButton * 94, mHeight - 48, width, 30);
}

Rect CoopHelpDialog::GetBodyRect() const
{
    const Rect r = GetSpeechBubbleRect();
    const int inset = mStep == 3 ? 30 : 40;
    return Rect(r.mX + inset, r.mY + 62, r.mWidth - inset * 2, r.mHeight - 108);
}

bool CoopHelpDialog::BodyTextFits() const
{
    const Rect bubble = GetSpeechBubbleRect();
    Font* header = HeaderFont();
    const double top = 44.0 - header->GetAscent();
    const double relative = (top - bubble.mHeight / 2.0) / (bubble.mHeight / 2.0);
    const int headerWidth = (int)((bubble.mWidth - 8) * std::sqrt(max(0.0, 1.0 - relative * relative)));
    if (header->StringWidth(StringToSexyStringFast(kTitles[mStep])) > headerWidth) return false;
    const Rect body = GetBodyRect();
    const auto lines = WrapSpeech(mSpeech, BodyFont(), body.mWidth);
    for (const SexyString& line : lines)
        if (BodyFont()->StringWidth(line) > body.mWidth) return false;
    return (int)lines.size() * SpeechLineHeight(BodyFont()) <= body.mHeight;
}

Rect CoopHelpDialog::GetHighlightRect() const
{
    NetLobbyPanel* p = mApp->mNetLobbyPanel;
    if (mStep == 0) return p->GetButtonRect(NetLobbyPanel::BTN_TOGGLE);
    if (mStep == 1)
    {
        const Rect r = p->GetPanelRect();
        return Rect(r.mX + 14, r.mY + 142, 272, 118);
    }
    const bool previewValid = mPreview && mApp->GetDialog(mPreviewId) == mPreview;
    if (mStep == 2 && previewValid) return Rect(mPreview->mX + mPreview->mWidth - 190, mPreview->mY + 98, 142, 44);
    if (mStep == 3 && previewValid)
    {
        const auto* options = static_cast<NewOptionsDialog*>(mPreview);
        const Widget* button = options->mCoopAppearanceButton;
        return Rect(mPreview->mX + button->mX - 5, mPreview->mY + button->mY - 5, button->mWidth + 10, button->mHeight + 10);
    }
    return Rect();
}

void CoopHelpDialog::Update()
{
    if (mClosed) return;
    if (!mApp->mGameSelector || mApp->mBoard)
    {
        Close();
        return;
    }
    Widget::Update();
    mZombie->Update();
    if (!IsTypingComplete())
    {
        const uint32_t now = GetTickCount();
        const size_t visible = min(mSpeechEnds.size(), (size_t)((now - mSpeechStarted) * 80U / 1000U));
        const bool advanced = visible > mVisibleCharacters;
        mVisibleCharacters = visible;
        if (IsTypingComplete()) StopTypingSound();
        else if (advanced && now - mLastTypingTick >= 80U)
        {
            mLastTypingTick = now;
            if (mApp->mSoundManager && !mApp->IsMuted() && mApp->GetSfxVolume() > 0)
            {
                if (!mTypingSound)
                {
                    mTypingSound = mApp->mSoundManager->GetSoundInstance(SOUND_TAP);
                    if (mTypingSound) mTypingSound->SetVolume(0.09);
                }
                if (mTypingSound && mTypingSound->Play(false, false)) ++mTypingSoundTicks;
            }
        }
    }
    if (mWidth != mApp->mWidth || mHeight != mApp->mHeight)
    {
        delete mBubbleImage; mBubbleImage = nullptr;
        Resize(0, 0, mApp->mWidth, mApp->mHeight);
    }
    MarkDirty();
}

void CoopHelpDialog::Draw(Graphics* g)
{
    Rect h = GetHighlightRect();
    h.mX = max(0, min(mWidth, h.mX)); h.mY = max(0, min(mHeight, h.mY));
    h.mWidth = max(0, min(h.mWidth, mWidth - h.mX)); h.mHeight = max(0, min(h.mHeight, mHeight - h.mY));
    g->SetColor(Color(0, 0, 0, 190));
    if (h.mWidth && h.mHeight)
    {
        g->FillRect(0, 0, mWidth, h.mY);
        g->FillRect(0, h.mY, h.mX, h.mHeight);
        g->FillRect(h.mX + h.mWidth, h.mY, mWidth - h.mX - h.mWidth, h.mHeight);
        g->FillRect(0, h.mY + h.mHeight, mWidth, mHeight - h.mY - h.mHeight);
        g->SetColor(Color(190, 245, 105));
        g->DrawRect(h.mX - 2, h.mY - 2, h.mWidth + 3, h.mHeight + 3);
        g->DrawRect(h.mX - 3, h.mY - 3, h.mWidth + 5, h.mHeight + 5);
    }
    else g->FillRect(0, 0, mWidth, mHeight);

    // The ordinary native zombie stays in one place throughout the short tour.
    const Rect zombieBounds = GetZombieBounds();
    Graphics zombie(*g);
    zombie.Translate(zombieBounds.mX + 6, zombieBounds.mY + 2);
    mZombie->Draw(&zombie);
    const Rect r = GetSpeechBubbleRect();
    if (!mBubbleImage) BuildBubbleImage();
    g->PushState(); g->SetColorizeImages(false); g->SetLinearBlend(true); g->SetPixelArtBlend(false);
    g->DrawImage(mBubbleImage, mBubbleImageBounds.mX, mBubbleImageBounds.mY);
    const Color speechInk(77, 66, 55);
    NetUi::Literal(g, HeaderFont(), StringToSexyStringFast(kTitles[mStep]), r.mX + r.mWidth / 2, r.mY + 44, speechInk, 0);
    const Rect body = GetBodyRect();
    const auto lines = WrapSpeech(mSpeech, BodyFont(), body.mWidth);
    g->PushState();
    g->ClipRect(body);
    int baseline = body.mY + BodyFont()->GetAscent();
    size_t remaining = mVisibleCharacters;
    for (const SexyString& line : lines)
    {
        const auto ends = CodepointEnds(line);
        const size_t count = min(remaining, ends.size());
        if (count) BodyFont()->DrawString(g, body.mX, baseline, line.substr(0, ends[count - 1]), speechInk, g->mClipRect);
        remaining = remaining > ends.size() ? remaining - ends.size() - 1 : 0;
        baseline += SpeechLineHeight(BodyFont());
    }
    g->PopState();
    g->PopState();
    if (mStep == 5) DrawCommunicationPreview(g);
    const SexyString labels[] = { StringToSexyStringFast("\xd0\x97\xd0\x90\xd0\x9a\xd0\xa0\xd0\xab\xd0\xa2\xd0\xac"), StringToSexyStringFast("\xd0\x9d\xd0\x90\xd0\x97\xd0\x90\xd0\x94"),
        StringToSexyStringFast(mStep == STEP_COUNT - 1 ? "\xd0\x93\xd0\x9e\xd0\xa2\xd0\x9e\xd0\x92\xd0\x9e" : "\xd0\x94\xd0\x90\xd0\x9b\xd0\x95\xd0\x95") };
    for (int b = 0; b < 3; ++b)
        NetUi::StoneButton(g, GetButtonRect(b), labels[b], mPressed == b, GetButtonRect(b).Contains(mMouseX, mMouseY), b == 1 && mStep == 0);
    NetUi::Literal(g, FONT_BRIANNETOD12, StrFormat(_S("%d / %d"), mStep + 1, STEP_COUNT),
        mWidth - 488, mHeight - 26, NetUi::Cream(), 0);
}

void CoopHelpDialog::DrawCommunicationPreview(Graphics* g)
{
    const uint32_t phase = (GetTickCount() - mPreviewStarted) % 2800U;
    const int y = 350;
    const auto blue = GetRelativePlayerColor(0, 1);
    const char* labels[] = { "\xd0\x9a\xd0\x9e\xd0\x9b\xd0\x81\xd0\xa1\xd0\x98\xd0\x9a\xd0\x9e", "CTRL", "ALT + \xd0\x9b\xd0\x9a\xd0\x9c" };
    for (int panel = 0; panel < 3; ++panel)
    {
        const int x = mWidth - 620 + panel * 144;
        NetUi::Paper(g, mPreviewPaper, Rect(x, y, 140, 154));
        NetUi::Literal(g, FONT_BRIANNETOD12, StringToSexyStringFast(labels[panel]), x + 70, y + 22, NetUi::Ink(), 0);
        float cursorX = (float)(x + 72), cursorY = (float)(y + 82);
        if (panel == 0 && phase >= 400U && phase < 2400U)
        {
            const uint32_t age = phase - 400U;
            const int radius = 8 + age / 70;
            g->SetColor(Color(blue.r, blue.g, blue.b, (2000U - age) * 255U / 2000U));
            g->DrawRect(x + 70 - radius, y + 86 - radius, radius * 2, radius * 2);
            g->DrawLine(x + 62, y + 86, x + 78, y + 86);
            g->DrawLine(x + 70, y + 78, x + 70, y + 94);
        }
        else if (panel == 1)
        {
            const int count = min(46, (int)(phase * 46 / 1700U));
            const int pointsX[] = { 20, 45, 66, 88, 117 };
            const int pointsY[] = { 112, 86, 97, 64, 75 };
            for (int i = 1; i <= count; ++i)
            {
                const int a = (i - 1) * 4, b = i * 4;
                const int segmentA = min(3, a / 46), segmentB = min(3, b / 46);
                const int ax = pointsX[segmentA] + (pointsX[segmentA + 1] - pointsX[segmentA]) * (a - segmentA * 46) / 46;
                const int ay = pointsY[segmentA] + (pointsY[segmentA + 1] - pointsY[segmentA]) * (a - segmentA * 46) / 46;
                const int bx = pointsX[segmentB] + (pointsX[segmentB + 1] - pointsX[segmentB]) * (b - segmentB * 46) / 46;
                const int by = pointsY[segmentB] + (pointsY[segmentB + 1] - pointsY[segmentB]) * (b - segmentB * 46) / 46;
                g->SetColor(Color(blue.r, blue.g, blue.b, phase > 2100U ? max(0, 255 - (int)(phase - 2100U) / 3) : 255));
                g->DrawLine(x + ax, y + ay, x + bx, y + by);
                cursorX = (float)(x + bx); cursorY = (float)(y + by);
            }
        }
        else if (panel == 2)
        {
            DrawSeedPacket(g, (float)(x + 45), (float)(y + 44), SEED_SUNFLOWER, SEED_NONE, 0, 255, false, false);
            if (phase >= 400U && phase < 2400U && ((phase - 400U) / 140U) % 2 == 0)
            { g->SetColor(Color(255, 255, 255, 125)); g->FillRect(x + 45, y + 44, 50, 70); }
            cursorX = (float)(x + 80); cursorY = (float)(y + 95);
        }
        if (mPreviewCursor)
        {
            g->PushState(); g->SetColorizeImages(true);
            g->SetColor(Color(blue.r, blue.g, blue.b));
            g->DrawImage(mPreviewCursor, (int)cursorX, (int)cursorY, 22, 22);
            g->PopState();
        }
    }
}

void CoopHelpDialog::Close()
{
    if (mClosed) return;
    mClosed = true;
    StopTypingSound();
    // Dispose before the next Board resets the shared effect holder, even
    // though the dialog itself is deleted later by SafeDeleteWidget.
    mZombie->Dispose();
    NetTrace("COOP_HELP_CLOSE synchronousZombieDispose=%d step=%d", mZombie->mReanim == nullptr ? 1 : 0, mStep + 1);
    ClosePreview();
    if (mApp->mNetLobbyPanel) mApp->mNetLobbyPanel->SetTutorialLayout(mWasCollapsed, mWasLanExpanded, false);
    mApp->KillDialog(DIALOG_COOP_HELP);
    if (mApp->mGameSelector) mApp->mWidgetManager->SetFocus(mApp->mGameSelector);
}

void CoopHelpDialog::MouseDown(int x, int y, int theClickCount)
{
    mPressed = -1;
    if (theClickCount != 1) return;
    for (int b = 0; b < 3; ++b)
        if ((b != 1 || mStep > 0) && GetButtonRect(b).Contains(x, y)) mPressed = b;
    if (mPressed < 0 && GetSpeechBubbleRect().Contains(x, y)) mPressed = 3;
}

void CoopHelpDialog::MouseUp(int x, int y, int theClickCount)
{
    const int b = mPressed;
    mPressed = -1;
    if (theClickCount != 1 || b < 0) return;
    if (b == 3) { if (GetSpeechBubbleRect().Contains(x, y)) FinishTyping(); return; }
    if (!GetButtonRect(b).Contains(x, y)) return;
    if (b == 0 || (b == 2 && mStep == STEP_COUNT - 1)) Close();
    else SetStep(mStep + (b == 1 ? -1 : 1));
}

void CoopHelpDialog::MouseMove(int x, int y)
{ mMouseX = x; mMouseY = y; }

void CoopHelpDialog::KeyDown(KeyCode theKey)
{
    if (theKey == KEYCODE_ESCAPE) Close();
    else if (theKey == KEYCODE_LEFT && mStep > 0) SetStep(mStep - 1);
    else if ((theKey == KEYCODE_RETURN || theKey == KEYCODE_SPACE) && !IsTypingComplete()) FinishTyping();
    else if (theKey == KEYCODE_RETURN || theKey == KEYCODE_SPACE || theKey == KEYCODE_RIGHT)
    {
        if (mStep == STEP_COUNT - 1) Close();
        else SetStep(mStep + 1);
    }
}

float CoopHelpDialog::GetZombieAnimationTime() const
{ return mZombie->mReanim->mAnimTime; }

void CoopHelpDialog::FinishTyping()
{
    mVisibleCharacters = mSpeechEnds.size();
    StopTypingSound();
}

void CoopHelpDialog::StopTypingSound()
{
    if (mTypingSound) { mTypingSound->Release(); mTypingSound = nullptr; }
}

double CoopHelpDialog::GetTypingSoundGain() const
{ return mTypingSound ? mTypingSound->GetVolume() : 0.0; }
