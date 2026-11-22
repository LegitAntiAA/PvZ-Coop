#pragma once
#include "FusionHostileProjectileRules.h"
#include "FusionSourceGraph313.h"
#include <cstdint>
namespace LawnFusion {
constexpr float kFusionSnowGun363Gravity=9.81f; // Actual ProjectSettings/Physics2DSettings.
bool FusionSnowBall363Metadata(uint16_t kind,int damage,uint8_t way,ProjectileType,ProjectileMotion);
bool TryPlanFusionSnowGun363Shot(double x,double y,double targetX,double targetY,double pixelsY,FusionHostilePlanarShot&);
bool FusionSnowBall363AreaCell(int columnDelta,int rowDelta);
bool FusionSnowBall363PlantAffected(bool flying,bool pumpkin,bool pumpkinPresent);
const FusionSourceGraph313* FindFusionSnowGun363Graph(uint16_t);
bool SourceRootDeltaSnowGun363(uint16_t,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
