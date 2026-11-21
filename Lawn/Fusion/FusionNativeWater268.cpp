#include "FusionNativeWater268.h"
#include "FusionWater268Rules.h"
#include "FusionNative.h"
#include "FusionNativeCold.h"
#include "FusionNativeFire.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativePlacement.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeVisual.h"
#include "FusionBasePlantRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionShooter267Rules.h"
#include "FusionRecipientShooterRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <limits>
#include <unordered_set>
namespace LawnFusion
{
// Root owns this canonical source damage-type11 route.
void ApplyNativeSourceSquashDamage(Zombie* zombie,int damage);
namespace
{
bool Current(const Plant* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&GetNativePlantData(p)&&p->mIsOnBoard;}
bool Host(const Plant* p)
{return Current(p)&&!p->mDead&&!p->mNetGhost&&!p->mApp->NetIsClient()&&CanApplyNativeFusionGameplay(p->mBoard);}
bool KelpImplemented(uint16_t k){return k==15||k==1049||k==1156||k==1050||k==1051||k==1056||k==1066;}
bool Sea(uint16_t k){return k==1076||k==1083;}
Zombie* CapturedTarget(const Plant* p)
{
    if(!Current(p))return nullptr;const auto* d=GetNativePlantData(p);
    if(!d->waterTargetId||!d->waterTargetToken)return nullptr;
    if(!p->mApp->NetIsClient())
    {auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);return z&&z->mNetId==d->waterTargetId&&unsigned(p->mTargetZombieID)==d->waterTargetToken?z:nullptr;}
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(!z->mDead&&z->mNetId==d->waterTargetId)return z;
    return nullptr; // Host token is opaque here; raw/FNS binding validates it.
}
bool ProjectileTarget(Zombie* z,bool retain)
{return z&&!z->mDead&&FusionWater268HomingTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,
    !NativeWater268ZombieCaptured(z),z->mBodyHealth>0,retain);}
bool Touches(double x,double y,double w,double h,const Sexy::Rect& r)
{return x-w*.5<=r.mX+r.mWidth&&x+w*.5>=r.mX&&y-h*.5<=r.mY+r.mHeight&&y+h*.5>=r.mY;}
Zombie* Nearest(Projectile* p)
{
    Zombie* out=nullptr;Zombie* z=nullptr;double best=0;bool air=false;
    const double py=GetNativeFusionPixelsY(p->mBoard),vision=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX;
    while(p->mBoard->IterateZombies(z))
    {
        const bool flying=z->IsFlying();
        if(!ProjectileTarget(z,flying)||(GetNativeProjectileData(p)->kind==42&&!FusionWater268StarTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,!NativeWater268ZombieCaptured(z),z->mBodyHealth>0)))continue;
        float sx=0,sy=0;if(!z->GetShadowCenter(sx,sy)||sx>=vision)continue;
        if(air&&!flying)continue;
        const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=(r.mY+r.mHeight*.5-p->mPosY-20)/py;
        const double distance=dx*dx+dy*dy;
        if(!out||(flying&&!air)||distance<best){out=z;best=distance;air=flying;}
    }
    return out;
}
Zombie* PlantAim(Plant* p)
{
    Zombie* z=nullptr;Zombie* first=nullptr;const auto* d=GetNativePlantData(p);const auto k=d->kind;
    float px=0,py=0;p->GetShadowCenter(px,py);const double sceneY=GetNativeFusionPixelsY(p->mBoard);
    const double vision=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX;
    if(k==1085)
    {
        while(p->mBoard->IterateZombies(z))
        {
            float x=0,y=0;
            if(!z->mDead&&FusionWater268StarTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,!NativeWater268ZombieCaptured(z),z->mBodyHealth>0)&&z->GetShadowCenter(x,y)&&x<vision)return z;
        }
        return nullptr;
    }
    if(k==1083)
    {
        for(size_t ray=0;ray<5;++ray)
        {
            Zombie* nearest=nullptr;double best=0;z=nullptr;
            while(p->mBoard->IterateZombies(z))
            {
                const int s=GetNativeFusionZombieStatus(z);
                if(z->mDead||z->mMindControlled||z->mBodyHealth<=0||NativeWater268ZombieCaptured(z)||s==1||s==3||s==7||s==9||s==10||s==12||s==16||s>=20)continue;
                const auto r=z->GetZombieRect();double distance=0;
                const FusionRecipientRect projected{double(r.mX),py+(r.mY-py)*kFusionBasePixelsY/sceneY,double(r.mWidth),r.mHeight*kFusionBasePixelsY/sceneY};
                if(TryGetFusionStarAcquisitionDistance(ray,z->mRow==p->mRow,px,py-.3*kFusionBasePixelsY,projected,distance)&&(!nearest||distance<best)){nearest=z;best=distance;}
            }
            if(nearest)return nearest;
        }
        return nullptr;
    }
    while(p->mBoard->IterateZombies(z))
    {
        if(!z||z->mDead||z->mMindControlled||z->mBodyHealth<=0||NativeWater268ZombieCaptured(z))continue;
        const int status=GetNativeFusionZombieStatus(z);
        if(status==1||status==3||status==7||status==9||status==12||(status>=20&&status<=24))continue;
        float x=0,y=0;if(!z->GetShadowCenter(x,y)||x>=vision)continue;
        if(k==1051||k==1066){if(std::abs(z->mRow-p->mRow)<=1&&x>px)return z;continue;}
        if(k==1076)
        {if(z->mRow!=p->mRow||x<=px||status==13)continue;if(z->IsFlying())return z;if(!first)first=z;continue;}
        return z;
    }
    return first;
}
bool Capture(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;Zombie* z=nullptr;
    if(GetNativePlantData(p)->kind==1050&&!GetNativeSourcePlantNodePoint(p,"",x,y))return false;
    while(p->mBoard->IterateZombies(z))
    {
        if(z->mDead||z->mBodyHealth<=0||NativeWater268ZombieCaptured(z)||
            !(GetNativePlantData(p)->kind==1050?
                z->mRow==p->mRow&&!z->mMindControlled&&!z->IsDeadOrDying()&&GetNativeFusionZombieStatus(z)!=1&&GetNativeFusionZombieStatus(z)!=10:
                FusionWater268GrabTarget(GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,z->IsDeadOrDying()))||
            !Touches(x,y,2*kFusionBasePixelsX,2*GetNativeFusionPixelsY(p->mBoard),z->GetZombieRect()))continue;
        p->mTargetZombieID=p->mBoard->ZombieGetID(z);
        auto* d=GetNativePlantData(p);d->batchMode=d->kind==1050?3:1;
        d->waterTargetId=z->mNetId;d->waterTargetToken=unsigned(p->mTargetZombieID);
        if(d->kind==1050){TryGetFusionWater268SearchTicks(d->animationSpeed,p->mShootingCounter);d->shootSeconds=0;p->mStateCountdown=0;}
        else TryGetFusionWater268GrabTicks(d->animationSpeed,p->mStateCountdown);
        d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=0;
        if(d->kind!=1050){if(auto* source=GetNativeSourceZombieData(z))source->originSpeed=0;z->StopEating();z->mVelX=0;z->ApplyAnimRate(0);}
        LawnNet::NetTrace("Fusion native Water268 capture plant=%u token=%u target=%u generation=%u grabTicks=%d tick=%d",
            p->mNetId,d->instanceToken,z->mNetId,unsigned(p->mTargetZombieID),p->mStateCountdown,p->mBoard->mMainCounter);
        return true;
    }
    return false;
}
void KillCapture(Plant* p)
{
    if(auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID))
        if(!z->mDead){z->PoolSplash(false);z->DieWithLoot();}
    p->mTargetZombieID=ZOMBIEID_NULL;
    auto* d=GetNativePlantData(p);d->waterTargetId=0;d->waterTargetToken=0;
}
bool Muzzle(Plant* p,size_t event,float& x,float& y)
{
    const auto* d=GetNativePlantData(p);const char* path="Shoot";
    if(d->kind==252)path=event?"Shoot2":"Shoot1";
    else if(d->kind==1051)path="head1/pos/ThreePeater_mouth_2"; // All3 source calls use shoot1.
    else if(d->kind==1076&&d->batchMode==1)path="Shoot2";
    return GetNativeSourcePlantNodePoint(p,path,x,y);
}
Projectile* Emit(Plant* p,uint16_t kind,int row,uint8_t way,float x,float y,float angle)
{
    auto* b=p->mBoard;const auto* stats=FusionFindPlant(GetNativePlantData(p)->kind);
    if(!stats||!b->IsValidGridCell(0,row)||!EnsureNativeProjectileVisuals(b,kind)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    const auto type=kind==37?PROJECTILE_SPIKE:(kind==41||kind==42)?PROJECTILE_STAR:kind==9?PROJECTILE_PUFF:PROJECTILE_PEA;
    auto* q=b->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,row,(kind==41||kind==42)?PROJECTILE_PEA:type);if(!q)return nullptr;
    q->mProjectileType=type;q->mMotionType=kind==41||kind==42||way==7?MOTION_STAR:way==4||way==5?MOTION_THREEPEATER:MOTION_STRAIGHT;
    NativeProjectileData data;data.kind=kind;data.damage=stats->damage;data.specialType=way;
    if(!AttachNativeProjectile(b,q,data)){q->Die();return nullptr;}
    q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=0;q->mVelZ=0;q->mAccZ=0;q->mShadowY=y;
    q->mVelX=float(std::cos(angle)*6*kFusionBasePixelsX*.01);q->mVelY=float(-std::sin(angle)*6*GetNativeFusionPixelsY(b)*.01);q->mRotation=angle;
    if(way==9)q->mVelX=float(-6*kFusionBasePixelsX*.01);
    q->mRotationSpeed=0;
    if(kind==32)
    {
        // SquashBullet.Awake draws these independently of its initial way0/4/5.
        q->mRotationSpeed=RandRangeFloat(1.5f,1.9f)*float(kFusionBasePixelsX*.01);
        q->mVelZ=-RandRangeFloat(4.8f,5.2f)*float(GetNativeFusionPixelsY(b)*.01);
        q->mAccZ=float(9.8*GetNativeFusionPixelsY(b)*.0001);
        q->mShadowY=y+float(10*.061242823*GetNativeFusionPixelsY(b));
    }
    q->mX=int(q->mPosX);q->mY=int(q->mPosY);
    LawnNet::NetTrace("Fusion native Water268 emit plant=%u source=%u token=%u projectile=%u kind=%u way=%u row=%d x=%.9g y=%.9g vx=%.9g vy=%.9g tick=%d",p->mNetId,GetNativePlantData(p)->kind,GetNativePlantData(p)->instanceToken,q->mNetId,kind,way,row,x,y,q->mVelX,q->mVelY,b->mMainCounter);
    return q;
}
void Crash(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return;std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
        if(z!=CapturedTarget(p)&&!z->mDead&&z->mBodyHealth>0&&!z->mMindControlled&&z->mRow==p->mRow&&!z->IsFlying()&&
            Touches(x,y,1.5*kFusionBasePixelsX,3*GetNativeFusionPixelsY(p->mBoard),z->GetZombieRect()))targets.push_back(p->mBoard->ZombieGetID(z));
    for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id)))ApplyNativeSourceSquashDamage(z,600);
}
}
bool IsNativeWater268ImplementedKind(uint16_t k)
{return KelpImplemented(k)||IsFusionWater268HomingPlant(k)||Sea(k)||k==1085;}
const FusionShooterTimingProfile* GetNativeWater268Timing(const Plant* p)
{if(!Current(p))return nullptr;const auto* d=GetNativePlantData(p);return FindFusionWater268Timing(d->kind,Sea(d->kind)&&d->batchBagCount>0,d->batchMode==1);}
bool InitializeNativeWater268Plant(Plant* p)
{
    if(!Current(p)||!IsNativeWater268ImplementedKind(GetNativePlantData(p)->kind))return false;
    if(!Host(p))return true;auto* d=GetNativePlantData(p);const auto* stats=FusionFindPlant(d->kind);
    if(!stats||!stats->hasStats)return false;
    p->mPlantHealth=p->mPlantMaxHealth=stats->maxHealth;p->mStateCountdown=p->mDoSpecialCountdown=p->mDisappearCountdown=0;
    p->mShootingCounter=0;p->mTargetZombieID=ZOMBIEID_NULL;
    d->batchMode=0;d->batchShotCount=d->batchRageTicks=d->batchBagCount=0;d->productionTicks=d->pendingProductionTicks=0;
    d->shootSeconds=-1;d->idleSeconds=0;d->animationSpeed=1;
    d->waterTargetId=0;d->waterTargetToken=0;
    p->mTargetX=p->mTargetY=0;
    if(const auto* timing=FindFusionWater268Timing(d->kind))InitializeNativeShooterTiming(p,*timing,true);
    else d->animationSpeed=RandRangeFloat(.9f,1.1f);
    if(d->kind==252)p->mStateCountdown=int(std::ceil(double(12.433333f)*double(.8304291f)/(4*d->animationSpeed)*100));
    if(Sea(d->kind))p->mStateCountdown=d->kind==1076?12001:6001;
    d->productionTicks=p->mStateCountdown;
    return true;
}
bool UpdateNativeWater268Plant(Plant* p)
{
    if(!Current(p)||!IsNativeWater268ImplementedKind(GetNativePlantData(p)->kind))return false;
    if(!Host(p)||p->mSquished)return true;auto* d=GetNativePlantData(p);
    if(KelpImplemented(d->kind))
    {
        if(d->kind==1050&&d->batchMode==3&&!CapturedTarget(p))
        {d->batchMode=0;d->waterTargetId=0;d->waterTargetToken=0;p->mTargetZombieID=ZOMBIEID_NULL;p->mShootingCounter=0;d->shootSeconds=-1;}
        if(!d->batchMode)Capture(p);
        else if(d->batchMode==1&&!p->mStateCountdown)
        {
            d->batchMode=2;p->mDoSpecialCountdown=kFusionWater268SinkTicks;d->productionTicks=0;d->pendingProductionTicks=kFusionWater268SinkTicks;
            if(auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID)){z->DragUnder();z->mPosY+=float(GetNativeFusionPixelsY(p->mBoard)*.01);z->mY=int(z->mPosY);}
            LawnNet::NetTrace("Fusion native Water268 sink plant=%u targetGeneration=%u ticks=50 tick=%d",p->mNetId,unsigned(p->mTargetZombieID),p->mBoard->mMainCounter);
        }
        else if(d->batchMode==2)
        {
            if(!p->mDoSpecialCountdown)
            {
                KillCapture(p);
                if(d->kind==1050&&++d->batchBagCount<3){d->batchMode=0;p->mStateCountdown=0;}
                else {p->Die();return true;}
            }
            else if(auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID))
            {z->mPosY+=float(GetNativeFusionPixelsY(p->mBoard)*.01);z->mY=int(z->mPosY);}
        }
        if(d->kind!=1051&&d->kind!=1066)return true;
    }
    if(Sea(d->kind)&&!p->mStateCountdown)
    {
        if(!d->batchBagCount){d->batchBagCount=1;d->batchMode=2;p->mStateCountdown=int(std::ceil(100./(d->kind==1076?1:d->animationSpeed)));p->mShootingCounter=0;d->shootSeconds=0;}
        else if(d->batchMode==2)
        {
            d->batchMode=0;d->shootSeconds=-1;
            if(p->mTargetX){p->mTargetX=0;if(d->kind==1076)d->batchMode=p->mTargetY?1:0;BeginNativeShooterShot(p,*GetNativeWater268Timing(p));}
        }
    }
    const auto* timing=GetNativeWater268Timing(p);
    if(--p->mLaunchCounter<=0)
    {
        if(d->kind==1076)p->mLaunchCounter=int(std::floor(double(1.5f+RandRangeFloat(-.1f,.1f))*100))+1;else ResetNativeShooterLaunchCounter(p,*timing);
        if(p->mShootingCounter<=0)if(auto* aim=PlantAim(p))
        {
            const bool waiting=(d->kind==252&&p->mStateCountdown)||(Sea(d->kind)&&d->batchMode==2);
            if(waiting){p->mTargetX=1;p->mTargetY=aim->IsFlying()?1:0;}
            else {if(d->kind==1076)d->batchMode=aim->IsFlying()?1:0;BeginNativeShooterShot(p,*GetNativeWater268Timing(p));}
        }
    }
    if(d->kind==252&&!p->mStateCountdown&&p->mTargetX){p->mTargetX=0;BeginNativeShooterShot(p,*timing);}
    return true;
}
bool UpdateNativeWater268Phase(Plant* p)
{
    if(!Current(p)||!IsNativeWater268ImplementedKind(GetNativePlantData(p)->kind))return false;
    if(!Host(p))return true;auto* d=GetNativePlantData(p);
    if(Sea(d->kind)&&d->batchMode==2)
    {d->idleSeconds+=.01f;const int total=int(std::ceil(100./(d->kind==1076?1:d->animationSpeed)));d->shootSeconds=(total-p->mStateCountdown)*.01f;}
    else if(d->kind==1050)
    {d->idleSeconds+=.01f;int total=0;TryGetFusionWater268SearchTicks(d->animationSpeed,total);d->shootSeconds=p->mShootingCounter>0?(total-p->mShootingCounter)*.01f:-1;}
    else if(const auto* timing=GetNativeWater268Timing(p))UpdateNativeShooterPhase(p,*timing);
    else d->idleSeconds+=.01f;
    d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=KelpImplemented(d->kind)?p->mDoSpecialCountdown:0;
    return true;
}
bool ApplyNativeWater268Special(Plant* p)
{return Current(p)&&KelpImplemented(GetNativePlantData(p)->kind);}
bool DispatchNativeWater268Events(Plant* p)
{
    if(!Current(p))return false;auto* pd=GetNativePlantData(p);
    if(pd->kind==1050)
    {
        if(!Host(p))return true;
        if(pd->batchMode==3&&FusionWater268SearchEventDue(p->mShootingCounter,pd->animationSpeed,false))
            if(auto* z=CapturedTarget(p)){if(auto* source=GetNativeSourceZombieData(z))source->originSpeed=0;z->StopEating();z->mVelX=0;z->ApplyAnimRate(0);pd->batchMode=1;TryGetFusionWater268GrabTicks(pd->animationSpeed,p->mStateCountdown);++p->mStateCountdown;}
        if(FusionWater268SearchEventDue(p->mShootingCounter,pd->animationSpeed,true))Crash(p);
        return true;
    }
    const auto* timing=GetNativeWater268Timing(p);
    if(!timing)return false;if(!Host(p))return true;
    for(size_t i=0;i<timing->eventClipSeconds.size();++i)
    {
        bool due=false;if(!TryGetFusionShooterEventAtCounter(*timing,GetNativePlantData(p)->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
        float x=0,y=0;if(timing->kind!=1083&&timing->kind!=1085&&!Muzzle(p,i,x,y))continue;const auto* stats=FusionFindPlant(timing->kind);
        if(IsFusionWater268HomingPlant(timing->kind))
        {if(SpawnNativeWater268Homing(p->mBoard,timing->projectileKind,p->mRow,stats->damage,x,y,0))++pd->batchShotCount;}
        else if(timing->kind==1051||timing->kind==1066)
        {for(int delta:{1,0,-1})if(Emit(p,timing->projectileKind,p->mRow+delta,delta>0?5:delta<0?4:0,x,y,0))++pd->batchShotCount;}
        else if(timing->kind==1076)
        {if(Emit(p,timing->projectileKind,p->mRow,pd->batchMode==1?7:0,x,y,pd->batchMode==1?.5235987901687622f:0))++pd->batchShotCount;}
        else if(timing->kind==1083||timing->kind==1085)
        {
            constexpr double angle[]={1.5707963267948966,-1.5707963267948966,3.141592653589793,.5235987755982989,-.5235987755982989};
            constexpr const char* paths[]={"Shoot1","Shoot2","Shoot3","Shoot4","Shoot5"};
            for(int ray=0;ray<5;++ray)
            {
                float rotation=float(angle[ray]);
                if(timing->kind==1085)
                {std::array<float,6> transform;if(!GetNativeSourcePlantNodeTransform(p,paths[ray],transform))continue;x=transform[4];y=transform[5];rotation=std::atan2(-transform[1],transform[0]);}
                else if(!GetNativeSourcePlantNodePoint(p,paths[ray],x,y))continue;
                if(Emit(p,timing->projectileKind,p->mRow,ray==2?9:2,x,y,rotation))++pd->batchShotCount;
            }
        }
    }
    return true;
}
void NativeWater268ZombieRemoving(Zombie* z)
{
    if(!z||!z->mBoard||!IsNativeFusionBoard(z->mBoard)||z->mApp->NetIsClient())return;
    Plant* p=nullptr;
    while(z->mBoard->IteratePlants(p))
    {
        auto* d=GetNativePlantData(p);
        if(!p->mDead&&d&&KelpImplemented(d->kind)&&d->batchMode>=1&&d->batchMode<=3&&
            d->waterTargetId==z->mNetId&&d->waterTargetToken==unsigned(z->mBoard->ZombieGetID(z)))
        {
            // MoveObject's null-object branch keeps the independent hand coroutine alive.
            d->waterTargetId=0;d->waterTargetToken=0;p->mTargetZombieID=ZOMBIEID_NULL;
            if(d->batchMode==3){d->batchMode=0;p->mShootingCounter=0;d->shootSeconds=-1;}
            LawnNet::NetTrace("Fusion native Water268 detach plant=%u token=%u target=%u mode=%u grab=%d sink=%d tick=%d",
                p->mNetId,d->instanceToken,z->mNetId,d->batchMode,p->mStateCountdown,p->mDoSpecialCountdown,p->mBoard->mMainCounter);
        }
    }
}
bool NativeWater268ZombieCaptured(const Zombie* z)
{
    if(!z||!GetNativeContext(z->mBoard)||z->mDead)return false;
    Plant* p=nullptr;
    while(z->mBoard->IteratePlants(p))
    {const auto* d=GetNativePlantData(p);if(d&&KelpImplemented(d->kind)&&!p->mDead&&p->mIsOnBoard&&(d->batchMode==1||d->batchMode==2)&&CapturedTarget(p)==z)return true;}
    return false;
}
void NativeWater268PlantRemoving(Plant* p,int reason)
{
    if(!Current(p)||p->mNetGhost||p->mApp->NetIsClient())return;const auto k=GetNativePlantData(p)->kind;
    if(KelpImplemented(k)&&k!=1049)KillCapture(p);
    // Jalakelp overrides DieEvent without calling Tanglekelp.DieEvent.
    if(k==1049)ApplyNativeBatchFireRow(p->mBoard,p->mRow,1800,false);
    if(IsFusionWater268HomingPlant(k)&&reason==0)
    {
        PlantsOnLawn layers{};p->mBoard->GetPlantsOnLawn(p->mPlantCol,p->mRow,&layers);
        if(layers.mPumpkinPlant&&!layers.mPumpkinPlant->mDead)
        {NativePlantData lily;lily.kind=12;lily.owner=LawnNet::NET_SHARED_OWNER;TrySpawnNativeFusionPlant(p->mBoard,p->mPlantCol,p->mRow,lily);}
    }
}
bool GetNativeWater268VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!Current(p)||!IsNativeWater268ImplementedKind(GetNativePlantData(p)->kind))return false;
    const auto* d=GetNativePlantData(p);state="idle";seconds=d->idleSeconds;
    if(d->kind==1050&&d->shootSeconds>=0){state="searchzombie";seconds=d->shootSeconds;}
    else if(KelpImplemented(d->kind))return true; // Grab/shoot are independent additive layers.
    else if(d->kind==252&&d->productionTicks>0)
    {state="enter";const int total=int(std::ceil(double(12.433333f)*double(.8304291f)/(4*d->animationSpeed)*100));seconds=(total-d->productionTicks)*.01f;}
    else if(Sea(d->kind)&&d->batchMode==2){state="grow";seconds=d->shootSeconds;}
    else if(d->shootSeconds>=0){if(const auto* timing=GetNativeWater268Timing(p))state=timing->visualState;seconds=d->shootSeconds;}
    else if(Sea(d->kind)&&d->batchBagCount)state="idle2";
    return true;
}
bool GetNativeWater268GrabVisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!Current(p))return false;const auto* d=GetNativePlantData(p);
    if(!KelpImplemented(d->kind)||(d->batchMode!=1&&d->batchMode!=2))return false;
    int total=0;if(!TryGetFusionWater268GrabTicks(d->animationSpeed,total))return false;
    state="grab";seconds=float(total-d->productionTicks)*.01f+(d->batchMode==2?(kFusionWater268SinkTicks-d->pendingProductionTicks)*.01f:0);return true;
}
bool GetNativeWater268ShootingVisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!Current(p))return false;const auto* d=GetNativePlantData(p);
    if((d->kind!=1051&&d->kind!=1066)||d->shootSeconds<0)return false;
    state="shoot";seconds=d->shootSeconds;return true;
}
bool GetNativeWater268GrabAnchor(const Plant* p,float& x,float& y)
{
    if(!Current(p)||!KelpImplemented(GetNativePlantData(p)->kind)||(GetNativePlantData(p)->batchMode!=1&&GetNativePlantData(p)->batchMode!=2))return false;
    auto* z=CapturedTarget(p);if(!z||!z->GetShadowCenter(x,y))return false;
    x-=float(.75*kFusionBasePixelsX);y-=float(.25*GetNativeFusionPixelsY(p->mBoard));return true;
}
Projectile* SpawnNativeWater268Homing(Board* b,uint16_t kind,int row,int damage,float x,float y,float angle)
{
    if(!b||!CanApplyNativeFusionGameplay(b)||b->mApp->NetIsClient()||!FusionWater268HomingMetadataValid(kind,damage,6)||
        row<0||row>=GetNativeFusionRowCount(b)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(angle)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    if(!EnsureNativeProjectileVisuals(b,kind))return nullptr;
    auto* p=b->AddProjectile(int(x-20),int(y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,row,0),row,kind==20?PROJECTILE_PEA:PROJECTILE_SPIKE);
    if(!p)return nullptr;NativeProjectileData d;d.kind=kind;d.damage=damage;d.specialType=6;p->mMotionType=MOTION_HOMING;
    if(!AttachNativeProjectile(b,p,d)){p->Die();return nullptr;}
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mRotation=angle;p->mVelZ=0;p->mShadowY=y;
    p->mVelX=float(std::cos(angle)*5*kFusionBasePixelsX*.01);p->mVelY=float(-std::sin(angle)*5*GetNativeFusionPixelsY(b)*.01);
    p->mTargetZombieID=ZOMBIEID_NULL;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    LawnNet::NetTrace("Fusion native Water268 homing born id=%u kind=%u damage=%d row=%d x=%.9g y=%.9g tick=%d",p->mNetId,kind,damage,row,x,y,b->mMainCounter);
    return p;
}
bool SetNativeWater268Grap(Zombie* z)
{
    if(!z||z->mDead||z->mNetGhost||z->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(z->mBoard))return false;
    auto* d=GetOrCreateNativeZombieActionData(z);if(!d)return false;
    int ticks=0,count=0;bool percent=false,full=false;
    if(!TryAddFusionWater268Grap(d->waterGrapTicks,d->waterGrapCount,ticks,count,percent,full))return false;
    d->waterGrapTicks=ticks;d->waterGrapCount=count;
    const int maximum=int((std::min)(int64_t((std::numeric_limits<int>::max)()),int64_t(z->mBodyMaxHealth)+z->mHelmMaxHealth));
    LawnNet::NetTrace("Fusion native Water268 grap target=%u count=%d ticks=%d percent=%d full=%d tick=%d",z->mNetId,count,ticks,percent,full,z->mBoard->mMainCounter);
    if(percent)z->TakeDamage(int(float(maximum)*.01f),2U);
    if(full&&!z->mDead)z->TakeDamage(maximum,2U);
    return true;
}
void TickNativeWater268ZombieState(Zombie* z)
{
    if(!z||z->mDead||z->mNetGhost||z->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(z->mBoard)||z->mBoard->mPaused)return;
    if(auto* d=GetNativeZombieActionData(z))if(d->waterGrapTicks>0)--d->waterGrapTicks;
}
float GetNativeWater268GrapSpeedFactor(const Zombie* z)
{const auto* d=GetNativeZombieActionData(z);return d&&d->waterGrapTicks>0?.5f:1.f;}
bool IsNativeWater268Projectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);if(!d||!FusionWater268ProjectileMetadataValid(d->kind,d->damage,d->specialType))return false;
    // Ordinary puff9 remains owned by its canonical native controller.
    if(d->kind==9)return false;
    if(d->specialType==6)return p->mMotionType==MOTION_HOMING;
    if(d->kind==41||d->kind==42||(d->kind==37&&d->specialType==7))return p->mMotionType==MOTION_STAR;
    if(d->kind!=29&&d->kind!=32)return false;
    return d->specialType==255?p->mMotionType==MOTION_LOBBED:
        d->specialType==0?p->mMotionType==MOTION_STRAIGHT:p->mMotionType==MOTION_THREEPEATER;
}
bool GetNativeWater268ProjectileRect(const Projectile* p,Sexy::Rect& out)
{
    if(!IsNativeWater268Projectile(p))return false;const auto* d=GetNativeProjectileData(p);FusionRecipientRect r;
    if(!TryMakeFusionWater268ProjectileRect(d->kind,p->mPosX+20,p->mPosY+20,p->mRotation,GetNativeFusionPixelsY(p->mBoard),r))return false;
    out=Sexy::Rect(int(std::floor(r.x)),int(std::floor(r.y)),int(std::ceil(r.width)),int(std::ceil(r.height)));return true;
}
namespace
{
bool WaterCanHit(const Projectile* p,Zombie* z)
{
    if(!z||z->mBoard!=p->mBoard)return false;const auto* d=GetNativeProjectileData(p);
    if(d->specialType==6)return p->mBoard->ZombieTryToGet(p->mTargetZombieID)==z&&ProjectileTarget(z,true);
    return FusionWater268ProjectileCanHit(d->specialType,GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,
        !NativeWater268ZombieCaptured(z),!z->mDead&&z->mBodyHealth>0);
}
bool Outside(const Projectile* p)
{
    const double py=GetNativeFusionPixelsY(p->mBoard),pitch=GetNativeFusionRowCount(p->mBoard)==6?85.:100.;
    const double x=(p->mPosX+20-p->mBoard->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8;
    const double y=2.3-(p->mPosY+20-(p->mBoard->GridToPixelY(0,0)+50))/py;
    // Bullet.Awake copies Board limits, then MoveDie applies -2/+1 X.
    return x< -12.||x>1.35*(p->mBoard->GetColumnCount()-1)-4.8+3.85||
        y<2.3-pitch/py*(GetNativeFusionRowCount(p->mBoard)-1)-.6||y>6.5;
}
void Landing(Projectile* p)
{
    std::vector<ZombieID> targets;Zombie* z=nullptr;const double x=p->mPosX+20,y=p->mPosY+20,py=GetNativeFusionPixelsY(p->mBoard);
    while(p->mBoard->IterateZombies(z))
    {
        if(z->mDead||z->mMindControlled||z->mRow!=p->mRow||NativeWater268ZombieCaptured(z))continue;
        const auto r=z->GetZombieRect();
        const double dx=((std::max)(double(r.mX),(std::min)(x,double(r.mX+r.mWidth)))-x)/kFusionBasePixelsX;
        const double dy=((std::max)(double(r.mY),(std::min)(y,double(r.mY+r.mHeight)))-y)/py;
        if(dx*dx+dy*dy<=.25)targets.push_back(p->mBoard->ZombieGetID(z));
    }
    for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id)))
    {const bool water=z->mInPool;z->TakeDamage(40,2U);if(water&&!z->mDead)SetNativeWater268Grap(z);}
    LawnNet::NetTrace("Fusion native Water268 squash landing id=%u row=%d targets=%zu tick=%d",p->mNetId,p->mRow,targets.size(),p->mBoard->mMainCounter);
}
}
bool UpdateNativeWater268Projectile(Projectile* p)
{
    if(!IsNativeWater268Projectile(p))return false;
    if(p->mDead||p->mNetGhost||p->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(p->mBoard))return true;
    auto* d=GetNativeProjectileData(p);const double py=GetNativeFusionPixelsY(p->mBoard);
    if(Outside(p)||p->mProjectileAge>2000){p->Die();return true;}
    if(d->kind==41||d->kind==42)
    {
        // Canonical child angle is carried by the existing raw body field.
        // ProjectileAge is host-local and cannot derive a late-join phase.
        constexpr double pi=3.14159265358979323846;
        p->mRotationSpeed=float(std::fmod(double(p->mRotationSpeed)+150*.01*pi/180,2*pi));
    }
    if(d->specialType==6)
    {
        const bool noTarget=p->mTargetZombieID==ZOMBIEID_NULL;auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);
        if(!ProjectileTarget(z,true))
        {
            z=Nearest(p);p->mTargetZombieID=z?p->mBoard->ZombieGetID(z):ZOMBIEID_NULL;
            if(!noTarget)p->mVelX=p->mVelY=0;
            else {p->mVelX=float(std::cos(p->mRotation)*5*kFusionBasePixelsX*.01);p->mVelY=float(-std::sin(p->mRotation)*5*py*.01);}
        }
        else
        {const auto r=z->GetZombieRect();TryGetFusionWater268HomingStep(r.mX+r.mWidth*.5-p->mPosX-20,r.mY+r.mHeight*.5-p->mPosY-20,py,5,p->mVelX,p->mVelY);p->mRotation=float(std::atan2(-p->mVelY/py,p->mVelX/kFusionBasePixelsX));}
    }
    else if(d->specialType==4||d->specialType==5)
        TryGetFusionShooter267ThreeVerticalStep(d->specialType,(std::max)(0,p->mProjectileAge-1),p->mVelY,GetNativeFusionRowCount(p->mBoard)==6?85.:100.);
    else if(d->specialType==7)
    {
        TryGetFusionWater268SkyAngle(p->mProjectileAge,p->mRotation);
        p->mVelX=float(std::cos(p->mRotation)*6*kFusionBasePixelsX*.01);p->mVelY=float(-std::sin(p->mRotation)*6*py*.01);
    }
    else if(d->kind==32&&d->specialType==255)
    {
        float next=p->mPosZ,velocity=p->mVelZ;bool bounce=false;
        if(!TryStepFusionSquashProjectile(p->mPosZ,p->mVelZ,p->mAccZ,float((12.25*.061242823-.15)*py),next,velocity,bounce)){p->Die();return true;}
        p->mPosZ=next;p->mVelZ=velocity;if(bounce)Landing(p);
    }
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    if(d->specialType==6||d->specialType==2)p->mRow=p->mBoard->PixelToGridYKeepOnBoard(p->mX,p->mY);
    if(d->kind==32&&d->specialType==255)return true; // Disabled root collider after its first hit.
    auto* c=GetNativeContext(p->mBoard);const auto generation=p->mBoard->mProjectiles.DataArrayGetID(p);
    std::unordered_set<uint32_t> touching;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!WaterCanHit(p,z))continue;const auto r=z->GetZombieRect();
        if(!FusionWater268ProjectileTouchesRect(d->kind,p->mPosX+20,p->mPosY+20,p->mRotation,py,{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)}))continue;
        const auto target=p->mBoard->ZombieGetID(z);touching.insert(target);
        const auto old=c->fireContacts.find(generation);if(old!=c->fireContacts.end()&&old->second.contains(target))continue;
        ApplyNativeWater268ProjectileImpact(p,z);if(p->mDead||d->kind==32)break;
    }
    if(!p->mDead)
    {
        c->fireContacts[generation]=std::move(touching);
        // Shared optimized CactusStarBullet.Update runs StarBullet.Update first.
        if(d->kind==42&&FusionWater268StarTrackingDue(d->specialType,p->mProjectileAge))
        {
            d->specialType=6;p->mMotionType=MOTION_HOMING;
            LawnNet::NetTrace("Fusion native CactusStar268 tracking projectile=%u age=%d rootAngle=%.9g childAngle=%.9g tick=%d",p->mNetId,p->mProjectileAge,p->mRotation,p->mRotationSpeed,p->mBoard->mMainCounter);
        }
    }
    return true;
}
bool ApplyNativeWater268ProjectileImpact(Projectile* p,Zombie* z)
{
    if(!IsNativeWater268Projectile(p)||p->mDead||p->mNetGhost||p->mApp->NetIsClient()||!WaterCanHit(p,z))return false;
    auto* d=GetNativeProjectileData(p);int damage=d->damage;const bool water=z->mInPool;
    if(d->kind==34){if(!z->mIceTrapCounter)damage*=4;const FusionColdDamageContext cold{34,FusionColdDamageDelivery::IceShieldless};z->TakeDamage(damage,1U,&cold);AddNativeColdLevel(z,10);}
    else if(d->kind==35){if(z->mChilledCounter||z->mIceTrapCounter)damage*=3;z->TakeDamage(damage,1U);NativeWarmAfterDamage(z);}
    else z->TakeDamage(damage,0U);
    if((d->kind==29||d->kind==32)&&water&&!z->mDead)SetNativeWater268Grap(z);
    p->PlayImpactSound(z);
    LawnNet::NetTrace("Fusion native Water268 hit id=%u kind=%u target=%u damage=%d body=%d helm=%d shield=%d tick=%d",p->mNetId,d->kind,z->mNetId,damage,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,p->mBoard->mMainCounter);
    if(d->kind==32)
    {d->specialType=255;p->mMotionType=MOTION_LOBBED;p->mVelZ*= -.75f;p->mVelX=p->mRotationSpeed;p->mVelY=0;}
    else p->Die(); // Cactus.SetPenetrationTime=0: first sky/track hit retires it.
    return true;
}
}
