#include "FusionNativeShooterBatch.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeShooter267.h"
#include "FusionNative.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeCold.h"
#include "FusionNativeFire.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeDeath.h"
#include "FusionFireActionBatchRules.h"
#include "FusionNativeVisual.h"
#include "FusionNativeCherryShooter.h"
#include "FusionSplitShooterRules.h"
#include "FusionStraightProjectileRules.h"
#include "FusionBasePlantRules.h"
#include "FusionCherryShooterRules.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionHostileProjectileRules.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeDefender.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWater268.h"
#include "FusionEquipmentRules.h"
#include "FusionSummon269Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <vector>

namespace LawnFusion
{
namespace
{
constexpr double pi=3.14159265358979323846;
bool Host(const GameObject* o)
{
    return o&&o->mApp&&o->mBoard&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&
        IsNativeFusionBoard(o->mBoard)&&NativeFusionBackgroundValid(static_cast<uint8_t>(o->mBoard->mBackground))&&
        !o->mBoard->mPaused&&o->mApp->mGameScene==SCENE_PLAYING;
}
bool Normal(Zombie* z)
{
    return Host(z)&&!z->mDead&&!z->IsDeadOrDying()&&z->mBodyHealth>0&&
        IsNativeFusionGroundZombie(z)&&!z->mMindControlled&&!z->IsFlying()&&
        (!GetNativeSourceZombieData(z)||(!z->mInPool&&!z->mOnHighGround));
}
bool Live(Plant* p)
{return Host(p)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Star(uint16_t k){return k==1096||k==1183||k==1081;}
bool Sniper(uint16_t k){return k==1109||k==1167;}
bool Rage(uint16_t k){return k==1165||k==1168;}
const FusionShooterTimingProfile* Timing(const Plant* p)
{
    const auto* d=GetNativePlantData(p);if(!d)return nullptr;
    return d->kind==1161?FindFusionBigGatlingTimingProfile(d->batchShotCount,d->batchMode==3):
        FindFusionShooterBatchTimingProfile(d->kind);
}
bool Muzzle(uint16_t k,std::size_t n,float& x,float& y)
{
    // Full serialized hierarchy, root editor translation excluded like DrawPose.
    // Nominal native-art anchor adaptation; no invented live bone sampling.
    switch(k)
    {
    case 1020:x=.467999977f;y=.66355f;return true; // callback +.1 sourceX
    case 1152:x=.2695f;y=.084f;return true;
    case 929:x=.18802f;y=.09911f;return true;
    case 1107:x=n==1?.367999977f:-.8624999724f;y=.66355f;return true;
    case 1161:x=1.55220009f;y=.45299979f;return true;
    case 1165:x=.5884252147f;y=.7206454714f;return true;
    case 1168:x=.4840998597f;y=.707249862f;return true;
    case 1079:x=.5949900924f;y=.7322000175f;return true;
    case 1005:case 901:x=.402f;y=.7452f;return true;
    case 907:x=.4840998597f;y=.507249862f;return true;
    case 933:x=.6963000924f;y=.6370000175f;return true;
    case 1191:x=.5742000924f;y=.6685000175f;return true;
    case 1081:
    case 1096:
    {
        constexpr float points[][2]={{-.1439999456f,.803199936f},{-.144f,-.2752000224f},
            {-.5664f,.3776f},{.553599936f,.6112f},{.5536f,.003200032f}};
        if(n>=5)return false;x=points[n][0];y=points[n][1];return true;
    }
    case 1183:
    {
        constexpr float points[][2]={{-.0735f,.38099994f},{-.067499958f,.05399997f},
            {-.2895f,.216f},{.2475f,.123f},{.228f,.299999999f}};
        if(n>=5)return false;x=points[n][0];y=points[n][1];return true;
    }
    default:return false;
    }
}
Zombie* Acquire(Plant* plant)
{
    auto* d=GetNativePlantData(plant);float px=0,py=0;if(!d||!plant->GetShadowCenter(px,py))return nullptr;
    auto* b=plant->mBoard;
    const double vision=b->GetCellCenterX(b->GetColumnCount()-1,plant->mRow)+1.85*kFusionBasePixelsX;
    Zombie* nearest=nullptr;double distance=0;
    if(Star(d->kind))
    {
        for(std::size_t ray=0;ray<5;++ray)
        {
            Zombie* z=nullptr;
            while(b->IterateZombies(z))if(Normal(z))
            {
                const auto r=z->GetZombieRect();double at=0;
                if(TryGetFusionStarAcquisitionDistance(ray,z->mRow==plant->mRow,px,py-.3*kFusionBasePixelsY,
                    {double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)},at)&&(!nearest||at<distance))
                {nearest=z;distance=at;}
            }
            if(nearest)return nearest; // Source ray order, not global nearest.
        }
        return nullptr;
    }
    Zombie* z=nullptr;
    while(b->IterateZombies(z))if(Normal(z))
    {
        float zx=0,zy=0;if(!z->GetShadowCenter(zx,zy)||zx>=vision)continue;
        if(Sniper(d->kind))
        {
            if(!FusionSniperCanAcquire(GetNativeFusionZombieStatus(z),false,false,px,zx,vision))continue;
            const double at=std::hypot(double(zx-px)/kFusionBasePixelsX,double(zy-py)/kFusionBasePixelsY);
            if(!nearest||at<distance){nearest=z;distance=at;}
        }
        else if(z->mRow==plant->mRow&&(d->kind==1107||zx>px))return z;
    }
    return nearest;
}
void Sound(Plant* p,bool random=true)
{if(random)(void)RandRangeInt(3,4);p->mApp->PlayFoley(FOLEY_THROW);}
void Particle(Projectile* p,ParticleEffect effect)
{
    const float x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;
    p->mApp->AddTodParticle(x,y,p->mRenderOrder+1,effect);
    LawnNet::NetBroadcastParticle(p->mApp,x,y,p->mRenderOrder+1,effect);
}
Projectile* Emit(Plant* plant,uint16_t payload,int damage,uint8_t way,float mx,float my,
    double speed=6,double angle=0,uint8_t variant=0)
{
    if(!Live(plant))return nullptr;auto* b=plant->mBoard;
    if(b->mProjectiles.mSize>=b->mProjectiles.mMaxSize||!EnsureNativeProjectileVisuals(b,payload))return nullptr;
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return nullptr;
    const auto sourceKind=GetNativePlantData(plant)->kind;
    const bool extended=sourceKind==1079||sourceKind==1081||sourceKind==1005||sourceKind==901||sourceKind==907||sourceKind==933;
    x+=mx*(extended?float(kFusionBasePixelsX):60.f);y-=my*(extended?float(kFusionBasePixelsY):60.f);
    NativeProjectileData data;data.kind=payload;data.damage=damage;data.specialType=way;data.effectVariant=variant;
    const auto type=NativeProjectileMetadataType(data);
    const auto motion=NativeProjectileMetadataMotion(data);
    if(!NativeProjectileMetadataValid(data,type,motion))return nullptr;
    auto* p=b->AddProjectile(int(x-20),int(y-20),plant->mRenderOrder-1,plant->mRow,
        type==PROJECTILE_STAR?PROJECTILE_PEA:type);
    if(!p)return nullptr;
    p->mProjectileType=type;
    p->mMotionType=motion;
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    p->mVelX=float(speed*std::cos(angle)*kFusionBasePixelsX*.01);
    p->mVelY=float(-speed*std::sin(angle)*kFusionBasePixelsY*.01);
    p->mVelZ=p->mAccZ=0;p->mShadowY=y;
    if(payload==39||payload==46||payload==40)p->mRotationSpeed=float((payload==40?5:6)*30*.01*pi/180);
    p->mRotation=float(-angle);p->mDamageRangeFlags=plant->GetDamageRangeFlags(WEAPON_PRIMARY);
    if(!AttachNativeProjectile(b,p,data)){p->Die();return nullptr;}
    LawnNet::NetTrace("Fusion native ShooterBatch projectile plant=%u kind=%u id=%u payload=%u way=%u damage=%d vx=%.9g vy=%.9g",
        plant->mNetId,GetNativePlantData(plant)->kind,p->mNetId,payload,way,damage,p->mVelX,p->mVelY);
    return p;
}
void RageVolley(Plant* p)
{
    auto* d=GetNativePlantData(p);float mx=0,my=0;if(!d||!Muzzle(d->kind,0,mx,my))return;
    mx-=.1f; // AttributeEvent uses the stored Shoot without normal callback+.1.
    for(int i=0;i<3;++i)
    {
        const float angle=RandRangeFloat(-15.f,15.f);
        const float speed=RandRangeFloat(12.f,14.f);
        const float dx=RandRangeFloat(-.1f,.1f),dy=RandRangeFloat(-.1f,.1f);
        Emit(p,d->kind==1165?NativeRemaining269SnowGatlingPayload(p):0,20,15,mx+dx,my+dy,speed,angle*pi/180);
    }
    Sound(p);p->mDoSpecialCountdown=p->mStateCountdown>0?2:0;
}
void StartDoomSkill(Plant* p)
{
    auto* d=GetNativePlantData(p);d->batchMode=1;d->shootSeconds=0;p->mShootingCounter=0;
    const double rate=static_cast<double>(.6f)*d->animationSpeed;
    p->mStateCountdown=int(std::ceil(static_cast<double>(1.8333334f)/rate*100));
    p->mDoSpecialCountdown=int(std::ceil(static_cast<double>(.41666666f)/rate*100));
}
void StartBigPhase(Plant* p,uint8_t mode)
{
    auto* d=GetNativePlantData(p);d->batchMode=mode;d->shootSeconds=mode==5?-1.f:0.f;
    p->mShootingCounter=0;
    if(mode==5)p->mStateCountdown=0;
    else TryGetFusionBigGatlingPhaseTicks(mode,d->animationSpeed,d->batchShotCount,p->mStateCountdown);
    if(mode==2||mode==3)BeginNativeShooterShot(p,*Timing(p));
}
void FireSniper(Plant* p)
{
    auto* d=GetNativePlantData(p);auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);
    Sound(p,false);
    if(!Normal(z))z=Acquire(p); // Source retargets at the actual event.
    if(!z){p->mTargetZombieID=ZOMBIEID_NULL;return;}
    p->mTargetZombieID=p->mBoard->ZombieGetID(z);
    int damage=0;uint32_t next=0;
    if(!TryAdvanceFusionSniperShot(d->kind,d->batchShotCount,next,damage))return;
    const uint16_t kind=d->kind;d->batchShotCount=next;
    z->TakeDamage(damage,1U); // Source Shieldless4.
    if(kind==1167){NativeWarmAfterDamage(z);ApplyNativeFireSourceExplosion(z,100,true);}
    LawnNet::NetTrace("Fusion native ShooterBatch sniper plant=%u kind=%u target=%u shot=%u damage=%d",p->mNetId,kind,z->mNetId,next,damage);
    if(!Normal(z))p->mTargetZombieID=ZOMBIEID_NULL;
}
void FireEvent(Plant* p,std::size_t event)
{
    auto* d=GetNativePlantData(p);const uint16_t k=d->kind;
    if(Sniper(k)){FireSniper(p);return;}
    float x=0,y=0;if(!Muzzle(k,0,x,y))return;
    if(Star(k))
    {
        constexpr double angles[]={pi/2,-pi/2,pi,pi/6,-pi/6};
        const int light=k==1081?GetNativeFireLightLevel(p):0;
        const uint16_t payload=k==1096?46:k==1081&&light>0?40:39;
        for(std::size_t i=0;i<5;++i)if(Muzzle(k,i,x,y))Emit(p,payload,k==1096||k==1081?40:20,i==2?9:2,x,y,6,angles[i],k==1081&&light>2?1:0);
        Sound(p);return;
    }
    if(k==1079)
    {
        const int light=GetNativeFireLightLevel(p);
        if(light>=2)
        {
            float px=0,py=0;p->GetShadowCenter(px,py);Zombie* z=nullptr;int count=0;
            while(p->mBoard->IterateZombies(z))if(Normal(z)&&z->mRow==p->mRow)
            {float zx=0,zy=0;if(z->GetShadowCenter(zx,zy)&&zx>=px){z->TakeDamage(light>2?40:20,2U);++count;}}
            LawnNet::NetTrace("Fusion native LanternCactus ray plant=%u light=%d hits=%d",p->mNetId,light,count);
        }
        else Emit(p,38,20,0,x,y,6,0,light==1?1:0);
        Sound(p);return;
    }
    if(k==1005||k==901){Emit(p,3,300,0,x,y);Sound(p);return;}
    if(k==907)
    {
        const bool big=++d->batchShotCount==16;
        Emit(p,23,big?1800:300,0,x,y,6,0,big?1:0);
        if(big){d->batchShotCount=0;p->mLaunchCounter=451;}Sound(p);return;
    }
    if(k==933){Emit(p,100,300,0,x,y);Sound(p);return;}
    if(k==1107)
    {
        // Callback order rear first, forward second; second event rear only.
        if(Muzzle(k,0,x,y))
        {
            FusionSplitEmissionSpec spec{k,static_cast<uint8_t>(event),15,x,y,1};
            p->Fire(nullptr,p->mRow,WEAPON_PRIMARY,&spec);
        }
        if(event==0&&Muzzle(k,1,x,y))
        {
            FusionSplitEmissionSpec spec{k,0,15,x,y,0};p->Fire(nullptr,p->mRow,WEAPON_PRIMARY,&spec);
        }
        p->mApp->PlayFoley(FOLEY_THROW);return;
    }
    if(k==1161)
    {
        float speed=6,rate=1;TryGetFusionGatlingMilestone(d->batchShotCount,speed,rate);
        const uint8_t tier=speed==12?18:speed==9?17:16;
        for(int i=0;i<3;++i)Emit(p,0,20,tier,x,y-.4f+.3f*i,speed);
        Sound(p);const auto* old=Timing(p);
        if(d->batchShotCount<static_cast<uint32_t>((std::numeric_limits<int>::max)()))++d->batchShotCount;
        const auto* now=Timing(p);
        // Preserve the actual clip phase at the two source speed transitions.
        if(now&&old&&now->stateSpeed!=old->stateSpeed&&event<now->eventClipSeconds.size())
        {
            TryRephaseFusionBigGatling(d->batchMode,d->animationSpeed,d->batchShotCount,event,
                p->mShootingCounter,p->mStateCountdown,d->shootSeconds);
        }
        return;
    }
    if(Rage(k))
    {
        const int draw=RandRangeInt(0,99); // Consumed even when a rage restarts.
        if(draw==50)
        {
            p->mStateCountdown=500;d->batchRageTicks=500;d->batchMode=1;
            p->mShootingCounter=0;d->shootSeconds=-1;
            RageVolley(p);p->mPlantHealth=p->mPlantMaxHealth;return;
        }
        Emit(p,k==1165?NativeRemaining269SnowGatlingPayload(p):0,20,0,x,y);Sound(p);return;
    }
    if(k==1191)
    {
        Emit(p,102,40,0,x,y);Sound(p);
        uint16_t next=0;bool skill=false;
        if(TryDrawFusionDoomCactusBag(d->batchBagCount,RandRangeInt(0,d->batchBagCount),next,skill))
        {d->batchBagCount=next;if(skill)StartDoomSkill(p);}return;
    }
    Emit(p,k==1020?11:k==1152?90:95,k==929?20:80,0,x,y);
    Sound(p,k!=929); // SunGatling source sound68 is fixed.
}
bool CanonicalTarget(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&z->mBodyHealth>0&&IsNativeFusionGroundZombie(z)&&!z->mMindControlled&&!z->IsFlying();}
void SourceExplosion(Projectile* p,float x,float y,int row,int damage,double radius,bool sameRow)
{
    auto* b=p->mBoard;std::vector<ZombieID> ids;Zombie* z=nullptr;
    while(b->IterateZombies(z))
    {
        if(!CanonicalTarget(z)||(row>=0&&(sameRow?z->mRow!=row:std::abs(z->mRow-row)>1)))continue;
        const auto r=z->GetZombieRect();
        if(FusionFireCircleTouchesRect(x,y,radius,r.mX,r.mY,r.mWidth,r.mHeight))ids.push_back(b->ZombieGetID(z));
    }
    for(auto id:ids)if((z=b->ZombieTryToGet(id))&&CanonicalTarget(z))z->TakeDamage(damage,18U);
}
void TrackLanternStar(Projectile* p)
{
    auto* data=GetNativeProjectileData(p);
    if(p->mProjectileAge<=30)return;
    data->specialType=6;
    auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);
    const auto eligible=[&](Zombie* target){return Normal(target)&&(data->effectVariant||target->mRow==p->mRow);};
    if(!eligible(z))
    {
        z=nullptr;Zombie* candidate=nullptr;double nearest=0;
        while(p->mBoard->IterateZombies(candidate))if(eligible(candidate))
        {
            const auto r=candidate->GetZombieRect();
            const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX;
            const double dy=(r.mY+r.mHeight*.5-p->mPosY-20)/kFusionBasePixelsY;
            const double distance=dx*dx+dy*dy;
            if(!z||distance<nearest){z=candidate;nearest=distance;}
        }
        p->mTargetZombieID=z?p->mBoard->ZombieGetID(z):ZOMBIEID_NULL;
        // Source acquires first, but keeps its collider disabled and fallback
        // velocity for this update; the target is used on the next update.
        p->mCobTargetRow=p->mBoard->mMainCounter;p->mVelX=0;p->mVelY=float(5.0/6*kFusionBasePixelsY*.01);return;
    }
    const auto r=z->GetZombieRect();
    const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX;
    const double dy=-(r.mY+r.mHeight*.5-p->mPosY-20)/kFusionBasePixelsY;
    const double distance=std::hypot(dx,dy);
    if(distance>0){p->mVelX=float(dx/distance*5*kFusionBasePixelsX*.01);p->mVelY=float(-dy/distance*5*kFusionBasePixelsY*.01);}
    p->mRotation=float(-std::atan2(dy,dx));
}
void Retire(Projectile* p)
{
    if(auto* c=GetNativeContext(p->mBoard))c->rollingHits.erase(p->mBoard->mProjectiles.DataArrayGetID(p));
    p->Die();
}
}

bool IsNativeShooterBatch(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&IsFusionShooterBatchKind(d->kind)&&d->kind!=1192;}
const FusionShooterTimingProfile* GetNativeShooterBatchTimingProfile(const Plant* p){return Timing(p);}
void InitializeNativeShooterBatch(Plant* p)
{
    if(IsNativeShooter267(p)){InitializeNativeShooter267(p);return;}
    const auto* profile=Timing(p);if(!profile||!Live(p))return;
    auto* d=GetNativePlantData(p);d->batchShotCount=d->batchRageTicks=d->batchMode=0;
    d->batchBagCount=d->kind==1191?20:0;
    p->mState=STATE_NOTREADY;p->mStateCountdown=p->mShootingCounter=p->mDoSpecialCountdown=0;
    InitializeNativeShooterTiming(p,*profile,false);
    if(d->kind==1161)StartBigPhase(p,0);
    if(d->kind==933)StartDoomSkill(p);
}
bool UpdateNativeShooterBatch(Plant* p)
{
    if(IsNativeShooter267(p))return UpdateNativeShooter267(p);
    if(!IsNativeShooterBatch(p))return false;if(!Live(p))return true;
    auto* d=GetNativePlantData(p);const auto k=d->kind;
    if(Sniper(k)&&!Normal(p->mBoard->ZombieTryToGet(p->mTargetZombieID)))
    {auto* z=Acquire(p);p->mTargetZombieID=z?p->mBoard->ZombieGetID(z):ZOMBIEID_NULL;}
    if(Rage(k))
    {
        d->batchRageTicks=static_cast<uint16_t>(p->mStateCountdown>0?p->mStateCountdown:0);
        d->batchMode=d->batchRageTicks?1:0;
        if(d->batchRageTicks)return true;
    }
    if((k==1191||k==933)&&d->batchMode)
    {if(p->mStateCountdown==0){d->batchMode=0;d->shootSeconds=-1;p->mDoSpecialCountdown=0;}return true;}
    if(k==1161)
    {
        if(p->mStateCountdown==0)
        {
            if(d->batchMode==0)StartBigPhase(p,1);
            else if(d->batchMode==1||d->batchMode==4)StartBigPhase(p,5);
            else if(d->batchMode==6)StartBigPhase(p,2);
            else if(d->batchMode==2||d->batchMode==3)StartBigPhase(p,3);
        }
    }
    if(--p->mLaunchCounter>0)return true;
    const auto* profile=Timing(p);
    TryGetFusionShooterBatchCycleTicks(k,Sniper(k)||k==1191||k==1079||k==1081||k==933?RandRangeFloat(-.1f,.1f):RandRangeFloat(.95f,1.05f),p->mLaunchCounter);
    Zombie* target=Acquire(p);
    if(Sniper(k))
    {
        auto* previous=p->mBoard->ZombieTryToGet(p->mTargetZombieID);
        if(Normal(previous))target=previous;
        p->mTargetZombieID=target?p->mBoard->ZombieGetID(target):ZOMBIEID_NULL;
    }
    if(k==1161)
    {
        if(!target)
        {
            d->batchShotCount=0;
            if(d->batchMode==2||d->batchMode==3||d->batchMode==6)StartBigPhase(p,4);
        }
        else if(d->batchMode==5)StartBigPhase(p,6);
        return true;
    }
    if((target||(k==1081&&GetNativeFireLightLevel(p)>=2))&&d->shootSeconds<0)BeginNativeShooterShot(p,*profile);
    return true;
}
void UpdateNativeShooterBatchPhase(Plant* p)
{
    if(IsNativeShooter267(p)){UpdateNativeShooter267Phase(p);return;}
    if(!IsNativeShooterBatch(p)||!Live(p))return;
    auto* d=GetNativePlantData(p);
    if((d->kind==1191||d->kind==933)&&d->batchMode){d->idleSeconds+=.01f;d->shootSeconds+=.01f;return;}
    if(d->kind==1161&&d->batchMode!=5)
    {
        d->idleSeconds+=.01f;
        int total=0;TryGetFusionBigGatlingPhaseTicks(d->batchMode,d->animationSpeed,d->batchShotCount,total);
        d->shootSeconds=float((std::max)(0,total-p->mStateCountdown))*.01f;return;
    }
    UpdateNativeShooterPhase(p,*Timing(p));
}
bool DispatchNativeShooterBatchShot(Plant* p)
{
    if(IsNativeShooter267(p))return DispatchNativeShooter267Shot(p);
    if(!IsNativeShooterBatch(p))return false;if(!Live(p))return true;
    const auto* profile=Timing(p);auto* d=GetNativePlantData(p);
    if((d->kind==1191||d->kind==933)&&d->batchMode)return true;
    for(std::size_t i=0;i<profile->eventClipSeconds.size();++i)
    {
        bool due=false;
        if(TryGetFusionShooterEventAtCounter(*profile,d->animationSpeed,p->mShootingCounter,i,due)&&due)
        {
            const int counter=p->mShootingCounter;const uint8_t mode=d->batchMode;
            FireEvent(p,i);
            if(counter!=p->mShootingCounter||mode!=d->batchMode)break;
        }
    }
    return true;
}
bool ApplyNativeShooterBatchSpecial(Plant* p)
{
    // PeaMine keeps the canonical native Potato trigger/explosion/death path.
    if(const auto* mine=GetNativePlantData(p);mine&&mine->kind==1007)return false;
    if(!IsNativeShooterBatch(p))return false;if(!Live(p))return true;
    auto* d=GetNativePlantData(p);
    if(Rage(d->kind)) {if(p->mStateCountdown>0)RageVolley(p);return true;}
    if((d->kind==1191||d->kind==933)&&d->batchMode)
    {
        const double rate=static_cast<double>(.6f)*d->animationSpeed;
        const double stop=static_cast<double>(1.5f)/rate;
        if(d->shootSeconds<=stop)
        {
            float x=0,y=0;Muzzle(d->kind,0,x,y);
            float angle=0;
            if(TryGetFusionDoomCactusAngle(d->idleSeconds+.01f,angle))
                Emit(p,d->kind==933?100:102,d->kind==933?300:40,18,x,y,6,angle);
            Sound(p);p->mDoSpecialCountdown=2;
        }
        return true;
    }
    return true;
}
bool GetNativeShooterBatchVisualPose(const Plant* p,const char*& state,float& elapsed)
{
    if(IsNativeShooter267(p))return GetNativeShooter267VisualPose(p,state,elapsed);
    if(!IsNativeShooterBatch(p))return false;const auto* d=GetNativePlantData(p);
    state=d->shootSeconds>=0?Timing(p)->visualState:d->kind==929?"idle_g":"idle";
    elapsed=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;
    if(Rage(d->kind)&&d->batchMode){state="supershoot";elapsed=(500-d->batchRageTicks)*.01f;}
    if((d->kind==1191||d->kind==933)&&d->batchMode){state="superskill";elapsed=d->shootSeconds;}
    if(d->kind==1161)
    {
        if(d->batchMode==0){state="idle";elapsed=d->shootSeconds;}
        if(d->batchMode==1){state="elevating";elapsed=d->shootSeconds;}
        else if(d->batchMode==4){state="rest";elapsed=d->shootSeconds;}
        else if(d->batchMode==5){state="idle2";elapsed=d->idleSeconds;}
        else if(d->batchMode==6){state="armed";elapsed=d->shootSeconds;}
    }
    return std::isfinite(elapsed)&&elapsed>=0;
}
bool GetNativeShooterBatchBaseVisualPose(const Plant* p,const char*& state,float& elapsed)
{
    const auto* d=GetNativePlantData(p);
    if(!d||d->kind!=1161)return false;
    // The base layer remains raised for shooting, rest and armed; elevation
    // itself still owns the original idle/elevating transition.
    state=d->batchMode>=2?"idle2":"idle";
    elapsed=d->idleSeconds;
    return std::isfinite(elapsed)&&elapsed>=0;
}
bool NativeShooterBatchInvulnerable(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&FusionShooterBatchInvulnerable(d->kind,p->mStateCountdown);}
void NativeShooterBatchChomperSwallowed(Plant* p)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=1192||!Live(p))return;
    const int col=p->mPlantCol+1;if(col>=p->mBoard->GetColumnCount())return;
    ApplyNativeBatchDoomAt(p->mBoard,p->mBoard->GetCellCenterX(col,p->mRow),
        float(p->mBoard->GridToPixelY(col,p->mRow)+50),col,p->mRow,1800,true,false);
}
bool LaunchNativePotatoPuff(Plant* p,int damage)
{
    if(damage!=20&&damage!=60&&damage!=600)return false;
    const auto* data=GetNativePlantData(p);
    if(!data||(data->kind!=1171&&data->kind!=1170))return false;
    return Emit(p,92,damage,3,.348500035f,.469f)!=nullptr;
}
bool IsNativeShooterBatchProjectile(const Projectile* p)
{
    if(IsNativeShooter267Projectile(p))return true;
    const auto* d=GetNativeProjectileData(p);if(!d)return false;
    const bool family=(d->kind==39&&d->damage==40)||d->kind==3||d->kind==23||d->kind==38||d->kind==40||d->kind==100||d->kind==11||d->kind==90||d->kind==46||d->kind==92||d->kind==95||d->kind==102||
        ((d->kind==0||d->kind==15||d->kind==97)&&d->specialType>=15);
    // Admission is shared with allocation and FNS, including IceTorch stamps
    // on unheated cold payloads and heated Rage projectiles retaining way15.
    return family&&!d->isZombieBullet&&NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);
}
bool GetNativeShooterBatchProjectileCenter(const Projectile* p,float& x,float& y)
{if(!IsNativeShooterBatchProjectile(p))return false;x=p->mPosX+20;y=p->mPosY+p->mPosZ+20;return std::isfinite(x)&&std::isfinite(y);}
bool GetNativeShooterBatchProjectileRect(const Projectile* p,Sexy::Rect& rect)
{
    if(IsNativeShooter267Projectile(p))return GetNativeShooter267ProjectileRect(p,rect);
    if(!IsNativeShooterBatchProjectile(p))return false;const auto* d=GetNativeProjectileData(p);
    FusionRecipientRect r;
    if(d->kind==46||d->kind==40||d->kind==39)
    {
        if(!TryMakeFusionRecipientProjectileRect(39,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mVelX,p->mVelY,r))return false;
    }
    else if(d->kind==102||d->kind==100)
    {
        if(!TryMakeFusionShooterBatchRotatedRect(d->kind,p->mPosX+20,p->mPosY+p->mPosZ+20,-p->mRotation,r))return false;
    }
    else if(d->kind==0)
        r=d->fireState?FusionRecipientRect{p->mPosX,p->mPosY+p->mPosZ,double(p->mWidth-10),double(p->mHeight)}:
            FusionRecipientRect{p->mPosX-15,p->mPosY+p->mPosZ,double(p->mWidth+15),double(p->mHeight)};
    else if(!TryMakeFusionShooterBatchProjectileRect(d->kind,p->mPosX+20,p->mPosY+p->mPosZ+20,r))
    {
        const auto* f=FindFusionStraightProjectileProfile(d->kind);
        FusionStraightProjectileRect known;
        if(!f||!TryMakeFusionStraightProjectileRect(*f,p->mPosX+20,p->mPosY+p->mPosZ+20,known))return false;
        r={known.x,known.y,known.width,known.height};
    }
    const int x=int(std::floor(r.x)),y=int(std::floor(r.y));rect=Sexy::Rect(x,y,int(std::ceil(r.x+r.width))-x,int(std::ceil(r.y+r.height))-y);return true;
}
bool NativeShooterBatchProjectileExpired(const Projectile* p)
{
    if(!IsNativeShooterBatchProjectile(p))return false;const auto* d=GetNativeProjectileData(p);auto* b=p->mBoard;
    if(d->kind==92&&p->mProjectileAge>75)return true;
    const double x=(p->mPosX+20-b->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8;
    const double y=2.3-(p->mPosY+p->mPosZ+20-(b->GridToPixelY(0,0)+50))/kFusionBasePixelsY;
    return FusionCherryProjectileExpired(x,y,p->mProjectileAge,-10,
        1.35*(b->GetColumnCount()-1)-4.8+1.85,2.3-1.67*(kNativeFusionRows-1)-.6,6.5);
}
bool MoveNativeShooterBatchProjectile(Projectile* p)
{
    if(IsNativeShooter267Projectile(p))return MoveNativeShooter267Projectile(p);
    if(!IsNativeShooterBatchProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    if(NativeShooterBatchProjectileExpired(p)){Retire(p);return true;}
    if(GetNativeProjectileData(p)->kind==40)TrackLanternStar(p);
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;return true;
}
bool NativeShooterBatchProjectileCanHit(const Projectile* p,Zombie* z)
{
    if(IsNativeShooter267Projectile(p))return NativeShooter267ProjectileCanHit(p,z);
    if(!IsNativeShooterBatchProjectile(p)||!Normal(z)||p->mBoard!=z->mBoard)return false;
    const auto* d=GetNativeProjectileData(p);
    if((d->kind==100||d->kind==102)&&d->specialType==18)return true;
    if(d->kind==40 && p->mProjectileAge>30 && (!p->mTargetZombieID || p->mCobTargetRow==p->mBoard->mMainCounter))return false;
    return d->kind==46||d->kind==40||d->kind==39?d->specialType!=9||z->mRow==p->mRow:d->specialType==8?false:z->mRow==p->mRow;
}
bool ApplyNativeShooterBatchProjectileImpact(Projectile* p,Zombie* z)
{
    if(IsNativeShooter267Projectile(p))return ApplyNativeShooter267ProjectileImpact(p,z);
    if(!IsNativeShooterBatchProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    if(!z)
    {
        const auto data=*GetNativeProjectileData(p);
        if(data.kind==3){SourceExplosion(p,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRow,data.damage,1.5,false);HealNativeCherryNutFromCircle(p->mBoard,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRow,100,1.5);}
        Particle(p,data.kind==23||data.kind==100?PARTICLE_DOOM:PARTICLE_PEA_SPLAT);Retire(p);return true;
    }
    if(!NativeShooterBatchProjectileCanHit(p,z))return true;
    const auto data=*GetNativeProjectileData(p);auto* board=p->mBoard;
    if(data.fireState)return ApplyNativeFireImpact(p,z);
    if(data.kind==3)
    {
        SourceExplosion(p,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRow,data.damage,1.5,false);
        HealNativeCherryNutFromCircle(board,p->mPosX+20,p->mPosY+p->mPosZ+20,p->mRow,100,1.5);
        Particle(p,PARTICLE_POWIE);Retire(p);return true;
    }
    if(data.kind==23)
    {
        if(data.effectVariant)SourceExplosion(p,p->mPosX+20,p->mPosY+p->mPosZ+20,-1,data.damage,4,false);
        if(!z->mDead)z->TakeDamage(data.damage,18U);
        Particle(p,PARTICLE_DOOM);Retire(p);return true;
    }
    if(data.kind==38||data.kind==100)
    {
        auto* c=GetNativeContext(board);const auto id=board->mProjectiles.DataArrayGetID(p);
        if(!c||!RememberFusionRollingHit(c->rollingHits,id,board->ZombieGetID(z)))return true;
        if(data.kind==100)
        {
            const auto* source=GetNativeSourceZombieData(z);float threshold=1;
            const int damage=z->mIceTrapCounter>0?data.damage*4:data.damage;
            if(TryGetFusionHypnoFumeThreshold(source?source->health:float(z->mBodyHealth),z->mBodyMaxHealth,z->mHelmHealth,z->mHelmMaxHealth,0,threshold)&&RandRangeFloat(0,1)>=threshold)
            {
                float x=0,y=0;z->GetShadowCenter(x,y);z->StopEating();z->StartMindControlled();z->TrySpawnLevelAward();
                SourceExplosion(p,x,y,p->mRow,data.damage*3,1,true);Particle(p,PARTICLE_DOOM);
            }
            else {const FusionColdDamageContext cold{0,FusionColdDamageDelivery::IceAll};z->TakeDamage(damage,2U,&cold);AddNativeColdLevel(z,10);}
        }
        else z->TakeDamage(data.damage,0U);
        p->PlayImpactSound(z);
        if(FusionProjectileHitCount(c->rollingHits,id)>=std::size_t(data.kind==100?1000000:data.effectVariant?3:1))Retire(p);
        return true;
    }
    if(data.kind==95)
    {
        auto* c=GetNativeContext(board);
        if(!c||!RememberFusionRollingHit(c->rollingHits,board->mProjectiles.DataArrayGetID(p),board->ZombieGetID(z)))return true;
        if(board->mCoins.mSize<board->mCoins.mMaxSize)
            board->AddCoin(int(p->mPosX+20),int(p->mPosY+p->mPosZ+20),COIN_SMALLSUN,COIN_MOTION_FROM_PLANT,SEED_NONE,5);
        z->TakeDamage(data.damage,2U);Particle(p,PARTICLE_SNOWPEA_SPLAT);
        if(FusionProjectileHitCount(c->rollingHits,board->mProjectiles.DataArrayGetID(p))>=3)Retire(p);
        return true;
    }
    int damage=data.damage;
    if(data.kind==11||data.kind==90||data.kind==46)
    {
        // Both IronPea IDs11/90 and IronStar46 add source secondArmorHealth.
        if(!TryGetFusionIronDamage(damage,z->mShieldHealth,damage)){Retire(p);return true;}
        z->TakeDamage(damage,0U);
        if(data.kind!=46&&!z->mDead)
        {
            float zx=0,zy=0;
            if(z->GetShadowCenter(zx,zy))
            {
                // Board.Awake312620: zombieMaxX=lastCell+4.0. The +1.85
                // plant-vision boundary is a different source field.
                const float limit=float(board->GetCellCenterX(board->GetColumnCount()-1,z->mRow)+(4.0-2.2)*kFusionBasePixelsX);
                if(zx<=limit)z->mPosX+=(std::min)(.2f*float(kFusionBasePixelsX),limit-zx);
            }
        }
    }
    else if(data.kind==15||data.kind==97)
    {
        const auto* profile=FindFusionColdProjectileProfile(data.kind);
        if(!profile||!AddNativeColdLevel(z,profile->levelPerHit)){Retire(p);return true;}
        damage*=profile->baseDamageMultiplier;
        if(z->mIceTrapCounter>0)damage*=4;
        FusionColdDamageContext context;context.projectileKind=data.kind;
        z->TakeDamage(damage,0U,&context);
    }
    else
    {
        // Source DoomSplat27 has only ParticleSystem/Renderer, no gameplay script.
        if(data.kind==102)Particle(p,PARTICLE_DOOM);
        z->TakeDamage(damage,0U);
    }
    p->PlayImpactSound(z);if(data.kind!=102)Particle(p,PARTICLE_PEA_SPLAT);
    LawnNet::NetTrace("Fusion native ShooterBatch impact id=%u payload=%u target=%u damage=%d helm=%d shield=%d body=%d",p->mNetId,data.kind,z->mNetId,damage,z->mHelmHealth,z->mShieldHealth,z->mBodyHealth);
    Retire(p);return true;
}
bool TryReflectNativeSuperCherryProjectile(Projectile* p,Plant* attacked)
{
    const auto* d=GetNativeProjectileData(p);
    if(!d||!d->isZombieBullet||d->kind!=3||!Host(p)||p->mDead||!Live(attacked)||attacked->mBoard!=p->mBoard||attacked->mRow!=p->mRow||
        !NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType))return false;
    auto* b=p->mBoard;
    int reflected=0;
    if(attacked&&TryPlanFusionSuperCherryRebound(GetNativePlantData(attacked)->kind,
        !attacked->mIsAsleep&&!attacked->mWakeUpCounter,GetNativeDefenderPotKind(attacked)==1133,reflected))
    {
        // Rebound4237E0 clears collision latch and allegiance, preserving this
        // actual source3 actor. Stock backwards motion must not run afterwards.
        auto* data=GetNativeProjectileData(p);data->specialType=0;data->isZombieBullet=false;
        data->hitLatched=false;data->damage=reflected;p->mMotionType=MOTION_STRAIGHT;p->mVelX=float(.06*kFusionBasePixelsX);
        ApplyNativeSourcePlantDamage(attacked,200,0);
        if(auto* target=GetNativePlantData(attacked);target&&target->kind==1131&&!attacked->mDead)
            target->batchShotCount=uint32_t((std::min)(uint64_t(1000000),uint64_t(target->batchShotCount)+uint32_t(reflected)));
        LawnNet::NetTrace("Fusion SuperCherry rebound projectile=%u defender=%u damage=%d tick=%d",p->mNetId,attacked->mNetId,reflected,b->mMainCounter);
        return true;
    }
    return false;
}
Plant* SelectNativeSuperCherryPlantTarget(Projectile* p,Plant* attacked)
{
    if(!p||!p->mBoard||!Live(attacked)||attacked->mBoard!=p->mBoard||attacked->mRow!=p->mRow)return nullptr;
    auto* b=p->mBoard;
    Plant* selected=attacked;Plant* cover=nullptr;
    while(b->mPlants.IterateNext(cover))
        if(Live(cover)&&cover->mPlantCol==attacked->mPlantCol&&cover->mRow==attacked->mRow&&
            FusionSuperCherryPlantPumpkin(GetNativePlantData(cover)->kind)&&!FusionSuperCherryPlantShort(GetNativePlantData(cover)->kind))selected=cover;
    return selected;
}

bool ApplyNativeSuperCherryProjectileImpact(Projectile* p,Plant* attacked,bool plantContact)
{
    const auto* d=GetNativeProjectileData(p);
    if(!d||d->kind!=3||!NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType))return false;
    // Friendly HitZombie/HitLand retain their existing single source leaf.
    if(!d->isZombieBullet)return ApplyNativeShooterBatchProjectileImpact(p,nullptr);
    if(!Host(p)||p->mDead)return true;
    if(attacked&&attacked->mBoard!=p->mBoard)return false;
    // Unity's destroyed target compares null. The already-born BombCherry still
    // retains its HitPlant offset and may hit nearby MC zombies without it.
    if(attacked&&!Live(attacked))attacked=nullptr;
    auto* b=p->mBoard;
    GetNativeProjectileData(p)->hitLatched=true; // Source CheckPlant consumes before callbacks.
    float x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;
    const double py=GetNativeFusionPixelsY(b),ratio=kFusionBasePixelsY/py;
    if(plantContact){x-=float(.5*kFusionBasePixelsX);y+=float(.2*py);}
    bool collateral=true;
    if(attacked)
    {
        Plant* selected=attacked;Plant* cover=nullptr;
        const int col=selected->mPlantCol,row=selected->mRow;
        collateral=!FusionSuperCherryPlantSingleTarget(GetNativePlantData(selected)->kind);
        std::vector<uint32_t> victims{b->mPlants.DataArrayGetID(selected)};Plant* q=nullptr;
        if(collateral)while(b->mPlants.IterateNext(q))
            if(q!=selected&&Live(q)&&FusionSuperCherryCollateralCell(q->mPlantCol-col,q->mRow-row))victims.push_back(b->mPlants.DataArrayGetID(q));
        for(auto id:victims)if((q=b->mPlants.DataArrayTryToGet(id))&&Live(q))
        {
            const auto kind=GetNativePlantData(q)->kind;bool protectedCell=false,graph=false;
            cover=nullptr;while(b->mPlants.IterateNext(cover))
                if(Live(cover)&&cover->mPlantCol==q->mPlantCol&&cover->mRow==q->mRow)
                {const auto k=GetNativePlantData(cover)->kind;if(k==922||k==1164||k==1169)protectedCell=true;}
            if(kind==911)
            {
                std::vector<Plant*> component{q};
                for(size_t n=0;n<component.size();++n)
                {
                    Plant* member=nullptr;while(b->mPlants.IterateNext(member))
                        if(Live(member)&&FusionEquipmentMagnetMember(GetNativePlantData(member)->kind)&&
                            std::find(component.begin(),component.end(),member)==component.end()&&
                            FusionEquipmentGraphAdjacent(component[n]->mPlantCol,component[n]->mRow,member->mPlantCol,member->mRow))component.push_back(member);
                }
                for(const auto* member:component)
                {const auto k=GetNativePlantData(member)->kind;if(k==935||k==1097)graph=true;}
            }
            const auto plan=PlanFusionSuperCherryPlantHit(kind,protectedCell,graph);
            if(plan.effect==FusionSuperCherryPlantEffect::Heal)RecoverNativeCompanions269Plant(q,plan.amount);
            else if(plan.effect==FusionSuperCherryPlantEffect::Damage)ApplyNativeSourcePlantDamage(q,plan.amount,1);
        }
    }
    // ZombieExplode440350: targetless or ordinary target also hits actual MC
    // zombies. Special Pumpkin/rock/BigNut targets stop after PlantTakeDamage.
    if(collateral)
    {
        std::vector<ZombieID> hits;Zombie* z=nullptr;
        while(b->IterateZombies(z))
        {
            if(z->mDead||z->IsDeadOrDying()||!z->mMindControlled||NativeWater268ZombieCaptured(z))continue;
            const bool special=GetNativeFusionZombieKind(z)==107;
            if(!special&&!FusionPuffLove269CanSplash(GetNativeFusionZombieStatus(z),z->mRow-p->mRow,false,false))continue;
            const auto r=z->GetZombieRect();
            if(FusionFireCircleTouchesRect(x,y,1.5,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio))hits.push_back(b->ZombieGetID(z));
        }
        for(auto id:hits)if(auto* z=b->ZombieTryToGet(id);z&&!z->mDead&&z->mMindControlled)
            z->TakeDamage(GetNativeFusionZombieKind(z)==107?150:300,18U); // Same canonical source Explode10 armor mapping; MC allegiance is intentional.
    }
    Particle(p,PARTICLE_POWIE);p->mApp->PlayFoley(FOLEY_CHERRYBOMB);
    LawnNet::NetTrace("Fusion SuperCherry hostile impact projectile=%u target=%u collateral=%d tick=%d",p->mNetId,attacked?attacked->mNetId:0,collateral?1:0,b->mMainCounter);
    Retire(p);return true;
}

bool HandleNativeShooterBatchProjectileCollision(Projectile* p)
{
    if(IsNativeShooter267Projectile(p))return HandleNativeShooter267ProjectileCollision(p);
    if(!IsNativeShooterBatchProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    if(NativeShooterBatchProjectileExpired(p)){Retire(p);return true;}
    Sexy::Rect r;if(!GetNativeShooterBatchProjectileRect(p,r))return true;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(NativeShooterBatchProjectileCanHit(p,z))
    {
        const auto target=z->GetZombieRect();
        if(r.mX>target.mX+target.mWidth||r.mX+r.mWidth<target.mX||r.mY>target.mY+target.mHeight||r.mY+r.mHeight<target.mY)continue;
        const auto* data=GetNativeProjectileData(p);
        if(data->kind==46||data->kind==102||data->kind==40||data->kind==100||data->kind==39)
        {
            const double angle=(data->kind==46||data->kind==40||data->kind==39)?std::atan2(-p->mVelY/kFusionBasePixelsY,p->mVelX/kFusionBasePixelsX):-p->mRotation;
            if(!FusionShooterBatchRotatedProjectileTouchesRect(data->kind==39?46:data->kind,p->mPosX+20,p->mPosY+p->mPosZ+20,angle,
                {double(target.mX),double(target.mY),double(target.mWidth),double(target.mHeight)}))continue;
        }
        {ApplyNativeShooterBatchProjectileImpact(p,z);if(p->mDead)break;}
    }
    return true;
}
}
