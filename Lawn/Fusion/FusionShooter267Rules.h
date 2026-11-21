#ifndef LAWN_FUSION_SHOOTER_267_RULES_H
#define LAWN_FUSION_SHOOTER_267_RULES_H
#include "FusionShooterTiming.h"
#include "FusionRecipientShooterRules.h"
#include "../../ConstEnums.h"
namespace LawnFusion
{
bool IsFusionShooter267Kind(uint16_t kind);
const FusionShooterTimingProfile* FindFusionShooter267Timing(uint16_t kind);
const FusionShooterTimingProfile* FindFusionShooter267Timing(uint16_t kind,const char* state);
bool FusionShooter267StateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode);
bool TryGetFusionShooter267PhaseTicks(uint16_t kind,uint8_t mode,float animationSpeed,uint16_t rateTier,int& ticks);
bool TryGetFusionPeaMineInterval(float sourceDistance,float& seconds);
bool TryGetFusionUltimateGatling(int health,int maximum,float& rate,float& velocity,int& damage);
bool FusionSourceZombieWithIron(int firstArmor,int secondArmor,int sourceKind);
bool TryGetFusionMagnetCactusDamage(int damage,int firstArmor,int secondArmor,int& result);
bool TryGetFusionGoldTorchReturn(uint8_t way,uint8_t flag,int age,int row,int rows,uint8_t& nextWay,int& nextRow);
bool FusionShooter267ProjectileMetadataValid(uint16_t kind,int damage,uint8_t way,uint8_t fire,
    uint8_t variant,int type,int motion);
bool TryMakeFusionShooter267ProjectileRect(uint16_t kind,double x,double y,FusionRecipientRect& result,
    double pixelsY=100./1.67,double angle=0);
bool FusionShooter267RotatedProjectileTouchesRect(uint16_t kind,double x,double y,double angle,
    const FusionRecipientRect& target,double pixelsY=100./1.67);
bool TryGetFusionShooter267ThreeVerticalStep(uint8_t way,int age,float& pixels,double rowPitch=100);
}
#endif
