#include "FusionNativeMoney.h"
#include "FusionMoneyRules.h"
#include "FusionNative.h"
#include "FusionProducerRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
bool Host(Board* board)
{ return board && board->mApp && board->mApp->mBoard == board && !board->mApp->NetIsClient() && IsNativeFusionBoard(board); }
}
bool EmitNativeMoneyItem(Board* board, float x, float y, uint16_t sourceItem)
{
    CoinType type; int value = 0;
    if (!Host(board) || !TryGetFusionMoneyItem(sourceItem,type,value) || !std::isfinite(x) || !std::isfinite(y) ||
        double(x)-20 < (std::numeric_limits<int>::min)() || double(x)-20 > (std::numeric_limits<int>::max)() ||
        double(y)-20 < (std::numeric_limits<int>::min)() || double(y)-20 > (std::numeric_limits<int>::max)() ||
        !FusionProducerCanFitCallback(board->mCoins.mSize,board->mCoins.mMaxSize,1)) return false;
    auto* coin = board->AddCoin(static_cast<int>(double(x)-20),static_cast<int>(double(y)-20),
        type,COIN_MOTION_FROM_PLANT,SEED_NONE,value,true);
    if (!coin) return false;
    LawnNet::NetTrace("Fusion native Money emit item=%u coin=%u value=%d type=%d x=%.4f y=%.4f typed=%d tick=%d",
        sourceItem,coin->mNetId,GetNativeCoinValue(coin,0),int(type),x,y,IsNativeMoneyCoin(coin)?1:0,board->mMainCounter);
    return true;
}
bool EmitNativeMoneyCoins(Plant* plant, int count)
{
    if (!plant || !Host(plant->mBoard) || plant->mNetGhost || plant->mDead || !plant->mIsOnBoard || !plant->IsInPlay()) return false;
    const auto* data = GetNativePlantData(plant);
    const auto* profile = data ? FindFusionProducerProfile(data->kind) : nullptr;
    if (!profile || profile->mode != FusionProducerMode::Marigold || count != profile->periodicCoinCount ||
        !FusionProducerCanFitCallback(plant->mBoard->mCoins.mSize,plant->mBoard->mCoins.mMaxSize,count)) return false;
    // Native motion/audio consume their own cosmetic RNG. Only these explicit
    // 0..99 draws choose money rewards; no Unity RNG-stream parity is claimed.
    for (int i = 0; i < count; ++i)
    {
        uint16_t item = 0; int value = 0;
        const int draw = RandRangeInt(0,99);
        if (!TryGetFusionMarigoldReward(draw,item,value)) return true;
        const bool emitted = EmitNativeMoneyItem(plant->mBoard,float(plant->mX+20),float(plant->mY+20),item);
        LawnNet::NetTrace("Fusion native Money producer plant=%u kind=%u index=%d count=%d draw=%d item=%u value=%d emitted=%d tick=%d",
            plant->mNetId,data->kind,i,count,draw,item,value,emitted?1:0,plant->mBoard->mMainCounter);
        // A capacity failure was rejected before any draw/allocation. An
        // unexpected native allocation failure consumes this callback rather
        // than retrying a partially emitted batch and duplicating money.
        if (!emitted) return true;
    }
    plant->mApp->PlayFoley(FOLEY_COIN);
    return true;
}
bool CreditNativeMoneyCoin(Coin* coin)
{
    if (!coin || !coin->mBoard || !IsNativeFusionBoard(coin->mBoard) || !coin->IsMoney()) return false;
    if (!Host(coin->mBoard) || coin->mNetGhost) return true;
    int value = 0;
    if (!ClaimNativeMoneyCoinValue(coin,value))
    {
        LawnNet::NetTrace("Fusion native Money credit omitted untyped coin=%u",coin->mNetId);
        return true;
    }
    if (!value) return true; // Kept money-tagged until prune: duplicate ScoreCoin is handled, not credited again.
    auto* context = GetNativeContext(coin->mBoard); int after = 0;
    if (!context || !TryCreditFusionMoney(context->money,value,after))
    {
        LawnNet::NetTrace("Fusion native Money credit rejected coin=%u value=%d",coin->mNetId,value);
        return true;
    }
    const int before = context->money; context->money = after;
    LawnNet::NetTrace("Fusion native Money credit coin=%u value=%d before=%d after=%d tick=%d host=1 profile=0 sun=0",
        coin->mNetId,value,before,after,coin->mBoard->mMainCounter);
    return true;
}
}
