#include "NetOnlineDialog.h"
#include "GameButton.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../Net/NetManager.h"
#include "../../SexyAppFramework/EditWidget.h"
#include "../../SexyAppFramework/ListWidget.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/Font.h"
#include "../../SexyAppFramework/DDImage.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "NetUi.h"

using namespace LawnNet;

namespace
{
SexyString FitOnlineText(const SexyString& theText, int theWidth)
{ return NetUi::Fit(FONT_BRIANNETOD12, theText, theWidth); }

SexyString OnlineFailure(NetManager* theNet, const SexyString& theFallback)
{
    NetOnline* anOnline = theNet->GetOnline();
    const std::string aDetail = anOnline ? anOnline->GetLastErrorMessage() : std::string();
    return aDetail.empty() ? theFallback : StringToSexyString(aDetail);
}

SexyString RoomError(const std::string& theCode)
{
    if (theCode == "wrong-password") return _S("That password is incorrect. Please try again.");
    if (theCode == "room-full") return _S("This room is full. Choose another room or refresh.");
    if (theCode == "room-not-found") return _S("This room has closed. Refresh to find another.");
    if (theCode == "version-mismatch") return _S("This room uses a different mod version.");
    if (theCode == "rate-limit" || theCode == "password-retry-later") return _S("Please wait a moment before trying again.");
    if (theCode == "directory-full") return _S("Online rooms are busy. Please try again shortly.");
    if (theCode == "direct-connection-failed") return _S("Could not reach this room. Try again or another network.");
    return _S("The room could not be opened. Please try again.");
}
}

class NetOnlineDialog::RoomList : public ListWidget
{
    NetOnlineDialog* mOwner;
public:
    explicit RoomList(NetOnlineDialog* theOwner) : ListWidget(204, FONT_BRIANNETOD12, theOwner), mOwner(theOwner) {}
    void Draw(Graphics* g) override
    {
        for (int i = 0; i < GetLineCount(); ++i)
        {
            const NetOnlineRoom& aRoom = mOwner->mRooms[mOwner->mPage * mOwner->mRowsPerPage + i];
            const Rect aRect(0, i * mItemHeight, mWidth, mItemHeight - 4);
            NetUi::Paper(g, mOwner->mPaperImage, aRect);
            if (i == mSelectIdx || i == mHiliteIdx)
            {
                g->SetColor(i == mSelectIdx ? Color(80, 125, 42) : Color(183, 124, 40));
                g->DrawRect(2, aRect.mY + 2, mWidth - 5, aRect.mHeight - 5);
                if (i == mSelectIdx) g->DrawRect(3, aRect.mY + 3, mWidth - 7, aRect.mHeight - 7);
            }
            NetUi::Literal(g, FONT_BRIANNETOD16,
                NetUi::Fit(FONT_BRIANNETOD16, StringToSexyString(aRoom.mName), mWidth - 120),
                13, aRect.mY + 18, NetUi::Ink());
            TodDrawString(g, StrFormat(_S("%d / %d"), aRoom.mPlayers, aRoom.mMaxPlayers),
                mWidth - 13, aRect.mY + 18, FONT_BRIANNETOD16, NetUi::Ink(), DS_ALIGN_RIGHT);
            const SexyString aHost = _S("Host: ") + StringToSexyString(aRoom.mHost);
            TodDrawString(g, FitOnlineText(aHost, mWidth - 175), 13, aRect.mY + 34,
                FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_LEFT);
            const SexyString aState = aRoom.mPlayers >= aRoom.mMaxPlayers ? _S("FULL") :
                aRoom.mPlaying ? _S("PLAYING") : aRoom.mHasPassword ? _S("PASSWORD") : _S("OPEN LOBBY");
            TodDrawString(g, aState, mWidth - 13, aRect.mY + 34,
                FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_RIGHT);
        }
    }
};

namespace
{
class OnlineButton : public LawnStoneButton
{
public:
    OnlineButton(int id, ButtonListener* listener, const SexyString& label) : LawnStoneButton(nullptr, id, listener)
    { SetLabel(label); mHasAlpha = true; mHasTransparencies = true; }
    void Draw(Graphics* g) override
    {
        if (!mBtnNoDraw)
            NetUi::StoneButton(g, Rect(0, 0, mWidth, mHeight), mLabel,
                mIsDown && mIsOver && !mDisabled, mIsOver && !mDisabled, mDisabled);
    }
};
}

NetOnlineDialog::NetOnlineDialog(LawnApp* theApp, bool theTutorialPreview) :
    LawnDialog(theApp, Dialogs::DIALOG_COOP_ONLINE, true, _S("ONLINE ROOMS"),
        _S(""), _S("CLOSE"), Dialog::BUTTONS_FOOTER),
    mPage(0), mRowsPerPage(6), mMaxPlayers(4), mLastActionTick(0), mAttemptedRoom(false), mPendingAction(0)
{
    mPassword = new EditWidget(202, this);
    mCreatePassword = new EditWidget(205, this);
    mRoomName = new EditWidget(203, this);
    mPaperImage = mApp->GetImage(_S("images/Almanac_PlantCard"));
    for (EditWidget* anEdit : { mPassword, mCreatePassword, mRoomName })
    {
        anEdit->SetFont(FONT_BRIANNETOD12);
        anEdit->SetColor(EditWidget::COLOR_BKG, Color(248, 243, 219));
        anEdit->SetColor(EditWidget::COLOR_OUTLINE, Color(95, 80, 50));
        anEdit->SetColor(EditWidget::COLOR_TEXT, Color(35, 30, 20));
        anEdit->mBlinkDelay = 14;
    }
    mPassword->mMaxChars = 64;
    mPassword->mPasswordChar = '*';
    mCreatePassword->mMaxChars = 64;
    mCreatePassword->mPasswordChar = '*';
    mRoomName->mMaxChars = 32;
    mRoomName->SetText(StringToSexyString(theApp->mNetManager->mConfig.mNick) + _S("'s room"));

    mRoomList = new RoomList(this);
    static int aListColors[][3] = {
        { 248, 243, 219 }, { 95, 80, 50 }, { 35, 30, 20 },
        { 225, 218, 180 }, { 85, 110, 60 }, { 255, 255, 235 }
    };
    mRoomList->SetColors(aListColors, 6);
    mRoomList->mItemHeight = 44;
    mRoomList->mDrawSelectWhenHilited = true;

    mConnect = new OnlineButton(REFRESH, this, _S("REFRESH"));
    mJoin = new OnlineButton(JOIN, this, _S("JOIN ROOM"));
    mCreate = new OnlineButton(CREATE, this, _S("CREATE"));
    mCapacity = new OnlineButton(CAPACITY, this, _S("4 PLAYERS"));
    mPrevious = new OnlineButton(PREV, this, _S("PREV"));
    mNext = new OnlineButton(NEXT, this, _S("NEXT"));

    const int aFrameEdges = IMAGE_DIALOG_TOPLEFT->mWidth + IMAGE_DIALOG_TOPRIGHT->mWidth;
    const int aFrameTile = IMAGE_DIALOG_TOPMIDDLE->mWidth;
    const int aFrameWidth = aFrameEdges + ((760 - aFrameEdges + aFrameTile - 1) / aFrameTile) * aFrameTile;
    const int aFrameHeight = DIALOG_HEADER_OFFSET + IMAGE_DIALOG_TOPLEFT->mHeight +
        6 * IMAGE_DIALOG_CENTERLEFT->mHeight + IMAGE_DIALOG_BOTTOMLEFT->mHeight;
    Resize(0, 0, aFrameWidth, aFrameHeight);
    LawnApp::CenterDialog(this, mWidth, mHeight);
    mConnect->Resize(mWidth - 184, 104, 130, 30);
    mRoomList->Resize(40, 172, 445, 264);
    mPrevious->Resize(318, 142, 76, 26);
    mNext->Resize(406, 142, 76, 26);
    mPassword->Resize(145, 447, 180, 28);
    mJoin->Resize(345, 443, 140, 36);
    mRoomName->Resize(514, 212, 205, 30);
    mCreatePassword->Resize(514, 279, 205, 30);
    mCapacity->Resize(514, 326, 205, 34);
    mCreate->Resize(514, 380, 205, 40);
    if (!theTutorialPreview && !mApp->mNetManager->IsActive()) ButtonDepress(REFRESH);
}

NetOnlineDialog::~NetOnlineDialog()
{
    delete mPassword;
    delete mCreatePassword;
    delete mRoomName;
    delete mRoomList;
    delete mConnect;
    delete mJoin;
    delete mCreate;
    delete mCapacity;
    delete mPrevious;
    delete mNext;
    delete mPaperImage;
}

void NetOnlineDialog::AddedToManager(WidgetManager* theManager)
{
    LawnDialog::AddedToManager(theManager);
    Widget* aWidgets[] = { mPassword, mCreatePassword, mRoomName, mRoomList,
        mConnect, mJoin, mCreate, mCapacity, mPrevious, mNext };
    for (Widget* aWidget : aWidgets)
        AddWidget(aWidget);
}

void NetOnlineDialog::RemovedFromManager(WidgetManager* theManager)
{
    Widget* aWidgets[] = { mPassword, mCreatePassword, mRoomName, mRoomList,
        mConnect, mJoin, mCreate, mCapacity, mPrevious, mNext };
    for (Widget* aWidget : aWidgets)
        RemoveWidget(aWidget);
    LawnDialog::RemovedFromManager(theManager);
}

void NetOnlineDialog::RebuildRoomList()
{
    const int aPageCount = max(1, ((int)mRooms.size() + mRowsPerPage - 1) / mRowsPerPage);
    for (int i = 0; i < (int)mRooms.size(); ++i)
        if (mRooms[i].mId == mSelectedRoom) mPage = i / mRowsPerPage;
    mPage = max(0, min(mPage, aPageCount - 1));
    mRoomList->RemoveAll();
    mRoomList->SetVisible(!mRooms.empty());
    int aSelection = -1;
    for (int i = mPage * mRowsPerPage; i < (int)mRooms.size() && i < (mPage + 1) * mRowsPerPage; i++)
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
    const int aGlobalIndex = mPage * mRowsPerPage + aSelection;
    mSelectedRoom = aSelection >= 0 && aGlobalIndex < (int)mRooms.size() ? mRooms[aGlobalIndex].mId : "";
    mPrevious->SetDisabled(mPage <= 0);
    mNext->SetDisabled(mPage >= aPageCount - 1);
    mPrevious->SetVisible(aPageCount > 1);
    mNext->SetVisible(aPageCount > 1);
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
            mPage = i / mRowsPerPage;
    RebuildRoomList();
}

void NetOnlineDialog::Update()
{
    LawnDialog::Update();
    NetManager* aNet = mApp->mNetManager;
    NetOnline* anOnline = aNet->GetOnline();
    const std::string aDetail = anOnline ? anOnline->GetLastErrorMessage() : std::string();
    if (!aDetail.empty() && (aDetail != mOnlineErrorStamp || mPendingAction))
    {
        mError = StringToSexyString(aDetail);
        mPendingAction = 0;
    }
    mOnlineErrorStamp = aDetail;
    if (mPendingAction && anOnline && !anOnline->GetLastErrorCode().empty())
    {
        mError = RoomError(anOnline->GetLastErrorCode());
        mPendingAction = 0;
    }
    if (mPendingAction && (!anOnline || (!anOnline->IsConnected() && !anOnline->IsConnecting())))
    {
        mError = _S("The connection was interrupted. Please try Refresh.");
        mPendingAction = 0;
    }
    int aRows = 6;
    if (!mError.empty())
    {
        Graphics aMeasure;
        const int aMessageHeight = TodDrawStringWrappedHelper(&aMeasure, mError, Rect(0, 0, 421, 600),
            FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT, false);
        aRows = max(1, min(5, (264 - aMessageHeight - 26) / 44));
    }
    if (aRows != mRowsPerPage)
    {
        mRowsPerPage = aRows;
        mRoomList->Resize(40, 172, 445, 44 * mRowsPerPage);
        RebuildRoomList();
    }
    if (mAttemptedRoom && ((anOnline && anOnline->IsHostingRoom() && !anOnline->GetRoomId().empty()) ||
        aNet->IsConnectedToHost()))
    {
        mApp->KillDialog(Dialogs::DIALOG_COOP_ONLINE);
        return;
    }
    SyncRooms();
    const bool aCanAct = anOnline && anOnline->IsConnected() && !aNet->IsActive() && !mPendingAction;
    mConnect->SetLabel(anOnline && anOnline->IsConnecting() ? _S("SEARCHING...") : _S("REFRESH"));
    mConnect->SetDisabled(aNet->IsActive() || (anOnline && anOnline->IsConnecting()));
    mCreate->SetDisabled(!aCanAct);
    mCapacity->SetDisabled(!aCanAct);
    mCreate->SetLabel(mPendingAction == CREATE ? _S("CREATING...") : _S("CREATE"));
    const int aSelectedIndex = mPage * mRowsPerPage + mRoomList->mSelectIdx;
    const NetOnlineRoom* aSelected = aSelectedIndex >= 0 && aSelectedIndex < (int)mRooms.size() ? &mRooms[aSelectedIndex] : nullptr;
    mJoin->SetDisabled(!aCanAct || !aSelected || aSelected->mPlayers >= aSelected->mMaxPlayers);
    mJoin->SetLabel(mPendingAction == JOIN ? _S("JOINING...") :
        aSelected && aSelected->mPlaying ? _S("WATCH GAME") : _S("JOIN ROOM"));
    mCapacity->SetLabel(StrFormat(_S("%d PLAYERS"), mMaxPlayers));
    MarkDirty();
}

void NetOnlineDialog::Draw(Graphics* g)
{
    LawnDialog::Draw(g);
    const Color aText(240, 228, 190);
    NetUi::Paper(g, mPaperImage, Rect(494, 172, 245, 264));
    TodDrawString(g, StrFormat(_S("ROOMS  %d"), (int)mRooms.size()), 40, 161,
        FONT_BRIANNETOD16, aText, DS_ALIGN_LEFT);
    TodDrawString(g, _S("CREATE A ROOM"), 616, 161, FONT_BRIANNETOD16, aText, DS_ALIGN_CENTER);
    TodDrawString(g, _S("ROOM NAME"), 512, 202, FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT);
    TodDrawString(g, _S("PASSWORD (OPTIONAL)"), 512, 269, FONT_BRIANNETOD12, NetUi::Ink(), DS_ALIGN_LEFT);
    TodDrawString(g, _S("PASSWORD"), 40, 468, FONT_BRIANNETOD12, aText, DS_ALIGN_LEFT);
    NetOnline* anOnline = mApp->mNetManager->GetOnline();
    const SexyString aStatus = mApp->mNetManager->IsActive() ? _S("Leave your current game to join another room.") :
        mPendingAction == CREATE ? _S("Opening your new room...") :
        mPendingAction == JOIN ? _S("Joining your friend...") :
        anOnline && anOnline->IsConnecting() ? _S("Looking for online rooms...") :
        anOnline && anOnline->IsConnected() ? _S("Choose a room, or invite friends to a new one.") :
        _S("Online play is unavailable. Refresh to try again.");
    TodDrawString(g, FitOnlineText(mError.empty() ? aStatus : _S("Connection needs attention. See the message below."), 550), 40, 139,
        FONT_BRIANNETOD12, mError.empty() ? aText : Color(255, 150, 130), DS_ALIGN_LEFT);
    if (!mError.empty())
    {
        const int aTop = mRoomList->mY + mRoomList->mHeight + 10;
        const Rect aNotice(40, aTop, 445, 436 - aTop);
        NetUi::Paper(g, mPaperImage, aNotice);
        TodDrawStringWrappedHelper(g, mError, Rect(aNotice.mX + 12, aNotice.mY + 8, aNotice.mWidth - 24, aNotice.mHeight - 16),
            FONT_BRIANNETOD12, Color(125, 42, 25), DS_ALIGN_LEFT, true);
    }
    if (mRooms.empty())
    {
        const Rect aListRect(mRoomList->mX, mRoomList->mY, mRoomList->mWidth, mRoomList->mHeight);
        NetUi::Paper(g, mPaperImage, aListRect);
        const int aCaptionY = aListRect.mY + aListRect.mHeight / 2 - 19;
        TodDrawString(g, anOnline && anOnline->IsConnecting() ? _S("FINDING YOUR NEIGHBORS...") : _S("NO ROOMS YET"),
            aListRect.mX + aListRect.mWidth / 2, aCaptionY, FONT_BRIANNETOD16, NetUi::Ink(), DS_ALIGN_CENTER);
        TodDrawString(g, _S("Create the first room and invite a friend!"),
            aListRect.mX + aListRect.mWidth / 2, aCaptionY + 31, FONT_BRIANNETOD12, NetUi::MutedInk(), DS_ALIGN_CENTER);
    }
}

void NetOnlineDialog::CloseBrowser()
{
    NetManager* aNet = mApp->mNetManager;
    if (mAttemptedRoom && !aNet->IsActive())
        aNet->Disconnect();
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
    if (theId == REFRESH)
    {
        if (aNet->IsActive()) return;
        if (anOnline && anOnline->IsConnected())
            anOnline->RefreshRooms();
        else if (!aNet->ConnectOnlineDirectory())
            mError = OnlineFailure(aNet, _S("Online play is unavailable. Please try again."));
        return;
    }
    if (!anOnline || !anOnline->IsConnected())
    {
        mError = OnlineFailure(aNet, _S("Online play is unavailable. Please try Refresh."));
        return;
    }
    if (mPendingAction || aNet->IsActive()) return;
    const std::string aPassword = SexyStringToStringFast(theId == CREATE ? mCreatePassword->mString : mPassword->mString);
    if (aPassword.size() > 64)
    {
        mError = _S("That password is too long. Please shorten it.");
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
        if (mAttemptedRoom) mPendingAction = CREATE;
        if (!mAttemptedRoom)
            mError = OnlineFailure(aNet, _S("Could not create the room. Please try again."));
    }
    else if (theId == JOIN && !mSelectedRoom.empty())
    {
        mAttemptedRoom = aNet->JoinOnlineRoom(mSelectedRoom, aPassword);
        if (mAttemptedRoom) mPendingAction = JOIN;
        if (!mAttemptedRoom)
            mError = OnlineFailure(aNet, _S("Could not join the room. Check password, capacity and status."));
    }
}

void NetOnlineDialog::ListClicked(int, int theIndex, int theClickCount)
{
    const int aGlobalIndex = mPage * mRowsPerPage + theIndex;
    if (theIndex < 0 || aGlobalIndex >= (int)mRooms.size())
        return;
    mRoomList->SetSelect(theIndex);
    mSelectedRoom = mRooms[aGlobalIndex].mId;
    if (theClickCount == 2)
        ButtonDepress(JOIN);
}

void NetOnlineDialog::EditWidgetText(int theId, const SexyString&)
{
    if (theId == 202)
        ButtonDepress(JOIN);
    else if (theId == 203 || theId == 205)
        ButtonDepress(CREATE);
}

void NetOnlineDialog::KeyDown(KeyCode theKey)
{
    if (theKey == KEYCODE_ESCAPE)
        CloseBrowser();
    else if (theKey == KEYCODE_RETURN)
        ButtonDepress(JOIN);
}
