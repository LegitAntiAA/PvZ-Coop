#include "FusionNativeEconomy.h"
#include "FusionNative.h"
#include "FusionNativeProducer.h"
#include "FusionNativeDefender.h"
#include "../Plant.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
namespace LawnFusion
{
namespace
{
NativePlantData* Host(Plant* p)
{
    auto* d=GetNativePlantData(p);
    return d&&IsFusionEconomyBatchKind(d->kind)&&p->mApp&&!p->mApp->NetIsClient()&&!p->mNetGhost&&
        p->mApp->mBoard==p->mBoard&&!p->mDead&&p->mIsOnBoard?d:nullptr;
}
}
bool InitializeNativeEconomyPlant(Plant* p)
{
    const auto* current=GetNativePlantData(p);
    if(!current||!IsFusionEconomyBatchKind(current->kind))return false;
    auto* d=Host(p);if(!d)return true;
    if(IsNativeDefender(p))InitializeNativeDefender(p);
    d->animationSpeed=RandRangeFloat(.9f,1.1f);
    InitializeNativeProducer(p);
    d->batchShotCount=0;d->batchRageTicks=d->batchBagCount=0;d->batchMode=0;
    p->SetSleeping(false);p->mWakeUpCounter=0;p->mDoSpecialCountdown=0;
    if(d->kind==1031)
    {
        p->mStateCountdown=kFusionSunShroomGrowthTicks;
        p->mState=STATE_SUNSHROOM_SMALL;
    }
    else if(d->kind==1188)
    {
        p->mStateCountdown=kFusionGoldSunflowerInitialTicks;
        p->mLaunchCounter=0;p->mLaunchRate=0;p->mTargetX=-1;
        d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=0;
    }
    LawnNet::NetTrace("Fusion native Economy init plant=%u kind=%u attribute=%d producer=%d tick=%d",
        p->mNetId,d->kind,p->mStateCountdown,d->productionTicks,p->mBoard->mMainCounter);
    return true;
}
bool UpdateNativeEconomyPlant(Plant* p)
{
    const auto* current=GetNativePlantData(p);
    if(!current||!IsFusionEconomyBatchKind(current->kind))return false;
    auto* d=Host(p);
    if(!d||p->mBoard->mPaused||p->mApp->mGameScene!=SCENE_PLAYING||p->mPlantHealth<=0||p->mSquished)return true;
    // Plant::Update has already advanced the actual attribute countdown once.
    if(d->kind==1031&&d->batchMode==0&&p->mStateCountdown==0)
    {
        d->batchMode=1;d->shootSeconds=0;p->mState=STATE_SUNSHROOM_BIG;
        LawnNet::NetTrace("Fusion native Economy grew plant=%u kind=1031 tick=%d",p->mNetId,p->mBoard->mMainCounter);
    }
    if(d->kind!=1188){UpdateNativeProducer(p);return true;}
    if(p->mStateCountdown==0)
    {
        if(d->batchShotCount<kFusionGoldSunflowerMaximumCharges)++d->batchShotCount;
        p->mStateCountdown=kFusionGoldSunflowerCycleTicks;
        LawnNet::NetTrace("Fusion native Economy charged plant=%u count=%u tick=%d",p->mNetId,d->batchShotCount,p->mBoard->mMainCounter);
    }
    // The command can start this native countdown earlier in the same Board
    // tick. Advance at most once on each subsequent simulation tick.
    if(d->batchMode&&p->mTargetX!=p->mBoard->mMainCounter)
    {
        p->mTargetX=p->mBoard->mMainCounter;
        if(p->mLaunchCounter>0)--p->mLaunchCounter;
        if(!p->mLaunchCounter)
        {
            if(!d->batchShotCount)d->batchMode=0;
            else if(EmitNativeSourceSun(p,1)){--d->batchShotCount;p->mLaunchCounter=10;}
            else p->mLaunchCounter=1;
        }
    }
    d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=p->mLaunchCounter;
    return true;
}
bool TryActivateNativeEconomySkill(Plant* p)
{
    auto* d=Host(p);
    if(!d||(d->kind!=1188&&d->kind!=1137)||p->mEatenFlashCountdown>0||
        !CanApplyNativeFusionGameplay(p->mBoard))return false;
    if(d->kind==1188&&d->batchShotCount&&
        !FusionProducerCanFitCallback(p->mBoard->mCoins.mSize,p->mBoard->mCoins.mMaxSize,1))return false;
    p->mPlantHealth=p->mPlantMaxHealth;
    if(d->kind==1137)p->mEatenFlashCountdown=200;
    else
    {
        if(d->batchShotCount)
        {
            if(!EmitNativeSourceSun(p,1))return false;
            --d->batchShotCount;d->batchMode=1;p->mLaunchCounter=10;
            p->mTargetX=p->mBoard->mMainCounter;
        }
        p->mEatenFlashCountdown=static_cast<int>(d->batchShotCount)*10;
        d->pendingProductionTicks=p->mLaunchCounter;
    }
    LawnNet::NetTrace("Fusion native Economy skill plant=%u kind=%u charges=%u pending=%d flash=%d tick=%d",
        p->mNetId,d->kind,d->batchShotCount,p->mLaunchCounter,p->mEatenFlashCountdown,p->mBoard->mMainCounter);
    return true;
}
bool UpdateNativeEconomyPhase(Plant* p)
{
    const auto* current=GetNativePlantData(p);
    if(!current||!IsFusionEconomyBatchKind(current->kind))return false;
    auto* d=Host(p);if(!d)return true;
    d->idleSeconds+=.01f;
    if(d->kind==1031&&d->shootSeconds>=0)
    {
        d->shootSeconds+=.01f;
        if(d->shootSeconds>=.5f/d->animationSpeed)d->shootSeconds=-1;
    }
    return true;
}
bool GetNativeEconomyVisualPose(const Plant* p,const char*& state,float& seconds)
{
    const auto* d=GetNativePlantData(p);
    if(!d||d->kind!=1031)return false;
    state=d->batchMode?"idle2":"idle";seconds=d->idleSeconds;
    if(d->batchMode&&d->shootSeconds>=0&&d->shootSeconds<.5f/d->animationSpeed)
    {state="grow";seconds=d->shootSeconds;}
    return true;
}
}
