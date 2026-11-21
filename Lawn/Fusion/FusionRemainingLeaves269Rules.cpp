#include "FusionRemainingLeaves269Rules.h"
#include "FusionBasePlantRules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
constexpr double endo[]={.5};
constexpr double sword[]={double(1.1666666f),double(1.3333334f),1.5,double(1.6666666f),double(1.8333334f),2,
 double(2.1666667f),double(2.3333333f),2.5,2.5,double(2.6666667f),double(2.8333333f),3,
 double(3.1666667f),double(3.3333333f),3.5,double(3.6666667f),double(3.8333333f),4,double(4.1666665f),double(4.3333335f)};
const FusionShooterTimingProfile timings[]={
 {242,3,endo,double(.6f),double(.8333333f),true,8,"shoot"},
 {249,6,sword,double(.85f),double(4.5833335f),false,51,"shoot"}};
}
const FusionShooterTimingProfile* FindFusionRemainingLeaves269Timing(uint16_t k)
{for(const auto& t:timings)if(t.kind==k)return &t;return nullptr;}
bool FusionRemainingLeaves269ProjectileValid(uint16_t kind,int damage,uint8_t way,uint8_t effect,int type,int motion)
{return kind==8?way==16&&effect==3&&damage>=50&&damage<=2550&&damage%25==0&&type==PROJECTILE_PEA&&motion==MOTION_HOMING:
 kind==51&&(way==2||way==9)&&!effect&&damage==200&&type==PROJECTILE_PEA&&motion==MOTION_STAR;}
bool FusionRemainingLeaves269TrackTarget(int s,bool mc,bool alive,bool collider,bool retain)
{if(mc||!alive||!collider||s<0)return false;if(retain)return s!=1&&s!=7&&s!=12&&s!=16&&(s<21||s>24);
 switch(s){case 1:case 3:case 7:case 9:case 10:case 12:case 16:case 20:case 21:case 22:case 23:case 24:return false;default:return true;}}
bool FusionRemainingLeaves269CanHit(uint16_t kind,uint8_t way,int s,bool row,bool mc,bool collider)
{if(mc||!collider||s<0)return false;if(kind==8&&way==16)return FusionRemainingLeaves269TrackTarget(s,false,true,true,true);
 if(kind!=51||(way!=2&&way!=9)||(way==9&&!row))return false;
 switch(s){case 3:case 7:case 9:case 10:case 12:case 13:case 16:case 20:case 21:return false;default:return true;}}
bool FusionRemainingLeaves269Touches(uint16_t kind,double x,double y,double angle,double py,const FusionRecipientRect& r)
{
    if((kind!=8&&kind!=51)||!std::isfinite(x+y+angle+py+r.x+r.y+r.width+r.height)||py<=0||r.width<0||r.height<0)return false;
    const double px=kFusionBasePixelsX,c=std::cos(angle),s=std::sin(angle),offset=kind==51?2.5235982*.3:0;
    const double hw=kind==51?3.8728037*.3*.5:.2,hh=kind==51?.5446532*.4*.5:.2;
    const double dx=(r.x+r.width*.5-x)/px-offset*c,dy=-(r.y+r.height*.5-y)/py-offset*s,rx=r.width*.5/px,ry=r.height*.5/py;
    return std::abs(dx)<=hw*std::abs(c)+hh*std::abs(s)+rx&&std::abs(dy)<=hw*std::abs(s)+hh*std::abs(c)+ry&&
        std::abs(dx*c+dy*s)<=hw+rx*std::abs(c)+ry*std::abs(s)&&std::abs(-dx*s+dy*c)<=hh+rx*std::abs(s)+ry*std::abs(c);
}
bool TryGetFusionFertilize269Upgrade(uint16_t k,bool covered,uint16_t& out)
{switch(k){case 3:out=1027;return true;case 7:out=1070;return true;case 12:if(covered)return false;out=1067;return true;
 case 17:out=1060;return true;case 28:out=1120;return true;case 1037:out=1072;return true;case 1058:out=14;return true;
 case 1062:out=1075;return true;case 1180:out=1074;return true;case 1189:out=1039;return true;default:return false;}}
bool TryGetFusionEndo269Damage(int count,int& damage)
{if(count<0||count>100)return false;damage=int((float(count)*.5f+1)*50);return true;}
bool FusionSword269AttackEvent(unsigned event){return event<21&&event!=8;}
}
