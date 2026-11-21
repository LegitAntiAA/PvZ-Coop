#include "FusionNativeShooter267.h"
#include "FusionNative.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeFire.h"
#include "FusionNativeCold.h"
#include "FusionNativePoison.h"
#include "FusionNativeDeath.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionBasePlantRules.h"
#include "FusionBatchActionRules.h"
#include "FusionCherryShooterRules.h"
#include "FusionPotatoRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
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
constexpr double PX=kFusionBasePixelsX,pi=3.14159265358979323846;
bool Current(const GameObject* o)
{return o&&o->mApp&&o->mBoard&&o->mApp->mBoard==o->mBoard&&!o->mApp->NetIsClient()&&!o->mNetGhost&&IsNativeFusionBoard(o->mBoard);}
bool Playing(const GameObject* o)
{return Current(o)&&!o->mBoard->mPaused&&o->mApp->mGameScene==SCENE_PLAYING;}
int Rows(Board* b){return GetNativeFusionRowCount(b);}
double RowPitch(Board* b,int row)
{const int from=(std::max)(0,(std::min)(row,Rows(b)-2));return b->GridToPixelY(0,from+1)-b->GridToPixelY(0,from);}
bool Target(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&z->mHasHead&&IsNativeFusionGroundZombie(z)&&!z->mMindControlled&&!z->IsFlying();}
int FirstArmor(const Zombie* z)
{return z->mHelmType==HELMTYPE_BUCKET?2:z->mHelmType==HELMTYPE_FOOTBALL?4:0;}
int SecondArmor(const Zombie* z)
{return z->mShieldType==SHIELDTYPE_DOOR?1:z->mShieldType==SHIELDTYPE_LADDER?3:0;}
int SourceKind(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d?d->kind:-1;}
void AdvanceSequence(NativePlantData* d)
{if(d->batchShotCount<static_cast<uint32_t>((std::numeric_limits<int>::max)()))++d->batchShotCount;}
bool Anchor(Plant* p,float& x,float& y)
{return p&&p->GetShadowCenter(x,y);}
bool Muzzle(Plant* p,float& x,float& y)
{
    if(!Anchor(p,x,y))return false;
    double dx=0,dy=0;
    switch(GetNativePlantData(p)->kind)
    {
    case 1007:dx=.4302771625;dy=.79365362;break;
    case 919:dx=.36410762;dy=.6120785484;break;
    case 1047:dx=.31810762;dy=.6304785484;break;
    case 1103:dx=.5742000924;dy=.6685000175;break;
    case 927:dx=1.27650018;dy=1.05149985;break;
    default:dx=.618;dy=.69;break;
    }
    x+=float(dx*PX);y-=float(dy*GetNativeFusionPixelsY(p->mBoard));return true;
}
Zombie* Acquire(Plant* p)
{
    const auto k=GetNativePlantData(p)->kind;
    float x=0,y=0;if(!Anchor(p,x,y))return nullptr;
    const double right=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*PX;
    Zombie* z=nullptr;Zombie* nearest=nullptr;float at=0;
    while(p->mBoard->IterateZombies(z))if(Target(z))
    {
        float zx=0,zy=0;if(!z->GetShadowCenter(zx,zy)||zx<=x||zx>=right)continue;
        if((k==919||k==1047)?std::abs(z->mRow-p->mRow)>1:z->mRow!=p->mRow)continue;
        if((k==1172||k==1158)&&!FusionBaseCanAcquire(7,GetNativeFusionZombieStatus(z),true,false,false,zx-x))continue;
        if(!nearest||zx<at){nearest=z;at=zx;}
    }
    return nearest;
}
Projectile* Emit(Plant* p,uint16_t kind,int damage,int row,uint8_t way,float x,float y,float speed=6,
    double angle=0,uint16_t stamp=0,uint32_t token=0,uint8_t fire=0,uint8_t variant=0)
{
    auto* b=p->mBoard;
    const double py=GetNativeFusionPixelsY(b);
    if(!Current(p)||row<0||row>=Rows(b)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize||!EnsureNativeProjectileVisuals(b,kind))return nullptr;
    NativeProjectileData data;data.kind=kind;data.damage=damage;data.specialType=way;data.fireState=fire;data.effectVariant=variant;
    data.lastTorchId=stamp;data.lastTorchToken=token;
    const auto type=NativeProjectileMetadataType(data);const auto motion=NativeProjectileMetadataMotion(data);
    if(!NativeProjectileMetadataValid(data,type,motion))
    {LawnNet::NetTrace("Fusion native Shooter267 emitReject source=%u payload=%u damage=%d way=%u type=%d motion=%d fire=%u effect=%u",GetNativePlantData(p)->kind,kind,damage,way,int(type),int(motion),fire,variant);return nullptr;}
    auto* q=b->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,row,PROJECTILE_PEA);
    if(!q)return nullptr;
    q->mProjectileType=type;q->mMotionType=motion;q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=0;
    q->mX=int(q->mPosX);q->mY=int(q->mPosY);q->mShadowY=y;
    q->mVelX=float(speed*PX*.01*std::cos(angle));q->mVelY=float(-speed*py*.01*std::sin(angle));
    q->mVelZ=q->mAccZ=0;q->mRotation=float(-angle);
    q->mDamageRangeFlags=p->GetDamageRangeFlags(WEAPON_PRIMARY);
    if(kind==28||kind==36)
    {
        // SquashBullet.Awake saves these for its post-contact continuation;
        // source0/4/5 movement before contact still uses normalSpeed6.
        const float vx=RandRangeFloat(1.5f,1.9f);
        q->mVelZ=-RandRangeFloat(4.8f,5.2f)*float(py*.01);
        q->mAccZ=float(9.8*py*.0001);
        q->mRotationSpeed=vx*float(PX*.01);
    }
    if(!AttachNativeProjectile(b,q,data)){q->Die();return nullptr;}
    LawnNet::NetTrace("Fusion native Shooter267 emit plant=%u kind=%u projectile=%u payload=%u row=%d way=%u damage=%d tick=%d",p->mNetId,GetNativePlantData(p)->kind,q->mNetId,kind,row,way,damage,b->mMainCounter);
    return q;
}
void Retire(Projectile* p)
{if(auto* c=GetNativeContext(p->mBoard))c->rollingHits.erase(p->mBoard->mProjectiles.DataArrayGetID(p));p->Die();}
void AreaFume(Plant* p)
{
    const auto k=GetNativePlantData(p)->kind;float x=0,y=0;if(!Anchor(p,x,y))return;
    const double py=GetNativeFusionPixelsY(p->mBoard);
    const float effectY=y-float(.5*py);
    p->mApp->AddTodParticle(x,effectY,p->mRenderOrder+1,PARTICLE_FUMECLOUD);
    LawnNet::NetBroadcastParticle(p->mApp,x,effectY,p->mRenderOrder+1,PARTICLE_FUMECLOUD);
    p->mApp->PlayFoley(FOLEY_FUME);
    std::vector<ZombieID> ids;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(Target(z)&&z->mRow==p->mRow)
    {
        float zx=0,zy=0;if(!z->GetShadowCenter(zx,zy)||zx<x)continue;
        if(k!=904&&!FusionFumeCanHit(GetNativeFusionZombieStatus(z),true,false,false,zx-x))continue;
        ids.push_back(p->mBoard->ZombieGetID(z));
    }
    for(auto id:ids)if((z=p->mBoard->ZombieTryToGet(id))&&Target(z))
    {
        if(k==904)
        {
            const auto* src=GetNativeSourceZombieData(z);float threshold=1;
            if(!TryGetFusionHypnoFumeThreshold(src?src->health:float(z->mBodyHealth),z->mBodyMaxHealth,z->mHelmHealth,z->mHelmMaxHealth,0,threshold))continue;
            if(RandRangeFloat(0,1)>=threshold)
            {
                float zx=0,zy=0;z->GetShadowCenter(zx,zy);z->StopEating();z->StartMindControlled();z->TrySpawnLevelAward();
                Zombie* other=nullptr;std::vector<ZombieID> blast;
                while(p->mBoard->IterateZombies(other))if(Target(other)&&other->mRow==p->mRow)
                {const auto r=other->GetZombieRect();const double scale=kFusionBasePixelsY/py;
                if(FusionFireCircleTouchesRect(zx,zy,1,r.mX,zy+(r.mY-zy)*scale,r.mWidth,r.mHeight*scale))blast.push_back(p->mBoard->ZombieGetID(other));}
                for(auto hit:blast)if((other=p->mBoard->ZombieTryToGet(hit))&&Target(other))other->TakeDamage(450,18U);
                p->mApp->AddTodParticle(zx,zy,p->mRenderOrder+1,PARTICLE_DOOM);
                LawnNet::NetBroadcastParticle(p->mApp,zx,zy,p->mRenderOrder+1,PARTICLE_DOOM);
            }
            else {const FusionColdDamageContext cold{0,FusionColdDamageDelivery::IceAll};z->TakeDamage(150,2U,&cold);AddNativeColdLevel(z,10);}
        }
        else
        {
            const bool iron=FusionSourceZombieWithIron(FirstArmor(z),SecondArmor(z),SourceKind(z));
            z->TakeDamage(k==1158&&iron?40:20,2U);
            if(k==1172)AddNativePoisonLevel(z);
        }
        LawnNet::NetTrace("Fusion native Shooter267 fume plant=%u kind=%u target=%u body=%d helm=%d poison=%d tick=%d",p->mNetId,k,z->mNetId,z->mBodyHealth,z->mHelmHealth,GetNativePoisonLevel(z),p->mBoard->mMainCounter);
    }
}
void Three(Plant* p,uint16_t kind,int damage,bool extra=false)
{
    float x=0,y=0;if(!Muzzle(p,x,y))return;
    Emit(p,kind,damage,p->mRow,0,x,y);
    if(extra)return;
    if(p->mRow>0)Emit(p,kind,damage,p->mRow-1,4,x,y);
    if(p->mRow+1<Rows(p->mBoard))Emit(p,kind,damage,p->mRow+1,5,x,y);
    if(p->mRow==0||p->mRow==Rows(p->mBoard)-1)p->mDoSpecialCountdown=21;
}
void FireEvent(Plant* p)
{
    auto* d=GetNativePlantData(p);const auto k=d->kind;
    if(k==1172||k==1158||k==904)AreaFume(p);
    else if(k==919||k==1047)Three(p,k==919?87:28,k==919?160:40);
    else
    {
        float x=0,y=0;if(!Muzzle(p,x,y))return;
        int damage=k==927?120:20;float speed=6,rate=1;
        if(k==927)TryGetFusionUltimateGatling(p->mPlantHealth,p->mPlantMaxHealth,rate,speed,damage);
        if(Emit(p,k==1007?7:k==1103?47:93,damage,p->mRow,0,x,y,speed)&&k==927&&p->mPlantHealth>1500)
        {p->mPlantHealth-=50;p->mEatenFlashCountdown=25;NativePlantHealthChanged(p);}
    }
    AdvanceSequence(d);p->mApp->PlayFoley(FOLEY_THROW);
    LawnNet::NetTrace("Fusion native Shooter267 callback plant=%u kind=%u sequence=%u mode=%u tick=%d",p->mNetId,k,d->batchShotCount,d->batchMode,p->mBoard->mMainCounter);
}
FusionShooterTimingProfile Clock(const Plant* p)
{
    auto clock=*FindFusionShooter267Timing(GetNativePlantData(p)->kind);
    if(clock.kind==927)
    {
        const auto* d=GetNativePlantData(p);
        if(d->batchMode==3)clock=*FindFusionShooter267Timing(927,"shoot_loop");
        const float rate=d->batchBagCount==0?1.f:d->batchBagCount==1?1.5f:2.f;
        clock.stateSpeed=rate;clock.speedParameterActive=false;
    }
    return clock;
}
void BeginMode(Plant* p,uint8_t mode)
{
    auto* d=GetNativePlantData(p);int ticks=0;
    if(!TryGetFusionShooter267PhaseTicks(d->kind,mode,d->animationSpeed,d->batchBagCount,ticks))return;
    d->batchMode=mode;p->mShootingCounter=0;
    const bool idle=d->kind==904?mode==0:mode==1;
    d->shootSeconds=idle?-1:0;
    if(mode==2||(d->kind==927&&mode==3))BeginNativeShooterShot(p,Clock(p));
    p->mStateCountdown=ticks;
}
void Torch(Plant* p)
{
    auto* pd=GetNativePlantData(p);std::vector<uint32_t> shots;Projectile* q=nullptr;
    while(p->mBoard->IterateProjectiles(q))if(!q->mDead)shots.push_back(p->mBoard->mProjectiles.DataArrayGetID(q));
    const auto area=p->GetPlantAttackRect(WEAPON_PRIMARY);
    for(auto id:shots)
    {
        q=p->mBoard->mProjectiles.DataArrayTryToGet(id);if(!q||q->mDead)continue;
        const auto* current=GetNativeProjectileData(q);if(!current||current->isZombieBullet||
            (current->lastTorchId==p->mNetId&&current->lastTorchToken==pd->instanceToken))continue;
        if(GetRectOverlap(area,q->GetProjectileRect())<10)continue;
        const auto data=*current;
        if(pd->kind==932)
        {
            if(data.kind==98||data.kind==99||q->mRow!=p->mRow||
                (data.specialType!=0&&data.specialType!=3&&data.specialType!=4&&data.specialType!=5&&data.specialType!=9&&data.specialType!=15))continue;
            const double py=GetNativeFusionPixelsY(p->mBoard);
            float x=0,y=0;Anchor(p,x,y);y-=float(.5*py);
            const float speed=q->mMotionType==MOTION_THREEPEATER?float(std::abs(q->mVelX/PX)*100):float(std::hypot(q->mVelX/PX,q->mVelY/py)*100);
            const int wanted=3;if(p->mBoard->mProjectiles.mSize+wanted>p->mBoard->mProjectiles.mMaxSize)continue;
            if(!EnsureNativeProjectileVisuals(p->mBoard,98)||!EnsureNativeProjectileVisuals(p->mBoard,99))continue;
            auto* a=Emit(p,98,data.damage,p->mRow?p->mRow-1:p->mRow,p->mRow?4:0,x+(p->mRow?0:float(.5*PX)),y,speed,0,p->mNetId,pd->instanceToken,0,1);
            auto* b=Emit(p,98,data.damage,p->mRow+1<Rows(p->mBoard)?p->mRow+1:p->mRow,p->mRow+1<Rows(p->mBoard)?5:0,x+(p->mRow+1<Rows(p->mBoard)?0:float(.5*PX)),y,speed,0,p->mNetId,pd->instanceToken,0,1);
            auto* c=Emit(p,99,data.damage,p->mRow,0,x,y,speed,0,p->mNetId,pd->instanceToken,0,1);
            if(a&&b&&c)q->Die();else{if(a)a->Die();if(b)b->Die();if(c)c->Die();}
        }
        else
        {
            const bool anyRow=data.specialType==2||data.specialType==6||data.specialType>=15&&data.specialType<=18;
            if(!anyRow&&q->mRow!=p->mRow)continue;
            if(data.kind==3)
            {
                auto* fresh=Emit(p,36,20,p->mRow,0,q->mPosX+20,q->mPosY+20,6,0,p->mNetId,pd->instanceToken);
                if(fresh){q->Die();NativeFireConversionCommitted(p);}
            }
            else if(data.kind==87&&!data.fireState)
            {
                auto hot=data;hot.damage*=2;hot.fireState=1;hot.lastTorchId=p->mNetId;hot.lastTorchToken=pd->instanceToken;
                if(AttachNativeProjectile(p->mBoard,q,hot))LawnNet::NetTrace("Fusion native Shooter267 heat87 plant=%u projectile=%u damage=%d",p->mNetId,q->mNetId,hot.damage);
            }
            else if(data.kind==11&&!data.effectVariant)
            {
                auto hot=data;hot.damage=320;hot.effectVariant=1;hot.lastTorchId=p->mNetId;hot.lastTorchToken=pd->instanceToken;
                AttachNativeProjectile(p->mBoard,q,hot);
            }
        }
    }
}
}
bool IsNativeShooter267(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&IsFusionShooter267Kind(d->kind);}
const FusionShooterTimingProfile* GetNativeShooter267Timing(const Plant* p)
{const auto* d=GetNativePlantData(p);if(!d)return nullptr;return FindFusionShooter267Timing(d->kind,d->kind==927&&d->batchMode==3?"shoot_loop":d->kind==904?"shooting":d->kind==1103?"shoot1":"shoot");}
void InitializeNativeShooter267(Plant* p)
{
    if(!IsNativeShooter267(p)||!Current(p)||p->mDead)return;
    auto* d=GetNativePlantData(p);d->batchShotCount=d->batchRageTicks=d->batchBagCount=d->batchMode=0;
    p->mShootingCounter=p->mDoSpecialCountdown=p->mStateCountdown=0;p->mTargetX=p->mTargetY=0;
    if(const auto* t=GetNativeShooter267Timing(p))InitializeNativeShooterTiming(p,*t,true);
    else {d->animationSpeed=RandRangeFloat(.9f,1.1f);p->mLaunchCounter=0;d->shootSeconds=-1;}
    if(d->kind==1007)
    {p->mState=STATE_NOTREADY;TryGetFusionPotatoPreparationTicks(RandRangeFloat(14,16),p->mStateCountdown);}
    if(d->kind==927)BeginMode(p,0);
    LawnNet::NetTrace("Fusion native Shooter267 init plant=%u kind=%u launch=%d state=%d speed=%.9g",p->mNetId,d->kind,p->mLaunchCounter,p->mStateCountdown,d->animationSpeed);
}
bool UpdateNativeShooter267(Plant* p)
{
    if(!IsNativeShooter267(p))return false;if(!Playing(p)||p->mDead||p->mPlantHealth<=0)return true;
    auto* d=GetNativePlantData(p);const auto k=d->kind;
    if(k==902||k==932){Torch(p);if(k==902)HandleNativeTorchwood(p);return true;}
    if(k==1007){p->UpdatePotato();if(p->mDead||p->mState==STATE_DOINGSPECIAL)return true;}
    if(k==927)
    {
        float rate=1,v=6;int damage=120;TryGetFusionUltimateGatling(p->mPlantHealth,p->mPlantMaxHealth,rate,v,damage);
        const uint16_t tier=rate==1?0:rate==1.5f?1:2;
        if(tier!=d->batchBagCount&&(d->batchMode==2||d->batchMode==3))
        {
            auto old=Clock(p);int before=0;TryGetFusionShooterShotCounter(old,d->animationSpeed,before);
            const double clipElapsed=(std::max)(0,before-p->mShootingCounter)*.01*old.stateSpeed;
            d->batchBagCount=tier;auto next=Clock(p);int initial=0;
            TryGetFusionShooterShotCounter(next,d->animationSpeed,initial);
            const int elapsed=int(std::ceil(clipElapsed/next.stateSpeed*100));
            p->mShootingCounter=(std::max)(0,initial-elapsed);
            p->mStateCountdown=(std::max)(0,int(std::ceil(FusionShooterDuration(next,d->animationSpeed)*100))-elapsed);
            d->shootSeconds=elapsed*.01f;
        }
        d->batchBagCount=tier;
        if(d->batchMode==0){if(!p->mStateCountdown)BeginMode(p,1);return true;}
        if((d->batchMode==2||d->batchMode==3)&&!p->mStateCountdown)BeginMode(p,3);
    }
    if(k==904&&d->batchMode&&p->mStateCountdown==0)BeginMode(p,d->batchMode==1?2:d->batchMode==2?2:0);
    if((k==919||k==1047)&&p->mDoSpecialCountdown==1)Three(p,k==919?87:28,k==919?160:40,true);
    if(--p->mLaunchCounter>0)return true;
    auto clock=Clock(p);
    if(k==1007)
    {
        auto* z=Acquire(p);float x=0,y=0,zx=0,zy=0;Anchor(p,x,y);
        clock.attackIntervalSeconds=1.5f;
        if(z&&z->GetShadowCenter(zx,zy))TryGetFusionPeaMineInterval(float((zx-x)/PX),clock.attackIntervalSeconds);
    }
    if(k==904)
    {
        const float seconds=clock.attackIntervalSeconds+RandRangeFloat(-.1f,.1f);
        p->mLaunchCounter=int(std::floor(double(seconds)*100))+1;
    }
    else TryGetFusionShooterCycleTicks(clock,RandRangeFloat(.95f,1.05f),p->mLaunchCounter);
    const bool target=Acquire(p)!=nullptr;
    if(k==927)
    {if(target&&d->batchMode==1)BeginMode(p,2);else if(!target&&d->batchMode>=2)BeginMode(p,1);return true;}
    if(k==904)
    {if(target&&!d->batchMode)BeginMode(p,1);else if(!target&&(d->batchMode==1||d->batchMode==2))BeginMode(p,3);return true;}
    if(target&&d->shootSeconds<0)BeginNativeShooterShot(p,clock);
    return true;
}
void UpdateNativeShooter267Phase(Plant* p)
{
    if(!IsNativeShooter267(p)||!Playing(p)||p->mDead)return;
    auto* d=GetNativePlantData(p);
    if(d->kind==902||d->kind==932){d->idleSeconds+=.01f;return;}
    if(d->kind==904||d->kind==927)
    {
        d->idleSeconds+=.01f;
        if((d->batchMode==0&&d->kind==904)||(d->batchMode==1&&d->kind==927))d->shootSeconds=-1;
        else
        {
            int total=0;
            // BeginMode can run earlier in this same tick. Derive presentation
            // from the sole native phase clock, keeping its activation at0.
            if(TryGetFusionShooter267PhaseTicks(d->kind,d->batchMode,d->animationSpeed,d->batchBagCount,total))
                d->shootSeconds=float((std::max)(0,total-p->mStateCountdown))*.01f;
        }
        return;
    }
    UpdateNativeShooterPhase(p,Clock(p));
}
bool DispatchNativeShooter267Shot(Plant* p)
{
    if(!IsNativeShooter267(p))return false;if(!Playing(p)||p->mDead)return true;
    const auto* d=GetNativePlantData(p);if(d->kind==902||d->kind==932)return true;
    if((d->kind==904&&d->batchMode!=2)||(d->kind==927&&d->batchMode<2))return true;
    const auto clock=Clock(p);
    for(size_t i=0;i<clock.eventClipSeconds.size();++i)
    {bool due=false;if(TryGetFusionShooterEventAtCounter(clock,d->animationSpeed,p->mShootingCounter,i,due)&&due)FireEvent(p);}
    return true;
}
bool GetNativeShooter267VisualPose(const Plant* p,const char*& state,float& elapsed)
{
    if(!IsNativeShooter267(p))return false;const auto* d=GetNativePlantData(p);
    state="idle";elapsed=d->idleSeconds;
    if(d->kind==904)
    {constexpr const char* states[]={"idle","startshoot","shooting","endshoot"};if(d->batchMode>3)return false;state=states[d->batchMode];if(d->batchMode)elapsed=d->shootSeconds;}
    else if(d->kind==927)
    {constexpr const char* states[]={"Kaiji","idle2","shoot","shoot_loop"};if(d->batchMode>3)return false;state=states[d->batchMode];elapsed=d->batchMode==1?d->idleSeconds:d->shootSeconds;
        if(d->batchMode>=2)elapsed*=float((d->batchBagCount==0?1.:d->batchBagCount==1?1.5:2.)/d->animationSpeed);}
    else if(d->kind==1007&&d->shootSeconds<0)
    {
        state=p->mState==STATE_NOTREADY?"idle":p->mState==STATE_POTATO_RISING?"rise":p->mState==STATE_DOINGSPECIAL?"mashed":"armed";
        if(p->mState==STATE_POTATO_RISING)elapsed=(std::max)(0,kFusionPotatoRiseTicks-p->mStateCountdown)*.01f;
        if(p->mState==STATE_DOINGSPECIAL)elapsed=(std::max)(0,kFusionPotatoDeathTicks-1-p->mDisappearCountdown)*.01f;
    }
    else if(d->shootSeconds>=0){const auto* t=GetNativeShooter267Timing(p);state=t?t->visualState:"idle";elapsed=d->shootSeconds;}
    return std::isfinite(elapsed)&&elapsed>=0;
}
void NativeShooter267PlantRemoving(Plant* p,int reason)
{
    if(!IsNativeShooter267(p)||!Current(p)||!FusionDeathReasonHasEffects(reason))return;
    const auto k=GetNativePlantData(p)->kind;
    if(k==902)ApplyNativeBatchFireRow(p->mBoard,p->mRow);
    if(k==919)for(int r=0;r<Rows(p->mBoard);++r)ApplyNativeBatchFireRow(p->mBoard,r);
    if(k==1007)
    {float x=0,y=0;if(!Anchor(p,x,y))return;for(int degrees=0;degrees<360;degrees+=10)Emit(p,7,20,p->mRow,2,x,y,6,(degrees-90)*pi/180);}
}
bool IsNativeShooter267Projectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);if(!d)return false;
    return (d->kind==7||d->kind==47||d->kind==87||d->kind==93||d->kind==98||d->kind==99||d->kind==11&&d->effectVariant==1)&&
        NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);
}
bool MoveNativeShooter267Projectile(Projectile* p)
{
    if(!IsNativeShooter267Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    auto* b=p->mBoard;auto* d=GetNativeProjectileData(p);
    if(p->mProjectileAge>2000||p->mPosX< -250||p->mPosX>b->GetCellCenterX(b->GetColumnCount()-1,p->mRow)+400||p->mPosY< -250||p->mPosY>900){Retire(p);return true;}
    if(d->specialType==4||d->specialType==5)
    {
        uint8_t next=0;int row=0;
        if((d->kind==98||d->kind==99)&&TryGetFusionGoldTorchReturn(d->specialType,d->effectVariant,p->mProjectileAge,p->mRow,Rows(b),next,row))
        {d->specialType=next;d->effectVariant=0;p->mRow=row;p->mProjectileAge=0;p->mVelY=0;}
        else {float dy=0;TryGetFusionShooter267ThreeVerticalStep(d->specialType,(std::max)(0,p->mProjectileAge-1),dy,RowPitch(b,p->mRow));p->mVelY=dy;}
    }
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;return true;
}
bool GetNativeShooter267ProjectileRect(const Projectile* p,Sexy::Rect& rect)
{
    if(!IsNativeShooter267Projectile(p))return false;const auto* d=GetNativeProjectileData(p);FusionRecipientRect r;
    const bool made=TryMakeFusionShooter267ProjectileRect(d->kind,p->mPosX+20,p->mPosY+p->mPosZ+20,r,
        GetNativeFusionPixelsY(p->mBoard),d->kind==7&&d->specialType==2?-p->mRotation:0);
    if(!made)return false;
    const int x=int(std::floor(r.x)),y=int(std::floor(r.y));rect=Sexy::Rect(x,y,int(std::ceil(r.x+r.width))-x,int(std::ceil(r.y+r.height))-y);return true;
}
bool NativeShooter267ProjectileCanHit(const Projectile* p,Zombie* z)
{return IsNativeShooter267Projectile(p)&&Target(z)&&p->mBoard==z->mBoard&&
    (p->mRow==z->mRow||(GetNativeProjectileData(p)->kind==7&&GetNativeProjectileData(p)->specialType==2));}
bool ApplyNativeShooter267ProjectileImpact(Projectile* p,Zombie* z)
{
    if(!IsNativeShooter267Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    if(!NativeShooter267ProjectileCanHit(p,z))return true;auto* d=GetNativeProjectileData(p);const auto k=d->kind;
    auto* c=GetNativeContext(p->mBoard);const auto id=p->mBoard->mProjectiles.DataArrayGetID(p);
    if((k==87||k==93)&&(!c||!RememberFusionRollingHit(c->rollingHits,id,p->mBoard->ZombieGetID(z))))return true;
    int damage=d->damage;
    if(k==47)TryGetFusionMagnetCactusDamage(damage,FirstArmor(z),SecondArmor(z),damage);
    if(k==11)TryGetFusionIronDamage(damage,z->mHelmHealth,damage);
    z->TakeDamage(damage,k==87?1U:0U);if(k==87)NativeWarmAfterDamage(z);
    if(k==93&&!z->mDead)
    {
        float x=0,y=0;if(z->GetShadowCenter(x,y))
        {const double right=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,z->mRow)+(4.0-2.2)*PX;
        if(x<=right){z->mPosX+=float((std::min)(.05*PX,right-x));z->mX=int(z->mPosX);}}
    }
    if(k==93)d->damage=static_cast<int>(d->damage*.5f);
    p->PlayImpactSound(z);
    LawnNet::NetTrace("Fusion native Shooter267 impact projectile=%u payload=%u target=%u damage=%d body=%d helm=%d shield=%d tick=%d",p->mNetId,k,z->mNetId,damage,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,p->mBoard->mMainCounter);
    if(k!=87&&k!=93||FusionProjectileHitCount(c->rollingHits,id)>=3)Retire(p);
    return true;
}
bool HandleNativeShooter267ProjectileCollision(Projectile* p)
{
    if(!IsNativeShooter267Projectile(p))return false;if(!Playing(p)||p->mDead)return true;
    Sexy::Rect r;if(!GetNativeShooter267ProjectileRect(p,r))return true;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(NativeShooter267ProjectileCanHit(p,z)&&r.Intersects(z->GetZombieRect()))
    {
        const auto* d=GetNativeProjectileData(p);
        if(d->kind==7&&d->specialType==2)
        {const auto t=z->GetZombieRect();if(!FusionShooter267RotatedProjectileTouchesRect(7,p->mPosX+20,p->mPosY+p->mPosZ+20,-p->mRotation,{double(t.mX),double(t.mY),double(t.mWidth),double(t.mHeight)},GetNativeFusionPixelsY(p->mBoard)))continue;}
        ApplyNativeShooter267ProjectileImpact(p,z);if(p->mDead)break;
    }
    return true;
}
}
