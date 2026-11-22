#include "FusionNativeSummonPlants269.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionHostileProjectileRules.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeWater268.h"
#include "FusionNative.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeZombie313Dispatch.h"
#include "../Net/NetPresentation.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <cmath>
#include <string>

namespace LawnFusion
{
namespace
{
bool Touches(const FusionRecipientRect& a,const Sexy::Rect& b)
{ return a.x<=b.mX+b.mWidth&&a.x+a.width>=b.mX&&a.y<=b.mY+b.mHeight&&a.y+a.height>=b.mY; }
}

bool IsNativeSourceSuperCherryProjectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    if(d&&d->kind==3&&d->damage==300&&d->isZombieBullet&&d->goldAttracted&&(d->specialType==4||d->specialType==5))
        return NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);
    return d&&d->isZombieBullet&&(FusionSuperCherryProjectileMetadataValid(d->kind,d->damage,
        d->specialType,true,p->mProjectileType,p->mMotionType)||FusionCatapultCherry343MetadataValid(
        d->kind,d->damage,d->specialType,true,p->mProjectileType,p->mMotionType))&&
        NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);
}
bool UpdateNativeSourceSuperCherryProjectile(Projectile* p,NativeSuperCherryContact contact)
{
    // Way13 keeps the genuine catapult gravity/trajectory below; all contact
    // paths share the actual SuperCherryBullet -> BombCherry continuation.
    if(!IsNativeSourceSuperCherryProjectile(p)||!contact||GetNativeProjectileData(p)->specialType==13)return false;
    auto* b=p->mBoard;
    if(p->mDead||p->mNetGhost||!b||!b->mApp||b->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(b))return true;
    if(UpdateNativeSuperCherryStart(p))return true;
    const double py=GetNativeFusionPixelsY(b),px=kFusionBasePixelsX;
    const double sx=(p->mPosX+20-b->GetCellCenterX(0,0))/px-4.8;
    const double sy=2.3-(p->mPosY+20-(b->GridToPixelY(0,0)+50))/py;
    const double maxX=1.35*(b->GetColumnCount()-1)-4.8+1.85,minY=2.3-(b->GridToPixelY(0,GetNativeFusionRowCount(b)-1)-b->GridToPixelY(0,0))/py-.6;
    // Bullet.MoveDieUpdate422CF0: non-throwing right margin is exactly+1.
    if(!std::isfinite(sx)||!std::isfinite(sy)||sx< -12||sx>maxX+1||sy<minY||sy>6.5)
    {p->Die();return true;}
    if(GetNativeProjectileData(p)->goldAttracted)
    {if(!MoveNativeGoldMagnet269Projectile(p))return true;}
    else
    {p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY);}
    FusionRecipientRect rect;
    if(!TryMakeFusionShooterBatchProjectileRect(3,p->mPosX+20,p->mPosY+20,rect)){p->Die();return true;}
    const double ratio=py/kFusionBasePixelsY;
    rect.y=p->mPosY+20+(rect.y-p->mPosY-20)*ratio;rect.height*=ratio;
    Plant* plant=nullptr;
    while(b->mPlants.IterateNext(plant))
    {
        const auto* target=GetNativePlantData(plant);
        if(!target||plant->mDead||plant->mPlantHealth<=0||
            !FusionSuperCherryPlantCollides(target->kind,p->mRow,plant->mRow,NativeSourcePlantColliderEnabled(plant))||
            !Touches(rect,plant->GetPlantRect()))continue;
        if(TryReflectNativeSuperCherryProjectile(p,plant))return true;
        contact(p,SelectNativeSuperCherryPlantTarget(p,plant),true);
        return true; // A refused birth must never fall through to stock enemy collision.
    }
    Zombie* zombie=nullptr;
    while(b->mZombies.IterateNext(zombie))
    {
        const int status=GetNativeFusionZombieStatus(zombie);
        if(zombie->mDead||zombie->IsDeadOrDying()||zombie->mBodyHealth<=0||!zombie->mMindControlled||
            zombie->mRow!=p->mRow||NativeWater268ZombieCaptured(zombie)||status==7||status==10||status==12||
            (status>=20&&status<=24)||!Touches(rect,zombie->GetZombieRect()))continue;
        contact(p,nullptr,false);return true;
    }
    return true;
}

bool QueueNativeSuperCherryStart(Projectile* p,Plant* target,bool plantContact)
{
    if(!IsNativeSourceSuperCherryProjectile(p)||p->mDead||p->mNetGhost||
       !p->mBoard||p->mBoard->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(p->mBoard))return false;
    auto* context=GetNativeContext(p->mBoard);
    if(!context||(target&&(target->mBoard!=p->mBoard||!target->mNetId)))return false;
    const uint32_t uid=p->mBoard->mProjectiles.DataArrayGetID(p);
    if(!uid)return false;
    NativeSuperCherryStart start;
    start.birthAge=p->mProjectileAge;
    // A plant collision may select no remaining cover. This still retains the
    // source HitPlant offset; zombie contact uses the unshifted BombCherry.
    start.plantContact=plantContact;
    if(target){start.targetGeneration=p->mBoard->mPlants.DataArrayGetID(target);start.targetId=target->mNetId;}
    context->superCherryStarts.try_emplace(uid,start);
    GetNativeProjectileData(p)->hitLatched=true;
    p->mVelX=p->mVelY=p->mVelZ=0;
    LawnNet::NetTrace("Fusion SuperCherry Start queued projectile=%u target=%u generation=%u age=%d tick=%d",
        p->mNetId,start.targetId,start.targetGeneration,start.birthAge,p->mBoard->mMainCounter);
    return true;
}

bool UpdateNativeSuperCherryStart(Projectile* p)
{
    auto* context=GetNativeContext(p?p->mBoard:nullptr);
    if(!context||!p||p->mDead||p->mNetGhost||p->mBoard->mApp->NetIsClient())return false;
    const uint32_t uid=p->mBoard->mProjectiles.DataArrayGetID(p);
    const auto found=context->superCherryStarts.find(uid);
    if(found==context->superCherryStarts.end())return false;
    const auto start=found->second;
    if(p->mProjectileAge<=start.birthAge)return true;
    context->superCherryStarts.erase(found); // Consume before damage can recurse.
    auto* target=start.targetGeneration?p->mBoard->mPlants.DataArrayTryToGet(start.targetGeneration):nullptr;
    if(target&&target->mNetId!=start.targetId)target=nullptr;
    ApplyNativeSuperCherryProjectileImpact(p,target,start.plantContact);
    return true;
}

bool IsNativeSourcePlanarProjectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    return d&&(d->kind==58||d->kind==59||FusionCatapultCherry343MetadataValid(d->kind,d->damage,d->specialType,d->isZombieBullet,p->mProjectileType,p->mMotionType))&&d->specialType==13;
}
Projectile* SpawnNativeSourcePlanarProjectile(Board* b,uint16_t kind,int row,int damage,
    float x,float y,float vx,float vy,float gravity,bool hostile)
{
    if(!CanApplyNativeFusionGameplay(b)||b->mApp->NetIsClient()||row<0||row>=GetNativeFusionRowCount(b)||
        (kind!=58&&kind!=59&&kind!=3)||damage<=0||(kind==3&&(damage!=60||!hostile))||(kind==58&&!hostile)||(kind==59&&hostile)||
        !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(vx)||!std::isfinite(vy)||
        !std::isfinite(gravity)||gravity<=0||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    const uint16_t visuals[]={kind};std::string error;
    if(!EnsureNativeVisuals(b,{},visuals,&error))return nullptr;
    const auto type=kind==58?PROJECTILE_BASKETBALL:kind==3?PROJECTILE_PEA:PROJECTILE_MELON;
    auto* p=b->AddProjectile(int(x-20),int(y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,row,0),row,type);
    if(!p)return nullptr;
    NativeProjectileData data;data.kind=kind;data.damage=damage;data.specialType=13;data.isZombieBullet=hostile;
    p->mMotionType=MOTION_LOBBED;
    if(!AttachNativeProjectile(b,p,data)){p->Die();return nullptr;}
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;
    p->mVelX=vx;p->mVelY=vy;p->mVelZ=0;p->mAccZ=gravity;
    p->mShadowY=float(b->GetPosYBasedOnRow(x,row)+79);
    p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    p->mRotation=p->mRotationSpeed=0;
    LawnNet::NetTrace("Fusion native planar factory id=%u kind=%u row=%d damage=%d hostile=%d way=13 x=%.9g y=%.9g vx=%.9g vy=%.9g gravity=%.9g tick=%d",
        p->mNetId,kind,row,damage,hostile?1:0,x,y,vx,vy,gravity,b->mMainCounter);
    return p;
}
Projectile* TryMakeNativeHostileCatapult343(Board* b,Plant* target,int row,float x,float y,uint16_t kind)
{
    if(kind!=3&&kind!=58)return nullptr;
    if(!b||!target||target->mBoard!=b||target->mDead||!target->mIsOnBoard||target->mRow!=row)return nullptr;
    float tx=0,ty=0;if(!target->GetShadowCenter(tx,ty))return nullptr;
    FusionHostilePlanarShot shot;
    if(!TryMakeFusionHostileBasketballShot(x,y,tx,ty,shot,GetNativeFusionPixelsY(b)))return nullptr;
    auto* p=SpawnNativeSourcePlanarProjectile(b,kind,row,60,float(shot.x),float(shot.y),
        float(shot.velocityX),float(shot.velocityY),float(shot.gravityY),true);
    if(p)p->mShadowY=ty;
    return p;
}
Projectile* TryMakeNativeHostileBasketball(Board* b,Plant* target,int row,float x,float y)
{return TryMakeNativeHostileCatapult343(b,target,row,x,y,58);}
bool UpdateNativeSourcePlanarProjectile(Projectile* p)
{
    if(!IsNativeSourcePlanarProjectile(p))return false;
    auto* b=p->mBoard;
    if(p->mDead||p->mNetGhost||!b||!b->mApp||b->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(b))return true;
    if(UpdateNativeSuperCherryStart(p))return true;
    AdvanceNativeUmbrellaProjectiles(p);
    if(p->mDead)return true;
    FusionHostilePlanarShot shot{p->mPosX+20.,p->mPosY+20.,p->mVelX,p->mVelY,p->mAccZ};
    if(!AdvanceFusionHostilePlanarShot(shot)){p->Die();return true;}
    p->mPosX=float(shot.x-20);p->mPosY=float(shot.y-20);p->mVelX=float(shot.velocityX);p->mVelY=float(shot.velocityY);
    p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    auto* data=GetNativeProjectileData(p);
    FusionRecipientRect rect;
    if(!TryMakeFusionHostileProjectileRect(data->kind,shot.x,shot.y,GetNativeFusionPixelsY(b),rect)){p->Die();return true;}
    if(!data->hitLatched)
    {
        if(data->isZombieBullet)
        {
            Plant* plant=nullptr;
            while(b->mPlants.IterateNext(plant))
            {
                const auto* target=GetNativePlantData(plant);
                if(plant->mDead||!plant->mIsOnBoard||plant->mPlantHealth<=0||plant->mRow!=p->mRow||
                    !target||target->kind==254||!NativeSourcePlantColliderEnabled(plant)||!Touches(rect,plant->GetPlantRect()))continue;
                if(TryInterceptNativeUmbrella(p,plant))return true;
                data=GetNativeProjectileData(p);data->hitLatched=true;
                if(data->kind==3)
                {
                    if(TryReflectNativeSuperCherryProjectile(p,plant))return true;
                    QueueNativeSuperCherryStart(p,SelectNativeSuperCherryPlantTarget(p,plant),true);
                    return true;
                }
                const int damage=data->damage;
                ApplyNativeSourcePlantDamage(plant,damage,0);
                LawnNet::NetTrace("Fusion native hostile plant hit projectile=%u plant=%u damage=%d tick=%d",p->mNetId,plant->mNetId,damage,b->mMainCounter);
                p->Die();return true;
            }
            if(data->kind==3)
            {
                Zombie* z=nullptr;
                while(b->mZombies.IterateNext(z))
                {
                    const int status=GetNativeFusionZombieStatus(z);NativeZombie313Semantics target;
                    if(z->mDead||!z->mMindControlled||z->mRow!=p->mRow||GetNativeFusionZombieHealth(z)<=0||
                       !FusionDayZombie273ProjectileCanHit(status)||NativeWater268ZombieCaptured(z)||
                       (GetNativeZombie313Semantics(z,target)&&!target.colliderEnabled)||!Touches(rect,z->GetZombieRect()))continue;
                    QueueNativeSuperCherryStart(p,nullptr,false);
                    return true;
                }
            }
        }
        else
        {
            Zombie* zombie=nullptr;
            while(b->mZombies.IterateNext(zombie))
                if(!zombie->mDead&&!zombie->mMindControlled&&zombie->mBodyHealth>0&&zombie->mRow==p->mRow&&
                    Touches(rect,zombie->GetZombieRect())&&ApplyNativeUmbrellaProjectileImpact(p,zombie))return true;
        }
    }
    // The source tests the moving root against LandY+.5 only while falling.
    if(shot.velocityY>0&&shot.y>p->mShadowY-.5*GetNativeFusionPixelsY(b))
    {
        if(data->kind==3){QueueNativeSuperCherryStart(p,nullptr,false);return true;}
        if(data->kind==59)ApplyNativeUmbrellaProjectileImpact(p,nullptr);
        if(!p->mDead)p->Die();
    }
    return true;
}
}
