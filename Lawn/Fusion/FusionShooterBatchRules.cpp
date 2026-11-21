#include "FusionShooterBatchRules.h"
#include "FusionBasePlantRules.h"
#include "FusionShooter267Rules.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <initializer_list>

namespace LawnFusion
{
namespace
{
constexpr double pea[] = {static_cast<double>(.6666667f)};
constexpr double puff[] = {static_cast<double>(.38333333f)};
constexpr double star[] = {static_cast<double>(.41666666f)};
constexpr double starPuff[] = {static_cast<double>(.25f)};
constexpr double split[] = {static_cast<double>(.6666667f), 1.0};
constexpr double sniper[] = {static_cast<double>(.75f)};
constexpr double gatling[] = {static_cast<double>(.33333334f),.5,static_cast<double>(.6666667f),
    static_cast<double>(.8333333f),1.0,static_cast<double>(1.1666666f)};
constexpr double loop[] = {static_cast<double>(.083333336f)};
constexpr double sunPuff[] = {.5,static_cast<double>(.7f),static_cast<double>(.9f),static_cast<double>(1.1f)};
constexpr double cactus[] = {static_cast<double>(.31666666f)};
constexpr double cherryRapid[]={static_cast<double>(.6666667f),1,static_cast<double>(1.3333334f),static_cast<double>(1.6666666f)};
constexpr double doomGatling[]={static_cast<double>(.33333334f),.5,static_cast<double>(.6666667f),static_cast<double>(.8333333f)};
constexpr double cactusAir[] = {static_cast<double>(.26666668f)};
constexpr FusionShooterTimingProfile profiles[] = {
    {1020,1.5f,pea,2,1,true,11},
    {1152,2.0f,puff,1,.5,true,90},
    {1096,1.5f,star,static_cast<double>(1.1f),static_cast<double>(.6666667f),true,46},
    {1183,2.25f,starPuff,1,.5,true,39},
    {929,1.5f,sunPuff,static_cast<double>(1.3f),1.25,true,95,"shoot_g"},
    {1107,1.5f,split,2,static_cast<double>(1.3333334f),true,15},
    {1109,3.0f,sniper,2,1,true,0xffff},
    {1167,3.0f,sniper,2,1,true,0xffff},
    {1161,.5f,gatling,1,1.25,false,0},
    {1161,.5f,loop,1,static_cast<double>(.16666667f),false,0,"shoot_loop"},
    {1165,1.5f,gatling,1,static_cast<double>(1.3333334f),true,15},
    {1168,1.5f,gatling,1,static_cast<double>(1.3333334f),true,0},
    {1191,1.5f,cactus,1,.5,true,102,"shoot1"},
    {1191,1.5f,cactusAir,1,static_cast<double>(.51666665f),true,102,"shoot2"},
    {1079,1.5f,cactus,1,.5,true,38,"shoot1"},
    {1081,3,star,static_cast<double>(1.1f),static_cast<double>(.6666667f),true,40},
    {1005,3,pea,2,1,true,3},
    {901,2,cherryRapid,2,static_cast<double>(1.8333334f),true,3},
    {907,1.5f,doomGatling,1,1,true,23},
    {933,1.5f,cactus,1,.5,true,100,"shoot1"}};
constexpr FusionShooterTimingProfile bigGatling[] = {
    {1161,.5f,gatling,1,1.25,false,0},
    {1161,.5f,loop,1,static_cast<double>(.16666667f),false,0,"shoot_loop"},
    {1161,.5f,gatling,1.5,1.25,false,0},
    {1161,.5f,loop,1.5,static_cast<double>(.16666667f),false,0,"shoot_loop"},
    {1161,.5f,gatling,2,1.25,false,0},
    {1161,.5f,loop,2,static_cast<double>(.16666667f),false,0,"shoot_loop"}};
bool RotatedCollider(uint16_t kind,double angle,double& c,double& s,
    double& hw,double& hh,double& ox,double& oy)
{
    if(!std::isfinite(angle)||(kind!=7&&kind!=46&&kind!=102&&kind!=40&&kind!=100))return false;
    c=std::cos(angle);s=std::sin(angle);ox=oy=0;
    if(kind==7){hw=1.37*.25500003*.5;hh=1.4*.25500003*.5;}
    else if(kind==46||kind==40){hw=.32703882*1.7*.5;hh=.31575292*1.7*.5;}
    else if(kind==100){hw=3.68*.2*.5;hh=1.7*.2*.5;}
    else
    {
        hw=.26838857*1.7*.5;hh=.16374153*1.7*.5;
        ox=-.015957542*1.7;oy=-4.4703484e-8*1.7;
    }
    return true;
}
}

bool IsFusionShooterBatchKind(uint16_t kind)
{
    if(kind==1192)return true;
    return FindFusionShooterBatchTimingProfile(kind)!=nullptr;
}
const FusionShooterTimingProfile* FindFusionShooterBatchTimingProfile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return FindFusionShooter267Timing(kind);}
const FusionShooterTimingProfile* FindFusionShooterBatchTimingProfile(uint16_t kind,const char* state)
{if(state)for(const auto& p:profiles)if(p.kind==kind&&!std::strcmp(p.visualState,state))return &p;return FindFusionShooter267Timing(kind,state);}
const FusionShooterTimingProfile* FindFusionBigGatlingTimingProfile(uint32_t count,bool looping)
{
    if(count>static_cast<uint32_t>((std::numeric_limits<int>::max)()))return nullptr;
    return &bigGatling[(count>300?4:count>100?2:0)+(looping?1:0)];
}
float FusionShooterBatchMaximumDuration(uint16_t kind,float speed)
{
    if(!std::isfinite(speed)||speed<.9f||speed>1.1f)return 0;
    float maximum=0;
    if(const auto* p=FindFusionShooter267Timing(kind))maximum=FusionShooterDuration(*p,speed);
    if(kind==927)maximum=(std::max)(maximum,2.67f);
    for(const auto& p:profiles)if(p.kind==kind)maximum=(std::max)(maximum,FusionShooterDuration(p,speed));
    if(kind==1161)for(uint8_t mode:{0,1,2,3,4,6})
    {int ticks=0;if(TryGetFusionBigGatlingPhaseTicks(mode,speed,0,ticks))maximum=(std::max)(maximum,ticks*.01f);}
    if(kind==1191||kind==933)maximum=(std::max)(maximum,float(std::ceil(static_cast<double>(1.8333334f)/(static_cast<double>(.6f)*speed)*100))*.01f);
    return maximum;
}
bool TryGetFusionBigGatlingPhaseTicks(uint8_t mode,float speed,uint32_t count,int& ticks)
{
    float velocity=0,rate=0;
    if(!std::isfinite(speed)||speed<.9f||speed>1.1f||!TryGetFusionGatlingMilestone(count,velocity,rate))return false;
    double seconds=0;
    // Actual controller fixed-duration transitions, not additional clocks.
    switch(mode)
    {
    case 0:seconds=static_cast<double>(.9109891f)*2.0/speed+static_cast<double>(.27999115f);break;
    case 1:seconds=static_cast<double>(.9318182f)*3.0/(static_cast<double>(1.2f)*speed)+.25;break;
    case 2:seconds=static_cast<double>(.9664294f)*1.25/rate+static_cast<double>(.044760704f);break;
    case 3:seconds=static_cast<double>(.16666667f)/rate;break;
    case 4:seconds=.75*.5+.25;break;
    case 6:seconds=static_cast<double>(.7f)*static_cast<double>(.8333333f)/speed+.25;break;
    default:return false;
    }
    ticks=static_cast<int>(std::ceil(seconds*100));return true;
}
bool TryRephaseFusionBigGatling(uint8_t mode,float speed,uint32_t count,
    std::size_t event,int& shootingCounter,int& stateCountdown,float& elapsedSeconds)
{
    if(mode!=2&&mode!=3)return false;
    const auto* profile=FindFusionBigGatlingTimingProfile(count,mode==3);
    int initial=0,total=0;
    if(!profile||event>=profile->eventClipSeconds.size()||
        !TryGetFusionShooterShotCounter(*profile,speed,initial)||
        !TryGetFusionBigGatlingPhaseTicks(mode,speed,count,total))return false;
    const int elapsed=static_cast<int>(std::ceil(profile->eventClipSeconds[event]/profile->stateSpeed*100));
    shootingCounter=initial-elapsed;
    stateCountdown=(std::max)(0,total-elapsed);
    elapsedSeconds=elapsed*.01f;
    return true;
}
bool TryGetFusionDoomCactusAngle(float seconds,float& radians)
{
    if(!std::isfinite(seconds)||seconds<0)return false;
    // UltimateCactus.Update 337CC0: two F32 pi products, F32 sum,
    // Sin rounded to F32, then degrees and Deg2Rad in the source order.
    const float halfPhase=seconds*3.1415927f;
    const float phase=halfPhase+halfPhase;
    if(!std::isfinite(phase))return false;
    const float sine=std::sin(phase);
    const float degrees=sine*45.f;
    radians=degrees*.017453292f;
    return true;
}

bool FusionShooterBatchStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode)
{
    if(IsFusionShooter267Kind(kind))return FusionShooter267StateValid(kind,shots,rage,bag,mode);
    if(kind==1109||kind==1167)return shots<6&&!rage&&!bag&&!mode;
    // Native visual states:0 startup idle,1 elevating,2 shooting,3 loop,
    //4 rest,5 raised idle,6 armed. Body raises without an enemy condition.
    if(kind==1161)return shots<=static_cast<uint32_t>((std::numeric_limits<int>::max)())&&!rage&&!bag&&mode<=6;
    if(kind==1165||kind==1168)return !shots&&!bag&&rage<=500&&mode<=1&&bool(rage)==bool(mode);
    // Mode1 is the source superskill clip, whose countdown remains native.
    if(kind==1191)return !shots&&!rage&&bag<=20&&mode<=1;
    if(kind==1011)return !shots&&!bag&&rage<=223&&mode<=1&&bool(rage)==bool(mode);
    if(kind==933||kind==1135)return !shots&&!rage&&!bag&&mode<=1;
    if(kind==907)return shots<16&&!rage&&!bag&&!mode;
    return !shots&&!rage&&!bag&&!mode;
}

bool TryGetFusionShooterBatchCycleTicks(uint16_t kind,float jitter,int& ticks)
{
    const auto* p=FindFusionShooterBatchTimingProfile(kind);
    if(!p||!std::isfinite(jitter))return false;
    if(kind==1109||kind==1167||kind==1191||kind==1079||kind==1081||kind==933)
    {
        if(jitter<-.1f||jitter>.1f)return false;
        const float seconds=p->attackIntervalSeconds+jitter;
        ticks=static_cast<int>(std::floor(static_cast<double>(seconds)*100.0))+1;
        return true;
    }
    return TryGetFusionShooterCycleTicks(*p,jitter,ticks);
}
bool TryAdvanceFusionSniperShot(uint16_t kind,uint32_t previous,uint32_t& next,int& damage)
{
    if((kind!=1109&&kind!=1167)||previous>=6)return false;
    const uint32_t value=(previous+1)%6;
    next=value;damage=value?kind==1109?500:300:1000000;return true;
}
bool TryDrawFusionDoomCactusBag(uint16_t remaining,int draw,uint16_t& next,bool& special)
{
    if(remaining>20||draw<0||draw>remaining)return false;
    special=draw==remaining;next=special?20:static_cast<uint16_t>(remaining-1);return true;
}
bool TryGetFusionGatlingMilestone(uint32_t callbacks,float& speed,float& rate)
{
    if(callbacks>static_cast<uint32_t>((std::numeric_limits<int>::max)()))return false;
    speed=callbacks>300?12.f:callbacks>100?9.f:6.f;
    rate=callbacks>300?2.f:callbacks>100?1.5f:1.f;return true;
}
bool FusionShooterBatchInvulnerable(uint16_t kind,int remaining)
{return (kind==1165||kind==1168)&&remaining>0&&remaining<=500;}
bool FusionSniperCanAcquire(int status,bool controlled,bool dying,double px,double zx,double vision)
{
    return !controlled&&!dying&&status!=1&&status!=7&&status!=12&&
        !(status>=20&&status<=24)&&std::isfinite(px)&&std::isfinite(zx)&&std::isfinite(vision)&&zx>px&&zx<vision;
}
bool TryGetFusionIronDamage(int base,int armor,int& damage)
{
    if(base<0||armor<0||base>(std::numeric_limits<int>::max)()-armor)return false;
    damage=base+armor;return true;
}
bool FusionShooterBatchProjectileMetadataValid(uint16_t kind,int damage,int sun,uint8_t way,
    ProjectileType type,ProjectileMotion motion)
{
    if(sun)return false;
    if(kind==3)return damage==300&&!way&&type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT;
    if(kind==23)return (damage==300||damage==1800)&&!way&&type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT;
    if(kind==38)return damage==20&&!way&&type==PROJECTILE_SPIKE&&motion==MOTION_STRAIGHT;
    if(kind==39)return damage==40&&(way==2||way==9)&&type==PROJECTILE_STAR&&motion==MOTION_STAR;
    if(kind==40)return damage==40&&(way==2||way==9||way==6)&&type==PROJECTILE_STAR&&motion==MOTION_STAR;
    if(kind==100)return damage==300&&(way==0||way==18)&&type==PROJECTILE_SPIKE&&motion==(way==18?MOTION_STAR:MOTION_STRAIGHT);
    if(kind==11||kind==90)return damage==80&&!way&&type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT;
    if(kind==46)return damage==40&&(way==2||way==9)&&type==PROJECTILE_STAR&&motion==MOTION_STAR;
    if(kind==92)return (damage==20||damage==60||damage==600)&&way==3&&type==PROJECTILE_PEA&&motion==MOTION_PUFF;
    if(kind==95)return damage==20&&!way&&type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT;
    if(kind==102)return damage==40&&(way==0||way==8||way==18)&&type==PROJECTILE_SPIKE&&motion==(way==18?MOTION_STAR:MOTION_STRAIGHT);
    if(kind==0&&damage==20&&way>=16&&way<=18&&type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT)return true;
    return (kind==0||kind==15||kind==97)&&damage==20&&way==15&&type==PROJECTILE_PEA&&motion==MOTION_STAR;
}
bool TryMakeFusionShooterBatchProjectileRect(uint16_t kind,double x,double y,FusionRecipientRect& rect)
{
    if(!std::isfinite(x)||!std::isfinite(y))return false;
    // Full prefab collider sizes/offsets; scale is source projection72/1.35,100/1.67.
    double w=0,h=0,ox=0,oy=0;
    switch(kind)
    {
    case 11:w=1.1409948*.4;h=1.3134866*.4;ox=-.012087107*.4;oy=-.07117817*.4;break;
    case 90:w=.95918655*.27522;h=.93252146*.27522;ox=.0007451102*.27522;oy=-.051929936*.27522;break;
    case 92:w=.32;h=.43096197;oy=-.08048099;break;
    case 95:w=3.5532758*.10968053;h=1.4299067*.10968053;ox=-.036273837*.10968053;oy=-.07308346*.10968053;break;
    case 3:w=2.13*.25;h=3.013503*.25;oy=-.56675196*.25;break;
    case 23:w=3.5332565*.124992006;h=3.6578424*.124992006;ox=-.14535189*.124992006;oy=1.7857376*.124992006;break;
    case 38:w=.28635082*1.7;h=.16374153*1.7;ox=-.114749774*1.7;oy=-4.4703484e-8*1.7;break;
    case 102:w=.26838857*1.7;h=.16374153*1.7;ox=-.015957542*1.7;oy=-4.4703484e-8*1.7;break;
    default:return false;
    }
    const double sx=72.0/1.35,sy=100.0/1.67;
    rect={x+(ox-w*.5)*sx,y+(-oy-h*.5)*sy,w*sx,h*sy};return true;
}
bool TryMakeFusionShooterBatchRotatedRect(uint16_t kind,double x,double y,double angle,FusionRecipientRect& rect)
{
    double c,s,hw,hh,ox,oy;
    if(!std::isfinite(x)||!std::isfinite(y)||!RotatedCollider(kind,angle,c,s,hw,hh,ox,oy))return false;
    const double cx=x+(c*ox-s*oy)*kFusionBasePixelsX,cy=y-(s*ox+c*oy)*kFusionBasePixelsY;
    const double ex=(std::abs(c)*hw+std::abs(s)*hh)*kFusionBasePixelsX;
    const double ey=(std::abs(s)*hw+std::abs(c)*hh)*kFusionBasePixelsY;
    rect={cx-ex,cy-ey,2*ex,2*ey};return true;
}
bool FusionShooterBatchRotatedProjectileTouchesRect(uint16_t kind,double x,double y,
    double angle,const FusionRecipientRect& target)
{
    double c,s,hw,hh,ox,oy;
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(target.x+target.y+target.width+target.height)||
        target.width<=0||target.height<=0||!RotatedCollider(kind,angle,c,s,hw,hh,ox,oy))return false;
    // SAT is evaluated before the anisotropic native projection. This retains
    // the source rotated box rather than admitting the corners of its AABB.
    const double dx=(target.x+target.width*.5-x)/kFusionBasePixelsX-(c*ox-s*oy);
    const double dy=-(target.y+target.height*.5-y)/kFusionBasePixelsY-(s*ox+c*oy);
    const double rx=target.width*.5/kFusionBasePixelsX,ry=target.height*.5/kFusionBasePixelsY;
    return std::abs(dx)<=rx+std::abs(c)*hw+std::abs(s)*hh &&
        std::abs(dy)<=ry+std::abs(s)*hw+std::abs(c)*hh &&
        std::abs(dx*c+dy*s)<=hw+rx*std::abs(c)+ry*std::abs(s) &&
        std::abs(-dx*s+dy*c)<=hh+rx*std::abs(s)+ry*std::abs(c);
}
}
