#ifndef LAWN_FUSION_NATIVE_ROLLING_PROJECTILE_H
#define LAWN_FUSION_NATIVE_ROLLING_PROJECTILE_H

#include "../../SexyAppFramework/Rect.h"
#include <cstdint>
class Plant;
class Projectile;
class Zombie;

namespace LawnFusion
{
bool IsNativeRollingProjectile(const Projectile* projectile);
bool LaunchNativeChomperBlock(Plant* plant,uint16_t kind);
void CreateNativeRollingProjectile(Plant* plant, Projectile* projectile);
bool GetNativeRollingProjectileCenter(const Projectile* projectile, float& x, float& y);
bool GetNativeRollingProjectileRect(const Projectile* projectile, Sexy::Rect& rect);
bool NativeRollingProjectileCanHit(const Projectile* projectile, Zombie* target);
bool UpdateNativeRollingProjectileMotion(Projectile* projectile);
bool HandleNativeRollingProjectileCollision(Projectile* projectile);
bool ApplyNativeRollingProjectileImpact(Projectile* projectile, Zombie* target);
void RetireNativeRollingProjectile(Projectile* projectile);
}
#endif
