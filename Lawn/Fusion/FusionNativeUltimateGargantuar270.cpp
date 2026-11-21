#include "FusionNativeUltimateGargantuar270.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativePlacement.h"
#include "FusionNativeWater268.h"
#include "FusionBatchActionRules.h"
#include "FusionBasePlantRules.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* o)
{return o&&o->mBoard&&o->mApp&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&CanApplyNativeFusionGameplay(o->mBoard);}
Reanimation* Body(const Zombie* z){return z&&z->mApp?z->mApp->ReanimationTryToGet(z->mBodyReanimID):nullptr;}
NativeSourceZombieCursor* Cursor(Zombie* z)
{auto* c=GetNativeContext(z->mBoard);return c?&c->sourceZombieCursors[z->mBoard->mZombies.DataArrayGetID(z)]:nullptr;}
bool Position(const Reanimation* body,float duration,FusionZombieClipPosition& out)
{
    if(!body||body->mDead||body->mLoopCount<0||!std::isfinite(body->mAnimTime)||body->mAnimTime<0||body->mAnimTime>1)return false;
    out={uint32_t(body->mLoopCount),body->mAnimTime*duration};return true;
}
void Play(Zombie* z,const char* track,ReanimLoopType loop,int blend)
{
    auto* body=Body(z);if(!body||!body->TrackExists(track))return;
    z->PlayZombieReanim(track,loop,blend,0);
    if(auto* cursor=Cursor(z))
    {cursor->attack={};cursor->attackValid=true;cursor->attackFrameStart=body->mFrameStart;cursor->motionValid=false;}
    UpdateNativeUltimateGarg270Animation(z);
}
bool Target(const Zombie* z,Zombie* q)
{return q&&q!=z&&!q->mDead&&!q->IsDeadOrDying()&&q->mBodyHealth>0&&!NativeWater268ZombieCaptured(q)&&
    FusionUltimateGarg270ZombieAllowed(GetNativeFusionZombieStatus(q),q->mRow==z->mRow,q->mMindControlled!=z->mMindControlled);}
bool PlantTarget(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&!p->mDead&&p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth>0&&
    NativeSourcePlantColliderEnabled(p)&&!IsFusionSourceFlyingKind(d->kind)&&d->kind!=1095;}
bool Area(Zombie* z,NativeUltimateGarg270NodeQuery node,bool crash,FusionBatchArea& a)
{
    float x=0,y=0;if(crash){if(!node||!node(z,"",x,y))return false;}
    else if(!z->GetShadowCenter(x,y))return false;
    const double px=kFusionBasePixelsX,py=GetNativeFusionPixelsY(z->mBoard);
    x+=float((z->mMindControlled?1:-1)*px);
    y+=float((crash&&z->mMindControlled?1:-1)*py);
    a={x-.65*px,y-py,1.3*px,2*py};return true;
}
bool Touch(const FusionBatchArea& a,const Sexy::Rect& r)
{return FusionBatchAreaTouchesRect(a,r.mX,r.mY,r.mWidth,r.mHeight);}
bool HasTarget(Zombie* z,NativeUltimateGarg270NodeQuery node)
{
    float x=0,y=0;if(!node||!node(z,"",x,y))return false;
    const float vision=float(z->mBoard->GetCellCenterX(z->mBoard->GetColumnCount()-1,z->mRow)+1.85*kFusionBasePixelsX);
    if(!z->mMindControlled&&x>=vision)return false;
    FusionBatchArea a;if(!Area(z,node,false,a))return false;
    Plant* p=nullptr;if(!z->mMindControlled)while(z->mBoard->IteratePlants(p))
        if(p->mRow==z->mRow&&PlantTarget(p)&&Touch(a,p->GetPlantRect()))return true;
    Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))if(Target(z,q)&&Touch(a,q->GetZombieRect()))return true;
    return false;
}
bool Npc(Board* b,uint16_t kind,int row)
{const auto* p=FindFusionZombieProfile(kind);return p&&row>=0&&row<GetNativeFusionRowCount(b)&&
    (!p->waterOnly||b->mPlantRow[row]==PLANTROW_POOL)&&GetNativeSourceZombieCarrier(kind)>=0&&EnsureNativeSourceZombieVisuals(b,kind);}
bool SpawnImps(Zombie* z,NativeUltimateGarg270NodeQuery node)
{
    if(z->mMindControlled)return true;
    if(!Npc(z->mBoard,213,z->mRow)||z->mBoard->mZombies.mMaxSize-z->mBoard->mZombies.mSize<4||!EnsureNativeSourceEffect269(z->mBoard,11))return false;
    float x=0,y=0;if(!node||!node(z,"",x,y))return false;
    x-=float(3*kFusionBasePixelsX);SpawnNativeSourceEffect269(z->mBoard,11,x,y-float(GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder+1,true);
    const int count=RandRangeInt(1,3);
    for(int i=0;i<count;++i)
    {NativeSourceZombieSpawnRequest r;r.kind=213;r.row=z->mRow;r.positioned=true;r.shadowX=x+RandRangeFloat(-1,1)*float(kFusionBasePixelsX);
     if(!SpawnNativeSourceZombie(z->mBoard,r)){LawnNet::NetTrace("Fusion Ultimate212 unexpected Imp refusal parent=%u ordinal=%d count=%d",z->mNetId,i,count);return false;}}
    return true;
}
bool Crash(Zombie* z,NativeUltimateGarg270NodeQuery node)
{
    FusionBatchArea a;if(!Area(z,node,true,a))return false;
    // Source SuperGargantuar.AnimCrash invokes its real offspring callback on
    // every non-MC hammer event. Never silently drop those unsupported actors.
    if(!z->mMindControlled&&(!Npc(z->mBoard,213,z->mRow)||z->mBoard->mZombies.mMaxSize-z->mBoard->mZombies.mSize<4||!EnsureNativeSourceEffect269(z->mBoard,11)))return false;
    std::set<int> columns;Plant* p=nullptr;
    if(!z->mMindControlled)while(z->mBoard->IteratePlants(p))if(PlantTarget(p)&&p->mRow==z->mRow&&Touch(a,p->GetPlantRect()))
    {const auto kind=GetNativePlantData(p)->kind;if(kind!=251&&kind!=255)columns.insert(p->mPlantCol);}
    std::vector<PlantID> plants;p=nullptr;while(z->mBoard->IteratePlants(p))if(PlantTarget(p)&&p->mRow==z->mRow&&columns.count(p->mPlantCol))plants.push_back(z->mBoard->PlantGetID(p));
    for(auto id:plants)if((p=z->mBoard->PlantTryToGet(id))&&PlantTarget(p))
    {const auto kind=GetNativePlantData(p)->kind;bool caltrop=false,nut=false;int level=0;
     if(!TryGetSpecialZombie269SourcePlantTraits(kind,caltrop,nut,level))continue;
     const auto hit=PlanFusionUltimateGarg270PlantHit(kind,kind==251||kind==255,nut,level);
     if(hit==FusionUltimateGarg270PlantHit::Crush)p->Squish();
     else if(hit!=FusionUltimateGarg270PlantHit::Ignore)ApplyNativeSourcePlantDamage(p,hit==FusionUltimateGarg270PlantHit::Damage500?500:1000,0);}
    std::vector<ZombieID> zombies;Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))if(Target(z,q)&&Touch(a,q->GetZombieRect()))zombies.push_back(z->mBoard->ZombieGetID(q));
    for(auto id:zombies)if((q=z->mBoard->ZombieTryToGet(id))&&Target(z,q))q->TakeDamage(1800,1U); // Source Shieldless4.
    z->mApp->PlayFoley(FOLEY_THUMP);z->mBoard->ShakeBoard(0,3);
    LawnNet::NetTrace("Fusion Ultimate212 hammer id=%u columns=%u zombies=%u tick=%d",z->mNetId,unsigned(columns.size()),unsigned(zombies.size()),z->mBoard->mMainCounter);
    return SpawnImps(z,node);
}
}
bool CanCreateNativeUltimateGarg270(Board* b,int row)
{
    if(!b||!b->mApp||!IsNativeFusionBoard(b)||row<0||row>=GetNativeFusionRowCount(b)||
       b->mPlantRow[row]!=PLANTROW_NORMAL||!b->RowCanHaveZombies(row)||b->mZombies.mMaxSize-b->mZombies.mSize<5)return false;
    // NPC40 is the genuine SuperBall landing child. No filtered subset or Imp
    // alias can turn this complete dependency preflight into a ready result.
    return Npc(b,212,row)&&Npc(b,213,row)&&Npc(b,40,row)&&EnsureNativeUltimateBall270Visual(b)&&
        EnsureNativeSourceEffect269(b,11)&&EnsureNativeSourceEffect269(b,41)&&EnsureNativeSourceEffect269(b,42);
}
bool IsNativeUltimateGarg270(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d&&d->kind==212&&IsNativeSourceZombie(z);}
bool InitializeNativeUltimateGarg270(Zombie* z)
{
    if(!IsNativeUltimateGarg270(z))return false;if(!Host(z))return true;
    auto* state=EditNativeUltimateGarg270(z,true);if(!state)return false;state->bodyStage=0;
    z->mHasObject=true;z->mIsEating=false;z->mZombiePhase=PHASE_ZOMBIE_NORMAL;
    NativeUltimateGarg270BoardEvent(z,true);StartNativeUltimateGarg270Walk(z,0);return true;
}
bool UpdateNativeUltimateGarg270Animation(Zombie* z)
{
    if(!IsNativeUltimateGarg270(z))return false;if(!Host(z))return true;auto* body=Body(z);const auto* d=GetNativeSourceZombieData(z);if(!body)return true;
    const bool dying=z->IsDeadOrDying();float duration=dying?5:z->mZombiePhase==PHASE_ZOMBIE_NORMAL?kFusionUltimateGarg270WalkDuration:kFusionUltimateGarg270ActionDuration;
    float speed=dying?1.2f:z->mZombiePhase==PHASE_GARGANTUAR_SMASHING?.9f:z->mZombiePhase==PHASE_GARGANTUAR_THROWING?1.2f:.2f;
    if(!dying)speed*=d->originSpeed*(z->mChilledCounter>0?.5f:1)*(z->IsImmobilizied()?0:1);
    z->mOriginalAnimRate=body->mFrameCount*speed/duration;body->mAnimRate=z->mOriginalAnimRate;return true;
}
bool StartNativeUltimateGarg270Walk(Zombie* z,int blend)
{
    if(!IsNativeUltimateGarg270(z))return false;if(!Host(z))return true;
    Play(z,"anim_walk",REANIM_LOOP,blend);
    if(auto* cursor=Cursor(z))if(auto* body=Body(z))
    {cursor->motionFrameStart=body->mFrameStart;cursor->motionValid=Position(body,kFusionUltimateGarg270WalkDuration,cursor->motion);cursor->attackValid=false;}
    return true;
}
bool MoveNativeUltimateGarg270(Zombie* z)
{
    if(!IsNativeUltimateGarg270(z))return false;if(!Host(z))return true;
    auto* c=Cursor(z);auto* body=Body(z);FusionZombieClipPosition current;
    if(!c||!Position(body,kFusionUltimateGarg270WalkDuration,current))return true;
    const bool moving=z->mZombiePhase==PHASE_ZOMBIE_NORMAL&&!z->IsDeadOrDying()&&!z->IsImmobilizied()&&!z->mIsEating;
    double delta=0;if(moving&&c->motionValid&&c->motionFrameStart==body->mFrameStart&&
        TryGetFusionUltimateGarg270RootDelta(c->motion,current,z->mMindControlled,.5*kFusionBasePixelsX*z->mScaleZombie,delta)&&std::isfinite(z->mPosX+delta))z->mPosX+=float(delta);
    c->motion=current;c->motionFrameStart=body->mFrameStart;c->motionValid=moving;return true;
}
bool UpdateNativeUltimateGarg270(Zombie* z,NativeUltimateGarg270NodeQuery node)
{
    if(!IsNativeUltimateGarg270(z))return false;if(!Host(z)||z->mDead||z->IsDeadOrDying())return true;
    if(z->mZombiePhase==PHASE_ZOMBIE_NORMAL)
    {if(!z->IsImmobilizied()&&HasTarget(z,node)){z->mZombiePhase=PHASE_GARGANTUAR_SMASHING;Play(z,"anim_smash",REANIM_LOOP,0);}return true;}
    if(z->mZombiePhase!=PHASE_GARGANTUAR_THROWING&&z->mZombiePhase!=PHASE_GARGANTUAR_SMASHING)return true;
    auto* body=Body(z);auto* c=Cursor(z);FusionZombieClipPosition current;
    if(!body||!c||!Position(body,kFusionUltimateGarg270ActionDuration,current))return true;
    uint32_t events=0;if(c->attackValid&&c->attackFrameStart==body->mFrameStart&&
        CountFusionUltimateGarg270Events(c->attack,current,z->mZombiePhase==PHASE_GARGANTUAR_THROWING?2.25f:1.8333334f,events))
        for(uint32_t i=0;i<events;++i)if(z->mZombiePhase==PHASE_GARGANTUAR_THROWING)CreateNativeUltimateBall270(z,node);else Crash(z,node);
    c->attack=current;c->attackFrameStart=body->mFrameStart;c->attackValid=true;
    if((z->mZombiePhase==PHASE_GARGANTUAR_THROWING&&body->mLoopCount>0)||
       (z->mZombiePhase==PHASE_GARGANTUAR_SMASHING&&!HasTarget(z,node)))
    {z->mZombiePhase=PHASE_ZOMBIE_NORMAL;StartNativeUltimateGarg270Walk(z,0);}
    return true;
}
bool AdjustNativeUltimateGarg270IncomingDamage(Zombie* z,int& damage)
{if(!IsNativeUltimateGarg270(z))return false;int capped=0;if(CapFusionUltimateGarg270Damage(damage,capped))damage=capped;return true;}
bool ApplyNativeUltimateGarg270BodyDamage(Zombie* z,int damage)
{
    if(!IsNativeUltimateGarg270(z))return false;if(!Host(z)||damage<0||z->IsDeadOrDying())return true;
    auto* d=GetNativeSourceZombieData(z);auto* state=EditNativeUltimateGarg270(z);if(!state)return true;
    d->health-=float(damage);z->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);
    state->bodyStage=FusionUltimateGarg270BodyStage(d->health,z->mBodyMaxHealth,state->bodyStage);
    float x=0,y=0;if(z->GetShadowCenter(x,y))
    {const float sourceX=float((x-z->mBoard->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8);
     if(z->mZombiePhase!=PHASE_GARGANTUAR_THROWING&&FusionUltimateGarg270MustThrow(d->health,z->mBodyMaxHealth,z->mHasObject,z->mMindControlled,sourceX))
     {z->mZombiePhase=PHASE_GARGANTUAR_THROWING;z->mIsEating=false;Play(z,"anim_throw",REANIM_PLAY_ONCE_AND_HOLD,0);}}
    return true;
}
bool GetNativeUltimateGarg270Presentation(const Zombie* z,const char*& clip,float& phase)
{
    if(!IsNativeUltimateGarg270(z))return false;auto* body=Body(z);if(!body)return false;
    clip=const_cast<Zombie*>(z)->IsDeadOrDying()?"die":z->mZombiePhase==PHASE_GARGANTUAR_THROWING?"throw":z->mZombiePhase==PHASE_GARGANTUAR_SMASHING?"attack":"walk";
    phase=body->mAnimTime;return std::isfinite(phase)&&phase>=0&&phase<=1;
}
bool GetNativeUltimateGarg270Status(const Zombie* z,int& status)
{if(!IsNativeUltimateGarg270(z))return false;status=const_cast<Zombie*>(z)->IsDeadOrDying()?1:z->mHasObject&&z->mZombiePhase!=PHASE_GARGANTUAR_THROWING?15:0;return true;}
void NativeUltimateGarg270Died(Zombie* z)
{
    if(!IsNativeUltimateGarg270(z)||!Host(z)||z->mMindControlled)return;
    NativeUltimateGarg270BoardEvent(z,false);
    if(z->mZombieAge<1000)LawnNet::NetTrace("Fusion Ultimate212 sourceAchievement1009 NotAvailable id=%u age=%d",z->mNetId,z->mZombieAge);
}
Projectile* CreateNativeUltimateBall270(Zombie* z,NativeUltimateGarg270NodeQuery node)
{
    if(!IsNativeUltimateGarg270(z)||!Host(z)||z->mDead||z->IsDeadOrDying()||z->mMindControlled||!z->mHasObject||
       !EnsureNativeUltimateBall270Visual(z->mBoard)||z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize)return nullptr;
    float x=0,y=0;if(!node||!node(z,"Shoot",x,y))return nullptr;
    auto* p=z->mBoard->AddProjectile(int(x-20),int(y-20),z->mRenderOrder+1,z->mRow,PROJECTILE_PEA);if(!p)return nullptr;
    p->mMotionType=MOTION_STAR;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY=y;
    p->mVelX=float(-3*kFusionBasePixelsX*.01);p->mVelY=float(-2*GetNativeFusionPixelsY(z->mBoard)*.01);p->mVelZ=p->mAccZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    auto* state=EditNativeUltimateBall270(p,true);if(!state){p->Die();return nullptr;}
    state->instanceToken=z->mBoard->mProjectiles.DataArrayGetID(p);state->ageTicks=p->mProjectileAge;z->mHasObject=false;
    LawnNet::NetTrace("Fusion Ultimate212 ball parent=%u projectile=%u token=%u row=%d",z->mNetId,p->mNetId,state->instanceToken,z->mRow);return p;
}
bool UpdateNativeUltimateBall270(Projectile* p)
{
    if(!GetNativeUltimateBall270(p))return false;if(!Host(p)||p->mDead)return true;
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(p->mBoard));
    const float x=(p->mPosX+20-p->mBoard->GetCellCenterX(0,0))/px-4.8f;
    const float rowY=float(p->mBoard->GridToPixelY(0,p->mRow)+50);
    // Normal D2's source row is horizontal; slope modes are not admitted here.
    const float y=-(p->mPosY+20-rowY)/py;float nx=0,ny=0,nvy=0;bool landed=false;
    if(!StepFusionUltimateBall270(x,y,p->mVelX/(px*.01f),-p->mVelY/(py*.01f),.01f,0,nx,ny,nvy,landed))return true;
    p->mPosX=p->mBoard->GetCellCenterX(0,0)+(nx+4.8f)*px-20;p->mPosY=rowY-ny*py-20;
    p->mVelY=-nvy*py*.01f;p->mX=int(p->mPosX);p->mY=int(p->mPosY);EditNativeUltimateBall270(p)->ageTicks=p->mProjectileAge;
    if(!landed)return true;
    if(!Npc(p->mBoard,40,p->mRow)||p->mBoard->mZombies.mSize>=p->mBoard->mZombies.mMaxSize-1||
       !EnsureNativeSourceEffect269(p->mBoard,41)||!EnsureNativeSourceEffect269(p->mBoard,42))return true;
    SpawnNativeSourceEffect269(p->mBoard,41,p->mPosX+20,p->mPosY+20,p->mRenderOrder,true);
    NativeSourceZombieSpawnRequest r;r.kind=40;r.row=p->mRow;r.positioned=true;r.shadowX=p->mPosX+20+RandRangeFloat(-1,1)*px;
    if(!SpawnNativeSourceZombie(p->mBoard,r))return true;
    SpawnNativeSourceEffect269(p->mBoard,42,p->mPosX+20,p->mPosY+20,p->mRenderOrder,true);p->mBoard->ShakeBoard(0,3);p->Die();return true;
}
}
