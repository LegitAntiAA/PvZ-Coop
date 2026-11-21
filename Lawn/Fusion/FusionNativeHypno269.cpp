#include "FusionNativeHypno269.h"
#include "FusionNative.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeVisual.h"
#include "FusionNativeWater268.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
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
bool Host(const GameObject* o)
{return o&&o->mApp&&o->mBoard&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&IsNativeFusionBoard(o->mBoard);}
bool Playing(const GameObject* o)
{return Host(o)&&!o->mBoard->mPaused&&o->mApp->mGameScene==SCENE_PLAYING;}
bool Target(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&GetNativeFusionZombieHealth(z)>0&&!z->mMindControlled&&!NativeWater268ZombieCaptured(z);}
void HitSound(Projectile* p,Zombie* z)
{
    // Bullet.PlaySound precedes damage/MC RNG. Source cone/football plays
    // both flesh and plastic samples; metal and ordinary flesh use one draw.
    if(!z)(void)RandRangeInt(0,2);
    else if(z->mShieldType!=SHIELDTYPE_NONE)(void)RandRangeInt(14,15);
    else if(z->mHelmType==HELMTYPE_TRAFFIC_CONE||z->mHelmType==HELMTYPE_FOOTBALL)
    {(void)RandRangeInt(0,2);(void)RandRangeInt(12,13);}
    else if(z->mHelmType!=HELMTYPE_NONE)(void)RandRangeInt(14,15);
    else(void)RandRangeInt(0,2);
    if(z)p->PlayImpactSound(z);else p->mApp->PlayFoley(FOLEY_SPLAT);
}
bool Acquire(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
    const double right=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        float zx=0,zy=0;
        if(Target(z)&&z->GetShadowCenter(zx,zy)&&zx>x&&zx<right&&
            FusionPuffLove269CanHit(GetNativeFusionZombieStatus(z),z->mRow==p->mRow,false,false))return true;
    }
    return false;
}
void Emit(Plant* plant)
{
    auto* d=GetNativePlantData(plant);auto* b=plant->mBoard;uint16_t payload=0;int damage=0;
    if(!TryGetFusionHypno269Projectile(d->kind,80,GetNativeHypno269PlantLevel(plant),payload,damage)||
        b->mProjectiles.mSize>=b->mProjectiles.mMaxSize||!EnsureNativeProjectileVisuals(b,payload))return;
    float x=0,y=0;if(!GetNativeSourcePlantNodePoint(plant,"Shoot",x,y))return;x+=float(.1*kFusionBasePixelsX);
    NativeProjectileData data;data.kind=payload;data.damage=damage;data.specialType=0;
    const auto type=NativeProjectileMetadataType(data);const auto motion=NativeProjectileMetadataMotion(data);
    if(!NativeProjectileMetadataValid(data,type,motion))return;
    auto* projectile=b->AddProjectile(int(x-20),int(y-20),plant->mRenderOrder-1,plant->mRow,type);if(!projectile)return;
    projectile->mMotionType=motion;projectile->mPosX=x-20;projectile->mPosY=y-20;
    projectile->mPosZ=0;projectile->mX=int(projectile->mPosX);projectile->mY=int(projectile->mPosY);
    projectile->mVelX=float(6*kFusionBasePixelsX*.01);projectile->mVelY=projectile->mVelZ=projectile->mAccZ=0;
    projectile->mShadowY=y;projectile->mRotation=projectile->mRotationSpeed=0;
    projectile->mDamageRangeFlags=plant->GetDamageRangeFlags(WEAPON_PRIMARY);
    if(!AttachNativeProjectile(b,projectile,data)){projectile->Die();return;}
    if(d->batchShotCount<uint32_t((std::numeric_limits<int>::max)()))++d->batchShotCount;
    plant->mApp->PlayFoley(FOLEY_THROW);
    LawnNet::NetTrace("Fusion Hypno269 emit source=%u token=%u kind=%u projectile=%u payload=14 damage=%d status=%u shotStatus=%u x=%.9g y=%.9g tick=%d",
        plant->mNetId,d->instanceToken,d->kind,projectile->mNetId,damage,d->batchMode,d->batchBagCount,x,y,b->mMainCounter);
}
}
bool IsNativeHypno269Plant(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&FindFusionHypno269TimingProfile(d->kind);}
const FusionShooterTimingProfile* GetNativeHypno269Timing(const Plant* p)
{
    const auto* d=GetNativePlantData(p);return d?FindFusionHypno269TimingProfile(d->kind,
        d->shootSeconds>=0?int(d->batchBagCount):int(d->batchMode)):nullptr;
}
bool InitializeNativeHypno269Plant(Plant* p)
{
    if(!IsNativeHypno269Plant(p)||!Host(p))return false;auto* d=GetNativePlantData(p);
    if(!InitializeNativeShooterTiming(p,*FindFusionHypno269TimingProfile(d->kind),true))return false;
    p->mPlantHealth=p->mPlantMaxHealth=300;p->mState=STATE_READY;p->mShootingCounter=0;
    p->mStateCountdown=d->kind==924?-100:0;d->productionTicks=p->mStateCountdown;
    d->pendingProductionTicks=0;d->batchMode=0;d->batchBagCount=0;d->batchShotCount=d->batchRageTicks=0;return true;
}
void UpdateNativeHypno269Phase(Plant* p)
{
    if(!IsNativeHypno269Plant(p)||!Playing(p)||p->mDead)return;auto* d=GetNativePlantData(p);
    if(d->kind==924&&p->mStateCountdown==0){d->batchMode=0;p->mStateCountdown=-100;}
    d->productionTicks=d->kind==924?p->mStateCountdown:0;
    if(const auto* profile=GetNativeHypno269Timing(p))UpdateNativeShooterPhase(p,*profile);
}
bool UpdateNativeHypno269Plant(Plant* p)
{
    if(!IsNativeHypno269Plant(p))return false;if(!Playing(p)||p->mDead)return true;auto* d=GetNativePlantData(p);
    if(--p->mLaunchCounter<0)
    {
        auto cycle=*FindFusionHypno269TimingProfile(d->kind);
        cycle.attackIntervalSeconds=float(p->mLaunchRate)*.01f;
        ResetNativeShooterLaunchCounter(p,cycle);
        if(Acquire(p))
        {
            d->batchBagCount=d->batchMode;
            if(const auto* profile=GetNativeHypno269Timing(p))BeginNativeShooterShot(p,*profile);
        }
    }
    // UltimateHypno.Update invokes Shooter.Update before applying Travel19.
    if(d->kind==924){const bool ultimate=HasNativeHypno269Ultimate(p,19);if(ultimate)d->batchMode=5;p->mLaunchRate=ultimate?100:200;}
    return true;
}
bool DispatchNativeHypno269Shot(Plant* p)
{
    if(!IsNativeHypno269Plant(p))return false;if(!Playing(p)||p->mDead)return true;
    const auto* d=GetNativePlantData(p);const auto* profile=GetNativeHypno269Timing(p);int count=0;
    if(d->shootSeconds>=0&&profile&&TryGetFusionShooterEventsAtCounter(*profile,d->animationSpeed,p->mShootingCounter,count))
        for(int i=0;i<count;++i)Emit(p);
    return true;
}
bool CanUseNativeHypno269Item(const Plant* p)
{
    const auto* d=GetNativePlantData(p);return Host(p)&&!p->mDead&&d&&d->kind==924&&
        (d->batchMode==0||d->batchMode==4||d->batchMode==5)&&(p->mStateCountdown==-100||p->mStateCountdown>=0)&&
        p->mStateCountdown<=(std::numeric_limits<int>::max)()-3000;
}
bool TryUseNativeHypno269Item(Plant* p,Coin* item)
{
    if(!CanUseNativeHypno269Item(p))return false;
    if(item&&(item->mBoard!=p->mBoard||!ConsumeNativeEquipment(item)))return false;
    auto* d=GetNativePlantData(p);d->batchMode=d->batchMode?5:4;p->mStateCountdown+=3000;d->productionTicks=p->mStateCountdown;
    LawnNet::NetTrace("Fusion Hypno269 charge source=%u token=%u status=%u attributeTicks=%d tick=%d",p->mNetId,d->instanceToken,d->batchMode,p->mStateCountdown,p->mBoard->mMainCounter);return true;
}
bool GetNativeHypno269ChargeNodes(const Plant* p,bool& first,bool& second)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=924)return false;
    first=d->batchMode==4||d->batchMode==5;second=d->batchMode==5;return true;
}
bool GetNativeHypno269VisualLayers(const Plant* p,std::array<NativeHypno269Layer,3>& layers,int& count)
{
    if(!IsNativeHypno269Plant(p))return false;layers={};count=0;const auto* d=GetNativePlantData(p);
    if(d->shootSeconds>=0){const auto* profile=GetNativeHypno269Timing(p);if(!profile)return false;layers[count++]={profile->visualState,d->shootSeconds,1};}
    bool first=false,second=false;if(GetNativeHypno269ChargeNodes(p,first,second))
    {if(first)layers[count++]={"magnet",d->idleSeconds,2};if(second)layers[count++]={"magnet",d->idleSeconds,3};}
    return true;
}
bool IsNativeHypno269Projectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);return d&&FusionHypno269ProjectileMetadataValid(d->kind,d->damage,d->specialType)&&
        p->mProjectileType==PROJECTILE_PUFF&&p->mMotionType==MOTION_STRAIGHT;
}
bool MoveNativeHypno269Projectile(Projectile* p)
{
    if(!IsNativeHypno269Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    auto* b=p->mBoard;const double py=GetNativeFusionPixelsY(b),x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;
    if(x<b->GetCellCenterX(0,0)-7.2*kFusionBasePixelsX||x>b->GetCellCenterX(b->GetColumnCount()-1,p->mRow)+3.85*kFusionBasePixelsX||
        y<b->GridToPixelY(0,0)+50-4.2*py||y>b->GridToPixelY(0,GetNativeFusionRowCount(b)-1)+50+.6*py){p->Die();return true;}
    p->mPosX+=p->mVelX;p->mX=int(p->mPosX);return true;
}
bool GetNativeHypno269ProjectileRect(const Projectile* p,Sexy::Rect& rect)
{
    if(!IsNativeHypno269Projectile(p))return false;
    // PuffLove enabled root Box, scale1: offset(0,-.08048099), size(.32,.43096197).
    const double px=kFusionBasePixelsX,py=GetNativeFusionPixelsY(p->mBoard),cx=p->mPosX+20,cy=p->mPosY+p->mPosZ+20+.08048099*py;
    const int x=int(std::floor(cx-.16*px)),y=int(std::floor(cy-.215480985*py));
    rect=Sexy::Rect(x,y,int(std::ceil(cx+.16*px))-x,int(std::ceil(cy+.215480985*py))-y);return true;
}
bool NativeHypno269ProjectileCanHit(const Projectile* p,Zombie* z)
{return IsNativeHypno269Projectile(p)&&Target(z)&&p->mBoard==z->mBoard&&FusionPuffLove269CanHit(GetNativeFusionZombieStatus(z),p->mRow==z->mRow,z->mMindControlled,NativeWater268ZombieCaptured(z));}
bool ApplyNativeHypno269ProjectileImpact(Projectile* p,Zombie* primary)
{
    if(!IsNativeHypno269Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    if(primary&&!NativeHypno269ProjectileCanHit(p,primary))return true;
    HitSound(p,primary);
    const auto data=*GetNativeProjectileData(p);auto* b=p->mBoard;std::vector<ZombieID> targets;
    const double px=kFusionBasePixelsX,py=GetNativeFusionPixelsY(b),x=(p->mPosX+20)/px,y=(p->mPosY+p->mPosZ+20)/py;
    Zombie* z=nullptr;while(b->IterateZombies(z))
    {
        if(!Target(z)||!FusionPuffLove269CanSplash(GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,NativeWater268ZombieCaptured(z)))continue;
        const auto rect=z->GetZombieRect();
        if(FusionPuffLove269CircleTouchesRect(x,y,rect.mX/px,rect.mY/py,rect.mWidth/px,rect.mHeight/py))targets.push_back(b->ZombieGetID(z));
    }
    for(auto id:targets)if(auto* target=b->ZombieTryToGet(id))
    {
        target->TakeDamage(data.damage,0U);float threshold=0;
        const bool attempt=TryGetFusionPuffLove269Threshold(GetNativeFusionZombieHealth(target),target->mHelmHealth,
            target->mBodyMaxHealth,target->mHelmMaxHealth,HasNativeHypno269ProjectileUltimate(p,18),threshold);
        const float draw=attempt?RandRangeFloat(0,1):-1;
        if(attempt&&draw>=threshold&&!target->mDead){target->StopEating();target->StartMindControlled();target->TrySpawnLevelAward();}
        LawnNet::NetTrace("Fusion Hypno269 area projectile=%u target=%u damage=%d body=%d helm=%d threshold=%.9g draw=%.9g mc=%d tick=%d",
            p->mNetId,target->mNetId,data.damage,target->mBodyHealth,target->mHelmHealth,threshold,draw,target->mMindControlled?1:0,b->mMainCounter);
    }
    p->Die();return true;
}
bool HandleNativeHypno269ProjectileCollision(Projectile* p)
{
    if(!IsNativeHypno269Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    Sexy::Rect rect;if(!GetNativeHypno269ProjectileRect(p,rect))return true;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        if(NativeHypno269ProjectileCanHit(p,z)&&rect.Intersects(z->GetZombieRect())){ApplyNativeHypno269ProjectileImpact(p,z);break;}
    return true;
}
}
