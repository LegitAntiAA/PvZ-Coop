#include "FusionAction266Rules.h"
#include "FusionFireActionBatchRules.h"
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr double rockEvents[]={static_cast<double>(.33333334f),static_cast<double>(.7f)};
constexpr double gloomEvents[]={static_cast<double>(.6f),static_cast<double>(.8666667f),
    static_cast<double>(1.1333333f),static_cast<double>(1.4f)};
constexpr double puffEvents[]={static_cast<double>(.38333333f)};
const FusionShooterTimingProfile clocks[]={
    {1060,1,rockEvents,static_cast<double>(1.2f),static_cast<double>(.9f),true,0xffff,"attack"},
    {1074,1,rockEvents,static_cast<double>(1.2f),static_cast<double>(.9f),true,0xffff,"attack"},
    {1075,1,rockEvents,static_cast<double>(1.2f),static_cast<double>(.9f),true,0xffff,"attack"},
    {1070,1.9f,gloomEvents,static_cast<double>(1.3f),static_cast<double>(2.0166667f),true,0xffff,"attack"},
    {1071,1.9f,gloomEvents,static_cast<double>(1.3f),2,true,0xffff,"attack"},
    {1072,1.9f,gloomEvents,static_cast<double>(1.3f),static_cast<double>(2.0166667f),true,0xffff,"attack"},
    {1170,1.5f,puffEvents,1,.5,true,92,"shoot"}};
}
bool IsFusionAction266Kind(uint16_t kind)
{
    constexpr uint16_t ids[]={1002,1009,1015,1060,1070,1071,1072,1074,1075,1087,
        1145,1146,1163,1170,1190,1057,1155,1048,1189,1039};
    for(auto id:ids)if(kind==id)return true;return false;
}
bool IsFusionAction266Gloom(uint16_t kind){return kind>=1070&&kind<=1072;}
bool IsFusionAction266Rock(uint16_t kind){return kind==1060||kind==1074||kind==1075;}
const FusionShooterTimingProfile* FindFusionAction266TimingProfile(uint16_t kind)
{for(const auto& clock:clocks)if(clock.kind==kind)return &clock;return nullptr;}
bool FusionAction266PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode)
{
    if(!IsFusionAction266Kind(kind)||shots||rage||bag)return false;
    return kind==1015?mode<=2:kind==1170?mode<=1:mode==0;
}
float FusionAction266MaximumDuration(uint16_t kind,float speed)
{
    if(!std::isfinite(speed)||speed<.9f||speed>1.1f)return 0;
    if(const auto* profile=FindFusionAction266TimingProfile(kind))return FusionShooterDuration(*profile,speed)+.02f;
    if(kind==1002)return 1.0f/speed+.02f;
    if(kind==1146)return 1.02f;
    return 0;
}
bool FusionAction266GloomTouches(uint16_t kind,int rowDelta,double sx,double sy,
    double x,double y,double width,double height)
{
    if(!IsFusionAction266Gloom(kind)||rowDelta< -1||rowDelta>1)return false;
    // Gloom radius2 (ctor4B2530), actual Shoot(0,2.12) relative to
    // Shadow(0,.46), root scale.33. Source OverlapCircle is projected once.
    return FusionFireCircleTouchesRect(sx,sy-(2.12-.46)*.33*(100.0/1.67),2,x,y,width,height);
}
bool TryGetFusionAction266RockStage(int health,int maximum,uint8_t& stage)
{
    if(maximum<=0||health>maximum)return false;
    // SpikeRock.SpriteUpdate includes both exact lower health boundaries.
    stage=health>int64_t(maximum)*2/3?0:health>maximum/3?1:2;return true;
}
bool TryGetFusionAction266LanternHeal(int count,int& amount)
{
    if(count<0||count>1+(std::numeric_limits<int>::max)()/150)return false;
    amount=count>1?(count-1)*150:0;return true;
}
bool TryGetFusionAction266Ticks(double seconds,float speed,int& ticks)
{
    if(!std::isfinite(seconds)||seconds<0||!std::isfinite(speed)||speed<=0)return false;
    const double n=std::ceil(seconds/static_cast<double>(speed)*100);
    if(n>(std::numeric_limits<int>::max)())return false;ticks=static_cast<int>(n);return true;
}
}
