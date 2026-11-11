#include "NetLobbyPanel.h"
#include "NetManager.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/EditWidget.h"
#include "../../SexyAppFramework/ImageFont.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Resources.h"
#include "../../GameConstants.h"
#include "../../SexyAppFramework/DDImage.h"
#include "../Widget/NetUi.h"
#include "../Widget/GameSelector.h"

using namespace LawnNet;

static int gPanelEditColors[][4] =
{
	{ 255, 255, 255, 255 },	// bkg
	{ 90, 70, 40, 255 },	// outline
	{ 50, 30, 10, 255 },	// text
	{ 200, 220, 255, 255 },	// hilite
	{ 20, 20, 20, 255 }		// hilite text
};

NetLobbyPanel::NetLobbyPanel(LawnApp* theApp) :
	mApp(theApp),
	mHoverButtonId(0),
	mPressedButtonId(0),
	mLastMouseX(-1),
	mLastMouseY(-1)
{
	mPaperImage = mApp->GetImage(_S("images/Almanac_PlantCard"));
	mLanExpanded = false;
	mCollapsed = true;
	mWasActive = false;
	mNickEdit = new EditWidget(100, this);
	mNickEdit->SetFont(Sexy::FONT_BRIANNETOD12);
	mNickEdit->SetColors(gPanelEditColors, EditWidget::NUM_COLORS);
	mNickEdit->mBlinkDelay = 14;
	mNickEdit->mMaxChars = NET_MAX_NICK_CHARS;
	mNickEdit->SetText(StringToSexyString(theApp->mNetManager->mConfig.mNick));
	AddWidget(mNickEdit);

	mIpEdit = new EditWidget(101, this);
	mIpEdit->SetFont(Sexy::FONT_BRIANNETOD12);
	mIpEdit->SetColors(gPanelEditColors, EditWidget::NUM_COLORS);
	mIpEdit->mBlinkDelay = 14;
	mIpEdit->mMaxChars = 45;
	mIpEdit->SetText(_S(""));
	AddWidget(mIpEdit);

	mPortEdit = new EditWidget(102, this);
	mPortEdit->SetFont(Sexy::FONT_BRIANNETOD12);
	mPortEdit->SetColors(gPanelEditColors, EditWidget::NUM_COLORS);
	mPortEdit->mBlinkDelay = 14;
	mPortEdit->mMaxChars = 5;
	mPortEdit->SetText(_S("27713"));
	AddWidget(mPortEdit);

	Resize(0, 0, mApp->mWidth, mApp->mHeight);
}

NetLobbyPanel::~NetLobbyPanel()
{
	delete mNickEdit;
	delete mIpEdit;
	delete mPortEdit;
	delete mPaperImage;
}

Rect NetLobbyPanel::GetPanelRect() const
{
	return mCollapsed ? Rect(8, mApp->mHeight - 52, 180, 36) : Rect(8, mApp->mHeight - 318, 300, 306);
}

bool NetLobbyPanel::IsOnMainSelector() const
{
	const GameSelector* aSelector = mApp->mGameSelector;
	return mApp->mLoadingThreadCompleted && !mApp->mBoard && aSelector &&
		aSelector->mSelectorState == SELECTOR_IDLE && aSelector->mSlideCounter == 0 &&
		aSelector->mX == 0 && aSelector->mY == 0 && aSelector->mDestX == 0 && aSelector->mDestY == 0;
}

void NetLobbyPanel::LayoutButtons()
{
	if (!mApp->mLoadingThreadCompleted)
		return;
	mButtons.clear();
	Rect aPanel = GetPanelRect();
	int aX = aPanel.mX + 18;

	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;
	if (mCollapsed)
	{
		mButtons.push_back({BTN_TOGGLE, aPanel, _S("PLAY TOGETHER +"), false,
			mHoverButtonId == BTN_TOGGLE && aPanel.Contains(mLastMouseX, mLastMouseY)});
		return;
	}
	mButtons.push_back({BTN_TOGGLE, Rect(aPanel.mX + 254, aPanel.mY + 7, 28, 26), _S("-"), false, false});

	if (!aNet->IsActive())
	{
		mButtons.push_back({BTN_ONLINE, Rect(aX, aPanel.mY + 94, 264, 42), _S("ONLINE PLAY"), false, false});
		mButtons.push_back({BTN_LAN, Rect(aX, aPanel.mY + 146, 264, 30),
			mLanExpanded ? _S("LOCAL NETWORK  -") : _S("LOCAL NETWORK  +"), false, false});
		if (mLanExpanded)
		{
			mButtons.push_back({BTN_HOST, Rect(aX, aPanel.mY + 224, 124, 34), _S("HOST LAN"), false, false});
			mButtons.push_back({BTN_CONNECT, Rect(aX + 140, aPanel.mY + 224, 124, 34), _S("JOIN LAN"), false, false});
		}
	}
	else if (aNet->IsConnecting())
	{
		UiButton aCancel = { BTN_DISCONNECT, Rect(aX, aPanel.mY + 262, 264, 34), _S("CANCEL"), false, false };
		mButtons.push_back(aCancel);
	}
	else
	{
		UiButton aLeave = { BTN_DISCONNECT, Rect(aX, aPanel.mY + 262, 264, 34), _S("LEAVE GAME"), false, false };
		mButtons.push_back(aLeave);
		mButtons.push_back({BTN_DIFFICULTY_DOWN, Rect(aPanel.mX + 202, aPanel.mY + 205, 34, 30), _S("-"), !aNet->IsHosting() || aNet->GetDifficulty() <= 1, false});
		mButtons.push_back({BTN_DIFFICULTY_UP, Rect(aPanel.mX + 248, aPanel.mY + 205, 34, 30), _S("+"), !aNet->IsHosting() || aNet->GetDifficulty() >= 10, false});
		if (aNet->IsHosting()) for (int slot = 1; slot < NET_MAX_PLAYERS; ++slot)
			if (aNet->IsPlayerActive(slot))
				mButtons.push_back({BTN_KICK_BASE + slot, Rect(aPanel.mX + 252, aPanel.mY + 65 + slot*32, 30, 26), _S("X"), false, false});
	}

	// restore hover state (LayoutButtons runs every frame and would otherwise
	// wipe the highlight set by MouseMove before Update)
	for (size_t i = 0; i < mButtons.size(); i++)
		mButtons[i].mMouseOver =
			(mHoverButtonId == mButtons[i].mId) &&
			mButtons[i].mRect.Contains(mLastMouseX, mLastMouseY);
}

void NetLobbyPanel::AddedToManager(WidgetManager* theWidgetManager)
{
	Widget::AddedToManager(theWidgetManager);
	AddWidget(mNickEdit);
	AddWidget(mIpEdit);
	AddWidget(mPortEdit);
	MarkDirty();
}

void NetLobbyPanel::RemovedFromManager(WidgetManager* theWidgetManager)
{
	RemoveWidget(mNickEdit);
	RemoveWidget(mIpEdit);
	RemoveWidget(mPortEdit);
	Widget::RemovedFromManager(theWidgetManager);
}

void NetLobbyPanel::Update()
{
	// don't touch fonts/UI until the loading thread has finished
	if (!mApp->mLoadingThreadCompleted)
		return;

	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;

	// Visible only in the main menu (after the title screen).  LawnApp hides
	// this full-screen widget before creating a board and restores it on return
	// to the selector; never move it to a huge negative coordinate as a proxy
	// for visibility because it remains in the draw/clip chain.
	bool aInMenu = IsOnMainSelector();
	const bool aActive = aNet->IsActive();
	if (aActive != mWasActive) mCollapsed = !aActive;
	mWasActive = aActive;
	if (aInMenu)
		Resize(0, 0, mApp->mWidth, mApp->mHeight);

	// the connection form's edit widgets are hidden once connected/connecting
	bool aShowEdits = aInMenu && !aNet->IsActive() && !mCollapsed;
	mNickEdit->mVisible = aShowEdits;
	mIpEdit->mVisible = aShowEdits && mLanExpanded;
	mPortEdit->mVisible = aShowEdits && mLanExpanded;
	const Rect aPanel = GetPanelRect();
	mNickEdit->Resize(aPanel.mX + 82, aPanel.mY + 48, 200, 28);
	mIpEdit->Resize(aPanel.mX + 70, aPanel.mY + 184, 142, 26);
	mPortEdit->Resize(aPanel.mX + 220, aPanel.mY + 184, 62, 26);

	LayoutButtons();
	MarkDirty();
	Widget::Update();
}

// Only the panel area itself intercepts the mouse - clicks anywhere else fall
// through to the widgets below (title screen, game selector, ...).
bool NetLobbyPanel::IsPointVisible(int theX, int theY)
{
	// theX/theY are relative to the widget origin: when the panel is parked
	// off-screen (-5000) its relative rect would still match screen points and
	// swallow clicks, so require the widget to actually sit at the origin
	if (mX != 0 || mY != 0)
		return false;
	if (!IsOnMainSelector())
		return false;
	return GetPanelRect().Contains(theX, theY);
}

void NetLobbyPanel::DrawButton(Graphics* g, const UiButton& theButton)
{
    NetUi::StoneButton(g, theButton.mRect, theButton.mLabel,
        mPressedButtonId == theButton.mId && theButton.mMouseOver,
        theButton.mMouseOver, theButton.mDisabled);
}

void NetLobbyPanel::Draw(Graphics* g)
{
    NetManager* aNet = mApp->mNetManager;
    if (!aNet || !IsOnMainSelector()) return;
    if (mCollapsed)
    {
        for (const UiButton& aButton : mButtons) DrawButton(g, aButton);
        Widget::Draw(g);
        return;
    }
    const Rect aPanel = GetPanelRect();
    NetUi::Paper(g, mPaperImage, aPanel);
    TodDrawString(g, _S("PLAY TOGETHER"), aPanel.mX + 150, aPanel.mY + 28,
        FONT_BRIANNETOD16, NetUi::Ink(), DS_ALIGN_CENTER);
    if (!aNet->IsActive())
    {
        TodDrawString(g, _S("NAME"), aPanel.mX + 18, aPanel.mY + 68,
            FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT);
        if (mLanExpanded)
            TodDrawString(g, _S("LAN IP"), aPanel.mX + 18, aPanel.mY + 203,
                FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT);
        else
        {
            TodDrawString(g, _S("A lawn is better with friends."), aPanel.mX + 150, aPanel.mY + 211,
                FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_CENTER);
            TodDrawString(g, _S("Find a room and join the fun!"), aPanel.mX + 150, aPanel.mY + 237,
                FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_CENTER);
        }
        NetUi::Literal(g, FONT_BRIANNETOD12,
            NetUi::Fit(FONT_BRIANNETOD12, StringToSexyString(aNet->GetConnectStatus()), 264),
            aPanel.mX + 18, aPanel.mY + 288, Color(160, 48, 32));
    }
    else if (aNet->IsConnecting())
    {
        TodDrawString(g, _S("JOINING YOUR FRIEND..."), aPanel.mX + 150, aPanel.mY + 126,
            FONT_BRIANNETOD16, NetUi::Ink(), DS_ALIGN_CENTER);
        NetUi::Literal(g, FONT_BRIANNETOD12,
            NetUi::Fit(FONT_BRIANNETOD12, StringToSexyString(aNet->GetConnectStatus()), 264),
            aPanel.mX + 150, aPanel.mY + 157, NetUi::MutedInk(), 0);
    }
    else
    {
        TodDrawString(g, aNet->IsHosting() ? StrFormat(_S("YOUR ROOM  %d / 4"), aNet->GetPlayerCount()) : _S("CONNECTED TO A FRIEND"),
            aPanel.mX + 150, aPanel.mY + 50, FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_CENTER);
        for (int i = 0; i < NET_MAX_PLAYERS; ++i)
        {
            const NetPlayerInfo& aPlayer = aNet->GetPlayer(i);
            const int aY = aPanel.mY + 65 + i * 32;
            g->SetColor(Color(145, 105, 58, 90));
            g->FillRect(aPanel.mX + 18, aY + 31, 264, 1);
            if (!aPlayer.mActive)
            {
                TodDrawString(g, _S("Open slot"), aPanel.mX + 38, aY + 20,
                    FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_LEFT);
                continue;
            }
            const NetColor aColor = GetRelativePlayerColor(aNet->GetMySlot(), i);
            g->SetColor(Color(aColor.r, aColor.g, aColor.b));
            g->FillRect(aPanel.mX + 18, aY + 4, 12, 12);
            NetUi::Literal(g, FONT_BRIANNETOD12,
                NetUi::Fit(FONT_BRIANNETOD12, StringToSexyString(aPlayer.mNick), 158),
                aPanel.mX + 38, aY + 15, NetUi::Ink());
            const SexyString aRole = aPlayer.mSpectator ? _S("WATCHING") : i == 0 ? _S("HOST") : _S("PLAYER");
            TodDrawString(g, aRole + (i == aNet->GetMySlot() ? _S(" / YOU") : _S("")),
                aPanel.mX + 38, aY + 29, FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_LEFT);
            TodDrawString(g, StrFormat(_S("%ums"), aPlayer.mPing), aPanel.mX + 244, aY + 17,
                FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_RIGHT);
        }
        TodDrawString(g, StrFormat(_S("DIFFICULTY  %d / 10"), aNet->GetDifficulty()),
            aPanel.mX + 18, aPanel.mY + 225, FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT);
        TodDrawString(g, aNet->IsHosting() ? _S("Choose a game to start.") : _S("The host chooses the game."),
            aPanel.mX + 150, aPanel.mY + 252, FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_CENTER);
    }
    for (const UiButton& aButton : mButtons) DrawButton(g, aButton);
    MarkDirty();
    Widget::Draw(g);
}

void NetLobbyPanel::MouseMove(int x, int y)
{
	mLastMouseX = x;
	mLastMouseY = y;
	mHoverButtonId = 0;
	for (size_t i = 0; i < mButtons.size(); i++)
	{
		mButtons[i].mMouseOver = mButtons[i].mRect.Contains(x, y);
		if (mButtons[i].mMouseOver)
			mHoverButtonId = mButtons[i].mId;
	}
	MarkDirty();
	Widget::MouseMove(x, y);
}

void NetLobbyPanel::MouseLeave()
{
	mLastMouseX = -1;
	mLastMouseY = -1;
	mHoverButtonId = 0;
	for (size_t i = 0; i < mButtons.size(); i++)
		mButtons[i].mMouseOver = false;
	Widget::MouseLeave();
}

void NetLobbyPanel::MouseDown(int x, int y, int theClickCount)
{
	mPressedButtonId = 0;
	for (size_t i = 0; i < mButtons.size(); i++)
	{
		if (mButtons[i].mDisabled || !mButtons[i].mRect.Contains(x, y))
			continue;
		mPressedButtonId = mButtons[i].mId;
		mApp->PlaySample(Sexy::SOUND_TAP2);
		MarkDirty();
		return;
	}
}

void NetLobbyPanel::MouseUp(int x, int y, int theClickCount)
{
	int aPressedButtonId = mPressedButtonId;
	mPressedButtonId = 0;
	for (size_t i = 0; i < mButtons.size(); i++)
	{
		if (mButtons[i].mId == aPressedButtonId && !mButtons[i].mDisabled && mButtons[i].mRect.Contains(x, y))
		{
			ActivateButton(aPressedButtonId);
			break;
		}
	}
	MarkDirty();
}

void NetLobbyPanel::ActivateButton(int theButtonId)
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;

	if (theButtonId > BTN_KICK_BASE && theButtonId < BTN_KICK_BASE + NET_MAX_PLAYERS)
	{ aNet->KickPlayer(theButtonId - BTN_KICK_BASE); return; }
	switch (theButtonId)
	{
		case BTN_HOST:
		{
			ApplyNick();
			mApp->NetRestoreLocalProfile();
			uint16_t aPort = (uint16_t)atoi(SexyStringToStringFast(mPortEdit->mString).c_str());
			if (aPort == 0)
				aPort = NET_DEFAULT_PORT;
			if (aNet->StartHost(aPort))
			{
				NetSaveConfig(aNet->mConfig);
				mApp->NetBroadcastProfile();
			}
			break;
		}
		case BTN_CONNECT:
		{
			ApplyNick();
			mApp->NetRestoreLocalProfile();
			uint16_t aPort = (uint16_t)atoi(SexyStringToStringFast(mPortEdit->mString).c_str());
			if (aPort == 0)
				aPort = NET_DEFAULT_PORT;
			std::string anIP = SexyStringToStringFast(mIpEdit->mString);
			aNet->StartClient(anIP.c_str(), aPort);
			break;
		}
		case BTN_DISCONNECT:
			aNet->Disconnect();
			mApp->NetRestoreLocalProfile();
			break;
		case BTN_ONLINE:
			ApplyNick(); mApp->NetShowOnlineBrowser(); break;
		case BTN_LAN:
			mLanExpanded = !mLanExpanded; LayoutButtons(); break;
		case BTN_TOGGLE:
			mCollapsed = !mCollapsed; LayoutButtons(); break;
		case BTN_DIFFICULTY_DOWN:
			aNet->SetDifficulty(aNet->GetDifficulty() - 1); break;
		case BTN_DIFFICULTY_UP:
			aNet->SetDifficulty(aNet->GetDifficulty() + 1); break;
		case BTN_TEAM_PLANTS:
			aNet->SetMyTeam(NETTEAM_PLANTS);
			break;
		case BTN_TEAM_ZOMBIES:
			aNet->SetMyTeam(NETTEAM_ZOMBIES);
			break;
	}
}

void NetLobbyPanel::ApplyNick()
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;
	std::string aNick;
	if (!NetNormalizeUtf8(SexyStringToStringFast(mNickEdit->mString), NET_MAX_NICK_BYTES,
		NET_MAX_NICK_CHARS, true, aNick))
		return;
	if (aNick != aNet->mConfig.mNick)
	{
		strcpy_s(aNet->mConfig.mNick, aNick.c_str());
		NetSaveConfig(aNet->mConfig);
	}
	if (aNet->IsHosting() && aNet->GetPlayerCount() == 1)
	{
		strcpy_s(aNet->GetPlayerMutable(0).mNick, aNet->mConfig.mNick);
		aNet->SendLobbyState();
	}
}
