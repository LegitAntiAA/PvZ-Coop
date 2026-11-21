#ifndef LAWN_FUSION_REMAINING_PLANTS269_RULES_H
#define LAWN_FUSION_REMAINING_PLANTS269_RULES_H
#include "FusionShooterTiming.h"
#include <cstdint>
namespace LawnFusion
{
// Membership is deliberately separate from complete source coverage.
bool IsFusionRemaining269Kind(uint16_t);
bool FusionRemaining269ControllerKind(uint16_t);
const FusionShooterTimingProfile* FindFusionRemaining269Timing(uint16_t kind,bool grab=false);
bool FusionRemaining269StateValid(uint16_t kind,uint8_t mode,uint16_t bag,uint16_t rage,uint32_t shots,int attribute,int pending);
bool TryChooseFusionPresentZombie269(bool water,int draw,uint16_t& kind);
bool TryGetFusionMixBomb269Recipe(uint16_t center,uint16_t& left,uint16_t& right,uint16_t& result);
bool FusionRemaining269Imitable(uint16_t kind,bool flying,bool pumpkin,bool pot,bool lily);
bool TryAdvanceFusionHamburger269(uint8_t stage,uint16_t donor,uint8_t& next,int& maxHealth,float& interval);
bool TryCapFusionSuperMachine269Damage(int maximum,int incoming,int& applied);
bool FusionBigChomper269Boss(uint16_t sourceZombieKind);
bool FusionBigChomper269AreaTarget(int status,bool sameRow,bool friendly,bool dying,uint16_t sourceKind);
uint16_t FusionRemaining269SnowGatlingPayload(bool sameCellIceBean);
}
#endif
