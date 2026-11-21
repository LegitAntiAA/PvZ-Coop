#include "FusionNativeGoldBoss269.h"
#include "FusionNative.h"
#include "FusionNativeCold.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
bool Host(const Zombie* z)
{return z&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeFusionBoard(z->mBoard);}
bool Playing(const Zombie* z){return Host(z)&&!z->mDead&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
}
bool IsNativeGoldBoss269(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return z&&d&&d->kind==223&&IsNativeFusionBoard(z->mBoard);}
bool InitializeNativeGoldBoss269(Zombie* z)
{
    if(!IsNativeGoldBoss269(z))return false;if(!Host(z))return true;
    if(!EnsureNativeGoldBoss269Dependencies(z->mBoard))return false;
    auto* state=EditNativeGoldBoss269State(z,true);if(!state)return false;*state={};
    // Existing source factory installs exact HP1500/12000/1500 and source float
    // body truth. Do not reinitialize HP in an OnCreate/ghost callback here.
    z->mSummonCounter=13;z->mZombiePhase=PHASE_NEWSPAPER_READING;return true;
}
Plant* FindNativeGoldBoss269Target(Zombie* z)
{
    if(!z||!z->mBoard)return nullptr;Plant* p=nullptr;Plant* target=nullptr;float far=-1;
    float axisX=0,axisY=0,zx=0,zy=0;
    if(!GetNativeGoldBoss269SourcePoint(z,"axis",axisX,axisY)||!GetNativeGoldBoss269SourcePoint(z,"shadow",zx,zy))return nullptr;
    while(z->mBoard->IteratePlants(p))
    {
        const auto* d=GetNativePlantData(p);if(p->mDead||!p->mIsOnBoard||p->mRow!=z->mRow||!d||d->kind==1095||!NativeGoldBoss269TargetPlantAllowed(p))continue;
        float x=0,y=0;if(!p->GetShadowCenter(x,y)||x>=axisX)continue;
        const float distance=std::abs(x-zx);if(distance>far){far=distance;target=p;}
    }
    return target;
}
bool UpdateNativeGoldBoss269(Zombie* z)
{
    if(!IsNativeGoldBoss269(z))return false;if(!Playing(z))return true;
    auto* s=EditNativeGoldBoss269State(z);if(!s||s->lastTick==z->mBoard->mMainCounter)return true;s->lastTick=z->mBoard->mMainCounter;
    if(z->mSummonCounter>0)--z->mSummonCounter;
    if(z->mSummonCounter<=0)
    {
        z->mSummonCounter=11;FusionGoldBoss269Attribute step;
        if(!TryStepFusionGoldBoss269Attribute(s->status,s->attributeCount,FindNativeGoldBoss269Target(z)!=nullptr,step))return true;
        s->attributeCount=step.count;if(step.shoot)TriggerNativeGoldBoss269Animation(z,false);if(step.aimedShoot)TriggerNativeGoldBoss269Animation(z,true);
    }
    return true;
}
bool NativeGoldBoss269Damage(Zombie* z,int resolved,int type)
{
    if(!IsNativeGoldBoss269(z))return false;if(!Playing(z))return true;
    auto* s=EditNativeGoldBoss269State(z);if(!s)return true;const auto* body=GetNativeSourceZombieData(z);if(!body)return true;
    FusionGoldBoss269Armor input{s->status,z->mHelmHealth,z->mHelmMaxHealth,z->mShieldHealth,z->mShieldMaxHealth,body->health};FusionGoldBoss269Damage plan;
    if(!TryPlanFusionGoldBoss269Damage(input,resolved,type,plan))return true;
    if(plan.secondLost&&!CanApplyNativeGoldBoss269ShieldFall(z))
    {LawnNet::NetTrace("Fusion Gold223 shield-fall dependency refused id=%u:203 conversion not committed",z->mNetId);return true;}
    z->mHelmHealth=plan.after.first;z->mShieldHealth=plan.after.second;s->status=uint8_t(plan.after.status);
    if(!plan.firstLost&&z->mHelmHealth>0)s->firstStage=uint8_t(FusionGoldBoss269ArmorStage(z->mHelmHealth,z->mHelmMaxHealth,s->firstStage));
    if(!plan.secondLost&&z->mShieldHealth>0)s->secondStage=uint8_t(FusionGoldBoss269ArmorStage(z->mShieldHealth,z->mShieldMaxHealth,s->secondStage));
    if(plan.firstLost)z->mHelmType=HELMTYPE_NONE;
    if(plan.secondLost){z->mShieldType=SHIELDTYPE_NONE;z->mZombiePhase=PHASE_NEWSPAPER_MADDENING;ApplyNativeGoldBoss269ShieldFall(z);}
    if(z->mDead)return true; // Source reverse conversion may consume223 itself.
    RefreshNativeGoldBoss269ArmorVisuals(z);
    if(plan.cold)ApplyNativeColdSlow(z);
    // The real shared override delegates to Zombie.BodyTakeDamage in6 only.
    if(plan.bodyDamage){z->TakeBodyDamage(plan.bodyDamage,0U);if(auto* d=GetNativeSourceZombieData(z);d&&d->health<0){d->health=0;z->mBodyHealth=1;}}
    LawnNet::NetTrace("Fusion Gold223 damage id=%u type=%d sourceType=%d capped=%d first=%d second=%d body=%.9g state=%u bodyOverrideStatus6=1",z->mNetId,type,plan.sourceType,plan.damage,z->mHelmHealth,z->mShieldHealth,body->health,s->status);
    if(plan.charred)SetNativeGoldBoss269Charred(z);
    if(plan.directDie)RetireNativeGoldBoss269(z,2); // No state reads after canonical removal; no vanilla reward substitute.
    return true;
}
bool NativeGoldBoss269BodyDamage(Zombie* z){const auto* s=GetNativeGoldBoss269State(z);return IsNativeGoldBoss269(z)&&s&&s->status!=6;}
bool NativeGoldBoss269RejectStatus(const Zombie* z,bool mc,bool knock,bool freezeButter)
{const auto* s=GetNativeGoldBoss269State(z);return IsNativeGoldBoss269(z)&&s&&FusionGoldBoss269RejectStatus(s->status,mc,knock,freezeButter);}
void NativeGoldBoss269LosePaperFinished(Zombie* z)
{if(!Playing(z)||!IsNativeGoldBoss269(z))return;if(auto* s=EditNativeGoldBoss269State(z)){s->status=6;z->mZombiePhase=PHASE_NEWSPAPER_MAD;}}
bool NativeGoldBoss269AnimShoot(Zombie* z,bool aimed,bool beforeDying,bool changingRow,bool towardsRight)
{
    if(!Playing(z)||!IsNativeGoldBoss269(z))return false;FusionGoldBoss269Shot plan;
    //223 MC is immune. Facing is still the actual source-facing value, not MC.
    if(!TryPlanFusionGoldBoss269Shot(aimed,z->mMindControlled,towardsRight,z->mDead,beforeDying,changingRow,plan))return false;
    Plant* target=aimed?FindNativeGoldBoss269Target(z):nullptr;if(aimed&&!target)return false;
    return CreateNativeGoldBoss269Cherry(z,plan,target)!=nullptr;
}
bool ApplyNativeGoldBoss269Post(Zombie* z,int award,int round,int difficulty)
{
    if(!Host(z)||!IsNativeGoldBoss269(z)||award<1||award>2)return false;auto* s=EditNativeGoldBoss269State(z);auto* d=GetNativeSourceZombieData(z);if(!s||!d)return false;
    FusionGoldBoss269Health after;if(!TryScaleFusionGoldBoss269Health(d->health,z->mHelmHealth,z->mShieldHealth,round,difficulty,after))return false;
    d->health=after.body;z->mBodyHealth=NativeSourceZombieLivenessHealth(after.body);z->mBodyMaxHealth=after.bodyMaximum;z->mHelmHealth=z->mHelmMaxHealth=after.first;z->mShieldHealth=z->mShieldMaxHealth=after.second;s->awardLevel=uint8_t(award);return true;
}
bool NativeGoldBoss269Dying(Zombie* z)
{
    if(!Host(z)||!IsNativeGoldBoss269(z))return false;auto* s=EditNativeGoldBoss269State(z);if(!s)return false;
    if(!s->deathVisualCommitted){if(!CreateNativeGoldBoss269DeathVisual(z))return false;s->deathVisualCommitted=true;}
    if(s->rewardCommitted||!s->awardLevel)return true;
    FusionGoldBoss269Award plan;if(!TryPlanFusionGoldBoss269Award(s->awardLevel,plan)||!CanCreateNativeGoldBoss269Award(z,plan))return false;
    s->rewardCommitted=true; // One admitted callback cannot duplicate a partial native allocation batch.
    return CreateNativeGoldBoss269Award(z,plan);
}
}
