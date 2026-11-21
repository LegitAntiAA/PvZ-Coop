#ifndef LAWN_FUSION_WORLD_REMAINDER269_RULES_H
#define LAWN_FUSION_WORLD_REMAINDER269_RULES_H
#include "FusionShooterTiming.h"
#include <cstdint>
namespace LawnFusion
{
const FusionShooterTimingProfile* FindFusionWorldRemainder269Timing(uint16_t kind);
bool FusionWorldRemainder269CattailTarget(int status,bool controlled,bool alive);
float FusionWorldRemainder269PassiveSeconds(int superStarCount);
bool TryGetFusionWorldRemainder269StarDamage(int base,int light,int magnets,bool ultimate9,int& damage);
bool FusionWorldRemainder269PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode,uint8_t damageStage);
bool FusionWorldRemainder269ClockValid(uint16_t kind,int production,int pending,float speed,float shootSeconds);
}
#endif
