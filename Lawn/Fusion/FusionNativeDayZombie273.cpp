#include "FusionNativeDayZombie273.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionBasePlantRules.h"
#include "FusionNative.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeDefender.h"
#include "FusionNativeWater268.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionShooterBatchRules.h"
#include "FusionHostileProjectileRules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
bool Host(const GameObject* o)
{
    return o&&o->mApp&&o->mBoard&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&
        !o->mBoard->mPaused&&o->mApp->mGameScene==SCENE_PLAYING&&IsNativeFusionBoard(o->mBoard)&&
        NativeFusionBackgroundValid(uint8_t(o->mBoard->mBackground));
}
uint16_t Kind(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
Reanimation* Body(const Zombie* z)
{return z&&z->mApp?z->mApp->ReanimationTryToGet(z->mBodyReanimID):nullptr;}
NativeSourceZombieCursor* Cursor(Zombie* z)
{
    auto* context=GetNativeContext(z?z->mBoard:nullptr);
    const auto generation=context?z->mBoard->mZombies.DataArrayGetID(z):0;
    return generation?&context->sourceZombieCursors[generation]:nullptr;
}
bool Read(const Reanimation* r,float duration,FusionZombieClipPosition& out)
{
    if(!r||r->mDead||r->mFrameCount<=0||r->mLoopCount<0||!std::isfinite(r->mAnimTime)||r->mAnimTime<0||r->mAnimTime>1)return false;
    out={uint32_t(r->mLoopCount),r->mAnimTime*duration};return true;
}
bool Factor(Zombie* z,float& factor)
{
    const auto* garlic=GetNativeZombieGarlicData(z);
    if(!TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,
        garlic&&FusionGarlicStopsWalking(*garlic),factor))return false;
    factor*=GetNativeWater268GrapSpeedFactor(z);return true;
}
bool ChangingRow(const Zombie* z)
{const auto* g=GetNativeZombieGarlicData(z);return g&&g->phase==GarlicActionPhase::Moving;}
bool PlantLive(const Plant* p)
{return p&&!p->mDead&&p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Touches(float left,float top,float width,float height,const Sexy::Rect& b)
{return left<=b.mX+b.mWidth&&left+width>=b.mX&&top<=b.mY+b.mHeight&&top+height>=b.mY;}
bool Head(Zombie* z)
{
    if(auto* head=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID))return head->mReanimationType==REANIM_PEASHOOTER;
    auto* body=Body(z);if(!body||!body->TrackExists("anim_head1"))return false;
    auto* effects=z->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)return false;
    auto& reanimations=effects->mReanimationHolder->mReanimations;
    auto& attachments=effects->mAttachmentHolder->mAttachments;
    auto* track=body->GetTrackInstanceByName("anim_head1");
    const auto attachmentReady=[&]()
    {
        if(!attachments.mBlock)return false;
        const auto* attachment=attachments.DataArrayTryToGet(unsigned(track->mAttachmentID));
        return attachment&&!attachment->mDead?
            attachment->mNumEffects>=0&&attachment->mNumEffects<MAX_EFFECTS_PER_ATTACHMENT:
            attachments.mSize<attachments.mMaxSize;
    };
    // Both native allocators assert on exhaustion; their return-value checks
    // alone are too late. A dead attachment still occupies its pool slot.
    if(!reanimations.mBlock||reanimations.mSize>=reanimations.mMaxSize||!attachmentReady())return false;
    auto* head=z->mApp->AddReanimation(0,0,0,REANIM_PEASHOOTER);if(!head)return false;
    head->PlayReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,0);
    head->mAnimTime=head->mLastFrameTime=1;head->mLoopCount=1;
    z->mSpecialHeadReanimID=z->mApp->ReanimationGetID(head);
    if(!attachmentReady()||!AttachReanim(track->mAttachmentID,head,0,0))
    {z->mApp->RemoveReanimation(z->mSpecialHeadReanimID);z->mSpecialHeadReanimID=REANIMATIONID_NULL;return false;}
    return true;
}
bool StartPoleJump(Zombie* z,NativeDayZombie273State& state)
{
    auto* body=Body(z);if(!body||!body->TrackExists("anim_jump"))return false;
    z->StopEating();state.status=3;state.shadowEnabled=false;z->mVelX=0;
    z->PlayZombieReanim("anim_jump",REANIM_PLAY_ONCE_AND_HOLD,0,0);
    state.poleEvent={};state.poleEventFrame=body->mFrameStart;state.poleEventValid=true;
    if(auto* cursor=Cursor(z)){cursor->motion={};cursor->motionFrameStart=body->mFrameStart;cursor->motionValid=true;cursor->attackValid=false;}
    UpdateNativeDayZombie273Animation(z);return true;
}
void LandPole(Zombie* z,NativeDayZombie273State& state)
{state.shadowEnabled=true;state.status=0;state.poleEventValid=false;StartNativeDayZombie273Walk(z);}
void Pole(Zombie* z,NativeDayZombie273State& state,const NativeDayZombie273Hooks& hooks)
{
    if(!hooks.plantTraits||!hooks.poleContact||!hooks.poleEvent)return; // Admission MUST require these.
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(z->mBoard));
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return;
    if(state.status==2&&!z->mMindControlled)
    {
        Plant* p=nullptr;
        while(z->mBoard->mPlants.IterateNext(p))
        {
            if(!PlantLive(p)||p->mRow!=z->mRow||!NativeSourcePlantColliderEnabled(p))continue;
            const auto rect=p->GetPlantRect();
            if(GetRectOverlap(z->GetZombieAttackRect(),rect)>0)hooks.poleContact(z,p);
            if(z->mDead||z->IsDeadOrDying())return;
            FusionDayZombie273PlantTraits traits;
            if(!hooks.plantTraits(p,traits)||!Touches(x-1.05f*px,y-2*py,.7f*px,2*py,rect)||
                !FusionDayZombie273PoleCanStart(state.status,z->mMindControlled,z->mIceTrapCounter>0,
                    true,traits.shadowX<x,traits))continue;
            StartPoleJump(z,state);break;
        }
    }
    if(state.status!=3)return;
    auto* body=Body(z);FusionZombieClipPosition now;uint8_t events=0;
    if(!Read(body,3.5f,now))return;
    if(state.poleEventValid&&state.poleEventFrame==body->mFrameStart)
        TryGetFusionDayZombie273PoleEvents(state.poleEvent,now,events);
    state.poleEvent=now;state.poleEventFrame=body->mFrameStart;state.poleEventValid=true;
    if(events&Day273JumpSound1)hooks.poleEvent(z,Day273JumpSound1);
    if(events&Day273JumpSound2)hooks.poleEvent(z,Day273JumpSound2);
    if(events&Day273JumpFail)
    {
        Plant* p=nullptr;
        while(z->mBoard->mPlants.IterateNext(p))
        {
            FusionDayZombie273PlantTraits traits;
            if(!PlantLive(p)||!NativeSourcePlantColliderEnabled(p)||!hooks.plantTraits(p,traits)||
                !FusionDayZombie273PoleBlocksLanding(p->mRow==z->mRow,traits)||
                !Touches(x-.5f*px,y-py,px,2*py,p->GetPlantRect()))continue;
            // Source AdjustPosition moves the REAL root so Shadow lands plantX+.5.
            z->mPosX+=traits.shadowX+.5f*px-x;z->mX=int(z->mPosX);
            LandPole(z,state);hooks.poleEvent(z,Day273JumpFail);break;
        }
    }
    if(state.status==3&&(events&Day273JumpOver))
    {
        LandPole(z,state);
        // DancePole6 override calls base JumpOver first, then births real kind7.
        if(Kind(z)==6&&!z->IsDeadOrDying()&&z->mHasHead)hooks.poleEvent(z,Day273JumpOver);
    }
}
}
bool IsNativeDayZombie273(const Zombie* z)
{return z&&z->mBoard&&IsNativeFusionBoard(z->mBoard)&&FindFusionDayZombie273Profile(Kind(z));}
bool InitializeNativeDayZombie273(Zombie* z)
{
    if(!IsNativeDayZombie273(z))return false;if(!Host(z))return true;
    auto* s=EditNativeDayZombie273(z,true);if(!s)return false;
    *s=NativeDayZombie273State{};s->status=FusionDayZombie273Pole(Kind(z))?2:0;
    z->mZombiePhase=PHASE_ZOMBIE_NORMAL;
    if(FusionDayZombie273Shooter(Kind(z))&&!Head(z))return false;
    if(!FusionDayZombie273Doll(Kind(z)))return StartNativeDayZombie273Walk(z);
    return true;
}
bool EnsureNativeDayZombie273ShooterHead(Zombie* z)
{return IsNativeDayZombie273(z)&&FusionDayZombie273Shooter(Kind(z))&&Head(z);}
bool UpdateNativeDayZombie273Animation(Zombie* z)
{
    if(!IsNativeDayZombie273(z)||FusionDayZombie273Doll(Kind(z)))return false;
    if(!Host(z)||z->mDead)return true;
    const auto* s=GetNativeDayZombie273(z);auto* body=Body(z);FusionDayZombie273Clip clip;float factor=1;
    const bool dying=z->IsDeadOrDying();
    if(!s||!body||!TryGetFusionDayZombie273Clip(Kind(z),dying?1:s->status,z->mIsEating,clip)||(!dying&&!Factor(z,factor)))return true;
    // Pole jump disables Animator's Speed parameter; origin/chill/freeze only
    // affect the run/walk/eat states. Native Board pause still stops its clock.
    const float speed=FusionDayZombie273Pole(Kind(z))&&s->status==3&&!dying?1.f:
        GetNativeSourceZombieData(z)->originSpeed*factor;
    z->mOriginalAnimRate=body->mFrameCount*clip.multiplier*speed/clip.seconds;
    body->mAnimRate=z->mOriginalAnimRate;return true;
}
bool StartNativeDayZombie273Walk(Zombie* z,int blend)
{
    if(!IsNativeDayZombie273(z)||FusionDayZombie273Doll(Kind(z)))return false;
    if(!Host(z)||z->IsDeadOrDying())return true;
    const auto* s=GetNativeDayZombie273(z);auto* body=Body(z);FusionDayZombie273Clip clip;
    if(!s||!body||!TryGetFusionDayZombie273Clip(Kind(z),s->status,false,clip)||!body->TrackExists(clip.nativeTrack))return true;
    z->PlayZombieReanim(clip.nativeTrack,clip.loop?REANIM_LOOP:REANIM_PLAY_ONCE_AND_HOLD,blend,0);
    if(auto* cursor=Cursor(z)){cursor->motionFrameStart=body->mFrameStart;cursor->motionValid=Read(body,clip.seconds,cursor->motion);cursor->attackValid=false;}
    z->mVelX=0;UpdateNativeDayZombie273Animation(z);return true;
}
bool MoveNativeDayZombie273(Zombie* z)
{
    if(!IsNativeDayZombie273(z)||FusionDayZombie273Doll(Kind(z)))return false;
    if(!Host(z)||z->IsDeadOrDying())return true;
    const auto* s=GetNativeDayZombie273(z);auto* body=Body(z);auto* cursor=Cursor(z);FusionDayZombie273Clip clip;FusionZombieClipPosition now;
    if(!s||!body||!cursor||!TryGetFusionDayZombie273Clip(Kind(z),s->status,false,clip)||!Read(body,clip.seconds,now))return true;
    const bool moving=!z->mIsEating&&z->mZombieHeight==HEIGHT_ZOMBIE_NORMAL&&!z->mOnHighGround&&!NativeWater268ZombieCaptured(z);
    double delta=0;
    if(moving&&cursor->motionValid&&cursor->motionFrameStart==body->mFrameStart&&std::isfinite(z->mScaleZombie)&&z->mScaleZombie>0&&
        TryGetFusionDayZombie273RootDelta(Kind(z),s->status,cursor->motion,now,z->mMindControlled,
            (FusionDayZombie273Pole(Kind(z))?1.25:.31)*kFusionBasePixelsX*z->mScaleZombie,delta)&&std::isfinite(z->mPosX+delta))z->mPosX+=float(delta);
    cursor->motion=now;cursor->motionFrameStart=body->mFrameStart;cursor->motionValid=moving;return true;
}
bool UpdateNativeDayZombie273(Zombie* z,const NativeDayZombie273Hooks& hooks)
{
    if(!IsNativeDayZombie273(z))return false;if(!Host(z)||z->mDead)return true;
    auto* s=EditNativeDayZombie273(z);if(!s)return true;
    if(z->IsDeadOrDying()){s->danceSpawnPending=false;return true;}
    if(Kind(z)==6&&s->danceSpawnPending)RetryNativeDancePole284(z);
    if(FusionDayZombie273Pole(Kind(z))){Pole(z,*s,hooks);return true;}
    if(!FusionDayZombie273Shooter(Kind(z)))return true;
    auto* head=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID);float factor=0;
    if(!head||!Factor(z,factor))return true;
    head->mAnimRate=head->mFrameCount*2*GetNativeSourceZombieData(z)->originSpeed*factor; // shoot1s, controller speed2.
    float after=s->shootCountdown;bool play=false;
    if(TryStepFusionDayZombie273Shoot(Kind(z),s->shootCountdown,FusionDayZombie273ShootInterval(Kind(z)),.01f,0,z->mBoard->mTimeStopCounter>0,!z->mHasHead,0,after,play))
    {
        if(play)after=FusionDayZombie273ShootInterval(Kind(z))+RandRangeFloat(-.1f,.1f);
        s->shootCountdown=after;
        if(play){head->PlayReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,head->mAnimRate);s->shootEvent=1;}
    }
    if(s->shootEvent==1&&head->mAnimTime>=.6666667f){s->shootEvent=2;CreateNativeDayZombie273Shot(z);}
    if(head->mLoopCount>0)s->shootEvent=0;
    return true;
}
bool GetNativeDayZombie273Status(const Zombie* z,int& status)
{
    if(!IsNativeDayZombie273(z))return false;
    const auto* s=GetNativeDayZombie273(z);if(!s)return false;
    status=z->mDead||const_cast<Zombie*>(z)->IsDeadOrDying()?1:s->status;return true;
}
bool GetNativeDayZombie273Presentation(const Zombie* z,const char*& clip,float& phase)
{
    if(!IsNativeDayZombie273(z)||FusionDayZombie273Doll(Kind(z)))return false;
    const auto* s=GetNativeDayZombie273(z);const auto* body=Body(z);FusionDayZombie273Clip c;
    if(!s||!body||!TryGetFusionDayZombie273Clip(Kind(z),const_cast<Zombie*>(z)->IsDeadOrDying()?1:s->status,z->mIsEating,c)||
        !std::isfinite(body->mAnimTime)||body->mAnimTime<0||body->mAnimTime>1)return false;
    clip=c.sourceName;phase=body->mAnimTime;return true;
}
bool GetNativeDayZombie273HeadPose(const Zombie* z,const char*& clip,float& phase)
{
    const auto* s=GetNativeDayZombie273(z);if(!IsNativeDayZombie273(z)||!FusionDayZombie273Shooter(Kind(z))||!s||!s->shootEvent)return false;
    const auto* head=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID);
    if(!head||!std::isfinite(head->mAnimTime)||head->mAnimTime<0||head->mAnimTime>1)return false;
    clip="shoot";phase=head->mAnimTime;return true;
}
bool NativeDayZombie273CanUseSharedBite(const Zombie* z)
{const auto* s=GetNativeDayZombie273(z);return !FusionDayZombie273Pole(Kind(z))||(s&&s->status==0);}
bool ApplyNativeDayZombie273Armor(Zombie* z,int damage,int& overkill,const NativeDayZombie273Hooks& hooks)
{
    const auto kind=Kind(z);if(!IsNativeDayZombie273(z)||(!FusionDayZombie273Doll(kind)&&!FusionDayZombie273NutArmor(kind)))return false;
    overkill=0;
    if(!Host(z)||z->mDead||z->IsDeadOrDying())return true;
    auto* s=EditNativeDayZombie273(z);int after=0,excess=0;
    if(!s||s->dollFalling||!TryStepFusionDayZombie273Armor(kind,z->mHelmHealth,damage,after,excess))return true;
    if(FusionDayZombie273Doll(kind)&&z->mHelmHealth>0&&!after)
    {
        uint16_t childKind=0;float x=0,y=0;
        if(!hooks.prepareDollFall||!hooks.finishDollFall||!hooks.prepareDollFall(z)||
            !TryGetFusionDayZombie273DollChild(kind,childKind)||
            !FusionDayZombie273DollCanFall(z->mBoard->mZombies.mSize,z->mBoard->mZombies.mMaxSize)||
            !GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)||!EnsureNativeSourceEffect269(z->mBoard,11))return true;
        NativeSourceZombieSpawnRequest request;request.kind=childKind;request.row=z->mRow;request.fromWave=z->mFromWave;
        request.positioned=true;request.shadowX=x;request.friendly=z->mMindControlled;
        s->dollFalling=true;auto* child=SpawnNativeSourceZombie(z->mBoard,request);
        if(!child){s->dollFalling=false;return true;}
        z->mHelmHealth=0;z->mHelmType=HELMTYPE_NONE;
        float rootX=x,rootY=y;GetNativeSourceZombie269NodePoint(z,"",rootX,rootY);
        SpawnNativeSourceEffect269(z->mBoard,11,rootX,rootY-float(GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder,true);
        hooks.finishDollFall(z,child);overkill=0;return true; // Never forward parent's remainder into the fresh child.
    }
    uint8_t stage=s->armorStage;int sprite=-1;
    if(after)TryGetFusionDayZombie273ArmorStage(kind,after,z->mHelmMaxHealth,stage,stage,sprite);
    s->armorStage=stage;z->mHelmHealth=after;if(!after)z->mHelmType=HELMTYPE_NONE;
    overkill=excess;return true;
}
Coin* DropNativeDayZombie273Bucket(Zombie* z)
{
    if(!Host(z)||(Kind(z)!=106&&Kind(z)!=108)||!CanCreateNativeEquipment(z->mBoard,FusionEquipmentType::Bucket))return nullptr;
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return nullptr;
    return CreateNativeEquipment(z->mBoard,FusionEquipmentType::Bucket,x,y-float(GetNativeFusionPixelsY(z->mBoard)),true);
}
bool PrepareNativeDayZombie273HeadLoss(Zombie* z,uint32_t flags)
{
    if((Kind(z)!=106&&Kind(z)!=108)||!Host(z))return true;
    auto* s=EditNativeDayZombie273(z);if(!s)return false;
    if(!DropNativeDayZombie273Bucket(z))
    {s->headDropPending=true;s->headDropDamageFlags=flags;return false;}
    s->headDropPending=false;s->headDropDamageFlags=0;return true;
}
bool UpdateNativeDayZombie273PendingHeadLoss(Zombie* z)
{
    if(!IsNativeDayZombie273(z)||!Host(z))return false;
    auto* s=EditNativeDayZombie273(z);if(!s||!s->headDropPending)return false;
    if(!z->mHasHead){s->headDropPending=false;s->headDropDamageFlags=0;return false;}
    z->DropHead(s->headDropDamageFlags);return s->headDropPending;
}
bool NativeDayZombie273ProjectileMetadataValid(const NativeProjectileData& d,ProjectileType type,ProjectileMotion motion)
{
    return NativeDay273ProjectileMetadataValid(d,type,motion);
}
Projectile* CreateNativeDayZombie273Shot(Zombie* z)
{
    if(!IsNativeDayZombie273(z)||!Host(z))return nullptr;
    const auto* s=GetNativeDayZombie273(z);FusionDayZombie273Shot shot;
    if(!s||!TryMakeFusionDayZombie273Shot(Kind(z),z->IsDeadOrDying()?1:s->status,GetNativeSourceZombieData(z)->beforeDying||z->IsDeadOrDying(),ChangingRow(z),
        z->mMindControlled?1:0,z->mMindControlled,shot))return nullptr;
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Zombie_head/Shoot",x,y))return nullptr;
    NativeProjectileData d;d.kind=shot.kind;d.damage=shot.damage;d.specialType=shot.way;d.isZombieBullet=shot.hostile;
    const auto motion=shot.way==9?MOTION_BACKWARDS:MOTION_STRAIGHT;
    if(!NativeProjectileMetadataValid(d,PROJECTILE_PEA,motion)||!EnsureNativeProjectileVisuals(z->mBoard,d.kind)||
        z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize)return nullptr;
    x+=shot.offsetX*float(kFusionBasePixelsX);
    auto* p=z->mBoard->AddProjectile(int(x-20),int(y-20),z->mRenderOrder-1,z->mRow,PROJECTILE_PEA);if(!p)return nullptr;
    p->mMotionType=motion;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY=y;
    p->mVelX=float((shot.way==9?-1:1)*.06*kFusionBasePixelsX);p->mVelY=p->mVelZ=p->mAccZ=0;
    p->mRotation=p->mRotationSpeed=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    if(!AttachNativeProjectile(z->mBoard,p,d)){p->Die();return nullptr;}
    z->mApp->PlayFoley(FOLEY_THROW);return p;
}
bool IsNativeDayZombie273HostileProjectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p);
    return d&&d->isZombieBullet&&NativeDayZombie273ProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);
}
bool UpdateNativeDayZombie273HostileProjectile(Projectile* p,const NativeDayZombie273Hooks& hooks)
{
    if(!IsNativeDayZombie273HostileProjectile(p))return false;if(!Host(p)||p->mDead)return true;
    auto* d=GetNativeProjectileData(p);auto* b=p->mBoard;
    const float py=float(GetNativeFusionPixelsY(b)),px=float(kFusionBasePixelsX);
    const float sourceX=(p->mPosX+20-b->GetCellCenterX(0,0))/px-4.8f;
    const float sourceY=2.3f-(p->mPosY+20-(b->GridToPixelY(0,0)+50))/py;
    const float maxX=1.35f*(b->GetColumnCount()-1)-4.8f+1.85f;
    const float minY=2.3f-(b->GridToPixelY(0,GetNativeFusionRowCount(b)-1)-b->GridToPixelY(0,0))/py-.6f;
    if(!std::isfinite(sourceX)||!std::isfinite(sourceY)||sourceX< -12||sourceX>maxX+1||sourceY<minY||sourceY>6.5f){p->Die();return true;}
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    auto rect=p->GetProjectileRect();FusionRecipientRect sourceRect;
    if(d->kind==11&&TryMakeFusionShooterBatchProjectileRect(11,p->mPosX+20,p->mPosY+20,sourceRect))
        rect=Sexy::Rect(int(sourceRect.x),int(p->mPosY+20+(sourceRect.y-p->mPosY-20)*py/kFusionBasePixelsY),
            int(std::ceil(sourceRect.width)),int(std::ceil(sourceRect.height*py/kFusionBasePixelsY)));
    if(d->hitLatched)return true;
    Plant* plant=nullptr;
    while(b->mPlants.IterateNext(plant))
    {
        const auto* pd=GetNativePlantData(plant);
        if(!PlantLive(plant)||!pd||!FusionSuperCherryPlantCollides(pd->kind,p->mRow,plant->mRow,NativeSourcePlantColliderEnabled(plant))||
            GetRectOverlap(rect,plant->GetPlantRect())<=0)continue;
        // Source CheckPlant latches even when the target's Animator speed is0.
        d->hitLatched=true;
        int ignored=0;
        if(TryPlanFusionSuperCherryRebound(pd->kind,!plant->mIsAsleep&&!plant->mWakeUpCounter,false,ignored))
        {
            d->specialType=0;d->isZombieBullet=false;d->hitLatched=false;
            if(GetNativeDefenderPotKind(plant)==1133)d->damage*=2;
            p->mMotionType=MOTION_STRAIGHT;p->mVelX=float(.06*kFusionBasePixelsX);
            if(pd->kind==1131)
                if(auto* state=GetNativePlantData(plant))state->batchShotCount=uint32_t((std::min)(uint64_t(1000000),uint64_t(state->batchShotCount)+uint32_t(d->damage)));
            return true;
        }
        Plant* selected=SelectNativeSuperCherryPlantTarget(p,plant);if(!selected)return true;
        const uint16_t fx=d->kind==0?0:d->kind==1?10:18;
        // Admission requires this actual effect resource, no missing-FX alias.
        if(!EnsureNativeSourceEffect269(b,fx))return true;
        ApplyNativeSourcePlantDamage(selected,d->damage,0);
        SpawnNativeSourceEffect269(b,fx,p->mPosX+20,p->mPosY+20,p->mRenderOrder,true);
        selected->mEatenFlashCountdown=25;p->mApp->PlayFoley(FOLEY_SPLAT);p->Die();return true;
    }
    Zombie* z=nullptr;
    while(b->mZombies.IterateNext(z))
    {
        if(!z->mMindControlled||z->mDead||z->IsDeadOrDying()||GetNativeFusionZombieHealth(z)<=0||z->mRow!=p->mRow||
            !FusionDayZombie273ProjectileCanHit(GetNativeFusionZombieStatus(z))||
            NativeWater268ZombieCaptured(z)||GetRectOverlap(rect,z->GetZombieRect())<=0)continue;
        FusionDayZombie273ZombieHit hit;
        if(!hooks.projectileHitZombie||!TryPlanFusionDayZombie273ZombieHit(d->kind,d->specialType,d->damage,
            z->mShieldHealth,z->mMindControlled,hit)||!EnsureNativeSourceEffect269(b,d->kind==0?0:d->kind==1?10:18))return true;
        if(!hooks.projectileHitZombie(p,z,hit.sourceDamageType,hit.damage,hit.knockback))return true;
        SpawnNativeSourceEffect269(b,d->kind==0?0:d->kind==1?10:18,p->mPosX+20,p->mPosY+20,p->mRenderOrder,true);
        p->mApp->PlayFoley(FOLEY_SPLAT);p->Die();return true;
    }
    return true;
}
void NativeDayZombie273Removing(Zombie* z)
{
    if(!IsNativeDayZombie273(z)||!FusionDayZombie273Shooter(Kind(z)))return;
    z->mApp->RemoveReanimation(z->mSpecialHeadReanimID);z->mSpecialHeadReanimID=REANIMATIONID_NULL;
    if(auto* s=EditNativeDayZombie273(z)){s->shootEvent=0;s->head.Reset();}
}
}
