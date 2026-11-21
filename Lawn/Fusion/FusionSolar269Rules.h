#ifndef LAWN_FUSION_SOLAR_269_RULES_H
#define LAWN_FUSION_SOLAR_269_RULES_H
#include "FusionShooterTiming.h"
#include <cstdint>
namespace LawnFusion
{
enum class FusionSolar269WorldBranch:uint8_t {Unavailable,Countdown,ActiveSolar};
const FusionShooterTimingProfile* FindFusionSolar269Timing(const char* state="shoot");
float FusionSolar269Interval(bool cabbagePot);
uint16_t FusionSolar269Projectile(bool travelUltimate22);
bool FusionSolar269RelayKind(uint16_t kind);
bool TryClampFusionSolar269Countdown(float before,float& after);
bool FusionSolar269PlantStateValid(uint32_t shots,uint16_t cooldown,uint16_t sourceStatus,uint8_t mode);
struct FusionSolar269SkillPlan {bool superAnimation=false;int cooldownTicks=0;};
bool TryPlanFusionSolar269Skill(int cooldown,FusionSolar269WorldBranch branch,FusionSolar269SkillPlan& plan);
}
#endif
