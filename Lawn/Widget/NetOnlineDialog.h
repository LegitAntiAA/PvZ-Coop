#ifndef __NETONLINEDIALOG_H__
#define __NETONLINEDIALOG_H__

#include "LawnDialog.h"
#include "../../SexyAppFramework/EditListener.h"
#include "../../SexyAppFramework/ListListener.h"
#include "../Net/NetOnline.h"

namespace Sexy
{
class EditWidget;
class ListWidget;
class DDImage;
}

class NetOnlineDialog : public LawnDialog, public Sexy::EditListener, public Sexy::ListListener
{
    enum { REFRESH = 101, JOIN, CREATE, CAPACITY, PREV, NEXT };
    class RoomList;
    Sexy::EditWidget* mPassword;
    Sexy::EditWidget* mCreatePassword;
    Sexy::EditWidget* mRoomName;
    Sexy::ListWidget* mRoomList;
    LawnStoneButton* mConnect;
    LawnStoneButton* mJoin;
    LawnStoneButton* mCreate;
    LawnStoneButton* mCapacity;
    LawnStoneButton* mPrevious;
    LawnStoneButton* mNext;
    std::vector<LawnNet::NetOnlineRoom> mRooms;
    std::string mRoomsStamp;
    std::string mOnlineErrorStamp;
    std::string mSelectedRoom;
    Sexy::DDImage* mPaperImage;
    SexyString mError;
    int mPage;
    int mRowsPerPage;
    int mMaxPlayers;
    unsigned int mLastActionTick;
    bool mAttemptedRoom;
    int mPendingAction;

    void RebuildRoomList();
    void SyncRooms();
    void CloseBrowser();

public:
    explicit NetOnlineDialog(LawnApp* theApp, bool theTutorialPreview = false);
    ~NetOnlineDialog();
    void AddedToManager(Sexy::WidgetManager* theManager) override;
    void RemovedFromManager(Sexy::WidgetManager* theManager) override;
    void Update() override;
    void Draw(Sexy::Graphics* g) override;
    void ButtonDepress(int theId) override;
    void ListClicked(int theId, int theIndex, int theClickCount) override;
    void EditWidgetText(int theId, const SexyString& theText) override;
    void KeyDown(Sexy::KeyCode theKey) override;
};
#endif
