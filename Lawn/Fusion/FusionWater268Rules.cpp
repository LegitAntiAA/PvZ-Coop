#include "FusionWater268Rules.h"
#include "FusionBasePlantRules.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
namespace LawnFusion
{
namespace
{
constexpr double girlEvents[]={1.0,2.75};
constexpr double cattailEvents[]={static_cast<double>(.33333334f),static_cast<double>(.6666667f)};
constexpr double kelpEvents[]={.5};
constexpr double cactusEvents[]={static_cast<double>(.3f)};
constexpr double cactusGrownEvents[]={static_cast<double>(.3f),.5,static_cast<double>(.7f),static_cast<double>(.9f)};
constexpr double starEvents[]={static_cast<double>(.41666666f)};
constexpr double starGrownEvents[]={static_cast<double>(.41666666f),static_cast<double>(.6666667f)};
constexpr FusionShooterTimingProfile timings[]={
    {252,1.5f,girlEvents,3,static_cast<double>(3.6666665f),true,20},
    {1067,1.5f,cattailEvents,1,static_cast<double>(.8333333f),true,37},
    {1068,1.5f,cattailEvents,1,static_cast<double>(.8333333f),true,34},
    {1069,1.5f,cattailEvents,1,static_cast<double>(.8333333f),true,35},
    {1051,1.5f,kelpEvents,1,1,true,29},{1066,3,kelpEvents,1,1,true,32},
    {1076,1.5f,cactusEvents,1,.5,true,9,"shoot"},
    {1076,1.5f,cactusEvents,1,.5,true,37,"shoot2"},
    {1076,1.5f,cactusGrownEvents,1,static_cast<double>(1.1f),false,9,"shoot_grown"},
    {1076,1.5f,cactusGrownEvents,1,static_cast<double>(1.1f),false,37,"shoot2_grown"},
    {1083,1.5f,starEvents,static_cast<double>(1.1f),static_cast<double>(.6666667f),true,41,"shoot"},
    {1083,1.5f,starGrownEvents,1,static_cast<double>(.8333333f),true,41,"shoot2"},
    {1085,1.5f,starEvents,static_cast<double>(1.1f),static_cast<double>(.6666667f),true,42,"shoot"}};
}
bool IsFusionWater268Kind(uint16_t k)
{return k==15||k==1049||k==1050||k==1051||k==1056||k==1066||k==1156||k==252||k==1067||k==1068||k==1069||k==1076||k==1083||k==1175||k==1085;}
bool IsFusionWater268WaterPlant(uint16_t k)
{return k==15||k==1049||k==1050||k==1051||k==1056||k==1175||k==1066||k==1156||IsFusionWater268HomingPlant(k)||k==1076||k==1083;}
bool FusionWater268StarTarget(int status,bool mc,bool collider,bool alive)
{return status==10&&!mc&&collider&&alive;}
bool FusionWater268StarTrackingDue(uint8_t way,int age)
{return age>30&&way!=6&&way!=10;}
bool IsFusionWater268Kelp(uint16_t k)
{return k==15||k==1049||k==1051||k==1056||k==1066||k==1156;}
bool IsFusionWater268HomingPlant(uint16_t k){return k==252||k==1067||k==1068||k==1069;}
bool FusionWater268HomingKind(uint16_t k){return k==20||k==37||k==34||k==35;}
bool FusionWater268HomingTarget(int s,bool mc,bool collider,bool alive,bool retain)
{
    if(s<0||s>24||mc||!collider||!alive)return false;
    // Bullet.TrackUpdate retains10/20, although ordinary nearest selection
    // excludes them. A separate source flying search supplies air targets.
    if(s==1||s==7||s==12||s==16||(s>=21&&s<=24))return false;
    return retain||(s!=3&&s!=9&&s!=10&&s!=20);
}
bool FusionWater268GrabTarget(int s,int rowDelta,bool mc,bool beforeDying)
{return rowDelta==0&&!mc&&!beforeDying&&s>=0&&s<=24&&s!=1&&s!=3&&s!=9&&s!=10&&s!=12&&s!=16&&s<20;}
bool TryGetFusionWater268HomingStep(double dx,double dy,double py,double speed,float& vx,float& vy)
{
    if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(py)||py<=0||!std::isfinite(speed)||speed<=0)return false;
    // Normalize in source Unity units, then project each native screen axis.
    const double x=dx/kFusionBasePixelsX,y=dy/py,length=std::sqrt(x*x+y*y);
    const float nx=length?static_cast<float>(x/length*speed*kFusionBasePixelsX*.01):0;
    const float ny=length?static_cast<float>(y/length*speed*py*.01):0;
    if(!std::isfinite(nx)||!std::isfinite(ny))return false;vx=nx;vy=ny;return true;
}
bool TryGetFusionWater268GrabTicks(float speed,int& ticks)
{if(!std::isfinite(speed)||speed<.9f||speed>1.1f)return false;ticks=int(std::ceil(static_cast<double>(.33333334f)/speed*100));return true;}
bool FusionWater268PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode)
{
    if(!IsFusionWater268Kind(kind)||rage)return false;
    if(IsFusionWater268HomingPlant(kind)||kind==1085)return !bag&&!mode;
    if(kind==1050)return !shots&&bag<3&&mode<=3;
    if(kind==15||kind==1049||kind==1156)return !shots&&!bag&&mode<=2;
    if(kind==1051||kind==1066)return !bag&&mode<=2;
    if(kind==1076||kind==1083)return bag<=1&&mode<=2;
    return false; // Other profiles remain gated until their full callbacks exist.
}
bool FusionWater268HomingMetadataValid(uint16_t k,int damage,uint8_t way)
{return way==6&&((k==20&&damage==40)||((k==37||k==34)&&damage==20)||(k==35&&damage==40));}
bool FusionWater268CaptureStateValid(uint16_t k,uint8_t mode,int grab,int sink,uint16_t id,uint32_t token,float speed)
{
    if(k!=15&&k!=1049&&k!=1156&&k!=1050&&k!=1051&&k!=1056&&k!=1066)return false;int total=0;
    if(!TryGetFusionWater268GrabTicks(speed,total)||mode>(k==1050?3:2)||grab<0||sink<0)return false;
    if(!mode)return !grab&&!sink&&!id&&!token;
    if((id==0)!=(token==0))return false;
    if(mode==3)return id&&token&&!grab&&!sink;
    return mode==1?grab>0&&grab<=total&&!sink:!grab&&sink<=kFusionWater268SinkTicks;
}
const FusionShooterTimingProfile* FindFusionWater268Timing(uint16_t k,bool grown,bool air)
{if(k==1076)return &timings[6+(grown?2:0)+(air?1:0)];if(k==1083)return &timings[grown?11:10];for(const auto& v:timings)if(v.kind==k)return &v;return nullptr;}
const FusionShooterTimingProfile* FindFusionWater268Timing(uint16_t k,const char* state)
{if(!state)return nullptr;for(const auto& v:timings)if(v.kind==k&&!std::strcmp(v.visualState,state))return &v;return nullptr;}
bool FusionWater268ProjectileMetadataValid(uint16_t k,int damage,uint8_t way)
{
    if(FusionWater268HomingMetadataValid(k,damage,way))return true;
    if(k==42)return damage==20&&(way==2||way==9||way==6);
    if(k==9)return damage==20&&way==0;
    if(k==29)return damage==20&&(way==0||way==4||way==5);
    if(k==32)return damage==40&&(way==0||way==4||way==5||way==255);
    if(k==37)return damage==20&&way==7;
    return k==41&&damage==20&&(way==2||way==9);
}
bool TryGetFusionWater268SkyAngle(int age,float& angle)
{if(age<0)return false;const double t=(std::max)(0,age-1)*.01;angle=float(.5235987901687622*(1-(std::min)(1.,t*.6/.5)));return true;}
bool TryAddFusionWater268Grap(int ticks,int count,int& nextTicks,int& nextCount,bool& percent,bool& full)
{if(ticks<0||ticks>(std::numeric_limits<int>::max)()-200||count<0||count==(std::numeric_limits<int>::max)())return false;nextTicks=ticks+200;nextCount=count+1;percent=count>5;full=count>=30;return true;}
bool TryGetFusionWater268SearchTicks(float speed,int& ticks)
{if(!std::isfinite(speed)||speed<.9f||speed>1.1f)return false;ticks=int(std::ceil(double(2.15f)/speed*100));return true;}
bool FusionWater268SearchEventDue(int remaining,float speed,bool crash)
{int total=0;if(!TryGetFusionWater268SearchTicks(speed,total)||remaining<0||remaining>total)return false;const int event=int(std::ceil(double(crash?.5833333f:.56666666f)/speed*100));return remaining==total-event;}
bool FusionWater268ProjectileCanHit(uint8_t way,int s,int rowDelta,bool mc,bool collider,bool alive)
{
    if(!collider||mc||!alive||s<0||s>24||s==3||s==9||s==12||s==13||s==16||s==20||s==21)return false;
    if(way==7)return s==10&&rowDelta==0;
    if(s==10||(s==7&&way!=255))return false;
    return way==2||rowDelta==0;
}
namespace
{
bool WaterBox(uint16_t k,double& ox,double& oy,double& hw,double& hh)
{
    double scale=0,w=0,h=0;ox=oy=0;
    switch(k)
    {
    case 20:scale=.15;ox=-1.3386801;oy=-.011206567;w=2.6561954;h=1.7804455;break;
    case 29:scale=.828495;ox=.015661418;oy=-.0071810186;w=.511238;h=.5101826;break;
    case 32:scale=.061242823;ox=.04242277;oy=2.6244638;w=5.310795;h=17.018724;break;
    case 34:scale=1.7;ox=-.19405252;oy=.005925268;w=.4993149;h=.15268683;break;
    case 35:scale=1.7;ox=-.19679368;w=.41355503;h=.16953081;break;
    case 37:scale=1.7;ox=-.015957542;oy=-4.4703484e-8;w=.26838857;h=.16374153;break;
    case 42:
    case 41:scale=1.7;w=.32703882;h=.31575292;break;
    default:return false;
    }
    ox*=scale;oy*=scale;hw=w*scale*.5;hh=h*scale*.5;return true;
}
}
bool TryMakeFusionWater268ProjectileRect(uint16_t k,double x,double y,double a,double py,FusionRecipientRect& out)
{
    double ox=0,oy=0,hw=0,hh=0;if(!std::isfinite(x+y+a+py)||py<=0||!WaterBox(k,ox,oy,hw,hh))return false;
    const double c=std::cos(a),s=std::sin(a),w=(std::abs(c)*hw+std::abs(s)*hh)*kFusionBasePixelsX,h=(std::abs(s)*hw+std::abs(c)*hh)*py;
    out={x+(c*ox-s*oy)*kFusionBasePixelsX-w,y-(s*ox+c*oy)*py-h,2*w,2*h};return true;
}
bool FusionWater268ProjectileTouchesRect(uint16_t k,double x,double y,double a,double py,const FusionRecipientRect& r)
{
    double ox=0,oy=0,hw=0,hh=0;if(!std::isfinite(x+y+a+py+r.x+r.y+r.width+r.height)||py<=0||r.width<=0||r.height<=0||!WaterBox(k,ox,oy,hw,hh))return false;
    const double c=std::cos(a),s=std::sin(a),dx=(r.x+r.width*.5-x)/kFusionBasePixelsX-(c*ox-s*oy),dy=-(r.y+r.height*.5-y)/py-(s*ox+c*oy);
    const double rx=r.width*.5/kFusionBasePixelsX,ry=r.height*.5/py;
    return std::abs(dx)<=rx+std::abs(c)*hw+std::abs(s)*hh&&std::abs(dy)<=ry+std::abs(s)*hw+std::abs(c)*hh&&
        std::abs(dx*c+dy*s)<=hw+rx*std::abs(c)+ry*std::abs(s)&&std::abs(-dx*s+dy*c)<=hh+rx*std::abs(s)+ry*std::abs(c);
}
}
