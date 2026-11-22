#include "FusionNativeRemainingPlants269.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeWater268.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativePlacement.h"
#include "FusionNativeMoney.h"
#include "FusionFireActionBatchRules.h"
#include "FusionBasePlantRules.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
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
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&GetNativePlantData(p);}
bool Host(const Plant* p)
{return Current(p)&&!p->mNetGhost&&!p->mApp->NetIsClient()&&!p->mDead&&CanApplyNativeFusionGameplay(p->mBoard);}
bool Target(const Plant* p,Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&!NativeWater268ZombieCaptured(z)&&FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),z->mMindControlled,z->IsDeadOrDying())&&z->mRow==p->mRow;}
bool Circle(Board* b,float x,float y,double radius,const Sexy::Rect& r)
{const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(b);return FusionFireCircleTouchesRect(x,y,radius,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio);}
void ClearTarget(Plant* p)
{auto* d=GetNativePlantData(p);d->waterTargetId=0;d->waterTargetToken=0;p->mTargetZombieID=ZOMBIEID_NULL;}
Zombie* ResolveTarget(const Plant* p)
{const auto* d=GetNativePlantData(p);if(!d->waterTargetId||!d->waterTargetToken)return nullptr;
 if(!p->mApp->NetIsClient()){auto* z=p->mBoard->ZombieTryToGet(p->mTargetZombieID);return z&&!z->mDead&&z->mNetId==d->waterTargetId&&unsigned(p->mTargetZombieID)==d->waterTargetToken?z:nullptr;}
 Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(!z->mDead&&z->mNetId==d->waterTargetId)return z;return nullptr;}
int GrabTicks(const Plant* p)
{const auto* t=FindFusionRemaining269Timing(1175,true);return int(std::ceil(FusionShooterDuration(*t,GetNativePlantData(p)->animationSpeed)*100))+1;}
void GrabSearch(Plant* p)
{
    auto* d=GetNativePlantData(p);if(d->batchMode||p->mStateCountdown||p->mDoSpecialCountdown)return;float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(Target(p,z)&&Circle(p->mBoard,x,y-float(GetNativeFusionPixelsY(p->mBoard)),3,z->GetZombieRect()))
    {p->mTargetZombieID=p->mBoard->ZombieGetID(z);d->waterTargetId=z->mNetId;d->waterTargetToken=unsigned(p->mTargetZombieID);d->batchMode=2;p->mDoSpecialCountdown=GrabTicks(p);break;}
    // Source OrderBy results are discarded. The original list's first actor
    // is selected; do not silently replace this with nearest/strongest.
}
void GrabHit(Plant* p)
{
    if(auto* z=ResolveTarget(p))
    {
        bool source313=false;const bool knockAllowed=!NativeZombie313StatusTarget(z,FusionExtendedEffect313::Knockback,source313)||source313;
        float x=0,y=0;if(knockAllowed&&z->GetShadowCenter(x,y))
        {auto* b=p->mBoard;const double origin=b->GetCellCenterX(0,z->mRow),sx=(x-origin)/kFusionBasePixelsX-4.8;
         const double right=1.35*(b->GetColumnCount()-1)-4.8+4.;double moved=sx;
         if(TryGetFusionFireKnockBack(sx,right,1.5,moved)){z->mPosX+=float((moved-sx)*kFusionBasePixelsX);z->mX=int(z->mPosX);}}
        z->TakeDamage(100,2U);if(!z->mDead)SetNativeWater268Grap(z);
        p->mApp->PlaySample(Sexy::SOUND_KERNELPULT);
        ClearTarget(p);GetNativePlantData(p)->batchMode=3;
    }
}
void Puff(Plant* p)
{
    // Serialized source shoot Transform4168775426067361 is month2back.
    auto* b=p->mBoard;float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"month2back",x,y)||!EnsureNativeProjectileVisuals(b,9)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return;
    auto* q=b->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,p->mRow,PROJECTILE_PUFF);if(!q)return;
    q->mMotionType=MOTION_STRAIGHT;NativeProjectileData d;d.kind=9;d.damage=20;d.specialType=0;
    if(!AttachNativeProjectile(b,q,d)){q->Die();return;}q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=q->mVelY=q->mVelZ=q->mAccZ=0;
    q->mVelX=float(6*kFusionBasePixelsX*.01);q->mShadowY=y;q->mRotation=q->mRotationSpeed=0;q->mX=int(q->mPosX);q->mY=int(q->mPosY);
    ++GetNativePlantData(p)->batchShotCount;
}
Plant* Certain(Plant* source,int col,uint16_t kind)
{Plant* p=nullptr;while(source->mBoard->IteratePlants(p))if(const auto* d=GetNativePlantData(p))if(p!=source&&!p->mDead&&p->mRow==source->mRow&&p->mPlantCol==col&&d->kind==kind)return p;return nullptr;}
void Mix(Plant* source)
{
    float x=0,y=0;if(!source->GetShadowCenter(x,y))return;Plant* p=nullptr;
    while(source->mBoard->IteratePlants(p))if(const auto* d=GetNativePlantData(p))
        if(p!=source&&!p->mDead&&p->mPlantCol==source->mPlantCol&&p->mRow==source->mRow)
        {uint16_t left=0,right=0,result=0;if(!TryGetFusionMixBomb269Recipe(d->kind,left,right,result))continue;
         auto* l=Certain(source,p->mPlantCol-1,left);auto* r=Certain(source,p->mPlantCol+1,right);
         if(l&&r){float px=0,py=0;p->GetShadowCenter(px,py);if(!CanCreateNativeRemaining269DroppedCard(source->mBoard,result))return;
          source->Die();if(CreateNativeRemaining269DroppedCard(source,result,px,py-float(GetNativeFusionPixelsY(source->mBoard)))){r->Die();l->Die();}return;}}
    if(!CanCreateNativeRemaining269DroppedCard(source->mBoard,244))return;source->Die();CreateNativeRemaining269DroppedCard(source,244,x,y-float(GetNativeFusionPixelsY(source->mBoard)));
}
void Imitate(Plant* source)
{
    uint16_t kind=245;Plant* p=nullptr;while(source->mBoard->IteratePlants(p))if(const auto* d=GetNativePlantData(p))
        if(p!=source&&!p->mDead&&p->mRow==source->mRow&&p->mPlantCol==source->mPlantCol)
        {bool air=false,pumpkin=false,pot=false,lily=false;if(GetNativeRemaining269PlantTags(p,air,pumpkin,pot,lily)&&FusionRemaining269Imitable(d->kind,air,pumpkin,pot,lily))kind=d->kind;}
    float x=0,y=0;if(!source->GetShadowCenter(x,y)||!CanCreateNativeRemaining269DroppedCard(source->mBoard,kind))return;
    source->Die();CreateNativeRemaining269DroppedCard(source,kind,x,y-float(.5*GetNativeFusionPixelsY(source->mBoard)));
}
void Endo(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return;y-=float(.4*GetNativeFusionPixelsY(p->mBoard));
    std::vector<uint32_t> hits;Projectile* q=nullptr;while(p->mBoard->IterateProjectiles(q))if(!q->mDead&&GetNativeProjectileData(q)&&Circle(p->mBoard,x,y,.75,q->GetProjectileRect()))hits.push_back(p->mBoard->mProjectiles.DataArrayGetID(q));
    for(auto id:hits)if((q=p->mBoard->mProjectiles.DataArrayTryToGet(id))&&!q->mDead)
    {auto* d=GetNativeProjectileData(q);if(!d)continue;float px=0,py=0;p->GetShadowCenter(px,py);
     if(p->mPlantHealth>0)EmitNativeMoneyItem(p->mBoard,px,py,0);
     d->hitLatched=true;ApplyNativeSourcePlantDamage(p,200,0);q->Die();if(p->mDead)break;}
}
}
bool IsNativeRemaining269Plant(const Plant* p)
{return Current(p)&&FusionRemaining269ControllerKind(GetNativePlantData(p)->kind);}
bool InitializeNativeRemaining269Plant(Plant* p)
{
    if(!IsNativeRemaining269Plant(p)||p->mNetGhost||p->mApp->NetIsClient())return false;auto* d=GetNativePlantData(p);const auto* stats=FusionFindPlant(d->kind);if(!stats)return false;
    p->mPlantHealth=p->mPlantMaxHealth=stats->maxHealth;p->SetSleeping(false);p->mWakeUpCounter=0;p->mState=STATE_NOTREADY;
    p->mStateCountdown=p->mShootingCounter=p->mDoSpecialCountdown=p->mLaunchCounter=0;p->mTargetZombieID=ZOMBIEID_NULL;
    d->batchMode=d->damageStage=0;d->batchBagCount=d->batchRageTicks=0;d->batchShotCount=0;d->waterTargetId=0;d->waterTargetToken=0;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->idleSeconds=0;d->shootSeconds=-1;d->productionTicks=d->pendingProductionTicks=0;
    if(d->kind==244)p->mStateCountdown=200;if(d->kind==245)p->mStateCountdown=150;
    if(d->kind==250)BeginNativeShooterShot(p,*FindFusionRemaining269Timing(250));
    if(d->kind==1175){d->batchMode=1;p->mStateCountdown=300;TryGetFusionShooterInitialTicks(*FindFusionRemaining269Timing(1175),RandRangeFloat(.2f,.8f),p->mLaunchCounter);}
    d->productionTicks=p->mStateCountdown;return true;
}
bool UpdateNativeRemaining269Plant(Plant* p)
{
    if(!IsNativeRemaining269Plant(p))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);
    if(d->kind==254)Endo(p);
    else if(d->kind==244&&!p->mStateCountdown)Mix(p);
    else if(d->kind==245&&!p->mStateCountdown&&!d->batchMode){d->batchMode=1;BeginNativeShooterShot(p,*FindFusionRemaining269Timing(245));}
    else if(d->kind==1175)
    {
        if(d->batchMode==1&&!p->mStateCountdown)d->batchMode=0;
        if(p->mDoSpecialCountdown>0)--p->mDoSpecialCountdown;
        if(d->batchMode==2&&!p->mDoSpecialCountdown&&!ResolveTarget(p)){ClearTarget(p);d->batchMode=0;}
        GrabSearch(p);
        if(!p->mStateCountdown&&--p->mLaunchCounter<0)
        {p->mLaunchCounter=int(std::ceil(150*RandRangeFloat(.95f,1.05f)));Zombie* z=nullptr;float px=0,py=0;p->GetShadowCenter(px,py);
         while(p->mBoard->IterateZombies(z))if(Target(p,z)){float x=0,y=0;z->GetShadowCenter(x,y);if(x>px&&x<p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX){BeginNativeShooterShot(p,*FindFusionRemaining269Timing(1175));break;}}}
    }
    d->productionTicks=p->mStateCountdown;d->pendingProductionTicks=p->mDoSpecialCountdown;return true;
}
void UpdateNativeRemaining269Phase(Plant* p)
{if(!IsNativeRemaining269Plant(p)||!Host(p))return;auto* d=GetNativePlantData(p);if(const auto* t=FindFusionRemaining269Timing(d->kind))UpdateNativeShooterPhase(p,*t);else d->idleSeconds+=.01f;}
bool DispatchNativeRemaining269Events(Plant* p)
{
    if(!IsNativeRemaining269Plant(p))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);const auto* t=FindFusionRemaining269Timing(d->kind);
    if(t&&d->shootSeconds>=0)for(size_t i=0;i<t->eventClipSeconds.size();++i)
    {bool due=false;if(!TryGetFusionShooterEventAtCounter(*t,d->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
     if(d->kind==245)Imitate(p);
     else if(d->kind==250)
     {uint16_t kind=0;const bool water=p->mBoard->mPlantRow[p->mRow]==PLANTROW_POOL;TryChooseFusionPresentZombie269(water,water?0:RandRangeInt(0,2),kind);
      if(!FindFusionZombieProfile(kind)||!EnsureNativeSourceZombieVisuals(p->mBoard,kind)){LawnNet::NetTrace("Fusion Remaining269 BLOCKED PresentZombie requiredNPC=%u",kind);continue;}
      float x=0,y=0;p->GetShadowCenter(x,y);NativeSourceZombieSpawnRequest request;request.kind=kind;request.row=p->mRow;request.positioned=true;request.shadowX=x;request.friendly=false;request.withEffect=false;
      p->Die();SpawnNativeSourceZombie(p->mBoard,request);}
     else if(d->kind==1175)Puff(p);}
    if(d->kind==1175&&d->batchMode==2&&p->mDoSpecialCountdown>0)
    {bool due=false;const auto* hand=FindFusionRemaining269Timing(1175,true);if(TryGetFusionShooterEventAtCounter(*hand,d->animationSpeed,p->mDoSpecialCountdown,0,due)&&due)GrabHit(p);}
    return true;
}
void NativeRemaining269PlantRemoving(Plant* p,int reason)
{if(!Current(p)||p->mApp->NetIsClient())return;const auto* d=GetNativePlantData(p);if(d->kind==254&&reason>=0&&reason<=7){float x=0,y=0;if(p->GetShadowCenter(x,y))CreateNativeRemaining269Fertilize(p,x,y-float(GetNativeFusionPixelsY(p->mBoard)));}}
void NativeRemaining269ZombieRemoving(Zombie* z)
{if(!z||!z->mBoard||z->mApp->NetIsClient()||!IsNativeFusionBoard(z->mBoard))return;Plant* p=nullptr;while(z->mBoard->IteratePlants(p))if(auto* d=GetNativePlantData(p))if(!p->mDead&&d->kind==1175&&d->waterTargetId==z->mNetId&&d->waterTargetToken==unsigned(z->mBoard->ZombieGetID(z)))ClearTarget(p);}
bool GetNativeRemaining269VisualPose(const Plant* p,const char*& state,float& seconds)
{if(!IsNativeRemaining269Plant(p))return false;const auto* d=GetNativePlantData(p);const auto* t=FindFusionRemaining269Timing(d->kind);
 if(d->kind==1175){if(GetNativeSeaShroom269GrabPose(p,state,seconds))return true;state=d->idleSeconds<double(2.25f)/d->animationSpeed?"Fangzhi":"Idle";seconds=d->idleSeconds;return true;}
 state=d->shootSeconds>=0&&t?t->visualState:"idle";seconds=d->shootSeconds>=0&&t?d->shootSeconds:d->idleSeconds;return true;}
bool GetNativeSeaShroom269GrabPose(const Plant* p,const char*& state,float& seconds)
{if(!Current(p)||GetNativePlantData(p)->kind!=1175||p->mDoSpecialCountdown<=0)return false;state="Attack";seconds=(GrabTicks(p)-p->mDoSpecialCountdown)*.01f;return true;}
bool GetNativeSeaShroom269GrabAnchor(const Plant* p,float& x,float& y)
{if(!Current(p)||GetNativePlantData(p)->kind!=1175)return false;auto* z=ResolveTarget(p);return z&&z->GetShadowCenter(x,y);}
bool GetNativeRemaining269VisualLayer(const Plant* p,unsigned layer,const char*& state,float& seconds)
{if(!Current(p)||GetNativePlantData(p)->kind!=1175)return false;const auto* d=GetNativePlantData(p);if(layer==1){state="water";seconds=d->idleSeconds;return true;}
 if(layer==2&&d->shootSeconds>=0){state="shoot";seconds=d->shootSeconds;return true;}return false;}
bool NativeRemaining269Invulnerable(const Plant* p)
{return Current(p)&&GetNativePlantData(p)->kind==241;}
uint16_t NativeRemaining269SnowGatlingPayload(const Plant* source)
{bool bean=false;if(Current(source)){Plant* p=nullptr;while(source->mBoard->IteratePlants(p))if(const auto* d=GetNativePlantData(p))if(!p->mDead&&d->kind==241&&p->mRow==source->mRow&&p->mPlantCol==source->mPlantCol)bean=true;}return FusionRemaining269SnowGatlingPayload(bean);}
bool CapNativeRemaining269Damage(const Plant* p,int& damage)
{return Current(p)&&GetNativePlantData(p)->kind==1151&&TryCapFusionSuperMachine269Damage(p->mPlantMaxHealth,damage,damage);}
void NativeRemaining269HealthChanged(Plant* p)
{if(Current(p)&&!p->mNetGhost&&!p->mApp->NetIsClient()&&GetNativePlantData(p)->kind==1151)p->mPlantHealth=(std::min)(p->mPlantHealth,1000000000);}
bool TryUpgradeNativeHamburger269(Plant* p,uint16_t donor)
{if(!Host(p)||GetNativePlantData(p)->kind!=243)return false;auto* d=GetNativePlantData(p);uint8_t next=0;int maximum=0;float interval=0;
 if(!TryAdvanceFusionHamburger269(uint8_t(d->batchBagCount),donor,next,maximum,interval))return false;d->batchBagCount=next;p->mLaunchRate=int(interval*100);p->mPlantMaxHealth=maximum;p->mPlantHealth=(std::min)(maximum,p->mPlantHealth+maximum);NativePlantHealthChanged(p);return true;}
bool GetNativeBigChomper269Range(const Plant* p,bool area,float& x,float& y,float& width,float& height)
{if(!Current(p)||GetNativePlantData(p)->kind!=1174||!const_cast<Plant*>(p)->GetShadowCenter(x,y))return false;x+=float(2*kFusionBasePixelsX);y-=float(.5*GetNativeFusionPixelsY(p->mBoard));width=float((area?5.5:3)*kFusionBasePixelsX);height=float(1.5*GetNativeFusionPixelsY(p->mBoard));return true;}
bool ApplyNativeBigChomper269Bite(Plant* p,Zombie* primary)
{
    if(!Host(p)||GetNativePlantData(p)->kind!=1174||!primary||primary->mDead)return false;
    if(FusionBigChomper269Boss(uint16_t(GetNativeFusionZombieKind(primary)))){primary->TakeDamage(40,1U);return false;}
    float x=0,y=0,w=0,h=0;if(!GetNativeBigChomper269Range(p,true,x,y,w,h))return false;std::vector<ZombieID> victims;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(!z->mDead&&!NativeWater268ZombieCaptured(z)&&FusionBigChomper269AreaTarget(GetNativeFusionZombieStatus(z),z->mRow==p->mRow,z->mMindControlled,z->IsDeadOrDying(),uint16_t(GetNativeFusionZombieKind(z))))
    {const auto r=z->GetZombieRect();if(x-w*.5<=r.mX+r.mWidth&&x+w*.5>=r.mX&&y-h*.5<=r.mY+r.mHeight&&y+h*.5>=r.mY)victims.push_back(p->mBoard->ZombieGetID(z));}
    for(auto id:victims)if((z=p->mBoard->ZombieTryToGet(id)))z->DieWithLoot();
    p->mStateCountdown=int(std::ceil((8+3*RandRangeFloat(0,1))*100));ClearTarget(p);return true;
}
}
