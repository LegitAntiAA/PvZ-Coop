#ifndef LAWN_FUSION_NATIVE_CHERRY_SHOOTER_H
#define LAWN_FUSION_NATIVE_CHERRY_SHOOTER_H

#include "../../SexyAppFramework/Rect.h"

class Plant;
class Projectile;
class Zombie;

namespace LawnFusion
{
struct FusionSplitEmissionSpec;
bool IsNativeCherryShooter(const Plant* plant);
bool IsNativeCherryProjectile(const Projectile* projectile);
bool IsNativeTimedPeaShooter(const Plant* plant);
bool IsNativeTimedPeaProjectile(const Projectile* projectile);
bool IsNativeSplitShooter(const Plant* plant);
Zombie* FindNativeSplitShooterTarget(Plant* plant, int row);
void InitializeNativeCherryShooter(Plant* plant);
bool ResetNativeCherryLaunchCounter(Plant* plant);
void BeginNativeCherryShot(Plant* plant);
void UpdateNativeCherryShooterPhase(Plant* plant);
void CreateNativeCherryProjectile(Plant* source, Projectile* projectile);
void CreateNativeTimedPeaProjectile(Plant* source, Projectile* projectile, const FusionSplitEmissionSpec* emission = nullptr);
bool UpdateNativeTimedPeaProjectileMotion(Projectile* projectile);
bool TryGetNativeTimedPeaDamageFlags(const Projectile* projectile, unsigned int& flags);
bool GetNativeTimedPeaProjectileCenter(const Projectile* projectile, float& x, float& y);
bool GetNativeTimedPeaProjectileRect(const Projectile* projectile, Sexy::Rect& rect);
bool NativeTimedPeaProjectileCanHit(const Projectile* projectile, Zombie* zombie);
bool NativeTimedPeaProjectileExpired(const Projectile* projectile);

// Board-space visual ground anchor and centre. Projectile retains native40x40
// logical top-left; centre=(mPosX+20,mPosY+20+mPosZ). BeginDraw translation is not
// included here: renderers subtract mX/mY exactly once for their local Graphics.
bool GetNativeCherryShooterGroundAnchor(Plant* plant, float& x, float& y);
bool GetNativeCherryProjectileCenter(const Projectile* projectile, float& x, float& y);
bool GetNativeCherryProjectileRect(const Projectile* projectile, Sexy::Rect& rect);
bool NativeCherryProjectileCanHit(const Projectile* projectile, Zombie* zombie);
bool NativeCherryProjectileExpired(const Projectile* projectile);
}

#endif
