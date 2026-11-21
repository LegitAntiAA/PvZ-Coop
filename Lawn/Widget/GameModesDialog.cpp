#include "GameModesDialog.h"
#include "GameButton.h"
#include "GameSelector.h"
#include "NetUi.h"
#include "../../LawnApp.h"
#include "../Net/NetManager.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/TodStringFile.h"

using namespace Sexy;
using LawnFusion::ContentMode;

namespace
{
class ModeButton : public LawnStoneButton
{
public:
    ModeButton(int theId, ButtonListener* theListener, const SexyString& theLabel) :
        LawnStoneButton(nullptr, theId, theListener)
    { SetLabel(theLabel); mHasAlpha = true; }

    void Draw(Graphics* g) override
    {
        if (!mBtnNoDraw)
            NetUi::StoneButton(g, Rect(0, 0, mWidth, mHeight), mLabel,
                mIsDown && mIsOver && !mDisabled, mIsOver && !mDisabled, mDisabled);
    }
};
}

GameModesDialog::GameModesDialog(LawnApp* theApp) :
    LawnDialog(theApp, DIALOG_GAME_MODES, true, _S("GAMEMODES"), _S(""),
        _S("CLOSE"), Dialog::BUTTONS_FOOTER),
    mPaperImage(theApp->GetImage(_S("images/Almanac_PlantCard"))),
    mKeyboardChoice((int)theApp->GetContentMode())
{
    mModeButtons[0] = new ModeButton(100, this, _S("Classic"));
    mModeButtons[1] = new ModeButton(101, this, _S("Fusion"));
    const int edges = IMAGE_DIALOG_TOPLEFT->mWidth + IMAGE_DIALOG_TOPRIGHT->mWidth;
    const int tile = IMAGE_DIALOG_TOPMIDDLE->mWidth;
    const int width = edges + ((600 - edges + tile - 1) / tile) * tile;
    const int caps = DIALOG_HEADER_OFFSET + IMAGE_DIALOG_TOPLEFT->mHeight + IMAGE_DIALOG_BOTTOMLEFT->mHeight;
    const int row = IMAGE_DIALOG_CENTERLEFT->mHeight;
    Resize(0, 0, width, caps + ((450 - caps + row - 1) / row) * row);
    LawnApp::CenterDialog(this, mWidth, mHeight);
    for (int i = 0; i < 2; ++i)
        mModeButtons[i]->Resize(51, 136 + i * 110, 145, 44);
}

GameModesDialog::~GameModesDialog()
{
    delete mModeButtons[0];
    delete mModeButtons[1];
    delete mPaperImage;
}

void GameModesDialog::AddedToManager(WidgetManager* theManager)
{
    LawnDialog::AddedToManager(theManager);
    for (auto* button : mModeButtons) AddWidget(button);
}

void GameModesDialog::RemovedFromManager(WidgetManager* theManager)
{
    for (auto* button : mModeButtons) RemoveWidget(button);
    LawnDialog::RemovedFromManager(theManager);
}

void GameModesDialog::Update()
{
    LawnDialog::Update();
    for (auto* button : mModeButtons) button->SetDisabled(!mApp->CanSelectContentMode());
}

Rect GameModesDialog::GetModeButtonRect(ContentMode theMode) const
{
    const int index = theMode == ContentMode::Fusion ? 1 : 0;
    return Rect(mX + mModeButtons[index]->mX, mY + mModeButtons[index]->mY,
        mModeButtons[index]->mWidth, mModeButtons[index]->mHeight);
}

void GameModesDialog::Draw(Graphics* g)
{
    LawnDialog::Draw(g);
    const SexyString descriptions[] = {
        _S("The original adventure, mini-games and co-op modes."),
        _S("Fusion plants, zombies, adventure and mini-games.")
    };
    for (int i = 0; i < 2; ++i)
    {
        const Rect card(39, 112 + i * 110, mWidth - 78, 96);
        NetUi::Paper(g, mPaperImage, card);
        if ((int)mApp->GetContentMode() == i)
        {
            g->SetColor(Color(75, 114, 38));
            g->DrawRect(card.mX + 3, card.mY + 3, card.mWidth - 7, card.mHeight - 7);
            NetUi::Literal(g, FONT_BRIANNETOD12, _S("SELECTED"),
                card.mX + card.mWidth - 13, card.mY + 19, NetUi::Ink(), 1);
        }
        TodDrawStringWrappedHelper(g, descriptions[i], Rect(211, card.mY + 30, mWidth - 265, 55),
            FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT, true);
        if (mKeyboardChoice == i)
        {
            g->SetColor(Color(238, 210, 106));
            const auto* button = mModeButtons[i];
            g->DrawRect(button->mX - 2, button->mY - 2, button->mWidth + 3, button->mHeight + 3);
        }
    }
    const SexyString status = !mNotice.empty() ? mNotice : mApp->NetIsClient() ?
        _S("The host chooses the gamemode for everyone.") :
        _S("Choose a gamemode. Your Classic progress is kept separately.");
    TodDrawStringWrappedHelper(g, status, Rect(46, 339, mWidth - 92, 58),
        FONT_BRIANNETOD12, mNotice.empty() ? NetUi::Cream() : Color(255, 218, 139), DS_ALIGN_CENTER, true);
}

void GameModesDialog::SelectMode(ContentMode theMode)
{
    mKeyboardChoice = (int)theMode;
    std::string error;
    if (!mApp->SetContentMode(theMode, &error))
    {
        mNotice = StringToSexyString(error);
        mApp->PlaySample(SOUND_BUZZER);
        return;
    }
    mApp->KillDialog(DIALOG_GAME_MODES);
}

void GameModesDialog::ButtonDepress(int theId)
{
    if (theId == 100 || theId == 101) SelectMode((ContentMode)(theId - 100));
    else if (theId == 1000) mApp->KillDialog(DIALOG_GAME_MODES);
}

void GameModesDialog::KeyDown(KeyCode theKey)
{
    if (theKey == KEYCODE_ESCAPE) { mApp->KillDialog(DIALOG_GAME_MODES); return; }
    if (theKey == KEYCODE_TAB || theKey == KEYCODE_UP || theKey == KEYCODE_DOWN ||
        theKey == KEYCODE_LEFT || theKey == KEYCODE_RIGHT)
    { mKeyboardChoice = 1 - mKeyboardChoice; return; }
    if (theKey == KEYCODE_RETURN || theKey == KEYCODE_SPACE)
    { SelectMode((ContentMode)mKeyboardChoice); return; }
}

bool LawnApp::CanSelectContentMode() const
{
    auto* app = const_cast<LawnApp*>(this);
    const int dialogs = app->GetDialogCount();
    return !NetIsClient() && mGameScene == SCENE_MENU && mGameSelector && !mBoard &&
        !mSeedChooserScreen && !mGameSelector->mLoading && mGameSelector->MainMenuIconsVisible() &&
        (dialogs == 0 || (dialogs == 1 && app->GetDialog(DIALOG_GAME_MODES)));
}

bool LawnApp::IsContentModeAvailable(ContentMode theMode, std::string* theError) const
{
    if (theError) theError->clear();
    if (theMode == ContentMode::Classic) return true;
    if (theMode == ContentMode::Fusion) return true;
    if (theError) *theError = theMode == ContentMode::Fusion ?
        "Fusion gameplay is not available in this build yet." : "Unknown gamemode.";
    return false;
}

bool LawnApp::SetContentMode(ContentMode theMode, std::string* theError)
{
    if (theError) theError->clear();
    if (!CanSelectContentMode())
    {
        if (theError) *theError = NetIsClient() ? "The host chooses the gamemode for everyone." :
            "Return to the main menu before changing the gamemode.";
        return false;
    }
    if (!IsContentModeAvailable(theMode, theError)) return false;
    if (theMode == mContentMode) return true;
    mContentMode = theMode;
    if (mNetManager) mNetManager->NotifyContentModeChanged();
    mGameSelector->SyncProfile(false);
    return true;
}

bool LawnApp::ApplyContentModeFromHost(ContentMode theMode, std::string* theError)
{
    if (theError) theError->clear();
    if (!NetIsClient() || !IsContentModeAvailable(theMode, theError)) return false;
    if (theMode != mContentMode && (mBoard || mGameScene != SCENE_MENU))
    {
        if (theError) *theError = "The host changed gamemode outside the main menu.";
        return false;
    }
    if (!mHasRemoteContentMode)
    {
        mLocalContentModeBackup = mContentMode;
        mHasRemoteContentMode = true;
    }
    if (theMode != mContentMode)
    {
        KillDialog(DIALOG_GAME_MODES);
        KillAlmanacDialog();
        mContentMode = theMode;
        if (mGameSelector) mGameSelector->SyncProfile(false);
    }
    return true;
}

void LawnApp::ShowGameModes()
{
    if (mGameScene != SCENE_MENU || !mGameSelector || !mGameSelector->MainMenuIconsVisible() ||
        mGameSelector->mLoading || mBoard || GetDialogCount()) return;
    auto* dialog = new GameModesDialog(this);
    AddDialog(dialog);
    mWidgetManager->SetFocus(dialog);
}
