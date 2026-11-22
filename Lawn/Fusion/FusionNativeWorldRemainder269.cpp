#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeStar269.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeWater268.h"
#include "FusionFireActionBatchRules.h"
#include "FusionBasePlantRules.h"
#include "FusionRecipientShooterRules.h"
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
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr float pi=3.14159265358979323846f;
bool Host(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const GameObject* p)
{return Host(p)&&!p->mBoard->mPaused&&CanApplyNativeFusionGameplay(p->mBoard);}
bool Live(const Plant* p){return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
bool Live(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&GetNativeFusionZombieHealth(z)>0&&!NativeWater268ZombieCaptured(z);}
NativeWorldRemainder269Reference Ref(const Plant* p)
{const auto* d=GetNativePlantData(p);return d?NativeWorldRemainder269Reference{p->mNetId,d->instanceToken}:NativeWorldRemainder269Reference{};}
Plant* Resolve(Board* b,NativeWorldRemainder269Reference ref)
{if(!ref.id||!ref.token)return nullptr;Plant* p=nullptr;while(b->IteratePlants(p))if(Live(p)&&p->mNetId==ref.id&&Ref(p).token==ref.token)return p;return nullptr;}
int Count(Board* b,uint16_t kind)
{int count=0;Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(Live(p)&&d&&d->kind==kind)++count;}return count;}
void SummonPose(Board* b,uint16_t kind)
{Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(Live(p)&&d&&d->kind==kind)p->mDoSpecialCountdown=253;}}
void Position(Projectile* p,float x,float y)
{const float dy=y-p->mPosY-p->mPosZ-20;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);p->mShadowY+=dy;}
float BoardY(Board* b,float sourceY)
{return float(b->GridToPixelY(0,0)+50+(2.3-sourceY)*GetNativeFusionPixelsY(b));}
float BoardX(Board* b,float sourceX)
{return float(b->GetCellCenterX(0,0)+(sourceX+4.8)*kFusionBasePixelsX);}
bool Acquire(Plant* p)
{
    const auto kind=GetNativePlantData(p)->kind;Zombie* z=nullptr;float x=0,y=0;
    if(!p->GetShadowCenter(x,y))return false;
    if(kind==926)while(p->mBoard->IterateZombies(z))
    {
        float zx=0,zy=0;const float vision=float(p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX);
        if(!Live(z)||!z->GetShadowCenter(zx,zy)||zx>=vision)continue;
        if(FusionWorldRemainder269CattailTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,true))return true;
    }
    if(kind==909)for(size_t ray=0;ray<5;++ray)
    {
        z=nullptr;while(p->mBoard->IterateZombies(z))
        {
            if(!Live(z)||!FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),z->mMindControlled,z->IsDeadOrDying()))continue;
            const auto r=z->GetZombieRect();double distance=0;
            if(TryGetFusionStarAcquisitionDistance(ray,z->mRow==p->mRow,x,y-.3*GetNativeFusionPixelsY(p->mBoard),{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)},distance))return true;
        }
    }
    return false;
}
Projectile* Emit(Board* b,int row,float x,float y,uint16_t kind,uint8_t way,int damage,float vx,float vy,NativeWorldRemainder269Reference owner={},uint8_t special=0)
{
    if(!EnsureNativeProjectileVisuals(b,kind)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    auto* q=b->AddProjectile(int(x-20),int(y-20),0,row,kind==91?PROJECTILE_SPIKE:PROJECTILE_STAR);if(!q)return nullptr;
    q->mProjectileType=kind==91?PROJECTILE_SPIKE:PROJECTILE_STAR;
    q->mMotionType=way==6?MOTION_HOMING:kind==91?MOTION_STRAIGHT:MOTION_STAR;
    Position(q,x,y);q->mShadowY=y;q->mVelX=vx;q->mVelY=vy;q->mVelZ=q->mAccZ=0;
    q->mRotation=float(std::atan2(-vy/GetNativeFusionPixelsY(b),vx/kFusionBasePixelsX));q->mRotationSpeed=0;
    NativeProjectileData data;data.kind=kind;data.damage=damage;data.specialType=way;data.effectVariant=special;
    if(!AttachNativeProjectile(b,q,data)){q->Die();return nullptr;}
    if(kind==91)
    {auto* s=EditNativeWorldRemainder269ShotState(q,true);if(!s){q->Die();return nullptr;}*s={owner,0};}
    return q;
}
bool EmitParent(Plant* p)
{
    const auto* d=GetNativePlantData(p);float x=0,y=0;
    if(d->kind==926)
    {
        if(!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return false;
        const int damage=FusionFindPlant(926)->damage*(HasNativeWorldRemainder269Trait(p->mBoard,NativeWorldRemainder269Trait::Advanced34)?5:1);
        return Emit(p->mBoard,p->mRow,x,y,91,6,damage,float(.06*kFusionBasePixelsX),0,Ref(p))!=nullptr;
    }
    int damage=0;
    if(!TryGetFusionWorldRemainder269StarDamage(FusionFindPlant(909)->damage,GetNativeFireLightLevel(p),GetNativeStar269MagnetCount(p),HasNativeWorldRemainder269Trait(p->mBoard,NativeWorldRemainder269Trait::Ultimate9),damage))return false;
    bool emitted=false;
    for(size_t index=0;index<5;++index)
    {
        char node[]="Shoot1";node[5]=char('1'+index);FusionRecipientEmissionSpec ray;
        if(!GetNativeSourcePlantNodePoint(p,node,x,y)||!TryGetFusionRecipientEmission(23,index,ray))continue;
        emitted=Emit(p->mBoard,p->mRow,x,y,49,uint8_t(ray.sourceMovingWay),damage,float(ray.velocityX),float(ray.velocityY/kFusionBasePixelsY*GetNativeFusionPixelsY(p->mBoard)))||emitted;
    }
    return emitted;
}
bool Rings(Projectile* p,const NativeWorldRemainder269AuxState& s)
{
    const uint16_t kind=s.type==1?48:(s.type==2||s.type==5)?49:39;
    const int damage=(s.type==5?200:s.type==2?100:s.type==1?60:20)*(s.sun?2:1);
    const float x=p->mPosX+20,y=p->mPosY+20;
    bool complete=true;
    for(int ring=1;ring<=5;++ring)for(int direction=0;direction<36;++direction)
    {
        const float angle=float(direction)*pi/18,radius=float(ring)*.5f;
        auto* q=Emit(p->mBoard,2,x+std::cos(angle)*radius*float(kFusionBasePixelsX),y-std::sin(angle)*radius*float(GetNativeFusionPixelsY(p->mBoard)),kind,2,damage,float(std::cos(angle)*.06*kFusionBasePixelsX),float(-std::sin(angle)*.06*GetNativeFusionPixelsY(p->mBoard)),{},s.type==1?4:0);
        if(!q)complete=false;
    }
    return complete;
}
void Impact(Projectile* p,NativeWorldRemainder269AuxState& s)
{
    if(s.impacted)return;s.impacted=true; // Set before callbacks which can retire actors.
    const auto snapshot=s;const float x=p->mPosX+20,y=p->mPosY+20;
    const int stars=Count(p->mBoard,909)+Count(p->mBoard,920),solar=Count(p->mBoard,934);
    double amount=1800+(HasNativeWorldRemainder269Trait(p->mBoard,NativeWorldRemainder269Trait::Advanced19)?300.*stars:0);
    if(s.type==5)amount=(1+.2*solar)*(amount+1800.*stars);
    if(amount>(std::numeric_limits<int>::max)()){p->Die();return;}
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        if(!Live(z)||z->mMindControlled||GetNativeFusionZombieStatus(z)==1)continue;
        const auto rect=z->GetZombieRect();const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
        if(s.type==5||FusionFireCircleTouchesRect(x,y,11,rect.mX,y+(rect.mY-y)*ratio,rect.mWidth,rect.mHeight*ratio))ApplyNativeSourceExplosionDamage(z,int(amount));
    }
    const uint16_t effect=snapshot.type==2?51:snapshot.type==5?98:49;
    (void)CreateNativeWorldRemainder269ImpactEffect(p->mBoard,effect,x,y,p->mRow);
    // Source sun births are real native coins; values/canonical sourceItem0 are
    // attached by the common source-sun adapter (Root callback below).
    if(snapshot.type==5||snapshot.sun)EmitNativeWorldRemainder269MeteoriteSun(p->mBoard,x,y,snapshot.type==5?50:10);
    p->mApp->PlayFoley(FOLEY_CHERRYBOMB);
    (void)Rings(p,snapshot);p->Die();
}
}
bool IsNativeWorldRemainder269Plant(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&IsNativeFusionBoard(p->mBoard)&&(d->kind==909||d->kind==926||d->kind==918);}
bool InitializeNativeWorldRemainder269Plant(Plant* p)
{
    if(!IsNativeWorldRemainder269Plant(p))return false;if(!Host(p))return true;
    auto* d=GetNativePlantData(p);p->mState=STATE_NOTREADY;p->mStateCountdown=p->mDoSpecialCountdown=p->mShootingCounter=0;d->shootSeconds=-1;d->idleSeconds=0;
    if(d->kind==918){d->animationSpeed=RandRangeFloat(.9f,1.1f);p->mLaunchCounter=0;return true;}
    return InitializeNativeShooterTiming(p,*FindFusionWorldRemainder269Timing(d->kind),false);
}
bool UpdateNativeWorldRemainder269Plant(Plant* p)
{
    if(!IsNativeWorldRemainder269Plant(p))return false;if(!Playing(p)||p->mDead)return true;auto* d=GetNativePlantData(p);
    if(d->kind==918)return true;
    if(--p->mLaunchCounter<=0)
    {
        const auto* timing=FindFusionWorldRemainder269Timing(d->kind);
        if(d->kind==909)p->mLaunchCounter=HasNativeWorldRemainder269Trait(p->mBoard,NativeWorldRemainder269Trait::Ultimate8)||NativeWorldRemainder269ShootingBoard(p->mBoard)?50:150;
        else ResetNativeShooterLaunchCounter(p,*timing);
        if(d->shootSeconds<0&&Acquire(p))BeginNativeShooterShot(p,*timing);
    }
    return true;
}
void UpdateNativeWorldRemainder269PlantPhase(Plant* p)
{if(IsNativeWorldRemainder269Plant(p)&&Playing(p)&&!p->mDead){auto* d=GetNativePlantData(p);const auto* t=FindFusionWorldRemainder269Timing(d->kind);if(t)UpdateNativeShooterPhase(p,*t);else d->idleSeconds+=.01f;d->pendingProductionTicks=d->kind==909?p->mDoSpecialCountdown:0;}}
bool DispatchNativeWorldRemainder269Shot(Plant* p)
{
    if(!IsNativeWorldRemainder269Plant(p))return false;if(!Playing(p)||p->mDead)return true;
    const auto* t=FindFusionWorldRemainder269Timing(GetNativePlantData(p)->kind);if(!t)return true;
    int initial=0;if(TryGetFusionShooterShotCounter(*t,GetNativePlantData(p)->animationSpeed,initial)&&p->mShootingCounter>0&&p->mShootingCounter<=initial)
        GetNativePlantData(p)->shootSeconds=(initial-p->mShootingCounter)*.01f;
    int events=0;if(TryGetFusionShooterEventsAtCounter(*t,GetNativePlantData(p)->animationSpeed,p->mShootingCounter,events))
        while(events-->0)if(EmitParent(p)){(void)RandRangeInt(3,4);p->mApp->PlayFoley(FOLEY_THROW);}
    return true;
}
bool GetNativeWorldRemainder269VisualPose(const Plant* p,const char*& base,float& seconds,const char*& additive,float& additiveSeconds)
{
    if(!IsNativeWorldRemainder269Plant(p))return false;const auto* d=GetNativePlantData(p);
    base=d->shootSeconds<0?"idle":"shoot";seconds=d->shootSeconds<0?d->idleSeconds:d->shootSeconds;additive=nullptr;additiveSeconds=0;
    GetNativeWorldRemainder269SummonPose(p,additive,additiveSeconds);
    return true;
}
bool GetNativeWorldRemainder269SummonPose(const Plant* p,const char*& state,float& seconds)
{
    const auto* d=GetNativePlantData(p);if(!d||(d->kind!=909&&d->kind!=1104)||p->mDoSpecialCountdown<=0||p->mDoSpecialCountdown>253)return false;
    state="summon";seconds=(253-p->mDoSpecialCountdown)*.01f;return true;
}
bool InitializeNativeWorldRemainder269Board(Board* b)
{
    if(!b||!b->mApp||b->mApp->NetIsClient()||!IsNativeFusionBoard(b))return false;
    auto* s=EditNativeWorldRemainder269BoardState(b,true);if(!s)return false;if(s->initialized)return true;
    *s={};s->initialized=true;s->passiveDeadline=b->mMainCounter+5000;s->activeDeadline=b->mMainCounter;s->ultimateDeadline=b->mMainCounter+1500;return true;
}
void UpdateNativeWorldRemainder269Board(Board* b)
{
    if(!b||!b->mApp||b->mApp->NetIsClient()||b->mPaused||!IsNativeFusionBoard(b)||!CanApplyNativeFusionGameplay(b))return;
    if(!InitializeNativeWorldRemainder269Board(b))return;auto* s=EditNativeWorldRemainder269BoardState(b);const int now=b->mMainCounter;
    if(HasNativeWorldRemainder269Trait(b,NativeWorldRemainder269Trait::Advanced19))
    {if(s->passiveDeadline>now)--s->passiveDeadline;if(s->activeDeadline>now)--s->activeDeadline;if(s->ultimateDeadline>now)--s->ultimateDeadline;}
    if(HasNativeWorldRemainder269Trait(b,NativeWorldRemainder269Trait::Debuff19))
    {if(s->passiveDeadline>now)--s->passiveDeadline;if(s->activeDeadline>now)--s->activeDeadline;if(s->ultimateDeadline>now)--s->ultimateDeadline;}
    if(now>=s->passiveDeadline)
    {const int count=Count(b,1104);s->passiveDeadline=now+int(FusionWorldRemainder269PassiveSeconds(count)*100);if(count){SummonPose(b,1104);s->pending.push_back({now+50,0});}}
    if(now>=s->ultimateDeadline)
    {s->ultimateDeadline=now+1500;if(Count(b,909)){SummonPose(b,909);const bool special=HasNativeWorldRemainder269Trait(b,NativeWorldRemainder269Trait::Advanced45)&&Count(b,909)+Count(b,920)>9&&Count(b,934)>9;s->pending.push_back({now+50,uint8_t(special?5:2)});}}
    for(size_t i=0;i<s->pending.size();)
    {const auto pending=s->pending[i];if(pending.deadline<=now&&CreateNativeWorldRemainder269Meteorite(b,pending.type)){s=EditNativeWorldRemainder269BoardState(b);s->pending.erase(s->pending.begin()+i);}else ++i;}
}
bool ActivateNativeWorldRemainder269Meteorite(Board* b)
{
    if(!b||!b->mApp||b->mApp->NetIsClient()||b->mPaused||!CanApplyNativeFusionGameplay(b)||!InitializeNativeWorldRemainder269Board(b))return false;
    auto* s=EditNativeWorldRemainder269BoardState(b);if(b->mMainCounter<s->activeDeadline||!Count(b,1104)||!EnsureNativeWorldRemainder269Assets(b,NativeWorldRemainder269Family::Meteorite,1))return false;
    s->activeDeadline=b->mMainCounter+5000;s->pending.push_back({b->mMainCounter+50,1});SummonPose(b,1104);return true;
}
Projectile* CreateNativeWorldRemainder269Meteorite(Board* b,uint8_t type)
{
    bool sun=false;if(!b||!b->mApp||b->mApp->NetIsClient()||!IsNativeFusionBoard(b)||(type!=0&&type!=1&&type!=2&&type!=5)||!EnsureNativeWorldRemainder269Assets(b,NativeWorldRemainder269Family::Meteorite,type)||!QueryNativeWorldRemainder269Solar(b,sun))return nullptr;
    const int rows=GetNativeFusionRowCount(b);const float minY=rows%2?float(b->GridToPixelY(0,rows/2)+50):float((b->GridToPixelY(0,rows/2-1)+b->GridToPixelY(0,rows/2))*.5+50);
    auto* p=CreateNativeAuxiliary269Carrier(b,rows/2,BoardX(b,-5.7f),BoardY(b,9));if(!p)return nullptr;
    auto* s=EditNativeWorldRemainder269AuxState(p,true);if(!s){p->Die();return nullptr;}
    *s={};s->family=NativeWorldRemainder269Family::Meteorite;s->instanceToken=b->mProjectiles.DataArrayGetID(p);s->targetY=minY;s->type=type;s->sun=sun;
    p->mVelX=float(.07*kFusionBasePixelsX);p->mVelY=0;p->mVelZ=p->mAccZ=0;p->mRotation=0;p->mRotationSpeed=-pi*.01f;return p;
}
bool SummonNativeWorldRemainder269LourFly(Plant* p)
{
    if(!Live(p)||!Playing(p)||GetNativePlantData(p)->kind!=926||!EnsureNativeWorldRemainder269Assets(p->mBoard,NativeWorldRemainder269Family::LourFly,0))return false;
    float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"FlyPos",x,y))return false;
    auto* q=CreateNativeAuxiliary269Carrier(p->mBoard,p->mRow,x,BoardY(p->mBoard,4));if(!q)return false;
    auto* s=EditNativeWorldRemainder269AuxState(q,true);if(!s){q->Die();return false;}
    *s={};s->family=NativeWorldRemainder269Family::LourFly;s->instanceToken=p->mBoard->mProjectiles.DataArrayGetID(q);s->parent=Ref(p);s->targetX=x;s->targetY=y;s->parentDamage=GetNativeTravelPlantAttack369(p,FusionFindPlant(926)->damage);return true;
}
bool IsNativeWorldRemainder269Aux(const Projectile* p)
{return p&&IsNativeFusionBoard(p->mBoard)&&GetNativeWorldRemainder269AuxState(p);}
bool MoveNativeWorldRemainder269GenericHoming(Projectile* p,float speed)
{
    if(!Playing(p)||speed<=0||!std::isfinite(speed))return false;
    auto acquire=[&]()
    {
        Zombie* best=nullptr;double nearest=0;const float limit=float(p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX);
        for(int pass=0;pass<2&&!best;++pass)
        {
            Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
            {
                const int status=GetNativeFusionZombieStatus(z);float zx=0,zy=0;
                if(!Live(z)||z->mMindControlled||status<0||!z->GetShadowCenter(zx,zy)||zx>=limit)continue;
                if(pass==0?status!=10:status==1||status==3||status==7||status==9||status==10||status==12||status==16||(status>=20&&status<=24))continue;
                const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=(r.mY+r.mHeight*.5-p->mPosY-p->mPosZ-20)/GetNativeFusionPixelsY(p->mBoard),distance=dx*dx+dy*dy;
                if(!best||distance<nearest){best=z;nearest=distance;}
            }
        }
        p->mTargetZombieID=best?p->mBoard->ZombieGetID(best):ZOMBIEID_NULL;
    };
    auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);
    if(!z||z->mDead)
    {
        acquire();p->mVelX=float(std::cos(p->mRotation)*speed*.01*kFusionBasePixelsX);p->mVelY=float(-std::sin(p->mRotation)*speed*.01*GetNativeFusionPixelsY(p->mBoard));
    }
    else
    {
        const int status=GetNativeFusionZombieStatus(z);
        if(!Live(z)||z->mMindControlled||status==1||status==7||status==12||status==16||(status>=21&&status<=24))acquire(); // Retain this tick's velocity.
        else
        {
            const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=-(r.mY+r.mHeight*.5-p->mPosY-p->mPosZ-20)/GetNativeFusionPixelsY(p->mBoard),length=std::hypot(dx,dy);
            if(length>1e-5){p->mVelX=float(dx/length*speed*.01*kFusionBasePixelsX);p->mVelY=float(-dy/length*speed*.01*GetNativeFusionPixelsY(p->mBoard));}
            else p->mVelX=p->mVelY=0;p->mRotation=float(std::atan2(dy,dx));
        }
    }
    Position(p,p->mPosX+20+p->mVelX,p->mPosY+p->mPosZ+20+p->mVelY);return true;
}
bool UpdateNativeWorldRemainder269Aux(Projectile* p)
{
    if(!IsNativeWorldRemainder269Aux(p))return false;if(!Playing(p)||p->mDead)return true;auto* s=EditNativeWorldRemainder269AuxState(p);s->ageTicks=p->mProjectileAge;
    if(s->family==NativeWorldRemainder269Family::Meteorite)
    {
        if(s->impacted)return true;p->mVelY+=float(.00098*GetNativeFusionPixelsY(p->mBoard));Position(p,p->mPosX+20+p->mVelX,p->mPosY+20+p->mVelY);p->mRotation=std::fmod(p->mRotation-pi*.01f,2*pi);
        if(p->mPosY+20>s->targetY)Impact(p,*s);return true;
    }
    if(!s->ready)
    {
        const float x=p->mPosX+20,y=p->mPosY+20;const double distance=std::hypot((s->targetX-x)/kFusionBasePixelsX,(s->targetY-y)/GetNativeFusionPixelsY(p->mBoard));
        const float nx=x+(s->targetX-x)*.03f,ny=y+(s->targetY-y)*.03f;
        if(distance<.1)s->ready=true;Position(p,distance<.01||ny>s->targetY?s->targetX:nx,distance<.01||ny>s->targetY?s->targetY:ny);
        s->nextShotAge=p->mProjectileAge;return true;
    }
    if(p->mProjectileAge<=s->nextShotAge)return true;
    if(auto* owner=Resolve(p->mBoard,s->parent))s->parentDamage=FusionFindPlant(GetNativePlantData(owner)->kind)->damage;
    float x=0,y=0;if(!GetNativeWorldRemainder269AuxNodePoint(p,"Pos",x,y))return true;
    if(p->mBoard->mProjectiles.mMaxSize-p->mBoard->mProjectiles.mSize<3||!EnsureNativeProjectileVisuals(p->mBoard,91))return true;
    const auto reference=s->parent;const int damage=s->parentDamage*(HasNativeWorldRemainder269Trait(p->mBoard,NativeWorldRemainder269Trait::Advanced35)?3:1);
    auto* a=Emit(p->mBoard,p->mRow,x,y,91,0,damage,float(.06*kFusionBasePixelsX),0,reference);
    auto* b=Emit(p->mBoard,p->mRow,x,y,91,10,damage,float(-.01*kFusionBasePixelsX),float(-.01*GetNativeFusionPixelsY(p->mBoard)),reference);
    auto* c=Emit(p->mBoard,p->mRow,x,y,91,10,damage,float(-.01*kFusionBasePixelsX),float(.01*GetNativeFusionPixelsY(p->mBoard)),reference);
    s=EditNativeWorldRemainder269AuxState(p);s->nextShotAge=p->mProjectileAge+int(std::floor(RandRangeFloat(.95f,1.05f)*10));
    if(a&&b&&c)s->shots=uint8_t(s->shots+3);if(s->shots>59)p->Die();return true;
}
bool MoveNativeWorldRemainder269Source91(Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);if(!d||d->kind!=91)return false;if(!Playing(p)||p->mDead)return true;
    auto* s=EditNativeWorldRemainder269ShotState(p);if(!s)return true;
    const double py=GetNativeFusionPixelsY(p->mBoard),x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;
    // Source Bullet.MoveDieUpdate bounds, shared with the existing water/PuffLove adapters.
    if(x<p->mBoard->GetCellCenterX(0,0)-7.2*kFusionBasePixelsX||
       x>p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+3.85*kFusionBasePixelsX||
       y<p->mBoard->GridToPixelY(0,0)+50-4.2*py||
       y>p->mBoard->GridToPixelY(0,GetNativeFusionRowCount(p->mBoard)-1)+50+.6*py){p->Die();return true;}
    const bool homing=d->specialType==6;
    if(homing){if(!MoveNativeWorldRemainder269GenericHoming(p,5))return true;}
    else Position(p,p->mPosX+20+p->mVelX,p->mPosY+p->mPosZ+20+p->mVelY);
    s->ageTicks=p->mProjectileAge;
    if(d->specialType==10&&p->mProjectileAge>20)
    {GetNativeProjectileData(p)->specialType=0;p->mMotionType=MOTION_STRAIGHT;p->mVelX=float(.06*kFusionBasePixelsX);p->mVelY=0;}
    return true;
}
bool ApplyNativeWorldRemainder269Source91Impact(Projectile* p,Zombie* z)
{
    const auto* d=GetNativeProjectileData(p);if(!d||d->kind!=91)return false;if(!Playing(p)||p->mDead||!Live(z)||z->mMindControlled)return true;
    const auto* state=GetNativeWorldRemainder269ShotState(p);if(!state)return true;const auto parent=state->parent;
    const int before=GetNativeFusionZombieHealth(z);const bool dying=z->IsDeadOrDying();z->TakeDamage(d->damage,0U);p->PlayImpactSound(z);
    if(!dying&&(z->IsDeadOrDying()||(before>0&&GetNativeFusionZombieHealth(z)<=0)))if(auto* owner=Resolve(p->mBoard,parent))SummonNativeWorldRemainder269LourFly(owner);
    p->Die();return true;
}
}
