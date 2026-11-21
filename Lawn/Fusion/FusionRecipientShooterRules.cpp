#include "FusionRecipientShooterRules.h"
#include "FusionBasePlantRules.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace LawnFusion
{
namespace
{
constexpr double cactusEvent[] = {static_cast<double>(.31666666f)};
constexpr double cactusUpperEvent[] = {static_cast<double>(.26666668f)};
constexpr double starEvent[] = {static_cast<double>(.41666666f)};
const FusionShooterTimingProfile timings[] = {
    {21,1.5f,cactusEvent,1,.5,true,37,"shoot1"},
    {21,1.5f,cactusUpperEvent,1,static_cast<double>(.51666665f),true,37,"shoot2"},
    {23,1.5f,starEvent,static_cast<double>(1.1f),static_cast<double>(.6666667f),true,39,"shoot"}
};
bool RectValid(const FusionRecipientRect& r)
{ return std::isfinite(r.x+r.y+r.width+r.height) && r.width>0 && r.height>0; }
bool Collider(uint16_t kind,double vx,double vy,double& c,double& s,double& hw,double& hh,double& ox,double& oy)
{
    if (!std::isfinite(vx+vy) || (kind!=37 && kind!=39)) return false;
    c=1;s=0;ox=oy=0;
    if (kind==37) { hw=.26838857*1.7*.5;hh=.16374153*1.7*.5;ox=-.015957542*1.7;oy=-4.4703484e-8*1.7; }
    else
    {
        hw=.32703882*1.7*.5;hh=.31575292*1.7*.5;
        const double x=vx/kFusionBasePixelsX,y=-vy/kFusionBasePixelsY,length=std::hypot(x,y);
        if (!(length>0)) return false;
        c=x/length;s=y/length; // Rotation of the root collider, not the spinning child sprite.
    }
    return true;
}
}
const FusionShooterTimingProfile* FindFusionRecipientTimingProfile(uint16_t kind,bool airborne)
{ return kind==21?&timings[airborne?1:0]:kind==23&&!airborne?&timings[2]:nullptr; }
const FusionShooterTimingProfile* FindFusionRecipientTimingProfile(uint16_t kind,const char* state)
{
    if (!state) return nullptr;
    for (const auto& p:timings) if (p.kind==kind && std::strcmp(p.visualState,state)==0) return &p;
    return nullptr;
}
bool TryGetFusionCactusCycleTicks(float jitter,int& ticks)
{
    if (!std::isfinite(jitter) || jitter<-.1f || jitter>.1f) return false;
    const float seconds=1.5f+jitter;
    ticks=static_cast<int>(std::floor(static_cast<double>(seconds)*100))+1; // Source strict<0.
    return true;
}
std::size_t FusionRecipientEmissionCount(uint16_t kind) { return kind==21?1:kind==23?5:0; }
bool TryGetFusionRecipientEmission(uint16_t kind,std::size_t index,FusionRecipientEmissionSpec& result)
{
    if (index>=FusionRecipientEmissionCount(kind)) return false;
    // Cactus shoot1 pins Shoot at(2.93,2.23) throughout the actual clip;
    // using its resting(1.96,2.31) node would put the projectile behind the mouth.
    FusionRecipientEmissionSpec candidate;
    if (kind==21) candidate={37,0,(2.93-.21999972)*.33,(2.23-.39999995)*.35,6*kFusionBasePixelsX*.01,0};
    else
    {
        constexpr double muzzles[][2]={{(-.42999983-.02)*.32,(3.5099998-1)*.32},
            {(-.43-.02)*.32,(.13999993-1)*.32},{(-1.75-.02)*.32,(2.18-1)*.32},
            {(1.7499998-.02)*.32,(2.91-1)*.32},{(1.75-.02)*.32,(1.0100001-1)*.32}};
        constexpr double cx[]={0,0,-1,.8660254037844386,.8660254037844386};
        constexpr double sy[]={1,-1,0,.5,-.5};
        candidate={39,index==2?9:2,muzzles[index][0],muzzles[index][1],
            6*cx[index]*kFusionBasePixelsX*.01,-6*sy[index]*kFusionBasePixelsY*.01};
    }
    result=candidate;return true;
}
bool FusionCactusCanAcquire(int status,bool sameRow,bool controlled,bool dying,double px,double zx,double vision)
{ return status==0 && sameRow && !controlled && !dying && std::isfinite(px+zx+vision) && zx>px && zx<vision; }
bool TryGetFusionStarAcquisitionDistance(std::size_t ray,bool sameRow,double x,double y,const FusionRecipientRect& rect,double& distance)
{
    if (ray>=5 || (ray==4&&!sameRow) || !std::isfinite(x+y) || !RectValid(rect)) return false;
    constexpr double dx[]={2,2,0,0,-1},dy[]={-1,1,-1,1,0}; // Board Y is down.
    const double length=std::hypot(dx[ray],dy[ray]);
    const double vx=dx[ray]/length*kFusionBasePixelsX,vy=dy[ray]/length*kFusionBasePixelsY;
    double entryDistance=0,exitDistance=2147483648.0; // Source RaycastAll distance0x4f000000.
    const double origins[]={x,y},velocities[]={vx,vy},lo[]={rect.x,rect.y},hi[]={rect.x+rect.width,rect.y+rect.height};
    for (int axis=0;axis<2;++axis)
    {
        if (velocities[axis]==0) { if(origins[axis]<lo[axis]||origins[axis]>hi[axis])return false;continue; }
        double a=(lo[axis]-origins[axis])/velocities[axis],b=(hi[axis]-origins[axis])/velocities[axis];
        if(a>b)std::swap(a,b);entryDistance=(std::max)(entryDistance,a);exitDistance=(std::min)(exitDistance,b);if(entryDistance>exitDistance)return false;
    }
    distance=entryDistance;return true;
}
bool FusionRecipientProjectileMetadataValid(uint16_t kind,int damage,int sun,uint8_t way,ProjectileType type,ProjectileMotion motion)
{ return damage==20 && !sun && ((kind==37&&(way==0||way==8)&&type==PROJECTILE_SPIKE&&motion==MOTION_STRAIGHT)||
    (kind==39&&(way==2||way==9)&&type==PROJECTILE_STAR&&motion==MOTION_STAR)); }
bool FusionRecipientProjectileVelocityValid(uint16_t kind,uint8_t way,double vx,double vy)
{
    if (!std::isfinite(vx+vy)) return false;
    if (kind==37) return (way==0||way==8)&&std::abs(vx-6*kFusionBasePixelsX*.01)<1e-5&&std::abs(vy)<1e-5;
    if (kind!=39) return false;
    for (std::size_t i=0;i<5;++i)
    { FusionRecipientEmissionSpec e;TryGetFusionRecipientEmission(23,i,e);
        if(e.sourceMovingWay==way&&std::abs(vx-e.velocityX)<1e-5&&std::abs(vy-e.velocityY)<1e-5)return true; }
    return false;
}
bool FusionRecipientProjectileCanHit(uint16_t kind,uint8_t way,int status,bool sameRow,bool controlled,bool dying,double hp)
{
    return status==0 && !controlled && !dying && std::isfinite(hp) && hp>0 &&
        ((kind==37&&way==0&&sameRow)||(kind==39&&(way==2||(way==9&&sameRow))));
}
unsigned int FusionRecipientDamageFlags(uint16_t kind,uint8_t way)
{ return kind==39&&way==9?1U:0U; } // Star.HitZombie4337F0 way9=Shieldless4; other ways=Normal0.
bool TryMakeFusionRecipientProjectileRect(uint16_t kind,double x,double y,double vx,double vy,FusionRecipientRect& result)
{
    double c,s,hw,hh,ox,oy;if(!std::isfinite(x+y)||!Collider(kind,vx,vy,c,s,hw,hh,ox,oy))return false;
    const double cx=x+(c*ox-s*oy)*kFusionBasePixelsX,cy=y-(s*ox+c*oy)*kFusionBasePixelsY;
    const double ex=(std::abs(c)*hw+std::abs(s)*hh)*kFusionBasePixelsX,ey=(std::abs(s)*hw+std::abs(c)*hh)*kFusionBasePixelsY;
    result={cx-ex,cy-ey,2*ex,2*ey};return true;
}
bool FusionRecipientProjectileTouchesRect(uint16_t kind,double x,double y,double vx,double vy,const FusionRecipientRect& rect)
{
    double c,s,hw,hh,ox,oy;if(!std::isfinite(x+y)||!RectValid(rect)||!Collider(kind,vx,vy,c,s,hw,hh,ox,oy))return false;
    // Compare in source units: anisotropic native projection otherwise turns a
    // rotated box into a parallelogram. The four SAT axes retain the actual box.
    const double dx=(rect.x+rect.width*.5-x)/kFusionBasePixelsX-(c*ox-s*oy);
    const double dy=-(rect.y+rect.height*.5-y)/kFusionBasePixelsY-(s*ox+c*oy);
    const double rx=rect.width*.5/kFusionBasePixelsX,ry=rect.height*.5/kFusionBasePixelsY;
    return std::abs(dx)<=rx+std::abs(c)*hw+std::abs(s)*hh &&
        std::abs(dy)<=ry+std::abs(s)*hw+std::abs(c)*hh &&
        std::abs(dx*c+dy*s)<=hw+rx*std::abs(c)+ry*std::abs(s) &&
        std::abs(-dx*s+dy*c)<=hh+rx*std::abs(s)+ry*std::abs(c);
}
}
