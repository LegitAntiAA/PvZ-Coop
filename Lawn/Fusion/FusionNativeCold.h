#ifndef LAWN_FUSION_NATIVE_COLD_H
#define LAWN_FUSION_NATIVE_COLD_H

#include "FusionColdRules.h"
class Plant;
class Board;
class Projectile;
class Zombie;

namespace LawnFusion
{
struct FusionColdDamageContext
{
    uint16_t projectileKind = 0;
    FusionColdDamageDelivery delivery = FusionColdDamageDelivery::ProjectileProfile;
};
bool IsNativeIceShroom(const Plant* plant);
bool UpdateNativeColdContact(Plant* plant);
void InitializeNativeIceShroom(Plant* plant);
bool ApplyNativeIceShroom(Plant* plant);
// Source Board.CreateFreeze, with no required plant and no caller retirement.
// x/y are the caller's canonical Board-space effect position.
bool ApplyNativeColdBoardFreeze(Board* board, float x, float y);
bool IsNativeColdProjectile(const Projectile* projectile);
bool PrepareNativeColdHit(Projectile* projectile, Zombie* target, int& damage, FusionColdDamageContext& context);
void NativeColdDamageAfterShield(Zombie* target, const FusionColdDamageContext& context, bool shieldPassed);
// Shared source cold-slow effect after the caller admits a susceptible hit.
void ApplyNativeColdSlow(Zombie* target);
// Local source SetFreeze only. No global Board freeze/defender pulse or damage.
bool ApplyNativeColdFreeze(Zombie* target, int ticks);
bool AddNativeColdLevel(Zombie* target, int amount);
void NativeColdFreezeExpired(Zombie* target);
bool HandleNativeColdPiercingCollision(Projectile* projectile);
bool ApplyNativeColdPiercingImpact(Projectile* projectile, Zombie* target);
// Only an actual source land callback; ordinary lifetime retirement stays Die.
bool ApplyNativeColdProjectileLand(Projectile* projectile);
void RetireNativeColdProjectile(Projectile* projectile);
}
#endif
