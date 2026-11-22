#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeLastPlants269.h"
#include "FusionBasePlantRules.h"
#include "FusionNative.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionSummonPlants269Rules.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativePlacement.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeWater268.h"
#include "FusionWater268Rules.h"
#include "FusionNativeFire.h"
#include "FusionFireRules.h"
#include "FusionNativeEffects269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
template<class T> bool Playing(const T* p){return Host(p)&&!p->mDead&&!p->mBoard->mPaused&&CanApplyNativeFusionGameplay(p->mBoard);}
bool Wet(const Plant* p){return p->mBoard->mPlantRow[p->mRow]==PLANTROW_POOL;}
bool CanNpc(Board* b,uint16_t kind)
{return FindFusionZombieProfile(kind)&&GetNativeSourceZombieCarrier(kind)>=0&&EnsureNativeSourceZombieVisuals(b,kind);}
bool EmperorRosterReady(Board* b,int row,bool shooting,bool advanced,bool balanced)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||!IsNativeFusionBoard(b)||
       row<0||row>=GetNativeFusionRowCount(b)||!b->RowCanHaveZombies(row)||
       b->mZombies.mSize>=b->mZombies.mMaxSize-1)return false;
    const auto terrain=b->mPlantRow[row];
    if(terrain!=PLANTROW_NORMAL&&terrain!=PLANTROW_POOL)return false;
    for(int draw=0;draw<4;++draw)
    {
        FusionSummon269Choice c;
        if(!TryChooseFusionHypno269(900,terrain==PLANTROW_POOL,shooting,advanced,balanced,draw,c))return false;
        const auto* profile=FindFusionZombieProfile(c.kind);
        if(!profile||(profile->waterOnly&&terrain!=PLANTROW_POOL)||
           ((c.kind==18||c.kind==109)&&terrain==PLANTROW_POOL)||!CanNpc(b,c.kind))return false;
        // The real factory refuses these vehicles without their source death FX.
        // Preflight the complete original roster before consuming the RNG draw.
        if((c.kind==14||c.kind==18)&&!EnsureNativeSourceEffect269(b,c.kind==14?35:36))return false;
    }
    return true;
}
bool EmperorReady(Plant* p)
{
    return p&&EmperorRosterReady(p->mBoard,p->mRow,NativeLastPlants269Shooting(p->mBoard),
        NativeLastPlants269Advanced(p,1),NativeLastPlants269Balanced(p->mBoard));
}
struct GoldBirth {uint16_t kind;int row,award;float x;};
bool GoldPlan(Plant* p,int draw,std::vector<GoldBirth>& out)
{
    FusionGoldHypno269Plan plan;if(!TryPlanFusionGoldHypno269(int(p->mBoard->mBackground),false,draw,plan))return false;
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
    const float entry=float(p->mBoard->GetCellCenterX(0,0)+(9.9+4.8)*kFusionBasePixelsX);
    out.clear();for(int i=0;i<plan.count;++i)
    {
        out.push_back({plan.bosses[i],p->mRow,2,x});uint16_t minion=0;
        if(TryGetFusionGoldHypno269Minion(plan.bosses[i],minion))for(int row=0;row<GetNativeFusionRowCount(p->mBoard);++row)out.push_back({minion,row,1,entry});
    }
    return true;
}
bool GoldReady(Plant* p)
{
    if(NativeLastPlants269GoldSummoned(p->mBoard))return true;
    if(!EnsureNativeLastPlants269BirthEffect(p->mBoard))return false;
    std::vector<GoldBirth> births;
    for(int draw=0;draw<7;++draw)
    {
        if(!GoldPlan(p,draw,births)||int(births.size())>p->mBoard->mZombies.mMaxSize-p->mBoard->mZombies.mSize)return false;
        for(const auto& b:births)if(!CanNpc(p->mBoard,b.kind)||!CanApplyNativeLastPlants269GoldPost(p->mBoard,b.kind))return false;
    }
    return true;
}
bool EmperorSummon(Plant* p)
{
    float x=0,y=0;if(!p->GetShadowCenter(x,y)||!EmperorReady(p))return false;
    const bool water=Wet(p),balanced=NativeLastPlants269Balanced(p->mBoard);
    // Source performs no random draw for water or balanced callbacks.
    FusionSummon269Choice choice;
    if(!TryChooseFusionHypno269(900,water,NativeLastPlants269Shooting(p->mBoard),NativeLastPlants269Advanced(p,1),balanced,water||balanced?0:RandRangeInt(0,3),choice))return false;
    auto* child=TrySpawnNativeSummon269(p,choice,p->mRow,x);
    const auto* d=GetNativePlantData(p);
    LawnNet::NetTrace("Fusion Emperor269 summon parent=%u token=%u child=%u kind=%u row=%d post=%u tick=%d",
        p->mNetId,d->instanceToken,child?child->mNetId:0,choice.kind,p->mRow,unsigned(choice.post),p->mBoard->mMainCounter);
    return child!=nullptr;
}
bool GoldSummon(Plant* p)
{
    if(NativeLastPlants269GoldSummoned(p->mBoard))
    {GetNativePlantData(p)->batchMode=1;NotifyNativeLastPlants269GoldAlreadySummoned(p);return true;}
    if(!GoldReady(p))return false;
    const int background=int(p->mBoard->mBackground);
    const bool deterministic=background==0||background==1||background==2||background==3||background==4||background==5||background==6||background==7||background==10||background==12||background==13;
    std::vector<GoldBirth> births;if(!GoldPlan(p,deterministic?0:RandRangeInt(0,6),births))return false;
    Board* board=p->mBoard;float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;const int row=p->mRow;
    MarkNativeLastPlants269GoldSummoned(board);p->Die();
    for(const auto& b:births)
    {
        if(b.award==2)CreateNativeLastPlants269BirthEffect(board,x,y-float(.5*GetNativeFusionPixelsY(board)),row);
        NativeSourceZombieSpawnRequest request;request.kind=b.kind;request.row=b.row;request.positioned=true;request.shadowX=b.x;request.friendly=false;request.withEffect=false;
        auto* z=SpawnNativeSourceZombie(board,request);
        if(z)ApplyNativeLastPlants269GoldPost(z,b.award);
        else LawnNet::NetTrace("Fusion Last269 unexpected Gold spawn failure kind=%u row=%d after full preflight",b.kind,b.row);
    }
    return true;
}
bool KelpSupplyReady(Plant* p)
{return EnsureNativeLastPlants269TorchConversions(p->mBoard)&&GetNativeSeedForPlant(1156)!=SEED_NONE&&EnsureNativePlantVisuals(p->mBoard,1156)&&EnsureNativeLastPlants269BirthEffect(p->mBoard);}
void KelpSupply(Plant* p)
{
    NativePlantData provenance;provenance.kind=1156;provenance.owner=GetNativePlantData(p)->owner;
    for(int col=p->mPlantCol+1;col<p->mBoard->GetColumnCount();++col)
        if(auto* child=TrySpawnNativeFusionPlant(p->mBoard,col,p->mRow,provenance))
        {float x=0,y=0;if(child->GetShadowCenter(x,y))CreateNativeLastPlants269BirthEffect(p->mBoard,x,y,p->mRow);return;}
}
bool TorchTouches(Plant* p,Projectile* q)
{
    float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"",x,y))return false;
    // Both enabled original root BoxCollider2D entries, actual root scale.33.
    constexpr float box[][4]={{-.09225941f,1.7252482f,4.5058517f,3.380287f},{.09932679f,2.2390742f,2.8872008f,4.1724463f}};
    const auto r=q->GetProjectileRect();const double px=.33*kFusionBasePixelsX,py=.33*GetNativeFusionPixelsY(p->mBoard);
    for(const auto& b:box){const double left=x+(b[0]-b[2]*.5)*px,top=y-(b[1]+b[3]*.5)*py;
        if(left<=r.mX+r.mWidth&&left+b[2]*px>=r.mX&&top<=r.mY+r.mHeight&&top+b[3]*py>=r.mY)return true;}
    return false;
}
bool LiveTarget(Zombie* z)
{return z&&!z->mDead&&GetNativeFusionZombieHealth(z)>0&&!z->mMindControlled&&!NativeWater268ZombieCaptured(z);}
bool Sputter(Zombie* z)
{return FusionLastPlants269KelpSputterAllowed(int(z->mShieldType),GetNativeFusionZombieKind(z));}
void FireKelpImpactEffect(Projectile* p)
{
    // Same native-compatible fire-impact adapter as NativeFire; source FX33
    // Unity trail/shader appearance remains a separately recorded visual debt.
    const float x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;
    p->mApp->AddTodParticle(x,y,p->mRenderOrder+1,PARTICLE_FIREBALL_DEATH);
    LawnNet::NetBroadcastParticle(p->mApp,x,y,p->mRenderOrder+1,PARTICLE_FIREBALL_DEATH);
}
}
bool CanPlaceNativeEmperor269(Board* b,int row)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||!IsNativeFusionBoard(b))return false;
    // Normal D2 has no per-plant Advanced trait. Any active board mode still
    // follows its complete source roster; unsupported choices fail as a group.
    return EmperorRosterReady(b,row,NativeLastPlants269Shooting(b),HasNativeTravelAdvanced369(b,1),NativeLastPlants269Balanced(b));
}
bool IsNativeLastPlants269(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&IsNativeFusionBoard(p->mBoard)&&IsFusionLastPlants269Kind(d->kind);}
bool CanCreateNativeLastPlants269(Plant* p)
{
    if(!IsNativeLastPlants269(p)||!Host(p))return false;const auto kind=GetNativePlantData(p)->kind;
    if(kind==918)return true;
    if(kind==900)return EmperorReady(p);if(kind==938)return GoldReady(p);
    if(kind==1056)return IsNativeWater268ImplementedKind(1056)&&KelpSupplyReady(p)&&EnsureNativeLastPlants269TorchConversions(p->mBoard);
    return CanReplaceNativeLastPlants269Wheat(p);
}
bool InitializeNativeLastPlants269(Plant* p)
{
    if(!CanCreateNativeLastPlants269(p))return false;auto* d=GetNativePlantData(p);
    if(d->kind==1056)return InitializeNativeWater268Plant(p);
    const auto* stats=FusionFindPlant(d->kind);if(!stats||!stats->hasStats)return false;
    p->mPlantHealth=p->mPlantMaxHealth=stats->maxHealth;p->SetSleeping(false);p->mWakeUpCounter=0;
    p->mShootingCounter=p->mStateCountdown=p->mDoSpecialCountdown=p->mLaunchCounter=0;p->mState=STATE_NOTREADY;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->idleSeconds=0;d->shootSeconds=-1;d->batchMode=d->damageStage=0;
    d->batchShotCount=d->batchBagCount=d->batchRageTicks=0;d->productionTicks=d->pendingProductionTicks=0;d->waterTargetId=0;d->waterTargetToken=0;
    if(d->kind==900)
    {
        d->batchBagCount=5;p->mStateCountdown=NativeLastPlants269EmperorFastStart(p->mBoard)?1000:3000;
        LawnNet::NetTrace("Fusion Emperor269 init parent=%u token=%u row=%d remaining=5 countdown=%d tick=%d",
            p->mNetId,d->instanceToken,p->mRow,p->mStateCountdown,p->mBoard->mMainCounter);
    }
    if(d->kind==938)p->mStateCountdown=151;
    if(d->kind==253)p->mLaunchCounter=FusionLastPlants269WheatTicks(NativeLastPlants269WheatFastStart(p->mBoard));
    d->productionTicks=d->kind==253?p->mLaunchCounter:p->mStateCountdown;return true;
}
bool UpdateNativeLastPlants269(Plant* p)
{
    if(!IsNativeLastPlants269(p))return false;if(!Playing(p))return true;auto* d=GetNativePlantData(p);
    if(d->kind==1056)
    {
        UpdateNativeWater268Plant(p);if(p->mDead)return true;
        std::vector<uint32_t> ids;Projectile* q=nullptr;while(p->mBoard->IterateProjectiles(q))if(!q->mDead)ids.push_back(p->mBoard->mProjectiles.DataArrayGetID(q));
        for(auto id:ids)if(auto* q=p->mBoard->mProjectiles.DataArrayTryToGet(id))if(!q->mDead&&TorchTouches(p,q))HandleNativeLastPlants269TorchContact(p,q);
        return true;
    }
    if(d->kind==900)
    {
        if(!d->batchBagCount)
        {
            LawnNet::NetTrace("Fusion Emperor269 exhausted parent=%u token=%u remaining=0 tick=%d",p->mNetId,d->instanceToken,p->mBoard->mMainCounter);
            p->Die();return true;
        }
        if(!p->mStateCountdown&&EmperorReady(p))
        {float seconds=0;if(TryGetFusionSummon269Interval(900,NativeLastPlants269Shooting(p->mBoard),NativeLastPlants269Advanced(p,0),seconds))
         {if(BeginNativeShooterShot(p,*FindFusionLastPlants269Timing(900)))
          {p->mStateCountdown=int(seconds*100);d->batchMode=1;
           LawnNet::NetTrace("Fusion Emperor269 trigger parent=%u token=%u countdown=%d tick=%d",p->mNetId,d->instanceToken,p->mStateCountdown,p->mBoard->mMainCounter);}}}
    }
    else if(d->kind==938&&!d->batchMode&&!p->mStateCountdown){GoldSummon(p);if(p->mDead)return true;}
    else if(d->kind==253&&NativeLastPlants269WheatClockRunning(p))
    {
        if(p->mLaunchCounter>1)--p->mLaunchCounter;
        else if(CanReplaceNativeLastPlants269Wheat(p)){if(ReplaceNativeLastPlants269Wheat(p))return true;}
    }
    d->productionTicks=d->kind==253?p->mLaunchCounter:p->mStateCountdown;return true;
}
void UpdateNativeLastPlants269Phase(Plant* p)
{
    if(!IsNativeLastPlants269(p)||!Playing(p))return;auto* d=GetNativePlantData(p);
    if(d->kind==1056){UpdateNativeWater268Phase(p);return;}
    if(const auto* t=FindFusionLastPlants269Timing(d->kind)){UpdateNativeShooterPhase(p,*t);d->batchMode=d->shootSeconds>=0?1:0;}
    else d->idleSeconds+=.01f;
    d->productionTicks=d->kind==253?p->mLaunchCounter:p->mStateCountdown;
}
bool DispatchNativeLastPlants269Events(Plant* p)
{
    if(!IsNativeLastPlants269(p))return false;if(!Playing(p))return true;
    const auto* d=GetNativePlantData(p);if(d->kind==1056)return DispatchNativeWater268Events(p);
    if(d->kind==900&&d->shootSeconds>=0)
    {
        const auto* t=FindFusionLastPlants269Timing(900);int initial=0,count=0;
        if(TryGetFusionShooterShotCounter(*t,d->animationSpeed,initial))GetNativePlantData(p)->shootSeconds=(initial-p->mShootingCounter)*.01f;
        if(TryGetFusionShooterEventsAtCounter(*t,d->animationSpeed,p->mShootingCounter,count))while(count-->0)EmperorSummon(p);
    }
    return true;
}
bool HandleNativeLastPlants269Eaten(Zombie* z,Plant* p)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=900)return false;
    if(!Playing(p)||!p->mIsOnBoard||p->mPlantHealth<=0||!z||z->mDead||z->mNetGhost||z->IsDeadOrDying()||
       z->mBodyHealth<=0||z->mMindControlled||z->mBoard!=p->mBoard||z->mRow!=p->mRow||!d->batchBagCount)return true;
    const auto before=d->batchBagCount;
    const auto targetToken=p->mBoard->mZombies.DataArrayGetID(z);
    z->StopEating();z->StartMindControlled();--GetNativePlantData(p)->batchBagCount;
    LawnNet::NetTrace("Fusion Emperor269 bite parent=%u token=%u target=%u targetToken=%u before=%u remaining=%u mindControlled=%d tick=%d",
        p->mNetId,d->instanceToken,z->mNetId,targetToken,unsigned(before),unsigned(GetNativePlantData(p)->batchBagCount),z->mMindControlled?1:0,p->mBoard->mMainCounter);
    return true;
}
void NativeLastPlants269Removing(Plant* p,int reason)
{
    const auto* d=GetNativePlantData(p);if(!d||!Host(p))return;
    if(d->kind==900)LawnNet::NetTrace("Fusion Emperor269 remove parent=%u token=%u row=%d reason=%d hp=%d remaining=%u state=%d disappear=%d tick=%d",
        p->mNetId,d->instanceToken,p->mRow,reason,p->mPlantHealth,unsigned(d->batchBagCount),int(p->mState),p->mDisappearCountdown,p->mBoard->mMainCounter);
    if(d->kind==1056){NativeWater268PlantRemoving(p,reason);return;}
    if(d->kind!=900||reason<0||reason>7)return;
    Plant* q=nullptr;while(p->mBoard->IteratePlants(q))if(const auto* other=GetNativePlantData(q))
        if(!q->mDead&&other->kind==917&&std::abs(q->mPlantCol-p->mPlantCol)<=1&&std::abs(q->mRow-p->mRow)<=1)
            if(const auto* t=FindFusionSummonPlants269Timing(917))if(BeginNativeShooterShot(q,*t))
                LawnNet::NetTrace("Fusion Emperor269 deathSummon parent=%u token=%u reason=%d receiver=%u receiverToken=%u tick=%d",
                    p->mNetId,d->instanceToken,reason,q->mNetId,other->instanceToken,p->mBoard->mMainCounter);
}
bool GetNativeLastPlants269VisualPose(const Plant* p,const char*& state,float& seconds)
{
    if(!IsNativeLastPlants269(p))return false;const auto* d=GetNativePlantData(p);
    if(d->kind==1056)return GetNativeWater268VisualPose(p,state,seconds);
    state=d->kind==900&&d->shootSeconds>=0?"summon":"idle";seconds=d->kind==900&&d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return true;
}
bool HandleNativeLastPlants269TorchContact(Plant* p,Projectile* q)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=1056)return false;
    if(!Playing(p)||!q||q->mDead||q->mBoard!=p->mBoard||q->mNetGhost)return true;
    NativeProjectileData legacy;const auto* b=GetNativeProjectileData(q);
    if(!b&&q->mProjectileType==PROJECTILE_PEA&&q->mMotionType==MOTION_STRAIGHT&&q->GetProjectileDef().mDamage==20)b=&legacy;
    if(!b)return true;
    if(!FusionLastPlants269TorchContactAllowed(b->specialType,q->mRow==p->mRow,b->isZombieBullet,b->lastTorchId==p->mNetId&&b->lastTorchToken==d->instanceToken)||!FusionLastPlants269TorchPayload(b->kind))return true;
    if(d->batchShotCount==100&&!KelpSupplyReady(p))return true;
    bool counted=false;
    if(b->kind==29)counted=CreateNativeLastPlants269FireKelp(p,q)!=nullptr;
    else if(!TryConvertNativeLastPlants269Torch(p,q,counted))return true;
    if(!counted)return true;
    auto* write=GetNativePlantData(p);if(++write->batchShotCount>100){write->batchShotCount=0;KelpSupply(p);}return true;
}
Projectile* CreateNativeLastPlants269FireKelp(Plant* p,Projectile* original)
{
    const auto* pd=GetNativePlantData(p);const auto* before=GetNativeProjectileData(original);
    if(!pd||pd->kind!=1056||!Playing(p)||!original||original->mDead||original->mBoard!=p->mBoard||!before||before->kind!=29||
       !EnsureNativeProjectileVisuals(p->mBoard,30)||p->mBoard->mProjectiles.mSize>=p->mBoard->mProjectiles.mMaxSize)return nullptr;
    const uint8_t way=before->specialType==2?2:0;float x=0,y=0;
    if(!GetNativeSourcePlantNodePoint(p,"Pos",x,y))return nullptr;
    NativeProjectileData data;data.kind=30;data.damage=40;data.specialType=way;data.fireState=1;data.lastTorchId=p->mNetId;data.lastTorchToken=pd->instanceToken;
    auto* q=p->mBoard->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,p->mRow,PROJECTILE_PEA);if(!q)return nullptr;
    q->mProjectileType=PROJECTILE_PEA;q->mMotionType=way==2?MOTION_STAR:MOTION_STRAIGHT;
    q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=q->mVelZ=q->mAccZ=0;q->mShadowY=y;q->mX=int(q->mPosX);q->mY=int(q->mPosY);
    q->mVelX=float(.06*kFusionBasePixelsX);q->mVelY=q->mRotation=q->mRotationSpeed=0;
    if(!AttachNativeProjectile(p->mBoard,q,data)){q->Die();return nullptr;}
    original->Die();return q;
}
bool IsNativeLastPlants269KelpProjectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    return d&&FusionLastPlants269KelpProjectileValid(d->kind,NativeProjectileValidationDamage369(*d),d->specialType)&&p->mProjectileType==PROJECTILE_PEA&&
        p->mMotionType==(d->specialType==2?MOTION_STAR:MOTION_STRAIGHT)&&d->fireState==1&&d->lastTorchId&&d->lastTorchToken&&
        !d->sunOnHit&&!d->melonEnhanced&&!d->effectVariant&&!d->isZombieBullet&&!d->hitLatched&&!d->umbrellaed&&!d->goldAttracted;
}
bool GetNativeLastPlants269KelpProjectileRect(const Projectile* p,Sexy::Rect& out)
{
    FusionRecipientRect r;if(!IsNativeLastPlants269KelpProjectile(p)||!FusionLastPlants269KelpProjectileRect(p->mPosX+20,p->mPosY+p->mPosZ+20,GetNativeFusionPixelsY(p->mBoard),r))return false;
    out=Sexy::Rect(int(std::floor(r.x)),int(std::floor(r.y)),int(std::ceil(r.width)),int(std::ceil(r.height)));return true;
}
bool MoveNativeLastPlants269KelpProjectile(Projectile* p)
{
    if(!IsNativeLastPlants269KelpProjectile(p))return false;if(!Playing(p))return true;
    auto* board=p->mBoard;const double py=GetNativeFusionPixelsY(board),x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;
    // Source Bullet.MoveDieUpdate, shared with the proven PuffLove adapter.
    if(x<board->GetCellCenterX(0,0)-7.2*kFusionBasePixelsX||x>board->GetCellCenterX(board->GetColumnCount()-1,p->mRow)+3.85*kFusionBasePixelsX||
       y<board->GridToPixelY(0,0)+50-4.2*py||y>board->GridToPixelY(0,GetNativeFusionRowCount(board)-1)+50+.6*py){p->Die();return true;}
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mShadowY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY+p->mPosZ);
    const auto* d=GetNativeProjectileData(p);if(d->specialType==2)p->mRow=p->mBoard->PixelToGridYKeepOnBoard(int(p->mPosX+20),int(p->mPosY+20));
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        if(!LiveTarget(z)||!FusionWater268ProjectileCanHit(d->specialType,GetNativeFusionZombieStatus(z),z->mRow-p->mRow,z->mMindControlled,!NativeWater268ZombieCaptured(z),true))continue;
        const auto r=z->GetZombieRect();
        if(FusionLastPlants269KelpProjectileTouches(p->mPosX+20,p->mPosY+p->mPosZ+20,GetNativeFusionPixelsY(p->mBoard),{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)}))
        {ApplyNativeLastPlants269KelpImpact(p,z);break;}
    }
    return true;
}
bool ApplyNativeLastPlants269KelpImpact(Projectile* p,Zombie* primary)
{
    if(!IsNativeLastPlants269KelpProjectile(p))return false;if(!Playing(p))return true;
    const int damage=GetNativeProjectileData(p)->damage;
    if(primary)
    {
        if(primary->mBoard!=p->mBoard||!LiveTarget(primary))return true;
        const int primaryKind=GetNativeFusionZombieKind(primary);
        primary->TakeDamage(damage,0U);if(primary->mInPool&&!primary->mDead)SetNativeWater268Grap(primary);NativeWarmAfterDamage(primary);
        if(FusionLastPlants269KelpSputterAllowed(int(primary->mShieldType),primaryKind))
        {
            (void)RandRangeInt(59,60);FireKelpImpactEffect(p);
            std::vector<ZombieID> targets;Zombie* z=nullptr;
            const float x=p->mPosX+20,y=p->mPosY+p->mPosZ+20;const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
            while(p->mBoard->IterateZombies(z))if(z!=primary&&LiveTarget(z)&&z->mRow==p->mRow&&Sputter(z))
            {const auto r=z->GetZombieRect();if(FusionFireAreaTouchesRect(x,y,{double(r.mX),y+(r.mY-y)*ratio,double(r.mWidth),r.mHeight*ratio}))targets.push_back(p->mBoard->ZombieGetID(z));}
            int splash=0;if(!targets.empty()&&TryGetFusionFireSplashDamage(damage,int(targets.size()),splash))
                for(auto id:targets)if(auto* target=p->mBoard->ZombieTryToGet(id))if(LiveTarget(target))
                {target->TakeDamage(splash,0U);if(target->mInPool&&!target->mDead)SetNativeWater268Grap(target);NativeWarmAfterDamage(target);}
        }
        else p->PlayImpactSound(primary);
    }
    else{(void)RandRangeInt(0,2);FireKelpImpactEffect(p);p->PlayImpactSound(nullptr);}
    p->Die();return true;
}
}
