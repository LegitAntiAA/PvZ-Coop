#ifndef LAWN_FUSION_NATIVE_RECIPIENT_SHOOTERS_H
#define LAWN_FUSION_NATIVE_RECIPIENT_SHOOTERS_H
#include "FusionRecipientShooterRules.h"
#include "../../SexyAppFramework/Rect.h"
class Plant;
class Projectile;
class Zombie;
namespace LawnFusion
{
bool IsNativeRecipientShooter(const Plant* plant);
const FusionShooterTimingProfile* GetNativeRecipientTimingProfile(const Plant* plant);
void InitializeNativeRecipientShooter(Plant* plant);
// Existing Plant.UpdateAbilities after UpdateShooting/stateCountdown--.
bool UpdateNativeRecipientShooter(Plant* plant);
void UpdateNativeRecipientShooterPhase(Plant* plant);
// Existing common shooting event dispatcher before its ordinary Fire path.
bool DispatchNativeRecipientShot(Plant* plant);
bool GetNativeRecipientVisualPose(const Plant* plant, const char*& state, float& elapsed);
bool IsNativeRecipientProjectile(const Projectile* projectile);
bool GetNativeRecipientProjectileCenter(const Projectile* projectile, float& x, float& y);
bool GetNativeRecipientProjectileRect(const Projectile* projectile, Sexy::Rect& rect);
bool NativeRecipientProjectileCanHit(const Projectile* projectile, Zombie* zombie);
bool NativeRecipientProjectileExpired(const Projectile* projectile);
// Root calls this as an early native motion branch and then the collision hook,
// returning before vanilla STAR row reassignment/ground and fixed3.33 motion.
bool MoveNativeRecipientProjectile(Projectile* projectile);
bool HandleNativeRecipientProjectileCollision(Projectile* projectile);
bool ApplyNativeRecipientProjectileImpact(Projectile* projectile, Zombie* zombie);
}
#endif
