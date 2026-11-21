#ifndef LAWN_FUSION_LAST_PLANTS269_RULES_H
#define LAWN_FUSION_LAST_PLANTS269_RULES_H
#include "FusionShooterTiming.h"
#include "FusionRecipientShooterRules.h"
#include <cstdint>
namespace LawnFusion
{
bool IsFusionLastPlants269Kind(uint16_t kind);
const FusionShooterTimingProfile* FindFusionLastPlants269Timing(uint16_t kind);
bool FusionLastPlants269StateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode,uint8_t stage);
bool FusionLastPlants269ClockValid(uint16_t kind,int production,int pending,float speed,float shootSeconds);
bool FusionLastPlants269TorchContactAllowed(uint8_t way,bool sameRow,bool zombie,bool sameTorch);
bool FusionLastPlants269TorchPayload(uint16_t kind);
int FusionLastPlants269WheatTicks(bool fastStart);
bool FusionLastPlants269KelpProjectileValid(uint16_t kind,int damage,uint8_t way);
bool FusionLastPlants269KelpSputterAllowed(int secondArmorType,int sourceNpcKind);
bool FusionLastPlants269KelpProjectileRect(double x,double y,double pixelsY,FusionRecipientRect& out);
bool FusionLastPlants269KelpProjectileTouches(double x,double y,double pixelsY,const FusionRecipientRect&);
}
#endif
