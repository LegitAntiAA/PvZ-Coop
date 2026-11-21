#ifndef LAWN_FUSION_SUMMON_PLANTS269_RULES_H
#define LAWN_FUSION_SUMMON_PLANTS269_RULES_H
#include "FusionShooterTiming.h"
#include "FusionSummon269Rules.h"
#include <cstdint>
namespace LawnFusion
{
bool IsFusionSummonPlants269Kind(uint16_t kind);
const FusionShooterTimingProfile* FindFusionSummonPlants269Timing(uint16_t kind);
float FusionSummonPlants269MaximumDuration(uint16_t kind,float speed);
bool FusionSummonPlants269StateValid(uint16_t kind,uint8_t mode,uint16_t bag,uint16_t colors,
    int attributeTicks,int launchTicks,uint32_t impacts);
bool TryReceiveFusionHypnoNut269Color(uint16_t before,unsigned color,uint16_t& after,bool& rainbow);
bool FusionGoldMagnet269Planar(int nativeType,int nativeMotion);
bool FusionGoldMagnet269Eligible(int nativeType,int nativeMotion,uint8_t sourceWay,bool alreadyAttracted,int rowDelta,float velocityY);
bool TryStepFusionGoldMagnet269(uint8_t way,bool zombieBullet,int age,float& sourceVelocityY);
struct FusionCherryMagnet269Point {float x=0,y=0;};
// Source Rigidbody force, mass1/gravity0. Actual Coin velocity/position are the
// only physical state; caller converts their native units at the boundary.
bool TryStepFusionCherryMagnet269(FusionCherryMagnet269Point position,FusionCherryMagnet269Point center,
    FusionCherryMagnet269Point target,bool hasTarget,FusionCherryMagnet269Point velocity,
    FusionCherryMagnet269Point& nextVelocity,bool& impact);
bool FusionSummonPlants269WaterItemSupported(uint8_t item);
}
#endif
