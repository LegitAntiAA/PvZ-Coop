#include "FusionBossBall366Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
float FusionBossBall366Scale(int age)
{return 1.f+.7f*std::clamp(float(age)*.01f,0.f,1.f);}
bool PlanFusionBossBall366Motion(int age,float x,float vx,bool roof,FusionBossBall366Motion& out)
{
 if(age<0||!std::isfinite(x)||!std::isfinite(vx)||vx>0||vx< -1000)return false;
 FusionBossBall366Motion p;p.vx=age>=100?-.3f:vx;p.vy=roof&&x<1.5f?.22f*p.vx:0;
 p.animatorSpeed=-p.vx*.5f;p.scale=FusionBossBall366Scale(age);out=p;return true;
}
}
