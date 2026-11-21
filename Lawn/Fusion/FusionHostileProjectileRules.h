#ifndef LAWN_FUSION_HOSTILE_PROJECTILE_RULES_H
#define LAWN_FUSION_HOSTILE_PROJECTILE_RULES_H
#include "FusionRecipientShooterRules.h"
#include <cstdint>

namespace LawnFusion
{
struct FusionHostilePlanarShot
{
    double x=0,y=0,velocityX=0,velocityY=0,gravityY=0;
};
// CatapultZombie.AnimShoot3511E0 uses the raw Shoot point for its two-second
// trajectory, then spawns one source world unit above that point.
bool TryMakeFusionHostileBasketballShot(double shootX,double shootY,
    double targetX,double targetY,FusionHostilePlanarShot& shot,double pixelsY=100./1.67);

enum class FusionSuperCherryPlantEffect { Ignore, Damage, Heal };
struct FusionSuperCherryPlantPlan
{
    FusionSuperCherryPlantEffect effect=FusionSuperCherryPlantEffect::Ignore;
    int amount=0;
};
// Source104 creates kind3/300, way9 hostile or way0 friendly. Rebound4237E0
// changes only the latter to1000 (2000 on UmbrellaPot1133).
bool FusionSuperCherryProjectileMetadataValid(uint16_t kind,int damage,uint8_t way,
    bool hostile,ProjectileType type,ProjectileMotion motion);
bool FusionSuperCherryPlantShort(uint16_t kind);
bool FusionSuperCherryPlantPumpkin(uint16_t kind);
bool FusionSuperCherryPlantSingleTarget(uint16_t kind);
bool FusionSuperCherryPlantCollides(uint16_t kind,int projectileRow,int plantRow,bool colliderEnabled);
bool FusionSuperCherryCollateralCell(int columnDelta,int rowDelta);
FusionSuperCherryPlantPlan PlanFusionSuperCherryPlantHit(uint16_t kind,bool cherryCover,bool cherryMagnetGraph);
bool TryPlanFusionSuperCherryRebound(uint16_t defenderKind,bool animationRunning,bool umbrellaPot,int& reflectedDamage);

bool FusionHostilePlanarShotValid(const FusionHostilePlanarShot& shot);
bool AdvanceFusionHostilePlanarShot(FusionHostilePlanarShot& shot);
bool TryMakeFusionHostileProjectileRect(uint16_t kind,double centerX,double centerY,double pixelsY,FusionRecipientRect& rect);
}

#endif
