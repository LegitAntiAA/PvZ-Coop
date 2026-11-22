#include "FusionNativeExtendedZombie347.h"
#include "FusionNativeObstacle313.h"
#include "FusionDrown343Rules.h"
#include "FusionNative.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombies.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeEffects269.h"
#include "FusionNativePlacement.h"
#include "FusionUltimateDeps270Rules.h"
#include "FusionBasePlantRules.h"
#include "FusionAnimation.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <fstream>
#include "../../third_party/nlohmann/json.hpp"
#include <windows.h>
namespace LawnFusion
{
namespace
{
bool Reject(const char* why,std::string* error){if(error)*error=why;return false;}
bool World(const Board* b){return IsNativeSourceWorld313(b);}
bool Host(const Projectile* p){return p&&World(p->mBoard)&&!p->mNetGhost&&!p->mApp->NetIsClient();}
bool Live(const Plant* p){return p&&!p->mDead&&!p->mSquished&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p)&&NativeSourcePlantColliderEnabled(p);}
bool Touch(const Sexy::Rect& a,const Sexy::Rect& b){return a.mX<=b.mX+b.mWidth&&a.mX+a.mWidth>=b.mX&&a.mY<=b.mY+b.mHeight&&a.mY+a.mHeight>=b.mY;}
NativeSourceObstacle313Visuals* Visuals(Board* b,bool create)
{auto* c=GetNativeContext(b);if(!c)return nullptr;if(!c->sourceObstacleVisuals313&&create)c->sourceObstacleVisuals313=std::make_unique<NativeSourceObstacle313Visuals>(b);return c->sourceObstacleVisuals313.get();}
std::filesystem::path Path(FusionSourceObstacle313Kind kind,uint8_t variant)
{wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);return std::filesystem::path(exe).parent_path()/"fusion"/"pilot"/(kind==FusionSourceObstacle313Kind::DrownWeapon?(variant&2?"source-obstacle343-1-big":"source-obstacle313-1"):("obstacle313-"+std::to_string(unsigned(kind))+"-"+std::to_string(variant)))/"animation.json";}
Reanimation* Clock(const Projectile* p,const NativeSourceObstacle313Data& d){return p&&d.clockUID?p->mApp->ReanimationTryToGet(ReanimationID(d.clockUID)):nullptr;}
bool ClockReady(Board* b,const Projectile* p=nullptr)
{
    auto* fx=b->mApp->mEffectSystem;if(!fx||fx!=gEffectSystem||!fx->mReanimationHolder||!fx->mAttachmentHolder)return false;
    const auto& r=fx->mReanimationHolder->mReanimations;auto& a=fx->mAttachmentHolder->mAttachments;
    if(!r.mBlock||r.mSize>=r.mMaxSize||!a.mBlock)return false;
    if(p){const auto* old=a.DataArrayTryToGet(unsigned(p->mAttachmentID));if(old&&!old->mDead)return old->mNumEffects>=0&&old->mNumEffects<MAX_EFFECTS_PER_ATTACHMENT;}
    return a.mSize<a.mMaxSize;
}
bool AllocateClock(Projectile* p,NativeSourceObstacle313Data& d)
{
    if(auto* r=Clock(p,d))return !r->mDead&&r->mReanimationType==REANIM_ZOMBIE&&r->TrackExists("anim_walk");
    if(!ClockReady(p->mBoard,p))return false;
    auto* r=p->mApp->AddReanimation(0,0,0,REANIM_ZOMBIE);if(!r||!r->TrackExists("anim_walk"))return false;
    r->PlayReanim("anim_walk",REANIM_LOOP,0,0);
    for(int t=0;t<r->mDefinition->mTracks.count;++t)r->mTrackInstances[t].mForceDontRender=true;
    if(!AttachReanim(p->mAttachmentID,r,0,0)){r->ReanimationDie();return false;}
    d.clockUID=uint32_t(p->mApp->ReanimationGetID(r));return RegisterNativeSourceGraphClock313(r,true);
}
bool Point(const Projectile* p,float& sx,float& sy)
{return NativeBoardPointToSource313(p->mBoard,p->mRow,p->mShadowY,p->mPosX+20,p->mPosY+20,sx,sy);}
bool Place(Projectile* p,float sx,float sy)
{float x=0,y=0;if(!NativeSourcePointToBoard313(p->mBoard,p->mRow,p->mShadowY,sx,sy,x,y))return false;p->mPosX=x-20;p->mPosY=y-20;p->mX=int(p->mPosX);p->mY=int(p->mPosY);return true;}
void Failed(Projectile* p,const char* why)
{if(auto* d=EditNativeSourceObstacle313(p))d->failed=true;if(auto* c=GetNativeContext(p->mBoard);c&&c->adventure.enabled){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}LawnNet::NetTrace("Fusion SourceObstacle313 stopped kind=%u id=%u age=%d reason=%s",unsigned(GetNativeSourceObstacle313(p)->kind),p->mNetId,p->mProjectileAge,why);}
void Fx(Projectile* p,uint16_t source,float x,float y,int row=-1)
{SpawnNativeSourceEffect269(p->mBoard,source,x,y,Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row<0?p->mRow:row,0),true);}
bool DieBall(Projectile* p,NativeSourceObstacle313Data& d)
{
    const uint16_t fx=d.variant?64:63;if(!EnsureNativeSourceEffect269(p->mBoard,fx))return false;
    // ZombieBall.Die uses source row11, not its contact row.
    Fx(p,fx,p->mPosX+20,p->mPosY+20-float(.5*GetNativeFusionPixelsY(p->mBoard)),11);d.exploded=true;p->Die();return true;
}
bool Kirov(Projectile* p,NativeSourceObstacle313Data& d)
{
    const float age=float(p->mProjectileAge)*.01f;p->mPosY+=age*.01f*float(GetNativeFusionPixelsY(p->mBoard));p->mY=int(p->mPosY);
    p->mVelY=age*.01f*float(GetNativeFusionPixelsY(p->mBoard));
    if(age<=1||d.exploded)return true;
    if(!EnsureNativeSourceEffect269(p->mBoard,41)||!LawnNet::NetSourceSound284Ready(p->mApp,43))return false;
    const float x=p->mPosX+20,y=p->mPosY+20,px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(p->mBoard));
    const Sexy::Rect box(int(x-.75f*px),int(y-.75f*py),int(1.5f*px),int(1.5f*py));
    std::vector<NativeSourceObstacle313Contact> targets;
    if(!d.fromHypno){Plant* q=nullptr;while(p->mBoard->IteratePlants(q))if(Live(q)&&q->mRow==p->mRow&&Touch(box,q->GetPlantRect()))targets.push_back({p->mBoard->mPlants.DataArrayGetID(q),1});}
    else {Zombie* q=nullptr;while(p->mBoard->IterateZombies(q)){const int s=GetNativeFusionZombieStatus(q);if(!q->mDead&&!q->mMindControlled&&q->mRow==p->mRow&&GetNativeSourceZombieData(q)&&s!=1&&s!=10&&s!=12&&(s<20||s>24)&&Touch(box,q->GetZombieRect()))targets.push_back({p->mBoard->mZombies.DataArrayGetID(q),2});}}
    d.exploded=true; // Consume before recursive damage; owner can be long gone.
    auto* b=p->mBoard;auto* app=p->mApp;const uint32_t uid=b->mProjectiles.DataArrayGetID(p);
    for(const auto& hit:targets)
    {
        if(app->mBoard!=b)return true;
        if(hit.entity==1){if(auto* q=b->mPlants.DataArrayTryToGet(hit.generation);Live(q)){ApplyNativeSourcePlantDamage(q,1000,2);if(auto* a=b->mPlants.DataArrayTryToGet(hit.generation);a&&!a->mDead)a->mEatenFlashCountdown=20;}}
        else if(auto* q=b->mZombies.DataArrayTryToGet(hit.generation);q&&!q->mDead&&!ApplyNativeSourceTypedDamage313(q,10,1000))return false;
    }
    if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
    LawnNet::NetPlaySourceSound284(app,43,.5f);Fx(p,41,x,y);b->ShakeBoard(3,3);p->Die();return true;
}
bool Super(Projectile* p,NativeSourceObstacle313Data& d)
{
    float sx=0,sy=0,land=0;if(!Point(p,sx,sy))return false;
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(p->mBoard));
    const float vx=p->mVelX/(px*.01f),vy=-p->mVelY/(py*.01f)-.1f;
    sx+=vx*.01f;sy+=vy*.01f;if(!Place(p,sx,sy)||!NativeSourceRowLandY313(p->mBoard,sx,p->mRow,land))return false;p->mVelY=-vy*py*.01f;
    if(d.exploded||sy>=land+.5f||vy>=0)return true;
    const uint16_t child=40;
    if(!EnsureNativeSourceZombieClosure280(p->mBoard,std::span(&child,1))||!CanAllocateNativeSourceZombieBatch280(p->mBoard,std::span(&child,1))||!EnsureNativeSourceEffect269(p->mBoard,41)||!EnsureNativeSourceEffect269(p->mBoard,42)||!LawnNet::NetSourceSound284Ready(p->mApp,43))return false;
    d.exploded=true;auto* b=p->mBoard;auto* app=p->mApp;const uint32_t uid=b->mProjectiles.DataArrayGetID(p);const float x=p->mPosX+20,y=p->mPosY+20;
    Fx(p,41,x,y);LawnNet::NetPlaySourceSound284(app,43,.5f);b->ShakeBoard(3,3);
    const float jitter=RandRangeFloat(-1,1); // Only after complete child/particle capacity/resource closure.
    if(!SpawnNativeExtendedZombieAtAxis313(b,40,p->mRow,sx+jitter,land,false))return false;
    if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
    LawnNet::NetPlaySourceSound284(app,43,.5f);b->ShakeBoard(9,9);Fx(p,42,x,y);p->Die();return true;
}
bool Machine(Projectile* p,NativeSourceObstacle313Data& d)
{
    if(p->mProjectileAge>3000)return DieBall(p,d);
    float sx=0,sy=0,land=0;if(!Point(p,sx,sy))return false;
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(p->mBoard));
    float vx=p->mVelX/(px*.01f);const bool slope=p->mBoard->StageHasRoof()&&sx<1.5f;
    if(slope)vx-=.002f;else if(vx<0)vx=(std::min)(0.f,vx+.0025f);
    const float vy=slope?.22f*vx:0;auto* r=Clock(p,d);if(!r||r->mDead||r->mFrameCount<=0)return false;
    // Source move clip speed.3 and Animator.Speed=-sourceVX; native R owns phase.
    r->mAnimRate=-vx*.3f*r->mFrameCount;r->mLastFrameTime=r->mAnimTime;
    const double next=double(r->mAnimTime)+double(-vx)*.003;const int loops=int(std::floor(next));r->mAnimTime=float(next-loops);r->mLoopCount+=loops;
    p->mVelX=vx*px*.01f;p->mVelY=-vy*py*.01f;if(!Place(p,sx+vx*.01f,sy+vy*.01f))return false;
    Sexy::Rect rect;if(!GetNativeExtendedObstacle313Rect(p,rect))return false;
    std::vector<NativeSourceObstacle313Contact> hits;Plant* plant=nullptr;Zombie* zombie=nullptr;Projectile* shot=nullptr;
    while(p->mBoard->IteratePlants(plant))if(Live(plant)&&plant->mRow==p->mRow&&Touch(rect,plant->GetPlantRect()))hits.push_back({p->mBoard->mPlants.DataArrayGetID(plant),1});
    while(p->mBoard->IterateZombies(zombie)){const int s=GetNativeFusionZombieStatus(zombie);if(!zombie->mDead&&!zombie->mMindControlled&&zombie->mRow==p->mRow&&GetNativeSourceZombieData(zombie)&&s!=10&&s!=12&&Touch(rect,zombie->GetZombieRect()))hits.push_back({p->mBoard->mZombies.DataArrayGetID(zombie),2});}
    while(p->mBoard->mProjectiles.IterateNext(shot)){const auto* q=GetNativeProjectileData(shot);if(shot!=p&&!shot->mDead&&q&&FusionMachine270CanHitProjectile(q->specialType,q->isZombieBullet,p->mRow==shot->mRow)&&Touch(rect,shot->GetProjectileRect()))hits.push_back({p->mBoard->mProjectiles.DataArrayGetID(shot),3});}
    const auto old=d.entered;d.entered=hits;auto* b=p->mBoard;auto* app=p->mApp;const uint32_t uid=b->mProjectiles.DataArrayGetID(p),token=d.instanceToken;
    for(const auto& hit:hits)
    {
        if(std::any_of(old.begin(),old.end(),[&](const auto& q){return q.entity==hit.entity&&q.generation==hit.generation;}))continue;
        if(hit.entity==1)
        {
            auto* q=b->mPlants.DataArrayTryToGet(hit.generation);if(!Live(q))continue;bool cal=false,nut=false;int level=0;
            if(!TryGetSpecialZombie269SourcePlantTraits(GetNativePlantData(q)->kind,cal,nut,level))return false;
            if(nut){ApplyNativeSourcePlantDamage(q,500,0);if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;auto* current=EditNativeSourceObstacle313(p);if(!current||current->instanceToken!=token)return false;q=b->mPlants.DataArrayTryToGet(hit.generation);if(q&&!q->mDead)q->mEatenFlashCountdown=25;return DieBall(p,*current);}
            if(cal)continue;const int sound=RandRangeInt(8,9);LawnNet::NetPlaySourceSound284(app,uint16_t(sound),.5f);q->Die(0);
        }
        else if(hit.entity==2){if(auto* q=b->mZombies.DataArrayTryToGet(hit.generation);q&&!q->mDead&&!ApplyNativeSourceTypedDamage313(q,4,20))return false;}
        else if(auto* q=b->mProjectiles.DataArrayTryToGet(hit.generation);q&&!q->mDead){q->Die();d.lastHitAge=uint32_t(p->mProjectileAge);if(++d.hitCount>=31)return DieBall(p,d);}
        if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
        auto* current=EditNativeSourceObstacle313(p);if(!current||current->instanceToken!=token)return false;
    }
    return true;
}
}
struct NativeSourceObstacle313Visuals::Impl
{
    Board* board;std::map<unsigned,std::unique_ptr<FusionAnimation>> models;std::map<unsigned,int64_t> drownBoxes;
    explicit Impl(Board* b):board(b){}
    const FusionAnimation* Find(FusionSourceObstacle313Kind k,uint8_t v)const{auto i=models.find(unsigned(k)*256+v);return i==models.end()?nullptr:i->second.get();}
};
NativeSourceObstacle313Visuals::NativeSourceObstacle313Visuals(Board* b):mImpl(std::make_unique<Impl>(b)){}
NativeSourceObstacle313Visuals::~NativeSourceObstacle313Visuals()=default;
bool NativeSourceObstacle313Visuals::Ensure(FusionSourceObstacle313Kind k,uint8_t v,std::string& error)
{if(unsigned(k)<1||unsigned(k)>5||(k==FusionSourceObstacle313Kind::DrownWeapon?v>3:k==FusionSourceObstacle313Kind::SpiderWeapon?v!=0:v>1))return false;if(mImpl->Find(k,v))return true;auto a=std::make_unique<FusionAnimation>();if(!a->Load(mImpl->board->mApp,Path(k,v).string(),error))return false;if(k==FusionSourceObstacle313Kind::DrownWeapon){
 try{std::ifstream file(Path(k,v));nlohmann::json j;file>>j;
  if(!j.value("static",false)||j.value("sourcePrefab",std::string{})!=((v&2)?"Resources/zombies/zombie_poseidon/weapon.prefab":"Resources/zombies/zombie_drown/weapon.prefab")||j.at("colliders").size()!=1){error="Drown genuine static prefab/collider contract differs";return false;}
  const auto& box=j.at("colliders").at(0);if(box.value("type",std::string{})!="box"||box.value("node",-1)!=0||!box.value("enabled",false)||!box.value("trigger",false)){error="Drown genuine root BoxCollider2D differs";return false;}
  const int64_t id=box.at("fileId").get<int64_t>();if(id<=0)return false;
  if(v&2){auto found=std::find_if(j.at("nodes").begin(),j.at("nodes").end(),[](const auto& n){return n.value("path",std::string{})=="dirt";});
   if(found==j.at("nodes").end()||found->at("parent")!=0){error="Drown big dirt world rotation hierarchy is not supported";return false;}}

  FusionAnimationBoxCollider303 test;FusionAnimationDrawOptions options;options.nodeRotations.push_back({"",0,true});
  if(!a->TryGetBoxCollider303(id,test,"",0,nullptr,-1,1,&options)||!test.enabled||!test.nodeActive){error="Drown actual root collider cannot be sampled";return false;}
  mImpl->drownBoxes[unsigned(k)*256+v]=id;
 }catch(const std::exception& ex){error=ex.what();return false;}}
if(k==FusionSourceObstacle313Kind::MachineBall&&(!a->GetDefaultStateName()||std::string(a->GetDefaultStateName())!="move"||std::abs(a->GetStateDurationSeconds("move")-1.f/.3f)>.0001f)){error="MachineBall genuine move Animator duration/speed differs.";return false;}mImpl->models.emplace(unsigned(k)*256+v,std::move(a));return true;}
float NativeObstacle313Brightness(const Projectile* p,const FusionSourceObstacle313View& d)
{if(p&&d.kind==FusionSourceObstacle313Kind::DrownWeapon)return (d.variant&2)?FusionDrown343Brightness(p->mProjectileAge,d.lastHitAge,d.drownLongFlash):1;
 if(!p||d.kind!=FusionSourceObstacle313Kind::MachineBall||!d.hitCount||p->mProjectileAge<int(d.lastHitAge))return 1;float t=.3f;const int steps=(p->mProjectileAge-int(d.lastHitAge))/2;if(steps>16)return 1;for(int i=0;i<steps;++i)t-=.02f;if(t<=0)return 1;return std::clamp(t>.2f?10-30*t:1+15*t,1.f,4.f);}
namespace {bool Pose(const Projectile* p,const NativeSourceObstacle313Data& d,const char*& state,float& seconds,FusionAnimationDrawOptions& options)
{state=d.kind==FusionSourceObstacle313Kind::MachineBall?"move":"";seconds=0;options.brightnessGain=NativeObstacle313Brightness(p,d);if(d.kind==FusionSourceObstacle313Kind::DrownWeapon){if(!std::isfinite(p->mRotation))return false;options.nodeRotations.push_back({"",p->mRotation,true});if(d.variant&2){options.nodeActivityOverrides.push_back({"dirt",d.landed});if(d.landed)options.nodeRotations.push_back({"dirt",-p->mRotation,true});}}if(d.kind!=FusionSourceObstacle313Kind::MachineBall)return true;auto* r=Clock(p,d);if(!r||r->mDead||r->mFrameCount<=0)return false;seconds=(float(r->mLoopCount)+r->mAnimTime)/.3f;return std::isfinite(seconds);}}
bool NativeSourceObstacle313Visuals::Draw(Sexy::Graphics* g,Projectile* p,const NativeSourceObstacle313Data& d)const
{const auto* a=mImpl->Find(d.kind,d.variant);if(!a)return false;const char* state=nullptr;float t=0;FusionAnimationDrawOptions o;if(!Pose(p,d,state,t,o))return false;a->DrawPose(g,p->mPosX+20-p->mX,p->mPosY+20-p->mY,.6f,state,t,nullptr,-1,1,&o);return true;}
bool NativeSourceObstacle313Visuals::Box313(const Projectile* p,const NativeSourceObstacle313Data& d,FusionAnimationBoxCollider303& out)const
{const auto* a=mImpl->Find(d.kind,d.variant);if(!a||(d.kind!=FusionSourceObstacle313Kind::MachineBall&&d.kind!=FusionSourceObstacle313Kind::DrownWeapon))return false;const char* state=nullptr;float t=0;FusionAnimationDrawOptions o;
 if(!Pose(p,d,state,t,o))return false;return a->TryGetBoxCollider303(d.kind==FusionSourceObstacle313Kind::DrownWeapon?mImpl->drownBoxes.at(unsigned(d.kind)*256+d.variant):61025901758691103LL,out,state,t,nullptr,-1,1,&o);}
bool NativeSourceObstacle313Visuals::Rect(const Projectile* p,const NativeSourceObstacle313Data& d,Sexy::Rect& out)const
{FusionAnimationBoxCollider303 box;if(!Box313(p,d,box)||!box.enabled||!box.nodeActive)return false;const float x=p->mPosX+20,y=p->mPosY+20;const int left=int(std::floor(x+60*box.aabb[0])),top=int(std::floor(y-60*box.aabb[3]));out=Sexy::Rect(left,top,int(std::ceil(x+60*box.aabb[2]))-left,int(std::ceil(y-60*box.aabb[1]))-top);return true;}
bool GetNativeSourceObstacle313Box(const Projectile* p,FusionAnimationBoxCollider303& out)
{const auto* d=GetNativeSourceObstacle313(p);auto* v=Visuals(p?p->mBoard:nullptr,false);return d&&v&&v->Box313(p,*d,out);}
NativeSourceObstacle313Data* EditNativeSourceObstacle313(Projectile* p,bool create)
{auto* c=GetNativeContext(p?p->mBoard:nullptr);if(!c||!p)return nullptr;const uint32_t uid=p->mBoard->mProjectiles.DataArrayGetID(p);if(!uid)return nullptr;auto i=c->sourceObstacles313.find(uid);if(i==c->sourceObstacles313.end()){if(!create)return nullptr;i=c->sourceObstacles313.emplace(uid,NativeSourceObstacle313Data{}).first;}return &i->second;}
const NativeSourceObstacle313Data* GetNativeSourceObstacle313(const Projectile* p){return EditNativeSourceObstacle313(const_cast<Projectile*>(p));}
namespace {bool Resources(Board* b,const FusionSourceObstacle313Spawn& s,std::string* error)
{
    if(!World(b)||unsigned(s.kind)<2||unsigned(s.kind)>5||s.row<0||s.row>=GetNativeFusionRowCount(b)||!s.ownerId||!s.ownerToken||s.variant>1||!std::isfinite(s.centerX)||!std::isfinite(s.centerY)||!std::isfinite(s.groundY))return Reject("Source obstacle actual world/factory identity unavailable.",error);
    if(s.targetPlantId||s.targetPlantToken||(s.kind!=FusionSourceObstacle313Kind::KirovBomb&&s.kind!=FusionSourceObstacle313Kind::SpiderWeapon&&s.fromHypno)||(s.kind==FusionSourceObstacle313Kind::SpiderWeapon&&s.variant))return false;
    std::string e;auto* v=Visuals(b,true);if(!v||!v->Ensure(s.kind,s.variant,e)){if(error)*error=e;return false;}
    for(uint16_t fx:s.kind==FusionSourceObstacle313Kind::MachineBall?std::initializer_list<uint16_t>{63,64}:std::initializer_list<uint16_t>{41,42})if(NativeSourceEffectParticle269(fx)<0||!EnsureNativeSourceEffect269(b,fx,error))return false;
    if(s.kind==FusionSourceObstacle313Kind::MachineBall){ReanimatorEnsureDefinitionLoaded(REANIM_ZOMBIE,false);if(!ClockReady(b))return Reject("MachineBall actual R/A peak unavailable.",error);}
    if(s.kind==FusionSourceObstacle313Kind::SuperBall){const uint16_t child=40;if(!EnsureNativeSourceZombieClosure280(b,std::span(&child,1),error)||!CanAllocateNativeSourceZombieBatch280(b,std::span(&child,1),error))return false;}
    return true;
}}
bool CanCreateNativeExtendedObstacle313(Board* b,const FusionSourceObstacle313Spawn& s,std::string* error)
{return Resources(b,s,error)&&b->mProjectiles.mBlock&&b->mProjectiles.mSize<b->mProjectiles.mMaxSize;}
bool InitializeNativeExtendedObstacle313(Projectile* p,const FusionSourceObstacle313Spawn& s,NativeSourceObstacle313Data& d,std::string* error)
{if(!p||!Resources(p->mBoard,s,error))return false;d={};d.kind=s.kind;d.instanceToken=p->mBoard->mProjectiles.DataArrayGetID(p);d.ownerId=s.ownerId;d.ownerToken=s.ownerToken;d.fromHypno=s.fromHypno;d.variant=s.variant;p->mPosX=s.centerX-20;p->mPosY=s.centerY-20;p->mPosZ=0;p->mShadowY=s.groundY;p->mRenderOrder=s.renderOrder;p->mVelX=p->mVelY=p->mVelZ=p->mAccZ=0;p->mRotation=0;p->mProjectileAge=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);if(s.kind==FusionSourceObstacle313Kind::SpiderWeapon)p->mVelX=(s.fromHypno?4.f:-4.f)*float(kFusionBasePixelsX)*.01f;
if(s.kind==FusionSourceObstacle313Kind::SuperBall){p->mVelX=-3.f*float(kFusionBasePixelsX)*.01f;p->mVelY=-2.f*float(GetNativeFusionPixelsY(p->mBoard))*.01f;}if(s.kind==FusionSourceObstacle313Kind::MachineBall){p->mVelX=-.5f*float(kFusionBasePixelsX)*.01f;if(!AllocateClock(p,d))return false;}return FusionSourceObstacle313ControllerValid(d);}
bool UpdateNativeExtendedObstacle313(Projectile* p)
{auto* d=EditNativeSourceObstacle313(p);if(!d||d->kind==FusionSourceObstacle313Kind::DrownWeapon)return false;if(!Host(p)||p->mDead||p->mBoard->mPaused||p->mApp->mGameScene!=SCENE_PLAYING||d->failed||d->tick==p->mBoard->mMainCounter)return true;d->tick=p->mBoard->mMainCounter;bool ok=false;if(d->kind==FusionSourceObstacle313Kind::KirovBomb)ok=Kirov(p,*d);else if(d->kind==FusionSourceObstacle313Kind::SuperBall)ok=Super(p,*d);else if(d->kind==FusionSourceObstacle313Kind::MachineBall)ok=Machine(p,*d);if(!ok&&!p->mDead)Failed(p,"Prepared source obstacle operation failed; no replay after mutation.");return true;}
bool DrawNativeExtendedObstacle313(Sexy::Graphics* g,Projectile* p){const auto* d=GetNativeSourceObstacle313(p);if(!d||d->kind==FusionSourceObstacle313Kind::DrownWeapon)return false;auto* v=Visuals(p->mBoard,false);if(v)v->Draw(g,p,*d);return true;}
bool GetNativeExtendedObstacle313Rect(const Projectile* p,Sexy::Rect& out){const auto* d=GetNativeSourceObstacle313(p);auto* v=Visuals(p?p->mBoard:nullptr,false);return d&&v&&v->Rect(p,*d,out);}
bool CaptureNativeObstacle313Clock(const Projectile* p,LawnNet::NetZombieAnimState& out)
{const auto* d=GetNativeSourceObstacle313(p);if(!d)return false;LawnNet::NetZombieAnimState c;if(d->kind!=FusionSourceObstacle313Kind::MachineBall){out=c;return true;}const auto* r=Clock(p,*d);if(!r||r->mFrameStart<0||r->mFrameCount<=0)return false;c.mPresent=true;c.mDead=r->mDead;c.mFrameStart=uint16_t(r->mFrameStart);c.mFrameCount=uint16_t(r->mFrameCount);c.mAnimTime=r->mAnimTime;c.mAnimRate=r->mAnimRate;c.mLoopType=uint8_t(r->mLoopType);c.mLoopCount=r->mLoopCount;if(!LawnNet::NetZombieAnimStateFitsFrames(c,r->mDefinition->mTracks.tracks[0].mTransforms.count))return false;out=c;return true;}
bool AdoptNativeObstacle313Clock(Projectile* p,NativeSourceObstacle313Data& d,const LawnNet::NetZombieAnimState& c)
{auto copy=d;copy.clock=c;if(!FusionSourceObstacle313ViewValid(copy))return false;if(d.kind!=FusionSourceObstacle313Kind::MachineBall)return true;if(!AllocateClock(p,d))return false;auto* r=Clock(p,d);if(!LawnNet::NetZombieAnimStateFitsFrames(c,r->mDefinition->mTracks.tracks[0].mTransforms.count))return false;r->mFrameStart=c.mFrameStart;r->mFrameCount=c.mFrameCount;r->mAnimTime=r->mLastFrameTime=c.mAnimTime;r->mAnimRate=c.mAnimRate;r->mLoopType=ReanimLoopType(c.mLoopType);r->mLoopCount=c.mLoopCount;d.clock=c;return true;}
void RemoveNativeObstacle313Clock(Projectile* p,NativeSourceObstacle313Data& d){if(auto* r=Clock(p,d))RegisterNativeSourceGraphClock313(r,false);d.clockUID=0;}
}
