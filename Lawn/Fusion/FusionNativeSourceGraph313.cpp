#include "FusionNativeSourceGraph313.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace LawnFusion
{
namespace
{
bool Ready(const NativeSourceGraph313Hooks& h)
{return h.stateFor&&h.clock&&h.parameters&&h.preflightEvent&&h.event&&h.rootDelta&&h.moveRootDelta&&h.nullExitReady&&h.failed;}
const FusionSourceGraph313* Graph(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d?FindFusionSourceGraph313(d->kind):nullptr;}
bool Host(const Zombie* z)
{return z&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&!z->mNetGhost&&!z->mApp->NetIsClient()&&Graph(z);}
bool Match(Reanimation* r,const FusionSourceGraph313State& s)
{
    if(!r||r->mDead||!r->TrackExists(s.nativeTrack))return false;int start=0,count=0;r->GetFramesForLayer(s.nativeTrack,start,count);
    return count>0&&r->mFrameStart==start&&r->mFrameCount==count;
}
double Absolute(const FusionSourceGraph313State& s,FusionSourceGraph313Cursor p){return double(p.loop)*s.duration+p.seconds;}
bool Write(Zombie* z,uint8_t layer,uint8_t phase,FusionSourceGraph313Cursor p,const NativeSourceGraph313Hooks& h)
{
    const auto* g=Graph(z);if(!g||phase>=g->stateCount)return false;const auto& s=g->states[phase];auto* r=h.clock(z,layer);
    if(!Match(r,s)||!FusionSourceGraph313CursorValid(s,p)||p.loop>uint32_t(INT_MAX))return false;
    float normalized=s.nullMotion&&s.duration==0?0:p.seconds/s.duration;
    if(s.duration>0&&normalized*s.duration<p.seconds)normalized=std::nextafter(normalized,1.f);
    r->mAnimTime=r->mLastFrameTime=normalized;r->mLoopCount=int(p.loop);return true;
}
struct Run {LawnApp* app;Board* board;uint32_t uid;uint16_t kind;Zombie* z;NativeSourceGraph313State* s;const NativeSourceGraph313Hooks& h;};
Run Capture(Zombie* z,NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h)
{return{z->mApp,z->mBoard,z->mBoard->mZombies.DataArrayGetID(z),GetNativeSourceZombieData(z)->kind,z,&s,h};}
bool Refresh(Run& r)
{
    if(r.app->mBoard!=r.board)return false;r.z=r.board->mZombies.DataArrayTryToGet(r.uid);const auto* d=GetNativeSourceZombieData(r.z);
    if(!r.z||r.z->mDead||!d||d->kind!=r.kind||d->instanceToken!=r.uid)return false;
    r.s=r.h.stateFor(r.z);return r.s&&r.s->initialized&&r.s->localUID==r.uid&&!r.s->failed;
}
bool Fail(Run& r,const char* why)
{if(Refresh(r))r.s->failed=true;if(r.app->mBoard==r.board)r.h.failed(r.board,why);return false;}
void ClearFade(NativeSourceGraph313State& s,uint8_t layer)
{auto& v=s.layers[layer];v.outgoing=v.state;v.outgoingSeconds=v.fadeElapsed=v.fadeDuration=0;v.fadeNormalized=false;s.outgoingCursors[layer]={};}
bool Exit(Run& r,uint8_t phase)
{
    const auto* g=FindFusionSourceGraph313(r.kind);const int16_t action=g->states[phase].exitEvent;if(action<0)return true;
    const auto event=FusionSourceGraph313Event(action);std::string error;
    if(!r.h.preflightEvent(r.z,event,&error))return Fail(r,"Source313 state-exit dependency unavailable before callback.");
    // The old fade/phase has already been consumed, so a recursive callback
    // cannot emit this OnStateExit twice.
    return r.h.event(r.z,event,0)||Fail(r,"Source313 state-exit callback failed after consumption.");
}
bool Markers(Run& r,uint8_t layer,uint8_t phase,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,bool entry)
{
    const auto* g=Graph(r.z);if(!g||phase>=g->stateCount)return false;const auto s=g->states[phase];
    // Current cursors are already committed BEFORE any reentrant gameplay.
    for(uint32_t loop=a.loop;loop<=b.loop;++loop)
    {
        for(uint8_t i=0;i<s.markerCount;++i)
        {
            const auto marker=s.markers[i];const double at=double(loop)*s.duration+marker.seconds;
            if((at>Absolute(s,a)||(entry&&at==Absolute(s,a)))&&at<=Absolute(s,b))
            {
                const uint32_t revision=r.s->revisions[layer];std::string error;
                if(!r.h.preflightEvent(r.z,marker.event,&error))return Fail(r,"Source313 event dependency unavailable before gameplay mutation.");
                if(!r.h.event(r.z,marker.event,marker.sourceInt))return Fail(r,"Source313 event failed after source progress consumed; no replay.");
                if(!Refresh(r))return true;
                if(r.s->revisions[layer]!=revision)return true;
            }
        }
        if(loop==UINT32_MAX)break;
    }
    return true;
}
bool Enter(Run& r,uint8_t layer,uint8_t phase,float offset,float fade,bool normalized=false)
{
    const auto* g=Graph(r.z);if(!g||!FusionSourceGraph313OwnsState(*g,layer,phase)||!std::isfinite(offset)||offset<0||!std::isfinite(fade)||fade<0)return false;
    const auto& s=g->states[phase];auto* clock=r.h.clock(r.z,layer);if(!clock||!clock->TrackExists(s.nativeTrack))return false;
    FusionSourceGraph313Cursor p;
    if(s.nullMotion){if(offset!=0)return false;}
    else if(s.loop){if(std::floor(offset)>INT_MAX)return false;p={uint32_t(std::floor(offset)),(offset-std::floor(offset))*s.duration};}
    else p.seconds=(std::min)(offset,1.f)*s.duration;
    // Include entry callbacks in the preflight BEFORE resetting the real clock.
    for(uint8_t i=0;i<s.markerCount;++i)if(s.markers[i].seconds==p.seconds&&p.loop==0)
    {std::string error;if(!r.h.preflightEvent(r.z,s.markers[i].event,&error))return false;}
    if(s.entryEvent>=0){std::string error;if(!r.h.preflightEvent(r.z,FusionSourceGraph313Event(s.entryEvent),&error))return false;}
    auto& v=r.s->layers[layer];const uint8_t previous=v.state;
    if(v.fadeDuration>0)
    {
        const uint8_t abandoned=v.outgoing;ClearFade(*r.s,layer);
        if(!Exit(r,abandoned))return false;if(!Refresh(r))return true;
    }
    if(fade>0){v.outgoing=v.state;v.outgoingSeconds=r.s->cursors[layer].seconds;v.fadeElapsed=0;v.fadeDuration=fade;v.fadeNormalized=normalized;r.s->outgoingCursors[layer]=r.s->cursors[layer];}
    v.state=phase;r.s->cursors[layer]=p;++r.s->revisions[layer];if(fade==0)ClearFade(*r.s,layer);
    // NORMAL source actors use only actual native clock tracks; source phase
    // index disambiguates tracks shared by multiple genuine source states.
    if(layer==0)r.z->PlayZombieReanim(s.nativeTrack,s.loop?REANIM_LOOP:REANIM_PLAY_ONCE_AND_HOLD,0,0);
    else clock->PlayReanim(s.nativeTrack,s.loop?REANIM_LOOP:REANIM_PLAY_ONCE_AND_HOLD,0,0);
    if(!Write(r.z,layer,phase,p,r.h))return false;
    if(fade==0&&previous!=phase){if(!Exit(r,previous))return false;if(!Refresh(r))return true;}
    if(s.entryEvent>=0)
    {
        const uint32_t enteredRevision=r.s->revisions[layer];
        if(!r.h.event(r.z,FusionSourceGraph313Event(s.entryEvent),0))return Fail(r,"Source361 state-entry callback failed after consumption.");
        if(!Refresh(r)||r.s->revisions[layer]!=enteredRevision)return true;
    }
    if(!Markers(r,layer,phase,p,p,offset==0))return false;
    return true;
}
bool Input(Run& r,uint8_t layer,FusionSourceGraph313Parameters& p)
{
    if(r.h.parametersForLayer361)
    {if(!r.h.parametersForLayer361(r.z,layer,p)||!std::isfinite(p.speed)||p.speed<0)return false;p.triggers|=r.s->layerParameters361[layer].triggers;r.s->layerParameters361[layer]=p;return true;}
    if(!r.h.parameters(r.z,p)||!std::isfinite(p.speed)||p.speed<0)return false;p.triggers|=r.s->parameters.triggers;r.s->parameters=p;return true;
}
void Consume(Run& r,uint8_t layer,uint64_t bits)
{if(r.h.parametersForLayer361)r.s->layerParameters361[layer].triggers&=~bits;else r.s->parameters.triggers&=~bits;}
bool Step(Run& r,uint8_t layer)
{
    const auto* g=FindFusionSourceGraph313(r.kind);auto& initial=r.s->layers[layer];const auto& first=g->states[initial.state];
    FusionSourceGraph313Cursor observed;if(!ReadNativeSourceGraph313Clock(r.z,layer,initial.state,r.h,observed))return false;
    const double span=Absolute(first,observed)-Absolute(first,r.s->cursors[layer]);
    // Write() rounds normalized native floats upward at exact event boundaries.
    // No independent holder step is permitted between the consumed mirror and
    // this actual native R read.
    if(!std::isfinite(span)||span<0||span>double(first.duration)*1e-6)return false;
    r.s->cursors[layer]=observed;float remaining=.01f;
    for(int splits=0;remaining>0;++splits)
    {
        if(splits>=64)return false;FusionSourceGraph313Parameters params;if(!Input(r,layer,params))return false;
        auto& v=r.s->layers[layer];const uint8_t phase=v.state;const auto& s=g->states[phase];const uint32_t revision=r.s->revisions[layer];
        const auto a=r.s->cursors[layer];const auto oa=r.s->outgoingCursors[layer];
        const FusionSourceGraph313Edge* edge=nullptr;uint64_t consumed=0;
        if(!SelectFusionSourceGraph313Edge(*g,layer,v,a,a,params,true,r.h.nullExitReady(r.z,layer,phase),edge,consumed))return false;
        if(edge)
        {
            Consume(r,layer,consumed);const float fade=edge->fixedDuration?edge->duration:edge->duration*s.duration;
            if(!Enter(r,layer,edge->target,edge->offset,fade,!edge->fixedDuration))return false;if(!Refresh(r))return true;continue;
        }
        float wait;if(!NextFusionSourceGraph313Boundary(*g,layer,v,a,params,wait))return false;
        const float incomingRate=FusionSourceGraph313Rate(s,FusionSourceGraph313Multiplier(s,params));
        float dt=(std::min)(remaining,wait);bool blending=v.fadeDuration>0;FusionSourceGraph313Cursor ob=oa;
        if(blending)
        {
            const auto& old=g->states[v.outgoing];FusionSourceGraph313Cursor marker;FusionSourceGraph313Event ev;
            const float rate=FusionSourceGraph313Rate(old,FusionSourceGraph313Multiplier(old,params));
            if(rate>0&&NextFusionSourceGraph313Marker(old,oa,marker,ev))dt=(std::min)(dt,float((Absolute(old,marker)-Absolute(old,oa))/rate));
            const float fadeRate=v.fadeNormalized?rate:1.f;
            if(fadeRate>0)dt=(std::min)(dt,(v.fadeDuration-v.fadeElapsed)/fadeRate);
            if(!AdvanceFusionSourceGraph313(old,oa,FusionSourceGraph313Multiplier(old,params),dt,ob))return false;
            if(rate>0&&NextFusionSourceGraph313Marker(old,oa,marker,ev)&&float((Absolute(old,marker)-Absolute(old,oa))/rate)==dt)ob=marker;
        }
        if(!std::isfinite(dt)||dt<0)return false;FusionSourceGraph313Cursor end;
        const float supplied=s.speedParameter?(s.speed>0?incomingRate/s.speed:0):1;
        if(!AdvanceFusionSourceGraph313(s,a,supplied,dt,end))return false;
        // Snap to the exact source marker/exit float when a segment ends there;
        // writing a normalized native float must not lose an event to rounding.
        FusionSourceGraph313Cursor nextMarker;FusionSourceGraph313Event markerEvent;
        if(incomingRate>0&&NextFusionSourceGraph313Marker(s,a,nextMarker,markerEvent)&&float((Absolute(s,nextMarker)-Absolute(s,a))/incomingRate)==dt)end=nextMarker;
        FusionSourceGraph313Cursor exit;if(incomingRate>0&&NextFusionSourceGraph313Exit(*g,layer,v,a,params,exit)&&float((Absolute(s,exit)-Absolute(s,a))/incomingRate)==dt)end=exit;
        double delta=0,oldDelta=0;
        if(layer==0&&!r.h.rootDelta(r.z,phase,a,end,delta))return false;
        if(blending&&layer==0)
        {if(!r.h.rootDelta(r.z,v.outgoing,oa,ob,oldDelta))return false;const double progress=v.fadeNormalized?Absolute(g->states[v.outgoing],ob)-Absolute(g->states[v.outgoing],oa):double(dt);const double w=(v.fadeElapsed+progress*.5)/v.fadeDuration;delta=oldDelta*(1-w)+delta*w;}
        const uint8_t outgoing=v.outgoing;
        if(!Write(r.z,layer,phase,end,r.h))return false;r.s->cursors[layer]=end;
        if(blending){FusionSourceGraph313View next;if(!AdvanceFusionSourceGraph313Fade(*g,v,oa,ob,dt,next))return false;r.s->outgoingCursors[layer]=ob;v=next;}
        if(delta&&!r.h.moveRootDelta(r.z,delta))return false;if(!Refresh(r))return true;
        if(blending&&!Markers(r,layer,outgoing,oa,ob,false))return false;if(!Refresh(r))return true;
        if(r.s->revisions[layer]==revision&&!Markers(r,layer,phase,a,end,false))return false;if(!Refresh(r))return true;
        remaining=(std::max)(0.f,remaining-dt);
        if(r.s->revisions[layer]!=revision){continue;}
        auto& now=r.s->layers[layer];if(now.fadeDuration>0&&now.fadeElapsed>=now.fadeDuration)
        {const uint8_t finished=now.outgoing;ClearFade(*r.s,layer);if(!Exit(r,finished))return false;if(!Refresh(r))return true;}
        if(!Input(r,layer,params)||!SelectFusionSourceGraph313Edge(*g,layer,now,a,end,params,true,r.h.nullExitReady(r.z,layer,phase),edge,consumed))return false;
        if(edge){Consume(r,layer,consumed);const float fade=edge->fixedDuration?edge->duration:edge->duration*s.duration;if(!Enter(r,layer,edge->target,edge->offset,fade,!edge->fixedDuration))return false;if(!Refresh(r))return true;}
        if(dt==0&&!edge)return false;
    }
    return true;
}
}
bool ReadNativeSourceGraph313Clock(Zombie* z,uint8_t layer,uint8_t phase,const NativeSourceGraph313Hooks& h,FusionSourceGraph313Cursor& out)
{
    const auto* g=Graph(z);if(!g||!h.clock||!FusionSourceGraph313OwnsState(*g,layer,phase))return false;const auto& s=g->states[phase];auto* r=h.clock(z,layer);
    if(!Match(r,s)||r->mLoopCount<0||!std::isfinite(r->mAnimTime)||r->mAnimTime<0||r->mAnimTime>1)return false;
    FusionSourceGraph313Cursor p=s.nullMotion&&s.duration==0?FusionSourceGraph313Cursor{}:FusionSourceGraph313Cursor{uint32_t(r->mLoopCount),r->mAnimTime*s.duration};
    if(!s.loop&&p.loop==1&&p.seconds==s.duration)p.loop=0;
    if(!FusionSourceGraph313CursorValid(s,p))return false;out=p;return true;
}
bool InitializeNativeSourceGraph313(Zombie* z,NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h)
{return InitializeNativeSourceGraph313Masked(z,s,h,0x7);}
bool InitializeNativeSourceGraph313Masked(Zombie* z,NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h,uint8_t layerMask)
{
    if(!Host(z)||!Ready(h))return false;const auto* g=Graph(z);auto r=Capture(z,s,h);
    layerMask&=uint8_t((1u<<g->layerCount)-1);
    if(s.initialized)return s.localUID==r.uid&&!s.failed&&s.initializedLayerMask343==layerMask;
    if(!(layerMask&uint8_t((1u<<g->layerCount)-1))||!NativeSourceGraph313ClockTracksReadyMasked(z,h,layerMask))return false;
    for(uint8_t i=0;i<g->layerCount;++i)if((layerMask&(1u<<i))&&!h.clock(z,i))return false;
    const uint32_t firstAux=s.auxiliaryUIDs[0],secondAux=s.auxiliaryUIDs[1];
    s={};s.auxiliaryUIDs[0]=firstAux;s.auxiliaryUIDs[1]=secondAux;s.localUID=r.uid;s.initialized=true;s.initializedLayerMask343=layerMask;
    for(uint8_t i=0;i<g->layerCount;++i)
    {if(!(layerMask&(1u<<i)))continue;s.layers[i].state=s.layers[i].outgoing=g->layers[i].defaultState;if(!Enter(r,i,g->layers[i].defaultState,0,0))return Fail(r,"Source313 default source clock failed.");if(!Refresh(r))return true;}
    if(!UpdateNativeSourceGraph313RatesMasked(z,s,h,layerMask))return false;
    for(uint8_t i=0;i<g->layerCount;++i)if((layerMask&(1u<<i))&&!RegisterNativeSourceGraphClock313(h.clock(z,i),true))
    {for(uint8_t j=0;j<i;++j)if(layerMask&(1u<<j))RegisterNativeSourceGraphClock313(h.clock(z,j),false);return false;}
    return true;
}
void ReleaseNativeSourceGraph313Clocks(Zombie* z,const NativeSourceGraph313Hooks& h)
{ReleaseNativeSourceGraph313ClocksMasked(z,h,0x7);}
void ReleaseNativeSourceGraph313ClocksMasked(Zombie* z,const NativeSourceGraph313Hooks& h,uint8_t layerMask)
{const auto* g=Graph(z);if(!g||!h.clock)return;for(uint8_t i=0;i<g->layerCount;++i)if(layerMask&(1u<<i))if(auto* r=h.clock(z,i))RegisterNativeSourceGraphClock313(r,false);}
bool NativeSourceGraph313ClockTracksReady(Zombie* z,const NativeSourceGraph313Hooks& h,std::string* error)
{return NativeSourceGraph313ClockTracksReadyMasked(z,h,0x7,error);}
bool NativeSourceGraph313ClockTracksReadyMasked(Zombie* z,const NativeSourceGraph313Hooks& h,uint8_t layerMask,std::string* error)
{
    const auto* g=Graph(z);if(!g||!h.clock)return false;
    for(uint8_t layer=0;layer<g->layerCount;++layer)
    {
        if(!(layerMask&(1u<<layer)))continue;
        auto* clock=h.clock(z,layer);if(!clock||clock->mDead){if(error)*error="Missing actual body/aux Reanimation.";return false;}
        for(uint8_t state=0;state<g->stateCount;++state)if(FusionSourceGraph313OwnsState(*g,layer,state))
        {
            const char* track=g->states[state].nativeTrack;if(!clock->TrackExists(track)){if(error)*error="Actual clock carrier lacks required source-phase track.";return false;}
            int first=0,count=0;clock->GetFramesForLayer(track,first,count);if(first<0||count<=0){if(error)*error="Actual native clock track has no frame span.";return false;}
        }
    }
    if(error)error->clear();return true;
}
bool PlayNativeSourceGraph313(Zombie* z,NativeSourceGraph313State& s,uint8_t layer,const char* name,float offset,const NativeSourceGraph313Hooks& h)
{
    if(!Host(z)||!Ready(h)||!s.initialized||s.failed||!name||layer>=3||!(s.initializedLayerMask343&(1u<<layer)))return false;const auto* g=Graph(z);auto r=Capture(z,s,h);
    // Source218 WD0 walk has a proved fresh-rest entry, not replay semantics.
    // All later locomotion returns through the genuine WD1 walk2 graph state.
    if(g->kind==218&&layer==0&&std::strcmp(name,"walk")==0&&
       (s.tick!=-1||s.layers[0].state!=g->layers[0].defaultState||offset!=0))return false;
    for(uint8_t i=0;i<g->stateCount;++i)if(FusionSourceGraph313OwnsState(*g,layer,i)&&std::strcmp(g->states[i].name,name)==0)
        return Enter(r,layer,i,offset,0)&&(!Refresh(r)||UpdateNativeSourceGraph313RatesMasked(r.z,*r.s,h,r.s->initializedLayerMask343));
    return false;
}
bool UpdateNativeSourceGraph313Rates(Zombie* z,const NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h)
{return UpdateNativeSourceGraph313RatesMasked(z,s,h,0x7);}
bool UpdateNativeSourceGraph313RatesMasked(Zombie* z,const NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h,uint8_t layerMask)
{
    if(!Host(z)||!Ready(h))return false;const auto* g=Graph(z);FusionSourceGraph313Parameters shared;if(!h.parametersForLayer361&&(!h.parameters(z,shared)||!std::isfinite(shared.speed)||shared.speed<0))return false;
    layerMask&=uint8_t((1u<<g->layerCount)-1);if(!layerMask||(layerMask&~s.initializedLayerMask343))return false;
    for(uint8_t i=0;i<g->layerCount;++i)
    {if(!(layerMask&(1u<<i)))continue;FusionSourceGraph313Parameters p=shared;if(h.parametersForLayer361&&(!h.parametersForLayer361(z,i,p)||!std::isfinite(p.speed)||p.speed<0))return false;const auto& state=g->states[s.layers[i].state];auto* r=h.clock(z,i);if(!Match(r,state))return false;if(h.layerActive366&&!h.layerActive366(z,i)){r->mAnimRate=0;continue;}r->mAnimRate=state.nullMotion&&state.duration==0?0:r->mFrameCount*FusionSourceGraph313Rate(state,FusionSourceGraph313Multiplier(state,p))/state.duration;if(i==0)z->mOriginalAnimRate=r->mAnimRate;}
    return true;
}
bool UpdateNativeSourceGraph313(Zombie* z,NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h)
{return UpdateNativeSourceGraph313Masked(z,s,h,0x7);}
bool UpdateNativeSourceGraph313Masked(Zombie* z,NativeSourceGraph313State& s,const NativeSourceGraph313Hooks& h,uint8_t layerMask)
{
    if(!Host(z)||!Ready(h)||!s.initialized||s.failed)return false;if(z->mDead||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
    const auto* g=Graph(z);layerMask&=uint8_t((1u<<g->layerCount)-1);if(!layerMask||(layerMask&~s.initializedLayerMask343))return false;
    if(s.tick==z->mBoard->mMainCounter)return true;s.tick=z->mBoard->mMainCounter;auto r=Capture(z,s,h);
    for(uint8_t i=0;i<g->layerCount;++i){if(!(layerMask&(1u<<i))||(h.layerActive366&&!h.layerActive366(r.z,i)))continue;if(!Step(r,i))return Fail(r,"Source313 segmented native phase/event update failed; no replay.");if(!Refresh(r))return true;}
    return UpdateNativeSourceGraph313RatesMasked(r.z,*r.s,h,layerMask)||Fail(r,"Source313 real body/aux rate unavailable.");
}
bool GetNativeSourceGraph313Presentation(Zombie* z,const NativeSourceGraph313State& s,uint8_t layer,const NativeSourceGraph313Hooks& h,
    const char*& incoming,float& time,const char*& outgoing,float& oldTime,float& weight)
{
    const auto* g=Graph(z);if(!g||!FusionSourceGraph313ViewValid(*g,layer,s.layers[layer]))return false;FusionSourceGraph313Cursor p;
    if(!ReadNativeSourceGraph313Clock(z,layer,s.layers[layer].state,h,p))return false;const auto& v=s.layers[layer];const auto& state=g->states[v.state];
    const char* old=nullptr;float before=0,w=1;
    if(v.fadeDuration>0){const auto& os=g->states[v.outgoing];old=os.nullMotion?nullptr:os.name;before=!os.nullMotion&&os.speed>0?v.outgoingSeconds/os.speed:0;w=v.fadeElapsed/v.fadeDuration;}
    incoming=state.nullMotion?nullptr:state.name;time=!state.nullMotion&&state.speed>0?p.seconds/state.speed:0;outgoing=old;oldTime=before;weight=w;return true;
}
}
