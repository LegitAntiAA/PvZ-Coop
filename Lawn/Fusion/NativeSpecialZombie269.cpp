#include "NativeSpecialZombie269.h"
#include "FusionBasePlantRules.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(Zombie* z)
{return IsNativeSpecialZombie269(z)&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&
    z->mApp->mBoard==z->mBoard&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
bool Paper(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d&&SpecialZombie269PaperKind(d->kind);}
int PaperStatus(const Zombie* z)
{return z->mZombiePhase==PHASE_NEWSPAPER_MADDENING?5:z->mZombiePhase==PHASE_NEWSPAPER_MAD?6:4;}
Reanimation* Body(const Zombie* z)
{return z&&z->mApp?z->mApp->ReanimationTryToGet(z->mBodyReanimID):nullptr;}
NativeSourceZombieCursor* Cursor(Zombie* z)
{auto* c=GetNativeContext(z->mBoard);return c?&c->sourceZombieCursors[z->mBoard->mZombies.DataArrayGetID(z)]:nullptr;}
bool ReadPhase(const Reanimation* body,float duration,FusionZombieClipPosition& p)
{
    if(!body||body->mDead||body->mLoopCount<0||!std::isfinite(body->mAnimTime)||body->mAnimTime<0||body->mAnimTime>1)return false;
    p={uint32_t(body->mLoopCount),body->mAnimTime*duration};return true;
}
bool Factor(Zombie* z,float& factor)
{
    const auto* garlic=GetNativeZombieGarlicData(z);
    // Both paper classes inherit ordinary Zombie.MoveUpdate status factors.
    if(!TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,
        garlic&&FusionGarlicStopsWalking(*garlic),factor))return false;
    factor*=GetNativeWater268GrapSpeedFactor(z);return true;
}
bool EatingBlocked(Zombie* z)
{const auto* g=GetNativeZombieGarlicData(z);return z->IsImmobilizied()||(g&&FusionGarlicPreventsEating(*g));}
Plant* LockedPlant(Zombie* z,const NativeSourceZombieData& d)
{
    if(d.targetEntity!=NativeZombieTargetEntity::Plant||z->mMindControlled)return nullptr;
    auto* p=z->mBoard->mPlants.DataArrayTryToGet(d.targetGeneration);
    return p&&p->mNetId==d.targetId&&!p->mDead&&p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth>0&&p->mRow==z->mRow&&
        !NativeBatchPlantUntargetable(p)&&!NativeFireActionUntargetable(p)&&GetRectOverlap(z->GetZombieAttackRect(),p->GetPlantRect())>0?p:nullptr;
}
Zombie* LockedZombie(Zombie* z,const NativeSourceZombieData& d)
{
    if(d.targetEntity!=NativeZombieTargetEntity::Zombie)return nullptr;
    auto* t=z->mBoard->mZombies.DataArrayTryToGet(d.targetGeneration);
    return t&&t!=z&&t->mNetId==d.targetId&&!t->mDead&&!t->IsDeadOrDying()&&GetNativeFusionZombieHealth(t)>0&&
        t->mMindControlled!=z->mMindControlled&&t->mRow==z->mRow&&!t->IsFlying()&&!NativeWater268ZombieCaptured(t)&&
        GetRectOverlap(z->GetZombieAttackRect(),t->GetZombieRect())>0?t:nullptr;
}
void ClearTarget(Zombie* z)
{
    auto* d=GetNativeSourceZombieData(z);d->targetEntity=NativeZombieTargetEntity::None;d->targetId=0;d->targetGeneration=0;
    if(auto* c=Cursor(z))c->attackValid=false;z->StopEating();
}
}
bool IsNativeSpecialZombie269(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return z&&d&&FindSpecialZombie269Profile(d->kind)&&
    z->mBoard&&IsNativeFusionBoard(z->mBoard)&&z->mApp;}
bool InitializeNativeSpecialZombie269(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z))return false;if(!Host(z))return true;
    const auto kind=GetNativeSourceZombieData(z)->kind;
    if(Paper(z)){z->mZombiePhase=PHASE_NEWSPAPER_READING;z->mSummonCounter=0;z->mBossHeadCounter=0;StartNativeSpecialZombie269Walk(z,0);}
    if(kind==14||kind==18)StartNativeSpecialZombie269Vehicle(z);
    return true;
}
bool BeginNativeSpecialZombie269PaperFall(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z)||!Paper(z))return false;
    if(!Host(z)||z->IsDeadOrDying())return true;
    auto* body=Body(z);if(!body||!body->TrackExists("anim_gasp"))return true;
    z->StopEating();z->mShieldType=SHIELDTYPE_NONE;z->mShieldHealth=0;z->mHasObject=false;
    z->mZombiePhase=PHASE_NEWSPAPER_MADDENING;
    z->PlayZombieReanim("anim_gasp",REANIM_PLAY_ONCE_AND_HOLD,0,0);
    UpdateNativeSpecialZombie269Animation(z);
    return true; // Suppress stock DropShield particles and its end-of-clip Angry.
}
bool UpdateNativeSpecialZombie269Animation(Zombie* z)
{
    if(IsNativeSpecialZombie269Vehicle(z))return UpdateNativeSpecialZombie269VehicleAnimation(z);
    if(!IsNativeSpecialZombie269(z)||!Paper(z))return false;
    if(!Host(z)||z->IsDeadOrDying())return true;
    auto* body=Body(z);SpecialZombie269PaperClip clip;
    const auto* d=GetNativeSourceZombieData(z);float statusFactor=0;
    if(!body||body->mFrameCount<=0||!TryGetSpecialZombie269PaperClip(d->kind,PaperStatus(z),z->mIsEating,clip)||
       !Factor(z,statusFactor))return true;
    z->mOriginalAnimRate=body->mFrameCount*clip.speed*d->originSpeed*statusFactor/clip.duration;
    body->mAnimRate=z->mOriginalAnimRate;return true;
}
bool UpdateNativeSpecialZombie269(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z))return false;if(!Host(z)||z->IsDeadOrDying())return true;
    if(Paper(z)&&z->mZombiePhase==PHASE_NEWSPAPER_MADDENING)
    {
        const auto* body=Body(z);
        if(body&&SpecialZombie269PaperAngryDue(body->mAnimTime))
        {
            // Actual losePaper event1.9166666, Paper.AngrySound35A050→status6.
            z->mZombiePhase=PHASE_NEWSPAPER_MAD;z->mApp->PlayFoley(FOLEY_NEWSPAPER_RARRGH);
            StartNativeSpecialZombie269Walk(z,0);UpdateNativeSpecialZombie269Animation(z);
        }
    }
    return Paper(z)||IsNativeSpecialZombie269Vehicle(z); // Vehicle hooks suppress stock Zamboni AI.
}
bool GetNativeSpecialZombie269Presentation(const Zombie* z,const char*& clip,float& phase)
{
    if(!IsNativeSpecialZombie269(z))return false;const auto* body=Body(z);
    if(!body||!std::isfinite(body->mAnimTime)||body->mAnimTime<0||body->mAnimTime>1)return false;
    const auto kind=GetNativeSourceZombieData(z)->kind;
    if(const_cast<Zombie*>(z)->IsDeadOrDying())clip="die";
    else if(Paper(z))
    {SpecialZombie269PaperClip p;if(!TryGetSpecialZombie269PaperClip(kind,PaperStatus(z),z->mIsEating,p))return false;clip=p.name;}
    else clip=kind==14?"walk":z->mFlatTires?"die":GetNativeSourceZombieData(z)->health<float(z->mBodyMaxHealth)/3?"shake":"idle";
    phase=body->mAnimTime;return true;
}
bool GetNativeSpecialZombie269BodyStage(const Zombie* z,uint8_t& stage)
{const auto* d=GetNativeSourceZombieData(z);return d&&TryGetSpecialZombie269BodyStage(d->kind,d->health,z->mBodyMaxHealth,stage);}
bool GetNativeSpecialZombie269Status(const Zombie* z,int& status)
{if(!IsNativeSpecialZombie269(z))return false;if(IsNativeSpecialZombie269Vehicle(z)){status=z->mDead?1:0;return true;}
 if(!Paper(z))return false;
 status=const_cast<Zombie*>(z)->IsDeadOrDying()?1:
    GetNativeSourceZombieData(z)->kind==104?(PaperStatus(z)==6?6:0):PaperStatus(z);
 return true;}
bool StartNativeSpecialZombie269Walk(Zombie* z,int blend)
{
    if(!IsNativeSpecialZombie269(z)||!Paper(z))return false;if(!Host(z)||z->IsDeadOrDying())return true;
    if(z->mZombiePhase==PHASE_NEWSPAPER_MADDENING)return true;
    auto* body=Body(z);const char* track=PaperStatus(z)==6?"anim_walk_nopaper":"anim_walk";
    if(!body||!body->TrackExists(track))return true;
    z->PlayZombieReanim(track,REANIM_LOOP,blend,0);UpdateNativeSpecialZombie269Animation(z);
    if(auto* c=Cursor(z))
    {SpecialZombie269PaperClip clip;TryGetSpecialZombie269PaperClip(GetNativeSourceZombieData(z)->kind,PaperStatus(z),false,clip);
     c->motionValid=ReadPhase(body,clip.duration,c->motion);c->motionFrameStart=body->mFrameStart;c->attackValid=false;}
    return true;
}
bool MoveNativeSpecialZombie269Paper(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z)||!Paper(z))return false;if(!Host(z)||z->IsDeadOrDying())return true;
    auto* c=Cursor(z);auto* body=Body(z);const auto* d=GetNativeSourceZombieData(z);SpecialZombie269PaperClip clip;
    if(!c||!body||!TryGetSpecialZombie269PaperClip(d->kind,PaperStatus(z),z->mIsEating,clip))return true;
    FusionZombieClipPosition now;if(!ReadPhase(body,clip.duration,now))return true;
    const bool walking=!z->mIsEating&&PaperStatus(z)!=5&&z->mHasHead&&!NativeWater268ZombieCaptured(z);
    double delta=0;
    if(walking&&c->motionValid&&c->motionFrameStart==body->mFrameStart&&
       TryGetSpecialZombie269PaperRootDelta(d->kind,PaperStatus(z),c->motion,now,z->mMindControlled,
           .26*kFusionBasePixelsX*z->mScaleZombie,delta)&&std::isfinite(z->mPosX+delta))z->mPosX+=float(delta);
    c->motion=now;c->motionFrameStart=body->mFrameStart;c->motionValid=walking;return true;
}
bool UpdateNativeSpecialZombie269PaperAttack(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z)||!Paper(z))return false;if(!Host(z)||z->IsDeadOrDying())return true;
    auto* d=GetNativeSourceZombieData(z);auto* c=Cursor(z);auto* body=Body(z);
    if(!c||!body)return true;
    if(PaperStatus(z)==5||!z->mHasHead||NativeWater268ZombieCaptured(z)){ClearTarget(z);return true;}
    if(d->targetEntity==NativeZombieTargetEntity::None)
    {
        if(EatingBlocked(z))return true;
        if(auto* target=z->FindZombieTarget())
        {d->targetEntity=NativeZombieTargetEntity::Zombie;d->targetId=target->mNetId;d->targetGeneration=z->mBoard->mZombies.DataArrayGetID(target);}
        else if(!z->mMindControlled)if(auto* p=z->FindPlantTarget(ATTACKTYPE_CHEW))
        {d->targetEntity=NativeZombieTargetEntity::Plant;d->targetId=p->mNetId;d->targetGeneration=z->mBoard->mPlants.DataArrayGetID(p);}
        if(d->targetEntity==NativeZombieTargetEntity::None)return true;
        z->StartEating();UpdateNativeSpecialZombie269Animation(z);c->attackValid=ReadPhase(body,2,c->attack);
        c->attackFrameStart=body->mFrameStart;c->motionValid=false;return true;
    }
    if(!LockedPlant(z,*d)&&!LockedZombie(z,*d)){ClearTarget(z);return true;}
    FusionZombieClipPosition now;if(!ReadPhase(body,2,now))return true;uint32_t count=0;
    if(c->attackValid&&c->attackFrameStart==body->mFrameStart)TryCountSpecialZombie269PaperBites(d->kind,c->attack,now,count);
    c->attack=now;c->attackFrameStart=body->mFrameStart;c->attackValid=true;
    if(EatingBlocked(z))return true;
    for(uint32_t i=0;i<count;++i)
    {
        if(auto* p=LockedPlant(z,*d)){if(!HandleNativeBatchPlantEaten(z,p))z->EatPlant(p);}
        else if(auto* target=LockedZombie(z,*d))z->EatZombie(target);
        else {ClearTarget(z);break;}
        z->mApp->PlayFoley(FOLEY_CHOMP);
        if(!z->mIsEating||!z->mHasHead||EatingBlocked(z))break;
    }
    return true;
}
bool InitializeNativeSpecialZombie269PaperAddon(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z)||GetNativeSourceZombieData(z)->kind!=104)return false;
    if(z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID))return true;
    auto* body=Body(z);if(!body||!body->TrackExists("anim_head1"))return false;
    auto* head=z->mApp->AddReanimation(0,0,0,REANIM_PEASHOOTER);if(!head)return false;
    head->PlayReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,0);head->mAnimTime=head->mLastFrameTime=1;head->mLoopCount=1;
    z->mSpecialHeadReanimID=z->mApp->ReanimationGetID(head);
    auto* track=body->GetTrackInstanceByName("anim_head1");
    if(!AttachReanim(track->mAttachmentID,head,0,0))
    {z->mApp->RemoveReanimation(z->mSpecialHeadReanimID);z->mSpecialHeadReanimID=REANIMATIONID_NULL;return false;}
    return true; // An actual attached native clock; source model supplies ALL visible art.
}
Projectile* CreateNativeSpecialZombie269PaperShot(Zombie* z,NativeSpecialZombie269NodeQuery query)
{
    if(!Host(z)||GetNativeSourceZombieData(z)->kind!=104||!query||z->IsDeadOrDying()||!z->mHasHead||z->mButteredCounter)return nullptr;
    float x=0,y=0;if(!query(z,"Zombie_head/Shoot",x,y)||!std::isfinite(x)||!std::isfinite(y))return nullptr;
    NativeProjectileData data;data.kind=3;data.damage=300;data.specialType=z->mMindControlled?0:9;data.isZombieBullet=!z->mMindControlled;
    const auto motion=z->mMindControlled?MOTION_STRAIGHT:MOTION_BACKWARDS;
    if(!NativeProjectileMetadataValid(data,PROJECTILE_PEA,motion)||!EnsureNativeProjectileVisuals(z->mBoard,3)||
       z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize)return nullptr;
    x+=float(.1*kFusionBasePixelsX);y+=float(.1*GetNativeFusionPixelsY(z->mBoard));
    auto* p=z->mBoard->AddProjectile(int(x-20),int(y-20),z->mRenderOrder-1,z->mRow,PROJECTILE_PEA);if(!p)return nullptr;
    p->mMotionType=motion;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY=y;
    p->mVelX=float((z->mMindControlled?1:-1)*.06*kFusionBasePixelsX);p->mVelY=p->mVelZ=p->mAccZ=0;
    p->mRotation=p->mRotationSpeed=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    if(!AttachNativeProjectile(z->mBoard,p,data)){p->Die();return nullptr;}
    z->mApp->PlayFoley(FOLEY_THROW);
    LawnNet::NetTrace("Fusion Paper269 shot source=%u token=%u projectile=%u kind=3 way=%u hostile=%d damage=300",z->mNetId,GetNativeSourceZombieData(z)->instanceToken,p->mNetId,data.specialType,data.isZombieBullet?1:0);
    return p;
}
bool UpdateNativeSpecialZombie269PaperShoot(Zombie* z,NativeSpecialZombie269NodeQuery query)
{
    if(!IsNativeSpecialZombie269(z)||GetNativeSourceZombieData(z)->kind!=104)return false;
    if(!Host(z)||z->IsDeadOrDying())return true;
    auto* head=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID);if(!head||!query)return true;
    float factor=0;if(!Factor(z,factor))return true;
    const float speed=3*GetNativeSourceZombieData(z)->originSpeed*factor;
    head->mAnimRate=head->mFrameCount*speed;
    if(StepSpecialZombie269PaperShootCounter(z->mSummonCounter,PaperStatus(z)==6,z->mBoard->mTimeStopCounter>0,!z->mHasHead))
    {head->PlayReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,head->mAnimRate);z->mBossHeadCounter=1;}
    // Callback follows the real additive clip even if base state changes.
    if(z->mBossHeadCounter==1&&head->mAnimTime>=.6666667f)
    {z->mBossHeadCounter=2;CreateNativeSpecialZombie269PaperShot(z,query);}
    if(head->mLoopCount>0)z->mBossHeadCounter=0;
    return true;
}
bool GetNativeSpecialZombie269AddonPose(const Zombie* z,const char*& clip,float& phase)
{
    if(!IsNativeSpecialZombie269(z)||GetNativeSourceZombieData(z)->kind!=104||z->mBossHeadCounter<1||z->mBossHeadCounter>2)return false;
    const auto* head=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID);
    if(!head||!std::isfinite(head->mAnimTime)||head->mAnimTime<0||head->mAnimTime>1)return false;
    clip="shoot";phase=head->mAnimTime;return true;
}
void NativeSpecialZombie269Removing(Zombie* z)
{
    if(!IsNativeSpecialZombie269(z)||GetNativeSourceZombieData(z)->kind!=104)return;
    z->mApp->RemoveReanimation(z->mSpecialHeadReanimID);z->mSpecialHeadReanimID=REANIMATIONID_NULL;
}
bool ApplyNativeSpecialZombie269CrashPlant(Zombie* z,Plant* p,bool caltrop,bool defended,int defense)
{
    const auto* d=GetNativePlantData(p);SpecialZombie269CrashPlan plan;
    if(!Host(z)||!p||!d||p->mBoard!=z->mBoard||p->mDead||!p->mIsOnBoard||p->mRow!=z->mRow||z->mMindControlled||
       !TryPlanSpecialZombie269Crash(GetNativeSourceZombieData(z)->kind,d->kind,caltrop,defended,defense,plan))return false;
    if(plan.action==SpecialZombie269CrashAction::Ignore)return true;
    if(plan.killByCaltrop&&!HandleNativeSpecialZombie269DriverCaltrop(z))return false;
    if(plan.action==SpecialZombie269CrashAction::Crush)p->Squish();
    else {ApplyNativeSourcePlantDamage(p,plan.damage,0);if(plan.knockRight)z->mPosX+=float(kFusionBasePixelsX);}
    return true;
}
bool ApplyNativeSpecialZombie269CrashZombie(Zombie* z,Zombie* target)
{
    if(!Host(z)||!target||target->mBoard!=z->mBoard||target->mDead||target->IsDeadOrDying()||
       !SpecialZombie269CrashZombieAllowed(target->mRow==z->mRow,target->mMindControlled==z->mMindControlled,GetNativeFusionZombieStatus(target)))return false;
    return ApplyNativeSpecialZombie269VehicleDamage4(target,20);
}
bool ApplyNativeSpecialZombie269VehicleDamage4(Zombie* target,int damage)
{
    if(!target||damage<0||!target->mApp||!target->mBoard||target->mDead||target->mNetGhost||
       target->mApp->NetIsClient()||!IsNativeFusionBoard(target->mBoard)||
       !CanApplyNativeFusionGameplay(target->mBoard))return false;
    // Zombie.TakeDamage38A5A0 case4 bypasses second armor, then applies first
    // armor/body normally. This is native1U, NOT source squash11's instant kill.
    target->TakeDamage(damage,1U);return true;
}
bool HandleNativeSpecialZombie269DriverCaltrop(Zombie* z)
{
    if(!Host(z))return false;
    if(GetNativeSourceZombieData(z)->kind==14)return true; // Source base slot50=30F780 is empty.
    if(GetNativeSourceZombieData(z)->kind!=18)return false;
    if(z->mFlatTires)return true;
    SpecialZombie269CaltropPlan plan;
    if(!TryPlanSpecialZombie269DriverCaltrop(GetNativeSourceZombieData(z)->health,z->mBodyMaxHealth,plan))return false;
    if(plan.damage)z->TakeDamage(plan.damage,0U);
    else
    {
        if(z->mZombieAge>(std::numeric_limits<int>::max)()-200)return false;
        // Source keeps its positive HP/head, disables the collider, and invokes
        // DieAndExplode in2s even if its animation is buttered. Native actor age
        // is the only clock; mPhaseCounter is not repurposed or double-ticked.
        z->mFlatTires=true;z->mVelX=0;z->mPhaseCounter=0;z->mSummonCounter=z->mZombieAge+200;
        z->mZombiePhase=PHASE_ZAMBONI_FLAT;
        if(auto* body=Body(z);body&&body->TrackExists("anim_wheelie1"))
            z->PlayZombieReanim("anim_wheelie1",REANIM_PLAY_ONCE_AND_HOLD,0,0);
        UpdateNativeSpecialZombie269VehicleAnimation(z);
    }
    return true;
}
bool MoveNativeSpecialZombie269Driver(Zombie* z,float currentSpeed,bool behind)
{
    if(!Host(z)||GetNativeSourceZombieData(z)->kind!=18)return false;float vx=0;
    if(!TryGetSpecialZombie269DriverVelocity(z->mMindControlled,z->mFlatTires,behind,currentSpeed,vx))return false;
    // Normal-D2 has no sloping source Board. Caller skips stock walking and
    // UpdateZamboni: its speed/ice-anchor law differs from source35E530.
    z->mVelX=std::abs(vx)*float(kFusionBasePixelsX)*.01f;
    z->mPosX+=vx*float(kFusionBasePixelsX)*.01f;z->mX=int(z->mPosX);return true;
}

bool IsNativeSpecialZombie269Vehicle(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return IsNativeSpecialZombie269(z)&&d&&(d->kind==14||d->kind==18);}
bool NativeSpecialZombie269VehicleTerrainValid(const Zombie* z)
{
    if(!IsNativeSpecialZombie269Vehicle(z)||z->mRow<0||z->mRow>=GetNativeFusionRowCount(z->mBoard))return false;
    return z->mBoard->mPlantRow[z->mRow]==(GetNativeSourceZombieData(z)->kind==14?PLANTROW_POOL:PLANTROW_NORMAL);
}
bool NativeSpecialZombie269VehicleColliderEnabled(const Zombie* z)
{return IsNativeSpecialZombie269Vehicle(z)&&!z->mDead&&!(GetNativeSourceZombieData(z)->kind==18&&z->mFlatTires);}
bool StartNativeSpecialZombie269Vehicle(Zombie* z)
{
    if(!IsNativeSpecialZombie269Vehicle(z))return false;if(!Host(z)||!NativeSpecialZombie269VehicleTerrainValid(z))return true;
    auto* body=Body(z);if(!body||!body->TrackExists("anim_drive"))return true;
    if(z->mFlatTires)return true; // A walk request cannot cancel source Invoke("DieAndExplode",2).
    z->mIsEating=false;z->mZombiePhase=PHASE_ZOMBIE_NORMAL;z->mZombieHeight=HEIGHT_ZOMBIE_NORMAL;
    z->mInPool=GetNativeSourceZombieData(z)->kind==14;
    z->PlayZombieReanim("anim_drive",REANIM_LOOP,0,0);UpdateNativeSpecialZombie269VehicleAnimation(z);
    if(auto* c=Cursor(z))
    {c->motionFrameStart=body->mFrameStart;c->motionValid=ReadPhase(body,1.6833333f,c->motion);c->attackValid=false;}
    return true;
}
bool UpdateNativeSpecialZombie269VehicleAnimation(Zombie* z)
{
    if(!IsNativeSpecialZombie269Vehicle(z))return false;if(!Host(z))return true;
    auto* body=Body(z);const auto* d=GetNativeSourceZombieData(z);SpecialZombie269PaperClip clip;float factor=0;
    if(!body||body->mFrameCount<=0||!TryGetSpecialZombie269VehicleClip(d->kind,z->mFlatTires,clip,d->health<float(z->mBodyMaxHealth)/3))return true;
    if(!TryGetFusionZombieStatusSpeedFactor(0,d->kind==14&&z->mChilledCounter>0,d->kind==14&&z->mIceTrapCounter>0,
        z->mButteredCounter>0,false,factor))return true;
    factor*=GetNativeWater268GrapSpeedFactor(z);
    z->mOriginalAnimRate=body->mFrameCount*clip.speed*d->originSpeed*factor/clip.duration;body->mAnimRate=z->mOriginalAnimRate;
    return true;
}
bool NativeSpecialZombie269VehicleTouchesRect(const Zombie* z,NativeSpecialZombie269NodeQuery query,
    double left,double top,double width,double height)
{
    if(!NativeSpecialZombie269VehicleColliderEnabled(z)||!query)return false;float x=0,y=0;
    if(!query(z,"",x,y))return false;
    return SpecialZombie269VehicleTouchesRect(GetNativeSourceZombieData(z)->kind,x,y,z->mScaleZombie,z->mMindControlled,
        kFusionBasePixelsX,GetNativeFusionPixelsY(z->mBoard),left,top,width,height);
}
bool MoveNativeSpecialZombie269Vehicle(Zombie* z,NativeSpecialZombie269NodeQuery query)
{
    if(!IsNativeSpecialZombie269Vehicle(z))return false;if(!Host(z)||!NativeSpecialZombie269VehicleTerrainValid(z))return true;
    const auto* d=GetNativeSourceZombieData(z);
    if(d->kind==18)
    {
        if(z->mFlatTires){z->mVelX=0;return true;}
        float frontX=0,frontY=0;if(!query||!query(z,"Zombie_zamboni_wheel_1",frontX,frontY)||!std::isfinite(frontX))return true;
        // Source Board.Update resets an absent ice road to sourceX35. Native
        // BOARD_ICE_START is a different integer renderer boundary.
        const float boundary=z->mBoard->mIceTimer[z->mRow]>0?float(z->mBoard->mIceMinX[z->mRow]):
            float(z->mBoard->GetCellCenterX(0,0)+(35+4.8)*kFusionBasePixelsX);
        return MoveNativeSpecialZombie269Driver(z,.8f,frontX<boundary);
    }
    auto* c=Cursor(z);auto* body=Body(z);if(!c||!body)return true;FusionZombieClipPosition now;
    if(!ReadPhase(body,1.6833333f,now))return true;
    double delta=0;
    if(c->motionValid&&c->motionFrameStart==body->mFrameStart&&!NativeWater268ZombieCaptured(z)&&
        TryGetSpecialZombie269SubmarineRootDelta(c->motion,now,z->mMindControlled,.9*kFusionBasePixelsX*z->mScaleZombie,delta)&&
        std::isfinite(z->mPosX+delta))z->mPosX+=float(delta);
    // Existing source-root native projection, not an unmeasured assertion of
    // Unity's builtin root-motion world-scale parity. No visual root translation.
    c->motion=now;c->motionFrameStart=body->mFrameStart;c->motionValid=true;z->mX=int(z->mPosX);return true;
}
bool UpdateNativeSpecialZombie269VehicleContacts(Zombie* z,NativeSpecialZombie269NodeQuery query,NativeSpecialZombie269PlantTraits traits)
{
    if(!IsNativeSpecialZombie269Vehicle(z))return false;if(!Host(z)||!NativeSpecialZombie269VehicleTerrainValid(z)||
        !NativeSpecialZombie269VehicleColliderEnabled(z)||!query)return true;
    // Source OnTriggerStay is a physics callback (.02s), not every native.01s
    // Update. The existing actor age supplies that cadence without a new clock.
    if(z->mBoard->mMainCounter%2)return true;
    if(!z->mMindControlled)
    {
        std::vector<uint32_t> hits;Plant* p=nullptr;
        while(z->mBoard->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard&&GetNativePlantData(p)&&p->mRow==z->mRow&&NativeSourcePlantColliderEnabled(p))
        {const auto r=p->GetPlantRect();if(NativeSpecialZombie269VehicleTouchesRect(z,query,r.mX,r.mY,r.mWidth,r.mHeight))hits.push_back(z->mBoard->mPlants.DataArrayGetID(p));}
        for(auto id:hits)if((p=z->mBoard->mPlants.DataArrayTryToGet(id))&&!p->mDead)
        {
            bool caltrop=false,nut=false;int level=0;
            if(!traits||!traits(p,caltrop,nut,level))continue; // Unavailable source traits cannot fall back to stock crushing.
            ApplyNativeSpecialZombie269CrashPlant(z,p,caltrop,nut,level);
            if(!NativeSpecialZombie269VehicleColliderEnabled(z))break;
        }
    }
    if(NativeSpecialZombie269VehicleColliderEnabled(z))
    {
        std::vector<uint32_t> hits;Zombie* t=nullptr;
        while(z->mBoard->IterateZombies(t))if(t!=z&&!t->mDead&&!t->IsDeadOrDying()&&t->mRow==z->mRow&&
            t->mMindControlled!=z->mMindControlled&&!NativeWater268ZombieCaptured(t))
        {const auto r=t->GetZombieRect();if(NativeSpecialZombie269VehicleTouchesRect(z,query,r.mX,r.mY,r.mWidth,r.mHeight))hits.push_back(z->mBoard->mZombies.DataArrayGetID(t));}
        for(auto id:hits)if((t=z->mBoard->mZombies.DataArrayTryToGet(id))&&!t->mDead)ApplyNativeSpecialZombie269CrashZombie(z,t);
    }
    return true;
}
bool DieNativeSpecialZombie269Vehicle(Zombie* z,NativeSpecialZombie269VehicleFx effect)
{
    if(!IsNativeSpecialZombie269Vehicle(z))return false;if(!Host(z))return true;
    float x=0,y=0;if(!effect||!z->GetShadowCenter(x,y))return true;
    const uint16_t particle=GetNativeSourceZombieData(z)->kind==14?35:36;
    if(!effect(z,particle,x,y-float(.6*GetNativeFusionPixelsY(z->mBoard))))return true;
    z->mVelX=0;z->mSummonCounter=0;
    // Shared native retirement runs existing tags, parent links and wave counts.
    // Source900 children are MC: source DropItem/GardenRNG branches are skipped.
    // Enemy native DropLoot is the same bounded adaptation as existing source
    // walkers; full source DropItem/Garden/strength accounting is not claimed.
    if(z->mMindControlled)z->DieNoLoot();else z->DieWithLoot();return true;
}
bool UpdateNativeSpecialZombie269VehiclePendingDeath(Zombie* z,NativeSpecialZombie269VehicleFx effect)
{
    if(!IsNativeSpecialZombie269Vehicle(z)||GetNativeSourceZombieData(z)->kind!=18||!z->mFlatTires)return false;
    if(!Host(z))return true;
    if(z->mSummonCounter>0&&z->mZombieAge>=z->mSummonCounter)DieNativeSpecialZombie269Vehicle(z,effect);
    return true;
}
bool ApplyNativeSpecialZombie269VehicleBodyDamage(Zombie* z,int damage,NativeSpecialZombie269VehicleFx effect)
{
    if(!IsNativeSpecialZombie269Vehicle(z))return false;
    if(!Host(z)||damage<0)return true;
    auto* d=GetNativeSourceZombieData(z);const float before=d->health,after=before-float(damage);if(!std::isfinite(after))return true;
    d->health=after;z->mBodyHealth=NativeSourceZombieLivenessHealth(after);z->mJustGotShotCounter=25;
    if(d->kind==18&&after<=0)DieNativeSpecialZombie269Vehicle(z,effect);
    else if(d->kind==18&&after<float(z->mBodyMaxHealth)/3&&before>=float(z->mBodyMaxHealth)/3)
    {
        if(auto* body=Body(z);body&&body->TrackExists("anim_wheelie1"))z->PlayZombieReanim("anim_wheelie1",REANIM_LOOP,0,0);
        UpdateNativeSpecialZombie269VehicleAnimation(z);
    }
    // Submarine.BodyTakeDamage only subtracts and changes children. Its inherited
    // Update checks (int)HP<1 on the next real update, without native limb loss.
    return true;
}
bool CreateNativeSpecialZombie269IceRoad(Zombie* z,NativeSpecialZombie269NodeQuery query)
{
    if(!IsNativeSpecialZombie269Vehicle(z)||GetNativeSourceZombieData(z)->kind!=18)return false;
    if(!Host(z)||!NativeSpecialZombie269VehicleTerrainValid(z)||!query)return true;
    float x=0,y=0;if(!query(z,"Zombie_zamboni_4",x,y)||!std::isfinite(x))return true;
    // Source inherited event355730: lower the boundary and refresh30s. Native
    // Board.UpdateIce owns the only timer and peer serialization of this road.
    if(x<z->mBoard->mIceMinX[z->mRow])z->mBoard->mIceMinX[z->mRow]=int(std::floor(x));
    z->mBoard->mIceTimer[z->mRow]=3000;return true;
}

}
