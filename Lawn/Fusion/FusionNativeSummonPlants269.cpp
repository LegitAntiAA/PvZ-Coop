#include "FusionNativeTravelPlantActions369.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionBasePlantRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionNative.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeMoney.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeVisual.h"
#include "FusionNativeCold.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeChomper268.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeStar269.h"
#include "FusionNativeWater268.h"
#include "FusionDefenderRules.h"
#include "FusionFireRules.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Projectile.h"
#include "../Board.h"
#include "../GridItem.h"
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
bool Host(const GameObject* o){return o&&o->mApp&&o->mBoard&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&IsNativeFusionBoard(o->mBoard);}
bool Playing(const GameObject* o){return Host(o)&&!o->mBoard->mPaused&&o->mApp->mGameScene==SCENE_PLAYING;}
bool Wet(const Plant* p){return p->mBoard->mPlantRow[p->mRow]==PLANTROW_POOL;}
void Heal(Plant* p,int amount)
{if(amount<=0||RecoverNativeChomper268(p,amount))return;p->mPlantHealth=int((std::min)(int64_t(p->mPlantMaxHealth),int64_t(p->mPlantHealth)+amount));NativePlantHealthChanged(p);}
bool Summon(Plant* p,FusionSummon269Choice choice,int row=-1,float x=std::numeric_limits<float>::quiet_NaN())
{
    float px=0,py=0;if(!p->GetShadowCenter(px,py))return false;
    auto* z=TrySpawnNativeSummon269(p,choice,row<0?p->mRow:row,std::isfinite(x)?x:px);
    if(!z){LawnNet::NetTrace("Fusion SummonPlants269 BLOCKED parent=%u kind=%u requiredNPC=%u",p->mNetId,GetNativePlantData(p)->kind,choice.kind);return false;}
    ++GetNativePlantData(p)->batchShotCount;return true;
}
Coin* Weapon(const Plant* p)
{
    Coin* c=nullptr;const auto* pd=GetNativePlantData(p);if(!pd)return nullptr;
    while(p->mBoard->IterateCoins(c))if(const auto* d=GetNativeEquipmentData(c))
        if(!c->mDead&&d->weapon&&d->sourcePlantId==p->mNetId&&d->sourcePlantToken==pd->instanceToken)return c;
    return nullptr;
}
Plant* Parent(Coin* c)
{
    const auto* d=GetNativeEquipmentData(c);if(!d)return nullptr;Plant* p=nullptr;
    while(c->mBoard->IteratePlants(p))if(const auto* pd=GetNativePlantData(p))
        if(!p->mDead&&pd->kind==1166&&p->mNetId==d->sourcePlantId&&pd->instanceToken==d->sourcePlantToken)return p;
    return nullptr;
}
void RetargetWeapon(Plant* p,Coin* c)
{
    auto* d=GetNativeEquipmentData(c);Zombie* chosen=nullptr;Zombie* z=nullptr;double best=0;float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    while(p->mBoard->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&!NativeWater268ZombieCaptured(z)&&z->mRow==p->mRow)
    {float zx=0,zy=0;if(!z->GetShadowCenter(zx,zy))continue;const double at=std::hypot((zx-x)/kFusionBasePixelsX,(zy-y)/GetNativeFusionPixelsY(p->mBoard));
     if(!chosen||at<best){chosen=z;best=at;}}
    if(chosen){const auto r=chosen->GetZombieRect();d->pullEndX=float(r.mX+r.mWidth*.5);d->pullEndY=float(r.mY+r.mHeight*.5);GetNativePlantData(p)->batchBagCount=uint16_t(chosen->mRow);}
}
void CherryImpact(Plant* p,float x,float y,int row)
{
    ApplyNativeSourceCherry269ExplosionAt(p->mBoard,x,y,row,2);
}
void GoldCapture(Plant* p)
{
    auto* pd=GetNativePlantData(p);Projectile* q=nullptr;
    while(p->mBoard->IterateProjectiles(q))
    {
        if(q->mDead)continue;
        auto* d=GetNativeProjectileData(q);
        // The ordinary native Peashooter emits an untyped source0 body.
        // Adopt that same actor before applying the shared Fusion conversion.
        if(!d&&q->mProjectileType==PROJECTILE_PEA&&q->mMotionType==MOTION_STRAIGHT&&
           q->GetProjectileDef().mDamage==20&&
           FusionGoldMagnet269Eligible(q->mProjectileType,q->mMotionType,0,false,q->mRow-p->mRow,q->mVelY))
        {
            if(!EnsureNativeProjectileVisuals(p->mBoard,0)||!AttachNativeProjectile(p->mBoard,q,NativeProjectileData{}))continue;
            // As in Torchwood adoption, preserve the stock pea's effective
            // 3.33px/tick; its untyped body kept mVelX at zero.
            q->mVelX=3.33f;
            d=GetNativeProjectileData(q);
        }
        if(!d)continue;
        if(q->mDead||!FusionGoldMagnet269Eligible(q->mProjectileType,q->mMotionType,d->specialType,d->goldAttracted,q->mRow-p->mRow,q->mVelY))continue;
        d->specialType=q->mRow<p->mRow?5:4;d->goldAttracted=true;q->mRow=p->mRow;q->mProjectileAge=0;q->mMotionType=MOTION_THREEPEATER;
        if(pd->batchShotCount<uint32_t((std::numeric_limits<int>::max)()))++pd->batchShotCount;
    }
}
bool CherryMagnetProtection(Plant* p)
{
    std::vector<Plant*> component{p};Plant* candidate=nullptr;
    for(size_t i=0;i<component.size();++i)
    {
        candidate=nullptr;
        while(p->mBoard->IteratePlants(candidate))if(const auto* d=GetNativePlantData(candidate))
            if(!candidate->mDead&&FusionEquipmentMagnetMember(d->kind)&&
               std::find(component.begin(),component.end(),candidate)==component.end()&&
               FusionEquipmentGraphAdjacent(component[i]->mPlantCol,component[i]->mRow,candidate->mPlantCol,candidate->mRow))component.push_back(candidate);
    }
    for(auto* q:component){const auto* d=GetNativePlantData(q);if(d&&(d->kind==935||d->kind==1097))return true;}return false;
}
}
bool IsNativeSummonPlants269(const Plant* p){const auto* d=GetNativePlantData(p);return d&&IsFusionSummonPlants269Kind(d->kind);}
bool InitializeNativeSummonPlants269(Plant* p)
{
    if(!IsNativeSummonPlants269(p)||!Host(p))return false;auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(d->kind);if(!definition)return false;
    p->mPlantHealth=p->mPlantMaxHealth=definition->maxHealth;p->SetSleeping(false);p->mWakeUpCounter=0;p->mState=STATE_NOTREADY;
    p->mShootingCounter=p->mLaunchCounter=p->mStateCountdown=p->mDoSpecialCountdown=0;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->idleSeconds=0;d->shootSeconds=-1;d->productionTicks=d->pendingProductionTicks=0;
    d->batchMode=0;d->batchRageTicks=d->batchBagCount=0;d->batchShotCount=0;
    if(d->kind==917){d->batchBagCount=5;p->mStateCountdown=3000;}
    if(d->kind==1147)p->mStateCountdown=4500;
    if(d->kind==1150)p->mStateCountdown=250;
    if(d->kind==1166){p->mLaunchCounter=100;p->mTargetX=0;}
    if(d->kind==928){d->batchMode=1;BeginNativeShooterShot(p,*FindFusionSummonPlants269Timing(928));}
    d->productionTicks=p->mStateCountdown;return true;
}
void UpdateNativeSummonPlants269Phase(Plant* p)
{
    if(!IsNativeSummonPlants269(p)||!Playing(p)||p->mDead)return;auto* d=GetNativePlantData(p);
    if(const auto* t=FindFusionSummonPlants269Timing(d->kind))UpdateNativeShooterPhase(p,*t);
    else
    {d->idleSeconds+=.01f;if(d->kind==1166&&d->shootSeconds>=0)
      {const float duration=FusionSummonPlants269MaximumDuration(1166,d->animationSpeed);const int initial=int(std::ceil(duration*100))+1;
       d->shootSeconds=float(initial-p->mShootingCounter)*.01f;if(d->shootSeconds>=duration)d->shootSeconds=-1;}}
    d->productionTicks=d->kind==1166?p->mTargetX:p->mStateCountdown;
}
bool UpdateNativeSummonPlants269(Plant* p)
{
    if(!IsNativeSummonPlants269(p))return false;if(!Playing(p)||p->mDead)return true;auto* d=GetNativePlantData(p);
    if(d->kind==917&&d->batchBagCount==0){p->Die();return true;}
    if((d->kind==917||d->kind==1150||d->kind==1147)&&p->mStateCountdown==0)
    {
        p->mStateCountdown=d->kind==917?3000:d->kind==1150?500:4500;d->productionTicks=p->mStateCountdown;
        if(d->kind==1147){FusionSummon269Choice c;TryChooseFusionNut269(false,Wet(p),c);Summon(p,c);}
        else BeginNativeShooterShot(p,*FindFusionSummonPlants269Timing(d->kind));
    }
    if(d->kind==1150&&d->batchMode==1)GoldCapture(p);
    if(d->kind==1166)
    {
        auto* weapon=Weapon(p);
        if(!weapon)
        {
            // Root registers1166 as the same proven Magnet capture profile.
            // Its existing TargetX is the sole inherited cooldown clock.
            if(!UpdateNativeEquipmentPlant(p)&&p->mTargetX>0)--p->mTargetX;
            if(d->shootSeconds==0&&p->mShootingCounter==0)p->mShootingCounter=int(std::ceil(FusionSummonPlants269MaximumDuration(1166,d->animationSpeed)*100))+1;
            if(auto* item=GetNativeEquipmentHeldBy(p))if(const auto* e=GetNativeEquipmentData(item))
                if(!e->weapon&&e->phase==NativeEquipmentPhase::Attached)CommitNativeCherryMagnet269Capture(p,item);
            weapon=Weapon(p);
        }
        else if(p->mTargetX>0)--p->mTargetX;
        if(weapon){if(--p->mLaunchCounter<0){p->mLaunchCounter=300;RetargetWeapon(p,weapon);}}
        else p->mLaunchCounter=100;
        d->productionTicks=p->mTargetX;
    }
    return true;
}
bool DispatchNativeSummonPlants269Events(Plant* p)
{
    if(!IsNativeSummonPlants269(p))return false;if(!Playing(p)||p->mDead)return true;auto* d=GetNativePlantData(p);
    const auto* timing=FindFusionSummonPlants269Timing(d->kind);if(!timing||d->shootSeconds<0)return true;
    for(size_t i=0;i<timing->eventClipSeconds.size();++i)
    {
        bool due=false;if(!TryGetFusionShooterEventAtCounter(*timing,d->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
        if(d->kind==928)
        {std::vector<NativeSourceZombieSpawnRequest> children;
         if(!PrepareNativeHypnoDoom369(p,children))return true;
         float x=0,y=0;p->GetShadowCenter(x,y);auto* b=p->mBoard;const int col=p->mPlantCol,row=p->mRow;
         p->Die();ApplyNativeBatchDoomAt(b,x,y,col,row,1800,true,false);CommitNativeHypnoDoom369(b,children);return true;}
        if(d->kind==917){FusionSummon269Choice c;TryChooseFusionHypno269(917,Wet(p),false,false,false,0,c);Summon(p,c);}
        if(d->kind==1157){FusionSummon269Choice c;if(TryChooseFusionMagnet269(uint8_t(d->batchBagCount),RandRangeInt(0,2),Wet(p),c))Summon(p,c);d->batchMode=0;}
        if(d->kind==1150)
        {if(i==0){d->batchMode=1;d->batchShotCount=0;}else{d->batchMode=0;for(uint32_t n=0;n<(std::min)(20U,d->batchShotCount);++n)EmitNativeMoneyItem(p->mBoard,float(p->mX+20),float(p->mY+20),38);}}
    }
    return true;
}
bool GetNativeSummonPlants269VisualPose(const Plant* p,const char*& state,float& seconds)
{if(!IsNativeSummonPlants269(p))return false;const auto* d=GetNativePlantData(p);const auto* t=FindFusionSummonPlants269Timing(d->kind);
 state=d->shootSeconds>=0?(t?t->visualState:d->kind==1166?"attrack":"idle"):d->kind==1166&&Weapon(p)?"idle2":"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return std::isfinite(seconds)&&seconds>=0;}
int NativeSummonPlants269VisualLayer(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&d->kind==917&&d->shootSeconds>=0?1:0;}
bool CanUseNativeSummonPlants269Item(const Plant* p,FusionEquipmentType type,const Coin* item)
{
    const auto* d=GetNativePlantData(p);if(!d||!Playing(p)||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0||uint8_t(type)>10)return false;
    if(d->kind==1166)
    {const auto* coin=GetNativeEquipmentData(item);return !Weapon(p)&&d->productionTicks==0&&
        (!item||(!item->mDead&&item->mBoard==p->mBoard&&coin&&!coin->weapon));}
    if(d->kind!=1157)return false;
    for(int draw=0;draw<3;++draw){FusionSummon269Choice c;if(!TryChooseFusionMagnet269(uint8_t(type),draw,Wet(p),c)||!FindFusionZombieProfile(c.kind)||!EnsureNativeSourceZombieVisuals(p->mBoard,c.kind))return false;}
    return true;
}
bool TryUseNativeSummonPlants269Item(Plant* p,FusionEquipmentType type,Coin* item)
{
    if(!CanUseNativeSummonPlants269Item(p,type,item))return false;auto* d=GetNativePlantData(p);
    if(d->kind==1157)
    {if(item&&(item->mBoard!=p->mBoard||!ConsumeNativeEquipment(item)))return false;d->batchBagCount=uint16_t(type);d->batchMode=2;return BeginNativeShooterShot(p,*FindFusionSummonPlants269Timing(1157));}
    if(!item)return true; // Source null Bucket succeeds without creating a weapon.
    if(!CommitNativeCherryMagnet269Capture(p,item))return false;p->mTargetX=1500;
    item->mVelX=float(RandRangeFloat(-4,-2)*kFusionBasePixelsX*.01);item->mVelY=float(-RandRangeFloat(-1,1)*GetNativeFusionPixelsY(p->mBoard)*.01);
    d->productionTicks=p->mTargetX;return true;
}
bool CommitNativeCherryMagnet269Capture(Plant* p,Coin* item)
{
    auto* pd=GetNativePlantData(p);auto* d=GetNativeEquipmentData(item);
    if(!pd||pd->kind!=1166||!Playing(p)||p->mDead||!item||item->mBoard!=p->mBoard||!d||d->weapon||Weapon(p))return false;
    float cx=0,cy=0;if(!GetNativeSourcePlantNodePoint(p,"Pos",cx,cy))return false;
    float dx=float((cx-item->mPosX-20)/kFusionBasePixelsX),dy=float(-(cy-item->mPosY-20)/GetNativeFusionPixelsY(p->mBoard));
    const float length=std::sqrt(dx*dx+dy*dy);if(length>1e-5f){dx/=length;dy/=length;}else dx=dy=0;
    NativeEquipmentCoinRemoving268(item);ClearNativeEquipmentLease(p->mBoard,item->mNetId,d->instanceToken);
    d->weapon=true;d->phase=NativeEquipmentPhase::Attached;d->sourcePlantId=p->mNetId;d->sourcePlantToken=pd->instanceToken;
    d->claimSlot=255;d->disappear=false;d->gravity=0;d->pullEndX=d->pullEndY=0;
    constexpr float a=.2617994f;item->mVelX=float((std::cos(a)*dx-std::sin(a)*dy)*kFusionBasePixelsX*.01);
    item->mVelY=float(-(std::cos(a)*dy+std::sin(a)*dx)*GetNativeFusionPixelsY(p->mBoard)*.01);
    pd->batchMode=1;pd->batchBagCount=uint16_t(p->mRow);pd->batchShotCount=0;
    if(pd->shootSeconds<0){p->mShootingCounter=int(std::ceil(FusionSummonPlants269MaximumDuration(1166,pd->animationSpeed)*100))+1;pd->shootSeconds=0;}return true;
}
bool NativeGoldMagnet269CanTrigger(const Plant* source)
{
    if(!source||!IsNativeFusionBoard(source->mBoard))return false;Plant* p=nullptr;
    while(source->mBoard->IteratePlants(p))if(const auto* d=GetNativePlantData(p))
        if(!p->mDead&&d->kind==1150&&d->batchMode==1&&std::abs(p->mRow-source->mRow)==1)return true;
    return false;
}
bool HandleNativeSummonPlants269Eaten(Zombie* z,Plant* p)
{
    auto* d=GetNativePlantData(p);if(!d||(d->kind!=917&&d->kind!=1041))return false;
    if(!Playing(p)||p->mDead||!z||z->mBoard!=p->mBoard||z->mDead)return true;
    if(d->kind==917){z->StopEating();z->StartMindControlled();if(d->batchBagCount)--d->batchBagCount;return true;}
    float x=0,y=0,zx=0,zy=0;p->GetShadowCenter(x,y);z->GetShadowCenter(zx,zy);const int row=z->mRow;
    ApplyNativeColdBoardFreeze(p->mBoard,x,y);p->Die();const int kind=GetNativeFusionZombieKind(z);if(kind!=218&&kind!=220)z->DieWithLoot();
    FusionSummon269Choice c;TryChooseFusionIceHypno269(p->mBoard->mPlantRow[row]==PLANTROW_POOL,c);Summon(p,c,row,zx);return true;
}
bool ReceiveNativeHypnoNut269Color(Plant* p,Projectile* q)
{
    auto* d=GetNativePlantData(p);const auto* b=GetNativeProjectileData(q);if(!d||d->kind!=1147||!b||b->kind!=16)return false;
    if(!Playing(p)||p->mDead||q->mDead||q->mBoard!=p->mBoard)return true;uint16_t after=0;bool rainbow=false;
    if(!TryReceiveFusionHypnoNut269Color(d->batchRageTicks,b->effectVariant,after,rainbow))return true;
    q->Die();Heal(p,100);d->batchRageTicks=after;if(rainbow){FusionSummon269Choice c;TryChooseFusionNut269(true,Wet(p),c);Summon(p,c);Heal(p,p->mPlantMaxHealth);}return true;
}
Plant* NativeSummonPlants269EatingCover(Zombie* z,Plant* target)
{
    if(!z||!target||!z->mIsEating||z->mRow!=target->mRow)return target;Plant* p=nullptr;
    while(z->mBoard->IteratePlants(p))if(const auto* d=GetNativePlantData(p))
        if(!p->mDead&&d->kind==1164&&p->mPlantCol==target->mPlantCol&&p->mRow==target->mRow)return p;
    return target;
}
bool IsNativeSummonPlants269Weapon(const Coin* c){const auto* d=GetNativeEquipmentData(c);return d&&d->weapon;}
bool UpdateNativeSummonPlants269Coin(Coin* c)
{
    if(!IsNativeSummonPlants269Weapon(c))return false;if(!Playing(c)||c->mDead)return true;auto* p=Parent(c);if(!p){RetireNativeEquipment(c,false);return true;}
    auto* d=GetNativeEquipmentData(c);auto* pd=GetNativePlantData(p);float cx=0,cy=0;
    if(!GetNativeSourcePlantNodePoint(p,"Pos",cx,cy))return true;
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(c->mBoard));
    FusionCherryMagnet269Point velocity{c->mVelX/(px*.01f),-c->mVelY/(py*.01f)},after;
    const bool target=d->pullEndX!=0||d->pullEndY!=0;bool impact=false;
    if(!TryStepFusionCherryMagnet269({(c->mPosX+20)/px,-(c->mPosY+20)/py},{cx/px,-cy/py},
        {d->pullEndX/px,-d->pullEndY/py},target,velocity,after,impact))return true;
    c->mVelX=after.x*px*.01f;c->mVelY=-after.y*py*.01f;
    c->mPosX+=c->mVelX;c->mPosY+=c->mVelY;c->mX=c->mY=0;
    if(impact)
    {const float x=d->pullEndX,y=d->pullEndY;d->pullEndX=d->pullEndY=0;++pd->batchShotCount;CherryImpact(p,x,y,pd->batchBagCount);
     if(pd->batchShotCount>=5){pd->batchShotCount=0;pd->batchMode=0;p->mTargetX=(std::min)(p->mTargetX,50);pd->productionTicks=p->mTargetX;RetireNativeEquipment(c,false);}}
    return true;
}
bool MoveNativeGoldMagnet269Projectile(Projectile* q)
{
    auto* d=GetNativeProjectileData(q);if(!d||!d->goldAttracted)return false;if(!Playing(q)||q->mDead)return true;
    if((d->kind==40||d->kind==48)&&(d->specialType==4||d->specialType==5)&&q->mProjectileAge>30)
    {d->specialType=6;q->mMotionType=MOTION_HOMING;return false;}
    float vy=0;if(!TryStepFusionGoldMagnet269(d->specialType,d->isZombieBullet,q->mProjectileAge,vy))return false;
    q->mVelY=float(-vy*GetNativeFusionPixelsY(q->mBoard)*.01);q->mPosX+=q->mVelX;q->mPosY+=q->mVelY;q->mShadowY+=q->mVelY;
    q->mX=int(q->mPosX);q->mY=int(q->mPosY+q->mPosZ);
    if(const auto* s=GetNativeStar269ProjectileState(q))q->mRotationSpeed=std::fmod(q->mRotationSpeed+float(s->trackSpeed*30*.01*3.14159265358979323846/180),float(2*3.14159265358979323846));
    return true;
}
void NativeSummonPlants269Removing(Plant* p,int reason)
{
    if(!IsNativeSummonPlants269(p)||!Host(p))return;const auto* d=GetNativePlantData(p);
    if(d->kind==1166)if(auto* coin=Weapon(p))RetireNativeEquipment(coin,false);
    if(d->kind==917&&reason>=0&&reason<=7)
    {Plant* q=nullptr;while(p->mBoard->IteratePlants(q))if(const auto* qd=GetNativePlantData(q))if(!q->mDead&&qd->kind==900&&std::abs(q->mRow-p->mRow)<=1&&std::abs(q->mPlantCol-p->mPlantCol)<=1)
      LawnNet::NetTrace("Fusion SummonPlants269 BLOCKED death recipient900 controller parent=%u target=%u",p->mNetId,q->mNetId);}
}
void NativeSummonPlants269HealthChanged(Plant* p)
{
    auto* d=GetNativePlantData(p);if(!d||!Host(p)||(d->kind!=1147&&d->kind!=1164))return;
    uint8_t stage=0;if(TryGetFusionDefenderDamageStage(p->mPlantHealth,p->mPlantMaxHealth,d->damageStage,stage))d->damageStage=stage;
}
bool ApplyNativeSourceCherry269ExplosionAt(Board* b,float x,float y,int row,int variant)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!IsNativeFusionBoard(b)||
       !std::isfinite(x)||!std::isfinite(y)||row<0||row>=GetNativeFusionRowCount(b)||(variant!=0&&variant!=2))return false;
    const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(b),radius=variant==2?1.5:2.;
    std::vector<ZombieID> hits;Zombie* z=nullptr;
    while(b->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&!NativeWater268ZombieCaptured(z)&&std::abs(z->mRow-row)<2)
    {
        if(variant==2&&!FusionPuffLove269CanSplash(GetNativeFusionZombieStatus(z),z->mRow-row,false,false))continue;
        const auto r=z->GetZombieRect();if(FusionFireCircleTouchesRect(x,y,radius,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio))hits.push_back(b->ZombieGetID(z));
    }
    for(auto id:hits)if((z=b->ZombieTryToGet(id)))
    {if(GetNativeFusionZombieKind(z)==107)ApplyNativeSourceExplosionDamage(z,150);else if(variant==2)ApplyNativeSourceExplosionDamage(z,300);else ApplyNativeFusionCherryDamage(z);}
    // Friendly Explode heals its specific Cherry recipients; the hostile
    // ZombieExplode/PlantTakeDamage path below is a separate source callback.
    Plant* p=nullptr;while(b->IteratePlants(p))if(const auto* d=GetNativePlantData(p))
        if(!p->mDead&&NativeSourcePlantColliderEnabled(p)&&std::abs(p->mRow-row)<2&&
           (d->kind==903||d->kind==922||d->kind==1003||d->kind==1164||d->kind==1169))
        {const auto r=p->GetPlantRect();if(FusionFireCircleTouchesRect(x,y,radius,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio))Heal(p,variant==0?1000:100);}
    if(variant==0)
    {GridItem* item=nullptr;while(b->IterateGridItems(item))if(!item->mDead&&item->mGridItemType==GRIDITEM_LADDER&&std::abs(item->mGridY-row)<2)
        {const float cx=float(b->GetCellCenterX(item->mGridX,item->mGridY)+.3*kFusionBasePixelsX),cy=float(b->GridToPixelY(item->mGridX,item->mGridY)+50-.6*GetNativeFusionPixelsY(b));
         if(FusionFireCircleTouchesRect(x,y,2,cx-.98*.7*kFusionBasePixelsX*.5,y+(cy-y)*ratio-2.09*.7*kFusionBasePixelsY*.5,.98*.7*kFusionBasePixelsX,2.09*.7*kFusionBasePixelsY))item->GridItemDie();}}
    b->mApp->AddTodParticle(x,y,static_cast<int>(RENDER_LAYER_TOP),PARTICLE_POWIE);
    LawnNet::NetBroadcastParticle(b->mApp,x,y,static_cast<int>(RENDER_LAYER_TOP),PARTICLE_POWIE);
    b->mApp->PlayFoley(FOLEY_CHERRYBOMB);if(variant==0)b->ShakeBoard(3,-4);
    LawnNet::NetTrace("Fusion SourceCherry269 variant=%d row=%d x=%.9g y=%.9g radius=%.9g targets=%u tick=%d",variant,row,x,y,radius,unsigned(hits.size()),b->mMainCounter);return true;
}
void ApplyNativeSummonPlants269CherryArea(Board* b,float x,float y,int row)
{
    if(!b||!IsNativeFusionBoard(b)||b->mApp->NetIsClient())return;const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(b);
    Plant* p=nullptr;std::vector<uint32_t> hits;
    while(b->IteratePlants(p))if(!p->mDead&&NativeSourcePlantColliderEnabled(p)&&std::abs(p->mRow-row)<2)
    {const auto r=p->GetPlantRect();if(FusionFireCircleTouchesRect(x,y,1.5,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio))hits.push_back(b->mPlants.DataArrayGetID(p));}
    for(auto id:hits)if((p=b->mPlants.DataArrayTryToGet(id)))if(const auto* d=GetNativePlantData(p))
    {
        if(d->kind==903||d->kind==922||d->kind==1164||d->kind==1169||d->kind==1003){Heal(p,200);continue;}
        if(d->kind==921||d->kind==1095)continue;
        bool protectedCell=false;Plant* cover=nullptr;while(b->IteratePlants(cover))if(const auto* cd=GetNativePlantData(cover))
          if(!cover->mDead&&cover->mRow==p->mRow&&cover->mPlantCol==p->mPlantCol&&(cd->kind==922||cd->kind==1164||cd->kind==1169))protectedCell=true;
        bool half=false;
        for(uint16_t k:{935,1020,1028,1029,1096,1097,1129,1151,1152,1159,1160})if(d->kind==k)half=true;
        if(d->kind==911&&CherryMagnetProtection(p))half=true;
        if(!protectedCell&&p->mSeedType!=SEED_LILYPAD&&!Plant::IsFlying(p->mSeedType)&&p->mSeedType!=SEED_SPIKEWEED&&p->mSeedType!=SEED_SPIKEROCK)ApplyNativeSourcePlantDamage(p,half?500:1000,1);
    }
}
}
