#include "FusionNativeExtras268.h"
#include "FusionNative.h"
#include "FusionNativeEquipment.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeCold.h"
#include "FusionNativeBatchActions.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&GetNativePlantData(p);}
bool Host(const Plant* p){return Current(p)&&!p->mNetGhost&&!p->mApp->NetIsClient();}
bool Live(const Plant* p){return Host(p)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
}
bool IsNativeMachine268(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&d->kind==1129;}
void InitializeNativeExtras268Plant(Plant* p)
{
    if(!Host(p)||!IsFusionExtras268Kind(GetNativePlantData(p)->kind))return;
    auto* d=GetNativePlantData(p);d->batchMode=0;d->damageStage=0;d->shootSeconds=-1;
    // CherryMine1162 : Plant owns only its death callback. Its kind4 children
    // use the real shared PotatoMine preparation/contact controller.
    if(d->kind==1129)p->mPlantHealth=p->mPlantMaxHealth=4000;
}
bool CapNativeMachine268Damage(Plant* p,int& damage)
{return Live(p)&&IsNativeMachine268(p)&&TryGetFusionMachine268Damage(damage,damage);}
bool TryRedirectNativeMachine268Damage(Plant* p,int damage)
{
    if(!Live(p)||IsNativeMachine268(p)||damage<=0||!CanApplyNativeFusionGameplay(p->mBoard))return false;
    Plant* receiver=nullptr;while(p->mBoard->IteratePlants(receiver))
    {
        if(!Live(receiver)||!IsNativeMachine268(receiver)||std::abs(receiver->mPlantCol-p->mPlantCol)>1||std::abs(receiver->mRow-p->mRow)>1)continue;
        int applied=0;if(!TryGetFusionMachine268ProtectionDamage(damage,applied))return false;
        // Its explicit false Instead override terminates recursion. Cap is
        // applied before the real source base damage continuation.
        ApplyNativeSourcePlantDamage(receiver,applied,0);
        receiver->mEatenFlashCountdown=(std::max)(receiver->mEatenFlashCountdown,20);
        LawnNet::NetTrace("Fusion extras268 protect target=%u machine=%u incoming=%d applied=%d tick=%d",p->mNetId,receiver->mNetId,damage,applied,p->mBoard->mMainCounter);
        return true;
    }
    return false;
}
void NativeExtras268HealthChanged(Plant* p)
{
    if(!Live(p)||!IsNativeMachine268(p))return;auto* d=GetNativePlantData(p);uint8_t stage=d->damageStage;
    if(TryGetFusionDefenderDamageStage((std::min)(p->mPlantHealth,4000),4000,d->damageStage,stage))d->damageStage=stage;
}
bool RecoverNativeMachine268(Plant* p,int amount)
{
    if(!Live(p)||!IsNativeMachine268(p))return false;int after=0;
    if(!TryGetFusionMachine268Recovery(p->mPlantHealth,amount,after))return false;
    const int before=p->mPlantHealth;p->mPlantHealth=after;p->mEatenFlashCountdown=25;
    NativePlantHealthChanged(p);NativeExtras268HealthChanged(p);
    LawnNet::NetTrace("Fusion extras268 recover machine=%u amount=%d hp=%d->%d tick=%d",p->mNetId,amount,before,after,p->mBoard->mMainCounter);return true;
}
void NativeMachine268Element(Board* b,int row,uint8_t form)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!IsNativeFusionBoard(b)||
        (form!=1&&form!=2)||(row< -1||row>=GetNativeFusionRowCount(b)))return;
    std::vector<uint32_t> actors;Plant* p=nullptr;
    while(b->IteratePlants(p))if(Live(p)&&IsNativeMachine268(p)&&(row<0||p->mRow==row))actors.push_back(b->mPlants.DataArrayGetID(p));
    for(auto id:actors)if((p=b->mPlants.DataArrayTryToGet(id))&&Live(p))
    {
        auto* d=GetNativePlantData(p);int recovery=0;if(!TryGetFusionMachine268Transition(d->batchMode,form,recovery))continue;
        if(recovery)RecoverNativeMachine268(p,recovery);d=GetNativePlantData(p);d->batchMode=form;p->mEatenFlashCountdown=25;
        LawnNet::NetTrace("Fusion extras268 form machine=%u form=%u recovery=%d hp=%d row=%d tick=%d",p->mNetId,unsigned(form),recovery,p->mPlantHealth,p->mRow,b->mMainCounter);
    }
}
void NativeExtras268PlantRemoving(Plant* p,int reason)
{
    if(!Host(p)||!p->mDead||!p->mIsOnBoard||!IsNativeMachine268(p)||reason<0||reason>7)return;
    const auto form=GetNativePlantData(p)->batchMode;float x=0,y=0;
    if(form==1&&p->GetShadowCenter(x,y))ApplyNativeColdBoardFreeze(p->mBoard,x,y-.5f*float(GetNativeFusionPixelsY(p->mBoard)));
    else if(form==2)ApplyNativeBatchFireRow(p->mBoard,p->mRow,1800);
    LawnNet::NetTrace("Fusion extras268 death machine=%u form=%u reason=%d tick=%d",p->mNetId,unsigned(form),reason,p->mBoard->mMainCounter);
}
bool CanUseNativeExtras268Equipment(const Plant* p,FusionEquipmentType type)
{
    if(!Current(p)||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0)return false;FusionBucketReceiverEffect e;
    return TryGetFusionExtras268BucketEffect(GetNativePlantData(p)->kind,type,p->mPlantHealth,e);
}
bool TryUseNativeExtras268Equipment(Plant* p,FusionEquipmentType type,Coin* item)
{
    if(!Live(p)||!CanApplyNativeFusionGameplay(p->mBoard)||!CanUseNativeExtras268Equipment(p,type))return false;
    if(item){const auto* d=GetNativeEquipmentData(item);if(!d||d->type!=type||item->mDead||item->mBoard!=p->mBoard)return false;}
    FusionBucketReceiverEffect e;if(!TryGetFusionExtras268BucketEffect(GetNativePlantData(p)->kind,type,p->mPlantHealth,e))return false;
    if(e.replacementKind!=65535){if(!TryReplaceNativeEquipmentReceiver(p,e.replacementKind))return false;}
    else if(!RecoverNativeMachine268(p,e.recoverHealth))return false;
    return !item||ConsumeNativeEquipment(item);
}
}
