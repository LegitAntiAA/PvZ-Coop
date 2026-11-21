#include "FusionCannon268Rules.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr FusionCannon268Profile profiles[]={
    {1120,65,300,1800,300,50,36,true},{1122,68,300,3600,425,50,36,true},
    {1123,69,300,1800,375,50,36,true},{1132,73,1000,200,750,50,36,true},
    {1139,77,300,60,225,7.5f,3,false},{1142,82,300,120,525,7.5f,3,false},
    {914,72,300,120,900,90,2,false},{915,84,300,1800,575,90,36,true}};
constexpr FusionLobberProfile lobbers[]={
    {1139,77,PROJECTILE_KERNEL,60,.2211,1.6401},
    {1139,79,PROJECTILE_BUTTER,80,.2211,1.6401,true},
    {1142,82,PROJECTILE_MELON,120,.2013,1.6071},
    {914,72,PROJECTILE_MELON,120,.30432,1.7664},
    {1139,80,PROJECTILE_MELON,450,.2211,1.6401,true}};
constexpr FusionLobberProjectileTraits lobberTraits[]={
    {77,1,FusionLobberBounce::None,false,false,false,false,true,1.2586598,.51232433,-.012087107,-.022978932},
    {79,1,FusionLobberBounce::None,false,false,true,false,true,2.0175073,1.3134866,-.012087107,-.07117817,39},
    {82,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {72,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {80,1,FusionLobberBounce::None,false,false,false,false,false,1,1,0,0}};
constexpr double ordinaryEvent[]={static_cast<double>(.33333334f)},ultimateEvent[]={static_cast<double>(.4f)};
constexpr double moneyCornEvents[]={.65,.8,.95,1.1},moneyMelonEvents[]={.6333333,1,1.25,1.5166667};
constexpr double subEvents[]={.5,.8333333};
const FusionShooterTimingProfile lobberClocks[]={
    {1139,3,ordinaryEvent,1,1,true,77,"shoot"},{1139,3,ordinaryEvent,1,1,true,79,"shoot2"},
    {1139,3,moneyCornEvents,1,1.35,true,80,"shoot3"},
    {1142,3,ordinaryEvent,1,1,true,82,"shoot"},{1142,3,moneyMelonEvents,1,2,false,83,"shoot2"},
    {914,2,ultimateEvent,1,static_cast<double>(1.1166667f),true,72,"shoot"}};
const FusionShooterTimingProfile subClock={915,1.5f,subEvents,1,static_cast<double>(1.1666666f),false,85,"subshoot"};
constexpr double oneShot[]={1.7333333};
constexpr double melonShots[]={1.5666667,2.0333333,2.3,2.5666666};
constexpr double chargeReady[]={1.3};
const FusionShooterTimingProfile shots[]={
    {1120,36,oneShot,1,3,true,65,"shooting"},{1122,36,oneShot,1,3,true,68,"shooting"},
    {1123,36,oneShot,1,3,true,69,"shooting"},{1132,36,melonShots,1,3.75,true,73,"shooting"},
    {915,36,oneShot,1,3,true,84,"shooting"}};
const FusionShooterTimingProfile charges[]={
    {1120,36,chargeReady,1.2,2,true,0xffff,"charge"},{1122,36,chargeReady,1.2,2,true,0xffff,"charge"},
    {1123,36,chargeReady,1.2,2,true,0xffff,"charge"},{915,36,chargeReady,1.2,2,true,0xffff,"charge"}};
}
const FusionCannon268Profile* FindFusionCannon268Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
const FusionShooterTimingProfile* FindFusionCannon268Timing(uint16_t kind,bool charge)
{
    if(charge){for(const auto& p:charges)if(p.kind==kind)return &p;}
    else for(const auto& p:shots)if(p.kind==kind)return &p;
    return nullptr;
}
bool TryGetFusionCannon268ShotCounter(const FusionShooterTimingProfile& p,float speed,int& counter)
{
    int generic=0;if(!TryGetFusionShooterShotCounter(p,speed,generic))return false;
    const double playback=p.stateSpeed*(p.speedParameterActive?speed:1);
    const double result=std::ceil(p.clipDurationSeconds/playback*100)+1;
    if(!std::isfinite(result)||result>(std::numeric_limits<int>::max)())return false;
    counter=static_cast<int>(result);return true;
}
bool TryGetFusionCannon268EventAtCounter(const FusionShooterTimingProfile& p,float speed,int remaining,size_t event,bool& due)
{
    int full=0,generic=0;
    if(event>=p.eventClipSeconds.size()||!TryGetFusionCannon268ShotCounter(p,speed,full)||
        !TryGetFusionShooterShotCounter(p,speed,generic)||remaining<0||remaining>=full)return false;
    const int shifted=remaining-(full-generic);
    if(shifted<0){due=false;return true;}
    return TryGetFusionShooterEventAtCounter(p,speed,shifted,event,due);
}
bool FusionCannon268ProjectileMetadataValid(uint16_t kind,int damage,int sun,uint8_t way,uint8_t variant,bool enhanced,ProjectileType type,ProjectileMotion motion)
{
    if(sun)return false;
    if(IsFusionCannon268Way14(kind))return way==14&&type==PROJECTILE_COBBIG&&motion==MOTION_LOBBED&&!enhanced&&
        (kind==65||kind==69||kind==84?damage==1800&&variant==0:
         kind==68?damage==3600&&variant==0:kind==73?damage==200&&variant<=1:damage==120&&(variant==0||variant==2));
    if(kind==85)return damage==360&&way==0&&variant==0&&!enhanced&&type==PROJECTILE_KERNEL&&motion==MOTION_STRAIGHT;
    const auto* p=FindFusionCannon268ProjectileProfile(kind);
    return p&&way==0&&type==p->nativeType&&motion==MOTION_LOBBED&&damage==p->damage&&
        (kind==72?variant<=1:variant==0)&&(!enhanced||kind==72||kind==82||kind==80);
}
bool FusionCannon268StateValid(uint16_t kind,uint8_t mode,int remaining,int pending,uint16_t rage,uint16_t bag)
{
    const auto* p=FindFusionCannon268Profile(kind);
    if(!p||bag)return false;
    if(!p->manual)return mode<=1&&remaining==0&&pending==0&&rage<=201&&(kind!=914||(!mode&&!rage));
    if(mode>3||remaining<0||remaining>3601||rage||pending<0||pending>(kind==915?117:0))return false;
    const auto phase=static_cast<FusionCannon268Phase>(mode);
    return phase==FusionCannon268Phase::Cooldown||phase==FusionCannon268Phase::Firing ? remaining>0 : remaining==0;
}
const FusionLobberProfile* FindFusionCannon268LobberProfile(uint16_t kind,bool alternate)
{for(const auto& p:lobbers)if(p.kind==kind&&p.alternate==alternate)return &p;return nullptr;}
const FusionLobberProfile* FindFusionCannon268ProjectileProfile(uint16_t kind)
{for(const auto& p:lobbers)if(p.projectileKind==kind)return &p;return nullptr;}
const FusionLobberProjectileTraits* FindFusionCannon268ProjectileTraits(uint16_t kind)
{for(const auto& p:lobberTraits)if(p.kind==kind)return &p;return nullptr;}
const FusionShooterTimingProfile* FindFusionCannon268LobberTiming(uint16_t kind,const char* state)
{if(state)for(const auto& p:lobberClocks)if(p.kind==kind&&!std::strcmp(state,p.visualState))return &p;return nullptr;}
const FusionShooterTimingProfile* FindFusionCannon268SubTiming(){return &subClock;}
int FusionCannon268SplitCount(uint16_t kind,uint8_t status)
{return (kind==73||kind==83)?status==1?10:status==2?15:0:0;}
bool TryGetFusionCannon268DoomDamage(int base,float body,int armor,int& damage)
{
    if(base<=0||!std::isfinite(body)||body<0||armor<0)return false;
    const double result=double(base)+std::floor(float(body+armor)*.3f);
    if(result>(std::numeric_limits<int>::max)())return false;
    damage=static_cast<int>(result);return true;
}
bool IsFusionCannon268Way14(uint16_t kind)
{return kind==65||kind==68||kind==69||kind==73||kind==83||kind==84;}
bool FusionCannon268RotationValid(float rotation)
{return rotation==kFusionCannon268UpRotation||rotation==kFusionCannon268DownRotation;}
double FusionCannon268LandingProbe(uint16_t kind)
{
    if(!IsFusionCannon268Way14(kind))return -1;
    // Source ProjectileCannon Sprite/head: root.4 * Sprite.84277505 * headX3.24.
    return kind==73||kind==83?0:.4*.84277505*3.24;
}
bool TryStepFusionCannon268(uint16_t kind,int age,int rows,double py,float targetX,float targetY,
    float& x,float& y,float& vy,float& rotation,bool& hit)
{
    if(!IsFusionCannon268Way14(kind)||age<0||(rows!=5&&rows!=6)||!std::isfinite(py)||py<=0||
       !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(targetX)||!std::isfinite(targetY))return false;
    const bool down=age>=100;
    const float nextX=down?targetX:x;
    const float nextV=static_cast<float>((down?3:-2)*rows*.01*py);
    const bool landed=down&&double(y)+FusionCannon268LandingProbe(kind)*py>targetY;
    x=nextX;vy=nextV;rotation=down?kFusionCannon268DownRotation:kFusionCannon268UpRotation;hit=landed;
    if(!landed)y+=nextV;
    return true;
}
bool FusionCannon268AreaTouches(double cx,double cy,double x,double y,double w,double h,double py)
{
    constexpr double px=72/1.35;
    if(!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(x)||!std::isfinite(y)||
       !std::isfinite(w)||!std::isfinite(h)||!std::isfinite(py)||w<0||h<0||py<=0)return false;
    const double dx=(std::clamp(cx,x,x+w)-cx)/px,dy=(std::clamp(cy,y,y+h)-cy)/py;
    return dx*dx+dy*dy<=4; // Source OverlapCircleAll radius2, then row difference<=1.
}
}
