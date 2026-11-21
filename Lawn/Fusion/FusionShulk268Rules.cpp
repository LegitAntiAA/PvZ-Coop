#include "FusionShulk268Rules.h"
#include "FusionBasePlantRules.h"
#include <cmath>
namespace LawnFusion
{
namespace
{
constexpr double events[]={double(.6666667f)};
constexpr FusionShooterTimingProfile twin{1177,1.5f,events,1,1,false,94,"shoot2"};
bool Box(uint16_t k,double& ox,double& oy,double& hw,double& hh)
{if(k==89){ox=-.003484249*.276256;oy=-.07547948*.276256;hw=2.2239718*.276256*.5;hh=1.7006075*.276256*.5;return true;}
if(k==94){ox=oy=0;hw=.7768318*.5;hh=.7768319*.5;return true;}return false;}
}
bool IsFusionShulk268Kind(uint16_t k){return k==33||k==1177;}
bool FusionShulk268Target(int s,int row,bool mc,bool collider,bool alive)
{return !row&&!mc&&collider&&alive&&s>=0&&s<=24&&s!=1&&s!=3&&s!=7&&s!=9&&s!=10&&s!=12&&s!=16&&s<20;}
const FusionShooterTimingProfile* FindFusionShulk268Timing(uint16_t k){return k==1177?&twin:nullptr;}
bool TryGetFusionShulk268PhaseTicks(uint16_t k,uint8_t mode,int& ticks)
{
    if(!IsFusionShulk268Kind(k)||mode>4)return false;
    const double seconds=mode==1?double(.6666667f)*double(k==33?.93912727f:.9516668f):mode==3?double(k==33?.25f:.23648873f):mode==4?.25:0;
    ticks=int(std::ceil(seconds*100));return true;
}
bool TryStepFusionShulk268Steering(double cx,double cy,double tx,double ty,bool target,float& vx,float& vy)
{
    if(!std::isfinite(cx+cy+tx+ty))return false;
    const double cl=std::hypot(cx,cy),tl=std::hypot(tx,ty),a=target?.1:.01;
    double x=(cl?cx/cl:0)*(1-a)+(tl?tx/tl:0)*a,y=(cl?cy/cl:0)*(1-a)+(tl?ty/tl:0)*a;
    const double length=std::hypot(x,y);vx=length?float(x/length*8):0;vy=length?float(y/length*8):0;return true;
}
bool FusionShulk268StateValid(uint16_t k,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode)
{return IsFusionShulk268Kind(k)&&!rage&&bag<=1&&mode<=4&&(k==1177||!shots);}
bool FusionShulk268ClockStateValid(uint16_t k,int search,int phase,float speed,float seconds,uint8_t mode)
{
    int maximum=0;if(!TryGetFusionShulk268PhaseTicks(k,mode,maximum)||search<0||search>51||phase<0||phase>maximum||!std::isfinite(speed)||speed<.9f||speed>1.1f||!std::isfinite(seconds))return false;
    return k==33?seconds==-1:seconds==-1||(seconds>=0&&seconds<=1.f);
}
bool FusionChomper268ProjectileMetadataValid(uint16_t k,int damage,uint8_t way)
{return k==89?damage==300&&way==0:k==94&&damage==10&&(way==16||way==2);}
bool TryMakeFusionChomper268ProjectileRect(uint16_t k,double x,double y,double a,double py,FusionRecipientRect& out)
{
    double ox=0,oy=0,hw=0,hh=0;if(!std::isfinite(x+y+a+py)||py<=0||!Box(k,ox,oy,hw,hh))return false;
    const double c=std::cos(a),s=std::sin(a),w=(std::abs(c)*hw+std::abs(s)*hh)*kFusionBasePixelsX,h=(std::abs(s)*hw+std::abs(c)*hh)*py;
    out={x+(c*ox-s*oy)*kFusionBasePixelsX-w,y-(s*ox+c*oy)*py-h,2*w,2*h};return true;
}
bool FusionChomper268ProjectileTouchesRect(uint16_t k,double x,double y,double a,double py,const FusionRecipientRect& r)
{
    double ox=0,oy=0,hw=0,hh=0;if(!std::isfinite(x+y+a+py+r.x+r.y+r.width+r.height)||py<=0||r.width<=0||r.height<=0||!Box(k,ox,oy,hw,hh))return false;
    const double c=std::cos(a),s=std::sin(a),dx=(r.x+r.width*.5-x)/kFusionBasePixelsX-(c*ox-s*oy),dy=-(r.y+r.height*.5-y)/py-(s*ox+c*oy),rx=r.width*.5/kFusionBasePixelsX,ry=r.height*.5/py;
    return std::abs(dx)<=rx+std::abs(c)*hw+std::abs(s)*hh&&std::abs(dy)<=ry+std::abs(s)*hw+std::abs(c)*hh&&std::abs(dx*c+dy*s)<=hw+rx*std::abs(c)+ry*std::abs(s)&&std::abs(-dx*s+dy*c)<=hh+rx*std::abs(s)+ry*std::abs(c);
}
}
