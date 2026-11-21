#ifndef __GAMEMODESDIALOG_H__
#define __GAMEMODESDIALOG_H__

#include "LawnDialog.h"
#include "../Fusion/FusionMode.h"

class GameModesDialog : public LawnDialog
{
    LawnStoneButton* mModeButtons[2];
    Sexy::Image* mPaperImage;
    SexyString mNotice;
    int mKeyboardChoice;

    void SelectMode(LawnFusion::ContentMode theMode);

public:
    explicit GameModesDialog(LawnApp* theApp);
    ~GameModesDialog() override;
    void AddedToManager(Sexy::WidgetManager* theManager) override;
    void RemovedFromManager(Sexy::WidgetManager* theManager) override;
    void Update() override;
    void Draw(Sexy::Graphics* g) override;
    void ButtonDepress(int theId) override;
    void KeyDown(Sexy::KeyCode theKey) override;
    Sexy::Rect GetModeButtonRect(LawnFusion::ContentMode theMode) const;
    int GetKeyboardChoice() const { return mKeyboardChoice; }
    const SexyString& GetNotice() const { return mNotice; }
};

#endif
