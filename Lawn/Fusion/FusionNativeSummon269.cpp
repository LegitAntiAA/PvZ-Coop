#include "FusionNativeSummon269.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeMoney.h"
#include "FusionBasePlantRules.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(Board* board)
{return board&&board->mApp&&board->mApp->mBoard==board&&IsNativeFusionBoard(board)&&!board->mApp->NetIsClient();}
void DetachChildren(Plant* parent,bool replace)
{
    if(!parent||!Host(parent->mBoard))return;
    auto* context=GetNativeContext(parent->mBoard);const auto* identity=GetNativePlantData(parent);
    if(!context||!identity)return;
    const auto parentId=parent->mNetId;const auto token=identity->instanceToken;
    std::vector<Zombie*> retire;
    for(auto& [generation,data]:context->sourceZombies)
    {
        if(data.sourceParentId!=parentId||data.sourceParentToken!=token)continue;
        if(replace&&!data.dieWithParent)continue;
        auto* child=parent->mBoard->mZombies.DataArrayTryToGet(generation);
        const bool linked=data.dieWithParent;
        data.sourceParentId=0;data.sourceParentToken=0;data.dieWithParent=false;
        if(linked&&child&&!child->mDead&&child->mNetId&&data.instanceToken==generation)retire.push_back(child);
    }
    for(auto* child:retire)child->DieNoLoot();
}
}
Zombie* TrySpawnNativeSummon269(Plant* parent,const FusionSummon269Choice& choice,int row,float x,bool replace)
{
    if(!parent||!Host(parent->mBoard)||parent->mNetGhost||parent->mBoard->mPaused||!std::isfinite(x)||
        !FindFusionZombieProfile(choice.kind)||
        (choice.post!=FusionSummon269Post::None&&choice.post!=FusionSummon269Post::Health60&&
         choice.post!=FusionSummon269Post::BreakSecondArmor))return nullptr;
    const auto* identity=GetNativePlantData(parent);
    if(!identity||!IsFusionSummon269Caller(identity->kind)||(replace&&identity->kind!=1151))return nullptr;
    std::string error;if(!EnsureNativeSourceZombieVisuals(parent->mBoard,choice.kind,&error))return nullptr;
    NativeSourceZombieSpawnRequest request;
    request.kind=choice.kind;request.row=row;request.positioned=true;request.shadowX=x;
    request.friendly=choice.friendly;request.withEffect=choice.withEffect;request.dieWithParent=replace;
    if(!parent->mDead){request.parentId=parent->mNetId;request.parentToken=identity->instanceToken;}
    else if(replace)return nullptr;
    if(replace)DetachChildren(parent,true);
    auto* child=SpawnNativeSourceZombie(parent->mBoard,request);if(!child)return nullptr;
    if(choice.post==FusionSummon269Post::BreakSecondArmor)
        child->TakeDamage(child->mShieldMaxHealth,0U);
    if(choice.post==FusionSummon269Post::Health60)
    {
        auto* data=GetNativeSourceZombieData(child);data->health=.6f*data->health;
        child->mBodyMaxHealth=int(float(child->mBodyMaxHealth)*.6f);
        child->mBodyHealth=NativeSourceZombieLivenessHealth(data->health);
        child->mHelmHealth=int(float(child->mHelmHealth)*.6f);child->mHelmMaxHealth=int(float(child->mHelmMaxHealth)*.6f);
        child->mShieldHealth=int(float(child->mShieldHealth)*.6f);child->mShieldMaxHealth=int(float(child->mShieldMaxHealth)*.6f);
    }
    LawnNet::NetTrace("Fusion Summon269 spawn parent=%u token=%u child=%u kind=%u friendly=%d linked=%d post=%u row=%d x=%.9g",
        request.parentId,request.parentToken,child->mNetId,choice.kind,choice.friendly?1:0,replace?1:0,unsigned(choice.post),row,x);
    return child;
}
void NativeSummon269PlantRemoving(Plant* parent){DetachChildren(parent,false);}
void NativeSummon269ZombieRemoving(Zombie* zombie)
{if(zombie&&Host(zombie->mBoard))if(auto* d=GetNativeSourceZombieData(zombie)){d->sourceParentId=0;d->sourceParentToken=0;d->dieWithParent=false;}}
bool NativeSummon269MoneyDamage(Zombie* zombie,int damage)
{
    const auto* read=GetNativeSourceZombieData(zombie);
    if(!read||!IsFusionSummon269Money(read->kind)||!IsNativeSourceZombie(zombie))return false;
    if(!Host(zombie->mBoard)||zombie->mDead||zombie->IsDeadOrDying())return true;
    auto* data=GetNativeSourceZombieData(zombie);const float health=data->health-float(damage);
    if(!std::isfinite(health))return true;
    data->health=health;zombie->mBodyHealth=NativeSourceZombieLivenessHealth(health);zombie->mJustGotShotCounter=30;
    return true;
}
bool NativeSummon269RejectStatus(const Zombie* zombie)
{const auto* d=GetNativeSourceZombieData(zombie);return d&&IsFusionSummon269Money(d->kind)&&IsNativeSourceZombie(zombie);}
bool NativeSummon269RejectCold(const Zombie* zombie)
{const auto* d=GetNativeSourceZombieData(zombie);return NativeSummon269RejectStatus(zombie)||(d&&d->kind==111&&IsNativeSourceZombie(zombie));}
void NativeSummon269MoneyDied(Zombie* zombie)
{
    const auto* d=GetNativeSourceZombieData(zombie);int item=0,count=0;
    if(!zombie||!Host(zombie->mBoard)||!d||zombie->mDroppedLoot||!TryGetFusionMoney269Reward(d->kind,item,count))return;
    zombie->mDroppedLoot=true;
    float x=0,y=0;if(!zombie->GetShadowCenter(x,y))return;
    if(zombie->mBoard->mCoins.mMaxSize-zombie->mBoard->mCoins.mSize<count)return;
    for(int i=0;i<count;++i)EmitNativeMoneyItem(zombie->mBoard,x,y-float(GetNativeFusionPixelsY(zombie->mBoard)),uint16_t(item));
}
void FinishNativeSummon269ZombiePosition(Zombie* zombie)
{
    const auto* d=GetNativeSourceZombieData(zombie);
    if(!zombie||!Host(zombie->mBoard)||zombie->mDead||!d||!IsFusionSummon269Money(d->kind))return;
    float x=0,y=0;if(!zombie->GetShadowCenter(x,y))return;
    const float sourceEight=float(zombie->mBoard->GetCellCenterX(0,0)+(8.0+4.8)*kFusionBasePixelsX);
    if(x>sourceEight){zombie->mPosX-=x-sourceEight;zombie->mX=int(zombie->mPosX);}
}
}
