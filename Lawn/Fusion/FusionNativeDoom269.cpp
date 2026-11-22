#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeDoom269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeCold.h"
#include "FusionNativeWater268.h"
#include "FusionBasePlantRules.h"
#include "FusionFireActionBatchRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
bool Host(const Plant* p)
{return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const Plant* p)
{return Host(p)&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
bool LiveTarget(Zombie* z)
{return z&&!z->mDead&&!z->mNetGhost&&GetNativeFusionZombieHealth(z)>0&&!NativeWater268ZombieCaptured(z);}
bool Land(Zombie* z)
{return LiveTarget(z)&&FusionDoom269LandTarget(GetNativeFusionZombieStatus(z),z->mMindControlled);}
bool Circle(Plant* p,NativeDoom269Point center,double radius,Zombie* z)
{
    const auto r=z->GetZombieRect();const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
    return FusionFireCircleTouchesRect(center.x,center.y,radius,r.mX,center.y+(r.mY-center.y)*ratio,r.mWidth,r.mHeight*ratio);
}
void IceHit(Plant* p,NativeDoom269Point center,double radius,bool adjacentRows,bool reverse)
{
    const int damage=GetNativeTravelPlantAttack369(p,FusionFindPlant(910)->damage);std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(Land(z)&&(!adjacentRows||std::abs(z->mRow-p->mRow)<2)&&Circle(p,center,radius,z))targets.push_back(p->mBoard->ZombieGetID(z));
    if(reverse)std::reverse(targets.begin(),targets.end());
    const FusionColdDamageContext cold{0,FusionColdDamageDelivery::IceAll};
    for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id))&&Land(z))
    {z->TakeDamage(damage,2U,&cold);AddNativeColdLevel(z,kFusionIceDoom269FreezeLevel);}
    LawnNet::NetTrace("Fusion Doom269 IceHit plant=%u token=%u radius=%.3f adjacent=%d x=%.9g y=%.9g targets=%zu damage=%d tick=%d",p->mNetId,GetNativePlantData(p)->instanceToken,radius,adjacentRows?1:0,center.x,center.y,targets.size(),damage,p->mBoard->mMainCounter);
    if(adjacentRows&&!targets.empty()){(void)RandRangeInt(0,2);p->mApp->PlayFoley(FOLEY_FUME);}
}
bool HasTarget(Plant* p)
{
    const auto kind=GetNativePlantData(p)->kind;float x=0,y=0;
    if(kind==910)
    {
        if(!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return false;Zombie* z=nullptr;
        while(p->mBoard->IterateZombies(z))if(Land(z)&&std::abs(z->mRow-p->mRow)<2&&Circle(p,{x,y},4,z))return true;
        return false;
    }
    if(!p->GetShadowCenter(x,y))return false;
    const double limit=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        float zx=0,zy=0;if(!LiveTarget(z)||z->mRow!=p->mRow||!FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),z->mMindControlled,z->IsDeadOrDying())||!z->GetShadowCenter(zx,zy))continue;
        if(zx>x&&zx<limit)return true;
    }
    return false;
}
bool Fear(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;const double py=GetNativeFusionPixelsY(p->mBoard);
    Sexy::Rect area(int(std::floor(x-1.25*kFusionBasePixelsX)),int(std::floor(y-1.75*py)),int(std::ceil(2.5*kFusionBasePixelsX)),int(std::ceil(2.5*py)));
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        if(LiveTarget(z)&&FusionSuperDoom269FearTarget(GetNativeFusionZombieStatus(z),z->mMindControlled)&&area.Intersects(z->GetZombieRect()))return true;
    return false;
}
void Pending(Plant* p)
{
    auto* clouds=EditNativeDoom269Clouds(p);if(!clouds)return;
    const uint32_t token=GetNativePlantData(p)->instanceToken;std::vector<NativeDoom269Cloud> due;
    for(auto it=clouds->begin();it!=clouds->end();)
    {
        if(it->sourceToken!=token){it=clouds->erase(it);continue;}
        if(p->mBoard->mMainCounter>=it->deadline){due.push_back(*it);it=clouds->erase(it);}else ++it;
    }
    // Erase admitted callbacks before synchronous damage/death/replacement.
    for(const auto& cloud:due)
    {
        if(p->mDead||GetNativePlantData(p)->instanceToken!=cloud.sourceToken)break;
        p->mApp->PlayFoley(FOLEY_FUME);
        for(const auto center:cloud.centers){if(p->mDead)break;IceHit(p,center,1.5,false,false);}
    }
}
bool Cloud(Plant* p)
{
    auto* queue=EditNativeDoom269Clouds(p,true);if(!queue||queue->size()>=4)return false;
    NativeDoom269Cloud cloud;cloud.sourceToken=GetNativePlantData(p)->instanceToken;
    float x=0,y=0;if(!TryMakeFusionIceDoom269Deadline(p->mBoard->mMainCounter,cloud.deadline)||!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return false;
    if(!CreateNativeIceDoom269Cloud(p,x,y,cloud.centers))
    {LawnNet::NetTrace("Fusion Doom269 BLOCKED actual source particle52/8child births unavailable plant=%u",p->mNetId);return false;}
    for(const auto point:cloud.centers)if(!std::isfinite(point.x+point.y)||std::abs(point.x)>2048||std::abs(point.y)>2048)return false;
    queue->push_back(cloud);IceHit(p,{x,y},4,true,true);
    LawnNet::NetTrace("Fusion Doom269 cloud plant=%u token=%u birth=%d deadline=%d centers=8 pending=%zu",p->mNetId,cloud.sourceToken,p->mBoard->mMainCounter,cloud.deadline,queue->size());return true;
}
bool Shoot(Plant* p)
{
    auto* state=EditNativeDoom269PlantState(p);if(!state)return false;const auto* definition=FusionFindPlant(930);
    FusionSuperDoom269Shot plan;
    if(!TryPrepareFusionSuperDoom269Shot(state->interval,state->ordinal,definition->damage,HasNativeDoom269Trait(p,NativeDoom269Trait::Basic3),HasNativeDoom269Trait(p,NativeDoom269Trait::Advanced2),plan))return false;
    float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"Shoot/ScaredyShroom_mouth",x,y)||!EnsureNativeProjectileVisuals(p->mBoard,23)||p->mBoard->mProjectiles.mSize>=p->mBoard->mProjectiles.mMaxSize)return false;
    x+=float(.1*kFusionBasePixelsX);y+=float(.2*GetNativeFusionPixelsY(p->mBoard));
    auto* q=p->mBoard->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,p->mRow,PROJECTILE_PEA);if(!q)return false;
    q->mMotionType=MOTION_STRAIGHT;q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=0;q->mShadowY=y;q->mX=int(q->mPosX);q->mY=int(q->mPosY);
    q->mVelX=float(6*kFusionBasePixelsX*.01);q->mVelY=q->mVelZ=q->mAccZ=0;q->mDamageRangeFlags=p->GetDamageRangeFlags(WEAPON_PRIMARY);
    NativeProjectileData data;data.kind=23;data.damage=plan.damage;data.effectVariant=plan.enhanced?1:0;
    if(!AttachNativeTravelProjectile369(p,q,data)){q->Die();return false;}
    state=EditNativeDoom269PlantState(p);state->interval=plan.interval;state->ordinal=plan.ordinal;
    if(plan.overrideRecovery)p->mLaunchCounter=int(std::floor(double(plan.recoverySeconds)*100))+1;
    ++GetNativePlantData(p)->batchShotCount;(void)RandRangeInt(3,4);p->mApp->PlayFoley(FOLEY_THROW);
    LawnNet::NetTrace("Fusion Doom269 shot plant=%u projectile=%u ordinal=%d damage=%d enhanced=%d interval=%.9g recoveryTicks=%d tick=%d",p->mNetId,q->mNetId,state->ordinal,plan.damage,plan.enhanced?1:0,state->interval,p->mLaunchCounter,p->mBoard->mMainCounter);return true;
}
}
bool IsNativeDoom269Plant(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&(d->kind==910||d->kind==930);}
bool InitializeNativeDoom269Plant(Plant* p)
{
    if(!IsNativeDoom269Plant(p))return false;if(!Host(p))return true;
    const auto kind=GetNativePlantData(p)->kind;auto* s=EditNativeDoom269PlantState(p,true);if(!s)return false;
    *s={};s->interval=FusionFindPlant(kind)->attackIntervalSeconds;p->mState=STATE_NOTREADY;p->mStateCountdown=p->mDoSpecialCountdown=p->mShootingCounter=0;
    if(kind==910&&HasNativeDoom269Trait(p,NativeDoom269Trait::EveningBalanced))p->mPlantHealth=p->mPlantMaxHealth=2000;
    return InitializeNativeShooterTiming(p,*FindFusionDoom269Timing(kind),true);
}
bool UpdateNativeDoom269Plant(Plant* p)
{
    if(!IsNativeDoom269Plant(p))return false;if(!Playing(p)||p->mDead)return true;
    auto* state=EditNativeDoom269PlantState(p);if(!state)return true;
    if(state->lastUpdateTick==p->mBoard->mMainCounter)return true;state->lastUpdateTick=p->mBoard->mMainCounter;
    const auto kind=GetNativePlantData(p)->kind;if(kind==910)Pending(p);
    if(p->mDead)return true;
    if(kind==930&&Fear(p))
    {ApplyNativeDoom269World(p,1800,false,true);if(!p->mDead)p->Die();return true;}
    auto timing=*FindFusionDoom269Timing(kind);timing.attackIntervalSeconds=state->interval;
    bool trigger=false;
    if(--p->mLaunchCounter<=0){ResetNativeShooterLaunchCounter(p,timing);trigger=HasTarget(p);}
    if(kind==910){if(trigger&&GetNativePlantData(p)->shootSeconds<0)BeginNativeShooterShot(p,timing);return true;}
    int initial=0;if(!TryGetFusionShooterShotCounter(timing,GetNativePlantData(p)->animationSpeed,initial))return true;
    const int eventDeadline=int(std::ceil(timing.eventClipSeconds[0]/double(GetNativePlantData(p)->animationSpeed)*100));
    FusionSelfTransition269Plan plan;
    if(!TryPlanFusionSelfTransition269({p->mShootingCounter,p->mStateCountdown,p->mDoSpecialCountdown,state->queuedTrigger},trigger,true,initial,initial-eventDeadline,25,plan))
    {LawnNet::NetTrace("Fusion Doom269 transition rejected plant=%u incoming=%d lock=%d outgoing=%d",p->mNetId,p->mShootingCounter,p->mStateCountdown,p->mDoSpecialCountdown);return true;}
    p->mShootingCounter=plan.counters.incoming;p->mStateCountdown=plan.counters.lock;p->mDoSpecialCountdown=plan.counters.outgoing;state->queuedTrigger=plan.counters.queued;
    if(plan.outgoingDue)Shoot(p);
    if(plan.begin){BeginNativeShooterShot(p,timing);LawnNet::NetTrace("Fusion Doom269 transition plant=%u incoming=%d lock=%d outgoing=%d tick=%d",p->mNetId,p->mShootingCounter,p->mStateCountdown,p->mDoSpecialCountdown,p->mBoard->mMainCounter);}
    return true;
}
void UpdateNativeDoom269Phase(Plant* p)
{if(IsNativeDoom269Plant(p)&&Playing(p)&&!p->mDead)UpdateNativeShooterPhase(p,*FindFusionDoom269Timing(GetNativePlantData(p)->kind));}
bool DispatchNativeDoom269Events(Plant* p)
{
    if(!IsNativeDoom269Plant(p))return false;if(!Playing(p)||p->mDead)return true;
    auto* d=GetNativePlantData(p);const auto* timing=FindFusionDoom269Timing(d->kind);int initial=0,events=0;
    if(!TryGetFusionShooterShotCounter(*timing,d->animationSpeed,initial)||!TryGetFusionShooterEventsAtCounter(*timing,d->animationSpeed,p->mShootingCounter,events))return true;
    if(events)d->shootSeconds=(initial-p->mShootingCounter)*.01f;
    for(int i=0;i<events;++i){if(d->kind==910){if(Cloud(p))++d->batchShotCount;}else Shoot(p);}return true;
}
bool GetNativeDoom269VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!IsNativeDoom269Plant(p))return false;const auto* d=GetNativePlantData(p);
    state=d->shootSeconds>=0?(d->kind==910?"attack":"shoot"):"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return std::isfinite(seconds)&&seconds>=0;
}
void NativeDoom269PlantRemoving(Plant* p,int reason)
{
    if(!Host(p)||!IsNativeDoom269Plant(p))return;if(auto* q=EditNativeDoom269Clouds(p))q->clear();
    if(reason<0||reason>7)return;const auto kind=GetNativePlantData(p)->kind;
    if(kind==910)ApplyNativeDoom269World(p,FusionIceDoom269DeathDamage(HasNativeDoom269Trait(p,NativeDoom269Trait::Expert10)),true,false);
    else if(HasNativeDoom269Trait(p,NativeDoom269Trait::EveningBalanced))ApplyNativeDoom269World(p,1800,false,false);
}
bool TryHandleNativeDoom269Damage(Plant* p,int damage,int type)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=910||!Host(p)||p->mDead)return false;
    auto* state=EditNativeDoom269PlantState(p);if(!state)return false;FusionIceDoom269Damage plan;
    const bool trait=HasNativeDoom269Trait(p,NativeDoom269Trait::Expert11);
    if(!TryPrepareFusionIceDoom269Damage(state->power,damage,type,trait,plan))return trait&&(type==1||type==2);
    if(!plan.absorbed)return false;
    state->power=plan.power;if(plan.doom)ApplyNativeDoom269World(p,1800,false,false);return true;
}
}
