#include "FusionUltimateDeps270Rules.h"
#include "FusionNativeUltimateDeps270.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
const FusionZombieProfile profiles[]={{37,270,0,0,50,true},{40,1350,0,0,50,true,false,true},{213,1500,0,0,200,true}};
struct Key {float t,x,inSlope,outSlope;};
constexpr Key imp[]={
 {0,18.757576f,0,-1.5757599f},{.25f,18.363636f,-1.2363663f,-1.2363663f},{.5f,18.139393f,-1.1939392f,-1.1939392f},
 {.75f,17.766666f,-.94999886f,-.94999886f},{1.0833334f,17.630302f,-.62878567f,-.6287861f},
 {1.3333334f,17.418182f,-1.3393936f,-1.3393936f},{1.5833334f,16.960606f,-.99697113f,-.99697113f},
 {2.0833333f,16.878788f,-.38181674f,-.3818169f},{2.5833333f,16.578789f,-.5999985f,0}};
constexpr Key obsidian[]={
 {0,0,0,-1.4533333f},{.25f,-.3633333f,-1.3066658f,-1.3066666f},{.5f,-.6533333f,-1.1000001f,-1.0999999f},
 {.75f,-.91333324f,-.67499995f,-.67499995f},{1.0833334f,-1.0166667f,-.6283333f,-.6283332f},
 {1.3333334f,-1.2533332f,-1.4533337f,-1.4533331f},{1.5833334f,-1.7433332f,-1.0333338f,-1.0333333f},
 {2.0833333f,-1.7966665f,-.15333341f,-.15333344f},{2.5833333f,-1.8966666f,-.2000003f,0}};
constexpr Key entering[]={{0,-1.3170004f,0,-12.099613f},{.8666667f,-11.803332f,.0000013084759f,0}};
double Sample(const Key* keys,size_t size,float t)
{
    for(size_t i=1;i<size;++i)if(t<keys[i].t)
    {const auto& a=keys[i-1];const auto& b=keys[i];const double h=b.t-a.t,u=(t-a.t)/h;
     return (2*u*u*u-3*u*u+1)*a.x+(u*u*u-2*u*u+u)*h*a.outSlope+(-2*u*u*u+3*u*u)*b.x+(u*u*u-u*u)*h*b.inSlope;}
    return keys[size-1].x;
}
bool Span(FusionZombieClipPosition a,FusionZombieClipPosition b,float duration)
{return std::isfinite(a.seconds)&&std::isfinite(b.seconds)&&a.seconds>=0&&a.seconds<=duration&&b.seconds>=0&&b.seconds<=duration&&
    b.loop>=a.loop&&(b.loop!=a.loop||b.seconds>=a.seconds);}
}
const FusionZombieProfile* FindFusionUltimateDeps270Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
bool FusionMachine270CanReserveOffspring(int live,int capacity,bool includeParent)
{
    return live>=0&&capacity>=live&&capacity-live>=kFusionMachine270OffspringCount+(includeParent?1:0);
}
bool FusionUltimateDeps270RejectEffect(uint16_t kind,int status,FusionUltimateDeps270Effect e)
{
    if(kind==40)return e==FusionUltimateDeps270Effect::Butter||e==FusionUltimateDeps270Effect::Poison||e==FusionUltimateDeps270Effect::Garlic;
    return status==16&&((kind==213&&e==FusionUltimateDeps270Effect::Freeze)||(kind==37&&(e==FusionUltimateDeps270Effect::Freeze||e==FusionUltimateDeps270Effect::Butter)));
}
bool ChooseFusionMachine270Type(int draw,uint8_t& type){if(draw<0||draw>2)return false;type=uint8_t(draw);return true;}
uint8_t FusionMachine270DamageStage(float hp,int maximum,uint8_t previous)
{
    if(!std::isfinite(hp)||maximum<=0||previous>2)return previous;
    const float low=float(maximum)/3.f,high=float(int32_t(uint32_t(maximum)*2U))/3.f;
    if(low<hp&&hp<=high)return 1;if(hp<low)return 2;return previous;
}
bool FusionMachine270CanHitProjectile(uint8_t way,bool hostile,bool row)
{return way==2||way==15||way==18||(!hostile&&row&&way!=13);}
bool TryGetFusionUltimateDeps270RootDelta(uint16_t kind,bool enter,FusionZombieClipPosition a,FusionZombieClipPosition b,bool friendly,double px,double& delta)
{
    const float duration=kind==40?2.f:enter?.8666667f:2.5833333f;
    if(!FindFusionUltimateDeps270Profile(kind)||!Span(a,b,duration)||!std::isfinite(px)||px<=0||
       (enter&&(kind!=213||a.loop||b.loop)))return false;
    if(kind==40)delta=(-6.*(double(b.loop-a.loop)*duration+b.seconds-a.seconds))*px;
    else
    {const Key* keys=enter?entering:kind==37?imp:obsidian;const size_t size=enter?2:9;
     delta=(double(b.loop-a.loop)*(keys[size-1].x-keys[0].x)+Sample(keys,size,b.seconds)-Sample(keys,size,a.seconds))*px;}
    if(friendly)delta=-delta;return std::isfinite(delta);
}
bool CountFusionUltimateDeps270Events(uint16_t kind,bool enter,FusionZombieClipPosition a,FusionZombieClipPosition b,uint32_t& count)
{
    if(kind==40||enter&&kind!=213||!FindFusionUltimateDeps270Profile(kind)||!Span(a,b,enter?.8666667f:2.1666667f))return false;
    uint64_t n=0;const float events[]={.9166667f,1.8333334f};
    for(int i=0;i<(enter?1:2);++i)
    {const float event=enter?.81666666f:events[i];const uint64_t before=uint64_t(a.loop)+(a.seconds>=event),after=uint64_t(b.loop)+(b.seconds>=event);
     if(after<before)return false;n+=after-before;}
    if(n>(std::numeric_limits<uint32_t>::max)())return false;count=uint32_t(n);return true;
}
bool NativeUltimateDeps270StateValid(uint16_t kind,const NativeUltimateDeps270State& s)
{
    if(!FindFusionUltimateDeps270Profile(kind)||s.bodyStage>2||s.machineType>2||!std::isfinite(s.startY)||std::abs(s.startY)>4096||s.rowTicks>200)return false;
    if(kind!=40&&(s.machineType||s.bodyStage||s.moving))return false;
    if(kind!=37&&!s.visible)return false;
    if(!s.moving)return s.originRow== -1&&s.targetRow== -1&&!s.startY&&!s.rowTicks;
    return kind==40&&s.originRow>=0&&s.originRow<6&&s.targetRow>=0&&s.targetRow<6&&std::abs(s.originRow-s.targetRow)==1;
}
float FusionMachine270BallVisualSeconds(int age)
{
    const int n=(std::max)(0,(std::min)(200,age));
    return float(.005*n-.0000125*double(n)*(n+1)); // Integral of source Animator.Speed=-VX, sampled by the real native age.
}
}
