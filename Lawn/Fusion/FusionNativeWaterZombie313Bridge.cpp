#include "FusionNativeWaterZombie313Bridge.h"
#include "FusionNativeDrown313Bridge.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeProjection313.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeWater268.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeSourceAnimated317.h"
#include "FusionAnimation.h"
#include "FusionBasePlantRules.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace LawnFusion
{
namespace
{
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool Host(const Zombie* z)
{return z&&z->mApp&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard)&&FusionWaterZombie313Kind(Kind(z));}
bool Playing(const Zombie* z){return Host(z)&&!z->mDead&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
bool Reject(const std::string& why,std::string* error){if(error)*error=why;return false;}
void Failed(Zombie* z,const char* why)
{
    if(auto* s=EditNativeWaterZombie313(z))s->failed=true;
    if(auto* c=GetNativeContext(z?z->mBoard:nullptr);c&&c->adventure.enabled){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}
    LawnNet::NetTrace("Fusion Water313 stopped kind=%u id=%u reason=%s",Kind(z),z?z->mNetId:0,why);
}
bool Pool(Board* b,size_t zombies,size_t reanimations,std::string* error)
{
    const auto* e=b&&b->mApp?b->mApp->mEffectSystem:nullptr;
    if(!e||e!=gEffectSystem||!e->mReanimationHolder)return Reject("Water313 actual native effect holder unavailable",error);
    const auto& r=e->mReanimationHolder->mReanimations;
    return b->mZombies.mBlock&&r.mBlock&&b->mZombies.mSize<=b->mZombies.mMaxSize&&r.mSize<=r.mMaxSize&&
        zombies<=size_t(b->mZombies.mMaxSize-b->mZombies.mSize)&&reanimations<=size_t(r.mMaxSize-r.mSize)||
        Reject("Water313 whole Z/R birth peak unavailable (no deletion credit)",error);
}
bool Definition(ReanimationType type,std::span<const char* const> tracks,std::string* error)
{
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return false;
    const auto& all=gReanimatorDefArray[type].mTracks;if(!all.tracks||all.count<=0)return false;
    for(int i=0;i<all.count;++i)if(all.tracks[i].mName&&_strnicmp(all.tracks[i].mName,"attacher__",10)==0)
        return Reject("Water313 native definition has an unbudgeted attacher__",error);
    for(const char* wanted:tracks)
    {bool found=false;for(int i=0;i<all.count;++i)if(all.tracks[i].mName&&!_stricmp(all.tracks[i].mName,wanted)&&all.tracks[i].mTransforms.count>0)found=true;
        if(!found)return Reject(std::string("Water313 actual native track unavailable: ")+wanted,error);}
    return true;
}
bool Resources(Board* b,uint16_t kind,std::string* error)
{
    if(!IsNativeSourceWorld313(b)||!FindFusionWaterZombie313Profile(kind))return Reject("Water313 source family/world unavailable",error);
    if(!EnsureNativeSourceZombieVisuals(b,kind,error)||!CanQueryNativeSourceZombieRoot273(b,kind))return false;
    std::vector<const char*> tracks;
    for(int phase=0;phase<=int(FusionWaterZombie313Phase::DrownDeath);++phase)
    {FusionWaterZombie313Clip clip;if(TryGetFusionWaterZombie313Clip(kind,FusionWaterZombie313Phase(phase),clip)&&
        std::none_of(tracks.begin(),tracks.end(),[&](const char* old){return !std::strcmp(old,clip.nativeTrack);}))tracks.push_back(clip.nativeTrack);}
    const auto carrier=kind==16?ZOMBIE_ZAMBONI:ZOMBIE_NORMAL;
    if(tracks.empty()||!Definition(GetZombieDefinition(carrier).mReanimationType,tracks,error))return false;
    const char* aux[]={"anim_walk"};if(kind!=20&&!Definition(REANIM_ZOMBIE,aux,error))return false;
    const auto sound=[&](uint16_t id){return LawnNet::NetSourceSound284Ready(b->mApp,id)||Reject("Water313 genuine sound unavailable: "+std::to_string(id),error);};
    // All own callback resources are prepared before the source startup draws.
    if(kind==16)
    {for(uint16_t id:std::array<uint16_t,7>{8,9,43,72,73,76,77})if(!sound(id))return false;
        return EnsureNativeSourceEffect269(b,34,error);}
    if(kind==20)
    {return sound(3)&&sound(4)&&EnsureNativeDrownWeapon313Visuals(b,error);}
    if(!EnsureNativeSourceAnimated317(b,3,0,error)||!EnsureNativeSourceEffect269(b,32,error)||!sound(75))return false;
    if(kind==19||kind==27)
        return EnsureNativeSourceEffect269(b,23,error)&&sound(64)&&sound(78)&&sound(79);
    return true;
}
bool Prepare(Zombie* z,NativeWaterZombie313Operation operation,std::string* error)
{
    if(!Host(z)||!Resources(z->mBoard,Kind(z),error))return false;
    if(operation==NativeWaterZombie313Operation::Initialize)
        return Pool(z->mBoard,0,Kind(z)==20?0:1,error);
    if(operation==NativeWaterZombie313Operation::Splash)return EnsureNativeSourceAnimated317(z->mBoard,3,1,error);
    // Death has no OWN gameplay descendants in any of these five families.
    // Existing source particle capacity drops are cosmetic, not callback replay.
    return true;
}
bool Contact(Zombie* z,bool assign){return NativeClockOnlyZombieContact309(z,assign);}
bool DrownContact(Zombie* z){return NativeClockOnlyZombieContact309(z,false);}
void Bite(Zombie* z){NativeClockOnlyZombieBite313(z);}
FusionContactQuad321 RectQuad(const Sexy::Rect& r)
{const float l=float(r.mX),t=float(r.mY),rr=float(r.mX+r.mWidth),b=float(r.mY+r.mHeight);return {{{l,t},{rr,t},{rr,b},{l,b}}};}
bool BodyQuad(const Zombie* z,FusionContactQuad321& out)
{
    FusionAnimationBoxCollider303 box;float x=0,y=0;
    if(!GetNativeSourceZombieBox313(z,box)||!box.enabled||!box.nodeActive||!GetNativeSourceZombie269NodePoint(z,"",x,y))return false;
    for(size_t i=0;i<4;++i)out[i]={x+60*z->mScaleZombie*box.corners[i][0],y-60*z->mScaleZombie*box.corners[i][1]};return true;
}
bool DolphinContact(Zombie* z,bool& blocked,float& failX,float& failY)
{
    if(z->mBoard->mMainCounter&1)return false; // Source FixedUpdate50Hz.
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(z->mBoard));
    const float left=x-.75f*px,right=x-.05f*px,top=y-2*py,bottom=y;
    const FusionContactQuad321 probe{{{left,top},{right,top},{right,bottom},{left,bottom}}};
    Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p))
    {
        const auto* d=GetNativePlantData(p);if(!d||p->mDead||!p->mIsOnBoard||p->mRow!=z->mRow||!NativeSourcePlantColliderEnabled(p))continue;
        bool flying=false,pumpkin=false,pot=false,lily=false;
        if(!GetNativeRemaining269PlantTags(p,flying,pumpkin,pot,lily)||!FusionDolphin313CanJumpPlant(flying,FusionDolphin313KelpKind(d->kind),p->mRow==z->mRow))continue;
        if(!FusionContactQuads321(probe,RectQuad(p->GetPlantRect())))continue;
        bool spike=false,nut=false;int level=0;if(!TryGetSpecialZombie269SourcePlantTraits(d->kind,spike,nut,level))return false;
        bool tall=nut&&level>=3;Plant* other=nullptr;
        while(z->mBoard->mPlants.IterateNext(other))if(other->mIsOnBoard&&!other->mDead&&other->mRow==p->mRow&&other->mPlantCol==p->mPlantCol)
        {const auto* od=GetNativePlantData(other);bool c=false,n=false;int l=0;if(od&&TryGetSpecialZombie269SourcePlantTraits(od->kind,c,n,l)&&n&&l>=3)tall=true;}
        float plantY=0;if(!p->GetShadowCenter(failX,plantY))return false;
        failX+=.5f*px;failY=y; // FixedUpdate354060: plantShadowX+.5, ownShadowY.
        blocked=tall;return true;
    }
    return false;
}
bool PlaceShadow(Zombie* z,float x,float y)
{
    float oldX=0,oldY=0,axisX=0,axisY=0;
    return std::isfinite(x)&&std::isfinite(y)&&GetNativeSourceZombie269NodePoint(z,"Shadow",oldX,oldY)&&
        NativeSourcePoint313(z,"axis",axisX,axisY)&&SetNativeSourceAxis313(z,axisX+(x-oldX)/float(kFusionBasePixelsX),axisY-(y-oldY)/float(GetNativeFusionPixelsY(z->mBoard)));
}
bool Facing(Zombie* z,bool)
{
    float x=0,y=0,sx=0,sy=0;
    // The SAME renderer normalizes source Shadow to the genuine native ground
    // anchor for either facing. Core changes its facing flag after this call.
    return GetNativeSourceZombie269NodePoint(z,"",x,y)&&GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy)&&std::isfinite(x+y+sx+sy);
}
bool Splash(Board* b,const NativeWaterZombie313Splash& e)
{
    Zombie* owner=nullptr;while(b->mZombies.IterateNext(owner))
    {const auto* d=GetNativeSourceZombieData(owner);if(owner->mNetId==e.ownerId&&d&&d->instanceToken==e.ownerToken&&!owner->mDead)
        return EmitNativeSourceAnimated317(owner,3,e.shadowX,e.shadowY-float(e.sourceYOffset*GetNativeFusionPixelsY(b)),Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,e.row,0));}
    return false;
}
void Particle(Zombie* z,uint16_t source,float offset)
{
    float x=0,y=0;if(GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))
        SpawnNativeSourceEffect269(z->mBoard,source,x,y-offset*float(GetNativeFusionPixelsY(z->mBoard)),Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,z->mRow,0),true);
}
void Sound(Zombie* z,uint16_t source,float volume){LawnNet::NetPlaySourceSound284(z->mApp,source,volume);}
void FallSound(Zombie* z){z->mApp->PlayFoley(FOLEY_ZOMBIE_FALLING);}
void Retire(Zombie* z,int reason)
{std::string error;if(!CanRetireNativeSourceZombie313(z,reason,&error)||!RetireNativeSourceZombie313(z,reason))Failed(z,error.c_str());}
void DriverTerrain(Zombie* z)
{
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Zombie_zamboni_4",x,y)){Failed(z,"Driver genuine child2 ice anchor unavailable");return;}
    z->mBoard->mIceMinX[z->mRow]=(std::min)(z->mBoard->mIceMinX[z->mRow],int(std::floor(x)));z->mBoard->mIceTimer[z->mRow]=3000;
}
void DriverContacts(Zombie* z)
{
    if(!Playing(z)||(z->mBoard->mMainCounter&1))return;
    FusionContactQuad321 box;if(!BodyQuad(z,box)){Failed(z,"Driver genuine enabled body Box unavailable");return;}
    auto* b=z->mBoard;auto* app=z->mApp;const uint32_t uid=b->mZombies.DataArrayGetID(z);
    std::vector<uint32_t> plants,enemies;Plant* p=nullptr;Zombie* q=nullptr;
    if(!z->mMindControlled)while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mRow==z->mRow&&GetNativePlantData(p)&&NativeSourcePlantColliderEnabled(p)&&FusionContactQuads321(box,RectQuad(p->GetPlantRect())))plants.push_back(b->mPlants.DataArrayGetID(p));
    while(b->mZombies.IterateNext(q))if(q!=z&&!q->mDead&&q->mRow==z->mRow&&q->mMindControlled!=z->mMindControlled&&GetNativeSourceZombieData(q)&&!NativeWater268ZombieCaptured(q))
    {const int status=GetNativeFusionZombieStatus(q);if(status!=10&&status!=12&&FusionContactQuads321(box,RectQuad(q->GetZombieRect())))enemies.push_back(b->mZombies.DataArrayGetID(q));}
    for(uint32_t target:plants)
    {
        p=b->mPlants.DataArrayTryToGet(target);const auto* d=GetNativePlantData(p);if(!p||p->mDead||!d)continue;
        bool cal=false,nut=false;int level=0;if(!TryGetSpecialZombie269SourcePlantTraits(d->kind,cal,nut,level)){Failed(z,"Driver source plant traits unavailable");return;}
        if(cal)continue;const uint16_t kind=d->kind;
        const bool tough=kind==903||kind==913||kind==921||kind==925||kind==1151,slippery=kind==931||kind==1048;
        if(slippery){auto* state=EditNativeWaterZombie313(z);if(!state)return;
            // Source CrashEntity invokes GoDie(2), not Garlic's row timer.
            if(!BeginNativeWaterZombie313Death(z,*state,2,GetNativeWaterZombie313Hooks()))return;
            LawnNet::NetPlaySourceSound284(app,77,.5f);}
        if(tough||slippery||nut&&level>=3)
        {ApplyNativeSourcePlantDamage(p,tough||slippery?500:1000,0);if(app->mBoard!=b)return;
            z=b->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)return;
            z->mPosX+=float(kFusionBasePixelsX);z->mX=int(z->mPosX);LawnNet::NetPlaySourceSound284(app,uint16_t(RandRangeInt(72,73)),.5f);}
        else {(void)RandRangeInt(8,9);p->Squish();}
        if(app->mBoard!=b)return;z=b->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)return;
    }
    for(uint32_t target:enemies)
    {q=b->mZombies.DataArrayTryToGet(target);if(q&&!q->mDead&&!ApplyNativeSourceTypedDamage313(q,4,20,false)){Failed(z,"Driver source contact damage refused");return;}
        if(app->mBoard!=b)return;z=b->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)return;}
}
bool Throw(Zombie* z){return ThrowNativeDrownWeapon313(z,GetNativeDrownWeapon313Hooks());}
int Random(Board*,int minimum,int exclusiveMaximum){return RandRangeInt(minimum,exclusiveMaximum-1);}
NativeWaterZombie313State* StateFor(Zombie* z){return EditNativeWaterZombie313(z);}
}
NativeWaterZombie313State* EditNativeWaterZombie313(Zombie* z,bool create)
{
    const auto* d=GetNativeSourceZombieData(z);auto* c=GetNativeContext(z?z->mBoard:nullptr);
    if(!z||!c||!d||!FusionWaterZombie313Kind(d->kind)||!IsNativeSourceWorld313(z->mBoard)||!d->instanceToken)return nullptr;
    const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);const bool ghost=z->mNetGhost||z->mApp->NetIsClient();
    if(!uid||(create&&ghost)||(!ghost&&d->instanceToken!=uid))return nullptr;
    auto i=c->waterZombies313.find(uid);if(i==c->waterZombies313.end()){if(!create)return nullptr;i=c->waterZombies313.emplace(uid,NativeWaterZombie313State{}).first;}return &i->second;
}
const NativeWaterZombie313State* GetNativeWaterZombie313State(const Zombie* z){return EditNativeWaterZombie313(const_cast<Zombie*>(z));}
const NativeWaterZombie313Hooks& GetNativeWaterZombie313Hooks()
{static const NativeWaterZombie313Hooks h{StateFor,Prepare,Contact,DrownContact,Bite,DolphinContact,PlaceShadow,Facing,Splash,Particle,Sound,FallSound,Retire,DriverContacts,DriverTerrain,Throw,Random};return h;}
bool EnsureNativeWaterZombie313Resources(Board* b,uint16_t kind,std::string* error)
{return Resources(b,kind,error);}
bool CanSpawnNativeWaterZombie313(Board* b,uint16_t kind,int row,bool controlled,std::string* error)
{return NativeWaterZombie313WorldValid(b,kind,row,error,controlled)&&Resources(b,kind,error)&&Pool(b,1,kind==20?1:2,error);}
bool CanAllocateNativeWaterZombie313Batch(Board* b,std::span<const uint16_t> kinds,std::string* error)
{size_t clocks=0;for(uint16_t k:kinds){if(!Resources(b,k,error))return false;clocks+=k==20?1:2;}return Pool(b,kinds.size(),clocks,error);}
bool InitializeNativeWaterZombie313Actor(Zombie* z)
{
    if(!z||!FusionWaterZombie313Kind(Kind(z))||!Resources(z->mBoard,Kind(z),nullptr))return false;
    if(z->mNetGhost||z->mApp->NetIsClient())return true;auto* state=EditNativeWaterZombie313(z,true);
    return state&&InitializeNativeWaterZombie313(z,*state,GetNativeWaterZombie313Hooks());
}
bool UpdateNativeWaterZombie313Actor(Zombie* z)
{
    if(!z||!FusionWaterZombie313Kind(Kind(z)))return false;if(!Playing(z))return true;
    auto* state=EditNativeWaterZombie313(z);if(!state){Failed(z,"Water313 initialized host state missing");return true;}
    const auto uid=z->mBoard->mZombies.DataArrayGetID(z);auto* b=z->mBoard;auto* app=z->mApp;
    const bool handled=UpdateNativeWaterZombie313(z,*state,GetNativeWaterZombie313Hooks());
    if(app->mBoard==b)if(auto* live=b->mZombies.DataArrayTryToGet(uid);live&&!live->mDead)
        if(const auto* current=GetNativeWaterZombie313State(live);current&&current->failed)Failed(live,"Water313 real source callback/preflight failed");return handled;
}
bool AdoptNativeWaterZombie313Actor(Zombie* z,const FusionWaterZombie313View& view)
{
    if(!z||(!z->mNetGhost&&!z->mApp->NetIsClient())||!FusionWaterZombie313WireViewValid(Kind(z),view)||
        !NativeWaterZombie313WorldValid(z->mBoard,Kind(z),z->mRow,nullptr,z->mMindControlled))return false;
    auto* c=GetNativeContext(z->mBoard);if(!c)return false;const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);if(!uid)return false;
    NativeWaterZombie313State next;auto old=c->waterZombies313.find(uid);if(old!=c->waterZombies313.end())next.auxiliaryID=old->second.auxiliaryID;
    static_cast<FusionWaterZombie313View&>(next)=view;next.localUID=uid;next.initialized=true;
    if(!AdoptNativeWaterZombie313Auxiliary(z,next,view.auxiliary))return false;c->waterZombies313[uid]=std::move(next);return true;
}
void RemoveNativeWaterZombie313Actor(Zombie* z)
{auto* c=GetNativeContext(z?z->mBoard:nullptr);if(!c||!z)return;auto it=c->waterZombies313.find(z->mBoard->mZombies.DataArrayGetID(z));if(it==c->waterZombies313.end())return;RemoveNativeWaterZombie313Auxiliary(z,it->second);c->waterZombies313.erase(it);}
bool BeginNativeWaterZombie313ActorDeath(Zombie* z,int reason)
{auto* s=EditNativeWaterZombie313(z);return s&&BeginNativeWaterZombie313Death(z,*s,reason,GetNativeWaterZombie313Hooks());}
bool NativeWaterZombie313ActorHeadLost(Zombie* z)
{auto* s=EditNativeWaterZombie313(z);return s&&NativeWaterZombie313LoseHead(z,*s,GetNativeWaterZombie313Hooks());}
bool NativeWaterZombie313ActorCaltrop(Zombie* z)
{
    auto* s=EditNativeWaterZombie313(z);if(!s)return false;
    auto* b=z->mBoard;auto* app=z->mApp;const uint32_t uid=b->mZombies.DataArrayGetID(z);
    const bool handled=NativeDriver313Caltrop(z,*s,GetNativeWaterZombie313Hooks());
    if(handled&&app->mBoard==b)if(auto* live=b->mZombies.DataArrayTryToGet(uid);live&&!live->mDead)
        if(const auto* current=GetNativeWaterZombie313State(live);current&&!current->colliderEnabled&&current->driverStopAge>=0)
            if(auto* data=GetNativeSourceZombieData(live))data->beforeDying=true;
    return handled;
}
bool NativeWaterZombie313ActorSetFacing(Zombie* z,bool right)
{auto* s=EditNativeWaterZombie313(z);return s&&NativeWaterZombie313SetFacing(z,*s,right,GetNativeWaterZombie313Hooks());}
bool ApplyNativeWaterZombie313BodyDamage(Zombie* z,int resolved)
{auto* s=EditNativeWaterZombie313(z);return s&&ApplyNativeDriver313BodyDamage(z,*s,resolved,GetNativeWaterZombie313Hooks());}
bool CanRetireNativeWaterZombie313(Zombie* z,int reason,std::string* error)
{return reason>=0&&reason<=2&&Host(z)&&GetNativeWaterZombie313State(z)&&Prepare(z,NativeWaterZombie313Operation::Death,error);}
bool BeforeRetireNativeWaterZombie313(Zombie* z,int reason)
{if(!CanRetireNativeWaterZombie313(z,reason))return false;auto* s=EditNativeWaterZombie313(z);return Kind(z)!=16||NativeDriver313DeathEvent(z,*s,GetNativeWaterZombie313Hooks());}
bool GetNativeWaterZombie313Semantics(const Zombie* z,int& status,bool& beforeDying,bool& collider,bool& water,bool& flip)
{
    const auto* s=GetNativeWaterZombie313State(z);const auto* d=GetNativeSourceZombieData(z);
    if(!s||!d||!s->initialized||s->failed||!FusionWaterZombie313ViewValid(d->kind,*s))return false;
    status=s->sourceStatus;beforeDying=d->beforeDying;collider=NativeWaterZombie313ColliderEnabled(z,*s);
    water=FusionWaterZombie313SourceInWater(d->kind);flip=s->facingRight;return true;
}
}
