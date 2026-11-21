#include "FusionNativeAction266.h"
#include "FusionBasePlantRules.h"
#include "FusionNative.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeCold.h"
#include "FusionNativeDeath.h"
#include "FusionNativeDefender.h"
#include "FusionNativeFire.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeGarlic.h"
#include "FusionNativePlacement.h"
#include "FusionNativeProducer.h"
#include "FusionFireActionBatchRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{
    return p && p->mBoard && p->mApp && p->mApp->mBoard==p->mBoard && IsNativeFusionBoard(p->mBoard) &&
        p->mBoard->mBackground==BACKGROUND_1_DAY && p->mIsOnBoard && GetNativePlantData(p);
}
bool Host(const Plant* p)
{ return Current(p) && !p->mNetGhost && !p->mApp->NetIsClient() && !p->mBoard->mPaused && p->mApp->mGameScene==SCENE_PLAYING; }
bool Target(Zombie* z)
{
    return z && !z->mDead && !z->mNetGhost && !z->IsDeadOrDying() && IsNativeFusionGroundZombie(z) &&
        !z->mMindControlled && !z->IsFlying() &&
        (!GetNativeSourceZombieData(z) || (!z->mInPool && !z->mOnHighGround));
}
int Ticks(double seconds,float speed=1)
{ int ticks=0; TryGetFusionAction266Ticks(seconds,speed,ticks);return ticks; }
void Particle(Plant* p,ParticleEffect effect)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    p->mApp->AddTodParticle(x,y,static_cast<int>(RENDER_LAYER_TOP),effect);
    LawnNet::NetBroadcastParticle(p->mApp,x,y,static_cast<int>(RENDER_LAYER_TOP),effect);
}
void Sun(Plant* p)
{
    if(p->mBoard->mCoins.mSize>=p->mBoard->mCoins.mMaxSize)return;
    auto* coin=p->mBoard->AddCoin(p->mX,p->mY,COIN_SUN,COIN_MOTION_FROM_PLANT,SEED_NONE,25);
    if(coin)AttachNativeCoinValue(p->mBoard,coin,25);
}
void SmallBlast(Plant* p,int damage=1800)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    NativeBlastSpec spec;spec.centerX=x;spec.centerY=y;spec.row=p->mRow;
    spec.policy=FusionDeathBlastPolicy::SmallPotato;spec.damage=damage;
    ApplyNativeFusionBlast(p->mBoard,spec);
}
Plant* Mine(Plant* p,int column,const NativePlantData& provenance,int preparation)
{
    NativePlantData incoming;incoming.kind=4;incoming.owner=provenance.owner;
    incoming.packetId=provenance.packetId;incoming.randomOrigin=provenance.randomOrigin;
    auto* child=TrySpawnNativeFusionPlant(p->mBoard,column,p->mRow,incoming);
    if(child)child->mStateCountdown=preparation;
    LawnNet::NetTrace("Fusion native Action266 mine source=%u column=%d child=%u prep=%d tick=%d",p->mNetId,column,child?child->mNetId:0,preparation,p->mBoard->mMainCounter);
    return child;
}
void SunBomb(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    // BombCherry type1 is centered root+.5; native Shadow=root+.3.
    y-=static_cast<float>(.2*kFusionBasePixelsY);
    std::vector<ZombieID> victims;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Target(z)||std::abs(z->mRow-p->mRow)>1)continue;
        const auto r=z->GetZombieRect();
        if(FusionFireCircleTouchesRect(x,y,2,r.mX,r.mY,r.mWidth,r.mHeight))victims.push_back(p->mBoard->ZombieGetID(z));
    }
    for(auto id:victims)if((z=p->mBoard->ZombieTryToGet(id))&&Target(z))
    {Sun(p);ApplyNativeFusionCherryDamage(z);}
    HealNativeCherryNutFromCircle(p->mBoard,x,y,p->mRow,100,2);
    Particle(p,PARTICLE_POWIE);p->mApp->PlayFoley(FOLEY_CHERRYBOMB);p->mBoard->ShakeBoard(3,-4);
    LawnNet::NetTrace("Fusion native Action266 SunBomb plant=%u victims=%zu tick=%d",p->mNetId,victims.size(),p->mBoard->mMainCounter);
    p->Die();
}
}
bool InitializeNativeAction266(Plant* p)
{
    if(!Host(p)||p->mDead)return false;auto* d=GetNativePlantData(p);
    if(d->kind==1009)return false; // Shared producer + native PotatoMine lifecycle.
    if(d->kind==1170)
    {InitializeNativeBatchAction(p);p->mDoSpecialCountdown=Ticks(static_cast<double>(20.0f*RandRangeFloat(.95f,1.05f)));d->batchMode=0;return true;}
    if(d->kind!=1002&&d->kind!=1087&&d->kind!=1145&&d->kind!=1146&&d->kind!=1163)return false;
    p->mState=STATE_NOTREADY;p->mStateCountdown=0;p->mDoSpecialCountdown=0;p->mDisappearCountdown=0;p->mShootingCounter=0;p->mTargetX=0;
    p->mIsAsleep=false;p->mWakeUpCounter=0;d->animationSpeed=RandRangeFloat(.9f,1.1f);d->shootSeconds=-1;
    if(d->kind==1002){p->mDoSpecialCountdown=Ticks(1,d->animationSpeed);d->shootSeconds=0;}
    if(d->kind==1087)p->mLaunchCounter=1500;
    if(d->kind==1145||d->kind==1146||d->kind==1087)InitializeNativeDefender(p);
    return true;
}
bool UpdateNativeAction266(Plant* p)
{
    if(!Current(p))return false;auto* d=GetNativePlantData(p);const auto kind=d->kind;
    if(kind==1009){UpdateNativeProducer(p);return false;}
    if(kind==1170)return false; // Shared shooter and common attribute callback.
    if(kind!=1002&&kind!=1087&&kind!=1145&&kind!=1146&&kind!=1163)return false;
    if(!Host(p)||p->mDead)return true;
    if(kind==1146&&p->mTargetX>0)--p->mTargetX; // Source flashCountDown is independent of bite flashing.
    if(kind==1087&&--p->mLaunchCounter<=0)
    {
        p->mLaunchCounter=1500;int amount=0;
        if(TryGetFusionAction266LanternHeal(GetNativeFireLightLevel(p),amount)&&amount>0)
        {const int before=p->mPlantHealth;p->mPlantHealth=(std::min)(p->mPlantMaxHealth,p->mPlantHealth+amount);NativePlantHealthChanged(p);
         LawnNet::NetTrace("Fusion native Action266 Lantern heal plant=%u amount=%d hp=%d->%d light=%d tick=%d",p->mNetId,amount,before,p->mPlantHealth,GetNativeFireLightLevel(p),p->mBoard->mMainCounter);}
    }
    if(kind==1146&&p->mState==STATE_DOINGSPECIAL&&p->mStateCountdown==0)
    {p->mState=STATE_NOTREADY;p->mDisappearCountdown=0;d->shootSeconds=-1;}
    return true;
}
bool UpdateNativeAction266Phase(Plant* p)
{
    if(!Host(p)||p->mDead)return false;auto* d=GetNativePlantData(p);
    if(d->kind!=1002&&d->kind!=1009&&d->kind!=1087&&d->kind!=1145&&d->kind!=1146&&d->kind!=1163)return false;
    d->idleSeconds+=.01f;if(d->shootSeconds>=0)d->shootSeconds+=.01f;return true;
}
bool ApplyNativeAction266Special(Plant* p)
{
    if(!Host(p)||p->mDead)return false;auto* d=GetNativePlantData(p);const auto kind=d->kind;
    if(kind==1002){SunBomb(p);return true;}
    if(kind==1170)
    {
        const auto provenance=*d;auto* child=TrySpawnNativeFusionPlant(p->mBoard,p->mPlantCol,p->mRow,provenance);
        d=GetNativePlantData(p);if(child)d->batchMode=1;
        else p->mDoSpecialCountdown=Ticks(static_cast<double>(20.0f*RandRangeFloat(.95f,1.05f)));
        LawnNet::NetTrace("Fusion native Action266 Puff clone source=%u child=%u stopped=%u tick=%d",p->mNetId,child?child->mNetId:0,d->batchMode,p->mBoard->mMainCounter);return true;
    }
    if(kind==1015&&d->batchMode)
    {
        const auto provenance=*d;
        if(d->batchMode==1){d->batchMode=2;SmallBlast(p);p->mDoSpecialCountdown=100;}
        else {d->batchMode=0;Mine(p,p->mPlantCol+1,provenance,1);}return true;
    }
    if(kind==1146&&p->mState==STATE_DOINGSPECIAL)
    {
        std::vector<ZombieID> targets;Zombie* z=nullptr;
        while(p->mBoard->IterateZombies(z))if(Target(z)&&z->mRow==p->mRow)targets.push_back(p->mBoard->ZombieGetID(z));
        for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id)))BeginNativeGarlicEffect(z,10,false);
        LawnNet::NetTrace("Fusion native Action266 GoldGarlic plant=%u targets=%zu tick=%d",p->mNetId,targets.size(),p->mBoard->mMainCounter);return true;
    }
    return false;
}
bool TryActivateNativeAction266Skill(Plant* p)
{
    if(!Host(p)||p->mDead||p->mPlantHealth<=0)return false;auto* d=GetNativePlantData(p);
    if(d->kind!=1146||p->mTargetX>0||p->mState==STATE_DOINGSPECIAL)return false;
    p->mPlantHealth=p->mPlantMaxHealth;NativePlantHealthChanged(p);p->mEatenFlashCountdown=200;p->mTargetX=200;
    p->mState=STATE_DOINGSPECIAL;p->mStateCountdown=101;p->mDisappearCountdown=102;
    p->mDoSpecialCountdown=68;d->shootSeconds=0;return true;
}
bool GetNativeAction266VisualPose(const Plant* p,const char*& state,float& elapsed)
{
    if(!Current(p))return false;const auto* d=GetNativePlantData(p);
    if(d->kind==1002){state="Bomb";elapsed=d->shootSeconds*d->animationSpeed;return true;}
    if(d->kind==1146&&p->mState==STATE_DOINGSPECIAL){state="super";elapsed=d->shootSeconds;return true;}
    return false;
}
int GetNativeAction266LightContribution(const Plant* receiver)
{
    if(!Current(receiver))return 0;int level=0;Plant* p=nullptr;
    while(receiver->mBoard->IteratePlants(p))
    {
        const auto* d=GetNativePlantData(p);
        if(d&&d->kind==1087&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&
            FusionFireLightCellAffected(p->mPlantCol,p->mRow,receiver->mPlantCol,receiver->mRow))++level;
    }
    return level;
}
void NativeAction266HealthChanged(Plant* p)
{
    if(!Host(p)||p->mDead)return;auto* d=GetNativePlantData(p);
    if(IsFusionAction266Rock(d->kind))TryGetFusionAction266RockStage(p->mPlantHealth,p->mPlantMaxHealth,d->damageStage);
}
void NativeAction266PlantRemoving(Plant* p,int reason)
{
    if(!Host(p)||!p->mDead||!FusionDeathReasonHasEffects(reason))return;
    const auto provenance=*GetNativePlantData(p);
    if(provenance.kind==1009&&!p->mApp->IsIZombieLevel()){Sun(p);Sun(p);}
    if(provenance.kind==1163)
    {
        // Source alternates +1,-1,+2,-2,... and admits up to seven successful mines.
        int made=0;const int attempts=p->mBoard->GetColumnCount()*2+1;
        for(int i=0;i<attempts&&made<7;++i)
        {const int offset=(i%2==0?1:-1)*(i/2+1);if(Mine(p,p->mPlantCol+offset,provenance,501))++made;}
    }
}
void NativeAction266ChomperSwallowStarted(Plant* p)
{
    if(!Host(p)||p->mDead)return;auto* d=GetNativePlantData(p);if(d->kind!=1015)return;
    d->batchMode=1;p->mDoSpecialCountdown=51;
}
bool NativeAction266GloomAcquire(Plant* p,Zombie* z)
{
    if(!Host(p)||p->mDead||!Target(z)||p->mBoard!=z->mBoard)return false;
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;const auto r=z->GetZombieRect();
    return FusionAction266GloomTouches(GetNativePlantData(p)->kind,z->mRow-p->mRow,x,y,r.mX,r.mY,r.mWidth,r.mHeight);
}
void ApplyNativeAction266Jalaed(Zombie* z)
{
    if(!z||z->mDead||z->mNetGhost||!z->mApp||z->mApp->NetIsClient())return;
    NativeWarmAfterDamage(z);
    auto* d=GetOrCreateNativeZombieActionData(z);if(d)d->jalaedOnDeath=true;
}
void ApplyNativeAction266GloomAttack(Plant* p)
{
    if(!Host(p)||p->mDead)return;const auto kind=GetNativePlantData(p)->kind;if(!IsFusionAction266Gloom(kind))return;
    std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(NativeAction266GloomAcquire(p,z))targets.push_back(p->mBoard->ZombieGetID(z));
    const FusionColdDamageContext cold{0,FusionColdDamageDelivery::IceAll};
    for(auto it=targets.rbegin();it!=targets.rend();++it)if((z=p->mBoard->ZombieTryToGet(*it))&&Target(z))
    {
        z->TakeDamage(kind==1071?40:20,2U,kind==1072?&cold:nullptr);
        if(kind==1071)ApplyNativeAction266Jalaed(z);
        if(kind==1072)AddNativeColdLevel(z,5);
        LawnNet::NetTrace("Fusion native Action266 Gloom plant=%u kind=%u target=%u body=%d shield=%d helm=%d tick=%d",p->mNetId,kind,z->mNetId,z->mBodyHealth,z->mShieldHealth,z->mHelmHealth,p->mBoard->mMainCounter);
    }
}
bool HandleNativeAction266Crush(Plant* p)
{
    if(!Host(p)||p->mDead||!IsFusionAction266Rock(GetNativePlantData(p)->kind))return false;
    // Source calls Plant.Crashed then clears isCrashed. Persistent squished
    // retirement belongs to the ordinary native path, so rocks consume it here.
    p->mSquished=false;p->mPlantHealth-=50;p->mEatenFlashCountdown=(std::max)(p->mEatenFlashCountdown,25);
    NativePlantHealthChanged(p);if(p->mPlantHealth<=0)p->Die();return true;
}
}
