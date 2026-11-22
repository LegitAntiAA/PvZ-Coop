#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeStar269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeWater268.h"
#include "FusionNativeDefender.h"
#include "FusionRecipientShooterRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
constexpr float delta=.01f;
bool Host(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const GameObject* p)
{return Host(p)&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
bool Storage(uint16_t kind){return kind==1084||kind==920;}
bool StarPayload(uint16_t kind){return FusionStar269ProjectileClass(kind);}
bool Sky(const Projectile* p){const auto* d=GetNativeProjectileData(p);return d&&d->kind==37&&d->specialType==8;}
bool Target(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&GetNativeFusionZombieHealth(z)>0&&!NativeWater268ZombieCaptured(z);}
FusionStar269Point Source(Board* b,float x,float y,float z=0)
{return {float((x-b->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8),float(2.3-(y-b->GridToPixelY(0,0)-50)/GetNativeFusionPixelsY(b)),float(z/GetNativeFusionPixelsY(b))};}
void Position(Projectile* p,FusionStar269Point source)
{
    auto* b=p->mBoard;const float x=float(b->GetCellCenterX(0,0)+(source.x+4.8)*kFusionBasePixelsX);
    const float y=float(b->GridToPixelY(0,0)+50+(2.3-source.y)*GetNativeFusionPixelsY(b));
    const float dy=y-(p->mPosY+p->mPosZ+20);
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY+=dy;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
}
Projectile* Resolve(Board* b,NativeStar269Reference reference)
{
    if(!reference.id||!reference.token)return nullptr;Projectile* p=nullptr;
    while(b->IterateProjectiles(p))if(!p->mDead&&p->mNetId==reference.id)
    {const auto* s=GetNativeStar269ProjectileState(p);return s&&s->instanceToken==reference.token?p:nullptr;}
    return nullptr;
}
Plant* Owner(Projectile* p,const NativeStar269ProjectileState& state)
{
    Plant* owner=nullptr;while(p->mBoard->IteratePlants(owner))
    {const auto* d=GetNativePlantData(owner);if(!owner->mDead&&owner->mIsOnBoard&&d&&owner->mNetId==state.ownerId&&d->instanceToken==state.ownerToken)return owner;}
    return nullptr;
}
void Unlink(Projectile* p,NativeStar269ProjectileState& state)
{
    if(auto* owner=Owner(p,state))if(auto* s=EditNativeStar269PlantState(owner))
        if(state.ownerSlot<s->slots.size()&&s->slots[state.ownerSlot].id==p->mNetId&&s->slots[state.ownerSlot].token==state.instanceToken)s->slots[state.ownerSlot]={};
    state.ownerId=0;state.ownerToken=0;state.ownerSlot=0;
}
void Release(Projectile* p,uint16_t kind,bool death)
{
    auto* s=EditNativeStar269ProjectileState(p);auto* d=GetNativeProjectileData(p);if(!s||!d)return;
    FusionStar269Dispatch plan;if(!TryMakeFusionStar269Dispatch(kind,death,d->damage,s->trackSpeed,plan))return;
    Unlink(p,*s);d->damage=plan.damage;d->specialType=plan.way;p->mMotionType=MOTION_HOMING;s->trackSpeed=plan.trackSpeed;s->colliderEnabled=true;
    LawnNet::NetTrace("Fusion Star269 release projectile=%u token=%u ownerKind=%u death=%d damage=%d speed=%.9g",p->mNetId,s->instanceToken,kind,death?1:0,d->damage,s->trackSpeed);
}
bool Pumpkin(Plant* p)
{
    Plant* other=nullptr;while(p->mBoard->IteratePlants(other))
    {const auto* d=GetNativePlantData(other);if(!other->mDead&&other->mIsOnBoard&&other->mPlantCol==p->mPlantCol&&other->mRow==p->mRow&&d&&d->kind==1089)return true;}
    return false;
}
void Blow(Plant* p)
{
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        if(!z->mDead&&FusionStar269StartupBlowTarget(GetNativeFusionZombieStatus(z)))z->mBlowingAway=true;
    p->mBoard->mFogBlownCountDown=4000;p->mApp->PlaySample(Sexy::SOUND_BLOVER);
}
Zombie* Acquire(Plant* p)
{
    const auto* d=GetNativePlantData(p);float x=0,y=0;if(!p->GetShadowCenter(x,y))return nullptr;
    const float vision=float(p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX);
    if(d->kind==1082)
    {
        Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        {float zx=0,zy=0;if(Target(z)&&z->GetShadowCenter(zx,zy)&&FusionStar269CactusCanAcquire(GetNativeFusionZombieStatus(z),z->mMindControlled,z->mRow==p->mRow,x,zx,vision))return z;}
        return nullptr;
    }
    for(std::size_t ray=0;ray<5;++ray)
    {
        Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(Target(z)&&FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),z->mMindControlled,z->IsDeadOrDying()))
        {const auto r=z->GetZombieRect();double distance=0;if(TryGetFusionStarAcquisitionDistance(ray,z->mRow==p->mRow,x,y-.3*GetNativeFusionPixelsY(p->mBoard),{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)},distance))return z;}
    }
    return nullptr;
}
bool Emit(Plant* plant,std::size_t index)
{
    auto* b=plant->mBoard;const auto* d=GetNativePlantData(plant);const auto* definition=FusionFindPlant(d->kind);
    int initial=0;if(const auto* timing=GetNativeStar269Timing(plant))if(TryGetFusionShooterShotCounter(*timing,d->animationSpeed,initial))GetNativePlantData(plant)->shootSeconds=(initial-plant->mShootingCounter)*.01f;
    FusionStar269Shot spec;if(!definition||!TryMakeFusionStar269Shot(d->kind,GetNativeFireLightLevel(plant),GetNativeStar269MagnetCount(plant),definition->damage,spec))return false;
    char node[8]="Shoot";if(d->kind!=1082){node[5]=char('1'+index);node[6]=0;}
    float x=0,y=0;if(!GetNativeSourcePlantNodePoint(plant,node,x,y)||!EnsureNativeProjectileVisuals(b,spec.projectile)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return false;
    FusionRecipientEmissionSpec ray;if(!TryGetFusionRecipientEmission(d->kind==1082?21:23,index,ray))return false;
    auto* p=b->AddProjectile(int(x-20),int(y-20),plant->mRenderOrder-1,plant->mRow,d->kind==1082?PROJECTILE_SPIKE:PROJECTILE_PEA);if(!p)return false;
    p->mProjectileType=d->kind==1082?PROJECTILE_SPIKE:PROJECTILE_STAR;p->mMotionType=d->kind==1082?MOTION_STRAIGHT:MOTION_STAR;
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY=y;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    p->mVelX=float(ray.velocityX);p->mVelY=float(ray.velocityY/kFusionBasePixelsY*GetNativeFusionPixelsY(b));p->mVelZ=p->mAccZ=0;
    p->mRotation=std::atan2(float(-p->mVelY/GetNativeFusionPixelsY(b)),float(p->mVelX/kFusionBasePixelsX));p->mRotationSpeed=0;
    p->mDamageRangeFlags=plant->GetDamageRangeFlags(WEAPON_PRIMARY);
    NativeProjectileData data;data.kind=spec.projectile;data.damage=spec.damage;data.specialType=d->kind==1082?8:uint8_t(ray.sourceMovingWay);data.effectVariant=spec.special;
    if(!AttachNativeTravelProjectile369(plant,p,data)){p->Die();return false;}
    if(StarPayload(data.kind)&&!NativeStar269ProjectileCreated(p)){p->Die();return false;}
    LawnNet::NetTrace("Fusion Star269 shot plant=%u kind=%u projectile=%u payload=%u way=%u damage=%d light=%d",plant->mNetId,d->kind,p->mNetId,data.kind,data.specialType,data.damage,GetNativeFireLightLevel(plant));return true;
}
bool HomingEligible(Projectile* p,Zombie* z)
{
    if(!Target(z))return false;const auto* d=GetNativeProjectileData(p);const auto* s=GetNativeStar269ProjectileState(p);
    const int status=GetNativeFusionZombieStatus(z);
    if(d->kind==42){if(status!=10)return false;}
    else if(!(d->kind==49&&status==10)&&!FusionPlantSearchUniqueZombieAllowed(status,z->mMindControlled,z->IsDeadOrDying()))return false;
    const bool allRows=s->accelerated||d->effectVariant==4||(d->kind==40&&d->effectVariant==1);
    if((d->kind==40||d->kind==48)&&!allRows&&z->mRow!=p->mRow)return false;
    float zx=0,zy=0;return z->GetShadowCenter(zx,zy)&&zx<float(p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,z->mRow)+1.85*kFusionBasePixelsX);
}
void Retarget(Projectile* p,NativeStar269ProjectileState& state)
{
    Zombie* nearest=nullptr;Zombie* z=nullptr;double best=0;const bool preferAir=GetNativeProjectileData(p)->kind==49;
    while(p->mBoard->IterateZombies(z))if(HomingEligible(p,z))
    {if(preferAir&&nearest&&nearest->IsFlying()&&!z->IsFlying())continue;const auto r=z->GetZombieRect();const auto a=Source(p->mBoard,p->mPosX+20,p->mPosY+p->mPosZ+20),t=Source(p->mBoard,float(r.mX+r.mWidth*.5),float(r.mY+r.mHeight*.5));const double dx=t.x-a.x,dy=t.y-a.y,dist=dx*dx+dy*dy;if(!nearest||(preferAir&&z->IsFlying()&&!nearest->IsFlying())||dist<best){nearest=z;best=dist;}}
    p->mTargetZombieID=nearest?p->mBoard->ZombieGetID(nearest):ZOMBIEID_NULL;
    state.targetZombieId=nearest?nearest->mNetId:0;state.targetZombieToken=nearest?p->mBoard->mZombies.DataArrayGetID(nearest):0;
}
void Track(Projectile* p,NativeStar269ProjectileState& s)
{
    const auto* d=GetNativeProjectileData(p);auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);
    if(!z||z->mDead)
    {
        Retarget(p,s);
        if(d->kind==40||d->kind==42||d->kind==48||d->kind==49){p->mVelX=float(-s.trackSpeed/(d->kind==49?4:6)*kFusionBasePixelsX*.01);p->mVelY=0;s.colliderEnabled=false;}
        else {p->mVelX=float(std::cos(p->mRotation)*s.trackSpeed*kFusionBasePixelsX*.01);p->mVelY=float(-std::sin(p->mRotation)*s.trackSpeed*GetNativeFusionPixelsY(p->mBoard)*.01);}
        return;
    }
    // Source invalid-target path acquires now but retains velocity/collider until next update.
    const int status=GetNativeFusionZombieStatus(z);const bool lantern=d->kind==40||d->kind==42||d->kind==48||d->kind==49;
    const bool badStatus=lantern?status==1:status==1||status==7||status==12||status==16||(status>=21&&status<=24);
    if(!Target(z)||badStatus){Retarget(p,s);return;}
    const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=-(r.mY+r.mHeight*.5-p->mPosY-p->mPosZ-20)/GetNativeFusionPixelsY(p->mBoard),length=std::hypot(dx,dy);
    s.colliderEnabled=true;if(length>1e-5){p->mVelX=float(dx/length*s.trackSpeed*kFusionBasePixelsX*.01);p->mVelY=float(-dy/length*s.trackSpeed*GetNativeFusionPixelsY(p->mBoard)*.01);}else p->mVelX=p->mVelY=0;
    p->mRotation=float(std::atan2(dy,dx));s.targetZombieId=z->mNetId;s.targetZombieToken=p->mBoard->mZombies.DataArrayGetID(z);
}
}
bool IsNativeStar269Plant(const Plant* p){const auto* d=GetNativePlantData(p);return d&&FindFusionStar269Profile(d->kind);}
const FusionShooterTimingProfile* GetNativeStar269Timing(const Plant* p)
{const auto* d=GetNativePlantData(p);return d?FindFusionStar269TimingProfile(d->kind):nullptr;}
bool InitializeNativeStar269Plant(Plant* p)
{
    if(!IsNativeStar269Plant(p))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);
    p->mState=STATE_NOTREADY;p->mStateCountdown=p->mDoSpecialCountdown=p->mShootingCounter=0;d->shootSeconds=-1;d->batchMode=0;
    if(Storage(d->kind))
    {auto* s=EditNativeStar269PlantState(p,true);if(!s)return false;*s={};d->animationSpeed=RandRangeFloat(.9f,1.1f);return BeginNativeShooterShot(p,*GetNativeStar269Timing(p));}
    if(d->kind==1089){InitializeNativeDefender(p);return true;}
    if(!InitializeNativeShooterTiming(p,*GetNativeStar269Timing(p),false))return false;
    if(d->kind==1082)Blow(p);return true;
}
bool UpdateNativeStar269Plant(Plant* p)
{
    if(!IsNativeStar269Plant(p))return false;if(!Playing(p)||p->mDead)return true;auto* d=GetNativePlantData(p);
    if(d->kind==1104&&GetNativeFireLightLevel(p)>=4)TryActivateNativeStar269Meteorite(p);
    if(Storage(d->kind))
    {
        auto* s=EditNativeStar269PlantState(p);if(!s||!s->attracting)return true;
        float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"Blover_head",x,y))return true;
        const auto center=Source(p->mBoard,x,y);const auto* profile=FindFusionStar269Profile(d->kind);
        std::array<uint32_t,240> occupied{};
        for(std::size_t i=0;i<profile->storageSlots;++i){if(auto* q=Resolve(p->mBoard,s->slots[i]))occupied[i]=p->mBoard->mProjectiles.DataArrayGetID(q);else s->slots[i]={};}
        Projectile* q=nullptr;while(p->mBoard->IterateProjectiles(q))
        {
            auto* qs=EditNativeStar269ProjectileState(q);auto* qd=GetNativeProjectileData(q);if(!qs||!qd||q->mDead||!qs->colliderEnabled)continue;
            const auto point=Source(p->mBoard,q->mPosX+20,q->mPosY+q->mPosZ+20);
            const bool nearby=FusionStar269CaptureTouches(center,point,q->mRotation);
            if(!FusionStar269CanCapture(s->attracting,StarPayload(qd->kind),qs->accelerated,qd->specialType,nearby))continue;
            std::size_t slot=0;if(!TryFindFusionStar269FreeSlot(d->kind,Pumpkin(p),occupied.data(),profile->storageSlots,slot))break;
            s->slots[slot]={q->mNetId,qs->instanceToken};occupied[slot]=p->mBoard->mProjectiles.DataArrayGetID(q);
            qs->ownerId=p->mNetId;qs->ownerToken=d->instanceToken;qs->ownerSlot=uint16_t(slot);qs->accelerated=true;qs->colliderEnabled=false;qd->specialType=10;q->mMotionType=MOTION_STAR;
            LawnNet::NetTrace("Fusion Star269 capture plant=%u token=%u projectile=%u token=%u slot=%zu capacity=%d",p->mNetId,d->instanceToken,q->mNetId,qs->instanceToken,slot,FusionStar269Capacity(d->kind,Pumpkin(p)));
        }
        return true;
    }
    const auto* clock=GetNativeStar269Timing(p);if(!clock)return true;
    if(--p->mLaunchCounter<=0){ResetNativeShooterLaunchCounter(p,*clock);if(d->shootSeconds<0&&Acquire(p))BeginNativeShooterShot(p,*clock);}return true;
}
void UpdateNativeStar269PlantPhase(Plant* p)
{if(IsNativeStar269Plant(p)&&Playing(p)&&!p->mDead){const auto* clock=GetNativeStar269Timing(p);if(clock)UpdateNativeShooterPhase(p,*clock);else GetNativePlantData(p)->idleSeconds+=.01f;}}
bool DispatchNativeStar269Shot(Plant* p)
{
    if(!IsNativeStar269Plant(p))return false;if(!Playing(p)||p->mDead)return true;
    const auto* d=GetNativePlantData(p);const auto* clock=GetNativeStar269Timing(p);if(!clock)return true;
    int events=0;if(!TryGetFusionShooterEventsAtCounter(*clock,d->animationSpeed,p->mShootingCounter,events)||!events)return true;
    if(Storage(d->kind)){if(auto* s=EditNativeStar269PlantState(p)){s->attracting=true;GetNativePlantData(p)->batchMode=1;Blow(p);}return true;}
    for(int e=0;e<events;++e){const std::size_t count=d->kind==1082?1:5;for(std::size_t i=0;i<count;++i)Emit(p,i);(void)RandRangeInt(3,4);p->mApp->PlayFoley(FOLEY_THROW);++GetNativePlantData(p)->batchShotCount;}return true;
}
bool GetNativeStar269VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!IsNativeStar269Plant(p))return false;const auto* d=GetNativePlantData(p);
    if(Storage(d->kind)){state=d->shootSeconds>=0?"blow":d->batchMode?"blowloop":"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;}
    else {state=d->shootSeconds>=0?"shoot":d->kind==1082?"blowloop":"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;}
    return std::isfinite(seconds)&&seconds>=0;
}
void NativeStar269PlantRemoving(Plant* p,int reason)
{
    if(!Host(p)||!IsNativeStar269Plant(p))return;const auto* d=GetNativePlantData(p);auto* s=EditNativeStar269PlantState(p);if(!s)return;
    for(auto& ref:s->slots){if(auto* q=Resolve(p->mBoard,ref)){if(reason!=255)Release(q,d->kind,true);else q->Die();}ref={};}s->attracting=false;
}
bool NativeStar269Invulnerable(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&d->kind==1095&&IsNativeFusionBoard(p->mBoard);}
bool NativeStar269ProjectileCreated(Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);if(!Host(p)||!d||!StarPayload(d->kind))return false;
    if(GetNativeStar269ProjectileState(p))return true;auto* s=EditNativeStar269ProjectileState(p,true);if(!s)return false;
    *s={};s->instanceToken=p->mBoard->mProjectiles.DataArrayGetID(p);if(d->kind==49)s->trackSpeed=8;
    // Raw source child phase is independent of the root collider orientation.
    p->mRotation=std::atan2(float(-p->mVelY/GetNativeFusionPixelsY(p->mBoard)),float(p->mVelX/kFusionBasePixelsX));p->mRotationSpeed=0;
    return s->instanceToken!=0;
}
void NativeStar269ProjectileRemoving(Projectile* p)
{if(Host(p))if(auto* s=EditNativeStar269ProjectileState(p))Unlink(p,*s);}
bool IsNativeStar269Projectile(const Projectile* p)
{const auto* d=GetNativeProjectileData(p);return d&&(Sky(p)||(StarPayload(d->kind)&&GetNativeStar269ProjectileState(p)));}
bool NativeStar269ProjectileColliderEnabled(const Projectile* p)
{const auto* s=GetNativeStar269ProjectileState(p);return !s||s->colliderEnabled;}
bool GetNativeStar269ProjectileCenter(const Projectile* p,float& x,float& y)
{if(!IsNativeStar269Projectile(p))return false;x=p->mPosX+20;y=p->mPosY+p->mPosZ+20;return std::isfinite(x+y);}
bool GetNativeStar269ProjectileRect(const Projectile* p,Sexy::Rect& result)
{
    if(const auto* d=GetNativeProjectileData(p);d&&(d->kind==41||d->kind==42)&&d->specialType!=6&&d->specialType!=10&&!d->goldAttracted)return false;
    if(!IsNativeStar269Projectile(p))return false;result={};if(!NativeStar269ProjectileColliderEnabled(p))return true;
    FusionRecipientRect r;float vx=p->mVelX,vy=float(p->mVelY/GetNativeFusionPixelsY(p->mBoard)*kFusionBasePixelsY);
    if(vx==0&&vy==0)vx=1;
    if(!TryMakeFusionRecipientProjectileRect(Sky(p)?37:39,p->mPosX+20,p->mPosY+p->mPosZ+20,vx,vy,r))return true;
    // Shared Star root collider projects source Y to the actual current terrain pitch.
    const double cy=p->mPosY+p->mPosZ+20,ratio=GetNativeFusionPixelsY(p->mBoard)/kFusionBasePixelsY;
    r.y=cy+(r.y-cy)*ratio;r.height*=ratio;const int x=int(std::floor(r.x)),y=int(std::floor(r.y));result=Sexy::Rect(x,y,int(std::ceil(r.x+r.width))-x,int(std::ceil(r.y+r.height))-y);return true;
}
bool MoveNativeStar269Projectile(Projectile* p)
{
    if(!IsNativeStar269Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    if(Sky(p))
    {p->mPosX+=p->mVelX;const auto point=Source(p->mBoard,p->mPosX+20,p->mPosY+20);if(FusionStar269HomingOutsideX(point.x,-10,float(1.35*(p->mBoard->GetColumnCount()-1)-4.8+1.85)))p->Die();p->mX=int(p->mPosX);return true;}
    auto* s=EditNativeStar269ProjectileState(p);auto* d=GetNativeProjectileData(p);
    // Keep the proven Water268 straight/penetrating leaf until real capture or
    // its natural source tracking transition. No duplicate movement/spin.
    if((d->kind==41||d->kind==42)&&d->specialType!=6&&d->specialType!=10)return false;
    p->mRotationSpeed=std::fmod(p->mRotationSpeed+float(s->trackSpeed*30*.01*3.14159265358979323846/180),float(2*3.14159265358979323846));
    if(d->specialType==10)
    {
        auto* owner=Owner(p,*s);if(!owner){Unlink(p,*s);d->specialType=6;p->mMotionType=MOTION_HOMING;s->colliderEnabled=true;}
        else
        {
            const auto* od=GetNativePlantData(owner);float after=0;bool release=false;
            if(!TryAdvanceFusionStar269Hold(od->kind,s->elapsed,delta,after,release))return true;
            if(release)Release(p,od->kind,false);
            else
            {
                float x=0,y=0;if(!GetNativeSourcePlantNodePoint(owner,"Blover_head",x,y))return true;s->elapsed=after;s->colliderEnabled=false;
                const auto center=Source(p->mBoard,x,y);auto point=Source(p->mBoard,p->mPosX+20,p->mPosY+p->mPosZ+20);
                if(s->movingToCircle)
                {const double dx=point.x-center.x,dy=point.y-center.y,length=std::hypot(dx,dy);s->targetX=center.x+(length>1e-5?float(dx/length*.8):0);s->targetY=center.y+(length>1e-5?float(dy/length*.8):0);TryAdvanceFusionStar269Approach(center,point,delta,point,s->movingToCircle,s->angle);}
                else TryAdvanceFusionStar269Orbit(center,delta,s->angularSpeed,s->angle,point);
                Position(p,point);return true;
            }
        }
    }
    if((d->kind==40||d->kind==48)&&p->mProjectileAge>30)d->specialType=6;
    p->mMotionType=d->specialType==6?MOTION_HOMING:MOTION_STAR;
    if(d->specialType==6)Track(p,*s);
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;
    const auto point=Source(p->mBoard,p->mPosX+20,p->mPosY+p->mPosZ+20);
    const float maximum=float(1.35*(p->mBoard->GetColumnCount()-1)-4.8+1.85);
    const float minimumY=float(2.3-(GetNativeFusionRowCount(p->mBoard)-1)*(GetNativeFusionRowCount(p->mBoard)==6?1.45:1.67)-.6);
    if(FusionStar269HomingOutsideX(point.x,-10,maximum)||(d->specialType!=6&&(point.y<minimumY||point.y>6.5f)))p->Die();
    p->mX=int(p->mPosX);p->mY=int(p->mPosY+p->mPosZ);return true;
}
bool ApplyNativeStar269ProjectileImpact(Projectile* p,Zombie* z)
{
    if(const auto* d=GetNativeProjectileData(p);d&&(d->kind==41||d->kind==42)&&d->specialType!=6&&d->specialType!=10&&!d->goldAttracted)return false;
    if(!IsNativeStar269Projectile(p))return false;if(!Playing(p)||p->mDead||!NativeStar269ProjectileColliderEnabled(p))return true;
    const auto data=*GetNativeProjectileData(p);if(z&&!Target(z))return true;
    if(Sky(p)){if(z&&FusionStar269CanHitStatus(8,GetNativeFusionZombieStatus(z))){z->TakeDamage(data.damage,0U);p->PlayImpactSound(z);p->Die();}return true;}
    if(data.kind==41||data.kind==42||data.kind==46||data.kind==49)
    {
        if(!z)return true;int damage=data.damage;
        if(data.kind==46){if(z->mShieldHealth>(std::numeric_limits<int>::max)()-damage)return true;damage+=z->mShieldHealth;}
        bool broken=false;
        if(data.kind==49){const int kind=GetNativeFusionZombieKind(z);if((kind==26||kind==205||kind==206)&&!TryApplyNativeStar269UltimateRecipient(z,broken))return true;}
        if(!broken){z->TakeDamage(damage,0U);p->PlayImpactSound(z);}p->Die();return true;
    }
    const int first=z?(z->mHelmType==HELMTYPE_BUCKET?2:z->mHelmType==HELMTYPE_FOOTBALL?4:0):0;
    const int second=z?(z->mShieldType==SHIELDTYPE_DOOR?1:z->mShieldType==SHIELDTYPE_LADDER?3:0):0;
    FusionStar269Hit hit;if(!TryMakeFusionStar269Hit(data.kind,data.specialType,data.damage,first,second,hit))return true;
    if(hit.area)
    {
        std::vector<ZombieID> targets;Zombie* other=nullptr;const int row=z?z->mRow:p->mRow;
        while(p->mBoard->IterateZombies(other))if(Target(other)&&other->mRow==row&&!other->IsFlying())
        {const auto r=other->GetZombieRect();const double cy=p->mPosY+p->mPosZ+20,ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);if(FusionFireCircleTouchesRect(p->mPosX+20,cy,hit.radius,r.mX,cy+(r.mY-cy)*ratio,r.mWidth,r.mHeight*ratio))targets.push_back(p->mBoard->ZombieGetID(other));}
        // SmallBomb -> Bomb virtual TakeDamage(Explode10,...), source same row.
        for(auto id:targets)if((other=p->mBoard->ZombieTryToGet(id))&&Target(other))ApplyNativeSourceExplosionDamage(other,hit.damage);
    }
    else if(z)z->TakeDamage(hit.damage,hit.sourceDamageType==4?1U:0U);
    p->PlayImpactSound(z);p->mApp->AddTodParticle(p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRenderOrder+1,data.kind==43?PARTICLE_POWIE:PARTICLE_PEA_SPLAT);
    LawnNet::NetBroadcastParticle(p->mApp,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRenderOrder+1,data.kind==43?PARTICLE_POWIE:PARTICLE_PEA_SPLAT);
    LawnNet::NetTrace("Fusion Star269 impact projectile=%u payload=%u target=%u damage=%d sourceType=%u",p->mNetId,data.kind,z?z->mNetId:0,hit.damage,hit.sourceDamageType);p->Die();return true;
}
bool HandleNativeStar269ProjectileCollision(Projectile* p)
{
    if(const auto* d=GetNativeProjectileData(p);d&&(d->kind==41||d->kind==42)&&d->specialType!=6&&d->specialType!=10&&!d->goldAttracted)return false;
    if(!IsNativeStar269Projectile(p))return false;if(!Playing(p)||p->mDead||!NativeStar269ProjectileColliderEnabled(p))return true;
    const auto* d=GetNativeProjectileData(p);Sexy::Rect r;if(!GetNativeStar269ProjectileRect(p,r)||r.mWidth<=0||r.mHeight<=0)return true;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        const int status=GetNativeFusionZombieStatus(z);
        if(!Target(z)||!FusionStar269CanHitStatus(d->specialType,status)||(d->specialType==9&&z->mRow!=p->mRow))continue;
        const auto zr=z->GetZombieRect();const double cy=p->mPosY+p->mPosZ+20,ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
        float vx=p->mVelX,vy=float(p->mVelY*ratio);if(vx==0&&vy==0)vx=1;
        if(FusionRecipientProjectileTouchesRect(Sky(p)?37:39,p->mPosX+20,cy,vx,vy,{double(zr.mX),cy+(zr.mY-cy)*ratio,double(zr.mWidth),zr.mHeight*ratio}))
        {ApplyNativeStar269ProjectileImpact(p,z);break;}
    }
    return true;
}
}
