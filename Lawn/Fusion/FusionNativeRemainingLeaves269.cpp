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
#include "FusionNativeEffects269.h"
#include "FusionNativeGlove.h"
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
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
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
struct FertilizeRecipient269
{uint32_t uid;uint16_t id,kind,next;};
bool FertilizePlan269(Coin* coin,Plant* target,std::vector<FertilizeRecipient269>& plan,std::string* error)
{
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    if(error)error->clear();auto* b=coin?coin->mBoard:nullptr;
    if(!Host(coin)||!target||target->mBoard!=b||target->mDead||!target->mIsOnBoard||
        target->mPlantHealth<=0||!GetNativePlantData(target)||!EnsureNativeSourceEffect269(b,16,error))
        return reject("Fertilize target or genuine source Recover16 resource unavailable.");
    int bodies=0,reanimations=0,attachments=0;Plant* p=nullptr;
    while(b->IteratePlants(p))if(p->mPlantCol==target->mPlantCol&&p->mRow==target->mRow)
    {
        const auto* d=GetNativePlantData(p);const auto uid=b->mPlants.DataArrayGetID(p);
        if(!d||d->instanceToken!=uid||!p->mNetId||p->mNetGhost||!p->mIsOnBoard||p->mPlantHealth<=0||
            p->mSeedType!=GetNativeSeedForPlant(d->kind)||IsNativeGlovePlantReserved(b,p,-1))
            return reject("Fertilize group has a foreign generation or active glove lease.");
        bool covered=false;
        if(d->kind==12)
        {
            Plant* q=nullptr;while(b->IteratePlants(q))if(q!=p&&q->mPlantCol==p->mPlantCol&&q->mRow==p->mRow)
            {bool air=false,pumpkin=false,pot=false,lily=false;
             if(GetNativeRemaining269PlantTags(q,air,pumpkin,pot,lily)&&!pumpkin&&!air)covered=true;}
        }
        uint16_t next=0;TryGetFusionFertilize269Upgrade(d->kind,covered,next);
        plan.push_back({uid,p->mNetId,d->kind,next});if(!next)continue;
        const auto seed=GetNativeSeedForPlant(next);const auto* stats=FusionFindPlant(next);
        // Exact ordinary native initializers used by the source upgrade table.
        // New table additions cannot silently borrow this resource budget.
        if((seed!=SEED_TALLNUT&&seed!=SEED_GLOOMSHROOM&&seed!=SEED_CATTAIL&&seed!=SEED_SPIKEROCK&&
            seed!=SEED_COBCANNON&&seed!=SEED_THREEPEATER)||!stats||!stats->hasStats||
            !EnsureNativePlantVisuals(b,next,error))return reject("Unbudgeted Fertilize replacement initializer.");
        const auto type=GetPlantDefinition(seed).mReanimationType;
        if(type==REANIM_NONE)return reject("Fertilize replacement native definition missing.");
        ReanimatorEnsureDefinitionLoaded(type,false);
        if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return reject("Fertilize native definition unavailable.");
        const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
        if(!tracks.tracks||tracks.count<=0)return reject("Fertilize native definition has no tracks.");
        bool ground=false;
        for(int i=0;i<tracks.count;++i)if(const char* name=tracks.tracks[i].mName)
        {if(strnicmp(name,"attacher__",10)==0)return reject("Fertilize native definition has an unbudgeted dynamic attacher.");
         if(!std::strcmp(name,"_ground"))ground=true;}
        ++bodies;++reanimations;
        if(seed==SEED_THREEPEATER){reanimations+=3;attachments+=3;}
        if(Plant::IsNocturnal(seed)&&!b->StageIsNight())
        {
            ReanimatorEnsureDefinitionLoaded(REANIM_SLEEPING,false);
            if(unsigned(REANIM_SLEEPING)>=gReanimatorDefCount)return reject("Fertilize sleeping definition unavailable.");
            const auto& sleep=gReanimatorDefArray[int(REANIM_SLEEPING)].mTracks;
            if(!sleep.tracks||sleep.count<=0)return reject("Fertilize sleeping definition has no tracks.");
            for(int i=0;i<sleep.count;++i)if(sleep.tracks[i].mName&&strnicmp(sleep.tracks[i].mName,"attacher__",10)==0)
                return reject("Fertilize sleeping definition has an unbudgeted dynamic attacher.");
            ++reanimations; // SetSleeping allocates one R, no Attachment.
        }
        if(!b->mApp->GetDialog(DIALOG_ALMANAC)&&b->mApp->IsWallnutBowlingLevel()&&ground)
        {++reanimations;++attachments;}
    }
    if(plan.empty())return reject("Fertilize has no actual cell recipients.");
    auto* fx=b->mApp->mEffectSystem;
    if(!fx||fx!=gEffectSystem||!fx->mReanimationHolder||!fx->mAttachmentHolder||!fx->mParticleHolder)
        return reject("Fertilize native pools unavailable.");
    const auto& plants=b->mPlants;const auto& r=fx->mReanimationHolder->mReanimations;const auto& a=fx->mAttachmentHolder->mAttachments;
    if(!plants.mBlock||!r.mBlock||!a.mBlock||plants.mSize>plants.mMaxSize||r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||
        bodies>plants.mMaxSize-plants.mSize||reanimations>r.mMaxSize-r.mSize||attachments>a.mMaxSize-a.mSize)
        return reject("Fertilize whole replacement peak unavailable; no dead credit.");
    if(bodies)
    {
        const auto effect=b->IsPoolSquare(target->mPlantCol,target->mRow)?PARTICLE_PLANTING_POOL:PARTICLE_PLANTING;
        if(!gParticleDefArray||int(effect)>=gParticleDefCount)return reject("Fertilize planting definition unavailable.");
        const auto& def=gParticleDefArray[int(effect)];const auto& ps=fx->mParticleHolder->mParticleSystems;const auto& em=fx->mParticleHolder->mEmitters;
        // Genuine Health16 has one PS/emitter per Recover. Reserve it together
        // with planting so healing cannot steal the later factory's PS peak.
        // Heal-only groups retain cosmetic-drop semantics under pool pressure.
        if(!def.mEmitterDefs||def.mEmitterDefCount<=0||!ps.mBlock||!em.mBlock||ps.mSize>ps.mMaxSize||em.mSize>em.mMaxSize||
            plan.size()+size_t(bodies)>size_t(ps.mMaxSize-ps.mSize)||
            plan.size()+size_t(bodies)*size_t(def.mEmitterDefCount)>size_t(em.mMaxSize-em.mSize))
            return reject("Fertilize whole planting particle peak unavailable.");
    }
    return true;
}
void Upgrade(Coin* c)
{
    auto* target=FertilizeTarget(c);if(!target){c->Die();return;}auto* b=c->mBoard;const int col=target->mPlantCol,row=target->mRow;
    std::vector<FertilizeRecipient269> plan;std::string error;
    if(!FertilizePlan269(c,target,plan,&error))
    {if(c->mCoinAge%100==0)LawnNet::NetTrace("Fusion Fertilize Upgrade deferred id=%u reason=%s",c->mNetId,error.c_str());return;}
    for(const auto& actor:plan)
    {
        auto* p=b->mPlants.DataArrayTryToGet(actor.uid);const auto* old=GetNativePlantData(p);
        if(!p||p->mDead||!old||p->mNetId!=actor.id||old->instanceToken!=actor.uid||old->kind!=actor.kind)
        {
            if(auto* ctx=GetNativeContext(b);ctx&&ctx->adventure.enabled)
            {ctx->adventure.schedulerFailed=true;ctx->adventureScheduler.failed=true;}
            LawnNet::NetTrace("Fusion Fertilize invariant FAILED after preflight coin=%u old=%u",c->mNetId,actor.id);
            c->Die();return;
        }
        RecoverNativeCompanions269Plant(p,p->mPlantMaxHealth);ReduceNativeFertilize269Attribute(p,50);
        if(actor.kind==1039&&p->mLaunchCounter>50)p->mLaunchCounter=50;
        if(!actor.next)continue;
        NativePlantData fresh;fresh.kind=actor.next;fresh.owner=LawnNet::NET_SHARED_OWNER;
        // Source freeSet upgrade group removes the old generation FIRST.
        // Corn28's non-free SetPlant1120 path is still the legacy native
        // Add-before-Die conversion; its shared CheckBox/CheckMix parity is
        // not claimed here. Both orders are covered by the same whole peak.
        if(actor.kind!=28)p->Die(2);
        auto* made=b->AddPlant(col,row,GetNativeSeedForPlant(actor.next),SEED_NONE,&fresh);const auto* d=GetNativePlantData(made);
        if(!made||made->mDead||!d||d->kind!=actor.next||!made->mNetId||!d->instanceToken||
            b->mPlants.DataArrayTryToGet(d->instanceToken)!=made||d->owner!=LawnNet::NET_SHARED_OWNER||
            d->packetId||d->randomOrigin||d->firstParent!=kFusionNoPlant||d->secondParent!=kFusionNoPlant)
        {
            if(made&&!made->mDead)made->Die(255);
            if(auto* ctx=GetNativeContext(b);ctx&&ctx->adventure.enabled)
            {ctx->adventure.schedulerFailed=true;ctx->adventureScheduler.failed=true;}
            LawnNet::NetTrace("Fusion Fertilize invariant FAILED after mutation old=%u result=%u coin=%u",actor.id,actor.next,c->mNetId);
            c->Die();return;
        }
        if(actor.kind==28)p->Die(2);
        LawnNet::NetTrace("Fusion Fertilize upgrade old=%u oldToken=%u kind=%u child=%u token=%u result=%u sourceFreeSetOrder=%d",actor.id,actor.uid,actor.kind,made->mNetId,d->instanceToken,actor.next,actor.kind!=28);
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
bool CanCreateNativeRemaining269Fertilize(Board* b,unsigned count,std::string* error)
{
    if(error)error->clear();const auto reject=[&](const char* why){if(error)*error=why;return false;};
    if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!IsNativeFusionBoard(b)||
        !b->mCoins.mBlock||b->mCoins.mSize>b->mCoins.mMaxSize||count>unsigned(b->mCoins.mMaxSize-b->mCoins.mSize))
        return reject("Fertilize whole native Coin peak unavailable.");
    if(!EnsureNativeFertilize269Visuals(b))return reject("Genuine source Fertilize package unavailable.");
    return true;
}
bool PreflightNativeFertilize269Upgrade(Coin* coin,std::string* error)
{std::vector<FertilizeRecipient269> plan;return FertilizePlan269(coin,FertilizeTarget(coin),plan,error);}
bool CreateNativeRemaining269Fertilize(Plant* p,float x,float y)
{
    if(!Host(p)||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>4096||std::abs(y)>4096||
       !CanCreateNativeRemaining269Fertilize(p->mBoard))return false;auto* b=p->mBoard;auto* c=b->mCoins.DataArrayAlloc();c->mApp=b->mApp;c->mBoard=b;
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
 {float x=0,y=0;std::vector<FertilizeRecipient269> plan;std::string error;
  if(!p->GetShadowCenter(x,y)||!FertilizePlan269(c,p,plan,&error))return false;
  d->targetId=p->mNetId;d->targetToken=GetNativePlantData(p)->instanceToken;d->phase=NativeFertilize269Phase::Using;d->phaseStartAge=c->mCoinAge;d->heldSlot=255;
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
