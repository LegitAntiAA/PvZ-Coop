#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeRecipientShooters.h"
#include "FusionNative.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeShooterTiming.h"
#include "FusionBasePlantRules.h"
#include "FusionCherryShooterRules.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
bool Host(const GameObject* object)
{
    return object&&object->mBoard&&object->mApp&&object->mApp->mBoard==object->mBoard&&
        !object->mNetGhost&&!object->mApp->NetIsClient()&&IsNativeFusionBoard(object->mBoard)&&
        NativeFusionBackgroundValid(static_cast<uint8_t>(object->mBoard->mBackground))&&!object->mBoard->mPaused&&object->mApp->mGameScene==SCENE_PLAYING;
}
bool Normal(Zombie* z)
{ return Host(z)&&!z->mDead&&!z->IsDeadOrDying()&&IsNativeFusionGroundZombie(z)&&!z->mMindControlled&&!z->IsFlying()&&(!GetNativeSourceZombieData(z)||(!z->mInPool&&!z->mOnHighGround)); }
const FusionShooterTimingProfile* Timing(const Plant* p)
{ const auto* d=GetNativePlantData(p);return d?FindFusionRecipientTimingProfile(d->kind,false):nullptr; }
void Play(Plant* plant)
{
    auto* body=plant->mApp->ReanimationTryToGet(plant->mBodyReanimID);
    if (body&&body->TrackExists("anim_shooting"))
    {
        int start=0,count=0;body->GetFramesForLayer("anim_shooting",start,count);
        const auto* data=GetNativePlantData(plant);
        plant->PlayBodyReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,
            static_cast<float>((count>0?count:1)/FusionShooterDuration(*Timing(plant),data->animationSpeed)));
    }
}
Zombie* Acquire(Plant* plant)
{
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return nullptr;
    const auto kind=GetNativePlantData(plant)->kind;auto* board=plant->mBoard;
    if(kind==21)
    {
        Zombie* z=nullptr;const double vision=board->GetCellCenterX(board->GetColumnCount()-1,plant->mRow)+1.85*kFusionBasePixelsX;
        while(board->IterateZombies(z))
        {float zx=0,zy=0;if(Normal(z)&&z->GetShadowCenter(zx,zy)&&FusionCactusCanAcquire(GetNativeFusionZombieStatus(z),z->mRow==plant->mRow,false,false,x,zx,vision))return z;}
        return nullptr;
    }
    for(std::size_t ray=0;ray<5;++ray)
    {
        Zombie* z=nullptr;Zombie* nearest=nullptr;double nearestDistance=0;
        while(board->IterateZombies(z))if(Normal(z))
        {
            const auto r=z->GetZombieRect();double distance=0;
            if(TryGetFusionStarAcquisitionDistance(ray,z->mRow==plant->mRow,x,y-.3*kFusionBasePixelsY,
                {double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)},distance)&&(!nearest||distance<nearestDistance))
            {nearest=z;nearestDistance=distance;}
        }
        if(nearest)return nearest;
    }
    return nullptr;
}
bool Emit(Plant* plant,const FusionRecipientEmissionSpec& spec,std::size_t index)
{
    auto* board=plant->mBoard;if(board->mProjectiles.mSize>=board->mProjectiles.mMaxSize)return false;
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return false;
    x+=static_cast<float>(spec.muzzleX*kFusionBasePixelsX);y-=static_cast<float>(spec.muzzleY*kFusionBasePixelsY);
    // STAR's stock constructor draws cosmetic RNG. Allocate its identical40px
    // native body as PEA and set STAR before metadata admission; source emission
    // owns exactly one sound draw after all five actual projectiles are created.
    auto* p=board->AddProjectile(static_cast<int>(x-20),static_cast<int>(y-20),plant->mRenderOrder-1,plant->mRow,
        spec.projectileKind==37?PROJECTILE_SPIKE:PROJECTILE_PEA);
    if(!p)return false;
    p->mProjectileType=spec.projectileKind==37?PROJECTILE_SPIKE:PROJECTILE_STAR;
    p->mMotionType=spec.projectileKind==37?MOTION_STRAIGHT:MOTION_STAR;
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    p->mVelX=static_cast<float>(spec.velocityX);p->mVelY=static_cast<float>(spec.velocityY);p->mVelZ=p->mAccZ=0;
    p->mShadowY=y;p->mRotationSpeed=spec.projectileKind==39?static_cast<float>(6*30*.01*3.14159265358979323846/180):0;
    p->mDamageRangeFlags=plant->GetDamageRangeFlags(WEAPON_PRIMARY);
    NativeProjectileData data;data.kind=spec.projectileKind;data.damage=20;data.specialType=static_cast<uint8_t>(spec.sourceMovingWay);
    if(!AttachNativeTravelProjectile369(plant,p,data)){p->Die();return false;}
    LawnNet::NetTrace("Fusion native Recipient projectile plant=%u kind=%u id=%u payload=%u way=%u shot=%zu vx=%.9g vy=%.9g damage=20",
        plant->mNetId,GetNativePlantData(plant)->kind,p->mNetId,data.kind,data.specialType,index,p->mVelX,p->mVelY);
    return true;
}
}
bool IsNativeRecipientShooter(const Plant* plant) { return Timing(plant)!=nullptr; }
const FusionShooterTimingProfile* GetNativeRecipientTimingProfile(const Plant* plant) { return Timing(plant); }
void InitializeNativeRecipientShooter(Plant* plant)
{
    const auto* profile=Timing(plant);if(!profile||!Host(plant)||plant->mDead)return;
    plant->mState=STATE_NOTREADY;plant->mStateCountdown=plant->mDoSpecialCountdown=plant->mShootingCounter=0;
    if(InitializeNativeShooterTiming(plant,*profile,false))
        LawnNet::NetTrace("Fusion native Recipient init id=%u kind=%u launch=%d speed=%.9g",plant->mNetId,profile->kind,plant->mLaunchCounter,GetNativePlantData(plant)->animationSpeed);
}
bool UpdateNativeRecipientShooter(Plant* plant)
{
    const auto* profile=Timing(plant);if(!profile)return false;if(!Host(plant)||plant->mDead)return true;
    if(--plant->mLaunchCounter>0)return true;
    if(profile->kind==21)TryGetFusionCactusCycleTicks(RandRangeFloat(-.1f,.1f),plant->mLaunchCounter);
    else ResetNativeShooterLaunchCounter(plant,*profile);
    if(GetNativePlantData(plant)->shootSeconds<0&&Acquire(plant)&&BeginNativeShooterShot(plant,*profile))Play(plant);
    return true;
}
void UpdateNativeRecipientShooterPhase(Plant* plant)
{ const auto* profile=Timing(plant);if(profile&&Host(plant)&&!plant->mDead)UpdateNativeShooterPhase(plant,*profile); }
bool DispatchNativeRecipientShot(Plant* plant)
{
    const auto* profile=Timing(plant);if(!profile)return false;if(!Host(plant)||plant->mDead)return true;
    bool due=false;const auto* data=GetNativePlantData(plant);
    if(!TryGetFusionShooterEventAtCounter(*profile,data->animationSpeed,plant->mShootingCounter,0,due)||!due)return true;
    for(std::size_t i=0;i<FusionRecipientEmissionCount(data->kind);++i)
    {FusionRecipientEmissionSpec spec;if(TryGetFusionRecipientEmission(data->kind,i,spec))Emit(plant,spec,i);}
    (void)RandRangeInt(3,4); // Source RangeInt(3,5), one draw after the entire callback.
    plant->mApp->PlayFoley(FOLEY_THROW);return true;
}
bool GetNativeRecipientVisualPose(const Plant* plant,const char*& state,float& elapsed)
{
    const auto* profile=Timing(plant);const auto* data=GetNativePlantData(plant);if(!profile||!data)return false;
    state=data->shootSeconds>=0?profile->visualState:"idle";elapsed=data->shootSeconds>=0?data->shootSeconds:data->idleSeconds;
    return std::isfinite(elapsed)&&elapsed>=0;
}
bool IsNativeRecipientProjectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    if(d&&d->kind==37&&IsNativeMore269Projectile(p))return true;
    return d&&!d->fireState&&!d->lastTorchId&&!d->lastTorchToken&&!d->melonEnhanced&&!d->effectVariant&&
        FusionRecipientProjectileMetadataValid(d->kind,NativeProjectileValidationDamage369(*d),d->sunOnHit,d->specialType,p->mProjectileType,p->mMotionType);
}
bool GetNativeRecipientProjectileCenter(const Projectile* p,float& x,float& y)
{if(!IsNativeRecipientProjectile(p))return false;x=p->mPosX+20;y=p->mPosY+p->mPosZ+20;return std::isfinite(x+y);}
bool GetNativeRecipientProjectileRect(const Projectile* p,Sexy::Rect& rect)
{
    if(!IsNativeRecipientProjectile(p))return false;FusionRecipientRect r;
    if(!TryMakeFusionRecipientProjectileRect(GetNativeProjectileData(p)->kind,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mVelX,p->mVelY,r)){rect={};return true;}
    const int x=int(std::floor(r.x)),y=int(std::floor(r.y));rect=Sexy::Rect(x,y,int(std::ceil(r.x+r.width))-x,int(std::ceil(r.y+r.height))-y);return true;
}
bool NativeRecipientProjectileCanHit(const Projectile* p,Zombie* z)
{
    if(!IsNativeRecipientProjectile(p)||!Normal(z)||p->mBoard!=z->mBoard)return false;const auto* d=GetNativeProjectileData(p);
    return FusionRecipientProjectileCanHit(d->kind,d->specialType,GetNativeFusionZombieStatus(z),z->mRow==p->mRow,false,false,GetNativeFusionZombieHealth(z));
}
bool NativeRecipientProjectileExpired(const Projectile* p)
{
    if(!IsNativeRecipientProjectile(p)||!Host(p))return false;auto* b=p->mBoard;
    const double x=(p->mPosX+20-b->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8;
    const double y=2.3-(p->mPosY+p->mPosZ+20-(b->GridToPixelY(0,0)+50))/kFusionBasePixelsY;
    return FusionCherryProjectileExpired(x,y,p->mProjectileAge,-10,1.35*(b->GetColumnCount()-1)-4.8+1.85,2.3-1.67*(kNativeFusionRows-1)-.6,6.5);
}
bool MoveNativeRecipientProjectile(Projectile* p)
{
    if(!IsNativeRecipientProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    const auto* d=GetNativeProjectileData(p);
    if(NativeRecipientProjectileExpired(p)||!FusionRecipientProjectileVelocityValid(d->kind,d->specialType,p->mVelX,p->mVelY)){p->Die();return true;}
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;return true;
}
bool ApplyNativeRecipientProjectileImpact(Projectile* p,Zombie* z)
{
    if(!IsNativeRecipientProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    if(z&&!NativeRecipientProjectileCanHit(p,z))return true;const auto data=*GetNativeProjectileData(p);
    const unsigned flags=FusionRecipientDamageFlags(data.kind,data.specialType);
    if(z)z->TakeDamage(data.damage,flags);
    p->PlayImpactSound(z);p->mApp->AddTodParticle(p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRenderOrder+1,PARTICLE_PEA_SPLAT);
    LawnNet::NetBroadcastParticle(p->mApp,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRenderOrder+1,PARTICLE_PEA_SPLAT);
    LawnNet::NetTrace("Fusion native Recipient impact projectile=%u payload=%u way=%u target=%u damage=20 flags=%u shield=%d helm=%d body=%d",
        p->mNetId,data.kind,data.specialType,z?z->mNetId:0,flags,z?z->mShieldHealth:0,z?z->mHelmHealth:0,z?z->mBodyHealth:0);
    p->Die();return true;
}
bool HandleNativeRecipientProjectileCollision(Projectile* p)
{
    if(!IsNativeRecipientProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    if(NativeRecipientProjectileExpired(p)){p->Die();return true;}
    const auto* d=GetNativeProjectileData(p);Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(NativeRecipientProjectileCanHit(p,z))
    {const auto r=z->GetZombieRect();if(FusionRecipientProjectileTouchesRect(d->kind,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mVelX,p->mVelY,
        {double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)})){ApplyNativeRecipientProjectileImpact(p,z);break;}}
    return true;
}
}
