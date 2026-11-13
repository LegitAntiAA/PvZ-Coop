#ifndef __COOPHELPDIALOG_H__
#define __COOPHELPDIALOG_H__

#include "../../SexyAppFramework/Dialog.h"
#include <cstdint>
#include <vector>

class LawnApp;
class ReanimationWidget;
namespace Sexy { class DDImage; class MemoryImage; class SDL3Font; class Font; class SoundInstance; }

class CoopHelpDialog : public Sexy::Dialog
{
    LawnApp* mApp;
    ReanimationWidget* mZombie;
    Sexy::DDImage* mPreviewCursor;
    Sexy::DDImage* mPreviewPaper;
    Sexy::MemoryImage* mBubbleImage = nullptr;
    Sexy::Rect mBubbleImageBounds;
    Sexy::SDL3Font* mSpeechFont = nullptr;
    Sexy::SDL3Font* mTitleFont = nullptr;
    uint32_t mPreviewStarted;
    uint32_t mSpeechStarted = 0, mLastTypingTick = 0, mTypingSoundTicks = 0;
    SexyString mSpeech;
    std::vector<size_t> mSpeechEnds;
    size_t mVisibleCharacters = 0;
    Sexy::SoundInstance* mTypingSound = nullptr;
    bool mClosed = false;
    int mStep = 0;
    int mPressed = -1;
    int mMouseX = -1, mMouseY = -1;
    bool mWasCollapsed, mWasLanExpanded;
    Sexy::Dialog* mPreview = nullptr;
    int mPreviewId = -1;

    void SetStep(int theStep);
    void ClosePreview();
    Sexy::Rect GetBodyRect() const;
    Sexy::Rect GetButtonRect(int theButton) const;
    void DrawCommunicationPreview(Sexy::Graphics* g);
    void FinishTyping();
    void StopTypingSound();
    void BuildBubbleImage();
    Sexy::Font* BodyFont() const;
    Sexy::Font* HeaderFont() const;

public:
    static const int STEP_COUNT = 7;
    explicit CoopHelpDialog(LawnApp* theApp);
    ~CoopHelpDialog();
    void Close();
    void Update() override;
    void Draw(Sexy::Graphics* g) override;
    void MouseDown(int x, int y, int theClickCount) override;
    void MouseUp(int x, int y, int theClickCount) override;
    void MouseMove(int x, int y) override;
    void KeyDown(Sexy::KeyCode theKey) override;
    int GetStep() const { return mStep; }
    Sexy::Rect GetHighlightRect() const;
    Sexy::Rect GetNavigationRect(int theButton) const { return GetButtonRect(theButton); }
    float GetZombieAnimationTime() const;
    bool BodyTextFits() const;
    Sexy::Rect GetSpeechBubbleRect() const;
    Sexy::Rect GetZombieBounds() const;
    Sexy::Point GetSpeechTailProbe() const;
    Sexy::Point GetSpeechTailJoinProbe() const;
    Sexy::Rect GetSpeechBodyBounds() const { return GetBodyRect(); }
    int GetSpeechFontHeight() const;
    size_t GetVisibleCharacters() const { return mVisibleCharacters; }
    size_t GetSpeechCharacters() const { return mSpeechEnds.size(); }
    bool IsTypingComplete() const { return mVisibleCharacters == mSpeechEnds.size(); }
    uint32_t GetTypingSoundTicks() const { return mTypingSoundTicks; }
    double GetTypingSoundGain() const;
};

#endif
