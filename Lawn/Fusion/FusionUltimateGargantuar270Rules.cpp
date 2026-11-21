#include "FusionUltimateGargantuar270Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
const FusionZombieProfile profile={212,8000,24000,0,50,true,false,true};
struct Key {float t,x,inSlope,outSlope;};
// Actual212 source root curve; the final0.6s of the walk clip holds its last key.
constexpr Key walk[]={
 {0,0,0,-2.3666666f},{.16666667f,-.39444444f,-2.3035352f,-2.3035355f},
 {.41666666f,-.95454544f,-1.9020201f,-1.9020202f},{.8333333f,-1.6060605f,-1.4409097f,-1.4409091f},
 {1.1666666f,-2.0454545f,-.6590909f,-.65909106f},{1.5833334f,-2.0454545f,-.31999978f,-.32000002f},
 {2,-2.3121212f,-1.0109087f,-1.0109088f},{2.4166667f,-2.8878787f,-1.5006238f,-1.5006237f},
 {2.9833333f,-3.8055553f,-1.6194296f,0}};
bool Position(FusionZombieClipPosition p,float duration)
{return std::isfinite(p.seconds)&&p.seconds>=0&&p.seconds<=duration;}
double X(float t)
{
    for(size_t i=1;i<sizeof(walk)/sizeof(*walk);++i)if(t<walk[i].t)
    {const auto& a=walk[i-1];const auto& b=walk[i];const double h=b.t-a.t,u=(t-a.t)/h;
     return (2*u*u*u-3*u*u+1)*a.x+(u*u*u-2*u*u+u)*h*a.outSlope+
         (-2*u*u*u+3*u*u)*b.x+(u*u*u-u*u)*h*b.inSlope;}
    return walk[sizeof(walk)/sizeof(*walk)-1].x;
}
}
const FusionZombieProfile* FindFusionUltimateGarg270Profile(uint16_t kind){return kind==212?&profile:nullptr;}
bool CapFusionUltimateGarg270Damage(int damage,int& capped)
{if(damage<0)return false;capped=(std::min)(damage,5000);return true;}
bool ScaleFusionUltimateGarg270Status(float seconds,FusionUltimateGarg270Effect effect,float& scaled)
{
    if(!std::isfinite(seconds)||uint8_t(effect)>uint8_t(FusionUltimateGarg270Effect::KnockBack))return false;
    if(effect==FusionUltimateGarg270Effect::MindControl||effect==FusionUltimateGarg270Effect::KnockBack)return false;
    scaled=seconds>0?seconds*.5f:seconds;return true;
}
bool FusionUltimateGarg270MustThrow(float hp,int maximum,bool carrying,bool friendly,float x)
{return std::isfinite(hp)&&std::isfinite(x)&&maximum>0&&hp<float(maximum)*.5f&&carrying&&!friendly&&x>3;}
uint8_t FusionUltimateGarg270BodyStage(float hp,int maximum,uint8_t previous)
{
    if(maximum<=0||!std::isfinite(hp)||previous>2)return previous;
    if(hp<float(maximum)/3.f)return 2;
    if(float(maximum)*.33333334f<hp&&hp<=float(maximum)*.6666667f)return 1;
    return previous; // Source ValueBetween is lower-exclusive; exact1/3 preserves its old sprite.
}
FusionUltimateGarg270PlantHit PlanFusionUltimateGarg270PlantHit(uint16_t kind,bool big,bool nut,int level)
{
    if(kind==1095||big)return FusionUltimateGarg270PlantHit::Ignore;
    // Decoded0x40401 at base903 is903/913/921; source also includes925/931/1151.
    if(kind==903||kind==913||kind==921||kind==925||kind==931||kind==1151)return FusionUltimateGarg270PlantHit::Damage500;
    return nut&&level>=3?FusionUltimateGarg270PlantHit::Damage1000:FusionUltimateGarg270PlantHit::Crush;
}
bool FusionUltimateGarg270ZombieAllowed(int status,bool sameRow,bool opposite)
{return status>=0&&status!=1&&status!=10&&status!=12&&sameRow&&opposite;}
bool TryGetFusionUltimateGarg270RootDelta(FusionZombieClipPosition a,FusionZombieClipPosition b,bool friendly,double px,double& delta)
{
    if(!Position(a,kFusionUltimateGarg270WalkDuration)||!Position(b,kFusionUltimateGarg270WalkDuration)||
       b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds)||!std::isfinite(px)||px<=0)return false;
    const double units=double(b.loop-a.loop)*X(kFusionUltimateGarg270WalkDuration)+X(b.seconds)-X(a.seconds);
    delta=units*px*(friendly?-1:1);return std::isfinite(delta);
}
bool CountFusionUltimateGarg270Events(FusionZombieClipPosition a,FusionZombieClipPosition b,float event,uint32_t& count)
{
    const float duration=kFusionUltimateGarg270ActionDuration;
    if(!Position(a,duration)||!Position(b,duration)||!std::isfinite(event)||event<0||event>duration||
       b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;
    const uint64_t before=uint64_t(a.loop)+(a.seconds>=event),after=uint64_t(b.loop)+(b.seconds>=event);
    if(after<before||after-before>(std::numeric_limits<uint32_t>::max)())return false;
    count=uint32_t(after-before);return true;
}
bool StepFusionUltimateBall270(float x,float y,float vx,float vy,float dt,float land,float& nx,float& ny,float& nvy,bool& landed)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(vx)||!std::isfinite(vy)||!std::isfinite(dt)||dt<0||dt>1||!std::isfinite(land))return false;
    nvy=vy-dt*10;nx=x+vx*dt;ny=y+nvy*dt;
    if(!std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(nvy))return false;
    landed=ny<land+.5f&&nvy<0;return true;
}
}
