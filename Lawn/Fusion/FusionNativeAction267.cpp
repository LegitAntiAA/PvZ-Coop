#include "FusionNativeAction267.h"
#include "FusionNative.h"
#include "FusionNativeDefender.h"
#include "FusionNativeDeath.h"
#include "FusionNativeFire.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeMoney.h"
#include "FusionNativePoison.h"
#include "FusionNativeProducer.h"
#include "FusionNativeShooterTiming.h"
#include "FusionAction266Rules.h"
#include "FusionBatchActionRules.h"
#include "FusionBasePlantRules.h"
#include "FusionProducerRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&p->mIsOnBoard&&GetNativePlantData(p);}
bool Host(const Plant* p)
{return Current(p)&&!p->mDead&&!p->mNetGhost&&!p->mApp->NetIsClient()&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
int Rows(Board* b){return b->StageHas6Rows()?6:5;}
bool Live(Plant* p){return Host(p)&&p->mPlantHealth>0;}
void Recover(Plant* p,int amount)
{const int before=p->mPlantHealth;p->mPlantHealth=static_cast<int>((std::min)(int64_t(p->mPlantMaxHealth),int64_t(before)+amount));NativePlantHealthChanged(p);
 LawnNet::NetTrace("Fusion native Action267 regen plant=%u kind=%u amount=%d hp=%d->%d tick=%d",p->mNetId,GetNativePlantData(p)->kind,amount,before,p->mPlantHealth,p->mBoard->mMainCounter);}
void Store(Plant* p,int damage)
{
    auto* d=GetNativePlantData(p);uint32_t after=0;bool explode=false;
    if(!TryAdvanceFusionObsidian267Store(d->batchShotCount,damage,after,explode))return;
    d->batchShotCount=after; // Source clears after the synchronous effect; pre-consume prevents recursion duplication.
    if(!explode)return;float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    NativeBlastSpec blast;blast.centerX=x;blast.centerY=y;blast.row=p->mRow;blast.policy=FusionDeathBlastPolicy::SmallPotato;blast.damage=500;
    ApplyNativeFusionBlast(p->mBoard,blast);
    LawnNet::NetTrace("Fusion native Action267 stored blast plant=%u damage=500 tick=%d",p->mNetId,p->mBoard->mMainCounter);
}
std::vector<uint32_t> Group(Board* b)
{
    std::vector<uint32_t> out;Plant* p=nullptr;
    // Source world913/925 type chains, not the independent same-column connection.
    while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(Live(p)&&d&&(d->kind==913||d->kind==925))out.push_back(b->mPlants.DataArrayGetID(p));}
    return out;
}
bool Producer(Plant* p)
{
    auto* d=GetNativePlantData(p);const auto kind=d->kind;if(kind!=936&&kind!=1077)return false;
    if(!Host(p)||p->mPlantHealth<=0)return true;
    if(d->pendingProductionTicks>0&&!--d->pendingProductionTicks)
    {
        bool emitted=false;
        if(kind==936)
        {
            if(FusionProducerCanFitCallback(p->mBoard->mCoins.mSize,p->mBoard->mCoins.mMaxSize,3))
                emitted=EmitNativeMoneyItem(p->mBoard,float(p->mX+20),float(p->mY+20),34)&&EmitNativeSunBatch(p,2,true);
        }
        else {uint16_t item=0;if(TryGetFusionSeaSun267Item(d->batchMode,item))emitted=EmitNativeSourceSun(p,item);}
        if(!emitted){d->pendingProductionTicks=1;return true;}
        LawnNet::NetTrace("Fusion native Action267 produce plant=%u kind=%u stage=%u tick=%d",p->mNetId,kind,d->batchMode,p->mBoard->mMainCounter);
    }
    const bool due=kind==936?--p->mLaunchCounter<=0:--p->mLaunchCounter<0;
    if(due)
    {
        if(kind==936)TryGetFusionSolar267Cycle(p->mBoard->mSunMoney,RandRangeFloat(.9f,1.1f),p->mLaunchCounter);
        else p->mLaunchCounter=(15+RandRangeInt(-2,2))*100+1;
        d->pendingProductionTicks=50;
    }
    d->productionTicks=p->mLaunchCounter;
    return true;
}
bool RockHit(Plant* p,bool apply=true)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
    const double py=GetNativeFusionPixelsY(p->mBoard);
    const FusionBatchArea area{x-kFusionBasePixelsX*.5,y-py*.5,kFusionBasePixelsX,py};
    std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(z->mDead||z->IsDeadOrDying()||z->mMindControlled||!IsNativeFusionGroundZombie(z)||z->mRow!=p->mRow||z->IsFlying())continue;
        const auto r=z->GetZombieRect();if(FusionBatchAreaTouchesRect(area,r.mX,r.mY,r.mWidth,r.mHeight))targets.push_back(p->mBoard->ZombieGetID(z));
    }
    if(!apply)return !targets.empty();
    for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id))&&!z->mDead)z->TakeDamage(100,1U);
    LawnNet::NetTrace("Fusion native Action267 rock plant=%u hits=%zu tick=%d",p->mNetId,targets.size(),p->mBoard->mMainCounter);
    return !targets.empty();
}
}
bool InitializeNativeAction267(Plant* p)
{
    if(!Host(p))return false;auto* d=GetNativePlantData(p);if(!IsFusionAction267Kind(d->kind))return false;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->shootSeconds=-1;d->batchMode=0;d->batchShotCount=0;
    p->mState=STATE_NOTREADY;p->mStateCountdown=p->mDoSpecialCountdown=p->mDisappearCountdown=p->mShootingCounter=0;p->mTargetX=0;
    if(IsFusionObsidian267Kind(d->kind))p->mTargetX=1500;
    if(d->kind==906)
    {p->mPlantMaxHealth=p->mPlantHealth=8000;InitializeNativeShooterTiming(p,*FindFusionAction267TimingProfile(906),true);}
    else if(d->kind==931||d->kind==913||d->kind==925||d->kind==936)InitializeNativeDefender(p);
    if(d->kind==1077||d->kind==936)
    {p->mLaunchCounter=static_cast<int>(std::ceil(static_cast<double>(RandRangeFloat(4,7))*100))+1;d->productionTicks=p->mLaunchCounter;d->pendingProductionTicks=0;}
    if(d->kind==1077){p->mTargetX=2000;p->SetSleeping(false);p->mWakeUpCounter=0;}
    if(d->kind==1181||d->kind==1176||d->kind==1086)
    {p->mDoSpecialCountdown=static_cast<int>(std::ceil(static_cast<double>(.48333332f)/(d->kind==1086?1:d->animationSpeed)*100));d->shootSeconds=0;}
    return true;
}
bool UpdateNativeAction267(Plant* p)
{
    if(!Current(p))return false;auto* d=GetNativePlantData(p);const auto kind=d->kind;if(!IsFusionAction267Kind(kind))return false;
    if(!Host(p))return true;
    if(IsFusionObsidian267Kind(kind)&&p->mTargetX>0&&!--p->mTargetX){p->mTargetX=1500;Recover(p,kind==906?500:1500);}
    if(kind==1077&&p->mTargetX>0&&!--p->mTargetX)
    {if(d->batchMode<2){++d->batchMode;p->mStateCountdown=100;d->shootSeconds=0;p->mTargetX=d->batchMode==1?4000:0;}}
    if(Producer(p))return true;
    if(kind==906)
    {
        if(--p->mLaunchCounter<=0)
        {p->mLaunchCounter=static_cast<int>(std::ceil(static_cast<double>(RandRangeFloat(.9f,1.1f))*100));
         if(d->shootSeconds<0&&RockHit(p,false)){BeginNativeShooterShot(p,*FindFusionAction267TimingProfile(kind));}}
    }
    if((kind==1181||kind==1176||kind==1086)&&d->batchMode)
    {
        if(!p->mStateCountdown){p->Die();return true;}
        if(kind==1176)
        {
            float px=0,py=0;if(p->GetShadowCenter(px,py))
            {Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
             {float zx=0,zy=0;if(!z->mDead&&z->mMindControlled&&IsNativeFusionGroundZombie(z)&&z->GetShadowCenter(zx,zy)&&zx>px)
              {z->mPosX-=static_cast<float>(.02*kFusionBasePixelsX);z->mX=static_cast<int>(z->mPosX);}}}
        }
    }
    return true;
}
bool UpdateNativeAction267Phase(Plant* p)
{
    if(!Host(p))return false;auto* d=GetNativePlantData(p);if(!IsFusionAction267Kind(d->kind))return false;
    if(d->kind==906){UpdateNativeShooterPhase(p,*FindFusionAction267TimingProfile(906));return true;}
    d->idleSeconds+=.01f;
    if(d->kind==1077){d->shootSeconds=p->mStateCountdown>0?(100-p->mStateCountdown)*.01f:-1;return true;}
    if(d->shootSeconds>=0&&!d->batchMode)d->shootSeconds+=.01f;
    return true;
}
bool DispatchNativeAction267Events(Plant* p)
{
    if(!Current(p)||GetNativePlantData(p)->kind!=906)return false;if(!Host(p))return true;
    const auto* clock=FindFusionAction267TimingProfile(906);auto* d=GetNativePlantData(p);
    for(size_t i=0;i<clock->eventClipSeconds.size();++i){bool due=false;if(TryGetFusionShooterEventAtCounter(*clock,d->animationSpeed,p->mShootingCounter,i,due)&&due)RockHit(p);}return true;
}
bool ApplyNativeAction267Special(Plant* p)
{
    if(!Host(p))return false;auto* d=GetNativePlantData(p);if(d->kind!=1181&&d->kind!=1176&&d->kind!=1086)return false;
    if(d->batchMode)return true;d->batchMode=1;d->shootSeconds=-1;p->mState=STATE_DOINGSPECIAL;
    p->mStateCountdown=d->kind==1176?500:200;p->mDisappearCountdown=p->mStateCountdown+1;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        if(z->mDead||z->mMindControlled)continue;
        if(z->IsFlying()||(d->kind==1086&&z->mRow==p->mRow))z->mBlowingAway=true;
        if(d->kind==1181&&IsNativeFusionGroundZombie(z))
        {auto* poison=GetOrCreateNativeZombieActionData(z);if(poison){poison->poisonLevel=(std::max)(poison->poisonLevel,5);AddNativePoisonLevel(z);}BeginNativeGarliced(z,false);}
    }
    p->mBoard->mFogBlownCountDown=4000;p->mApp->PlaySample(Sexy::SOUND_BLOVER);
    return true;
}
bool GetNativeAction267VisualPose(const Plant* p,const char*& state,float& elapsed)
{
    if(!Current(p))return false;const auto* d=GetNativePlantData(p);if(!IsFusionAction267Kind(d->kind))return false;
    if(d->kind==906){state=d->shootSeconds>=0?"attack":"idle";elapsed=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return true;}
    if(d->kind==1077){state=p->mStateCountdown>0?(d->batchMode==1?"grow1":"grow2"):d->batchMode==0?"idle":d->batchMode==1?"idle1":"idle2";elapsed=p->mStateCountdown>0?d->shootSeconds:d->idleSeconds;return true;}
    if(d->kind==1181||d->kind==1176||d->kind==1086){state=d->batchMode?"blowloop":"blow";elapsed=d->batchMode?d->idleSeconds:d->shootSeconds;return true;}
    state="idle";elapsed=d->idleSeconds;return true;
}
bool TryHandleNativeDefender267Damage(Plant* p,int& damage,int type)
{
    if(!Live(p)||damage<0)return false;auto* d=GetNativePlantData(p);const auto kind=d->kind;
    if(!IsFusionObsidian267Kind(kind))return false;
    if(kind==925&&damage>0)Store(p,damage);
    const auto victims=kind==913||kind==925?Group(p->mBoard):std::vector<uint32_t>();
    int applied=damage;bool shared=false;if(!TryGetFusionObsidian267Damage(kind,damage,type,static_cast<int>(victims.size()),applied,shared))return false;
    damage=applied;if(!shared)return false;
    for(auto id:victims){auto* other=p->mBoard->mPlants.DataArrayTryToGet(id);if(Live(other))ApplyNativeSourcePlantDamage(other,applied,3);}
    return true;
}
void NativeAction267HealthChanged(Plant* p)
{if(Host(p)&&GetNativePlantData(p)->kind==906){auto* d=GetNativePlantData(p);TryGetFusionAction266RockStage(p->mPlantHealth,p->mPlantMaxHealth,d->damageStage);}}
bool HandleNativeAction267Crush(Plant* p)
{
    if(!Host(p)||p->mDead||GetNativePlantData(p)->kind!=906)return false;
    p->mSquished=false;p->mPlantHealth-=50;p->mEatenFlashCountdown=(std::max)(p->mEatenFlashCountdown,25);
    NativePlantHealthChanged(p);NativeAction267HealthChanged(p);if(p->mPlantHealth<=0)p->Die();return true;
}
}
