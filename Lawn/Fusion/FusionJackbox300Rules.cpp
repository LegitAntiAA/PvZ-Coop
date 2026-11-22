#include "FusionJackbox300Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
using Phase=FusionJackbox300Phase;
bool Original(float f)
{return std::isfinite(f)&&((f>=4.4f&&f<=7.54f)||(f>=13.22f&&f<=22.68f));}
bool Span(Phase phase,FusionZombieClipPosition& a,FusionZombieClipPosition& b)
{
    FusionJackbox300Clip c;if(!TryGetFusionJackbox300Clip(phase,c))return false;
    const auto normalize=[&](FusionZombieClipPosition& p)
    {if(!c.loop&&p.loop==1&&p.seconds==c.seconds)p.loop=0;return c.loop||p.loop==0;};
    return normalize(a)&&normalize(b)&&std::isfinite(a.seconds)&&std::isfinite(b.seconds)&&
        a.seconds>=0&&a.seconds<=c.seconds&&b.seconds>=0&&b.seconds<=c.seconds&&
        b.loop>=a.loop&&(b.loop>a.loop||b.seconds>=a.seconds);
}
struct Key {float time,value,inSlope,outSlope;};
constexpr float negativeInfinity=-(std::numeric_limits<float>::infinity)();
constexpr Key walk[]={{0,.30499998f,0,negativeInfinity},{.083333336f,.30499998f,0,0},
    {.6666667f,.11666666f,-8.1346165e-8f,negativeInfinity},{.75f,.11666666f,0,0},
    {1.5f,-.0074999994f,6.7055225e-8f,0}};
double Sample(float t)
{
    if(t<=walk[0].time)return walk[0].value;if(t>=walk[4].time)return walk[4].value;
    int i=1;while(walk[i].time<=t)++i;const auto& a=walk[i-1];const auto& b=walk[i];
    if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.value;
    const double h=b.time-a.time,u=(t-a.time)/h,u2=u*u,u3=u2*u;
    return (2*u3-3*u2+1)*a.value+(u3-2*u2+u)*h*a.outSlope+(-2*u3+3*u2)*b.value+(u3-u2)*h*b.inSlope;
}
bool AllowedTransition(Phase from,Phase to)
{return from!=Phase::Death&&to==Phase::Death || from==Phase::Walk&&(to==Phase::Eat||to==Phase::Pop) ||
    from==Phase::Eat&&(to==Phase::Walk||to==Phase::Pop);}
}
bool FusionJackbox300Kind(uint16_t k){return k==24||k==30;}
bool TryGetFusionJackbox300Clip(Phase phase,FusionJackbox300Clip& out)
{
    FusionJackbox300Clip c;
    switch(phase)
    {
    case Phase::Walk:c={"walk","anim_walk",1.5f,1.6f,true};break;
    case Phase::Eat:c={"eat","anim_eat",1.1666666f,1.6f,true};break;
    // Clock-only NORMAL carrier. The canonical phase disambiguates the same
    // native span used by Eat; the genuine source pop model is rendered instead.
    case Phase::Pop:c={"pop","anim_eat",2.1666667f,2,false};break;
    case Phase::Death:c={"death","anim_death",1.6666666f,2,false};break;
    default:return false;
    }
    out=c;return true;
}
bool FusionJackbox300ViewValid(uint16_t kind,const FusionJackbox300View& v)
{
    FusionJackbox300Clip c,old;
    if(!FusionJackbox300Kind(kind)||!TryGetFusionJackbox300Clip(v.phase,c)||!TryGetFusionJackbox300Clip(v.outgoing,old)||
        !Original(v.originalCountdown)||!std::isfinite(v.countdown)||v.countdown<0||v.countdown>22.68f||
        !std::isfinite(v.outgoingSeconds)||!std::isfinite(v.transitionElapsed)||!std::isfinite(v.transitionTotal))return false;
    if(!v.boxPresent&&(v.audioPlaying||v.popTriggered))return false;
    // Source Unfreeze/UnButter eligibility does not test the current status.
    if(v.popTriggered&&(!v.boxPresent||v.countdown!=0))return false;
    if(v.phase==Phase::Pop&&!v.popTriggered)return false;
    if(v.transitionTotal==0)return v.outgoing==Phase::Walk&&v.outgoingSeconds==0&&v.transitionElapsed==0;
    return v.transitionTotal==.25f&&AllowedTransition(v.outgoing,v.phase)&&v.transitionElapsed>=0&&v.transitionElapsed<=.25f&&
        v.outgoingSeconds>=0&&v.outgoingSeconds<=old.seconds;
}
bool TryInitializeFusionJackbox300(uint16_t kind,int chance,float draw,bool instant,FusionJackbox300View& out)
{
    if(!FusionJackbox300Kind(kind)||chance<0||chance>=100||!std::isfinite(draw)||
        (chance<5?(draw<4.4f||draw>7.54f):(draw<13.22f||draw>22.68f)))return false;
    FusionJackbox300View v;v.originalCountdown=draw;v.countdown=instant?.01f:draw;out=v;return true;
}
bool TryStepFusionJackbox300Countdown(uint16_t kind,const FusionJackbox300View& v,const FusionJackbox300Conditions& c,
    float dt,FusionJackbox300View& out)
{
    if(!FusionJackbox300ViewValid(kind,v)||!std::isfinite(dt)||dt<0)return false;auto next=v;
    if(v.boxPresent&&v.countdown>0&&c.sourcePlaying&&!c.beforeDying&&v.phase!=Phase::Death&&c.speedNonzero)
    {
        next.countdown=(std::max)(0.f,v.countdown-dt);
        if(next.countdown==0){next.popTriggered=true;next.audioPlaying=false;}
    }
    out=next;return true;
}
int FusionJackbox300SourceStatus(const FusionJackbox300View& v)
{return v.phase==Phase::Death?1:v.popTriggered?11:0;}
bool TryFilterFusionJackbox300Damage(uint16_t kind,const FusionJackbox300View& v,int incoming,int& out)
{
    if(!FusionJackbox300ViewValid(kind,v)||incoming<0)return false;
    out=FusionJackbox300SourceStatus(v)==11?0:incoming;return true;
}
bool TrySelectFusionJackbox300Phase(uint16_t kind,const FusionJackbox300View& v,bool attack,bool positive,Phase& out)
{
    if(!FusionJackbox300ViewValid(kind,v)||v.transitionTotal>0||!positive)return false;
    // Original serialized transition-list priority: contact transition precedes pop.
    if(v.phase==Phase::Walk&&attack){out=Phase::Eat;return true;}
    if(v.phase==Phase::Eat&&!attack){out=Phase::Walk;return true;}
    if((v.phase==Phase::Walk||v.phase==Phase::Eat)&&v.popTriggered){out=Phase::Pop;return true;}
    return false;
}
bool TryAdvanceFusionJackbox300Clock(Phase phase,FusionZombieClipPosition a,float speed,float dt,FusionZombieClipPosition& out)
{
    auto same=a;FusionJackbox300Clip c;
    if(!Span(phase,a,same)||!TryGetFusionJackbox300Clip(phase,c)||!std::isfinite(speed)||speed<0||!std::isfinite(dt)||dt<0)return false;
    if(speed==0||dt==0){out=a;return true;}
    const double time=double(a.seconds)+double(dt)*speed*c.multiplier;
    if(!std::isfinite(time))return false;
    if(!c.loop){out={0,float((std::min)(time,double(c.seconds)))};return true;}
    const double loops=std::floor(time/c.seconds);
    if(loops>double((std::numeric_limits<uint32_t>::max)()-a.loop))return false;
    out={a.loop+uint32_t(loops),float(time-loops*c.seconds)};return true;
}
bool TryGetFusionJackbox300NextEvent(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition& out)
{
    auto b=a;FusionJackbox300Clip c;if(!Span(phase,a,b)||!TryGetFusionJackbox300Clip(phase,c)||phase==Phase::Walk)return false;
    const float t=phase==Phase::Eat?.73333335f:c.seconds;
    if(t>a.seconds){out={a.loop,t};return true;}
    if(c.loop&&a.loop<(std::numeric_limits<uint32_t>::max)()){out={a.loop+1,t};return true;}
    return false;
}
bool TryGetFusionJackbox300Events(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b,uint8_t& out)
{
    FusionJackbox300Clip c;if(!Span(phase,a,b)||!TryGetFusionJackbox300Clip(phase,c))return false;
    const float t=phase==Phase::Eat?.73333335f:c.seconds;
    const bool cross=b.loop>a.loop?(a.seconds<t||b.seconds>=t||b.loop>a.loop+1):(a.seconds<t&&b.seconds>=t);
    out=phase==Phase::Walk||!cross?0:phase==Phase::Eat?Jackbox300Bite:phase==Phase::Pop?Jackbox300Explode:Jackbox300Destroy;return true;
}
bool TryGetFusionJackbox300RootDelta(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b,double& out)
{
    if(!Span(phase,a,b))return false;
    out=phase==Phase::Walk?double(b.loop-a.loop)*(walk[4].value-walk[0].value)+Sample(b.seconds)-Sample(a.seconds):0;
    return std::isfinite(out);
}
bool TryPlanFusionJackbox300Death(uint16_t kind,const FusionJackbox300View& v,int reason,float roll,bool& out)
{
    if(!FusionJackbox300ViewValid(kind,v)||reason<0||!std::isfinite(roll)||roll<0||roll>v.originalCountdown)return false;
    out=kind==30&&v.boxPresent&&reason==0&&v.countdown<roll;return true;
}
bool TryGetFusionJackbox300ArmorStage(uint16_t kind,int hp,int max,uint8_t previous,uint8_t& out,int& sprite)
{
    if(kind!=30||max<=0||hp<0||hp>max||previous>2)return false;
    const uint32_t twice=uint32_t(max)*2u;const int64_t wrap=twice&0x80000000u?int64_t(twice)-0x100000000LL:twice;
    uint8_t stage=previous;if(hp<wrap/3&&stage<1)stage=1;if(hp<max/3&&stage<2)stage=2;
    out=stage;sprite=stage?63+stage:-1;return true; // Source spritePrefab64/65.
}
bool TryDoubleFusionJackbox300Speed(uint16_t kind,float before,float& after)
{
    if(kind!=30||!std::isfinite(before)||before<.9f||before>1.8f)return false;after=before+before;return true;
}
bool FusionJackbox300RuntimeSpeedValid(uint16_t kind,const FusionJackbox300View& v,float speed)
{
    if(!FusionJackbox300ViewValid(kind,v)||!std::isfinite(speed))return false;
    return speed>=.9f&&speed<=(kind==30&&!v.boxPresent?3.6f:1.8f);
}
bool TryDisarmFusionJackbox300(uint16_t kind,const FusionJackbox300View& v,FusionJackbox300View& out)
{
    if(!FusionJackbox300ViewValid(kind,v)||!v.boxPresent||v.popTriggered||v.phase==Phase::Death)return false;
    auto next=v;next.boxPresent=false;next.audioPlaying=false;out=next;return true;
}
}
