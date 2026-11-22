#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeChomper268.h"
#include "FusionChomper268Rules.h"
#include "FusionShulk268Rules.h"
#include "FusionNative.h"
#include "FusionNativeTravelUltimate370.h"
#include "FusionNativeVisual.h"
#include "FusionNativeWater268.h"
#include "FusionWater268Rules.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativePoison.h"
#include "FusionBasePlantRules.h"
#include "FusionRollingProjectileRules.h"
#include "../Plant.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&p->mIsOnBoard&&IsNativeFusionBoard(p->mBoard)&&GetNativePlantData(p);}
bool Host(const Plant* p)
{return Current(p)&&!p->mDead&&!p->mNetGhost&&!p->mApp->NetIsClient()&&CanApplyNativeFusionGameplay(p->mBoard);}
bool Target(Zombie* z)
{return z&&!z->mDead&&z->mBodyHealth>0&&!z->mMindControlled&&!NativeWater268ZombieCaptured(z);}
bool Box(Plant* p,Zombie* z,double ox,double oy,double w,double h)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;const double py=GetNativeFusionPixelsY(p->mBoard);
    x+=float(ox*kFusionBasePixelsX);y-=float(oy*py);const auto r=z->GetZombieRect();
    return x+w*.5*kFusionBasePixelsX>=r.mX&&x-w*.5*kFusionBasePixelsX<=r.mX+r.mWidth&&y+h*.5*py>=r.mY&&y-h*.5*py<=r.mY+r.mHeight;
}
bool ShulkTarget(Plant* p,Zombie* z)
{return z&&FusionShulk268Target(GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,!NativeWater268ZombieCaptured(z),!z->mDead&&z->mBodyHealth>0);}
Zombie* ChomperTarget(Plant* p)
{
    Zombie* z=nullptr;
    // Source gives priority to the actual Zombie currently eating this actor.
    while(p->mBoard->IterateZombies(z))if(!z->mDead&&z->mIsEating&&z->FindPlantTarget(ATTACKTYPE_CHEW)==p)return z;
    Zombie* best=nullptr;float bestX=(std::numeric_limits<float>::max)();z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Target(z)||!FusionStrongChomperTarget(GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,z->IsDeadOrDying(),!NativeWater268ZombieCaptured(z))||!Box(p,z,1.5,.5,1.5,1.5))continue;
        float x=0,y=0;if(z->GetShadowCenter(x,y)&&x<bestX){best=z;bestX=x;}
    }
    return best;
}
void Bite(Plant* p)
{
    auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);if(!Target(z))return;
    auto* d=GetNativePlantData(p);const auto* profile=FindFusionStrongChomperProfile(d->kind);
    const bool armor=z->mHelmHealth>0&&z->mHelmType!=HelmType::HELMTYPE_NONE;
    const bool heavy=armor||z->mBodyHealth>float(profile->attackDamage)*2.5f;
    FusionStrongChomperBite plan;
    if(!TryPlanFusionStrongChomperBite(d->kind,float(z->mBodyHealth),armor,(std::max)(0,z->mHelmHealth),!p->mLaunchCounter,d->kind==1013&&heavy?RandRangeInt(4,5):0,plan))return;
    PlanNativeTravelUltimate370Bite(p,z,plan);
    z->TakeDamage(plan.sourceDamage,2U); // Source NormalAll1, not Real6.
    RecoverNativeChomper268(p,plan.recovery);if(plan.rechargeTicks)p->mLaunchCounter=plan.rechargeTicks;
    if(d->kind==1013)d->batchBagCount=plan.nextShotKind;
    LawnNet::NetTrace("Fusion native Chomper268 bite plant=%u source=%u target=%u damage=%d recovery=%d health=%d charge=%d stored=%u tick=%d",p->mNetId,d->kind,z->mNetId,plan.sourceDamage,plan.recovery,p->mPlantHealth,p->mLaunchCounter,d->batchBagCount,p->mBoard->mMainCounter);
    p->mTargetZombieID=ZOMBIEID_NULL;
}
Projectile* Emit(Plant* p,uint16_t kind,uint8_t way,int damage,const char* path)
{
    float x=0,y=0;auto* b=p->mBoard;
    if(!GetNativeSourcePlantNodePoint(p,path,x,y)||!EnsureNativeProjectileVisuals(b,kind)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    auto* q=b->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,p->mRow,PROJECTILE_PEA);if(!q)return nullptr;
    NativeProjectileData d;d.kind=kind;d.damage=damage;d.specialType=way;
    q->mMotionType=way==16?MOTION_HOMING:MOTION_STRAIGHT;
    if(!AttachNativeTravelProjectile369(p,q,d)){q->Die();return nullptr;}
    q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=0;q->mVelY=q->mVelZ=q->mAccZ=0;q->mShadowY=y;q->mRotation=q->mRotationSpeed=0;
    q->mVelX=float((way==1?-6:6)*kFusionBasePixelsX*.01);
    if(kind==94)
    {q->mVelX=float(-2*kFusionBasePixelsX*.01);q->mVelY=-RandRangeFloat(.5f,1.f)*float(GetNativeFusionPixelsY(b)*.01);q->mTargetZombieID=ZOMBIEID_NULL;}
    q->mX=int(q->mPosX);q->mY=int(q->mPosY);
    LawnNet::NetTrace("Fusion native Chomper268 emit plant=%u source=%u token=%u projectile=%u kind=%u way=%u damage=%d x=%.9g y=%.9g tick=%d",p->mNetId,GetNativePlantData(p)->kind,GetNativePlantData(p)->instanceToken,q->mNetId,kind,way,damage,x,y,b->mMainCounter);
    return q;
}
void SearchShulk(Plant* p)
{
    auto* d=GetNativePlantData(p);Zombie* z=nullptr;bool found=false;
    while(p->mBoard->IterateZombies(z))if(ShulkTarget(p,z)&&Box(p,z,1,.5,2,2)){found=true;break;}
    d->batchBagCount=found?1:0;
    if(found&&!d->batchMode){d->batchMode=1;TryGetFusionShulk268PhaseTicks(d->kind,1,p->mDoSpecialCountdown);}
    else if(!found&&d->batchMode==2){d->batchMode=3;TryGetFusionShulk268PhaseTicks(d->kind,3,p->mDoSpecialCountdown);}
    else if(!found&&d->batchMode==1){d->batchMode=4;TryGetFusionShulk268PhaseTicks(d->kind,4,p->mDoSpecialCountdown);}
}
}
bool IsNativeChomper268Kind(uint16_t k){return FindFusionStrongChomperProfile(k)||IsFusionShulk268Kind(k);}
bool TryCapNativeChomper268IncomingDamage(const Plant* p,int incoming,int& damage)
{return Current(p)&&TryCapFusionStrongChomperDamage(GetNativePlantData(p)->kind,NativeTravelUltimate370ChomperCap(p,incoming),damage);}
void NativeChomper268HealthChanged(Plant* p)
{
    if(!Host(p))return;auto* d=GetNativePlantData(p);if(!FindFusionStrongChomperProfile(d->kind)||p->mPlantHealth<=0)return;
    int hp=0;if(TryCapFusionStrongChomperHealth(d->kind,p->mPlantHealth,hp))p->mPlantHealth=hp;
    uint8_t stage=d->damageStage;if(TryAdvanceFusionStrongChomperStage(d->kind,p->mPlantHealth,stage,stage))d->damageStage=stage;
}
bool RecoverNativeChomper268(Plant* p,int amount)
{
    if(!Host(p)||amount<0||!FindFusionStrongChomperProfile(GetNativePlantData(p)->kind))return false;
    p->mPlantHealth=int((std::min)(int64_t((std::numeric_limits<int>::max)()),int64_t(p->mPlantHealth)+NativeTravelUltimate370Recovery(p,amount)));NativeChomper268HealthChanged(p);return true;
}
bool InitializeNativeChomper268Plant(Plant* p)
{
    if(!Current(p)||!IsNativeChomper268Kind(GetNativePlantData(p)->kind))return false;if(!Host(p))return true;
    auto* d=GetNativePlantData(p);const auto* stats=FusionFindPlant(d->kind);if(!stats)return false;
    p->mPlantHealth=p->mPlantMaxHealth=stats->maxHealth;p->mTargetZombieID=ZOMBIEID_NULL;p->mShootingCounter=p->mDoSpecialCountdown=p->mDisappearCountdown=0;
    d->damageStage=d->batchMode=0;d->batchRageTicks=0;d->batchShotCount=0;d->batchBagCount=d->kind==1013?6:0;
    d->animationSpeed=1;d->shootSeconds=-1;d->idleSeconds=0;
    p->mLaunchCounter=0;p->mStateCountdown=FindFusionStrongChomperProfile(d->kind)?1:51;
    if(const auto* profile=FindFusionShulk268Timing(d->kind))InitializeNativeShooterTiming(p,*profile,true);
    else d->animationSpeed=RandRangeFloat(.9f,1.1f);
    d->productionTicks=FindFusionStrongChomperProfile(d->kind)?0:p->mStateCountdown;
    d->pendingProductionTicks=FindFusionStrongChomperProfile(d->kind)?p->mStateCountdown:0;return true;
}
bool UpdateNativeChomper268Plant(Plant* p)
{
    if(!Current(p)||!IsNativeChomper268Kind(GetNativePlantData(p)->kind))return false;if(!Host(p)||p->mSquished)return true;
    auto* d=GetNativePlantData(p);
    if(FindFusionStrongChomperProfile(d->kind))
    {
        if(p->mLaunchCounter>0)--p->mLaunchCounter;
        if(!p->mStateCountdown){p->mStateCountdown=101;int hp=0;if(TryDecayFusionStrongChomperHealth(d->kind,p->mPlantHealth,hp)){p->mPlantHealth=hp;NativeChomper268HealthChanged(p);}}
        if(d->batchMode&&!p->mShootingCounter){d->batchMode=0;d->shootSeconds=-1;}
        if(!d->batchMode)if(auto* z=ChomperTarget(p)){p->mTargetZombieID=p->mBoard->ZombieGetID(z);d->batchMode=1;TryGetFusionStrongChomperBiteTicks(d->animationSpeed,p->mShootingCounter);d->shootSeconds=0;}
    }
    else
    {
        if(!p->mStateCountdown){p->mStateCountdown=51;SearchShulk(p);}
        if(d->batchMode==1&&!p->mDoSpecialCountdown)
        {if(d->batchBagCount){d->batchMode=2;d->idleSeconds=0;}else {d->batchMode=4;TryGetFusionShulk268PhaseTicks(d->kind,4,p->mDoSpecialCountdown);}}
        else if((d->batchMode==3||d->batchMode==4)&&!p->mDoSpecialCountdown)
        {d->batchMode=0;if(d->batchBagCount){d->batchMode=1;TryGetFusionShulk268PhaseTicks(d->kind,1,p->mDoSpecialCountdown);}}
        // Unity FixedUpdate is one global50Hz physics clock, projected on the native100Hz Board.
        if((d->batchMode==2||d->batchMode==3)&&!(p->mBoard->mMainCounter&1))
        {std::vector<ZombieID> ids;Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(ShulkTarget(p,z)&&Box(p,z,.6,.5,1.1,2))ids.push_back(p->mBoard->ZombieGetID(z));for(auto id:ids)if((z=p->mBoard->ZombieTryToGet(id)))z->TakeDamage(5,0U);}
        if(d->kind==1177&&--p->mLaunchCounter<=0)
        {
            const auto* t=FindFusionShulk268Timing(d->kind);ResetNativeShooterLaunchCounter(p,*t);
            Zombie* z=nullptr;const float vision=float(p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX);
            if(p->mShootingCounter<=0)while(p->mBoard->IterateZombies(z))
            {const int s=GetNativeFusionZombieStatus(z);float x=0,y=0;if(!Target(z)||!FusionShulk268Target(s,0,z->mMindControlled,true,true)||!z->GetShadowCenter(x,y)||x>=vision)continue;BeginNativeShooterShot(p,*t);break;}
        }
    }
    return true;
}
bool ApplyNativeChomper268Special(Plant* p)
{return Current(p)&&IsFusionShulk268Kind(GetNativePlantData(p)->kind);}
bool UpdateNativeChomper268Phase(Plant* p)
{
    if(!Current(p)||!IsNativeChomper268Kind(GetNativePlantData(p)->kind))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);
    if(FindFusionStrongChomperProfile(d->kind))
    {d->idleSeconds+=.01f;int total=0;TryGetFusionStrongChomperBiteTicks(d->animationSpeed,total);d->shootSeconds=d->batchMode?float(total-p->mShootingCounter)*.01f:-1;d->productionTicks=p->mLaunchCounter;d->pendingProductionTicks=p->mStateCountdown;}
    else
    {if(const auto* t=FindFusionShulk268Timing(d->kind))UpdateNativeShooterPhase(p,*t);else d->idleSeconds+=.01f;d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=p->mDoSpecialCountdown;}
    return true;
}
bool DispatchNativeChomper268Events(Plant* p)
{
    if(!Current(p)||!IsNativeChomper268Kind(GetNativePlantData(p)->kind))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);
    if(FindFusionStrongChomperProfile(d->kind))
    {
        if(FusionStrongChomperEventDue(d->animationSpeed,p->mShootingCounter,false))Bite(p);
        if(FusionStrongChomperEventDue(d->animationSpeed,p->mShootingCounter,true))
        {uint16_t k=0;uint8_t way=0;int damage=0;if(TryGetFusionStrongChomperShot(d->kind,d->batchBagCount,k,way,damage)&&Emit(p,k,way,damage,"Shoot"))++d->batchShotCount;}
    }
    else if(d->kind==1177)
    {bool due=false;if(TryGetFusionShooterEventAtCounter(*FindFusionShulk268Timing(1177),d->animationSpeed,p->mShootingCounter,0,due)&&due&&Emit(p,94,16,10,"SunFlower_head/SunFlower_head_1/flower"))++d->batchShotCount;}
    return true;
}
bool GetNativeChomper268VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!Current(p)||!IsNativeChomper268Kind(GetNativePlantData(p)->kind))return false;const auto* d=GetNativePlantData(p);state="idle";seconds=d->idleSeconds;
    if(FindFusionStrongChomperProfile(d->kind)){if(d->batchMode){state="bite";seconds=d->shootSeconds;}return true;}
    if(d->batchMode==1){int total=0;TryGetFusionShulk268PhaseTicks(d->kind,1,total);state=d->kind==33?"shoot":"shoot1";seconds=float(total-d->pendingProductionTicks)*.01f;}
    else if(d->batchMode==2||d->batchMode==3){state=d->kind==33?"shoot_loop":"shoot1_loop";seconds=d->idleSeconds;}
    return true;
}
bool GetNativeShulk268HeadVisualPose(const Plant* p,const char*& state,float& seconds)
{if(!Current(p)||GetNativePlantData(p)->kind!=1177)return false;const auto* d=GetNativePlantData(p);state=d->shootSeconds>=0?"shoot2":"head1";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return true;}
bool IsNativeChomper268Projectile(const Projectile* p)
{const auto* d=GetNativeProjectileData(p);return d&&FusionChomper268ProjectileMetadataValid(d->kind,NativeProjectileValidationDamage369(*d),d->specialType)&&
    (d->kind==89?p->mMotionType==MOTION_STRAIGHT:d->specialType==16?p->mMotionType==MOTION_HOMING:p->mMotionType==MOTION_STAR);}
bool GetNativeChomper268ProjectileRect(const Projectile* p,Sexy::Rect& rect)
{
    if(!IsNativeChomper268Projectile(p))return false;FusionRecipientRect r;
    if(!TryMakeFusionChomper268ProjectileRect(GetNativeProjectileData(p)->kind,p->mPosX+20,p->mPosY+20,p->mRotation,GetNativeFusionPixelsY(p->mBoard),r))return false;
    rect=Sexy::Rect(int(std::floor(r.x)),int(std::floor(r.y)),int(std::ceil(r.width)),int(std::ceil(r.height)));return true;
}
namespace
{
bool Retained(Zombie* z)
{return Target(z)&&FusionWater268HomingTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,!NativeWater268ZombieCaptured(z),z->mBodyHealth>0,true);}
Zombie* Nearest(Projectile* p)
{
    Zombie* best=nullptr;Zombie* z=nullptr;bool flying=false;double distance=0;const double py=GetNativeFusionPixelsY(p->mBoard);
    const double vision=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX;
    while(p->mBoard->IterateZombies(z))
    {
        const bool air=z->IsFlying();if(!Target(z)||!FusionWater268HomingTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,true,z->mBodyHealth>0,air)||(flying&&!air))continue;
        float x=0,y=0;if(!z->GetShadowCenter(x,y)||x>=vision)continue;const auto r=z->GetZombieRect();
        const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=(r.mY+r.mHeight*.5-p->mPosY-20)/py,d=dx*dx+dy*dy;
        if(!best||(air&&!flying)||d<distance){best=z;flying=air;distance=d;}
    }
    return best;
}
bool CanHit(Projectile* p,Zombie* z)
{
    if(!Target(z)||z->mBoard!=p->mBoard)return false;const auto* d=GetNativeProjectileData(p);const int s=GetNativeFusionZombieStatus(z);
    if(d->kind==94&&d->specialType==16)
    {
        if(p->mBoard->ZombieTryToGet(p->mTargetZombieID)==z&&Retained(z))return true;
        return z->mRow==p->mRow&&s!=3&&s!=7&&s!=9&&s!=12&&s!=13&&s!=16&&s!=20&&s!=21;
    }
    return FusionWater268ProjectileCanHit(d->specialType,s,z->mRow-p->mRow,z->mMindControlled,true,z->mBodyHealth>0);
}
void GarlicBomb(Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);const double x=p->mPosX+20,y=p->mPosY+20,py=GetNativeFusionPixelsY(p->mBoard);Zombie* z=nullptr;std::vector<ZombieID> ids;
    while(p->mBoard->IterateZombies(z))
    {
        const int s=GetNativeFusionZombieStatus(z);
        if(!Target(z)||std::abs(z->mRow-p->mRow)>=2||s==1||s==7||s==10||s==12||s==16||s>=20)continue;
        const auto r=z->GetZombieRect();const double dx=((std::max)(double(r.mX),(std::min)(x,double(r.mX+r.mWidth)))-x)/kFusionBasePixelsX,dy=((std::max)(double(r.mY),(std::min)(y,double(r.mY+r.mHeight)))-y)/py;
        if(dx*dx+dy*dy<=2.25)ids.push_back(p->mBoard->ZombieGetID(z));
    }
    for(auto id:ids)if((z=p->mBoard->ZombieTryToGet(id)))
    {z->TakeDamage(d->damage,2U);if(!z->mDead)SetNativePoison(z,1000);}
    LawnNet::NetTrace("Fusion native Chomper268 bomb89 id=%u damage=%d targets=%zu tick=%d",p->mNetId,d->damage,ids.size(),p->mBoard->mMainCounter);
}
}
bool UpdateNativeChomper268Projectile(Projectile* p)
{
    if(!IsNativeChomper268Projectile(p))return false;if(p->mDead||p->mNetGhost||p->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(p->mBoard))return true;
    auto* d=GetNativeProjectileData(p);const double py=GetNativeFusionPixelsY(p->mBoard);
    if(d->kind==94&&d->specialType==16)
    {
        auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);const bool noTarget=p->mTargetZombieID==ZOMBIEID_NULL;
        if(!Retained(z))
        {
            z=Nearest(p);p->mTargetZombieID=z?p->mBoard->ZombieGetID(z):ZOMBIEID_NULL;
            if(noTarget){float vx=0,vy=0;TryStepFusionShulk268Steering(p->mVelX/kFusionBasePixelsX,-p->mVelY/py,std::cos(p->mRotation),std::sin(p->mRotation),false,vx,vy);p->mVelX=vx*float(kFusionBasePixelsX*.01);p->mVelY=-vy*float(py*.01);}
        }
        else
        {
            const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=-(r.mY+r.mHeight*.5-p->mPosY-20)/py;float vx=0,vy=0;
            TryStepFusionShulk268Steering(p->mVelX/kFusionBasePixelsX,-p->mVelY/py,dx,dy,true,vx,vy);p->mVelX=vx*float(kFusionBasePixelsX*.01);p->mVelY=-vy*float(py*.01);p->mRotation=float(std::atan2(dy,dx));
        }
    }
    else if(d->kind==94)
    {p->mVelX=float(std::cos(p->mRotation)*6*kFusionBasePixelsX*.01);p->mVelY=float(-std::sin(p->mRotation)*6*py*.01);}
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    //94's field stores the actual integrated child angle: velocity can reverse,
    // so multiplying the current spin sign by total age would jump the sprite.
    if(d->kind==94)p->mRotationSpeed=std::fmod(p->mRotationSpeed+float((p->mVelX>0?-1:1)*600*.01*3.14159265358979323846/180),float(2*3.14159265358979323846));
    const double sx=(p->mPosX+20-p->mBoard->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8,sy=2.3-(p->mPosY+20-(p->mBoard->GridToPixelY(0,0)+50))/py,pitch=GetNativeFusionRowCount(p->mBoard)==6?85.:100.;
    if(p->mProjectileAge>2000||sx< -12||sx>1.35*(p->mBoard->GetColumnCount()-1)-4.8+3.85||sy<2.3-pitch/py*(GetNativeFusionRowCount(p->mBoard)-1)-.6||sy>6.5){p->Die();return true;}
    Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!CanHit(p,z))continue;const auto r=z->GetZombieRect();
        if(!FusionChomper268ProjectileTouchesRect(d->kind,p->mPosX+20,p->mPosY+20,p->mRotation,py,{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)}))continue;
        ApplyNativeChomper268ProjectileImpact(p,z);if(p->mDead)break;
    }
    return true;
}
bool ApplyNativeChomper268ProjectileImpact(Projectile* p,Zombie* z)
{
    if(!IsNativeChomper268Projectile(p)||p->mDead||p->mNetGhost||p->mApp->NetIsClient()||!CanHit(p,z))return false;auto* d=GetNativeProjectileData(p);
    if(d->kind==89){GarlicBomb(p);p->Die();return true;}
    auto* c=GetNativeContext(p->mBoard);if(!c||!RememberFusionRollingHit(c->rollingHits,p->mBoard->mProjectiles.DataArrayGetID(p),p->mBoard->ZombieGetID(z)))return true;
    z->TakeDamage(10,0U);p->PlayImpactSound(z);d->specialType=2;p->mMotionType=MOTION_STAR;
    LawnNet::NetTrace("Fusion native Shulk268 leaf hit id=%u target=%u damage=10 way=2 tick=%d",p->mNetId,z->mNetId,p->mBoard->mMainCounter);return true;
}
}
