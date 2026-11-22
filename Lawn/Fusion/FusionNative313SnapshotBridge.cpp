#include "FusionNative313SnapshotBridge.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeWaterZombie313Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include <algorithm>
#include <cstring>
#include <unordered_set>
namespace LawnFusion
{
namespace
{
bool Reject(const char* why,std::string* error)
{if(error)*error=why;return false;}
bool Current(const Board* b)
{const auto* c=GetNativeContext(b);return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)&&c&&
    !c->adventureTeardown&&c->epoch&&c->epoch==b->mApp->mNativeFusionEpoch;}
uint32_t UID(const Zombie* z)
{if(!z||!Current(z->mBoard))return 0;auto& a=z->mBoard->mZombies;const auto uid=a.DataArrayGetID(const_cast<Zombie*>(z));return uid&&a.DataArrayTryToGet(uid)==z?uid:0;}
Reanimation* LiveClock(Board* b,uint32_t id)
{
    auto* fx=b&&b->mApp?b->mApp->mEffectSystem:nullptr;
    if(!fx||fx!=gEffectSystem||!fx->mReanimationHolder||!id)return nullptr;
    auto* r=fx->mReanimationHolder->mReanimations.DataArrayTryToGet(id);
    return r&&!r->mDead&&r->mReanimationHolder==fx->mReanimationHolder?r:nullptr;
}
uint32_t Frames(const ReanimatorDefinition* d)
{if(!d||!d->mTracks.tracks||d->mTracks.count<=0)return 0;uint32_t n=0;for(int i=0;i<d->mTracks.count;++i)
    if(d->mTracks.tracks[i].mTransforms.count>0)n=(std::max)(n,uint32_t(d->mTracks.tracks[i].mTransforms.count));return n;}
bool CaptureClock(const Reanimation* r,LawnNet::NetZombieAnimState& out)
{
    if(!r||r->mDead||r->mFrameStart<0||r->mFrameStart>65535||r->mFrameCount<=0||r->mFrameCount>65535||
       int(r->mLoopType)<0||int(r->mLoopType)>5)return false;
    LawnNet::NetZombieAnimState v;v.mPresent=true;v.mFrameStart=uint16_t(r->mFrameStart);v.mFrameCount=uint16_t(r->mFrameCount);
    v.mAnimTime=r->mAnimTime;v.mAnimRate=r->mAnimRate;v.mLoopType=uint8_t(r->mLoopType);
    v.mLoopCount=uint8_t((std::clamp)(r->mLoopCount,0,255));
    if(!Native313AnimationViewValid(v)||!LawnNet::NetZombieAnimStateFitsFrames(v,Frames(r->mDefinition)))return false;out=v;return true;
}
void ApplyClock(Reanimation* r,const LawnNet::NetZombieAnimState& v)
{r->mFrameStart=v.mFrameStart;r->mFrameCount=v.mFrameCount;r->mAnimTime=r->mLastFrameTime=v.mAnimTime;
 r->mAnimRate=v.mAnimRate;r->mLoopType=ReanimLoopType(v.mLoopType);r->mLoopCount=v.mLoopCount;}
const ReanimatorTrack* Track(const ReanimatorDefinition* d,const char* name)
{if(!d||!d->mTracks.tracks||!name)return nullptr;for(int n=0;n<d->mTracks.count;++n)
    if(d->mTracks.tracks[n].mName&&!_stricmp(d->mTracks.tracks[n].mName,name))return &d->mTracks.tracks[n];return nullptr;}
// Reanimation::GetFramesForLayer's exact span rule, against loaded definition;
// no temporary/fake Zombie or Reanimation and no actor/native clock mutation.
bool Span(const ReanimatorDefinition* d,const char* name,const LawnNet::NetZombieAnimState* v=nullptr)
{
    const auto* t=Track(d,name);if(!t||!t->mTransforms.mTransforms||t->mTransforms.count<=0)return false;
    int start=0,count=1;for(int i=0;i<t->mTransforms.count;++i)if(t->mTransforms.mTransforms[i].mFrame>=0){start=i;break;}
    for(int i=start;i<t->mTransforms.count;++i)if(t->mTransforms.mTransforms[i].mFrame>=0)count=i-start+1;
    return !v||(v->mFrameStart==start&&v->mFrameCount==count);
}
const ReanimatorDefinition* Definition(ReanimationType type)
{
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return nullptr;
    const auto* d=&gReanimatorDefArray[unsigned(type)];if(!Frames(d))return nullptr;
    for(int i=0;i<d->mTracks.count;++i)if(d->mTracks.tracks[i].mName&&
        !_strnicmp(d->mTracks.tracks[i].mName,"attacher__",10))return nullptr;
    return d;
}
uint8_t AuxCount(uint16_t kind,const Native313Snapshot& v)
{return v.graph?uint8_t(v.graph->layerCount-1):uint8_t(kind==20?0:1);}
const LawnNet::NetZombieAnimState& Aux(const Native313Snapshot& v,uint8_t n)
{return v.graph?v.graph->auxiliary[n]:v.water->auxiliary;}
bool Definitions(uint16_t kind,const Native313Snapshot& v,const LawnNet::NetZombieAnimState& body,std::string* error)
{
    // Source16 is the real Zamboni clock; every other new family uses NORMAL.
    const auto* d=Definition(GetZombieDefinition(kind==16?ZOMBIE_ZAMBONI:ZOMBIE_NORMAL).mReanimationType);
    if(!d||!Native313AnimationViewValid(body)||!LawnNet::NetZombieAnimStateFitsFrames(body,Frames(d)))
        return Reject("313 ghost body native definition/frame span unavailable",error);
    if(v.graph)
    {
        const auto* g=FindFusionSourceGraph313(kind);
        for(uint8_t n=0;n<g->stateCount;++n)if(!Span(d,g->states[n].nativeTrack))return Reject("313 graph native track unavailable",error);
        if(!Span(d,g->states[v.graph->layers[0].state].nativeTrack,&body))return Reject("313 body clock disagrees with canonical graph state",error);
    }
    else
    {
        FusionWaterZombie313Clip clip;
        for(unsigned n=0;n<=unsigned(FusionWaterZombie313Phase::DrownDeath);++n)
            if(TryGetFusionWaterZombie313Clip(kind,FusionWaterZombie313Phase(n),clip)&&!Span(d,clip.nativeTrack))
                return Reject("313 water native track unavailable",error);
        if(!TryGetFusionWaterZombie313Clip(kind,v.water->phase,clip)||!Span(d,clip.nativeTrack,&body))
            return Reject("313 body clock disagrees with canonical water phase",error);
    }
    const auto count=AuxCount(kind,v);if(!count)return true;
    const auto* ad=Definition(REANIM_ZOMBIE);if(!ad)return Reject("313 auxiliary definition/attacher unavailable",error);
    if(v.graph&&!Span(d,"anim_head1"))return Reject("313 native auxiliary attachment track unavailable",error);
    for(uint8_t n=0;n<count;++n)
    {
        const char* name=v.graph?FindFusionSourceGraph313(kind)->states[v.graph->layers[n+1].state].nativeTrack:"anim_walk";
        if(!LawnNet::NetZombieAnimStateFitsFrames(Aux(v,n),Frames(ad))||!Span(ad,name,&Aux(v,n)))
            return Reject("313 auxiliary native frame span disagrees with canonical state",error);
    }
    return true;
}
bool ExistingAux(Board* b,Zombie* z,uint16_t kind,std::array<uint32_t,2>& ids,std::string* error)
{
    if(!z)return true;const auto uid=UID(z);const auto* d=GetNativeSourceZombieData(z);
    if(!uid||!d||d->kind!=kind||!d->instanceToken||z->mDead)return Reject("313 existing ghost identity is not current typed LOCAL generation",error);
    const auto* c=GetNativeContext(b);
    if(FusionWaterZombie313Kind(kind))
    {const auto it=c->waterZombies313.find(uid);if(it!=c->waterZombies313.end()){
        if(it->second.localUID!=uid)return Reject("313 water map belongs to stale LOCAL generation",error);ids[0]=it->second.auxiliaryID;}}
    else if(const auto* g=GetNativeSourceGraph313(z))
    {if(g->localUID!=uid)return Reject("313 graph map belongs to stale LOCAL generation",error);std::copy_n(g->auxiliaryUIDs,2,ids.begin());}
    for(const auto id:ids)if(id&&!LiveClock(b,id))return Reject("313 auxiliary generation is missing/dead",error);
    if(ids[0]&&ids[0]==ids[1])return Reject("313 auxiliary generation is aliased",error);return true;
}
Attachment* BodyAttachment(Reanimation* body,AttachmentHolder* holder)
{return body&&body->TrackExists("anim_head1")?holder->mAttachments.DataArrayTryToGet(unsigned(body->GetTrackInstanceByName("anim_head1")->mAttachmentID)):nullptr;}
bool Bound(const Attachment* a,uint32_t id)
{if(!a||a->mDead||a->mNumEffects<0||a->mNumEffects>MAX_EFFECTS_PER_ATTACHMENT)return false;
 for(int n=0;n<a->mNumEffects;++n)if(a->mEffectArray[n].mEffectType==EFFECT_REANIM&&a->mEffectArray[n].mEffectID==id)return true;return false;}
}
bool Native313ClockTrackMatches(uint16_t type,const char* track,const LawnNet::NetZombieAnimState& clock)
{
    if(type>=NUM_REANIMS||!track||!Native313AnimationViewValid(clock))return false;
    const auto* definition=Definition(ReanimationType(type));
    return definition&&LawnNet::NetZombieAnimStateFitsFrames(clock,Frames(definition))&&Span(definition,track,&clock);
}
bool CaptureNative313(const Zombie* z,Native313Snapshot& out,std::string* error)
{
    const auto uid=UID(z);const auto* d=GetNativeSourceZombieData(z);if(!uid||!d||z->mDead)return Reject("313 capture requires current typed live actor",error);
    Native313Snapshot v;
    if(FusionWaterZombie313Kind(d->kind))
    {const auto* state=GetNativeWaterZombie313State(z);if(!state||!state->initialized||state->failed||state->localUID!=uid)return Reject("313 water capture state unavailable",error);
     v.water=static_cast<const FusionWaterZombie313View&>(*state);
     if(!CaptureNativeWaterZombie313Auxiliary(z,*state,v.water->auxiliary))return Reject("313 water capture auxiliary unavailable",error);}
    else if(NativeHeavyZombie313BKind(d->kind))
    {const auto* s=GetNativeHeavyZombie313B(z);if(!s||!s->initialized||s->failed)return Reject("313 heavy capture state unavailable",error);
     v.heavy.emplace();v.heavy->actor=*s;if(!GetNativeHeavyGold313BView(z,v.heavy->gold))return Reject("313 Gold capture state unavailable",error);}
    else if(FindFusionExtendedZombie313Profile(d->kind))
    {const auto* s=GetNativeExtendedZombie313(z);if(!s||!s->initialized||s->failed)return Reject("313 extended capture state unavailable",error);v.extended=*s;}
    else return Reject("313 capture family unsupported",error);
    if(!v.water)
    {
        const auto* g=GetNativeSourceGraph313(z);const auto* def=FindFusionSourceGraph313(d->kind);
        if(!g||!def||!g->initialized||g->failed||g->localUID!=uid)return Reject("313 graph capture state unavailable",error);
        v.graph.emplace();v.graph->layerCount=def->layerCount;
        for(uint8_t n=0;n<def->layerCount;++n){v.graph->layers[n]=g->layers[n];if(n&&!CaptureClock(LiveClock(z->mBoard,g->auxiliaryUIDs[n-1]),v.graph->auxiliary[n-1]))return Reject("313 graph capture auxiliary unavailable",error);}
    }
    LawnNet::NetZombieAnimState body;
    if(!Native313SnapshotValid(d->kind,v)||!CaptureClock(LiveClock(z->mBoard,uint32_t(z->mBodyReanimID)),body)||!Definitions(d->kind,v,body,error))
        return Reject("313 capture canonical/actual native clocks disagree",error);
    out=std::move(v);if(error)error->clear();return true;
}
bool PreflightNative313GhostBatch(Board* b,std::span<const Native313GhostRequest> requests,FusionSourceDeathBudget319& out,std::string* error)
{
    if(!Current(b)||!b->mApp->NetIsClient())return Reject("313 ghost preflight requires current client Fusion board",error);
    auto* fx=b->mApp->mEffectSystem;if(!fx||fx!=gEffectSystem||!fx->mReanimationHolder||!fx->mAttachmentHolder)
        return Reject("313 ghost native effect holders unavailable",error);
    FusionSourceDeathBudget319 sum;std::unordered_set<const Zombie*> actors;std::unordered_set<uint32_t> clocks;
    for(const auto& r:requests)
    {
        if(!r.snapshot||!Native313SnapshotValid(r.kind,*r.snapshot))return Reject("313 ghost canonical family/graph invalid",error);
        if(r.existing&&(r.existing->mBoard!=b||!actors.insert(r.existing).second))return Reject("313 ghost batch duplicate/foreign actor",error);
        if(!EnsureNativeSourceZombieVisuals(b,r.kind,error)||!CanQueryNativeSourceZombieRoot273(b,r.kind)||!Definitions(r.kind,*r.snapshot,r.body,error))return false;
        std::array<uint32_t,2> ids{};if(!ExistingAux(b,r.existing,r.kind,ids,error))return false;
        const auto count=AuxCount(r.kind,*r.snapshot);unsigned need=0;
        for(uint8_t n=0;n<2;++n)
        {if(n>=count&&ids[n])return Reject("313 unused auxiliary handle must be zero",error);
         if(n<count){if(!ids[n])++need;else if(!clocks.insert(ids[n]).second)return Reject("313 auxiliary shared between actors",error);}}
        FusionSourceDeathBudget319 add;add.reanimations=need;
        if(r.snapshot->graph&&count)
        {
            auto* body=r.existing?LiveClock(b,uint32_t(r.existing->mBodyReanimID)):nullptr;
            if(r.existing&&!body)return Reject("313 existing ghost native body is absent",error);
            auto* a=BodyAttachment(body,fx->mAttachmentHolder);
            if(a&&!a->mDead)
            {if(a->mNumEffects<0||a->mNumEffects>MAX_EFFECTS_PER_ATTACHMENT||need>unsigned(MAX_EFFECTS_PER_ATTACHMENT-a->mNumEffects))return Reject("313 body attachment effect capacity unavailable",error);
             for(uint8_t n=0;n<count;++n)if(ids[n]&&!Bound(a,ids[n]))return Reject("313 auxiliary not bound to its current body generation",error);}
            else {for(auto id:ids)if(id)return Reject("313 existing auxiliary lost its body attachment",error);if(need)add.attachments=1;}
        }
        if(!AddFusionSourceDeathBudget319(sum,add,sum))return Reject("313 auxiliary budget overflow",error);
    }
    out=sum;if(error)error->clear();return true;
}
bool AdoptNative313(Zombie* z,const Native313Snapshot& v,std::string* error)
{
    const auto uid=UID(z);const auto* data=GetNativeSourceZombieData(z);
    if(!uid||!data||z->mDead||!z->mApp->NetIsClient()||!z->mNetGhost||!Native313SnapshotValid(data->kind,v))
        return Reject("313 adoption requires a validated current raw client ghost",error);
    auto* b=z->mBoard;auto* c=GetNativeContext(b);const auto kind=data->kind;
    auto* body=LiveClock(b,uint32_t(z->mBodyReanimID));LawnNet::NetZombieAnimState bodyView;
    if(!CaptureClock(body,bodyView)||!Definitions(kind,v,bodyView,error))return false;
    if(v.water)
    {
        // Reuse the family's real auxiliary allocation/adoption. HOST A keeps
        // native automatic advancement; ONLY this ghost path leases clocks.
        if(!AdoptNativeWaterZombie313Actor(z,*v.water))return Reject("313 prepared water adoption failed",error);
        const auto* state=GetNativeWaterZombie313State(z);
        if(!state||!RegisterNativeSourceGraphClock313(body,true)||
            (kind!=20&&!RegisterNativeSourceGraphClock313(LiveClock(b,state->auxiliaryID),true)))
            return Reject("313 prepared water ghost clock lease unavailable",error);
        if(error)error->clear();return true;
    }
    std::array<uint32_t,2> ids{};if(!ExistingAux(b,z,kind,ids,error))return false;
    const auto count=AuxCount(kind,v);std::array<uint32_t,2> born{};
    const auto rollback=[&](){for(auto id:born)if(id)z->mApp->RemoveReanimation(ReanimationID(id));};
    // Native R/A capacity was reserved by Root's ONE whole-frame Fits check.
    for(uint8_t n=0;n<count;++n)if(!ids[n])
    {
        auto* r=z->mApp->AddReanimation(0,0,0,REANIM_ZOMBIE);if(!r){rollback();return Reject("313 prepared auxiliary native allocation failed",error);}
        born[n]=ids[n]=uint32_t(z->mApp->ReanimationGetID(r));
        r->PlayReanim(v.graph?"anim_head1":"anim_walk",REANIM_LOOP,0,0);
        for(int t=0;t<r->mDefinition->mTracks.count;++t)r->mTrackInstances[t].mForceDontRender=true;
    }
    if(v.graph)
    {
        if(!body->TrackExists("anim_head1")){rollback();return Reject("313 prepared body attachment track disappeared",error);}
        for(uint8_t n=0;n<count;++n)if(born[n]&&!AttachReanim(body->GetTrackInstanceByName("anim_head1")->mAttachmentID,LiveClock(b,born[n]),0,0))
        {rollback();return Reject("313 prepared auxiliary attachment failed",error);}
    }
    for(uint8_t n=0;n<count;++n)ApplyClock(LiveClock(b,ids[n]),Aux(v,n));
    if(!RegisterNativeSourceGraphClock313(body,true)){rollback();return Reject("313 prepared body lease unavailable",error);}
    for(uint8_t n=0;n<count;++n)if(!RegisterNativeSourceGraphClock313(LiveClock(b,ids[n]),true))
    {rollback();return Reject("313 prepared auxiliary lease unavailable",error);}
    {
        NativeSourceGraph313State next;next.localUID=uid;next.initialized=true;
        std::copy(v.graph->layers.begin(),v.graph->layers.end(),next.layers);
        std::copy(ids.begin(),ids.end(),next.auxiliaryUIDs);c->sourceGraphs313[uid]=std::move(next);
        if(v.heavy)
        {
            if(!AdoptNativeHeavyZombie313B(z,v.heavy->actor,v.heavy->gold))return Reject("313 prepared heavy adoption failed",error);
        }
        else {NativeExtendedZombie313State state;static_cast<FusionExtendedZombie313View&>(state)=*v.extended;state.initialized=true;c->extendedZombies313[uid]=std::move(state);}
    }
    if(error)error->clear();return true;
}
}
