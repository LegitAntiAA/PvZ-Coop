#include "FusionSolar269Rules.h"
#include <cmath>
#include <cstring>
namespace LawnFusion
{
namespace
{
constexpr double shoots[]={double(.33333334f),double(.5f),double(.6666667f),double(.8333333f),double(1.f)};
constexpr double supers[]={double(.33333334f),double(1.3333334f)};
const FusionShooterTimingProfile timings[]={
    {934,2,shoots,double(1.3f),double(1.3333334f),true,75,"shoot"},
    {934,2,supers,1,double(1.6666666f),false,65535,"super"}};
}
const FusionShooterTimingProfile* FindFusionSolar269Timing(const char* state)
{if(state)for(const auto& t:timings)if(!std::strcmp(state,t.visualState))return &t;return nullptr;}
float FusionSolar269Interval(bool pot){return pot?1.5f:2.f;}
uint16_t FusionSolar269Projectile(bool travel){return travel?53:75;}
bool FusionSolar269RelayKind(uint16_t kind){return kind==1128||kind==916;}
bool TryClampFusionSolar269Countdown(float before,float& after)
{if(!std::isfinite(before))return false;after=before<.5f?before:.5f;return true;}
bool FusionSolar269PlantStateValid(uint32_t,uint16_t cooldown,uint16_t status,uint8_t mode)
{return cooldown<=200&&(status==0||status==13)&&mode<=1;}
bool TryPlanFusionSolar269Skill(int cooldown,FusionSolar269WorldBranch branch,FusionSolar269SkillPlan& result)
{
    if(cooldown!=0||(branch!=FusionSolar269WorldBranch::Countdown&&branch!=FusionSolar269WorldBranch::ActiveSolar))return false;
    result={branch==FusionSolar269WorldBranch::ActiveSolar,200};return true;
}
}
