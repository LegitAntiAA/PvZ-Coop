#ifndef LAWN_FUSION_SHULK268_RULES_H
#define LAWN_FUSION_SHULK268_RULES_H
#include "FusionShooterTiming.h"
#include "FusionRecipientShooterRules.h"
namespace LawnFusion
{
bool IsFusionShulk268Kind(uint16_t kind);
bool FusionShulk268Target(int status,int rowDelta,bool controlled,bool colliderEnabled,bool alive);
const FusionShooterTimingProfile* FindFusionShulk268Timing(uint16_t kind);
bool TryGetFusionShulk268PhaseTicks(uint16_t kind,uint8_t mode,int& ticks);
bool TryStepFusionShulk268Steering(double currentX,double currentY,double targetX,double targetY,bool target,float& vx,float& vy);
bool FusionShulk268StateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode);
bool FusionShulk268ClockStateValid(uint16_t kind,int searchTicks,int phaseTicks,float speed,float shootSeconds,uint8_t mode);
bool FusionChomper268ProjectileMetadataValid(uint16_t kind,int damage,uint8_t way);
bool TryMakeFusionChomper268ProjectileRect(uint16_t kind,double x,double y,double angle,double pixelsY,FusionRecipientRect& rect);
bool FusionChomper268ProjectileTouchesRect(uint16_t kind,double x,double y,double angle,double pixelsY,const FusionRecipientRect& target);
}
#endif
