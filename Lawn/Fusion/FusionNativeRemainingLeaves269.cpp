#include "FusionNativeRemainingLeaves269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeWater268.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeMoney.h"
#include "FusionNativePlacement.h"
#include "FusionBasePlantRules.h"
#include "FusionSummon269Rules.h"
#include "FusionFireActionBatchRules.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* o)
{return o&&o->mBoard&&o->mApp&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&IsNativeFusionBoard(o->mBoard);}
bool Playing(const GameObject* o){return Host(o)&&CanApplyNativeFusionGameplay(o->mBoard);}
bool Active(const Board* b,int slot)
{const auto* ctx=GetNativeContext(b);if(!ctx||slot<0||slot>=4||!(ctx->selection.State().activeMask&(1U<<slot)))return false;
 if(!b->mApp->NetActive())return slot==0;const auto& player=b->mApp->mNetManager->GetPlayer(slot);return player.mActive&&!player.mSpectator;}
bool Eligible(Zombie* z,bool retain)
{return z&&!z->mDead&&FusionRemainingLeaves269TrackTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,z->mBodyHealth>0,!NativeWater268ZombieCaptured(z),retain);}
Zombie* Nearest(Projectile* p)
{
    Zombie* chosen=nullptr;Zombie* z=nullptr;bool air=false;double best=0;const auto py=GetNativeFusionPixelsY(p->mBoard);
    const double maxX=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX;
    while(p->mBoard->IterateZombies(z))
    {const bool flying=z->IsFlying();if(!z||z->mDead||z->mMindControlled||z->mBodyHealth<=0||NativeWater268ZombieCaptured(z)||
      (!flying&&!Eligible(z,false))||(air&&!flying))continue;float x=0,y=0;if(!z->GetShadowCenter(x,y)||x>=maxX)continue;
     const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=(r.mY+r.mHeight*.5-p->mPosY-20)/py,dist=dx*dx+dy*dy;
     if(!chosen||(flying&&!air)||dist<best){chosen=z;best=dist;air=flying;}}
    return chosen;
}
Projectile* Emit(Plant* p,uint16_t kind,uint8_t way,int damage,const char* path,double angle)
{
    auto* b=p->mBoard;float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,path,x,y)||!EnsureNativeProjectileVisuals(b,kind)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    auto* q=b->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,p->mRow,PROJECTILE_PEA);if(!q)return nullptr;
    NativeProjectileData d;d.kind=kind;d.damage=damage;d.specialType=way;d.effectVariant=kind==8?3:0;q->mMotionType=kind==8?MOTION_HOMING:MOTION_STAR;
    if(!AttachNativeProjectile(b,q,d)){q->Die();return nullptr;}
    q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=q->mVelZ=q->mAccZ=0;q->mShadowY=y;q->mRotation=float(angle);q->mRotationSpeed=0;
    q->mVelX=float(std::cos(angle)*(kind==8?5:6)*kFusionBasePixelsX*.01);q->mVelY=float(-std::sin(angle)*(kind==8?5:6)*GetNativeFusionPixelsY(b)*.01);
    q->mTargetZombieID=ZOMBIEID_NULL;q->mX=int(q->mPosX);q->mY=int(q->mPosY);return q;
}
void EndoShoot(Plant* p)
{
    std::array<bool,100> present{};Plant* q=nullptr;while(p->mBoard->IteratePlants(q))if(const auto* d=GetNativePlantData(q))if(!q->mDead&&d->kind>=900&&d->kind<1000)present[d->kind-900]=true;
    int count=0;for(int i=0;i<100&&FusionFindPlant(900+i);++i)if(present[i])++count;int damage=0;if(!TryGetFusionEndo269Damage(count,damage))return;
    constexpr const char* paths[]={"Weapon/right1","Weapon/right2","Weapon/right3","Weapon/right4","Weapon/left1","Weapon/left2","Weapon/left3","Weapon/left4"};
    for(auto* path:paths)Emit(p,8,16,damage,path,0);++GetNativePlantData(p)->batchShotCount;
}
bool SwordArea(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;y-=float(.3*GetNativeFusionPixelsY(p->mBoard));
    std::vector<ZombieID> hits;Zombie* z=nullptr;const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
    while(p->mBoard->IterateZombies(z))if(!z->mDead&&!NativeWater268ZombieCaptured(z)&&FusionPuffLove269CanSplash(GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,false))
    {const auto r=z->GetZombieRect();if(FusionFireCircleTouchesRect(x,y,2,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio))hits.push_back(p->mBoard->ZombieGetID(z));}
    for(auto id:hits)if((z=p->mBoard->ZombieTryToGet(id)))ApplyNativeSourceExplosionDamage(z,30);return !hits.empty();
}
bool PlantHasTarget(Plant* p)
{
    float x=0,y=0;p->GetShadowCenter(x,y);const double py=GetNativeFusionPixelsY(p->mBoard);const bool sword=GetNativePlantData(p)->kind==249;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {if(z->mDead||NativeWater268ZombieCaptured(z)||!FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),z->mMindControlled,z->IsDeadOrDying()))continue;
     if(!sword)return true;const auto r=z->GetZombieRect();const double ratio=kFusionBasePixelsY/py;
     if(FusionFireCircleTouchesRect(x,y-.3*kFusionBasePixelsY,2,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio))return true;
     for(unsigned i=0;i<5;++i){double distance=0;if(TryGetFusionStarAcquisitionDistance(i,z->mRow==p->mRow,x,y-.3*kFusionBasePixelsY,
       {double(r.mX),y+(r.mY-y)*ratio,double(r.mWidth),r.mHeight*ratio},distance))return true;}}
    return false;
}
void SwordShoot(Plant* p)
{constexpr const char* paths[]={"Shoot1","Shoot2","Shoot3","Shoot4","Shoot5"};constexpr double angles[]={1.5707963267948966,-1.5707963267948966,3.141592653589793, .5235987755982988,-.5235987755982988};
 for(unsigned i=0;i<5;++i)Emit(p,51,i==2?9:2,200,paths[i],angles[i]);++GetNativePlantData(p)->batchShotCount;}
Plant* FertilizeTarget(Coin* c)
{const auto* d=GetNativeFertilize269Data(c);if(!d||!d->targetId||!d->targetToken)return nullptr;Plant* p=nullptr;
 while(c->mBoard->IteratePlants(p))if(!p->mDead&&p->mNetId==d->targetId)if(const auto* pd=GetNativePlantData(p))if(pd->instanceToken==d->targetToken)return p;return nullptr;}
void Upgrade(Coin* c)
{
    auto* target=FertilizeTarget(c);if(!target){c->Die();return;}auto* b=c->mBoard;const int col=target->mPlantCol,row=target->mRow;
    std::vector<uint32_t> ids;Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==col&&p->mRow==row&&GetNativePlantData(p))ids.push_back(b->mPlants.DataArrayGetID(p));
    for(auto id:ids)if((p=b->mPlants.DataArrayTryToGet(id))&&!p->mDead)
    {const auto kind=GetNativePlantData(p)->kind;RecoverNativeCompanions269Plant(p,p->mPlantMaxHealth);ReduceNativeFertilize269Attribute(p,50);
     bool covered=false;if(kind==12){Plant* q=nullptr;while(b->IteratePlants(q))if(q!=p&&!q->mDead&&q->mPlantCol==col&&q->mRow==row)
       {bool air=false,pumpkin=false,pot=false,lily=false;if(GetNativeRemaining269PlantTags(q,air,pumpkin,pot,lily)&&!pumpkin&&!air)covered=true;}}
     if(kind==1039&&p->mLaunchCounter>50)p->mLaunchCounter=50;uint16_t next=0;
     if(!TryGetFusionFertilize269Upgrade(kind,covered,next)||GetNativeSeedForPlant(next)==SEED_NONE||!EnsureNativePlantVisuals(b,next))continue;
     NativePlantData fresh;fresh.kind=next;fresh.owner=LawnNet::NET_SHARED_OWNER;
     auto* made=b->AddPlant(col,row,GetNativeSeedForPlant(next),SEED_NONE,&fresh);if(made&&!made->mDead&&GetNativePlantData(made))p->Die(2);else if(made)made->Die(255);
    }
    c->Die();
}
}
bool IsNativeRemainingLeaves269Plant(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&FindFusionRemainingLeaves269Timing(d->kind);}
bool InitializeNativeRemainingLeaves269Plant(Plant* p)
{if(!IsNativeRemainingLeaves269Plant(p)||!Host(p))return false;const auto* stats=FusionFindPlant(GetNativePlantData(p)->kind);p->mPlantHealth=p->mPlantMaxHealth=stats->maxHealth;
 p->mStateCountdown=p->mDoSpecialCountdown=p->mShootingCounter=0;GetNativePlantData(p)->batchShotCount=0;return InitializeNativeShooterTiming(p,*FindFusionRemainingLeaves269Timing(stats->id),true);}
bool UpdateNativeRemainingLeaves269Plant(Plant* p)
{if(!IsNativeRemainingLeaves269Plant(p))return false;if(!Playing(p)||p->mDead)return true;auto* d=GetNativePlantData(p);const auto* t=FindFusionRemainingLeaves269Timing(d->kind);
 if(--p->mLaunchCounter<=0){ResetNativeShooterLaunchCounter(p,*t);if(d->shootSeconds<0&&PlantHasTarget(p))BeginNativeShooterShot(p,*t);}return true;}
void UpdateNativeRemainingLeaves269Phase(Plant* p)
{if(IsNativeRemainingLeaves269Plant(p)&&Playing(p)&&!p->mDead)UpdateNativeShooterPhase(p,*FindFusionRemainingLeaves269Timing(GetNativePlantData(p)->kind));}
bool DispatchNativeRemainingLeaves269Events(Plant* p)
{if(!IsNativeRemainingLeaves269Plant(p))return false;if(!Playing(p)||p->mDead)return true;const auto* t=FindFusionRemainingLeaves269Timing(GetNativePlantData(p)->kind);
 for(unsigned i=0;i<t->eventClipSeconds.size();++i){bool due=false;if(!TryGetFusionShooterEventAtCounter(*t,GetNativePlantData(p)->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
 if(t->kind==242)EndoShoot(p);else if(FusionSword269AttackEvent(i)){if(SwordArea(p))p->mApp->PlayFoley(FOLEY_CHERRYBOMB);}else SwordShoot(p);}return true;}
bool GetNativeRemainingLeaves269VisualPose(const Plant* p,const char*& state,float& seconds)
{if(!IsNativeRemainingLeaves269Plant(p))return false;const auto* d=GetNativePlantData(p);state=d->shootSeconds>=0?"shoot":"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return true;}
bool IsNativeRemainingLeaves269Projectile(const Projectile* p)
{const auto* d=GetNativeProjectileData(p);return d&&FusionRemainingLeaves269ProjectileValid(d->kind,d->damage,d->specialType,d->effectVariant,p->mProjectileType,p->mMotionType);}
bool GetNativeRemainingLeaves269ProjectileRect(const Projectile* p,Sexy::Rect& out)
{if(!IsNativeRemainingLeaves269Projectile(p))return false;const auto* d=GetNativeProjectileData(p);const double c=std::cos(p->mRotation),s=std::sin(p->mRotation),py=GetNativeFusionPixelsY(p->mBoard);
 const double offset=d->kind==51?2.5235982*.3:0,hw=d->kind==51?3.8728037*.3*.5:.2,hh=d->kind==51?.5446532*.4*.5:.2;
 const double x=p->mPosX+20+offset*c*kFusionBasePixelsX,y=p->mPosY+20-offset*s*py,rx=(hw*std::abs(c)+hh*std::abs(s))*kFusionBasePixelsX,ry=(hw*std::abs(s)+hh*std::abs(c))*py;
 const int left=int(std::floor(x-rx)),top=int(std::floor(y-ry));out=Sexy::Rect(left,top,int(std::ceil(x+rx))-left,int(std::ceil(y+ry))-top);return true;}
bool MoveNativeRemainingLeaves269Projectile(Projectile* p)
{
    if(!IsNativeRemainingLeaves269Projectile(p))return false;if(!Playing(p)||p->mDead)return true;const auto* d=GetNativeProjectileData(p);const double py=GetNativeFusionPixelsY(p->mBoard);
    if(d->kind==8)
    {auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);if(!Eligible(z,true)){const bool empty=p->mTargetZombieID==ZOMBIEID_NULL;z=Nearest(p);p->mTargetZombieID=z?p->mBoard->ZombieGetID(z):ZOMBIEID_NULL;
      if(empty){p->mVelX=float(std::cos(p->mRotation)*5*kFusionBasePixelsX*.01);p->mVelY=float(-std::sin(p->mRotation)*5*py*.01);}}
     else{const auto r=z->GetZombieRect();const double dx=(r.mX+r.mWidth*.5-p->mPosX-20)/kFusionBasePixelsX,dy=-(r.mY+r.mHeight*.5-p->mPosY-20)/py,length=std::hypot(dx,dy);
      if(length>1e-8){p->mVelX=float(dx/length*5*kFusionBasePixelsX*.01);p->mVelY=float(-dy/length*5*py*.01);p->mRotation=float(std::atan2(dy,dx));}}
     p->mRotationSpeed=std::fmod(p->mRotationSpeed+float((p->mVelX>0?-1:1)*200*.01*3.141592653589793/180),float(2*3.141592653589793));}
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    const double maxX=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX;
    const double minX=p->mBoard->GetCellCenterX(0,0)+(-10+4.8)*kFusionBasePixelsX;
    const double top=p->mBoard->GridToPixelY(0,0)+50+(2.3-6.5)*py,bottom=p->mBoard->GridToPixelY(0,GetNativeFusionRowCount(p->mBoard)-1)+50+.6*py;
    if(p->mPosX+20<minX||p->mPosX+20>maxX||p->mPosY+20<top||p->mPosY+20>bottom)p->Die();return true;
}
bool ApplyNativeRemainingLeaves269Impact(Projectile* p,Zombie* z)
{
    if(!IsNativeRemainingLeaves269Projectile(p)||!Playing(p)||p->mDead||!z||z->mDead)return false;const auto d=*GetNativeProjectileData(p);
    if(!FusionRemainingLeaves269CanHit(d.kind,d.specialType,GetNativeFusionZombieStatus(z),z->mRow==p->mRow,z->mMindControlled,!NativeWater268ZombieCaptured(z)))return false;
    if(d.kind==8)
    {z->TakeDamage(d.damage,0U);p->PlayImpactSound(z);p->Die();return true;}
    auto* ctx=GetNativeContext(p->mBoard);if(!ctx||!RememberFusionRollingHit(ctx->rollingHits,p->mBoard->mProjectiles.DataArrayGetID(p),p->mBoard->ZombieGetID(z)))return true;
    z->TakeDamage(d.damage,1U);p->PlayImpactSound(z);return true;
}
bool HandleNativeRemainingLeaves269Collision(Projectile* p)
{if(!IsNativeRemainingLeaves269Projectile(p))return false;if(!Playing(p)||p->mDead)return true;const auto* d=GetNativeProjectileData(p);Zombie* z=nullptr;
 while(p->mBoard->IterateZombies(z))if(!z->mDead)
 {if(d->kind==8&&p->mBoard->ZombieTryToGet(p->mTargetZombieID)!=z)continue;const auto r=z->GetZombieRect();
  if(FusionRemainingLeaves269Touches(d->kind,p->mPosX+20,p->mPosY+20,p->mRotation,GetNativeFusionPixelsY(p->mBoard),{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)}))ApplyNativeRemainingLeaves269Impact(p,z);if(p->mDead)break;}return true;}
void NativeRemainingLeaves269ProjectileRemoving(Projectile* p)
{if(Host(p))if(auto* ctx=GetNativeContext(p->mBoard))ctx->rollingHits.erase(p->mBoard->mProjectiles.DataArrayGetID(p));}
bool IsNativeFertilize269(const Coin* c){return c&&GetNativeFertilize269Data(c);}
bool CreateNativeRemaining269Fertilize(Plant* p,float x,float y)
{
    if(!Host(p)||!std::isfinite(x+y)||!EnsureNativeFertilize269Visuals(p->mBoard)||p->mBoard->mCoins.mSize>=p->mBoard->mCoins.mMaxSize)return false;auto* b=p->mBoard;auto* c=b->mCoins.DataArrayAlloc();c->mApp=b->mApp;c->mBoard=b;
    c->CoinInitializeNetGhost(int(x-20),int(y-20),COIN_NONE,COIN_MOTION_COIN,nullptr);c->mNetGhost=false;c->mNetId=b->NetAllocId();if(!c->mNetId){b->mCoins.DataArrayFree(c);return false;}
    auto* d=EditNativeFertilize269Data(c,true);if(!d){b->mCoins.DataArrayFree(c);return false;}*d={};d->instanceToken=b->mCoins.DataArrayGetID(c);d->anchorX=x;d->anchorY=y;
    c->mPosX=x-20;c->mPosY=y-20;c->mX=c->mY=0;c->mCoinAge=c->mDisappearCounter=0;c->mVelX=RandRangeFloat(-1.5f,1.5f)*float(kFusionBasePixelsX*.01);c->mVelY=-4.f*float(GetNativeFusionPixelsY(b)*.01);return true;
}
bool PickNativeFertilize269(Coin* c,int slot)
{auto* d=EditNativeFertilize269Data(c);if(!d||!Playing(c)||c->mDead||!Active(c->mBoard,slot)||d->heldSlot!=255||d->phase==NativeFertilize269Phase::Using)return false;
 Coin* other=nullptr;while(c->mBoard->IterateCoins(other))if(other!=c&&!other->mDead)if(const auto* od=GetNativeFertilize269Data(other))if(od->heldSlot==slot)return false;
 d->heldSlot=uint8_t(slot);d->phase=NativeFertilize269Phase::Cursor;d->phaseStartAge=c->mCoinAge;return true;}
bool UseNativeFertilize269(Coin* c,int slot,int col,int row)
{auto* d=EditNativeFertilize269Data(c);if(!d||!Playing(c)||!Active(c->mBoard,slot)||d->heldSlot!=slot||d->phase!=NativeFertilize269Phase::Cursor||!c->mBoard->IsValidGridCell(col,row))return false;
 Plant* p=nullptr;while(c->mBoard->IteratePlants(p))if(!p->mDead&&GetNativePlantData(p)&&p->mPlantCol==col&&p->mRow==row)
 {float x=0,y=0;if(!p->GetShadowCenter(x,y))continue;d->targetId=p->mNetId;d->targetToken=GetNativePlantData(p)->instanceToken;d->phase=NativeFertilize269Phase::Using;d->phaseStartAge=c->mCoinAge;d->heldSlot=255;
  c->mPosX=x-20;c->mPosY=y-float(1.5*GetNativeFusionPixelsY(c->mBoard))-20;return true;}return false;}
bool PutDownNativeFertilize269(Coin* c,int slot)
{auto* d=EditNativeFertilize269Data(c);if(!d||!Host(c)||c->mDead||d->phase!=NativeFertilize269Phase::Cursor||d->heldSlot!=slot)return false;
 d->heldSlot=255;d->phase=NativeFertilize269Phase::World;d->phaseStartAge=c->mCoinAge;c->mPosX=d->anchorX-20;c->mPosY=d->anchorY-20;return true;}
bool UpdateNativeFertilize269(Coin* c)
{
    if(!IsNativeFertilize269(c))return false;if(!Playing(c)||c->mDead)return true;auto* d=EditNativeFertilize269Data(c);++c->mCoinAge;
    if(d->phase==NativeFertilize269Phase::Falling)
    {const double py=GetNativeFusionPixelsY(c->mBoard);c->mVelY+=float(9.8*py*.0001);c->mPosX+=c->mVelX;c->mPosY+=c->mVelY;
     const double floor=c->mBoard->GridToPixelY(0,GetNativeFusionRowCount(c->mBoard)-1)+50-1.4*py;
     if(c->mPosY+20>d->anchorY+py||c->mPosY+20>floor){c->mPosY=float((std::min)(double(c->mPosY+20),floor)-20);d->anchorX=c->mPosX+20;d->anchorY=c->mPosY+20;d->phase=NativeFertilize269Phase::World;d->phaseStartAge=c->mCoinAge;}}
    if(d->phase==NativeFertilize269Phase::Using){if(c->mCoinAge-d->phaseStartAge>=int(std::ceil(double(.9166667f)*100)))Upgrade(c);}
    else if(d->phase!=NativeFertilize269Phase::Cursor&&++c->mDisappearCounter>3000)c->Die();return true;
}
void NativeFertilize269PlantRemoving(Plant* p)
{if(!Host(p)||!GetNativePlantData(p))return;Coin* c=nullptr;while(p->mBoard->IterateCoins(c))if(auto* d=EditNativeFertilize269Data(c))if(d->targetId==p->mNetId&&d->targetToken==GetNativePlantData(p)->instanceToken){d->targetId=0;d->targetToken=0;}}
bool GetNativeFertilize269VisualPose(const Coin* c,const char*& state,float& seconds)
{const auto* d=GetNativeFertilize269Data(c);if(!d)return false;state=d->phase==NativeFertilize269Phase::Using?"use":d->phase==NativeFertilize269Phase::Cursor?"static":"idle";seconds=(c->mCoinAge-d->phaseStartAge)*.01f;return true;}
}
