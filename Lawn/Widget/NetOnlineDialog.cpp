#include "NetOnlineDialog.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../Net/NetManager.h"
#include "../../SexyAppFramework/EditWidget.h"
#include "../../SexyAppFramework/ListWidget.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/Font.h"
#include "../../Sexy.TodLib/TodStringFile.h"

using namespace LawnNet;

namespace
{
SexyString FitOnlineText(const SexyString& theText, int theWidth)
{
    if (FONT_BRIANNETOD12->StringWidth(theText) <= theWidth)
        return theText;
    SexyString aText = theText;
    while (!aText.empty() && FONT_BRIANNETOD12->StringWidth(aText + _S("...")) > theWidth)
        aText.pop_back();
    return aText + _S("...");
}
}

NetOnlineDialog::NetOnlineDialog(LawnApp* theApp) :
    LawnDialog(theApp, Dialogs::DIALOG_COOP_ONLINE, true, _S("ONLINE ROOMS"),
        _S(""), _S("CLOSE"), Dialog::BUTTONS_FOOTER),
    mPage(0), mMaxPlayers(4), mLastActionTick(0), mAttemptedRoom(false)
{
    mEndpoint = new EditWidget(201, this);
    mPassword = new EditWidget(202, this);
    mRoomName = new EditWidget(203, this);
    for (EditWidget* anEdit : { mEndpoint, mPassword, mRoomName })
    {
        anEdit->SetFont(FONT_BRIANNETOD12);
        anEdit->SetColor(EditWidget::COLOR_BKG, Color(248, 243, 219));
        anEdit->SetColor(EditWidget::COLOR_OUTLINE, Color(95, 80, 50));
        anEdit->SetColor(EditWidget::COLOR_TEXT, Color(35, 30, 20));
        anEdit->mBlinkDelay = 14;
    }
    mEndpoint->mMaxChars = 255;
    mPassword->mMaxChars = 64;
    mPassword->mPasswordChar = '*';
    mRoomName->mMaxChars = 32;
    mEndpoint->SetText(StringToSexyString(theApp->mNetManager->mConfig.mOnlineEndpoint));
    mRoomName->SetText(StringToSexyString(theApp->mNetManager->mConfig.mNick) + _S("'s room"));

    mRoomList = new ListWidget(204, FONT_BRIANNETOD12, this);
    static int aListColors[][3] = {
        { 248, 243, 219 }, { 95, 80, 50 }, { 35, 30, 20 },
        { 225, 218, 180 }, { 85, 110, 60 }, { 255, 255, 235 }
    };
    mRoomList->SetColors(aListColors, 6);
    mRoomList->mItemHeight = 24;
    mRoomList->mDrawSelectWhenHilited = true;

    mConnect = MakeButton(CONNECT, this, _S("CONNECT"));
    mJoin = MakeButton(JOIN, this, _S("JOIN ROOM"));
    mCreate = MakeButton(CREATE, this, _S("CREATE"));
    mCapacity = MakeButton(CAPACITY, this, _S("4 PLAYERS"));
    mPrevious = MakeButton(PREV, this, _S("PREV"));
    mNext = MakeButton(NEXT, this, _S("NEXT"));

    Resize(0, 0, 760, 560);
    LawnApp::CenterDialog(this, mWidth, mHeight);
    mEndpoint->Resize(165, 100, 410, 28);
    mConnect->Resize(590, 96, 130, 34);
    mRoomList->Resize(40, 177, 680, 152);
    mPrevious->Resize(40, 335, 90, 30);
    mNext->Resize(135, 335, 90, 30);
    mPassword->Resize(170, 378, 250, 28);
    mJoin->Resize(480, 374, 240, 34);
    mRoomName->Resize(170, 423, 250, 28);
    mCapacity->Resize(435, 419, 130, 34);
    mCreate->Resize(575, 419, 145, 34);

    if (NetOnline* anOnline = theApp->mNetManager->GetOnline(); anOnline && anOnline->IsConnected())
    {
        mConnectedEndpoint = theApp->mNetManager->mConfig.mOnlineEndpoint;
        anOnline->RefreshRooms();
    }
    else if (!theApp->mNetManager->mConfig.mOnlineEndpoint.empty() && !theApp->mNetManager->IsActive())
        ButtonDepress(CONNECT);
}

NetOnlineDialog::~NetOnlineDialog()
{
    delete mEndpoint;
    delete mPassword;
    delete mRoomName;
    delete mRoomList;
    delete mConnect;
    delete mJoin;
    delete mCreate;
    delete mCapacity;
    delete mPrevious;
    delete mNext;
}

void NetOnlineDialog::AddedToManager(WidgetManager* theManager)
{
    LawnDialog::AddedToManager(theManager);
    Widget* aWidgets[] = { mEndpoint, mPassword, mRoomName, mRoomList,
        mConnect, mJoin, mCreate, mCapacity, mPrevious, mNext };
    for (Widget* aWidget : aWidgets)
        AddWidget(aWidget);
}

void NetOnlineDialog::RemovedFromManager(WidgetManager* theManager)
{
    Widget* aWidgets[] = { mEndpoint, mPassword, mRoomName, mRoomList,
        mConnect, mJoin, mCreate, mCapacity, mPrevious, mNext };
    for (Widget* aWidget : aWidgets)
        RemoveWidget(aWidget);
    LawnDialog::RemovedFromManager(theManager);
}

void NetOnlineDialog::RebuildRoomList()
{
    const int aPageCount = max(1, ((int)mRooms.size() + 5) / 6);
    mPage = max(0, min(mPage, aPageCount - 1));
    mRoomList->RemoveAll();
    int aSelection = -1;
    for (int i = mPage * 6; i < (int)mRooms.size() && i < mPage * 6 + 6; i++)
    {
        const NetOnlineRoom& aRoom = mRooms[i];
        SexyString aName = FitOnlineText(StringToSexyString(aRoom.mName), 330);
        SexyString aLine = aName + StrFormat(_S("   %d/%d   "), aRoom.mPlayers, aRoom.mMaxPlayers) +
            (aRoom.mHasPassword ? _S("PASSWORD") : _S("OPEN")) +
            (aRoom.mPlaying ? _S("   WATCH") : _S("   LOBBY"));
        const int anIndex = mRoomList->AddLine(aLine, false);
        if (aRoom.mId == mSelectedRoom)
            aSelection = anIndex;
    }
    if (aSelection < 0 && mRoomList->GetLineCount() > 0)
        aSelection = 0;
    mRoomList->SetSelect(aSelection);
    const int aGlobalIndex = mPage * 6 + aSelection;
    mSelectedRoom = aSelection >= 0 && aGlobalIndex < (int)mRooms.size() ? mRooms[aGlobalIndex].mId : "";
    mPrevious->SetDisabled(mPage <= 0);
    mNext->SetDisabled(mPage >= aPageCount - 1);
}

void NetOnlineDialog::SyncRooms()
{
    NetOnline* anOnline = mApp->mNetManager->GetOnline();
    const std::vector<NetOnlineRoom> aRooms = anOnline ? anOnline->GetRooms() : std::vector<NetOnlineRoom>();
    std::string aStamp;
    for (const NetOnlineRoom& aRoom : aRooms)
        aStamp += aRoom.mId + '\n' + aRoom.mName + '\n' + std::to_string(aRoom.mPlayers) + '/' +
            std::to_string(aRoom.mMaxPlayers) + (aRoom.mHasPassword ? "P" : "O") +
            (aRoom.mPlaying ? "G" : "L");
    if (aStamp == mRoomsStamp && !mRoomsStamp.empty())
        return;
    mRoomsStamp = aStamp;
    mRooms = aRooms;
    for (int i = 0; i < (int)mRooms.size(); i++)
        if (mRooms[i].mId == mSelectedRoom)
            mPage = i / 6;
    RebuildRoomList();
}

void NetOnlineDialog::Update()
{
    LawnDialog::Update();
    NetManager* aNet = mApp->mNetManager;
    NetOnline* anOnline = aNet->GetOnline();
    if (mAttemptedRoom && ((anOnline && anOnline->IsHostingRoom() && !anOnline->GetRoomId().empty()) ||
        aNet->IsConnectedToHost()))
    {
        mApp->KillDialog(Dialogs::DIALOG_COOP_ONLINE);
        return;
    }
    SyncRooms();
    const bool aCanAct = anOnline && anOnline->IsConnected() && !aNet->IsActive();
    mConnect->SetLabel(anOnline && anOnline->IsConnected() ? _S("REFRESH") : _S("CONNECT"));
    mConnect->SetDisabled(aNet->IsActive());
    mCreate->SetDisabled(!aCanAct);
    mJoin->SetDisabled(!aCanAct || mSelectedRoom.empty());
    mCapacity->SetLabel(StrFormat(_S("%d PLAYERS"), mMaxPlayers));
    MarkDirty();
}

void NetOnlineDialog::Draw(Graphics* g)
{
    LawnDialog::Draw(g);
    const Color aText(80, 75, 95);
    TodDrawString(g, _S("DIRECTORY URL"), 40, 121, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    TodDrawString(g, _S("Rooms for your game version"), 40, 168, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    TodDrawString(g, _S("PASSWORD"), 40, 399, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    TodDrawString(g, _S("ROOM NAME"), 40, 444, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    TodDrawString(g, StrFormat(_S("Page %d/%d"), mPage + 1, max(1, ((int)mRooms.size() + 5) / 6)),
        255, 356, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    NetOnline* anOnline = mApp->mNetManager->GetOnline();
    const SexyString aStatus = anOnline ? StringToSexyString(anOnline->GetStatus()) :
        _S("Enter your directory URL and connect.");
    TodDrawString(g, FitOnlineText(aStatus, 680), 40, 151, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    if (mRooms.empty())
        TodDrawString(g, _S("No rooms yet. Refresh or create one below."), 380, 250,
            FONT_BRIANNETOD12, aText, DS_ALIGN_CENTER);
    if (!mError.empty())
        TodDrawString(g, FitOnlineText(mError, 680), 40, 480,
            FONT_BRIANNETOD12, Color(170, 45, 35), DS_ALIGN_LEFT);
}

void NetOnlineDialog::CloseBrowser()
{
    NetManager* aNet = mApp->mNetManager;
    if (mAttemptedRoom && !aNet->IsActive())
        if (NetOnline* anOnline = aNet->GetOnline(); anOnline && !anOnline->IsHostingRoom())
            anOnline->LeaveRoom();
    mApp->KillDialog(Dialogs::DIALOG_COOP_ONLINE);
}

void NetOnlineDialog::ButtonDepress(int theId)
{
    if (theId == 1000)
    {
        CloseBrowser();
        return;
    }
    if (theId == CAPACITY)
    {
        mMaxPlayers = mMaxPlayers == 4 ? 2 : mMaxPlayers + 1;
        return;
    }
    if (theId == PREV || theId == NEXT)
    {
        mPage += theId == PREV ? -1 : 1;
        mSelectedRoom.clear();
        RebuildRoomList();
        return;
    }
    const unsigned int aNow = GetTickCount();
    if (mLastActionTick && aNow - mLastActionTick < 500U)
        return;
    mLastActionTick = aNow;
    mError.clear();
    NetManager* aNet = mApp->mNetManager;
    NetOnline* anOnline = aNet->GetOnline();
    if (theId == CONNECT)
    {
        const std::string anEndpoint = SexyStringToStringFast(mEndpoint->mString);
        if (anEndpoint.empty())
        {
            mError = _S("Enter the directory URL from your setup guide.");
            return;
        }
        if (anOnline && anOnline->IsConnected() && anEndpoint == mConnectedEndpoint)
            anOnline->RefreshRooms();
        else if (aNet->ConnectOnlineDirectory(anEndpoint))
        {
            mConnectedEndpoint = anEndpoint;
            aNet->mConfig.mOnlineEndpoint = anEndpoint;
            NetSaveConfig(aNet->mConfig);
        }
        else
            mError = _S("Could not connect. Check the directory URL and connection status.");
        return;
    }
    if (!anOnline || !anOnline->IsConnected())
    {
        mError = _S("Connect to the room directory first.");
        return;
    }
    const std::string aPassword = SexyStringToStringFast(mPassword->mString);
    if (aPassword.size() > 64)
    {
        mError = _S("Password must fit in 64 UTF-8 bytes.");
        return;
    }
    if (theId == CREATE)
    {
        std::string aName;
        if (!NetNormalizeUtf8(SexyStringToStringFast(mRoomName->mString), 64, 32, true, aName) || aName.empty())
        {
            mError = _S("Enter a room name (up to 32 characters).");
            return;
        }
        mAttemptedRoom = aNet->CreateOnlineRoom(aName, aPassword, mMaxPlayers);
        if (!mAttemptedRoom)
            mError = _S("Could not create the room. Check the connection status.");
    }
    else if (theId == JOIN && !mSelectedRoom.empty())
    {
        mAttemptedRoom = aNet->JoinOnlineRoom(mSelectedRoom, aPassword);
        if (!mAttemptedRoom)
            mError = _S("Could not join the room. Check password, capacity and status.");
    }
}

void NetOnlineDialog::ListClicked(int, int theIndex, int theClickCount)
{
    const int aGlobalIndex = mPage * 6 + theIndex;
    if (theIndex < 0 || aGlobalIndex >= (int)mRooms.size())
        return;
    mRoomList->SetSelect(theIndex);
    mSelectedRoom = mRooms[aGlobalIndex].mId;
    if (theClickCount == 2)
        ButtonDepress(JOIN);
}

void NetOnlineDialog::EditWidgetText(int theId, const SexyString&)
{
    if (theId == 201)
        ButtonDepress(CONNECT);
    else if (theId == 202)
        ButtonDepress(JOIN);
    else if (theId == 203)
        ButtonDepress(CREATE);
}

void NetOnlineDialog::KeyDown(KeyCode theKey)
{
    if (theKey == KEYCODE_ESCAPE)
        CloseBrowser();
    else if (theKey == KEYCODE_RETURN)
        ButtonDepress(JOIN);
}
