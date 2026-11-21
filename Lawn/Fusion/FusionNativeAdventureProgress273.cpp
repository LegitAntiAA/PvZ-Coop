#include "FusionNativeAdventureProgress273.h"
#include "FusionNative.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../MessageWidget.h"
#include "../System/Music.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Playing(const Board* b)
{return Host(b)&&b->mApp->mGameScene==SCENE_PLAYING&&!b->mPaused&&!b->mApp->GetDialogCount();}
bool Actor(const Plant* p)
{return p&&!p->mDead&&!p->mNetGhost&&p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth>0;}
SexyString Text(FusionAdventureMessage273 message)
{
    switch(message)
    {
    case FusionAdventureMessage273::Welcome:return _S("Добро пожаловать в PvZ Fusion!");
    case FusionAdventureMessage273::NewPlants:return _S("Здесь вас ждут новые растения и способы игры.");
    case FusionAdventureMessage273::Resources:return _S("Начнём с получения ресурсов!");
    case FusionAdventureMessage273::PlantSunflower:return _S("Посадите подсолнух.");
    case FusionAdventureMessage273::FusePeaSunflower:return _S("Подсолнух даёт солнце. Посадите на него горохострел.");
    case FusionAdventureMessage273::BuildGatling:return _S("Сажайте горох на горох, чтобы получить стрелка Гатлинга.");
    case FusionAdventureMessage273::Ready:return _S("Теперь высаживайте растения и защищайтесь от зомби!");
    default:return _S("");
    }
}
bool MissionCards(Board* b)
{
    if(b->mCoins.mMaxSize-b->mCoins.mSize<3||!CanCreateNativeDroppedCard269(b,0,0))return false;
    int count=0;Coin* existing=nullptr;
    while(b->IterateCoins(existing))if(!existing->mDead&&GetNativeDroppedCard269Data(existing))++count;
    if(count>125)return false;
    std::array<Coin*,3> cards{};
    for(int i=0;i<3;++i)
    {
        const int column=4+i,row=2;
        cards[i]=CreateNativeDroppedCard269(b,0,0,float(b->GetCellCenterX(column,row)),
            float(b->GridToPixelY(column,row)+50));
        if(!cards[i])
        {
            for(auto* card:cards)if(card)card->Die();
            return false;
        }
    }
    return true;
}
bool Active(Board* b,int slot)
{
    const auto* c=GetNativeContext(b);
    if(!c||slot<0||slot>=int(kFusionBankPlayers)||!(c->selection.State().activeMask&(1U<<slot)))return false;
    if(!b->mApp->NetActive())return slot==0;
    const auto* net=b->mApp->mNetManager;if(!net)return false;
    const auto& player=net->GetPlayer(slot);return player.mActive&&!player.mSpectator;
}
}
void PresentNativeFusionAdventureTutorial273(Board* b,const FusionAdventureRun273& r)
{
    if(!Current(b)||!FusionAdventureRunValid273(r)||!r.level||!b->mAdvice)return;
    const auto text=Text(r.tutorial.message);
    if(!r.tutorial.messageTicks)
    {
        if(!text.empty()&&b->mAdvice->mLabel==text)b->ClearAdviceImmediately();
        return;
    }
    if(b->mAdvice->mLabel!=text||b->mAdvice->mDuration==0)
        b->DisplayAdvice(text,MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY,ADVICE_NONE);
    // Native Advice supplies font/layout/drawing. The source message clock,
    // including600s BuildGatling, stays in the explicitly replicated room view.
    b->mAdvice->mDuration=r.tutorial.messageTicks;
    b->mAdvice->mDisplayTime=FusionAdventureMessageDuration273(r.tutorial.message);
}
bool UpdateNativeFusionAdventureTutorial273(Board* b,FusionAdventureRun273& r,FusionAdventureTutorialEffects273& out)
{
    out={};
    if(!Playing(b)||!FusionAdventureRunValid273(r)||!r.level)return false;
    bool sunflower=false,peaSunflower=false,gatling=false;Plant* p=nullptr;
    while(b->IteratePlants(p))if(Actor(p))if(const auto* d=GetNativePlantData(p))
    {sunflower|=d->kind==1;peaSunflower|=d->kind==1000;gatling|=d->kind==1032;}
    auto candidate=r;FusionAdventureTutorialEffects273 effects;
    if(!StepFusionAdventureTutorial273(candidate,1,sunflower,peaSunflower,gatling,effects))return false;
    // Retry the same source stage if native allocation is temporarily full.
    // A partial group is retired and no stage/message/card grant is committed.
    if(effects.grantMissionCards&&!MissionCards(b))return false;
    r=candidate;out=effects;
    if(effects.messageChanged||!r.tutorial.messageTicks)PresentNativeFusionAdventureTutorial273(b,r);
    return true;
}
bool NativeFusionAdventureEnemyDied273(Board* b,FusionAdventureRun273& r,int wave,int maximum,int enemies,bool iz,float x,float y)
{
    if(!Host(b)||!std::isfinite(x)||!std::isfinite(y)||
        !TryArmFusionAdventureReward273(r,wave,maximum,enemies,iz))return false;
    // Unity clamps the root+(0,1) anchor to viewport[.2,.8]. Use the actual
    // native viewport and the established source vertical-unit projection.
    r.trophySpawnX=std::clamp(x,float(b->mWidth)*.2f,float(b->mWidth)*.8f);
    r.trophySpawnY=std::clamp(y-float(GetNativeFusionPixelsY(b)),float(b->mHeight)*.2f,float(b->mHeight)*.8f);
    EnsureNativeFusionAdventureTrophy273(b,r);return true;
}
bool EnsureNativeFusionAdventureTrophy273(Board* b,FusionAdventureRun273& r)
{
    if(!Host(b)||!FusionAdventureRunValid273(r)||!r.over||r.trophyNetId||r.rewardCollected||
        !b->mCoins.mBlock||b->mCoins.mSize>=b->mCoins.mMaxSize)return false;
    auto* coin=b->mCoins.DataArrayAlloc();coin->mApp=b->mApp;coin->mBoard=b;
    // The real Coin owns drawing, native drop physics and hit testing. Create
    // locally, then expose it through the normal complete native snapshot.
    coin->CoinInitialize(int(r.trophySpawnX),int(r.trophySpawnY),COIN_TROPHY,COIN_MOTION_COIN);
    coin->mNetGhost=false;coin->mNetId=b->NetAllocId();
    if(!coin->mNetId){coin->Die();return false;}
    coin->mPosX=r.trophySpawnX-coin->mWidth*.5f;coin->mPosY=r.trophySpawnY-coin->mHeight*.5f;
    coin->mGroundY=int(coin->mPosY+float(GetNativeFusionPixelsY(b))*.5f);
    if(!BindFusionAdventureTrophy273(r,coin->mNetId,b->mCoins.DataArrayGetID(coin)))
    {coin->Die();return false;}
    LawnNet::NetTrace("Fusion source Adventure trophy spawned level=%u id=%u token=%u over=1 progressCommitted=0",
        r.level,r.trophyNetId,r.trophyToken);return true;
}
bool IsNativeFusionAdventureTrophy273(const Coin* coin,const FusionAdventureRun273& r)
{
    if(!coin||!Current(coin->mBoard)||!FusionAdventureRunValid273(r)||!r.over||!r.trophyToken||
        coin->mType!=COIN_TROPHY||!coin->mNetId||coin->mNetId!=r.trophyNetId)return false;
    // Client Coin array generations are local; only the host compares its
    // canonical generation. Commands carry the replicated host token explicitly.
    return coin->mApp->NetIsClient()||coin->mBoard->mCoins.DataArrayGetID(const_cast<Coin*>(coin))==r.trophyToken;
}
bool CollectNativeFusionAdventureTrophy273(Coin* coin,int slot,FusionAdventureProfile273& profile,
    FusionAdventureRun273& r,bool& committed)
{
    committed=false;if(!IsNativeFusionAdventureTrophy273(coin,r))return false;
    auto* b=coin->mBoard;
    if(!Playing(b)||coin->mDead||coin->mNetGhost||!Active(b,slot)||
        !TryCollectFusionAdventureReward273(profile,r,coin->mNetId,b->mCoins.DataArrayGetID(coin)))return true;
    coin->mCollectX=coin->mPosX;coin->mCollectY=coin->mPosY;
    coin->mIsBeingCollected=true;coin->mHitGround=true;coin->mVelX=coin->mVelY=0;
    coin->mNetCollectionFxStarted=true;
    AttachmentDie(coin->mAttachmentID);
    b->mApp->mMusic->StopAllMusic();b->mApp->PlaySample(Sexy::SOUND_DIAMOND);
    committed=true;
    LawnNet::NetTrace("Fusion source Adventure trophy CLICK level=%u slot=%d frontier=%u cycle=%u replay=%d completed=%llu",
        r.level,slot,profile.frontierLevel,profile.cycle,r.replay?1:0,static_cast<unsigned long long>(profile.completedLevels));
    return true;
}
bool UpdateNativeFusionAdventureTrophy273(Coin* coin,const FusionAdventureRun273& r)
{
    if(!IsNativeFusionAdventureTrophy273(coin,r))return false;
    if(!Playing(coin->mBoard)||coin->mDead||coin->mNetGhost)return true;
    if(coin->mCoinAge<(std::numeric_limits<int>::max)())++coin->mCoinAge;
    if(!r.rewardCollected){coin->UpdateFall();return true;}
    const float t=(std::min)(1.0f,float(kFusionAdventureRewardTicks273-r.rewardTicks)/125.0f);
    auto* b=coin->mBoard;
    // Source target local(10,-3), projected with the same native cell basis.
    const float targetX=float(b->GetCellCenterX(0,0))+(10.0f+4.8f)*float(kFusionBasePixelsX)-coin->mWidth*.5f;
    const float targetY=float(b->GridToPixelY(0,0)+50)+(2.3f+3.0f)*float(GetNativeFusionPixelsY(b))-coin->mHeight*.5f;
    coin->mPosX=coin->mCollectX+(targetX-coin->mCollectX)*t;
    coin->mPosY=coin->mCollectY+(targetY-coin->mCollectY)*t;coin->mScale=1.0f-.5f*t;
    return true;
}
bool UpdateNativeFusionAdventureReward273(Board* b,FusionAdventureRun273& r)
{
    if(!Playing(b)||!FusionAdventureRunValid273(r)||!r.level)return false;
    if(r.over&&!r.trophyNetId)EnsureNativeFusionAdventureTrophy273(b,r);
    return StepFusionAdventureReward273(r,1);
}
}
