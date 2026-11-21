#ifndef LAWN_FUSION_WATER268_RULES_H
#define LAWN_FUSION_WATER268_RULES_H
#include "FusionShooterTiming.h"
#include "FusionRecipientShooterRules.h"
#include <cstdint>
namespace LawnFusion
{
bool IsFusionWater268Kind(uint16_t kind);
// Placement is separate from controller-module membership (land CactusStar1085).
bool IsFusionWater268WaterPlant(uint16_t kind);
bool FusionWater268StarTarget(int status,bool controlled,bool colliderEnabled,bool alive);
bool FusionWater268StarTrackingDue(uint8_t way,int ageTicks);
bool IsFusionWater268Kelp(uint16_t kind);
bool IsFusionWater268HomingPlant(uint16_t kind);
bool FusionWater268HomingKind(uint16_t kind);
bool FusionWater268HomingTarget(int status,bool controlled,bool colliderEnabled,bool alive,bool retain);
bool FusionWater268GrabTarget(int status,int rowDelta,bool controlled,bool beforeDying);
bool TryGetFusionWater268HomingStep(double dx,double dy,double pixelsY,double speed,float& vx,float& vy);
bool TryGetFusionWater268GrabTicks(float animationSpeed,int& ticks);
bool FusionWater268PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode);
bool FusionWater268CaptureStateValid(uint16_t kind,uint8_t mode,int grabTicks,int sinkTicks,
    uint16_t targetId,uint32_t targetToken,float animationSpeed);
bool FusionWater268HomingMetadataValid(uint16_t kind,int damage,uint8_t way);
const FusionShooterTimingProfile* FindFusionWater268Timing(uint16_t kind,bool grown=false,bool air=false);
const FusionShooterTimingProfile* FindFusionWater268Timing(uint16_t kind,const char* state);
bool FusionWater268ProjectileMetadataValid(uint16_t kind,int damage,uint8_t way);
bool TryGetFusionWater268SkyAngle(int age,float& angle);
bool TryAddFusionWater268Grap(int ticks,int count,int& nextTicks,int& nextCount,bool& percent,bool& full);
bool TryGetFusionWater268SearchTicks(float speed,int& ticks);
bool FusionWater268SearchEventDue(int remaining,float speed,bool crash);
bool FusionWater268ProjectileCanHit(uint8_t way,int status,int rowDelta,bool controlled,bool colliderEnabled,bool alive);
bool TryMakeFusionWater268ProjectileRect(uint16_t kind,double x,double y,double angle,double pixelsY,FusionRecipientRect& result);
bool FusionWater268ProjectileTouchesRect(uint16_t kind,double x,double y,double angle,double pixelsY,const FusionRecipientRect& target);
constexpr int kFusionWater268SinkTicks=50;
}
#endif
