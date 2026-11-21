#include "FusionNativeMoneyAction.h"
#include "FusionNative.h"
#include "FusionWire.h"
#include "FusionEconomyRules.h"
#include "FusionNativeEconomy.h"
#include "FusionNativeProducer.h"
#include "FusionNativeLobber.h"
#include "FusionNativeCannon268.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeAction266.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeGlove.h"
#include "FusionNativeVisual.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <vector>
namespace LawnFusion
{
namespace
{
bool Present(const Plant* p)
{return p&&!p->mDead&&!p->mSquished&&p->mIsOnBoard&&p->mPlantHealth>0&&
    p->mBurnedCounter==-1&&p->mOnBungeeState==NOT_ON_BUNGEE;}
bool Living(const Plant* p){return Present(p)&&!p->mNetGhost;}
enum class Availability { Unavailable, Unsupported, NotOwned, Cooldown, Funds, Capacity, Ready };
Availability Preview(const Plant* p,int slot,FusionMoneyActionQuote& quote)
{
    if(!Present(p)||!p->mApp||p->mApp->mBoard!=p->mBoard||!CanApplyNativeFusionGameplay(p->mBoard))
        return Availability::Unavailable;
    const auto* c=GetNativeContext(p->mBoard);const auto* d=GetNativePlantData(p);
    if(!c||!d||slot<0||slot>=int(kFusionBankPlayers)||!(c->selection.State().activeMask&(1U<<slot))||
        c->selection.State().phase!=FusionBankPhase::Playing||p->mBoard->mPaused||p->mApp->mGameScene!=SCENE_PLAYING)
        return Availability::Unavailable;
    if(!p->mBoard->CanUsePlantTools(slot))return Availability::Unavailable;
    if((d->owner!=slot&&d->owner!=LawnNet::NET_SHARED_OWNER)||IsNativeGlovePlantReserved(p->mBoard,p,-1))
        return Availability::NotOwned;
    int goldCount=0;Plant* other=nullptr;
    if(d->kind==1136||d->kind==1187)
        while(p->mBoard->mPlants.IterateNext(other))
            if(const auto* identity=GetNativePlantData(other);identity&&Present(other)&&
                (d->kind==1136?(identity->kind==1137||identity->kind==936):identity->kind==1188))++goldCount;
    if(!TryGetFusionMoneyActionQuote(d->kind,goldCount,quote)||
        (quote.upgrade&&GetNativeSeedForPlant(quote.resultKind)==SEED_NONE))return Availability::Unsupported;
    if(!quote.upgrade&&p->mEatenFlashCountdown>0)return Availability::Cooldown;
    if(c->money<quote.fee)return Availability::Funds;
    const int coins=FusionMoneySpendSunCount(quote.fee,p->mApp->IsIZombieLevel())+
        (!quote.upgrade&&d->kind==1188&&d->batchShotCount?1:0);
    if(coins>p->mBoard->mCoins.mMaxSize-p->mBoard->mCoins.mSize||
        (quote.upgrade&&p->mBoard->mPlants.mSize>=p->mBoard->mPlants.mMaxSize))return Availability::Capacity;
    return Availability::Ready;
}
void Explain(Board* b,Availability status,const FusionMoneyActionQuote& quote)
{
    SexyString text;
    switch(status)
    {
    case Availability::Unsupported:text=_S("Монетка не усиливает это растение.");break;
    case Availability::NotOwned:text=_S("Можно усилить только свои растения.");break;
    case Availability::Cooldown:text=_S("Способность ещё не готова.");break;
    case Availability::Funds:text=StrFormat(_S("Для усиления нужно денег: %d."),quote.fee);break;
    case Availability::Capacity:text=_S("Недостаточно места для эффекта усиления.");break;
    default:return;
    }
    b->DisplayAdvice(text,MESSAGE_STYLE_HINT_FAST,ADVICE_NONE);
}
bool Skill(Plant* p)
{
    const auto* d=GetNativePlantData(p);
    if(!d||!Living(p)||p->mEatenFlashCountdown>0)return false;
    if(d->kind==1188||d->kind==1137)return TryActivateNativeEconomySkill(p);
    if(d->kind==1144||d->kind==916||d->kind==923||d->kind==1148)return TryActivateNativeUmbrellaBatchSkill(p);
    if(d->kind==934)return TryActivateNativeSolar269Skill(p);
    if(d->kind==1135&&!TryActivateNativeGoldCabbage(p))return false;
    if((d->kind==1139||d->kind==1142)&&!TryActivateNativeCannon268Money(p))return false;
    if((d->kind==1145||d->kind==1146)&&!TryActivateNativeAction266Skill(p))return false;
    // These source classes override SuperSkill; their specific policies must
    // exist before they can participate in the shared base-heal fallback.
    if(d->kind==916||d->kind==923||d->kind==934||d->kind==1143||d->kind==1144||d->kind==1148)return false;
    p->mPlantHealth=p->mPlantMaxHealth;p->mEatenFlashCountdown=200;
    NativePlantHealthChanged(p);
    return true;
}
bool GoldPot(const Plant* p)
{
    Plant* other=nullptr;
    while(p->mBoard->mPlants.IterateNext(other))
    {
        const auto* d=GetNativePlantData(other);
        if(d&&d->kind==1137&&!other->mDead&&other->mIsOnBoard&&
            other->mPlantCol==p->mPlantCol&&other->mRow==p->mRow)return true;
    }
    return false;
}
void OtherSkill(Plant* original,bool upgradedPot)
{
    const auto* source=GetNativePlantData(original);if(!source)return;
    const bool sunflower=source->kind==1188;
    if(!sunflower&&!upgradedPot&&!GoldPot(original))return;
    std::vector<uint32_t> ids;Plant* p=nullptr;
    while(original->mBoard->mPlants.IterateNext(p))
    {
        if(p==original||!Living(p))continue;
        const auto* d=GetNativePlantData(p);if(!d)continue;
        const bool sameCell=p->mPlantCol==original->mPlantCol&&p->mRow==original->mRow;
        if(sunflower?sameCell:upgradedPot?(sameCell&&(d->kind==1135||d->kind==1139||d->kind==1142)):
            (GoldPot(p)&&(d->kind==1135||d->kind==1146))) ids.push_back(p->mBoard->mPlants.DataArrayGetID(p));
    }
    for(auto id:ids)if(auto* live=original->mBoard->mPlants.DataArrayTryToGet(id))Skill(live);
}
}
bool IsNativeMoneyTargetApplicable(const Plant* plant,int slot)
{FusionMoneyActionQuote quote;return Preview(plant,slot,quote)==Availability::Ready;}
bool TryNativeMoneyPlantAction(Plant* p,int slot)
{
    if(!Living(p)||!p->mApp||p->mApp->NetIsClient()||p->mApp->mBoard!=p->mBoard||
        !CanApplyNativeFusionGameplay(p->mBoard))return false;
    FusionMoneyActionQuote quote;
    if(Preview(p,slot,quote)!=Availability::Ready)return false;
    auto* context=GetNativeContext(p->mBoard);const auto* d=GetNativePlantData(p);
    const auto next=quote.resultKind;const int fee=quote.fee;const bool upgrade=quote.upgrade;
    const int sunCount=FusionMoneySpendSunCount(fee,p->mApp->IsIZombieLevel());
    Plant* result=p;
    const uint16_t beforeKind=d->kind;
    if(upgrade)
    {
        std::string error;
        if(!EnsureNativePlantVisuals(p->mBoard,next,&error))return false;
        NativePlantData fresh;
        fresh.kind=next;fresh.firstParent=d->kind;fresh.secondParent=kFusionNoPlant;
        fresh.packetId=d->packetId;fresh.owner=d->owner;fresh.randomOrigin=d->randomOrigin;
        result=p->mBoard->AddPlant(p->mPlantCol,p->mRow,GetNativeSeedForPlant(next),SEED_NONE,&fresh);
        const auto* bound=GetNativePlantData(result);
        if(!result||result->mDead||!bound||!bound->instanceToken||bound->kind!=next||!Skill(result))
        {if(result)result->Die(255);return false;}
        // Atomic native adaptation: allocate/verify the replacement before
        // consuming the original. Its HP, clocks and UID are freshly initialized.
        p->Die(2);
    }
    else if(!Skill(result))return false;
    const int before=context->money;context->money-=fee;
    if(sunCount)
    {
        const bool emitted=EmitNativeSunBatch(result,sunCount,false);
        TOD_ASSERT(emitted); // Same admitted gameplay gate and whole callback capacity were reserved above.
    }
    OtherSkill(result,upgrade&&next==1137);
    LawnNet::NetTrace("Fusion native Money action old=%u result=%u plant=%u fee=%d before=%d after=%d sunItems=%d upgrade=%d slot=%d tick=%d",
        beforeKind,GetNativePlantData(result)->kind,result->mNetId,fee,before,context->money,sunCount,upgrade?1:0,slot,p->mBoard->mMainCounter);
    return true;
}
bool ToggleNativeMoneyFromKeyboard(Board* board)
{
    auto* context=GetNativeContext(board);
    if(!context||!board->mCursorObject||board->mApp->mGameScene!=SCENE_PLAYING||board->mPaused||
        board->mTimeStopCounter>0||board->mApp->GetDialogCount()||board->mApp->mSeedChooserScreen||
        board->mApp->mNetChatTyping||context->selection.State().phase!=FusionBankPhase::Playing)return false;
    auto* net=board->mApp->mNetManager;
    const int slot=board->mApp->NetActive()&&net?net->GetMySlot():0;
    if(slot<0||slot>=static_cast<int>(kFusionBankPlayers)||
        !(context->selection.State().activeMask&(1U<<slot))||!board->CanUsePlantTools(slot))return false;
    if(board->mApp->NetActive()&&(!net||net->IsSpectator()||!net->GetPlayer(slot).mActive||
        net->GetPlayer(slot).mSpectator||net->GetInputPhase()!=LawnNet::NETINPUT_PLAYING||
        (board->mApp->NetIsClient()&&net->mRemotePaused)))return false;
    Sexy::Rect rect=board->GetShovelButtonRect();board->GetZenButtonRect(OBJECT_TYPE_MONEY_SIGN,rect);
    return HandleNativeMoneyPointer(board,1,rect.mX+rect.mWidth/2,rect.mY+rect.mHeight/2);
}

bool HandleNativeMoneyPointer(Board* board,int button,int x,int y)
{
    auto* context=GetNativeContext(board);
    if(!context||!board->mCursorObject||!button)return false;
    auto* net=board->mApp->mNetManager;
    const int slot=board->mApp->NetActive()&&net?net->GetMySlot():0;
    if(board->mApp->NetActive()&&net&&net->GetRawActingPlayerSlot()>=0&&net->GetRawActingPlayerSlot()!=slot)return false;
    Sexy::Rect rect=board->GetShovelButtonRect();board->GetZenButtonRect(OBJECT_TYPE_MONEY_SIGN,rect);
    const bool hitButton=rect.Contains(x,y),holding=board->mCursorObject->mCursorType==CURSOR_TYPE_MONEY_SIGN;
    if(!hitButton&&!holding)return false;
    if(slot<0||slot>=static_cast<int>(kFusionBankPlayers)||context->selection.State().phase!=FusionBankPhase::Playing||
        board->mPaused||board->mApp->mGameScene!=SCENE_PLAYING||board->mApp->GetDialogCount()||
        board->mApp->mNetChatTyping||!board->CanUsePlantTools(slot)||
        (net&&board->mApp->NetActive()&&(net->IsSpectator()||net->GetInputPhase()!=LawnNet::NETINPUT_PLAYING)))return true;
    if(button<0||hitButton)
    {
        board->ClearCursor();
        if(button>0&&!holding)board->mCursorObject->mCursorType=CURSOR_TYPE_MONEY_SIGN;
        return true;
    }
    Plant* plant=board->ToolHitTest(x,y);const auto* d=GetNativePlantData(plant);
    if(!plant||!d||!plant->mNetId||!d->instanceToken)return true;
    FusionMoneyActionQuote quote;const auto available=Preview(plant,slot,quote);
    if(available!=Availability::Ready)
    {
        LawnNet::NetTrace("Fusion native Money pointer rejected kind=%u reason=%d fee=%d money=%d slot=%d",
            d->kind,int(available),quote.fee,context->money,slot);
        Explain(board,available,quote);return true;
    }
    FusionCommand command;command.action=FusionCommandKind::MoneyPlant;
    command.sourceId=plant->mNetId;command.sourceToken=d->instanceToken;
    board->mApp->SubmitNativeFusionCommand(command);
    return true;
}
}
