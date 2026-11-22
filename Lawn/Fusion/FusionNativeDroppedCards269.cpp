#include "FusionNativeDroppedCards269.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNative.h"
#include "FusionNativeTools268.h"
#include "FusionNativePlacement.h"
#include "FusionNativeVisual.h"
#include "FusionWire.h"
#include "FusionNativeConveyor308.h"
#include "FusionNativeSourceLifecycle313.h"
#include "../SeedPacket.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../CursorObject.h"
#include "../SeedPacket.h"
#include "../Net/NetManager.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../SexyAppFramework/Graphics.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
namespace LawnFusion
{
namespace
{
bool Current(const Board* b){return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Point(float x,float y){return std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=4096&&std::abs(y)<=4096;}
bool Active(const Board* b,int slot)
{const auto* c=GetNativeContext(b);if(!c||slot<0||slot>=4||!(c->selection.State().activeMask&(1U<<slot)))return false;
 if(!b->mApp->NetActive())return slot==0;const auto& p=b->mApp->mNetManager->GetPlayer(slot);return p.mActive&&!p.mSpectator;}
CursorObject* Cursor(Board* b,int slot)
{if(slot<0||slot>=4)return nullptr;const int local=b->mApp->NetActive()?b->mApp->mNetManager->GetMySlot():0;return slot==local?b->mCursorObject:b->mNetPlayerCursors[slot];}
void ClearCursor(Board* b,int slot,const Coin* expected)
{if(auto* c=Cursor(b,slot))if(c->mCursorType==CURSOR_TYPE_PLANT_FROM_USABLE_COIN&&
    (!expected||b->mCoins.DataArrayTryToGet(c->mCoinID)==expected))
 {c->mCoinID=COINID_NULL;c->mType=c->mImitaterType=SEED_NONE;c->mSeedBankIndex=-1;c->mCursorType=CURSOR_TYPE_NORMAL;}}
void HoldCursor(Board* b,int slot,Coin* coin)
{if(auto* c=Cursor(b,slot)){c->mCursorType=CURSOR_TYPE_PLANT_FROM_USABLE_COIN;c->mCoinID=CoinID(b->mCoins.DataArrayGetID(coin));
 c->mType=GetNativeSeedForPlant(GetNativeDroppedCard269Data(coin)->kind);c->mImitaterType=SEED_NONE;c->mSeedBankIndex=-1;}}
Coin* Resolve(Board* b,uint16_t id,uint32_t token)
{if(!id||!token)return nullptr;Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead&&c->mNetId==id)
 {const auto* d=GetNativeDroppedCard269Data(c);return d&&d->instanceToken==token?c:nullptr;}return nullptr;}
int Gray(const Coin* c,const NativeDroppedCard269Data& d)
{return d.flashing?int(((std::cos(float(c->mDisappearCounter)*.01f*2.f*3.1415927f)+1.f)*.25f+.5f)*255.f):255;}
}
bool IsNativeDroppedCard269(const Coin* c)
{return c&&Current(c->mBoard)&&GetNativeDroppedCard269Data(c);}
bool CanCreateNativeDroppedCard269(Board* b,uint16_t kind,int cost)
{if(!Host(b)||!FusionFindPlant(kind)||GetNativeSeedForPlant(kind)==SEED_NONE||cost<0||cost>1000000000||
 !b->mCoins.mBlock||b->mCoins.mSize>=b->mCoins.mMaxSize)return false;
 size_t count=0;Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead&&GetNativeDroppedCard269Data(c))++count;
 return count<128&&EnsureNativePortraitVisuals(b,kind);}
Coin* CreateNativeDroppedCard269(Board* b,uint16_t kind,int cost,float x,float y,bool toss)
{
    if(!Point(x,y)||!CanCreateNativeDroppedCard269(b,kind,cost))return nullptr;
    auto* c=b->mCoins.DataArrayAlloc();c->mApp=b->mApp;c->mBoard=b;
    c->CoinInitializeNetGhost(int(x-25),int(y-35),COIN_USABLE_SEED_PACKET,COIN_MOTION_COIN,nullptr);
    c->mNetGhost=false;c->mNetId=b->NetAllocId();if(!c->mNetId){b->mCoins.DataArrayFree(c);return nullptr;}
    auto* d=EditNativeDroppedCard269Data(c,true);if(!d){b->mCoins.DataArrayFree(c);return nullptr;}
    *d={};d->kind=kind;d->cost=cost;d->instanceToken=b->mCoins.DataArrayGetID(c);
    c->mUsableSeedType=GetNativeSeedForPlant(kind);c->mX=c->mY=0;c->mWidth=50;c->mHeight=70;c->mScale=1;
    c->mPosX=x-25;c->mPosY=y-35;c->mCoinAge=c->mDisappearCounter=c->mFadeCount=c->mTimesDropped=0;c->mIsBeingCollected=false;c->mHitGround=false;
    c->mVelX=toss?RandRangeFloat(-2,2)*float(kFusionBasePixelsX*.01):0;c->mVelY=toss?-RandRangeFloat(2,3)*float(GetNativeFusionPixelsY(b)*.01):0;
    c->mGroundY=int(b->GridToPixelY(0,GetNativeFusionRowCount(b)-1)+50+.1*GetNativeFusionPixelsY(b)-35);
    LawnNet::NetTrace("Fusion DroppedCard269 birth id=%u token=%u kind=%u cost=%d x=%.9g y=%.9g",c->mNetId,d->instanceToken,kind,cost,x,y);return c;
}
bool CanCreateNativeRemaining269DroppedCard(Board* b,uint16_t kind){return CanCreateNativeDroppedCard269(b,kind,0);}
bool CreateNativeRemaining269DroppedCard(Plant* p,uint16_t kind,float x,float y)
{return p&&Current(p->mBoard)&&GetNativePlantData(p)&&CreateNativeDroppedCard269(p->mBoard,kind,0,x,y);}
bool GetNativeRemaining269PlantTags(const Plant* p,bool& flying,bool& pumpkin,bool& pot,bool& lily)
{
    const auto* d=GetNativePlantData(p);if(!p||!d||!Current(p->mBoard))return false;
    // TypeMgr source tables, not native carrier SeedType categories.
    const auto kind=d->kind;flying=kind==241||kind==912||kind==1082;lily=kind==12;pumpkin=pot=false;
    for(uint16_t k:{24,911,922,935,1087,1088,1089,1091,1092,1097,1098,1099,1110,1164,1169,1190})if(kind==k)pumpkin=true;
    for(uint16_t k:{27,936,1112,1114,1125,1130,1133,1136,1137,1184})if(kind==k)pot=true;
    return true;
}
bool UpdateNativeDroppedCard269(Coin* c,bool tutorial)
{
    if(!IsNativeDroppedCard269(c))return false;if(c->mDead||c->mNetGhost||!Host(c->mBoard)||!CanApplyNativeFusionGameplay(c->mBoard))return true;
    auto* d=EditNativeDroppedCard269Data(c);const auto age=c->mCoinAge;
    if(d->fallByBoss366)
    {++c->mCoinAge;c->mVelY+=float(9.8*GetNativeFusionPixelsY(c->mBoard)*.0001);c->mPosX+=c->mVelX;c->mPosY+=c->mVelY;
     if(c->mCoinAge>500)c->Die();return true;}
    if(!d->landed)
    {c->mVelY+=float(9.8*GetNativeFusionPixelsY(c->mBoard)*.0001);c->mPosX+=c->mVelX;c->mPosY+=c->mVelY;
     if(c->mPosY>c->mGroundY){c->mPosY=float(c->mGroundY);d->landed=true;}
     const float right=float(c->mBoard->GetCellCenterX(c->mBoard->GetColumnCount()-1,0)-.15*kFusionBasePixelsX-25);
     if(c->mPosX>right){c->mPosX=right;c->mVelX=-float(kFusionBasePixelsX*.01);}if(age>60)d->landed=true;}
    c->mHitGround=d->landed;
    if(d->heldSlot!=255)return true;
    ++c->mCoinAge;
    if(tutorial)return true;
    if(c->mCoinAge>1500){c->Die();return true;}
    if(c->mCoinAge>1000&&c->mCoinAge<1500)d->flashing=true;
    if(d->flashing)++c->mDisappearCounter;
    return true;
}
Coin* GetNativeDroppedCard269Held(const Board* b,int slot)
{if(!Current(b)||slot<0||slot>=4)return nullptr;Coin* c=nullptr;while(const_cast<Board*>(b)->IterateCoins(c))if(!c->mDead)if(const auto* d=GetNativeDroppedCard269Data(c))if(d->heldSlot==slot)return c;return nullptr;}
Coin* FindNativeDroppedCard269At(Board* b,float x,float y)
{if(!Current(b)||!Point(x,y))return nullptr;Coin* c=nullptr;Coin* result=nullptr;while(b->IterateCoins(c))if(!c->mDead)
 if(const auto* d=GetNativeDroppedCard269Data(c))if(!d->fallByBoss366&&d->heldSlot==255&&x>=c->mPosX&&x<=c->mPosX+50&&y>=c->mPosY&&y<=c->mPosY+70)result=c;return result;}
bool DrawNativeDroppedCard269(Sexy::Graphics* g,const Coin* c)
{if(!IsNativeDroppedCard269(c))return false;const auto* d=GetNativeDroppedCard269Data(c);if(c->mDead||d->heldSlot!=255)return true;
 DrawNativeFusionSeedPacket(g,c->mPosX,c->mPosY,d->kind,false,d->cost,0,Gray(c,*d),true);return true;}
bool DrawNativeDroppedCard269Cursor(Sexy::Graphics* g,const Board* b,int slot,float x,float y)
{auto* c=GetNativeDroppedCard269Held(b,slot);if(!c)return false;const auto* d=GetNativeDroppedCard269Data(c);
 DrawNativeFusionSeedPacket(g,x-25,y-35,d->kind,false,d->cost,0,255,false);return true;}
bool HandleNativeDroppedCard269Command(Board* b,int slot,NativeDroppedCard269Action action,const FusionCommand& command)
{
    const auto* ctx=GetNativeContext(b);if(!ctx||!Host(b)||!Active(b,slot)||command.epoch!=ctx->epoch)return false;
    auto* c=Resolve(b,command.sourceId,command.sourceToken);if(!c)return false;auto* d=EditNativeDroppedCard269Data(c);
    if(command.kind!=d->kind||d->fallByBoss366||!NativeDroppedCard269DataValid(*d))return false;
    if(action==NativeDroppedCard269Action::PutDown)
    {if(d->heldSlot!=slot)return false;d->heldSlot=255;ClearCursor(b,slot,c);return true;}
    if(ctx->selection.State().phase!=FusionBankPhase::Playing||b->mPaused||b->mApp->mGameScene!=SCENE_PLAYING||b->mApp->GetDialogCount())return false;
    if(action==NativeDroppedCard269Action::Pickup)
    {auto* cursor=Cursor(b,slot);if(d->heldSlot!=255||GetNativeEquipmentClaimedBy(b,slot)||GetNativeFertilize269ClaimedBy(b,slot)||ctx->gloveHolds[slot].plantId||GetNativeDroppedCard269Held(b,slot)||!cursor||cursor->mCursorType!=CURSOR_TYPE_NORMAL)return false;
     d->heldSlot=uint8_t(slot);d->landed=true;c->mHitGround=true;HoldCursor(b,slot,c);return true;}
    if(d->heldSlot!=slot)return false;
    if(action!=NativeDroppedCard269Action::Plant||command.targetId>65535||!b->IsValidGridCell(command.column,command.row)||!b->CanTakeSunMoney(d->cost))return false;
    const int price=d->cost;NativePlantData incoming;incoming.kind=d->kind;incoming.owner=uint8_t(slot);incoming.packetId=0;incoming.randomOrigin=0;
    auto* made=TryPlaceNativeFusionPlant(b,command.column,command.row,incoming,uint16_t(command.targetId),slot,command.targetToken);
    if(!made)return false;b->TakeSunMoney(price);++c->mTimesDropped;d->heldSlot=255;ClearCursor(b,slot,c);
    if(c->mTimesDropped>=(IsFusionSourcePuffKind(d->kind)?3:1))c->Die();return true;
}
void CancelNativeDroppedCard269(Board* b,int slot)
{if(!Host(b)||slot<0||slot>=4)return;if(auto* c=GetNativeDroppedCard269Held(b,slot)){EditNativeDroppedCard269Data(c)->heldSlot=255;ClearCursor(b,slot,c);}}
void NativeDroppedCard269Removing(Coin* c)
{if(!IsNativeDroppedCard269(c)||!Host(c->mBoard))return;const auto* d=GetNativeDroppedCard269Data(c);if(d->heldSlot<4)ClearCursor(c->mBoard,d->heldSlot,c);}
void RefreshNativeDroppedCard269Cursors(Board* b)
{if(!Current(b))return;for(int slot=0;slot<4;++slot)
 {auto* card=GetNativeDroppedCard269Held(b,slot);if(card&&Active(b,slot))HoldCursor(b,slot,card);else{if(Host(b)&&card)EditNativeDroppedCard269Data(card)->heldSlot=255;
  if(auto* c=Cursor(b,slot))if(c->mCursorType==CURSOR_TYPE_PLANT_FROM_USABLE_COIN){auto* actual=b->mCoins.DataArrayTryToGet(c->mCoinID);if(!actual||IsNativeDroppedCard269(actual))ClearCursor(b,slot,nullptr);}}}}
bool FallNativeDroppedCardByBoss366(Coin* c)
{if(!c||!Host(c->mBoard)||c->mDead)return false;auto* d=EditNativeDroppedCard269Data(c);if(!d)return false;
 if(d->heldSlot<4)ClearCursor(c->mBoard,d->heldSlot,c);d->heldSlot=255;d->fallByBoss366=true;d->landed=d->flashing=false;
 c->mCoinAge=c->mDisappearCounter=0;c->mVelY=0;c->mHitGround=false;return true;}
bool CanFallNativeConveyorCards366(Board* b,std::string* error)
{
 if(!Host(b))return false;if(!IsNativeSourceConveyor308(b))return true;auto* ctx=GetNativeContext(b);
 const auto& packets=ctx->selection.State().packets;unsigned dropped=0;Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!c->mDead&&IsNativeDroppedCard269(c))++dropped;
 if(packets.size()+dropped>128)return false;
 for(const auto& packet:packets)if(!CanCreateNativeDroppedCard269(b,packet.kind,packet.price))return false;
 FusionSourceDeathBudget319 peak;peak.coins=packets.size();peak.droppedCards=packets.size();return CanAllocateNativeSourceBudget319(b,peak,error);
}
bool FallNativeConveyorCards366(Board* b)
{
 if(!CanFallNativeConveyorCards366(b))return false;if(!IsNativeSourceConveyor308(b))return true;auto* ctx=GetNativeContext(b);auto next=ctx->selection;
 const auto packets=next.State().packets;auto empty=next.State();empty.packets.clear();++empty.revision;if(!next.ApplySnapshot(empty))return false;
 struct Birth{FusionBankPacket packet;float x=0,y=0;uint16_t oldId=0;};std::vector<Birth> plan;plan.reserve(packets.size());
 for(const auto& packet:packets){SeedPacket* raw=nullptr;for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* q=&b->mSeedBank->mSeedPackets[i];const auto found=ctx->packets.find(q->mNetId);if(found!=ctx->packets.end()&&found->second.selectionId==packet.id){raw=q;break;}}if(!raw)return false;
  plan.push_back({packet,float(b->mSeedBank->mX+raw->mX+raw->mWidth/2),float(b->mSeedBank->mY+raw->mY+raw->mHeight/2),raw->mNetId});}
 // Capture all source cards before changing the queue. The existing Coin is
 // the native fall/Draw carrier; no player use, free purchase or extra RNG.
 for(const auto& item:plan){auto* c=CreateNativeDroppedCard269(b,item.packet.kind,item.packet.price,item.x,item.y,false);if(!c)return false;c->mNetId=item.oldId;c->mTimesDropped=item.packet.conveyorUses;if(!FallNativeDroppedCardByBoss366(c))return false;}
 ctx->selection=std::move(next);return SyncNativeConveyor308(b);
}
}
