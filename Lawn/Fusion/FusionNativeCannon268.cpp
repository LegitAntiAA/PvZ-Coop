#include "FusionNativeCannon268.h"
#include "FusionNative.h"
#include "FusionNativeTravelUltimate370.h"
#include "FusionNativeVisual.h"
#include "FusionNativeAction266.h"
#include "FusionNativeCold.h"
#include "FusionNativeWater268.h"
#include "FusionNativePoison.h"
#include "FusionNativeLobber.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeDefender.h"
#include <algorithm>
#include "../Board.h"
#include "../GridItem.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* object)
{
    return object&&object->mApp&&object->mBoard&&object->mApp->mBoard==object->mBoard&&
        !object->mNetGhost&&!object->mApp->NetIsClient()&&IsNativeFusionBoard(object->mBoard)&&
        NativeFusionBackgroundValid(static_cast<uint8_t>(object->mBoard->mBackground));
}
bool Live(const Plant* p)
{return IsNativeCannon268(p)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
bool StartClock(Plant* p,bool charge)
{
    auto* d=GetNativePlantData(p);const auto* clock=FindFusionCannon268Timing(d->kind,charge);
    if(!clock||!TryGetFusionCannon268ShotCounter(*clock,d->animationSpeed,p->mShootingCounter))return false;
    d->shootSeconds=0;d->batchMode=static_cast<uint8_t>(charge?FusionCannon268Phase::Charging:FusionCannon268Phase::Firing);
    p->mState=charge?STATE_COBCANNON_LOADING:STATE_COBCANNON_FIRING;return true;
}
bool Normal(Zombie* z)
{return z&&!z->mDead&&!z->mNetGhost&&!z->mMindControlled&&z->mBodyHealth>0&&!z->IsDeadOrDying()&&IsNativeFusionGroundZombie(z)&&!z->IsFlying()&&!NativeWater268ZombieCaptured(z);}
Projectile* SpawnCannon(Board* b,int row,float x,float y,float targetX,float targetY,uint16_t kind,int damage,uint8_t status,int age=0)
{
    if(!b||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize||!EnsureNativeProjectileVisuals(b,kind))return nullptr;
    auto* shot=b->AddProjectile(int(x-20),int(y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,row,0),row,PROJECTILE_COBBIG);
    if(!shot)return nullptr;
    NativeProjectileData data;data.kind=kind;data.damage=damage;data.specialType=14;data.effectVariant=status;
    shot->mMotionType=MOTION_LOBBED;
    if(!AttachNativeProjectile(b,shot,data)){shot->Die();return nullptr;}
    shot->mCobTargetX=targetX;shot->mShadowY=targetY;shot->mProjectileAge=age;
    shot->mPosX=x-20;shot->mPosY=targetY-20;shot->mPosZ=y-targetY;
    shot->mVelX=shot->mVelY=shot->mAccZ=0;
    shot->mVelZ=static_cast<float>(GetNativeFusionRowCount(b)*(age>=100?3:-1.5)*.01*GetNativeFusionPixelsY(b));
    shot->mRotation=age>=100?kFusionCannon268DownRotation:kFusionCannon268UpRotation;shot->mRotationSpeed=0;
    shot->mX=int(shot->mPosX);shot->mY=int(shot->mPosY+shot->mPosZ);return shot;
}
bool Emit(Plant* plant)
{
    auto* b=plant->mBoard;auto* d=GetNativePlantData(plant);const auto* profile=FindFusionCannon268Profile(d->kind);
    // Node sampling uses this exact event tick, not the preceding draw's phase.
    const auto* clock=FindFusionCannon268Timing(d->kind);int initial=0;
    if(clock&&TryGetFusionCannon268ShotCounter(*clock,d->animationSpeed,initial))d->shootSeconds=(initial-plant->mShootingCounter)*.01f;
    float x=0,y=0;
    if(!profile||!GetNativeSourcePlantNodePoint(plant,d->kind==1132?"Shoot":"CobCannon_cob",x,y))return false;
    if(d->kind!=1132)y-=float(.24*GetNativeFusionPixelsY(b));
    auto* shot=SpawnCannon(b,b->PixelToGridYKeepOnBoard(plant->mTargetX,plant->mTargetY),x,y,
        float(plant->mTargetX),float(plant->mTargetY),profile->projectileKind,profile->damage,d->kind==1132?1:0);
    if(!shot)return false;
    ++d->batchShotCount;
    LawnNet::NetTrace("Fusion Cannon268 shot plant=%u kind=%u projectile=%u payload=%u target=%d,%d root=%.9g,%.9g tick=%d",plant->mNetId,d->kind,shot->mNetId,profile->projectileKind,plant->mTargetX,plant->mTargetY,x,y,b->mMainCounter);
    return true;
}
void Split(Projectile* p)
{
    auto* data=GetNativeProjectileData(p);const int count=FusionCannon268SplitCount(data->kind,data->effectVariant);
    if(!count)return;
    const bool global=data->effectVariant==2;data->effectVariant=0; // Before child callbacks: no recursive fork.
    auto* b=p->mBoard;const int columns=b->GetColumnCount(),rows=GetNativeFusionRowCount(b);
    const int center=global?columns/2:b->PixelToGridXKeepOnBoard(int(p->mCobTargetX),int(p->mShadowY));
    const int minCol=global?0:(std::max)(0,center-2),maxCol=global?columns-1:(std::min)(columns-1,center+2);
    const int minRow=global?0:(std::max)(0,p->mRow-1),maxRow=global?rows-1:(std::min)(rows-1,p->mRow+1);
    const double py=GetNativeFusionPixelsY(b);const float rootY=p->mPosY+p->mPosZ+20;
    int emitted=0;
    for(int i=0;i<count;++i)
    {
        const int col=RandRangeInt(minCol,maxCol),row=RandRangeInt(minRow,maxRow);
        const float tx=float(b->GetCellCenterX(col,row)),ty=float(b->GridToPixelY(col,row)+50-.5*py);
        const float y=rootY-float(((p->mRow-row)*1.6+RandRangeFloat(3,15))*py);
        // Source's immediate CannonBulletUpdate assigns down velocity/rotation/X;
        // SpawnCannon does that without integrating a second physics step here.
        if(SpawnCannon(b,row,tx,y,tx,ty,data->kind,data->damage,0,p->mProjectileAge))++emitted;
    }
    LawnNet::NetTrace("Fusion Cannon268 split projectile=%u payload=%u requested=%d emitted=%d age=%d global=%d tick=%d",p->mNetId,data->kind,count,emitted,p->mProjectileAge,global?1:0,b->mMainCounter);
    if(global)p->Die();
}

void Impact(Projectile* p)
{
    const auto data=*GetNativeProjectileData(p);auto* b=p->mBoard;
    const float x=p->mCobTargetX,y=p->mShadowY;const double py=GetNativeFusionPixelsY(b);
    std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(b->IterateZombies(z))
    {
        if(z->mDead||z->mNetGhost||z->mMindControlled||z->mBodyHealth<=0||std::abs(z->mRow-p->mRow)>1||NativeWater268ZombieCaptured(z))continue;
        const auto rect=z->GetZombieRect();
        if(FusionCannon268AreaTouches(x,y,rect.mX,rect.mY,rect.mWidth,rect.mHeight,py))targets.push_back(b->ZombieGetID(z));
    }
    for(auto id:targets)if((z=b->ZombieTryToGet(id))&&!z->mDead&&!z->mMindControlled)
    {
        z->TakeDamage(NativeTravelUltimate370CannonDamage(b,data.kind,data.damage),data.kind==73||data.kind==83?1U:18U);
        if(data.kind==68)ApplyNativeAction266Jalaed(z);
        if(data.kind==69)ApplyNativeColdFreeze(z,500);
    }
    // Non-melon Cannon payloads remove real overlapping source Ladder grid items.
    if(data.kind!=73&&data.kind!=83)
    {
        GridItem* grid=nullptr;while(b->IterateGridItems(grid))
        {
            if(grid->mDead||grid->mGridItemType!=GRIDITEM_LADDER||std::abs(grid->mGridY-p->mRow)>1)continue;
            const double gx=b->GetCellCenterX(grid->mGridX,grid->mGridY)+.3*72/1.35,
                gy=b->GridToPixelY(grid->mGridX,grid->mGridY)+50-.6*py;
            if(FusionCannon268AreaTouches(x,y,gx-.343*72/1.35,gy-.7315*py,.686*72/1.35,1.463*py,py))grid->GridItemDie();
        }
    }
    if(data.kind==84)
    {
        // Source Board.SetDoom effect2: local explosion completed first, then
        // freeze and the current body/first-armor percentage for every hostile.
        Zombie* target=nullptr;std::vector<ZombieID> global;
        while(b->IterateZombies(target))if(!target->mDead&&!target->mNetGhost&&!target->mMindControlled&&target->mBodyHealth>0)
            global.push_back(b->ZombieGetID(target));
        for(auto id:global)if((target=b->ZombieTryToGet(id))&&!target->mDead&&!target->mMindControlled)
        {
            int damage=0;if(!TryGetFusionCannon268DoomDamage(data.damage,GetNativeFusionZombieHealth(target),target->mHelmHealth,damage))continue;
            ApplyNativeColdFreeze(target,500);target->TakeDamage(damage,18U);
        }
    }
    LawnNet::NetTrace("Fusion Cannon268 impact projectile=%u payload=%u target=%.9g,%.9g count=%zu tick=%d",p->mNetId,data.kind,x,y,targets.size(),b->mMainCounter);
    p->Die();
}
}
bool IsNativeCannon268(const Plant* p)
{
    const auto* d=GetNativePlantData(p);const auto* profile=d?FindFusionCannon268Profile(d->kind):nullptr;
    return p&&p->mBoard&&IsNativeFusionBoard(p->mBoard)&&profile&&profile->manual;
}
bool CanAimNativeCannon268(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    return Live(p)&&CanApplyNativeFusionGameplay(p->mBoard)&&d->batchMode==static_cast<uint8_t>(FusionCannon268Phase::Ready)&&
        d->productionTicks==0&&p->mState==STATE_COBCANNON_READY;
}
bool CommitAimNativeCannon268(Plant* p,int x,int y)
{
    if(!Host(p)||!CanAimNativeCannon268(p)||!p->mBoard->IsValidGridCell(p->mBoard->PixelToGridX(x,y),p->mBoard->PixelToGridY(x,y)))return false;
    const auto* clock=FindFusionCannon268Timing(GetNativePlantData(p)->kind);
    int counter=0;if(!clock||!TryGetFusionCannon268ShotCounter(*clock,GetNativePlantData(p)->animationSpeed,counter))return false;
    auto* d=GetNativePlantData(p);p->mTargetX=x;p->mTargetY=y;p->mStateCountdown=NativeTravelUltimate370CannonReload(p,3601);d->productionTicks=p->mStateCountdown;
    return StartClock(p,false);
}
bool InitializeNativeCannon268(Plant* p)
{
    if(!Host(p)||!IsNativeCannon268(p))return false;auto* d=GetNativePlantData(p);
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->idleSeconds=0;d->shootSeconds=-1;
    d->batchMode=0;d->batchShotCount=0;d->batchRageTicks=d->batchBagCount=0;d->pendingProductionTicks=0;
    p->mState=STATE_COBCANNON_ARMING;p->mStateCountdown=d->kind==1120?501:51;d->productionTicks=p->mStateCountdown;
    p->mShootingCounter=0;p->mTargetX=p->mTargetY=-1;
    if(d->kind==915){p->mLaunchCounter=101;p->mDoSpecialCountdown=0;}return true;
}
bool UpdateNativeCannon268(Plant* p)
{
    if(!IsNativeCannon268(p))return false;if(!Host(p)||!Live(p))return true;
    auto* d=GetNativePlantData(p);d->productionTicks=p->mStateCountdown;
    if(d->kind==915)
    {
        if(p->mLaunchCounter>0)--p->mLaunchCounter;
        if(p->mLaunchCounter==0)
        {
            p->mLaunchCounter=int(std::floor(double(1.5f*RandRangeFloat(.95f,1.05f))*100))+1;
            bool acquired=false;Zombie* target=nullptr;float sx=0,sy=0;p->GetShadowCenter(sx,sy);
            while(p->mBoard->IterateZombies(target))
            {float x=0,y=0;if(Normal(target)&&target->mRow==p->mRow&&target->GetShadowCenter(x,y)&&x>sx){acquired=true;break;}}
            if(acquired)TryGetFusionCannon268ShotCounter(*FindFusionCannon268SubTiming(),d->animationSpeed,p->mDoSpecialCountdown);
        }
    }
    if(d->batchMode==0&&p->mStateCountdown==0)
    {
        if(d->kind==1132){d->batchMode=2;p->mState=STATE_COBCANNON_READY;}
        else StartClock(p,true);
    }
    return true;
}
bool DispatchNativeCannon268Events(Plant* p)
{
    if(!IsNativeCannon268(p))return false;if(!Host(p)||!Live(p))return true;
    auto* d=GetNativePlantData(p);const bool charging=d->batchMode==1||(d->batchMode==2&&d->shootSeconds>=0);
    if(!charging&&d->batchMode!=3)return true;
    const auto* clock=FindFusionCannon268Timing(d->kind,charging);if(!clock)return true;
    for(size_t i=0;i<clock->eventClipSeconds.size();++i)
    {
        bool due=false;if(!TryGetFusionCannon268EventAtCounter(*clock,d->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
        if(charging){d->batchMode=2;p->mState=STATE_COBCANNON_READY;p->mApp->PlayFoley(FOLEY_SHOOP);}
        else Emit(p);
    }
    if(!charging&&d->kind!=1132)
    {
        const double soundEvent[]={1.2833333};auto sound=*clock;sound.eventClipSeconds=soundEvent;
        bool due=false;if(TryGetFusionCannon268EventAtCounter(sound,d->animationSpeed,p->mShootingCounter,0,due)&&due)p->mApp->PlayFoley(FOLEY_COB_LAUNCH);
    }
    if(!charging&&p->mShootingCounter==0){d->batchMode=0;d->shootSeconds=-1;p->mState=STATE_COBCANNON_ARMING;}
    if(charging&&p->mShootingCounter==0)d->shootSeconds=-1;
    return true;
}
void UpdateNativeCannon268Phase(Plant* p)
{
    if(!Host(p)||!Live(p))return;auto* d=GetNativePlantData(p);d->idleSeconds+=.01f;
    if(d->batchMode!=1&&d->batchMode!=3&&!(d->batchMode==2&&d->shootSeconds>=0))return;
    const auto* clock=FindFusionCannon268Timing(d->kind,d->batchMode!=3);int initial=0;
    if(clock&&TryGetFusionCannon268ShotCounter(*clock,d->animationSpeed,initial))d->shootSeconds=(initial-p->mShootingCounter)*.01f;
}
bool GetNativeCannon268VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!IsNativeCannon268(p))return false;const auto* d=GetNativePlantData(p);
    const bool charge=d->batchMode==1||(d->batchMode==2&&d->shootSeconds>=0);
    state=charge?"charge":d->batchMode==2?"idle2":d->batchMode==3?"shooting":"idle";
    seconds=charge||d->batchMode==3?d->shootSeconds:d->idleSeconds;return true;
}
bool IsNativeCannon268Projectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    return p&&p->mBoard&&IsNativeFusionBoard(p->mBoard)&&d&&IsFusionCannon268Way14(d->kind)&&
        d->specialType==14&&p->mProjectileType==PROJECTILE_COBBIG&&p->mMotionType==MOTION_LOBBED;
}
bool GetNativeCannon268ProjectileCenter(const Projectile* p,float& x,float& y)
{if(!IsNativeCannon268Projectile(p))return false;x=p->mPosX+20;y=p->mPosY+p->mPosZ+20;return true;}
bool MoveNativeCannon268Projectile(Projectile* p)
{
    if(!IsNativeCannon268Projectile(p))return false;if(!Host(p)||p->mDead)return true;
    if(p->mProjectileAge>=100&&GetNativeProjectileData(p)->effectVariant)
    {Split(p);if(p->mDead)return true;}
    float x=0,y=0;GetNativeCannon268ProjectileCenter(p,x,y);bool land=false;
    if(!TryStepFusionCannon268(GetNativeProjectileData(p)->kind,p->mProjectileAge,GetNativeFusionRowCount(p->mBoard),
        GetNativeFusionPixelsY(p->mBoard),p->mCobTargetX,p->mShadowY,x,y,p->mVelZ,p->mRotation,land))return true;
    if(land){Impact(p);return true;}
    p->mPosX=x-20;p->mPosZ=y-p->mShadowY;p->mX=int(p->mPosX);p->mY=int(p->mPosY+p->mPosZ);
    return true;
}
bool CanActivateNativeCannon268Money(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    return p&&d&&(d->kind==1139||d->kind==1142)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&
        CanApplyNativeFusionGameplay(p->mBoard)&&p->mStateCountdown==0&&d->batchRageTicks==0;
}
bool TryActivateNativeCannon268Money(Plant* p)
{
    if(!Host(p)||!CanActivateNativeCannon268Money(p))return false;
    auto* d=GetNativePlantData(p);const auto* clock=FindFusionCannon268LobberTiming(d->kind,d->kind==1139?"shoot3":"shoot2");
    int counter=0;if(!clock||!TryGetFusionShooterShotCounter(*clock,d->animationSpeed,counter))return false;
    p->mShootingCounter=counter;d->shootSeconds=0;d->batchMode=1;p->mStateCountdown=201;d->batchRageTicks=201;
    // Ordinary launch's captured aim cannot survive this source Animator trigger.
    GetNativeContext(p->mBoard)->lobberAims.erase(d->instanceToken);return true;
}
void UpdateNativeCannon268Money(Plant* p)
{
    if(!Host(p))return;auto* d=GetNativePlantData(p);
    if(!d||(d->kind!=1139&&d->kind!=1142))return;
    d->batchRageTicks=static_cast<uint16_t>((std::clamp)(p->mStateCountdown,0,201));
    if(d->batchMode&&p->mShootingCounter==0&&d->shootSeconds<0)d->batchMode=0;
}
bool FireNativeCannon268Money(Plant* p)
{
    auto* d=GetNativePlantData(p);
    if(!d||(d->kind!=1139&&d->kind!=1142)||!d->batchMode)return false;
    if(!Host(p)||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0)return true;
    float x=0,y=0,sx=0,sy=0;
    if(!p->GetShadowCenter(sx,sy)||!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return true;
    if(d->kind==1142)
    {
        if(SpawnCannon(p->mBoard,p->mRow,x,y,sx,sy,83,120,2))++d->batchShotCount;
        return true;
    }
    Zombie* z=nullptr;Zombie* nearest=nullptr;float nearestX=0;
    while(p->mBoard->IterateZombies(z))
    {
        float zx=0,zy=0;
        if(!Normal(z)||z->mRow!=p->mRow||!z->GetShadowCenter(zx,zy)||zx<=sx)continue;
        if(!nearest||zx<nearestX){nearest=z;nearestX=zx;}
    }
    if(!nearest)return true;
    const auto r=nearest->GetZombieRect();const double vx=nearest->IsImmobilizied()||nearest->mIsEating?0:nearest->mVelX;
    const FusionLobberPoint current{r.mX+r.mWidth*.5,double(r.mY)},sample{current.x+vx,current.y};
    FusionLobberLaunch launch;
    if(TryMakeFusionLobberLaunch({x,y},sample,current,.01,nullptr,launch,GetNativeFusionPixelsY(p->mBoard))&&
       EmitNativeLobberChild(p->mBoard,nearest->mRow,sy,launch,80,450,GetNativeDefenderMelonEnhanced(p)))++d->batchShotCount;
    return true;
}
void NativeCannon268LobberEmitted(Plant* p,Projectile* projectile)
{
    if(!Host(p)||!projectile)return;const auto* d=GetNativePlantData(p);auto* bullet=GetNativeProjectileData(projectile);
    if(!d||d->kind!=914||!bullet||bullet->kind!=72)return;
    // Source339E40 latches WithMelonPot only after this whole callback.
    struct Complete370{Plant* plant;~Complete370(){NativeTravelUltimate370MelonAfterShot(plant);}} complete370{p};
    bullet->effectVariant=GetNativeDefenderCornDenominator(p,4)==2?1:0;
    if(RandRangeInt(0,99)<42)p->mLaunchCounter=21;
    if(!NativeTravelUltimate370ExtraMelon(p,RandRangeInt(0,99)))return;
    float x=0,y=0,sx=0,sy=0;
    if(!p->GetShadowCenter(sx,sy)||!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return;
    Plant* receiver=nullptr;Plant* candidate=nullptr;
    while(p->mBoard->IteratePlants(candidate))
    {
        const auto* c=GetNativePlantData(candidate);
        if(!c||(c->kind!=1128&&c->kind!=916)||candidate->mDead||!candidate->mIsOnBoard||candidate->mPlantHealth<=0||
           candidate->mRow!=p->mRow||candidate->mPlantCol<p->mPlantCol)continue;
        if(!receiver||candidate->mPlantCol<receiver->mPlantCol)receiver=candidate;
    }
    std::vector<ZombieID> targets;Zombie* target=nullptr;
    while(p->mBoard->IterateZombies(target))
    {float zx=0,zy=0;if(Normal(target)&&target->mRow==p->mRow&&target->GetShadowCenter(zx,zy)&&zx>sx)targets.push_back(p->mBoard->ZombieGetID(target));}
    int emitted=0;
    for(auto id:targets)if((target=p->mBoard->ZombieTryToGet(id))&&Normal(target))
    {
        const auto r=target->GetZombieRect();FusionLobberPoint point{r.mX+r.mWidth*.5,double(r.mY)};
        float tx=0,ty=0,ux=0,uy=0;target->GetShadowCenter(tx,ty);
        if(receiver&&receiver->GetShadowCenter(ux,uy)&&ux<tx)point={ux,uy};
        FusionLobberLaunch launch;
        if(!TryMakeFusionLobberLaunch({x,y},point,point,1,nullptr,launch,GetNativeFusionPixelsY(p->mBoard)))continue;
        if(auto* child=EmitNativeLobberChild(p->mBoard,p->mRow,sy,launch,72,120,GetNativeDefenderMelonEnhanced(p)))
        {
            ++emitted;
            if(receiver)GetNativeContext(p->mBoard)->lobberPlantTargets[p->mBoard->mProjectiles.DataArrayGetID(child)]=p->mBoard->mPlants.DataArrayGetID(receiver);
        }
    }
    LawnNet::NetTrace("Fusion Cannon268 ExtraShoot plant=%u count=%d receiver=%u row=%d tick=%d",p->mNetId,emitted,receiver?receiver->mNetId:0,p->mRow,p->mBoard->mMainCounter);
}
void NativeCannon268LobberChildren(Projectile* p,Zombie* primary)
{
    const auto* d=GetNativeProjectileData(p);
    if(!Host(p)||!d||d->kind!=72||!primary||primary->mBoard!=p->mBoard)return;
    float x=0,y=0;if(!GetNativeLobberProjectileCenter(p,x,y))return;
    FusionLobberLaunch launch;launch.spawn={x,y-.3*GetNativeFusionPixelsY(p->mBoard)};launch.accelerationY=p->mAccZ;
    const float baseline=p->mPosY+20;
    launch.velocityX=p->mVelX*.5;launch.velocityY=-std::abs(p->mVelZ)*.5;
    EmitNativeLobberChild(p->mBoard,primary->mRow,baseline,launch,63,40,d->melonEnhanced);
    launch.velocityX=p->mVelX*.4;launch.velocityY=-std::abs(p->mVelZ)*.4;
    EmitNativeLobberChild(p->mBoard,primary->mRow,baseline,launch,64,80,d->melonEnhanced);
    const bool butter=RandRangeInt(0,d->effectVariant?1:3)==1;
    launch.velocityX=p->mVelX*.3;launch.velocityY=-std::abs(p->mVelZ)*.3;
    EmitNativeLobberChild(p->mBoard,primary->mRow,baseline,launch,butter?62:61,butter?40:20,d->melonEnhanced);
    LawnNet::NetTrace("Fusion Cannon268 UltimateMelon split projectile=%u payloads=63,64,%d p=%d tick=%d",p->mNetId,butter?62:61,d->effectVariant?2:4,p->mBoard->mMainCounter);
}
bool ApplyNativeCannon268SplashDamage(Projectile* p,Zombie* z,int distributedDamage)
{
    const auto* d=GetNativeProjectileData(p);if(!d||d->kind!=72)return false;
    if(!Host(p)||p->mDead||!Normal(z)||z->mBoard!=p->mBoard)return true;
    int damage=0;
    if(TryGetFusionPoisonLobberDamage(64,distributedDamage,GetNativePoisonLevel(z),true,damage))
    {z->TakeDamage(damage,2U);AddNativePoisonLevel(z);}
    return true;
}
bool ApplyNativeCannon268LittleGoldImpact(Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    if(!d||d->kind!=80)return false;if(!Host(p)||p->mDead)return true;
    float x=0,y=0;if(!GetNativeLobberProjectileCenter(p,x,y))return true;
    Zombie* z=nullptr;std::vector<ZombieID> targets;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Normal(z)||std::abs(z->mRow-p->mRow)>1)continue;const auto r=z->GetZombieRect();
        if(FusionCannon268AreaTouches(x,y,r.mX,r.mY,r.mWidth,r.mHeight,GetNativeFusionPixelsY(p->mBoard)))targets.push_back(p->mBoard->ZombieGetID(z));
    }
    for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id))&&Normal(z))z->TakeDamage(450,18U);
    LawnNet::NetTrace("Fusion Cannon268 LittleGold impact projectile=%u count=%zu damage=450 tick=%d",p->mNetId,targets.size(),p->mBoard->mMainCounter);
    p->Die();return true;
}
bool UpdateNativeCannon268LittleGoldRotation(Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);if(!d||d->kind!=80)return false;
    if(!Host(p)||p->mDead)return true;
    constexpr double pi=3.14159265358979323846;
    const double vx=p->mVelX/kFusionLobberPixelsX,vy=-p->mVelZ/GetNativeFusionPixelsY(p->mBoard);
    if(vx!=0||vy!=0)
    {
        const double desired=std::atan2(vy,vx)-pi*.5;
        p->mRotation+=float(std::remainder(desired-p->mRotation,2*pi)*.15);
    }
    return true;
}
bool DispatchNativeCannon268SubEvents(Plant* p)
{
    const auto* current=GetNativePlantData(p);if(!IsNativeCannon268(p)||!current||current->kind!=915)return false;
    if(!Host(p)||!Live(p))return true;auto* d=GetNativePlantData(p);d->pendingProductionTicks=p->mDoSpecialCountdown;
    if(p->mDoSpecialCountdown<=0)return true;const auto* clock=FindFusionCannon268SubTiming();
    for(size_t i=0;i<clock->eventClipSeconds.size();++i)
    {
        bool due=false;if(!TryGetFusionCannon268EventAtCounter(*clock,d->animationSpeed,p->mDoSpecialCountdown,i,due)||!due)continue;
        float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"Shoot",x,y)||!EnsureNativeProjectileVisuals(p->mBoard,85)||p->mBoard->mProjectiles.mSize>=p->mBoard->mProjectiles.mMaxSize)continue;
        auto* shot=p->mBoard->AddProjectile(int(x-20),int(y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,p->mRow,0),p->mRow,PROJECTILE_KERNEL);
        if(!shot)continue;NativeProjectileData bullet;bullet.kind=85;bullet.damage=360;
        shot->mMotionType=MOTION_STRAIGHT;
        if(!AttachNativeProjectile(p->mBoard,shot,bullet)){shot->Die();continue;}
        shot->mPosX=x-20;shot->mPosY=y-20;shot->mPosZ=0;shot->mVelX=float(6*.01*kFusionLobberPixelsX);shot->mVelY=shot->mVelZ=shot->mAccZ=0;
        shot->mShadowY=y;shot->mRotation=shot->mRotationSpeed=0;++d->batchShotCount;
        LawnNet::NetTrace("Fusion Cannon268 SubShoot plant=%u projectile=%u payload=85 damage=360 tick=%d",p->mNetId,shot->mNetId,p->mBoard->mMainCounter);
    }
    return true;
}
bool GetNativeCannon268SubVisualPose(const Plant* p,const char*& state,float& seconds)
{
    const auto* d=GetNativePlantData(p);if(!IsNativeCannon268(p)||!d||d->kind!=915||d->pendingProductionTicks<=0)return false;
    int initial=0;if(!TryGetFusionCannon268ShotCounter(*FindFusionCannon268SubTiming(),d->animationSpeed,initial))return false;
    state="subshoot";seconds=(initial-d->pendingProductionTicks)*.01f;return true;
}
bool IsNativeCannon268Kernel(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);return p&&p->mBoard&&IsNativeFusionBoard(p->mBoard)&&d&&d->kind==85&&
        p->mProjectileType==PROJECTILE_KERNEL&&p->mMotionType==MOTION_STRAIGHT;
}
bool GetNativeCannon268KernelRect(const Projectile* p,Sexy::Rect& rect)
{
    if(!IsNativeCannon268Kernel(p))return false;
    const double py=GetNativeFusionPixelsY(p->mBoard),w=1.2586598*.4*kFusionLobberPixelsX,h=.51232433*.4*py;
    const double cx=p->mPosX+20-.012087107*.4*kFusionLobberPixelsX,cy=p->mPosY+20+.022978932*.4*py;
    const int left=int(std::floor(cx-w*.5)),top=int(std::floor(cy-h*.5));
    rect={left,top,int(std::ceil(cx+w*.5))-left,int(std::ceil(cy+h*.5))-top};return true;
}
bool MoveNativeCannon268Kernel(Projectile* p)
{
    if(!IsNativeCannon268Kernel(p))return false;if(!Host(p)||p->mDead)return true;
    p->mPosX+=p->mVelX;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    if(p->mPosX>p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+2*kFusionLobberPixelsX)p->Die();
    return true;
}
bool ApplyNativeCannon268KernelImpact(Projectile* p,Zombie* z)
{
    if(!IsNativeCannon268Kernel(p))return false;if(!Host(p)||p->mDead)return true;
    if(!Normal(z)||z->mBoard!=p->mBoard||z->mRow!=p->mRow)return true;
    int damage=0;TryGetFusionLobberColdDamage(GetNativeProjectileData(p)->damage,z->mIceTrapCounter>0,damage);
    const FusionColdDamageContext cold{85,FusionColdDamageDelivery::Ice};
    z->TakeDamage(damage,2U,&cold);AddNativeColdLevel(z,10);p->Die();return true;
}

}
