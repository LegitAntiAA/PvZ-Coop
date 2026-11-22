#pragma once
#include <cstdint>
namespace LawnFusion
{
struct FusionBossBall366Motion {float vx=0,vy=0,animatorSpeed=0,scale=1;};
float FusionBossBall366Scale(int ageTicks);
bool PlanFusionBossBall366Motion(int ageTicks,float sourceX,float currentSourceVX,bool roof,FusionBossBall366Motion&);
}
