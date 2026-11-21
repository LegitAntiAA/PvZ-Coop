#ifndef LAWN_FUSION_NATIVE_DROPPED_CARDS269_H
#define LAWN_FUSION_NATIVE_DROPPED_CARDS269_H
#include <cstdint>
#include "FusionCatalog.h"
#include "FusionNativePlacement.h"
class Board;class Plant;class Coin;
namespace Sexy {class Graphics;}
namespace LawnFusion
{
struct FusionCommand;
struct NativeDroppedCard269Data
{
    uint16_t kind=0xffff;
    int cost=0;
    uint32_t instanceToken=0;
    uint8_t heldSlot=255;
    bool landed=false,flashing=false;
};
inline bool NativeDroppedCard269DataValid(const NativeDroppedCard269Data& d)
{return d.instanceToken&&FusionFindPlant(d.kind)&&d.cost>=0&&d.cost<=1000000000&&(d.heldSlot==255||d.heldSlot<4)&&
    (d.heldSlot==255||d.landed);}
inline bool NativeDroppedCard269CoinStateValid(const NativeDroppedCard269Data& d,int age,int uses,int flash)
{return NativeDroppedCard269DataValid(d)&&age>=0&&uses>=0&&uses<(IsFusionSourcePuffKind(d.kind)?3:1)&&flash>=0&&flash<=age&&
    (d.flashing?(age>1000&&flash>0):flash==0);}
// Root stores metadata by actual Coin full generation in NativeContext/FNS.
// No allocation/RNG from a client getter; host create binds that generation.
NativeDroppedCard269Data* EditNativeDroppedCard269Data(Coin*,bool create=false);
const NativeDroppedCard269Data* GetNativeDroppedCard269Data(const Coin*);
enum class NativeDroppedCard269Action:uint8_t {Pickup,PutDown,Plant};
bool IsNativeDroppedCard269(const Coin*);
bool CanCreateNativeDroppedCard269(Board*,uint16_t kind,int cost=0);
Coin* CreateNativeDroppedCard269(Board*,uint16_t kind,int cost,float centerX,float centerY);
bool CanCreateNativeRemaining269DroppedCard(Board*,uint16_t kind);
bool CreateNativeRemaining269DroppedCard(Plant*,uint16_t kind,float centerX,float centerY);
// BEFORE Coin::Update's existing mCoinAge++/legacy motion; true owns the one
// actual Coin age increment. Held source cards pause age, without another clock.
bool UpdateNativeDroppedCard269(Coin*,bool isSourceTutorial=false);
bool DrawNativeDroppedCard269(Sexy::Graphics*,const Coin*);
bool DrawNativeDroppedCard269Cursor(Sexy::Graphics*,const Board*,int slot,float x,float y);
Coin* FindNativeDroppedCard269At(Board*,float boardX,float boardY);
Coin* GetNativeDroppedCard269Held(const Board*,int slot);
bool HandleNativeDroppedCard269Command(Board*,int authenticatedSlot,NativeDroppedCard269Action,const FusionCommand&);
// Roster/disconnect/cursor-clear adapter. Host mutates only the specified
// authenticated slot; a client sends PutDown rather than writing its ghost.
void CancelNativeDroppedCard269(Board*,int slot);
void NativeDroppedCard269Removing(Coin*);
// FNS adoption after complete actual Coin metadata restores each player's
// real local CoinID cursor; opaque host token is never a local array index.
void RefreshNativeDroppedCard269Cursors(Board*);
}
#endif
