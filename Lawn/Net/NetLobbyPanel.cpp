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
	mLastMouseX(-1),
	mLastMouseY(-1),
	mLastCursorSend(0)
{
	mNickEdit = new EditWidget(100, this);
	mNickEdit->SetFont(Sexy::FONT_BRIANNETOD12);
	mNickEdit->SetColors(gPanelEditColors, EditWidget::NUM_COLORS);
	mNickEdit->mBlinkDelay = 14;
	mNickEdit->mMaxChars = NET_MAX_NICK;
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
}

Rect NetLobbyPanel::GetPanelRect() const
{
	return Rect(8, 265, 290, 320);
}

void NetLobbyPanel::LayoutButtons()
{
	if (!mApp->mLoadingThreadCompleted)
		return;
	mButtons.clear();
	Rect aPanel = GetPanelRect();
	int aX = aPanel.mX + 10;
	int aY = aPanel.mY + 246;

	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;

	if (!aNet->IsActive())
	{
		UiButton aHost = { BTN_HOST, Rect(aX, aY, 128, 30), _S("HOST GAME"), false, false };
		UiButton aJoin = { BTN_CONNECT, Rect(aX + 142, aY, 128, 30), _S("CONNECT"), false, false };
		mButtons.push_back(aHost);
		mButtons.push_back(aJoin);
	}
	else if (aNet->IsConnecting())
	{
		UiButton aCancel = { BTN_DISCONNECT, Rect(aX + 70, aY + 40, 128, 30), _S("CANCEL"), false, false };
		mButtons.push_back(aCancel);
	}
	else
	{
		UiButton aPlants = { BTN_TEAM_PLANTS, Rect(aX, aY, 128, 28), _S("TEAM PLANTS"), false, false };
		UiButton aZombies = { BTN_TEAM_ZOMBIES, Rect(aX + 142, aY, 128, 28), _S("TEAM ZOMBIES"), false, false };
		UiButton aLeave = { BTN_DISCONNECT, Rect(aX, aY + 36, 128, 26), _S("DISCONNECT"), false, false };
		mButtons.push_back(aPlants);
		mButtons.push_back(aZombies);
		mButtons.push_back(aLeave);
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

	// visible only in the main menu (after the title screen)
	bool aInMenu = mApp->mBoard == nullptr && mApp->mGameSelector != nullptr;
	Resize(aInMenu ? 0 : -5000, aInMenu ? 0 : -5000, mApp->mWidth, mApp->mHeight);

	// the connection form's edit widgets are hidden once connected/connecting
	bool aShowEdits = aInMenu && !aNet->IsActive();
	mNickEdit->mVisible = aShowEdits;
	mIpEdit->mVisible = aShowEdits;
	mPortEdit->mVisible = aShowEdits;

	if (aInMenu && !aNet->IsActive())
	{
		// share our menu cursor position
		uint32_t aNow = GetTickCount();
		if (aNow - mLastCursorSend > 50)
		{
			mLastCursorSend = aNow;
			WidgetManager* aWM = mApp->mWidgetManager;
			aNet->SendCursor(CURSOR_POINTER, SEED_NONE, SEED_NONE, 0, aWM->mLastMouseX, aWM->mLastMouseY);
		}
	}

	LayoutButtons();
	MarkDirty();
	Widget::Update();
}

// Only the panel area itself intercepts the mouse - clicks anywhere else fall
// through to the widgets below (title screen, game selector, ...).
bool NetLobbyPanel::IsPointVisible(int theX, int theY)
{
	if (mApp->mBoard != nullptr || mApp->mGameSelector == nullptr || !mApp->mLoadingThreadCompleted)
		return false;
	return GetPanelRect().Contains(theX, theY);
}

void NetLobbyPanel::DrawButton(Graphics* g, const UiButton& theButton)
{
	Color aFill = theButton.mDisabled ? Color(120, 120, 120) :
		(theButton.mMouseOver ? Color(255, 235, 160) : Color(215, 180, 100));
	Color anOutline = theButton.mMouseOver ? Color(255, 250, 210) : Color(90, 60, 30);
	g->SetColor(aFill);
	g->FillRect(theButton.mRect.mX, theButton.mRect.mY, theButton.mRect.mWidth, theButton.mRect.mHeight);
	g->SetColor(anOutline);
	g->DrawRect(theButton.mRect.mX, theButton.mRect.mY, theButton.mRect.mWidth - 1, theButton.mRect.mHeight - 1);
	g->SetColor(theButton.mMouseOver ? Color(255, 255, 255, 90) : Color(255, 255, 255, 40));
	g->FillRect(theButton.mRect.mX + 2, theButton.mRect.mY + 2, theButton.mRect.mWidth - 4, 3);
	TodDrawString(g, theButton.mLabel,
		theButton.mRect.mX + theButton.mRect.mWidth / 2,
		theButton.mRect.mY + theButton.mRect.mHeight / 2 + 4,
		Sexy::FONT_BRIANNETOD12, Color(60, 35, 10), DrawStringJustification::DS_ALIGN_CENTER);
}

void NetLobbyPanel::Draw(Graphics* g)
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet || mApp->mBoard || mApp->mGameSelector == nullptr || !mApp->mLoadingThreadCompleted)
		return;

	// ---- remote cursors (menus) ----
	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		if (i == aNet->GetMySlot() || !aNet->mRemoteCursors[i].mValid)
			continue;
		const NetRemoteCursor& aCursor = aNet->mRemoteCursors[i];
		const NetPlayerInfo& aPlayer = aNet->GetPlayer(i);
		if (!aPlayer.mActive)
			continue;

		NetColor aColor = GetRelativePlayerColor(aNet->GetMySlot(), i);
		g->SetColor(Color(aColor.r, aColor.g, aColor.b, 255));
		g->FillRect(aCursor.mX, aCursor.mY, 9, 9);
		g->SetColor(Color(0, 0, 0, 200));
		g->FillRect(aCursor.mX + 12, aCursor.mY + 8, 2, 14);
		if (aPlayer.mNick[0])
		{
			SexyString aLabel = StringToSexyString(aPlayer.mNick) + _S(" ") + StrFormat(_S("%ums"), aPlayer.mPing);
			g->SetColor(Color(0, 0, 0, 180));
			TodDrawString(g, aLabel, aCursor.mX + 15, aCursor.mY + 21, Sexy::FONT_BRIANNETOD12, Color(0, 0, 0, 180), DrawStringJustification::DS_ALIGN_LEFT);
			TodDrawString(g, aLabel, aCursor.mX + 14, aCursor.mY + 20, Sexy::FONT_BRIANNETOD12, Color(aColor.r, aColor.g, aColor.b), DrawStringJustification::DS_ALIGN_LEFT);
		}
	}

	// ---- panel background ----
	Rect aPanel = GetPanelRect();
	g->SetColor(Color(233, 209, 155, 235));
	g->FillRect(aPanel.mX, aPanel.mY, aPanel.mWidth, aPanel.mHeight);
	g->SetColor(Color(110, 80, 40));
	g->DrawRect(aPanel.mX, aPanel.mY, aPanel.mWidth - 1, aPanel.mHeight - 1);
	TodDrawString(g, _S("PvZ CO-OP"), aPanel.mX + aPanel.mWidth / 2, aPanel.mY + 20,
		Sexy::FONT_BRIANNETOD16, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_CENTER);

	int aY = aPanel.mY + 40;

	if (!aNet->IsActive())
	{
		// ---- connection form ----
		TodDrawString(g, _S("Nickname:"), aPanel.mX + 10, aY + 10, Sexy::FONT_BRIANNETOD12, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_LEFT);
		mNickEdit->Resize(aPanel.mX + 80, aY, aPanel.mWidth - 95, 22);
		aY += 36;

		TodDrawString(g, _S("Host IP:"), aPanel.mX + 10, aY + 10, Sexy::FONT_BRIANNETOD12, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_LEFT);
		mIpEdit->Resize(aPanel.mX + 80, aY, aPanel.mWidth - 95, 22);
		aY += 36;

		TodDrawString(g, _S("Port:"), aPanel.mX + 10, aY + 10, Sexy::FONT_BRIANNETOD12, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_LEFT);
		mPortEdit->Resize(aPanel.mX + 80, aY, 100, 22);
		aY += 40;

		// last connection error (only when idle, not while retrying)
		if (!aNet->IsConnecting() && !aNet->IsHosting() && aNet->GetMySlot() == 0)
		{
			SexyString aStatus = StringToSexyString(aNet->GetConnectStatus());
			if (aStatus.length() > 0)
				TodDrawString(g, aStatus, aPanel.mX + 10, aY - 6,
					Sexy::FONT_BRIANNETOD12, Color(180, 60, 40), DrawStringJustification::DS_ALIGN_LEFT);
		}
	}
	else if (aNet->IsConnecting())
	{
		// ---- connecting overlay ----
		TodDrawString(g, _S("Connecting..."), aPanel.mX + aPanel.mWidth / 2, aPanel.mY + 130,
			Sexy::FONT_BRIANNETOD16, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_CENTER);
		TodDrawString(g, StringToSexyString(aNet->GetConnectStatus()), aPanel.mX + aPanel.mWidth / 2, aPanel.mY + 155,
			Sexy::FONT_BRIANNETOD12, Color(120, 80, 30), DrawStringJustification::DS_ALIGN_CENTER);
	}
	else
	{
		// ---- connected: player list ----
		SexyString aStatus;
		if (aNet->IsHosting())
			aStatus = StrFormat(_S("Hosting  (%d/4)"), aNet->GetPlayerCount());
		else
			aStatus = _S("Connected to host");
		TodDrawString(g, aStatus, aPanel.mX + 10, aY + 10, Sexy::FONT_BRIANNETOD12, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_LEFT);
		aY += 26;

		TodDrawString(g, _S("Players in lobby:"), aPanel.mX + 10, aY + 10, Sexy::FONT_BRIANNETOD12, Color(120, 85, 35), DrawStringJustification::DS_ALIGN_LEFT);
		aY += 24;

		for (int i = 0; i < NET_MAX_PLAYERS; i++)
		{
			const NetPlayerInfo& aP = aNet->GetPlayer(i);
			NetColor aColor = GetRelativePlayerColor(aNet->GetMySlot(), i);
			if (!aP.mActive)
			{
				g->SetColor(Color(150, 130, 95, 120));
				g->FillRect(aPanel.mX + 10, aY - 12, 12, 12);
				TodDrawString(g, _S("- empty slot -"), aPanel.mX + 30, aY, Sexy::FONT_BRIANNETOD12, Color(150, 125, 85), DrawStringJustification::DS_ALIGN_LEFT);
			}
			else
			{
				g->SetColor(Color(aColor.r, aColor.g, aColor.b));
				g->FillRect(aPanel.mX + 10, aY - 12, 12, 12);
				SexyString aRow = StringToSexyString(aP.mNick);
				if (i == 0)
					aRow += _S(" [HOST]");
				if (i == aNet->GetMySlot())
					aRow += _S(" (you)");
				SexyString aPing = StrFormat(_S("%ums"), aP.mPing);
				TodDrawString(g, aRow, aPanel.mX + 30, aY, Sexy::FONT_BRIANNETOD12, Color(60, 35, 10), DrawStringJustification::DS_ALIGN_LEFT);
				TodDrawString(g, aPing, aPanel.mX + aPanel.mWidth - 14, aY, Sexy::FONT_BRIANNETOD12, Color(90, 55, 20), DrawStringJustification::DS_ALIGN_RIGHT);
				if (aP.mTeam == NETTEAM_PLANTS)
					TodDrawString(g, _S("P"), aPanel.mX + 260, aY, Sexy::FONT_BRIANNETOD12, Color(40, 140, 40), DrawStringJustification::DS_ALIGN_LEFT);
				else if (aP.mTeam == NETTEAM_ZOMBIES)
					TodDrawString(g, _S("Z"), aPanel.mX + 260, aY, Sexy::FONT_BRIANNETOD12, Color(150, 40, 40), DrawStringJustification::DS_ALIGN_LEFT);
			}
			aY += 20;
		}

		aY += 6;
		TodDrawString(g, _S("PvP team (for PvP mode):"), aPanel.mX + 10, aY + 8, Sexy::FONT_BRIANNETOD12, Color(120, 85, 35), DrawStringJustification::DS_ALIGN_LEFT);
	}

	// ---- buttons ----
	for (size_t i = 0; i < mButtons.size(); i++)
		DrawButton(g, mButtons[i]);

	MarkDirty();
	Widget::Draw(g);
}

void NetLobbyPanel::MouseMove(int x, int y)
{
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
	for (size_t i = 0; i < mButtons.size(); i++)
		mButtons[i].mMouseOver = false;
	Widget::MouseLeave();
}

void NetLobbyPanel::MouseDown(int x, int y, int theClickCount)
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;

	for (size_t i = 0; i < mButtons.size(); i++)
	{
		if (!mButtons[i].mRect.Contains(x, y))
			continue;

		mApp->PlaySample(Sexy::SOUND_TAP2);

		switch (mButtons[i].mId)
		{
		case BTN_HOST:
		{
			ApplyNick();
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
			uint16_t aPort = (uint16_t)atoi(SexyStringToStringFast(mPortEdit->mString).c_str());
			if (aPort == 0)
				aPort = NET_DEFAULT_PORT;
			std::string anIP = SexyStringToStringFast(mIpEdit->mString);
			aNet->StartClient(anIP.c_str(), aPort);
			break;
		}
		case BTN_DISCONNECT:
			aNet->Disconnect();
			break;
		case BTN_TEAM_PLANTS:
			aNet->SetMyTeam(NETTEAM_PLANTS);
			break;
		case BTN_TEAM_ZOMBIES:
			aNet->SetMyTeam(NETTEAM_ZOMBIES);
			break;
		}
		MarkDirty();
		return;
	}
}

void NetLobbyPanel::ApplyNick()
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;
	std::string aNick = SexyStringToStringFast(mNickEdit->mString);
	if (aNick.empty())
		return;
	strncpy_s(aNet->mConfig.mNick, aNick.c_str(), NET_MAX_NICK);
	aNet->mConfig.mNick[NET_MAX_NICK] = 0;
	if (aNet->IsHosting() && aNet->GetPlayerCount() == 1)
	{
		strncpy_s(aNet->GetPlayerMutable(0).mNick, aNet->mConfig.mNick, NET_MAX_NICK);
		aNet->SendLobbyState();
	}
}
