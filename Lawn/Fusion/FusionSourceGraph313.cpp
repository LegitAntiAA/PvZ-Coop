#include "FusionSourceGraph313.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
double Absolute(const FusionSourceGraph313State& s,FusionSourceGraph313Cursor p){return double(p.loop)*s.duration+p.seconds;}
bool Parameters(const FusionSourceGraph313Parameters& p){return std::isfinite(p.speed)&&p.speed>=0&&std::isfinite(p.jumpSpeed)&&p.jumpSpeed>=0&&(p.triggers&~((1U<<24)-1))==0;}
bool Conditions(const FusionSourceGraph313Edge& e,const FusionSourceGraph313Parameters& p,uint32_t& consumed)
{
    uint32_t triggers=0;
    for(uint8_t i=0;i<e.conditionCount;++i)
    {
        const auto& c=e.conditions[i];bool yes=false;
        if(c.parameter==FusionSourceGraph313Param::Speed||c.parameter==FusionSourceGraph313Param::JumpSpeed)
        {const float value=c.parameter==FusionSourceGraph313Param::Speed?p.speed:p.jumpSpeed;if(c.mode==3)yes=value>c.threshold;else if(c.mode==4)yes=value<c.threshold;else return false;}
        else if(c.parameter==FusionSourceGraph313Param::Attacking||c.parameter==FusionSourceGraph313Param::Shooting||c.parameter==FusionSourceGraph313Param::Wait)
        {const bool value=c.parameter==FusionSourceGraph313Param::Attacking?p.attacking:c.parameter==FusionSourceGraph313Param::Shooting?p.shooting:p.wait;if(c.mode==1)yes=value;else if(c.mode==2)yes=!value;else return false;}
        else
        {if(c.mode!=1)return false;const uint32_t bit=FusionSourceGraph313Trigger(c.parameter);yes=(p.triggers&bit)!=0;triggers|=bit;}
        if(!yes)return false;
    }
    consumed=triggers;return true;
}
bool ExitCrossed(const FusionSourceGraph313State& s,const FusionSourceGraph313Edge& e,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,bool nullReady)
{
    if(!e.hasExitTime)return true;if(s.nullMotion)return s.duration==1.f?b.seconds>=e.exitTime*s.duration:nullReady;
    const double x=Absolute(s,a),y=Absolute(s,b);const float fraction=e.exitTime-std::floor(e.exitTime);const float seconds=fraction*s.duration;
    if(e.exitTime>=1||!s.loop){const double threshold=double(std::floor(e.exitTime))*s.duration+seconds;return x<threshold&&y>=threshold;}
    const double next=(std::floor((x-seconds)/s.duration)+1)*s.duration+seconds;return next<=y;
}
}
bool FusionSourceGraph313OwnsState(const FusionSourceGraph313& g,uint8_t layer,uint8_t state)
{if(layer>=g.layerCount||state>=g.stateCount)return false;const auto& l=g.layers[layer];return std::find(l.states,l.states+l.stateCount,state)!=l.states+l.stateCount;}
bool FusionSourceGraph313ViewValid(const FusionSourceGraph313& g,uint8_t layer,const FusionSourceGraph313View& v)
{
    if(!FusionSourceGraph313OwnsState(g,layer,v.state)||!FusionSourceGraph313OwnsState(g,layer,v.outgoing)||
        !std::isfinite(v.fadeDuration)||v.fadeDuration<0||v.fadeDuration>60||!std::isfinite(v.fadeElapsed)||v.fadeElapsed<0||v.fadeElapsed>v.fadeDuration||
        !std::isfinite(v.outgoingSeconds)||v.outgoingSeconds<0)return false;
    if(v.fadeDuration==0)return v.outgoing==v.state&&v.fadeElapsed==0&&v.outgoingSeconds==0&&!v.fadeNormalized;
    const auto& s=g.states[v.outgoing];return s.nullMotion&&s.duration==0?v.outgoingSeconds==0:v.outgoingSeconds<=s.duration;
}
bool FusionSourceGraph313CursorValid(const FusionSourceGraph313State& s,FusionSourceGraph313Cursor p)
{return std::isfinite(p.seconds)&&p.seconds>=0&&(s.nullMotion&&s.duration==0?(p.loop==0&&p.seconds==0):s.duration>0&&p.seconds<=s.duration&&(s.loop||p.loop==0));}
float FusionSourceGraph313Rate(const FusionSourceGraph313State& s,float p)
{return s.speed*(s.speedParameter?p:1.f);}
float FusionSourceGraph313Multiplier(const FusionSourceGraph313State& s,const FusionSourceGraph313Parameters& p)
{return s.speedParameter==2?p.jumpSpeed:s.speedParameter==1?p.speed:1.f;}
bool AdvanceFusionSourceGraph313(const FusionSourceGraph313State& s,FusionSourceGraph313Cursor p,float speed,float dt,FusionSourceGraph313Cursor& out)
{
    if(!FusionSourceGraph313CursorValid(s,p)||!std::isfinite(speed)||speed<0||!std::isfinite(dt)||dt<0||dt>1)return false;
    if(s.nullMotion&&s.duration==0){out=p;return true;}double next=Absolute(s,p)+double(FusionSourceGraph313Rate(s,speed))*dt;
    FusionSourceGraph313Cursor q;
    if(!s.loop){q.seconds=float((std::min)(next,double(s.duration)));}
    else{const double loops=std::floor(next/s.duration);if(loops>double(UINT32_MAX))return false;q.loop=uint32_t(loops);q.seconds=float(next-loops*s.duration);}
    if(!FusionSourceGraph313CursorValid(s,q))return false;out=q;return true;
}
bool SelectFusionSourceGraph313Edge(const FusionSourceGraph313& g,uint8_t layer,const FusionSourceGraph313View& v,
    FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,const FusionSourceGraph313Parameters& p,
    bool deathOverride,bool nullReady,const FusionSourceGraph313Edge*& output,uint32_t& consumed)
{
    if(!FusionSourceGraph313ViewValid(g,layer,v)||!Parameters(p))return false;const auto& s=g.states[v.state];
    if(!FusionSourceGraph313CursorValid(s,a)||!FusionSourceGraph313CursorValid(s,b)||Absolute(s,b)<Absolute(s,a))return false;
    const FusionSourceGraph313Edge* selected=nullptr;uint32_t bits=0;
    const auto scan=[&](const FusionSourceGraph313Edge* edges,uint8_t n,bool any)
    {
        for(uint8_t i=0;i<n;++i)
        {
            const auto& e=edges[i];uint32_t take=0;
            if(!e.self&&e.target==v.state)continue;
            if(!Conditions(e,p,take))continue;
            if(v.fadeDuration>0&&!(any&&deathOverride&&(take&FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die))))continue;
            if(!ExitCrossed(s,e,a,b,nullReady))continue;
            selected=&e;bits=take;return true;
        }
        return false;
    };
    const auto& l=g.layers[layer];if(!scan(l.any,l.anyCount,true))scan(s.edges,s.edgeCount,false);
    output=selected;consumed=bits;return true;
}
bool NextFusionSourceGraph313Marker(const FusionSourceGraph313State& s,FusionSourceGraph313Cursor p,FusionSourceGraph313Cursor& out,FusionSourceGraph313Event& event)
{
    if(!FusionSourceGraph313CursorValid(s,p)||s.nullMotion||!s.markerCount)return false;
    for(uint8_t i=0;i<s.markerCount;++i)if(s.markers[i].seconds>p.seconds){out={p.loop,s.markers[i].seconds};event=s.markers[i].event;return true;}
    if(!s.loop||p.loop==UINT32_MAX)return false;out={p.loop+1,s.markers[0].seconds};event=s.markers[0].event;return true;
}
bool NextFusionSourceGraph313Boundary(const FusionSourceGraph313& g,uint8_t layer,const FusionSourceGraph313View& v,
    FusionSourceGraph313Cursor p,const FusionSourceGraph313Parameters& params,float& output)
{
    if(!FusionSourceGraph313ViewValid(g,layer,v)||!Parameters(params))return false;const auto& s=g.states[v.state];if(!FusionSourceGraph313CursorValid(s,p))return false;
    float wait=(std::numeric_limits<float>::infinity)();const float rate=FusionSourceGraph313Rate(s,FusionSourceGraph313Multiplier(s,params));
    if(rate>0&&(!s.nullMotion||s.duration==1.f))
    {
        FusionSourceGraph313Cursor marker;FusionSourceGraph313Event ev;
        if(NextFusionSourceGraph313Marker(s,p,marker,ev))wait=float((Absolute(s,marker)-Absolute(s,p))/rate);
        if(v.fadeDuration==0)
        {
            FusionSourceGraph313Cursor exit;if(NextFusionSourceGraph313Exit(g,layer,v,p,params,exit))wait=(std::min)(wait,float((Absolute(s,exit)-Absolute(s,p))/rate));
        }
    }
    if(v.fadeDuration>0)
    {const auto& old=g.states[v.outgoing];const float fadeRate=v.fadeNormalized?FusionSourceGraph313Rate(old,FusionSourceGraph313Multiplier(old,params)):1;
     if(fadeRate>0)wait=(std::min)(wait,(v.fadeDuration-v.fadeElapsed)/fadeRate);}
    output=wait;return true;
}
bool NextFusionSourceGraph313Exit(const FusionSourceGraph313& g,uint8_t layer,const FusionSourceGraph313View& v,
    FusionSourceGraph313Cursor p,const FusionSourceGraph313Parameters& params,FusionSourceGraph313Cursor& output)
{
    if(!FusionSourceGraph313ViewValid(g,layer,v)||!Parameters(params)||v.fadeDuration>0)return false;const auto& s=g.states[v.state];
    if((s.nullMotion&&s.duration==0)||!FusionSourceGraph313CursorValid(s,p))return false;const double x=Absolute(s,p);double best=(std::numeric_limits<double>::infinity)();FusionSourceGraph313Cursor result;
    const auto test=[&](const FusionSourceGraph313Edge* edges,uint8_t count)
    {for(uint8_t i=0;i<count;++i){const auto& e=edges[i];uint32_t bits;if(!e.hasExitTime||!Conditions(e,params,bits)||(!e.self&&e.target==v.state))continue;
        const double whole=std::floor(e.exitTime);const float seconds=(e.exitTime-float(whole))*s.duration;
        const double loop=e.exitTime>=1||!s.loop?whole:std::floor((x-seconds)/s.duration)+1;
        const double next=loop*s.duration+seconds;if(next<=x||next>=best||loop>UINT32_MAX)continue;
        FusionSourceGraph313Cursor at{uint32_t(loop),seconds};if(!s.loop){if(at.loop==1&&at.seconds==0)at={0,s.duration};else if(at.loop)return;}
        if(!FusionSourceGraph313CursorValid(s,at))continue;best=next;result=at;}};
    test(g.layers[layer].any,g.layers[layer].anyCount);test(s.edges,s.edgeCount);if(!std::isfinite(best))return false;output=result;return true;
}
bool AdvanceFusionSourceGraph313Fade(const FusionSourceGraph313& g,const FusionSourceGraph313View& v,
    FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,float dt,FusionSourceGraph313View& output)
{
    if(v.outgoing>=g.stateCount||!std::isfinite(dt)||dt<0||dt>1)return false;const auto& old=g.states[v.outgoing];
    if(!FusionSourceGraph313CursorValid(old,a)||!FusionSourceGraph313CursorValid(old,b))return false;
    const double delta=Absolute(old,b)-Absolute(old,a);if(delta<0||!std::isfinite(delta))return false;
    auto next=v;next.outgoingSeconds=b.seconds;
    if(v.fadeDuration>0)next.fadeElapsed=float((std::min)(double(v.fadeDuration),v.fadeElapsed+(v.fadeNormalized?delta:double(dt))));
    output=next;return true;
}
}
