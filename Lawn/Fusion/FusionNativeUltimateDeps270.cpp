#include "FusionNativeUltimateDeps270.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeEquipment.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeWater268.h"
#include "FusionNativeLobber.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionDefenderRules.h"
#include "FusionNativePlacement.h"
#include "FusionBasePlantRules.h"
#include "FusionGarlicRules.h"
#include "SpecialZombie269Rules.h"
#include "FusionBatchActionRules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* o){return o&&o->mBoard&&o->mApp&&o->mApp->mBoard==o->mBoard&&!o->mNetGhost&&!o->mApp->NetIsClient()&&CanApplyNativeFusionGameplay(o->mBoard);}
Reanimation* Body(const Zombie* z){return z&&z->mApp?z->mApp->ReanimationTryToGet(z->mBodyReanimID):nullptr;}
NativeSourceZombieCursor* Cursor(Zombie* z){auto* c=GetNativeContext(z->mBoard);return c?&c->sourceZombieCursors[z->mBoard->mZombies.DataArrayGetID(z)]:nullptr;}
bool Position(const Reanimation* r,float duration,FusionZombieClipPosition& p)
{if(!r||r->mDead||r->mLoopCount<0||!std::isfinite(r->mAnimTime)||r->mAnimTime<0||r->mAnimTime>1)return false;p={uint32_t(r->mLoopCount),r->mAnimTime*duration};return true;}
bool Entering(const Zombie* z){return GetNativeSourceZombieData(z)->kind==213&&z->mZombiePhase==PHASE_IMP_LANDING;}
void Play(Zombie* z,const char* track,ReanimLoopType loop,int blend)
{
    auto* r=Body(z);if(!r||!r->TrackExists(track))return;z->PlayZombieReanim(track,loop,blend,0);
    if(auto* c=Cursor(z)){c->motionValid=c->attackValid=false;c->attack={};c->attackFrameStart=r->mFrameStart;}
    UpdateNativeUltimateDeps270Animation(z);
}
bool Touch(const Sexy::Rect& a,const Sexy::Rect& b)
{return FusionBatchAreaTouchesRect({double(a.mX),double(a.mY),double(a.mWidth),double(a.mHeight)},b.mX,b.mY,b.mWidth,b.mHeight);}
bool PlantLive(Plant* p){return p&&!p->mDead&&!p->mSquished&&p->mIsOnBoard&&p->mPlantHealth>0&&NativeSourcePlantColliderEnabled(p);}
bool TargetLive(Zombie* self,Zombie* z)
{return z&&z!=self&&!z->mDead&&!z->IsDeadOrDying()&&z->mBodyHealth>0&&z->mRow==self->mRow&&z->mMindControlled!=self->mMindControlled&&
    GetNativeFusionZombieStatus(z)!=10&&GetNativeFusionZombieStatus(z)!=12&&!NativeWater268ZombieCaptured(z);}
bool Npc(Board* b,uint16_t kind,int row)
{return FindFusionZombieProfile(kind)&&GetNativeSourceZombieCarrier(kind)>=0&&row>=0&&row<GetNativeFusionRowCount(b)&&b->RowCanHaveZombies(row)&&EnsureNativeSourceZombieVisuals(b,kind);}
void ClearTarget(Zombie* z)
{auto* d=GetNativeSourceZombieData(z);d->targetEntity=NativeZombieTargetEntity::None;d->targetId=0;d->targetGeneration=0;z->mIsEating=false;StartNativeUltimateDeps270Walk(z,0);}
Plant* LockedPlant(Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);if(d->targetEntity!=NativeZombieTargetEntity::Plant||z->mMindControlled)return nullptr;
 auto* p=z->mBoard->mPlants.DataArrayTryToGet(d->targetGeneration);Sexy::Rect r;
 return p&&p->mNetId==d->targetId&&p->mRow==z->mRow&&PlantLive(p)&&GetNativeUltimateDeps270Rect(z,r)&&Touch(r,p->GetPlantRect())?p:nullptr;}
Zombie* LockedZombie(Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);if(d->targetEntity!=NativeZombieTargetEntity::Zombie)return nullptr;
 auto* q=z->mBoard->mZombies.DataArrayTryToGet(d->targetGeneration);Sexy::Rect r;
 return q&&q->mNetId==d->targetId&&TargetLive(z,q)&&GetNativeUltimateDeps270Rect(z,r)&&Touch(r,q->GetZombieRect())?q:nullptr;}
bool Acquire(Zombie* z)
{
    Sexy::Rect r;if(!GetNativeUltimateDeps270Rect(z,r))return false;auto* d=GetNativeSourceZombieData(z);Plant* p=nullptr;
    if(!z->mMindControlled)while(z->mBoard->IteratePlants(p))if(PlantLive(p)&&p->mRow==z->mRow&&Touch(r,p->GetPlantRect()))
    {const auto* pd=GetNativePlantData(p);if(pd&&IsFusionSourceFlyingKind(pd->kind))continue;
     d->targetEntity=NativeZombieTargetEntity::Plant;d->targetId=p->mNetId;d->targetGeneration=z->mBoard->mPlants.DataArrayGetID(p);break;}
    Zombie* q=nullptr;if(d->targetEntity==NativeZombieTargetEntity::None)while(z->mBoard->IterateZombies(q))if(TargetLive(z,q)&&Touch(r,q->GetZombieRect()))
    {d->targetEntity=NativeZombieTargetEntity::Zombie;d->targetId=q->mNetId;d->targetGeneration=z->mBoard->mZombies.DataArrayGetID(q);break;}
    if(d->targetEntity==NativeZombieTargetEntity::None)return false;
    z->mIsEating=true;Play(z,"anim_eat",REANIM_LOOP,0);if(auto* c=Cursor(z)){c->attack={};c->attackValid=true;}return true;
}
bool StartMachineRow(Zombie* z,NativeUltimateDeps270State& s)
{
    const int origin=z->mRow,rows=GetNativeFusionRowCount(z->mBoard);const auto wet=z->mBoard->mPlantRow[origin]==PLANTROW_POOL;
    const auto compatible=[&](int row){return row>=0&&row<rows&&z->mBoard->RowCanHaveZombies(row)&&(z->mBoard->mPlantRow[row]==PLANTROW_POOL)==wet;};
    GarlicRowPlan plan;if(!TryPlanFusionGarlicRow(origin,rows,compatible(origin-1),compatible(origin+1),false,plan))return false;
    const int target=plan.randomZeroRow==plan.randomOneRow?plan.randomZeroRow:RandRangeInt(0,1)?plan.randomOneRow:plan.randomZeroRow;
    if(target==origin)return true;s.moving=true;s.originRow=int8_t(origin);s.targetRow=int8_t(target);s.startY=z->mPosY;s.rowTicks=200;z->mPhaseCounter=200;
    // Source switches logical row immediately when moving down, sorting when up.
    z->SetRow((std::max)(origin,target));return true;
}
void MachineContacts(Zombie* z,NativeUltimateDeps270State& s)
{
    if(s.moving)return;Sexy::Rect r;if(!GetNativeUltimateDeps270Rect(z,r))return;Plant* p=nullptr;
    if(!z->mMindControlled)while(z->mBoard->IteratePlants(p))if(PlantLive(p)&&p->mRow==z->mRow&&Touch(r,p->GetPlantRect()))
    {
        const auto* pd=GetNativePlantData(p);if(!pd)continue;
        if(IsFusionUmbrellaBatchKind(pd->kind)){GetNativeSourceZombieData(z)->originSpeed=0;return;}
        bool caltrop=false,nut=false;int level=0;if(!TryGetSpecialZombie269SourcePlantTraits(pd->kind,caltrop,nut,level)||caltrop)continue;
        // Actual source pumpkin covers the other same-cell layers.
        bool covered=false;Plant* cover=nullptr;while(z->mBoard->IteratePlants(cover))if(PlantLive(cover)&&cover!=p&&cover->mRow==p->mRow&&cover->mPlantCol==p->mPlantCol)
            if(const auto* cd=GetNativePlantData(cover))if(FindFusionDefenderProfile(cd->kind)&&FindFusionDefenderProfile(cd->kind)->layer==FusionDefenderLayer::Pumpkin)covered=true;
        if(covered)continue;
        bool bounce=false;if(p->mPlantHealth>=1000)
        {ApplyNativeSourcePlantDamage(p,1000,0);auto* d=GetNativeSourceZombieData(z);d->originSpeed=(std::max)(.2f,d->originSpeed*.6f);bounce=true;}
        else if(pd->kind==903||pd->kind==913||pd->kind==921||pd->kind==925||pd->kind==931||pd->kind==1048||pd->kind==1151||nut&&level>=3)
        {ApplyNativeSourcePlantDamage(p,nut&&level>=3?1000:500,0);z->mPosX+=float(kFusionBasePixelsX);bounce=true;}
        else{p->Squish();bounce=true;}
        if(bounce){StartMachineRow(z,s);return;}
    }
    Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))if(TargetLive(z,q)&&Touch(r,q->GetZombieRect()))
    {q->TakeDamage(20,1U);StartMachineRow(z,s);return;}
}
bool Enter(const std::vector<NativeUltimateDeps270Contact>& old,uint32_t generation,bool zombie)
{return !std::any_of(old.begin(),old.end(),[&](const auto& c){return c.generation==generation&&c.zombie==zombie;});}
bool DieBall(Projectile* p)
{
    const auto* s=GetNativeUltimateMachineBall270(p);if(!s||p->mDead)return false;const uint16_t fx=s->type?64:63;
    if(!EnsureNativeSourceEffect269(p->mBoard,fx))return false;
    SpawnNativeSourceEffect269(p->mBoard,fx,p->mPosX+20,p->mPosY+20-float(.5*GetNativeFusionPixelsY(p->mBoard)),p->mRenderOrder,true);p->Die();return true;
}
}
bool IsNativeUltimateDeps270(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d&&FindFusionUltimateDeps270Profile(d->kind)&&IsNativeSourceZombie(z);}
bool CanCreateNativeUltimateDeps270(Board* b,uint16_t kind,int row)
{
    if(!b||!IsNativeFusionBoard(b)||!FindFusionUltimateDeps270Profile(kind)||row<0||row>=GetNativeFusionRowCount(b)||b->mPlantRow[row]!=PLANTROW_NORMAL||!Npc(b,kind,row))return false;
    if(kind==213)return EnsureNativeSourceEffect269(b,11);
    if(kind==37)return true; // Ground SetZombie creation; thrown37 is not admitted by this adapter.
    return FusionMachine270CanReserveOffspring(b->mZombies.mSize,b->mZombies.mMaxSize,true)&&CanCreateNativeUltimateDeps270(b,37,row)&&CanCreateNativeEquipment(b,FusionEquipmentType::Machine)&&
        EnsureNativeUltimateMachineBall270Visual(b,0)&&EnsureNativeUltimateMachineBall270Visual(b,1)&&
        EnsureNativeSourceEffect269(b,54)&&EnsureNativeSourceEffect269(b,63)&&EnsureNativeSourceEffect269(b,64);
}
bool InitializeNativeUltimateDeps270(Zombie* z)
{
    if(!IsNativeUltimateDeps270(z))return false;if(!Host(z))return true;auto* s=EditNativeUltimateDeps270(z,true);if(!s)return false;*s={};
    const auto kind=GetNativeSourceZombieData(z)->kind;z->mIsEating=false;z->mZombiePhase=kind==213?PHASE_IMP_LANDING:PHASE_ZOMBIE_NORMAL;
    if(kind==37)s->visible=false;if(kind==40)ChooseFusionMachine270Type(RandRangeInt(0,2),s->machineType);
    StartNativeUltimateDeps270Walk(z,0);return true;
}
bool UpdateNativeUltimateDeps270Animation(Zombie* z)
{
    if(!IsNativeUltimateDeps270(z))return false;if(!Host(z))return true;auto* r=Body(z);if(!r)return true;const auto* d=GetNativeSourceZombieData(z);
    const bool enter=Entering(z),dead=z->IsDeadOrDying();const float duration=d->kind==40?2.f:enter?.8666667f:dead?1.5833334f:z->mIsEating?2.1666667f:2.5833333f;
    const float state=d->kind==40?.5f:enter?.4f:z->mIsEating?(d->kind==213?2.f:1.f):!dead&&d->kind==213?1.5f:1.f;
    r->mAnimRate=r->mFrameCount*state*d->originSpeed*(z->mChilledCounter>0?.5f:1)*(z->IsImmobilizied()?0:1)/duration;z->mOriginalAnimRate=r->mAnimRate;return true;
}
bool StartNativeUltimateDeps270Walk(Zombie* z,int blend)
{
    if(!IsNativeUltimateDeps270(z))return false;if(!Host(z))return true;
    // The real native IMP already has anim_land. Its body clock is retimed to
    // the source213 entry clip; source presentation remains state"walk".
    Play(z,Entering(z)?"anim_land":"anim_walk",Entering(z)?REANIM_PLAY_ONCE_AND_HOLD:REANIM_LOOP,blend);
    if(auto* c=Cursor(z))if(auto* r=Body(z)){c->motionFrameStart=r->mFrameStart;c->motionValid=Position(r,Entering(z)?.8666667f:GetNativeSourceZombieData(z)->kind==40?2.f:2.5833333f,c->motion);}
    return true;
}
bool MoveNativeUltimateDeps270(Zombie* z)
{
    if(!IsNativeUltimateDeps270(z))return false;if(!Host(z))return true;auto* c=Cursor(z);auto* r=Body(z);FusionZombieClipPosition p;
    const auto kind=GetNativeSourceZombieData(z)->kind;const bool enter=Entering(z);if(!c||!Position(r,enter?.8666667f:kind==40?2.f:2.5833333f,p))return true;
    const bool moving=!z->IsDeadOrDying()&&!z->IsImmobilizied()&&!z->mIsEating;double delta=0;
    if(moving&&c->motionValid&&c->motionFrameStart==r->mFrameStart&&TryGetFusionUltimateDeps270RootDelta(kind,enter,c->motion,p,z->mMindControlled,
        (kind==40?.25:kind==213?.35:.36)*kFusionBasePixelsX*z->mScaleZombie,delta)&&std::isfinite(z->mPosX+delta))z->mPosX+=float(delta);
    c->motion=p;c->motionFrameStart=r->mFrameStart;c->motionValid=moving;return true;
}
bool UpdateNativeUltimateDeps270(Zombie* z)
{
    if(!IsNativeUltimateDeps270(z))return false;if(!Host(z)||z->mDead||z->IsDeadOrDying())return true;auto* s=EditNativeUltimateDeps270(z);if(!s)return true;
    const auto kind=GetNativeSourceZombieData(z)->kind;auto* r=Body(z);auto* c=Cursor(z);if(!r||!c)return true;
    if(kind==37)s->visible=true;
    if(kind==40)
    {
        if(s->moving)
        {s->rowTicks=uint16_t((std::max)(0,(std::min)(200,z->mPhaseCounter)));const float target=z->GetPosYBasedOnRow(s->targetRow);
         z->mPosY=s->startY+(target-s->startY)*float(200-s->rowTicks)/200;z->mY=int(z->mPosY);
         if(!s->rowTicks){z->SetRow(s->targetRow);s->moving=false;s->originRow=s->targetRow=-1;s->startY=0;}}
        else if(z->mBoard->mMainCounter%2==0)MachineContacts(z,*s);return true;
    }
    FusionZombieClipPosition current;
    if(Entering(z))
    {
        if(!Position(r,.8666667f,current))return true;uint32_t events=0;
        if(c->attackValid&&CountFusionUltimateDeps270Events(kind,true,c->attack,current,events)&&events)
        {float x=0,y=0;if(GetNativeSourceZombie269NodePoint(z,"",x,y)&&EnsureNativeSourceEffect269(z->mBoard,11))
         {SpawnNativeSourceEffect269(z->mBoard,11,x,y,z->mRenderOrder,true);z->mZombiePhase=PHASE_ZOMBIE_NORMAL;StartNativeUltimateDeps270Walk(z,0);return true;}}
        c->attack=current;c->attackValid=true;return true;
    }
    auto* d=GetNativeSourceZombieData(z);if(d->targetEntity==NativeZombieTargetEntity::None){if(!z->IsImmobilizied()&&z->mHasHead)Acquire(z);return true;}
    if(!LockedPlant(z)&&!LockedZombie(z)){ClearTarget(z);return true;}
    if(!Position(r,2.1666667f,current))return true;uint32_t events=0;
    if(c->attackValid&&c->attackFrameStart==r->mFrameStart)CountFusionUltimateDeps270Events(kind,false,c->attack,current,events);
    c->attack=current;c->attackFrameStart=r->mFrameStart;c->attackValid=true;
    for(uint32_t i=0;i<events;++i)
    {if(z->IsImmobilizied())break;if(auto* p=LockedPlant(z)){if(!HandleNativeBatchPlantEaten(z,p))z->EatPlant(p);}else if(auto* q=LockedZombie(z))z->EatZombie(q);else break;}
    return true;
}
bool GetNativeUltimateDeps270Presentation(const Zombie* z,const char*& clip,float& phase)
{
    if(!IsNativeUltimateDeps270(z)||!Body(z))return false;const auto kind=GetNativeSourceZombieData(z)->kind;
    clip=const_cast<Zombie*>(z)->IsDeadOrDying()?"die":Entering(z)?"walk":z->mIsEating?"attack":kind==213?"walk2":"walk";
    phase=Body(z)->mAnimTime;return std::isfinite(phase)&&phase>=0&&phase<=1;
}
bool GetNativeUltimateDeps270Status(const Zombie* z,int& status)
{if(!IsNativeUltimateDeps270(z))return false;status=const_cast<Zombie*>(z)->IsDeadOrDying()?1:Entering(z)?16:0;return true;}
bool GetNativeUltimateDeps270Rect(const Zombie* z,Sexy::Rect& out)
{
    if(!IsNativeUltimateDeps270(z))return false;float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"",x,y))return false;
    const auto kind=GetNativeSourceZombieData(z)->kind;const double scale=(kind==40?.25:kind==213?.35:.36)*z->mScaleZombie;
    const double ox=kind==40?0:kind==213?-.19353771:-.48629588,oy=kind==40?0:kind==213?2.123322:2.086319;
    const double w=kind==40?4.2782784:kind==213?2.405696:1.9725915,h=kind==40?5.8695273:kind==213?3.1686592:3.3280709;
    const double px=scale*kFusionBasePixelsX,py=scale*GetNativeFusionPixelsY(z->mBoard);
    out=Sexy::Rect(int(std::floor(x+(ox*(z->mMindControlled?-1:1)-w*.5)*px)),int(std::floor(y-(oy+h*.5)*py)),int(std::ceil(w*px)),int(std::ceil(h*py)));return true;
}
bool ApplyNativeUltimateDeps270BodyDamage(Zombie* z,int damage)
{
    if(!IsNativeUltimateDeps270(z)||GetNativeSourceZombieData(z)->kind!=40)return false;if(!Host(z)||damage<0||z->mDead||z->IsDeadOrDying())return true;
    auto* s=EditNativeUltimateDeps270(z);if(!s)return true;auto* d=GetNativeSourceZombieData(z);d->health-=float(damage);z->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);
    s->bodyStage=FusionMachine270DamageStage(d->health,z->mBodyMaxHealth,s->bodyStage);return true;
}
void NativeUltimateDeps270AfterKnockback(Zombie* z,int reason)
{if(IsNativeUltimateDeps270(z)&&GetNativeSourceZombieData(z)->kind==40&&Host(z)&&reason==0)GetNativeSourceZombieData(z)->originSpeed=0;}
bool NativeUltimateDeps270Died(Zombie* z)
{
    if(!IsNativeUltimateDeps270(z)||GetNativeSourceZombieData(z)->kind!=40)return false;if(!Host(z))return false;auto* s=EditNativeUltimateDeps270(z);if(!s)return true;
    if(s->deathExecuting)return false;
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"",x,y)||!EnsureNativeSourceEffect269(z->mBoard,54))return true;
    if(s->machineType==0)
    {if(!CanCreateNativeEquipment(z->mBoard,FusionEquipmentType::Machine))return true;
     s->deathExecuting=true;
     SpawnNativeSourceEffect269(z->mBoard,54,x,y-float(.6*GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder,true);
     if(CreateNativeEquipment(z->mBoard,FusionEquipmentType::Machine,x,y-float(GetNativeFusionPixelsY(z->mBoard)),true))z->DieWithLoot();else s->deathExecuting=false;return true;}
    if(!s->moving&&(!Npc(z->mBoard,37,z->mRow)||!FusionMachine270CanReserveOffspring(z->mBoard->mZombies.mSize,z->mBoard->mZombies.mMaxSize,false)||
       (!z->mMindControlled&&(!EnsureNativeUltimateMachineBall270Visual(z->mBoard,s->machineType-1)||z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize))))return true;
    s->deathExecuting=true;
    SpawnNativeSourceEffect269(z->mBoard,54,x,y-float(.6*GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder,true);if(s->moving){z->DieWithLoot();return true;}
    Projectile* ball=nullptr;if(!z->mMindControlled&&!(ball=CreateNativeUltimateMachineBall270(z->mBoard,s->machineType-1,x,y,z->mRow,z->mRenderOrder))){s->deathExecuting=false;return true;}
    std::vector<Zombie*> children;
    for(int i=0;i<kFusionMachine270OffspringCount;++i){NativeSourceZombieSpawnRequest r;r.kind=37;r.row=z->mRow;r.positioned=true;r.shadowX=x+RandRangeFloat(-1,1)*float(kFusionBasePixelsX);r.friendly=z->mMindControlled;r.withEffect=false;
     auto* child=SpawnNativeSourceZombie(z->mBoard,r);if(!child){for(auto* born:children)born->DieNoLoot();if(ball)ball->Die();s->deathExecuting=false;return true;}children.push_back(child);}
    z->DieWithLoot();return true;
}
Projectile* CreateNativeUltimateMachineBall270(Board* b,uint8_t type,float x,float y,int row,int order)
{
    if(!b||!b->mApp||b->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(b)||row<0||row>=GetNativeFusionRowCount(b)||
       !std::isfinite(x)||!std::isfinite(y)||type>1||!EnsureNativeUltimateMachineBall270Visual(b,type)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return nullptr;
    auto* p=b->AddProjectile(int(x-20),int(y-20),order,row,PROJECTILE_PEA);if(!p)return nullptr;
    p->mMotionType=MOTION_STAR;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=p->mVelZ=p->mAccZ=0;p->mShadowY=y;
    p->mVelX=float(-.5*kFusionBasePixelsX*.01);p->mVelY=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    auto* s=EditNativeUltimateMachineBall270(p,true);if(!s){p->Die();return nullptr;}
    s->instanceToken=b->mProjectiles.DataArrayGetID(p);s->type=type;s->shots=0;s->ageTicks=p->mProjectileAge;s->nextShotAge=0;return p;
}
bool UpdateNativeUltimateMachineBall270(Projectile* p)
{
    const auto* read=GetNativeUltimateMachineBall270(p);if(!read)return false;if(!Host(p)||p->mDead)return true;
    auto* s=EditNativeUltimateMachineBall270(p);s->ageTicks=p->mProjectileAge;if(p->mProjectileAge>3000){DieBall(p);return true;}
    if(p->mVelX<0)p->mVelX=(std::min)(0.f,p->mVelX+float(.25*kFusionBasePixelsX*.0001));
    p->mPosX+=p->mVelX;p->mX=int(p->mPosX);Sexy::Rect rect;if(!GetNativeUltimateMachineBall270Rect(p,rect))return true;
    auto* old=EditNativeUltimateMachineBall270Contacts(p);if(!old)return true;std::vector<NativeUltimateDeps270Contact> contacts;
    Plant* plant=nullptr;while(p->mBoard->IteratePlants(plant))if(PlantLive(plant)&&plant->mRow==p->mRow&&Touch(rect,plant->GetPlantRect()))
    {const uint32_t gen=p->mBoard->mPlants.DataArrayGetID(plant);contacts.push_back({gen,false});if(!Enter(*old,gen,false))continue;
     const auto* d=GetNativePlantData(plant);if(!d)continue;bool caltrop=false,nut=false;int level=0;if(!TryGetSpecialZombie269SourcePlantTraits(d->kind,caltrop,nut,level))continue;
     if(nut){ApplyNativeSourcePlantDamage(plant,500,0);DieBall(p);break;}if(!caltrop)plant->Squish();}
    if(p->mDead)return true;Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&z->mRow==p->mRow&&
        GetNativeFusionZombieStatus(z)!=10&&GetNativeFusionZombieStatus(z)!=12&&Touch(rect,z->GetZombieRect()))
    {const uint32_t gen=p->mBoard->mZombies.DataArrayGetID(z);contacts.push_back({gen,true});if(Enter(*old,gen,true))z->TakeDamage(20,1U);}
    Projectile* q=nullptr;while(p->mBoard->IterateProjectiles(q))if(q!=p&&!q->mDead&&Touch(rect,q->GetProjectileRect()))
    {NativeProjectileData legacy;const auto* d=GetNativeProjectileData(q);
     if(!d&&q->mProjectileType==PROJECTILE_PEA&&q->mMotionType==MOTION_STRAIGHT&&q->GetProjectileDef().mDamage==20)d=&legacy;
     if(!d)continue;const uint8_t way=IsNativeLobberProjectile(q)?13:d->specialType;
     if(FusionMachine270CanHitProjectile(way,d->isZombieBullet,q->mRow==p->mRow))
     {q->Die();s=EditNativeUltimateMachineBall270(p);s->nextShotAge=p->mProjectileAge;if(++s->shots>=31){DieBall(p);break;}}}
    if(!p->mDead)*old=std::move(contacts);return true;
}
}
