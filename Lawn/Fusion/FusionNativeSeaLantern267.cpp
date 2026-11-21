#include "FusionNativeSeaLantern267.h"
#include "FusionSeaLantern267Rules.h"
#include "FusionNative.h"
#include "../Plant.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    return p&&d&&d->kind==1078&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&
        IsNativeFusionBoard(p->mBoard)&&p->mIsOnBoard&&!p->mDead&&p->mPlantHealth>0&&
        p->mSeedType==GetNativeSeedForPlant(1078);
}
bool Host(const Plant* p){return Current(p)&&!p->mNetGhost&&!p->mApp->NetIsClient();}
}
bool InitializeNativeSeaLantern267Plant(Plant* p)
{
    if(!Current(p))return false;if(!Host(p))return true;
    auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(1078);
    if(!definition||!definition->hasStats)return false;
    p->mPlantMaxHealth=p->mPlantHealth=definition->maxHealth;
    p->mStateCountdown=kFusionSeaLantern267GrowthTicks;p->mState=STATE_NOTREADY;
    p->mDoSpecialCountdown=p->mDisappearCountdown=p->mShootingCounter=0;
    p->SetSleeping(false);p->mWakeUpCounter=0;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->shootSeconds=-1;d->idleSeconds=0;
    d->batchShotCount=0;d->batchRageTicks=d->batchBagCount=0;d->batchMode=0;
    d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=0;
    LawnNet::NetTrace("Fusion native SeaLantern267 init plant=%u token=%u growth=%d radius=1 tick=%d sourceFX40=Pending",
        p->mNetId,d->instanceToken,p->mStateCountdown,p->mBoard->mMainCounter);
    return true;
}
bool UpdateNativeSeaLantern267Plant(Plant* p)
{
    if(!Current(p))return false;if(!Host(p)||!CanApplyNativeFusionGameplay(p->mBoard)||p->mSquished)return true;
    auto* d=GetNativePlantData(p);
    // Plant::Update already decremented the single native attribute countdown.
    if(!d->batchMode&&!p->mStateCountdown)
    {
        d->batchMode=1;d->shootSeconds=0;
        LawnNet::NetTrace("Fusion native SeaLantern267 grew plant=%u token=%u radius=2 tick=%d sourceFX40=Pending",
            p->mNetId,d->instanceToken,p->mBoard->mMainCounter);
    }
    d->productionTicks=p->mStateCountdown;return true;
}
bool UpdateNativeSeaLantern267Phase(Plant* p)
{
    if(!Current(p))return false;if(!Host(p)||!CanApplyNativeFusionGameplay(p->mBoard)||p->mSquished)return true;
    auto* d=GetNativePlantData(p);d->idleSeconds+=.01f;
    if(d->shootSeconds>=0)
    {d->shootSeconds+=.01f;if(d->shootSeconds>=1.f/d->animationSpeed)d->shootSeconds=-1;}
    return true;
}
bool GetNativeSeaLantern267VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!Current(p))return false;const auto* d=GetNativePlantData(p);
    state=d->batchMode?"idle2":"idle";seconds=d->idleSeconds;
    if(d->batchMode&&d->shootSeconds>=0){state="grow";seconds=d->shootSeconds;}
    return true;
}
int NativeSeaLantern267LightRadius(const Plant* p)
{return Current(p)?FusionSeaLantern267LightRadius(GetNativePlantData(p)->batchMode):0;}
}
