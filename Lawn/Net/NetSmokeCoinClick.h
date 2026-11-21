#ifndef LAWN_NET_SMOKE_COIN_CLICK_H
#define LAWN_NET_SMOKE_COIN_CLICK_H
// Fixture-only point selection. Never changes a Coin or bypasses SDL input.
#include "../Board.h"
#include "../Coin.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>

namespace LawnNet
{
struct NetSmokeCoinClickState
{
    struct Sample { float x=0,y=0;uint32_t firstSnapshot=0;bool valid=false; };
    std::map<uint16_t,Sample> coins;
};
inline bool NetSmokeSettledCoinPoint(Board* board,Coin* coin,uint32_t snapshots,
    NetSmokeCoinClickState& state,int& pointX,int& pointY)
{
    if(!board||!coin||coin->mDead||!coin->mNetGhost||!coin->mHitGround||coin->mIsBeingCollected||!coin->mNetId||
        !std::isfinite(coin->mPosX)||!std::isfinite(coin->mPosY)||!std::isfinite(coin->mNetTargetX)||!std::isfinite(coin->mNetTargetY))return false;
    auto& sample=state.coins[coin->mNetId];
    if(!sample.valid||std::abs(sample.x-coin->mNetTargetX)>.01f||std::abs(sample.y-coin->mNetTargetY)>.01f)
    {sample={coin->mNetTargetX,coin->mNetTargetY,snapshots,true};return false;}
    if(snapshots-sample.firstSnapshot<2||std::abs(coin->mPosX-sample.x)>.25f||std::abs(coin->mPosY-sample.y)>.25f)return false;
    auto targetHits=[](Coin* actor,int x,int y)
    {
        HitResult hit{};
        return actor->MouseHitTest(int(std::lround(x+actor->mPosX-actor->mNetTargetX)),
            int(std::lround(y+actor->mPosY-actor->mNetTargetY)),&hit);
    };
    const int left=int(std::ceil((std::max)(coin->mPosX,sample.x)))+8;
    const int top=int(std::ceil((std::max)(coin->mPosY,sample.y)))+8;
    const int right=int(std::floor((std::min)(coin->mPosX,sample.x)+coin->mWidth))-8;
    const int bottom=int(std::floor((std::min)(coin->mPosY,sample.y)+coin->mHeight))-8;
    if(left>right||top>bottom)return false;
    auto point=[&](int x,int y)
    {
        HitResult hit{};if(!board->MouseHitTest(x,y,&hit)||hit.mObjectType!=OBJECT_TYPE_COIN||hit.mObject!=coin)return false;
        for(int dy:{-8,8})for(int dx:{-8,8})
        {HitResult own{};if(!coin->MouseHitTest(x+dx,y+dy,&own)||!targetHits(coin,x+dx,y+dy))return false;}
        Coin* other=nullptr;
        while(board->mCoins.IterateNext(other))if(other!=coin&&!other->mDead&&!other->mIsBeingCollected)
            for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx)
            {HitResult ignored{};if(other->MouseHitTest(x+dx,y+dy,&ignored)||(other->mNetGhost&&targetHits(other,x+dx,y+dy)))return false;}
        pointX=x;pointY=y;return true;
    };
    if(point((left+right)/2,(top+bottom)/2))return true;
    for(int y=top;y<=bottom;y+=4)for(int x=left;x<=right;x+=4)if(point(x,y))return true;
    return false;
}
}
#endif
