#include "FusionSummonPlants269Rules.h"
#include "../../ConstEnums.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr double summon[]={double(.33333334f)},explode[]={double(.6666667f)},
    gold[]={double(.8666667f),double(4.0666666f)},hypno[]={double(.6666667f)};
const FusionShooterTimingProfile timings[]={
    {917,30,summon,.5,double(.6666667f),true,65535,"summon"},
    {928,1,explode,double(.7f),double(.6666667f),true,65535,"explode"},
    {1150,5,gold,1,double(4.4333334f),true,65535,"attrack"},
    {1157,1,hypno,1,double(1.85f),true,65535,"attrack"}};
bool Point(FusionCherryMagnet269Point p){return std::isfinite(p.x)&&std::isfinite(p.y);}
}
bool IsFusionSummonPlants269Kind(uint16_t k)
{return k==917||k==928||k==1041||k==1147||k==1150||k==1157||k==1164||k==1166;}
const FusionShooterTimingProfile* FindFusionSummonPlants269Timing(uint16_t k)
{for(const auto& p:timings)if(p.kind==k)return &p;return nullptr;}
float FusionSummonPlants269MaximumDuration(uint16_t k,float speed)
{if(k==1166)return std::isfinite(speed)&&speed>=.9f&&speed<=1.1f?float(double(.68333334f)/speed):0;
 const auto* t=FindFusionSummonPlants269Timing(k);return t?FusionShooterDuration(*t,speed):0;}
bool FusionSummonPlants269StateValid(uint16_t k,uint8_t mode,uint16_t bag,uint16_t colors,int ticks,int launch,uint32_t impacts)
{
    if(!IsFusionSummonPlants269Kind(k)||ticks<0||launch<0||impacts>uint32_t((std::numeric_limits<int>::max)()))return false;
    switch(k)
    {
    case 917:return mode<=1&&bag<=5&&!colors&&ticks<=3000;
    case 928:return mode==1&&!bag&&!colors&&!ticks;
    case 1147:return !mode&&!bag&&colors<=16383&&ticks<=4500;
    case 1150:return mode<=1&&!bag&&!colors&&ticks<=500;
    case 1157:return (mode==0||mode==2)&&bag<=10&&!colors&&!ticks;
    case 1166:return mode<=1&&bag<=5&&!colors&&ticks<=1500&&launch<=300&&impacts<5;
    default:return !mode&&!bag&&!colors&&!ticks&&!impacts;
    }
}
bool TryReceiveFusionHypnoNut269Color(uint16_t before,unsigned color,uint16_t& after,bool& rainbow)
{
    if(before>16383||color>=7)return false;const unsigned shift=2*color;
    const unsigned count=(before>>shift)&3;uint16_t next=uint16_t(before&~(3U<<shift));next|=uint16_t((std::min)(count+1,3U)<<shift);
    bool all=true;for(unsigned i=0;i<7;++i)all=all&&((next>>(2*i))&3)!=0;
    if(all)for(unsigned i=0;i<7;++i)next=uint16_t(next-(1U<<(2*i)));
    after=next;rainbow=all;return true;
}
bool FusionGoldMagnet269Planar(int type,int motion)
{
    switch(type){case PROJECTILE_PEA:case PROJECTILE_SNOWPEA:case PROJECTILE_PUFF:case PROJECTILE_SPIKE:
        case PROJECTILE_STAR:case PROJECTILE_FIREBALL:break;default:return false;}
    return motion==MOTION_STRAIGHT||motion==MOTION_PUFF||motion==MOTION_THREEPEATER||(type==PROJECTILE_STAR&&motion==MOTION_STAR);
}
bool FusionGoldMagnet269Eligible(int type,int motion,uint8_t way,bool attracted,int delta,float vy)
{return !attracted&&delta!=0&&(delta==-1||delta==1)&&std::isfinite(vy)&&FusionGoldMagnet269Planar(type,motion)&&
    (way==0||((way==4||way==5)&&vy==0));}
bool TryStepFusionGoldMagnet269(uint8_t way,bool zombie,int age,float& y)
{
    if((way!=4&&way!=5)||age<0)return false;const float t=float((std::max)(0,age-1))*.01f;
    y=t>=.5f?0.f:way==4?(zombie?6.f:7.f)-t*16.f:t*16.f-(zombie?8.f:7.f);return true;
}
bool TryStepFusionCherryMagnet269(FusionCherryMagnet269Point p,FusionCherryMagnet269Point c,
    FusionCherryMagnet269Point target,bool aim,FusionCherryMagnet269Point v,FusionCherryMagnet269Point& after,bool& impact)
{
    if(!Point(p)||!Point(c)||!Point(target)||!Point(v))return false;
    float dx=(aim?target.x:c.x)-p.x,dy=(aim?target.y:c.y)-p.y;
    const float r=std::sqrt(dx*dx+dy*dy);impact=aim&&r<1;
    if(aim)
    {if(r>1e-5f){v={dx/r*40.f,dy/r*40.f};}else v={};if(impact){v.x*= -.8f;v.y*= -.8f;}}
    else
    {
        // The source uses its normalized3D direction (sourceZ0 here). Inside1
        // it applies tangential force-3; outside it attracts by (r²+r)*5+10.
        if(r>1e-5f){dx/=r;dy/=r;}else dx=dy=0;
        const float speed=std::sqrt(v.x*v.x+v.y*v.y);
        // Shared IL2CPP address2FB110 is Vector2.zero (Ghidra's misleading
        // MousePositionDebug label). This branch adds zero force after clamp.
        if(r<3&&speed>10){after={v.x/speed*10,v.y/speed*10};return true;}
        const float force=r<1?-3.f:((r*r+r)*5.f+10.f)/(r*r);
        const FusionCherryMagnet269Point direction=r<1?FusionCherryMagnet269Point{dy,-dx}:FusionCherryMagnet269Point{dx,dy};
        v.x+=direction.x*force*.01f;v.y+=direction.y*force*.01f;
    }
    if(!Point(v))return false;after=v;return true;
}
bool FusionSummonPlants269WaterItemSupported(uint8_t item)
{return item==0||item==1||item==2||item==7||item==10;}
}
