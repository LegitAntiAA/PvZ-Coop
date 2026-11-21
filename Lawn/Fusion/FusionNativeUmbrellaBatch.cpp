#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeUmbrella.h"
#include "FusionNative.h"
#include "FusionNativeAction267.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeButter.h"
#include "FusionNativeDefender.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionNativeLobber.h"
#include "FusionNativeMoney.h"
#include "FusionFireActionBatchRules.h"
#include "FusionLobberRules.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeSummon269.h"
#include "FusionSummon269Rules.h"
#include "FusionNativeWater268.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard);}
bool Host(const GameObject* p)
{return Current(p)&&!p->mNetGhost&&!p->mApp->NetIsClient()&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
bool Live(const Plant* p){return Current(p)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
bool Awake(const Plant* p,float& speed)
{const auto* d=GetNativePlantData(p);speed=d?d->animationSpeed:0;return Live(p)&&!p->mIsAsleep&&!p->mSquished&&speed>0;}
uint32_t Generation(Plant* p){return p->mBoard->mPlants.DataArrayGetID(p);}
uint32_t Generation(Projectile* p){return p->mBoard->mProjectiles.DataArrayGetID(p);}
bool Pot(Plant* p){return GetNativeDefenderPotKind(p)==1133;}
bool Target(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&z->mBodyHealth>0&&!NativeWater268ZombieCaptured(z);}
bool Converting(uint16_t kind){return kind==916||kind==923||kind==1148;}
void Knock(Zombie* z,double distance)
{
    float x=0,y=0;if(!GetNativeBaseZombieGroundAnchor(z,x,y))return;auto* b=z->mBoard;
    const double origin=b->GetCellCenterX(0,z->mRow),sourceX=(x-origin)/kFusionBasePixelsX-4.8;
    const double right=1.35*(b->GetColumnCount()-1)-4.8+4;double moved=sourceX;
    if(TryGetFusionFireKnockBack(sourceX,right,distance,moved)){z->mPosX+=float((moved-sourceX)*kFusionBasePixelsX);z->mX=int(z->mPosX);}
}
bool Circle(Plant* p,Zombie* z,double radius)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;const auto r=z->GetZombieRect();
    const double py=GetNativeFusionPixelsY(p->mBoard),cx=x,cy=y-.5*py;
    const double dx=((std::max)(double(r.mX),(std::min)(cx,double(r.mX+r.mWidth)))-cx)/kFusionBasePixelsX;
    const double dy=((std::max)(double(r.mY),(std::min)(cy,double(r.mY+r.mHeight)))-cy)/py;
    return dx*dx+dy*dy<=radius*radius;
}
void Pose(Plant* p,uint8_t mode)
{auto* d=GetNativePlantData(p);d->batchMode=mode;d->shootSeconds=0;}
void BlockZombies(Plant* p,bool skill)
{
    auto* d=GetNativePlantData(p);const bool pot=Pot(p);int count=0;Zombie* z=nullptr;
    const int stored=d->kind==1131?int(d->batchShotCount):0;
    // Consume before synchronous damage; captured source damage is identical for every recipient.
    if(d->kind==1131)d->batchShotCount=0;
    std::vector<ZombieID> targets;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Target(z)||(!skill&&z->mRow!=p->mRow)||!Circle(p,z,skill?3:2))continue;
        const int status=GetNativeFusionZombieStatus(z);
        if(skill){if(std::abs(z->mRow-p->mRow)>1||z->IsFlying()||status==7||status==10||status==12||(status>=21&&status<=24))continue;}
        else if(z->IsFlying()||!IsNativeFusionGroundZombie(z))continue;
        targets.push_back(p->mBoard->ZombieGetID(z));
    }
    for(auto id:targets)
    {
        z=p->mBoard->ZombieTryToGet(id);if(!Target(z))continue;++count;
        if(skill){z->TakeDamage(40,1U);Knock(z,pot?4:2);EmitNativeMoneyItem(p->mBoard,z->mPosX+20,z->mPosY+20,38);}
        else if(d->kind==1115){z->TakeDamage(40,1U);ApplyNativeFusionButter(z,pot?600:400);Knock(z,pot?3:2);}
        else if(d->kind==1127){z->TakeDamage(40,1U);BeginNativeGarliced(z,false);Knock(z,pot?4:2);}
        else if(d->kind==1131){z->TakeDamage(80+stored,1U);Knock(z,pot?3:2);}
        else if(Converting(d->kind)){z->TakeDamage(40,1U);BeginNativeGarliced(z,false);Knock(z,pot?4:2);}
        else Knock(z,pot?3:2);
    }
    if(!skill&&count)ApplyNativeSourcePlantDamage(p,FusionUmbrellaSelfDamage267(count),0);
    LawnNet::NetTrace("Fusion native Umbrella267 area plant=%u kind=%u count=%d skill=%d stored=%d tick=%d",p->mNetId,d->kind,count,skill?1:0,stored,p->mBoard->mMainCounter);
}
bool Touching(Plant* p,Zombie* z)
{
    if(!Target(z)||z->mRow!=p->mRow||z->IsFlying()||!IsNativeFusionGroundZombie(z))return false;
    const auto a=p->GetPlantRect(),b=z->GetZombieRect();
    return a.mX<=b.mX+b.mWidth&&a.mX+a.mWidth>=b.mX&&a.mY<=b.mY+b.mHeight&&a.mY+a.mHeight>=b.mY;
}
void ConversionSkill(Plant* p)
{
    std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        const int status=GetNativeFusionZombieStatus(z),kind=GetNativeFusionZombieKind(z);
        if(!Target(z)||std::abs(z->mRow-p->mRow)>1||!Circle(p,z,3)||IsFusionSummon269Money(uint16_t(kind))||
            status==7||status==10||status==12||(status>=21&&status<=24)||status<0)continue;
        targets.push_back(p->mBoard->ZombieGetID(z));
    }
    int affected=0;
    for(auto id:targets)
    {
        z=p->mBoard->ZombieTryToGet(id);if(!Target(z))continue;
        const int sourceKind=GetNativeFusionZombieKind(z);
        const bool big=sourceKind>=0?IsFusionSummon269BigKind(uint16_t(sourceKind)):
            z->mZombieType==ZOMBIE_GARGANTUAR||z->mZombieType==ZOMBIE_REDEYE_GARGANTUAR;
        // Source AnimSuperShoot: knock first, then40 Shieldless, then virtual
        // ConvertZombie(.1,.25). Conversion reads those post-hit HP channels.
        Knock(z,Pot(p)?(big?2:4):(big?1:2));z->TakeDamage(40,1U);
        if(!z->mDead&&!z->IsDeadOrDying())TryConvertNativeUmbrella269(p,z,.1f,.25f);
        ++affected;
    }
    float x=0,y=0;if(p->GetShadowCenter(x,y))for(int i=0;i<affected;++i)EmitNativeMoneyItem(p->mBoard,x,y,38);
    LawnNet::NetTrace("Fusion Umbrella269 skill plant=%u kind=%u targets=%d tick=%d",p->mNetId,GetNativePlantData(p)->kind,affected,p->mBoard->mMainCounter);
}
}
bool IsNativeUmbrellaBatch(const Plant* p)
{const auto* d=GetNativePlantData(p);return Current(p)&&d&&IsFusionUmbrellaBatchKind(d->kind);}
bool InitializeNativeUmbrellaBatch(Plant* p)
{
    if(!Host(p)||!IsNativeUmbrellaBatch(p))return false;auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(d->kind);
    if(!definition||!definition->hasStats)return false;p->mPlantHealth=p->mPlantMaxHealth=definition->maxHealth;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->shootSeconds=-1;d->batchMode=0;d->batchShotCount=0;d->batchRageTicks=0;d->batchBagCount=d->kind==1185?2000:0;
    p->mTargetX=0;p->mTargetY=100;p->mLaunchCounter=0;p->mShootingCounter=0;p->mDoSpecialCountdown=0;
    return true;
}
bool UpdateNativeUmbrellaBatch(Plant* p)
{
    if(!IsNativeUmbrellaBatch(p))return false;if(!Host(p)||!Live(p))return true;auto* d=GetNativePlantData(p);float speed=0;
    if(p->mTargetX>0)--p->mTargetX;d->batchRageTicks=FusionUmbrellaBatchMoneySkillFee(d->kind)>0?uint16_t(p->mTargetX):0;
    if(d->kind==1185&&p->mTargetY>0&&!--p->mTargetY)
    {p->mTargetY=100;const int level=GetNativeFireLightLevel(p);if(level>0)d->batchBagCount=uint16_t((std::min)(2000,int(d->batchBagCount)+level*25));}
    if(!Awake(p,speed)||d->kind==30)return true;
    if(p->mLaunchCounter>0)--p->mLaunchCounter;
    if(d->kind!=1186&&p->mLaunchCounter<=0)
    {
        Zombie* z=nullptr;bool touched=false;
        while(p->mBoard->IterateZombies(z))if(Touching(p,z)){touched=true;break;}
        if(touched){p->mLaunchCounter=100;if(!FusionUmbrellaBatchMoneySkillFee(d->kind)||(d->batchMode&4)==0)Pose(p,d->kind==1128||d->kind==1131?4:3);BlockZombies(p,false);}
    }
    if(d->kind==1186&&!p->mTargetX)
    {
        Zombie* z=nullptr;int count=0;
        while(p->mBoard->IterateZombies(z))if(Target(z)&&z->mRow==p->mRow&&z->IsFlying()&&Circle(p,z,1))
        {++count;if(z->mZombieType==ZOMBIE_BALLOON)z->LandFlyer(0U);else Knock(z,Pot(p)?3:2);}
        if(count){p->mTargetX=100;Pose(p,3);ApplyNativeSourcePlantDamage(p,FusionUmbrellaSelfDamage267(count),0);}
    }
    return true;
}
bool UpdateNativeUmbrellaBatchPhase(Plant* p)
{
    if(!IsNativeUmbrellaBatch(p))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);d->idleSeconds+=.01f;
    if(d->shootSeconds<0)return true;d->shootSeconds+=.01f;
    const bool alternate=(d->batchMode&4)!=0;
    const double stateSpeed=alternate?(d->kind==1128?1.1:FusionUmbrellaBatchMoneySkillFee(d->kind)>0?1:1.2):1.2;
    const double length=alternate?(d->kind==1128?double(.8333333f):d->kind==1131?double(.41666666f):1):1;
    const double allOver=alternate&&!FusionUmbrellaBatchMoneySkillFee(d->kind)?length:.75;
    const double clip=double(d->shootSeconds)*stateSpeed*d->animationSpeed;
    if(clip>=allOver)
    {
        d->batchMode&=uint8_t(~3U);
        auto& pose=GetNativeContext(p->mBoard)->umbrellaPoses[Generation(p)];
        if(pose.plantGeneration){ApplyNativeUmbrellaPoseEvent(p,pose,FusionUmbrellaPoseEvent::BlockOver);ApplyNativeUmbrellaPoseEvent(p,pose,FusionUmbrellaPoseEvent::AllBlockOver);}
    }
    if(clip>=length){d->shootSeconds=-1;d->batchMode=0;}
    return true;
}
bool ApplyNativeUmbrellaBatchSpecial(Plant* p)
{if(!Host(p)||!IsNativeUmbrellaBatch(p)||!FusionUmbrellaBatchMoneySkillFee(GetNativePlantData(p)->kind)||(GetNativePlantData(p)->batchMode&4)==0)return false;if(Converting(GetNativePlantData(p)->kind))ConversionSkill(p);else BlockZombies(p,true);return true;}
bool GetNativeUmbrellaBatchVisualPose(const Plant* p,const char*& state,float& elapsed)
{if(!IsNativeUmbrellaBatch(p))return false;const auto* d=GetNativePlantData(p);state=d->shootSeconds<0?"idle":d->batchMode&4?(FusionUmbrellaBatchMoneySkillFee(d->kind)>0?"super":"block2"):"block";elapsed=d->shootSeconds<0?d->idleSeconds:d->shootSeconds;return true;}
bool CanActivateNativeUmbrellaBatchSkill(const Plant* p)
{return IsNativeUmbrellaBatch(p)&&Live(p)&&FusionUmbrellaBatchMoneySkillFee(GetNativePlantData(p)->kind)>0&&
    (p->mNetGhost||p->mApp->NetIsClient()?GetNativePlantData(p)->batchRageTicks==0:!p->mTargetX);}
bool TryActivateNativeUmbrellaBatchSkill(Plant* p)
{
    if(!Host(p)||!CanActivateNativeUmbrellaBatchSkill(p))return false;
    p->mTargetX=750;GetNativePlantData(p)->batchRageTicks=750;
    p->mPlantHealth=p->mPlantMaxHealth;NativePlantHealthChanged(p);Pose(p,4);
    // All source super clips dispatch BlockStart at0. Keep an incoming shot
    // from restarting block3 and cancelling the pending AnimSuperShoot event.
    auto& pose=GetNativeContext(p->mBoard)->umbrellaPoses[Generation(p)];
    pose.plantGeneration=Generation(p);pose.blocking=true;ApplyNativeUmbrellaPoseEvent(p,pose,FusionUmbrellaPoseEvent::BlockStart);
    p->mDoSpecialCountdown=int(std::ceil(double(.083333336f)/GetNativePlantData(p)->animationSpeed*100));return true;
}
bool TryInterceptNativeUmbrella(Projectile* projectile,Plant* attacked)
{
    if(!Host(projectile)||!Live(attacked))return false;auto* d=GetNativeProjectileData(projectile);auto* context=GetNativeContext(projectile->mBoard);if(!d||!context)return false;
    NativeUmbrellaProjectileView view{Generation(projectile),d->kind,d->specialType,d->isZombieBullet,d->hitLatched,projectile->mRow,NativeUmbrellaVerticalAxis::PlanarY};
    auto* umbrella=FindNativeUmbrellaForTarget(projectile,attacked,&view,Awake);if(!umbrella)return false;
    auto& block=context->umbrellaBlocks[view.generation];auto& pose=context->umbrellaPoses[Generation(umbrella)];bool start=false;
    if(!TryNativeUmbrellaIntercept(projectile,attacked,umbrella,&view,Awake,block,pose,start))return false;
    d->hitLatched=true;if(start){Pose(umbrella,3);ApplyNativeUmbrellaPoseEvent(umbrella,pose,FusionUmbrellaPoseEvent::BlockStart);}
    const auto kind=GetNativePlantData(umbrella)->kind;
    if(kind==1131||kind==923)
    {
        auto* pd=GetNativePlantData(umbrella);if(kind==1131)pd->batchShotCount=uint32_t((std::min)(uint64_t(1000000),uint64_t(pd->batchShotCount)+uint32_t(d->damage)));
        SpawnNativeSourcePlanarProjectile(projectile->mBoard,59,projectile->mRow,Pot(umbrella)?160:80,projectile->mPosX+20,projectile->mPosY+20,-projectile->mVelX,-projectile->mVelY,projectile->mAccZ,false);
        projectile->Die();
        if(kind==923){EmitNativeMoneyItem(umbrella->mBoard,umbrella->mX+20,umbrella->mY+20,38);EmitNativeMoneyItem(umbrella->mBoard,umbrella->mX+20,umbrella->mY+20,38);}
    }
    else if(kind==1143||kind==1144||kind==916||kind==1148)
    {EmitNativeMoneyItem(umbrella->mBoard,umbrella->mX+20,umbrella->mY+20,38);if(kind!=1143)EmitNativeMoneyItem(umbrella->mBoard,umbrella->mX+20,umbrella->mY+20,38);}
    return true;
}
void AdvanceNativeUmbrellaProjectiles(Projectile* p)
{
    if(!Current(p))return;auto* c=GetNativeContext(p->mBoard);if(!c)return;const auto id=Generation(p);const auto found=c->umbrellaBlocks.find(id);if(found==c->umbrellaBlocks.end())return;
    AdvanceNativeUmbrellaDeflection(p,found->second,NativeUmbrellaVerticalAxis::PlanarY);
}
bool ApplyNativeUmbrellaProjectileImpact(Projectile* p,Zombie* z)
{
    const auto* d=GetNativeProjectileData(p);if(!Host(p)||!d||d->kind!=59||d->isZombieBullet||d->hitLatched||(z&&!Target(z)))return false;
    // CreateBullet441CC0 maps59 to MelonBullet; use its existing native splash.
    return ApplyNativeLobberImpact(p,z);
}
void NativeUmbrellaBatchRemoving(Plant* p)
{if(Current(p))if(auto* c=GetNativeContext(p->mBoard))c->umbrellaPoses.erase(Generation(p));}
bool TryConvertNativeUmbrella269(Plant* p,Zombie* z,float gold,float silver)
{
    const auto* pd=GetNativePlantData(p);
    if(!Host(p)||!Live(p)||!pd||!Converting(pd->kind)||!Target(z)||p->mBoard!=z->mBoard)return false;
    const int sourceKind=GetNativeFusionZombieKind(z);
    if(sourceKind==41||sourceKind==42)return false;
    const bool big=sourceKind>=0?IsFusionSummon269BigKind(uint16_t(sourceKind)):
        z->mZombieType==ZOMBIE_GARGANTUAR||z->mZombieType==ZOMBIE_REDEYE_GARGANTUAR;
    FusionUmbrella269Plan plan;
    if(!TryPlanFusionUmbrella269(pd->kind,GetNativeFusionZombieHealth(z),z->mHelmHealth,z->mBodyMaxHealth,z->mHelmMaxHealth,
        gold,silver,big,p->mBoard->mPlantRow[z->mRow]==PLANTROW_POOL,z->mRow==p->mRow,!z->mHasHead,plan))return false;
    float x=0,y=0;if(!z->GetShadowCenter(x,y))return false;
    if(plan.coins>p->mBoard->mCoins.mMaxSize-p->mBoard->mCoins.mSize)return false;
    uint16_t replacement=0;
    if(plan.action==FusionUmbrella269Action::Replace)
    {
        FusionSummon269Choice choice;choice.kind=plan.kind;choice.friendly=false;choice.withEffect=false;
        auto* child=TrySpawnNativeSummon269(p,choice,z->mRow,x);
        if(!child){LawnNet::NetTrace("Fusion Umbrella269 conversion blocked parent=%u target=%u requiredNPC=%u",p->mNetId,z->mNetId,plan.kind);return false;}
        replacement=child->mNetId;
    }
    else if(plan.action==FusionUmbrella269Action::Garlic)BeginNativeGarliced(z,false);
    else if(plan.action==FusionUmbrella269Action::DamageAndCoins)z->TakeDamage(plan.damage,1U);
    for(int i=0;i<plan.coins;++i)EmitNativeMoneyItem(p->mBoard,x,y-float(GetNativeFusionPixelsY(p->mBoard)),36);
    const auto original=z->mNetId;
    // Source Die(reason2) destroys the old identity immediately. The new Money
    // actor has its own actual full generation and remains hostile, not MC.
    if(plan.retireOriginal)z->DieNoLoot();
    LawnNet::NetTrace("Fusion Umbrella269 convert plant=%u kind=%u original=%u replacement=%u npcKind=%u action=%u coins=%d tick=%d",
        p->mNetId,pd->kind,original,replacement,plan.kind,unsigned(plan.action),plan.coins,p->mBoard->mMainCounter);
    return true;
}
bool TryHandleNativeUmbrella267Damage(Plant* p,int& damage,int)
{
    if(Host(p)&&IsNativeUmbrellaBatch(p)&&GetNativePlantData(p)->kind==923&&damage>0)damage=int(float(damage)*.1f);
    if(!Host(p)||!IsNativeUmbrellaBatch(p)||GetNativePlantData(p)->kind!=1185)return false;
    auto* d=GetNativePlantData(p);uint16_t after=0;int applied=damage;
    if(TryApplyFusionLanternUmbrella267Damage(damage,d->batchBagCount,applied,after)){damage=applied;d->batchBagCount=after;if(!after)p->mTargetY=500;}
    return false;
}
bool TryEnhanceNativeUmbrellaProjectile(Projectile* p,Plant* actualTarget)
{
    if(!Host(p)||!IsNativeUmbrellaBatch(actualTarget)||!Live(actualTarget)||GetNativePlantData(actualTarget)->kind!=1128||actualTarget->mBoard!=p->mBoard||actualTarget->mRow!=p->mRow)return false;
    auto* d=GetNativeProjectileData(p);if(!d||d->isZombieBullet||d->umbrellaed||!IsNativeLobberProjectile(p))return false;
    Zombie* z=nullptr;Zombie* candidate=nullptr;double best=(std::numeric_limits<double>::max)();float ux=0,uy=0;if(!actualTarget->GetShadowCenter(ux,uy))return false;
    const double vision=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX;
    while(p->mBoard->IterateZombies(candidate))
    {
        float x=0,y=0;const int status=GetNativeFusionZombieStatus(candidate);
        if(!Target(candidate)||!candidate->GetShadowCenter(x,y)||std::abs(candidate->mRow-p->mRow)>(Pot(actualTarget)?1:0)||x>=vision||status==3||
            (status>=9&&status<=24&&status!=11&&status!=14&&status!=15&&status!=17&&status!=18&&status!=19))continue;
        const double distance=std::abs(double(x)-ux);if(distance<best){best=distance;z=candidate;}
    }
    if(!z)return SetNativeLobberRetarget(p,p->mRow,0,float(-10*GetNativeFusionPixelsY(p->mBoard)*.01),float(14.715*GetNativeFusionPixelsY(p->mBoard)*.0001));
    float px=0,py=0;if(!GetNativeLobberProjectileCenter(p,px,py))return false;const auto r=z->GetZombieRect();
    float vx=0,vy=0,g=0;
    if(!TryMakeFusionUmbrella267Retarget(px,py,r.mX+r.mWidth*.5,double(r.mY),GetNativeFusionPixelsY(p->mBoard),vx,vy,g))return false;int reinforced=0;if(!TryGetFusionUmbrella267Reinforcement(d->damage,false,reinforced))return false;
    if(!SetNativeLobberRetarget(p,z->mRow,vx,vy,g))return false;d->damage=reinforced;d->umbrellaed=true;
    if(d->kind==53)p->mBoard->AddCoin(int(px-20),int(py-20),COIN_SMALLSUN,COIN_MOTION_FROM_PLANT,SEED_NONE,5);
    LawnNet::NetTrace("Fusion native Umbrella267 reinforce plant=%u projectile=%u damage=%d tick=%d",actualTarget->mNetId,p->mNetId,reinforced,p->mBoard->mMainCounter);return true;
}
int GetNativeUmbrella267LightContribution(const Plant* p)
{
    if(!Current(p))return 0;int level=0;Plant* other=nullptr;
    while(p->mBoard->IteratePlants(other)){const auto* d=GetNativePlantData(other);if(Live(other)&&d&&d->kind==1185&&std::abs(other->mPlantCol-p->mPlantCol)<=1&&std::abs(other->mRow-p->mRow)<=1)++level;}
    return level;
}
}
