#include "FusionNativeSolar269.h"
#include "FusionNative.h"
#include "FusionNativeLobber.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeDefender.h"
#include "../Plant.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>
namespace LawnFusion
{
namespace
{
bool Host(const Plant* p)
{return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const Plant* p)
{return Host(p)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
}
bool IsNativeSolar269(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&d->kind==934;}
float GetNativeSolar269LobberInterval(Plant* p)
{
    if(!IsNativeSolar269(p))return 0;
    if(NativeSolar269ShootingBoard(p->mBoard))return p->mLaunchRate==200?2.f:1.5f;
    const float interval=FusionSolar269Interval(GetNativeDefenderPotKind(p)==1112);
    if(Host(p))p->mLaunchRate=int(interval*100);return interval;
}
bool UpdateNativeSolar269(Plant* p)
{
    if(!IsNativeSolar269(p))return false;if(!Playing(p))return true;auto* d=GetNativePlantData(p);
    if(p->mTargetX>0)--p->mTargetX;d->batchRageTicks=uint16_t(p->mTargetX);
    if(d->batchMode&&d->shootSeconds<0)d->batchMode=0;
    if(--p->mLaunchCounter<=0)
    {
        ResetNativeLobberLaunchCounter(p);
        if(!d->batchMode&&d->shootSeconds<0)if(auto* z=FindNativeLobberTarget(p,p->mRow))BeginNativeLobberShot(p,z);
    }
    return true;
}
bool HandleNativeSolar269LobberEvent(Plant* p)
{
    if(!IsNativeSolar269(p)||!GetNativePlantData(p)->batchMode)return false;
    if(!Playing(p))return true;auto* d=GetNativePlantData(p);const auto* profile=FindFusionSolar269Timing("super");
    for(std::size_t i=0;i<profile->eventClipSeconds.size();++i)
    {
        bool due=false;if(!TryGetFusionShooterEventAtCounter(*profile,d->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
        d->batchBagCount=i==0?13:0;
        LawnNet::NetTrace("Fusion Solar269 super event plant=%u token=%u sourceStatus=%u backOrder=%d tick=%d",p->mNetId,d->instanceToken,d->batchBagCount,GetNativeSolar269BackOrder(p),p->mBoard->mMainCounter);
    }
    return true;
}
bool CanActivateNativeSolar269Skill(const Plant* p)
{
    if(!IsNativeSolar269(p)||!p->mApp||!p->mBoard||p->mApp->mBoard!=p->mBoard||!IsNativeFusionBoard(p->mBoard)||
       p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0||p->mBoard->mPaused||p->mApp->mGameScene!=SCENE_PLAYING||
       !CanCreateNativeSolar269SuperParticle(p->mBoard))return false;
    FusionSolar269SkillPlan plan;return TryPlanFusionSolar269Skill(GetNativePlantData(p)->batchRageTicks,QueryNativeSolar269WorldBranch(p->mBoard),plan);
}
bool TryActivateNativeSolar269Skill(Plant* p)
{
    if(!Playing(p)||!CanActivateNativeSolar269Skill(p))return false;
    const auto branch=QueryNativeSolar269WorldBranch(p->mBoard);FusionSolar269SkillPlan plan;
    if(!TryPlanFusionSolar269Skill(p->mTargetX,branch,plan))return false;
    int counter=0;auto* d=GetNativePlantData(p);
    if(plan.superAnimation&&!TryGetFusionShooterShotCounter(*FindFusionSolar269Timing("super"),d->animationSpeed,counter))return false;
    float x=0,y=0;if(!p->GetShadowCenter(x,y)||!CreateNativeSolar269SuperParticle(p,x,y-float(.75*GetNativeFusionPixelsY(p->mBoard))))return false;
    p->mPlantHealth=p->mPlantMaxHealth;NativePlantHealthChanged(p);p->mTargetX=plan.cooldownTicks;d->batchRageTicks=uint16_t(plan.cooldownTicks);
    if(plan.superAnimation){p->mShootingCounter=counter;d->batchMode=1;d->shootSeconds=0;}
    CommitNativeSolar269World(p,branch); // Root also maps actual source sound66.
    LawnNet::NetTrace("Fusion Solar269 skill plant=%u token=%u branch=%d health=%d cooldown=%d super=%d tick=%d",p->mNetId,d->instanceToken,int(branch),p->mPlantHealth,p->mTargetX,plan.superAnimation?1:0,p->mBoard->mMainCounter);return true;
}
int GetNativeSolar269SourceStatus(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&d->kind==934?d->batchBagCount:0;}
int GetNativeSolar269BackOrder(const Plant* p){return GetNativeSolar269SourceStatus(p)==13?30:1;}
bool GetNativeSolar269VisualPose(const Plant* p,const char*& state,float& seconds)
{if(!IsNativeSolar269(p))return false;const auto* d=GetNativePlantData(p);state=d->shootSeconds>=0?(d->batchMode?"super":"shoot"):"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return std::isfinite(seconds)&&seconds>=0;}
}
