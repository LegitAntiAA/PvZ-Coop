#include "FusionNativeTools268.h"
#include "FusionNative.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeCannon268.h"
#include "FusionNativeGlove.h"
#include "FusionNativePlacement.h"
#include "FusionNativeRemainingLeaves269.h"
#include "FusionWire.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <cmath>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Current(Board* b){return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(Board* b){return Current(b)&&!b->mApp->NetIsClient();}
int LocalSlot(Board* b){return b->mApp->NetActive()&&b->mApp->mNetManager?b->mApp->mNetManager->GetMySlot():0;}
bool Active(Board* b,int slot)
{
    const auto* c=GetNativeContext(b);if(!c||slot<0||slot>=int(kFusionBankPlayers)||!(c->selection.State().activeMask&(1U<<slot)))return false;
    if(!b->mApp->NetActive())return slot==0;
    auto* net=b->mApp->mNetManager;return net&&net->GetPlayer(slot).mActive&&!net->GetPlayer(slot).mSpectator;
}
bool Playing(Board* b,int slot)
{
    const auto* c=GetNativeContext(b);
    return Current(b)&&Active(b,slot)&&c->selection.State().phase==FusionBankPhase::Playing&&
        CanApplyNativeFusionGameplay(b)&&!b->mApp->GetDialogCount()&&!b->mApp->mNetChatTyping&&b->CanUsePlantTools(slot);
}
CursorObject* Cursor(Board* b,int slot)
{
    if(slot<0||slot>=int(kFusionBankPlayers))return nullptr;
    return slot==LocalSlot(b)?b->mCursorObject:b->mNetPlayerCursors[slot];
}
void Reset(CursorObject* cursor)
{
    if(!cursor)return;cursor->mCoinID=COINID_NULL;cursor->mCobCannonPlantID=PLANTID_NULL;cursor->mGlovePlantID=PLANTID_NULL;cursor->mDuplicatorPlantID=PLANTID_NULL;
    cursor->mCursorType=CURSOR_TYPE_NORMAL;cursor->mType=cursor->mImitaterType=SEED_NONE;cursor->mSeedBankIndex=-1;
}
Plant* ResolvePlant(Board* b,uint16_t id,uint32_t token)
{
    Plant* p=nullptr;while(b->IteratePlants(p))if(p->mNetId==id)
        if(const auto* d=GetNativePlantData(p);d&&d->instanceToken==token)return p;
    return nullptr;
}
bool AllowedPlant(Board* b,Plant* p,int slot)
{
    const auto* d=GetNativePlantData(p);
    return p&&d&&!p->mDead&&!p->mNetGhost&&p->mIsOnBoard&&p->mPlantHealth>0&&
        (d->owner==slot||d->owner==LawnNet::NET_SHARED_OWNER)&&!IsNativeGlovePlantReserved(b,p,-1);
}
bool LocalPlant(Board* b,Plant* p,int slot)
{
    const auto* d=GetNativePlantData(p);return p&&d&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&
        (d->owner==slot||d->owner==LawnNet::NET_SHARED_OWNER)&&!IsNativeGlovePlantReserved(b,p,-1);
}
void SubmitCancel(Board* b,Coin* coin)
{
    FusionCommand command;command.action=FusionCommandKind::EquipmentCancel;
    if(coin){command.sourceId=coin->mNetId;if(const auto* d=GetNativeFertilize269Data(coin))command.sourceToken=d->instanceToken;
        else if(const auto* d=GetNativeEquipmentData(coin))command.sourceToken=d->instanceToken;else return;}
    b->mApp->SubmitNativeFusionCommand(command);
}
Coin* ResolveFertilize(Board* b,uint16_t id,uint32_t token)
{
    if(!id||!token)return nullptr;Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead&&c->mNetId==id&&c->mType==COIN_NONE)
        if(const auto* d=GetNativeFertilize269Data(c);d&&d->instanceToken==token)return c;return nullptr;
}
void ClearFertilizeCursor(Coin* coin)
{
    if(!coin||!coin->mBoard)return;auto* b=coin->mBoard;
    const auto id=static_cast<CoinID>(b->mCoins.DataArrayGetID(coin));if(id==COINID_NULL)return;
    for(int slot=0;slot<int(kFusionBankPlayers);++slot)if(auto* cursor=Cursor(b,slot);cursor&&cursor->mCoinID==id)cursor->mCoinID=COINID_NULL;
    if(b->mCursorObject&&b->mCursorObject->mCoinID==id)b->mCursorObject->mCoinID=COINID_NULL;
}
}

Coin* GetNativeEquipmentClaimedBy(Board* b,int slot)
{
    if(!Current(b)||slot<0||slot>=int(kFusionBankPlayers))return nullptr;Coin* coin=nullptr;
    while(b->IterateCoins(coin))if(!coin->mDead)
        if(const auto* d=GetNativeEquipmentData(coin);d&&d->phase==NativeEquipmentPhase::Cursor&&d->claimSlot==slot)return coin;
    return nullptr;
}
Coin* GetNativeFertilize269ClaimedBy(Board* b,int slot)
{
    if(!Current(b)||slot<0||slot>=int(kFusionBankPlayers))return nullptr;Coin* c=nullptr;
    while(b->IterateCoins(c))if(!c->mDead&&c->mType==COIN_NONE)
        if(const auto* d=GetNativeFertilize269Data(c);d&&d->phase==NativeFertilize269Phase::Cursor&&d->heldSlot==slot)return c;
    return nullptr;
}
void NativeFertilize269CoinRemoving(Coin* coin)
{
    auto* d=EditNativeFertilize269Data(coin);if(!d)return;
    ClearFertilizeCursor(coin);d->heldSlot=255; // Removal owns phase/target metadata; don't reset another tool.
}
void ClearNativeEquipmentLease(Board* b,uint16_t id,uint32_t token)
{
    auto* coin=ResolveNativeEquipment(b,id,token);auto* d=GetNativeEquipmentData(coin);if(!coin||!d)return;
    const auto localId=static_cast<CoinID>(b->mCoins.DataArrayGetID(coin));
    for(int slot=0;slot<int(kFusionBankPlayers);++slot)
        if(auto* cursor=Cursor(b,slot);cursor&&cursor->mCoinID==localId)cursor->mCoinID=COINID_NULL;
    if(b->mCursorObject&&b->mCursorObject->mCoinID==localId)b->mCursorObject->mCoinID=COINID_NULL;
    d->claimSlot=255; // Caller owns the phase transition/deletion; source pair survives.
}
bool GetNativeEquipmentCursorPosition(Board* b,uint8_t slot,float& x,float& y)
{
    if(!Current(b)||!Active(b,slot)||!b->mApp->mDDInterface)return false;
    const float dx=float(b->mX+b->mApp->mDDInterface->mWideScreenOffsetX),dy=float(b->mY+b->mApp->mDDInterface->mWideScreenOffsetY);
    if(slot==LocalSlot(b))
    {auto* widgets=b->mApp->mWidgetManager;if(!widgets)return false;x=float(widgets->mLastMouseX)-dx;y=float(widgets->mLastMouseY)-dy;}
    else
    {auto* net=b->mApp->mNetManager;if(!net||!net->mRemoteCursors[slot].mValid)return false;x=net->mRemoteCursors[slot].mRenderX-dx;y=net->mRemoteCursors[slot].mRenderY-dy;}
    return std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=4096&&std::abs(y)<=4096;
}
void SyncNativeTools268Cursors(Board* b)
{
    if(!Current(b))return;
    for(int slot=0;slot<int(kFusionBankPlayers);++slot)
    {
        auto* coin=GetNativeEquipmentClaimedBy(b,slot);auto* fert=GetNativeFertilize269ClaimedBy(b,slot);auto* cursor=Cursor(b,slot);
        if(slot!=LocalSlot(b)&&cursor==b->mCursorObject)cursor=nullptr;
        if(coin&&!Active(b,slot)&&Host(b))
        {if(!PutDownNativeEquipment(coin))DropNativeEquipmentFromPlant(coin);coin=nullptr;}
        if(fert&&!Active(b,slot)&&Host(b))
        {if(PutDownNativeFertilize269(fert,slot))ClearFertilizeCursor(fert);fert=nullptr;}
        if(fert)
        {
            if(cursor&&cursor->mCursorType==CURSOR_TYPE_NORMAL)
            {cursor->mCoinID=static_cast<CoinID>(b->mCoins.DataArrayGetID(fert));cursor->mType=cursor->mImitaterType=SEED_NONE;cursor->mSeedBankIndex=-1;}
            float x=0,y=0;if(Host(b)&&GetNativeEquipmentCursorPosition(b,uint8_t(slot),x,y))
            {fert->mPosX=x-20;fert->mPosY=y-20;fert->mX=fert->mY=0;}
        }
        if(coin)
        {
            if(cursor&&cursor->mCursorType==CURSOR_TYPE_NORMAL)
            {cursor->mCoinID=static_cast<CoinID>(b->mCoins.DataArrayGetID(coin));cursor->mType=cursor->mImitaterType=SEED_NONE;cursor->mSeedBankIndex=-1;}
            float x=0,y=0;if(Host(b)&&GetNativeEquipmentCursorPosition(b,uint8_t(slot),x,y))
            {coin->mPosX=x-20;coin->mPosY=y-20;coin->mX=coin->mY=0;}
        }
        else if(cursor&&cursor->mCoinID!=COINID_NULL)
        {auto* old=b->mCoins.DataArrayTryToGet(cursor->mCoinID);if(!old||IsNativeEquipmentCoin(old)||(IsNativeFertilize269(old)&&!fert))cursor->mCoinID=COINID_NULL;}
    }
    // A sanitized empty held-card echo must not erase a local native Cannon
    // target selection. The real local Plant generation is the only anchor.
    auto* cursor=b->mCursorObject;if(!cursor||cursor->mCobCannonPlantID==PLANTID_NULL)return;
    auto* p=b->mPlants.DataArrayTryToGet(cursor->mCobCannonPlantID);
    if(!GetNativeEquipmentClaimedBy(b,LocalSlot(b))&&!GetNativeFertilize269ClaimedBy(b,LocalSlot(b))&&LocalPlant(b,p,LocalSlot(b))&&CanAimNativeCannon268(p))
    {cursor->mCursorType=CURSOR_TYPE_COBCANNON_TARGET;cursor->mType=cursor->mImitaterType=SEED_NONE;cursor->mSeedBankIndex=-1;}
    else {cursor->mCobCannonPlantID=PLANTID_NULL;if(cursor->mCursorType==CURSOR_TYPE_COBCANNON_TARGET)Reset(cursor);}
}
void OnNativeTools268CursorClearing(Board* b)
{
    if(!Current(b)||!b->mCursorObject)return;auto* net=b->mApp->mNetManager;const int slot=LocalSlot(b);
    if(net&&net->GetRawActingPlayerSlot()>=0&&net->GetRawActingPlayerSlot()!=slot)return;
    if(auto* card=GetNativeDroppedCard269Held(b,slot))
    {const auto* d=GetNativeDroppedCard269Data(card);FusionCommand command;command.action=FusionCommandKind::DroppedCardPutDown;
     command.kind=d->kind;command.sourceId=card->mNetId;command.sourceToken=d->instanceToken;b->mApp->SubmitNativeFusionCommand(command);}
    if(auto* coin=GetNativeEquipmentClaimedBy(b,slot))SubmitCancel(b,coin);
    if(auto* fert=GetNativeFertilize269ClaimedBy(b,slot))SubmitCancel(b,fert);
}
bool HandleNativeTools268Command(Board* b,int slot,const FusionCommand& command)
{
    auto* context=GetNativeContext(b);
    if(!Host(b)||!context||command.epoch!=context->epoch||!Active(b,slot))return false;
    if(command.action==FusionCommandKind::CannonAim)
    {
        auto* p=ResolvePlant(b,command.sourceId,command.sourceToken);
        if(!Playing(b,slot)||GetNativeEquipmentClaimedBy(b,slot)||GetNativeFertilize269ClaimedBy(b,slot)||context->gloveHolds[slot].plantId||
            !AllowedPlant(b,p,slot)||!CanAimNativeCannon268(p))return false;
        if(!CommitAimNativeCannon268(p,command.boardTargetX,command.boardTargetY))return false;
        Reset(Cursor(b,slot));return true;
    }
    auto* heldFert=GetNativeFertilize269ClaimedBy(b,slot);auto* fert=ResolveFertilize(b,command.sourceId,command.sourceToken);
    if(fert||(heldFert&&command.action==FusionCommandKind::EquipmentCancel))
    {
        if(command.action==FusionCommandKind::EquipmentCancel)
        {
            if((command.sourceId==0)!=(command.sourceToken==0)||!heldFert||(command.sourceId&&fert!=heldFert))return false;
            if(!PutDownNativeFertilize269(heldFert,slot))return false;ClearFertilizeCursor(heldFert);return true;
        }
        if(!Playing(b,slot)||!fert)return false;
        if(command.action==FusionCommandKind::EquipmentPickup)
        {
            if(heldFert)return heldFert==fert;
            if(GetNativeEquipmentClaimedBy(b,slot)||GetNativeDroppedCard269Held(b,slot)||!Cursor(b,slot))return false;
            if(!PickNativeFertilize269(fert,slot))return false;
            CancelNativeGlove(b,slot);Reset(Cursor(b,slot));Cursor(b,slot)->mCoinID=static_cast<CoinID>(b->mCoins.DataArrayGetID(fert));return true;
        }
        if(command.action!=FusionCommandKind::EquipmentUse||heldFert!=fert||!b->IsValidGridCell(command.column,command.row))return false;
        // Fert affects every actual layer in this cell, so preflight every
        // recipient against the same source owner/glove guards as equipment.
        bool recipient=false;Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&GetNativePlantData(p)&&p->mPlantCol==command.column&&p->mRow==command.row)
        {if(!AllowedPlant(b,p,slot))return false;recipient=true;}
        if(!recipient||!UseNativeFertilize269(fert,slot,command.column,command.row))return false;
        ClearFertilizeCursor(fert);return true;
    }
    if(heldFert)return false; // Another item cannot acquire the same authenticated cursor lease.
    Coin* held=GetNativeEquipmentClaimedBy(b,slot);
    if(command.action==FusionCommandKind::EquipmentCancel)
    {
        if((command.sourceId==0)!=(command.sourceToken==0))return false;
        if(command.sourceId&&(!held||held->mNetId!=command.sourceId||GetNativeEquipmentData(held)->instanceToken!=command.sourceToken))return false;
        return !held||PutDownNativeEquipment(held);
    }
    if(!Playing(b,slot))return false;
    auto* coin=ResolveNativeEquipment(b,command.sourceId,command.sourceToken);auto* data=GetNativeEquipmentData(coin);
    if(!coin||!data||data->weapon||coin->mType!=COIN_NONE)return false;
    if(command.action==FusionCommandKind::EquipmentPickup)
    {
        if(held)return held==coin;
        if(GetNativeDroppedCard269Held(b,slot))return false;
        if(data->phase==NativeEquipmentPhase::Cursor||data->claimSlot!=255||!Cursor(b,slot))return false;
        CancelNativeGlove(b,slot);Reset(Cursor(b,slot));
        data->phase=NativeEquipmentPhase::Cursor;data->claimSlot=uint8_t(slot);data->phaseStartAge=coin->mCoinAge;coin->mHitGround=true;
        Cursor(b,slot)->mCoinID=static_cast<CoinID>(b->mCoins.DataArrayGetID(coin));
        LawnNet::NetTrace("Fusion tools268 pickup slot=%d coin=%u token=%u type=%u",slot,coin->mNetId,data->instanceToken,unsigned(data->type));return true;
    }
    if(command.action!=FusionCommandKind::EquipmentUse||held!=coin||data->phase!=NativeEquipmentPhase::Cursor||
        data->claimSlot!=slot||!b->IsValidGridCell(command.column,command.row))return false;
    const auto type=data->type;std::vector<uint32_t> recipients;Plant* p=nullptr;
    while(b->IteratePlants(p))if(AllowedPlant(b,p,slot)&&p->mPlantCol==command.column&&p->mRow==command.row&&
        (CanUseNativeEquipmentOnPlant(p,type)||CanUseNativeExtras268Equipment(p,type)))recipients.push_back(b->mPlants.DataArrayGetID(p));
    bool used=false;for(auto generation:recipients)if((p=b->mPlants.DataArrayTryToGet(generation))&&AllowedPlant(b,p,slot))
        if(TryUseNativeEquipmentOnPlant(p,type,GetNativePlantData(p)->kind==1166?coin:nullptr)||TryUseNativeExtras268Equipment(p,type,nullptr))used=true;
    if(!used)return false;
    LawnNet::NetTrace("Fusion tools268 use slot=%d coin=%u token=%u column=%u row=%u layers=%zu",slot,coin->mNetId,data->instanceToken,command.column,command.row,recipients.size());
    return GetNativeEquipmentData(coin)->weapon||ConsumeNativeEquipment(coin); // CherryMagnet keeps the same Coin body.
}
bool HandleNativeTools268Pointer(Board* b,int button,int x,int y)
{
    if(!Current(b)||!button||!b->mCursorObject)return false;auto* net=b->mApp->mNetManager;const int slot=LocalSlot(b);
    if(net&&net->GetRawActingPlayerSlot()>=0&&net->GetRawActingPlayerSlot()!=slot)return false;
    if(!Playing(b,slot)||(net&&b->mApp->NetIsClient()&&net->mRemotePaused))return false;
    SyncNativeTools268Cursors(b);auto* cursor=b->mCursorObject;
    if(auto* card=GetNativeDroppedCard269Held(b,slot))
    {
        const auto* d=GetNativeDroppedCard269Data(card);FusionCommand command;
        command.kind=d->kind;command.sourceId=card->mNetId;command.sourceToken=d->instanceToken;
        command.action=button<0?FusionCommandKind::DroppedCardPutDown:FusionCommandKind::DroppedCardPlant;
        if(button>0)
        {const int col=b->PixelToGridX(x,y),row=b->PixelToGridY(x,y);Plant* target=nullptr;
         if(!b->IsValidGridCell(col,row)||!ResolveNativeFusionPlacementTarget(b,col,row,d->kind,target,true))return true;
         command.column=uint8_t(col);command.row=uint8_t(row);
         if(target){command.targetId=target->mNetId;command.targetToken=GetNativePlantData(target)->instanceToken;}}
        b->mApp->SubmitNativeFusionCommand(command);return true;
    }
    if(auto* fert=GetNativeFertilize269ClaimedBy(b,slot))
    {
        if(cursor->mCursorType!=CURSOR_TYPE_NORMAL){SubmitCancel(b,fert);return false;}
        const int col=b->PixelToGridX(x,y),row=b->PixelToGridY(x,y);
        if(button<0){SubmitCancel(b,fert);return true;}
        if(!b->IsValidGridCell(col,row))return true;
        const auto* d=GetNativeFertilize269Data(fert);FusionCommand command;command.action=FusionCommandKind::EquipmentUse;
        command.sourceId=fert->mNetId;command.sourceToken=d->instanceToken;command.column=uint8_t(col);command.row=uint8_t(row);
        b->mApp->SubmitNativeFusionCommand(command);return true;
    }
    if(button>0)
    {
        Coin* fert=nullptr;while(b->IterateCoins(fert))
        {
            const auto* d=GetNativeFertilize269Data(fert);if(!d||fert->mDead||fert->mType!=COIN_NONE||d->heldSlot!=255||d->phase==NativeFertilize269Phase::Using)continue;
            if(std::abs(float(x)-(fert->mPosX+20))>25||std::abs(float(y)-(fert->mPosY+20))>25)continue;
            FusionCommand command;command.action=FusionCommandKind::EquipmentPickup;command.sourceId=fert->mNetId;command.sourceToken=d->instanceToken;
            b->ClearCursor();b->mApp->SubmitNativeFusionCommand(command);return true;
        }
    }
    if(button>0)if(auto* card=FindNativeDroppedCard269At(b,float(x),float(y)))
    {const auto* d=GetNativeDroppedCard269Data(card);FusionCommand command;command.action=FusionCommandKind::DroppedCardPickup;
     command.kind=d->kind;command.sourceId=card->mNetId;command.sourceToken=d->instanceToken;
     b->ClearCursor();b->mApp->SubmitNativeFusionCommand(command);return true;}
    if(cursor->mCursorType==CURSOR_TYPE_COBCANNON_TARGET)
    {
        if(button<0){b->ClearCursor();return true;}
        auto* p=b->mPlants.DataArrayTryToGet(cursor->mCobCannonPlantID);const auto* d=GetNativePlantData(p);
        if(!LocalPlant(b,p,slot)||!CanAimNativeCannon268(p)||!d){b->ClearCursor();return true;}
        if(!b->IsValidGridCell(b->PixelToGridX(x,y),b->PixelToGridY(x,y)))return true;
        FusionCommand command;command.action=FusionCommandKind::CannonAim;command.sourceId=p->mNetId;command.sourceToken=d->instanceToken;
        command.boardTargetX=int16_t(x);command.boardTargetY=int16_t(y);
        if(b->mApp->SubmitNativeFusionCommand(command))Reset(cursor);return true;
    }
    if(auto* held=GetNativeEquipmentClaimedBy(b,slot))
    {
        if(cursor->mCursorType!=CURSOR_TYPE_NORMAL){SubmitCancel(b,held);return false;}
        const int col=b->PixelToGridX(x,y),row=b->PixelToGridY(x,y);
        if(button<0||!b->IsValidGridCell(col,row)){SubmitCancel(b,held);return button<0;}
        const auto* d=GetNativeEquipmentData(held);FusionCommand command;command.action=FusionCommandKind::EquipmentUse;
        command.sourceId=held->mNetId;command.sourceToken=d->instanceToken;command.column=uint8_t(col);command.row=uint8_t(row);
        b->mApp->SubmitNativeFusionCommand(command);return true;
    }
    if(button<=0)return false;
    Coin* coin=nullptr;while(b->IterateCoins(coin))
    {
        const auto* d=GetNativeEquipmentData(coin);HitResult hit;
        if(!d||d->weapon||coin->mDead||d->phase==NativeEquipmentPhase::Cursor||!coin->MouseHitTest(x,y,&hit))continue;
        FusionCommand command;command.action=FusionCommandKind::EquipmentPickup;command.sourceId=coin->mNetId;command.sourceToken=d->instanceToken;
        b->ClearCursor();b->mApp->SubmitNativeFusionCommand(command);return true;
    }
    if(cursor->mCursorType!=CURSOR_TYPE_NORMAL)return false;
    Plant* p=b->ToolHitTest(x,y);const auto* d=GetNativePlantData(p);if(!p||!IsNativeCannon268(p))return false;
    if(LocalPlant(b,p,slot)&&CanAimNativeCannon268(p)&&d&&!GetNativeContext(b)->gloveHolds[slot].plantId)
    {b->ClearCursor();cursor->mCobCannonPlantID=static_cast<PlantID>(b->mPlants.DataArrayGetID(p));cursor->mCursorType=CURSOR_TYPE_COBCANNON_TARGET;}
    return true;
}
}
