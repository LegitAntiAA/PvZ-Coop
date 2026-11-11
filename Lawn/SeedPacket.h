#ifndef __SEEDPACKET_H__
#define __SEEDPACKET_H__

#include "GameObject.h"
#include "../GameConstants.h"
#include <cstdint>

constexpr const int SLOT_MACHINE_TIME = 400;
constexpr const int CONVEYOR_SPEED = 4;

class HitResult;
class SeedPacket : public GameObject
{
public:
    int                 mRefreshCounter;                            //+0x24
    int                 mRefreshTime;                               //+0x28
    int                 mIndex;                                     //+0x2C
    int                 mOffsetX;                                   //+0x30
    SeedType            mPacketType;                                //+0x34
    SeedType            mImitaterType;                              //+0x38
    int                 mSlotMachineCountDown;                      //+0x3C
    SeedType            mSlotMachiningNextSeed;                     //+0x40
    float               mSlotMachiningPosition;                     //+0x44
    bool                mActive;                                    //+0x48
    bool                mRefreshing;                                //+0x49
    int                 mTimesUsed;                                 //+0x4C
    unsigned char       mNetOwnerSlot;                              // co-op owner slot, or NET_SHARED_OWNER
    uint8_t             mRandomDiscountLevel;                      // 0 ordinary, 1..10 intrinsic mystery origin

public:
    SeedPacket();
    bool                IsMysteryPending() const { return mRandomDiscountLevel > 0 && mPacketType == SeedType::SEED_NONE; }
    void                SetMysteryPlaceholder(uint8_t theLevel);

    void                Update();
    void                Draw(Graphics* g);
    void                MouseDown(int x, int y, int theClickCount);
    bool                MouseHitTest(int theX, int theY, HitResult* theHitResult);
    void                Deactivate();
    void                Activate();
    /*inline*/ void     SetActivate(bool theActivate);
    void                PickNextSlotMachineSeed();
    void                WasPlanted();
    /*inline*/ void     SlotMachineStart();
    void                FlashIfReady();
    bool                CanPickUp();
    void                SetPacketType(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
};

static_assert(sizeof(SeedPacket) == sizeof(GameObject) + 48, "Mystery metadata must use existing SeedPacket tail padding");

class SeedBank : public GameObject
{
public:
    int				    mNumPackets;                    //+0x24
    SeedPacket		    mSeedPackets[SEEDBANK_CAPACITY]; // stock bank + custom PvP rows
    int				    mCutSceneDarken;                //+0x348
    int				    mConveyorBeltCounter;           //+0x34C

public:
    SeedBank();

    void			    Draw(Graphics* g);
    bool			    MouseHitTest(int x, int y, HitResult* theHitResult);
    inline void		    Move(int x, int y) { mX = x; mY = y; }
    bool			    ContainsPoint(int theX, int theY);
    void			    AddSeed(SeedType theSeedType, bool thePlaceOnLeft = false);
    void			    RemoveSeed(int theIndex);
    int				    GetNumSeedsOnConveyorBelt();
    /*inline*/ int		CountOfTypeOnConveyorBelt(SeedType theSeedType);
    void			    UpdateConveyorBelt();
    void			    UpdateWidth();
    void			    RefreshAllPackets();
};

void				    SeedPacketDrawSeed(Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float theOffsetX, float theOffsetY, float theScale);
void				    DrawSeedPacket(Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float thePercentDark, int theGrayness, bool theDrawCost, bool theUseCurrentCost);
void                    DrawMysterySeedPacket(Graphics* g, float x, float y, int theGrayness = 255);


#endif
