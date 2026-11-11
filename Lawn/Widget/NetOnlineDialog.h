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
}

class NetOnlineDialog : public LawnDialog, public Sexy::EditListener, public Sexy::ListListener
{
    enum { CONNECT = 101, JOIN, CREATE, CAPACITY, PREV, NEXT };
    Sexy::EditWidget* mEndpoint;
    Sexy::EditWidget* mPassword;
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
    std::string mSelectedRoom;
    std::string mConnectedEndpoint;
    SexyString mError;
    int mPage;
    int mMaxPlayers;
    unsigned int mLastActionTick;
    bool mAttemptedRoom;

    void RebuildRoomList();
    void SyncRooms();
    void CloseBrowser();

public:
    explicit NetOnlineDialog(LawnApp* theApp);
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
