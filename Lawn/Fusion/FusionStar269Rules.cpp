#include "FusionStar269Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr FusionStar269Profile profiles[]={
    {1082,37,0,0,0,0,0},{1084,65535,80,20,80,6,2},{1089,65535,0,0,0,0,0},
    {1093,45,0,0,0,0,0},{1094,43,0,0,0,0,0},{1095,44,0,0,0,0,0},
    {1104,48,0,0,0,0,0},{920,65535,240,60,240,1.5f,3}};
bool PointValid(FusionStar269Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
constexpr float radius=.8f,tau=6.2831855f;
constexpr double cactusEvents[]={double(.45f),double(.75f)},starEvents[]={double(.41666666f)},blowEvents[]={double(.38333333f)};
const FusionShooterTimingProfile timings[]={
    {1082,1.5f,cactusEvents,double(1.2f),double(.9f),true,37,"shoot"},
    {1084,1,blowEvents,1,double(1.2666667f),true,65535,"blow"},
    {920,1,blowEvents,1,double(1.2666667f),true,65535,"blow"},
    {1093,1.5f,starEvents,double(1.1f),double(.6666667f),true,45,"shoot"},
    {1094,3,starEvents,double(1.1f),double(.6666667f),true,43,"shoot"},
    {1095,1.5f,starEvents,double(1.1f),double(.6666667f),true,44,"shoot"},
    {1104,3,starEvents,double(1.1f),double(.6666667f),true,48,"shoot"}};
}
const FusionShooterTimingProfile* FindFusionStar269TimingProfile(uint16_t kind)
{for(const auto& p:timings)if(p.kind==kind)return &p;return nullptr;}
bool FusionStar269ProjectileClass(uint16_t kind)
{return kind>=39&&kind<=49&&kind!=47;}
bool FusionStar269CanHitStatus(uint8_t way,int status)
{
    if(status<0||status==3||status==9||status==12||status==13||status==16||status==20||status==21)return false;
    if(way==7||way==8)return status==10;
    if(way==6||way==16||way==18)return status!=7;
    if(way==10||way==11||way==12||way==14||way>18)return true;
    if(way==1||way==13||way==17)return status!=10;
    return status!=7&&status!=10;
}
const FusionStar269Profile* FindFusionStar269Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
bool FusionStar269PlantTakesDamage(uint16_t kind){return FindFusionStar269Profile(kind)&&kind!=1095;}
bool FusionStar269CaptureTouches(FusionStar269Point c,FusionStar269Point p,float angle)
{
    if(!PointValid(c)||!PointValid(p)||!std::isfinite(angle))return false;
    const double dx=c.x-p.x,dy=c.y-p.y,cosine=std::cos(angle),sine=std::sin(angle);
    const double x=(std::max)(0.,std::abs(dx*cosine+dy*sine)-.32703882*1.7*.5);
    const double y=(std::max)(0.,std::abs(-dx*sine+dy*cosine)-.31575292*1.7*.5);
    return x*x+y*y<=9;
}
int FusionStar269Capacity(uint16_t kind,bool pumpkin)
{const auto* p=FindFusionStar269Profile(kind);return p?(pumpkin?p->pumpkinCapacity:p->capacity):0;}
bool TryFindFusionStar269FreeSlot(uint16_t kind,bool pumpkin,const uint32_t* refs,std::size_t count,std::size_t& index)
{
    const auto* p=FindFusionStar269Profile(kind);if(!p||!p->storageSlots||!refs||count!=p->storageSlots)return false;
    const auto capacity=std::size_t(FusionStar269Capacity(kind,pumpkin));
    for(std::size_t i=0;i<capacity;++i)if(!refs[i]){index=i;return true;}return false;
}
bool FusionStar269CanCapture(bool attracting,bool component,bool accelerated,uint8_t way,bool nearby)
{return attracting&&component&&!accelerated&&way!=10&&nearby;}
bool TryAdvanceFusionStar269Hold(uint16_t kind,float elapsed,float delta,float& after,bool& release)
{
    const auto* p=FindFusionStar269Profile(kind);
    if(!p||!p->storageSlots||!std::isfinite(elapsed)||elapsed<0||!std::isfinite(delta)||delta<0)return false;
    const bool due=elapsed>=p->storageSeconds;const float next=due?elapsed:elapsed+delta;
    if(!std::isfinite(next))return false;after=next;release=due;return true;
}
bool TryMakeFusionStar269Dispatch(uint16_t kind,bool death,int damage,float speed,FusionStar269Dispatch& result)
{
    const auto* p=FindFusionStar269Profile(kind);if(!p||!p->storageSlots||damage<0||!std::isfinite(speed)||speed<0)return false;
    const int multiplier=death?1:p->dispatchDamageMultiplier;
    if(damage>(std::numeric_limits<int>::max)()/multiplier)return false;
    const float nextSpeed=death?speed:speed*2;if(!std::isfinite(nextSpeed))return false;
    result={damage*multiplier,nextSpeed,6,true};return true;
}
bool TryAdvanceFusionStar269Approach(FusionStar269Point center,FusionStar269Point p,float delta,FusionStar269Point& after,bool& moving,float& angle)
{
    if(!PointValid(center)||!PointValid(p)||!std::isfinite(delta)||delta<0)return false;
    float dx=p.x-center.x,dy=p.y-center.y;const float radial=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(radial))return false;if(radial>1e-5f){dx/=radial;dy/=radial;}else dx=dy=0;
    const float tx=center.x+dx*radius,ty=center.y+dy*radius;
    dx=tx-p.x;dy=ty-p.y;const float dz=-p.z,dist=std::sqrt(dx*dx+dy*dy+dz*dz),step=6*delta;
    if(!std::isfinite(dist)||!std::isfinite(step))return false;
    FusionStar269Point next={tx,ty,0};if(dist>step&&dist>0)next={p.x+dx/dist*step,p.y+dy/dist*step,p.z+dz/dist*step};
    if(!PointValid(next))return false;
    const float rx=next.x-tx,ry=next.y-ty;const bool stays=std::sqrt(rx*rx+ry*ry)>=.1f;
    if(!stays)angle=std::atan2(next.y-center.y,next.x-center.x);after=next;moving=stays;return true;
}
bool TryAdvanceFusionStar269Orbit(FusionStar269Point c,float delta,float& speed,float& angle,FusionStar269Point& after)
{
    if(!PointValid(c)||!std::isfinite(delta)||delta<0||!std::isfinite(speed)||!std::isfinite(angle))return false;
    float nextAngle=angle+delta*speed,nextSpeed=(std::max)(speed-delta,-6.f);
    if(!std::isfinite(nextAngle)||!std::isfinite(nextSpeed))return false;
    if(nextAngle>=tau)nextAngle-=tau; // Source subtracts once; negative angles stay negative.
    const FusionStar269Point p={c.x+std::cos(nextAngle)*radius,c.y+std::sin(nextAngle)*radius,after.z};
    if(!PointValid(p))return false;angle=nextAngle;speed=nextSpeed;after=p;return true;
}
bool TrySelectFusionStar269Target(const FusionStar269Target* candidates,std::size_t count,float x,float y,bool flying,std::size_t& index)
{
    if((count&&!candidates)||!std::isfinite(x)||!std::isfinite(y))return false;
    std::size_t chosen=count;double best=(std::numeric_limits<double>::max)();bool hasFlying=false;
    for(std::size_t i=0;i<count;++i)
    {
        const auto& c=candidates[i];if(!c.eligible)continue;if(!std::isfinite(c.x)||!std::isfinite(c.y))return false;
        if(flying&&hasFlying&&!c.flying)continue;
        const double dx=double(c.x)-x,dy=double(c.y)-y,d=dx*dx+dy*dy;
        if((flying&&c.flying&&!hasFlying)||d<best){chosen=i;best=d;}if(flying&&c.flying)hasFlying=true;
    }
    if(chosen==count)return false;index=chosen;return true;
}
bool TryMakeFusionStar269HomingVelocity(bool target,bool valid,float x,float y,float tx,float ty,float speed,float& vx,float& vy,bool& collider,bool& reacquire)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(speed)||speed<0)return false;
    if(!target){vx=-speed*.25f;vy=0;collider=false;reacquire=true;return true;}
    if(!valid){reacquire=true;return true;}
    if(!std::isfinite(tx)||!std::isfinite(ty))return false;
    const double dx=double(tx)-x,dy=double(ty)-y,length=std::sqrt(dx*dx+dy*dy);
    vx=length>1e-5?float(dx/length*speed):0;vy=length>1e-5?float(dy/length*speed):0;collider=true;reacquire=false;return true;
}
bool FusionStar269CactusCanAcquire(int status,bool mc,bool row,float x,float zx,float vision)
{return status==10&&!mc&&row&&std::isfinite(x)&&std::isfinite(zx)&&std::isfinite(vision)&&x<zx&&zx<vision;}
bool FusionStar269StartupBlowTarget(int status){return status==10;}
bool FusionStar269MeteoriteDue(int light,float countdown){return light>3&&std::isfinite(countdown)&&countdown<=0;}
bool TryMakeFusionStar269Shot(uint16_t kind,int light,int magnets,int damage,FusionStar269Shot& result)
{
    const auto* p=FindFusionStar269Profile(kind);if(!p||p->projectile==65535||light<0||magnets<0||damage<0)return false;
    FusionStar269Shot shot{p->projectile,0,damage,false};
    if(kind==1104)
    {
        const double scaled=(double(magnets)*.05+1)*damage;if(scaled>(std::numeric_limits<int>::max)())return false;
        shot.projectile=light<1?39:48;shot.special=light>2?4:0;shot.damage=(std::min)(120,int(scaled));shot.requestMeteorite=light>3;
    }
    result=shot;return true;
}
bool TryMakeFusionStar269Hit(uint16_t kind,uint8_t way,int damage,int first,int second,FusionStar269Hit& result)
{
    if(damage<0||(kind!=39&&kind!=40&&kind!=43&&kind!=44&&kind!=45&&kind!=48))return false;
    FusionStar269Hit hit{damage,uint8_t((kind==39||kind==40||kind==48)&&way==9?4:0),0,false};
    if(kind==45&&(first==2||first==4||first==7||first==8||second==1||second==3))
    {if(damage>(std::numeric_limits<int>::max)()/2)return false;hit.damage*=2;}
    if(kind==43){hit.area=true;hit.radius=1.5f;hit.sourceDamageType=10;}result=hit;return true;
}
bool TryAdvanceFusionUltimateStar269Status(int kind,int before,int& after,bool& destroy)
{
    if(before<0||before==(std::numeric_limits<int>::max)())return false;
    if(kind!=26&&kind!=205&&kind!=206){after=before;destroy=false;return true;}
    after=before+1;destroy=after>9;return true;
}
bool FusionStar269HomingOutsideX(float x,float minimum,float maximum)
{return std::isfinite(x)&&std::isfinite(minimum)&&std::isfinite(maximum)&&minimum<=maximum&&(x<minimum||x>maximum);}
}
