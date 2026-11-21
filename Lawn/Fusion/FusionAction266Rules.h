#ifndef LAWN_FUSION_ACTION266_RULES_H
#define LAWN_FUSION_ACTION266_RULES_H
#include "FusionShooterTiming.h"
#include <cstdint>
namespace LawnFusion
{
bool IsFusionAction266Kind(uint16_t kind);
bool IsFusionAction266Gloom(uint16_t kind);
bool IsFusionAction266Rock(uint16_t kind);
const FusionShooterTimingProfile* FindFusionAction266TimingProfile(uint16_t kind);
bool FusionAction266PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode);
float FusionAction266MaximumDuration(uint16_t kind,float speed);
bool FusionAction266GloomTouches(uint16_t kind,int rowDelta,double shadowX,double shadowY,
    double targetX,double targetY,double width,double height);
bool TryGetFusionAction266RockStage(int health,int maximum,uint8_t& stage);
bool TryGetFusionAction266LanternHeal(int sourceLightLevel,int& amount);
bool TryGetFusionAction266Ticks(double seconds,float speed,int& ticks);
}
#endif
