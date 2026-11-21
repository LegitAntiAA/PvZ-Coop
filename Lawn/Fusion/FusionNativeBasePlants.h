#ifndef LAWN_FUSION_NATIVE_BASE_PLANTS_H
#define LAWN_FUSION_NATIVE_BASE_PLANTS_H

#include "../../SexyAppFramework/Rect.h"
#include <cstdint>
class Plant;
class Zombie;
class Projectile;

namespace LawnFusion
{
bool IsNativeBasePlant(const Plant* plant, uint16_t kind);
bool IsNativeBaseMushroom(const Plant* plant);
bool IsNativeChomper(const Plant* plant);
// New source-model cohort uses authoritative cached phase time, not ghost timers.
// Kind5 keeps its existing native presentation and metadata.
bool GetNativeChomperVisualPose(const Plant* plant, const char*& sourceState, float& elapsedSeconds);
void NativeChomperHealthChanged(Plant* plant);
void InitializeNativeBasePlant(Plant* plant);
bool ResetNativeBaseLaunchCounter(Plant* plant);
void BeginNativeBaseShot(Plant* plant);
void UpdateNativeBasePlantPhase(Plant* plant);
Zombie* FindNativeBaseShooterTarget(Plant* plant, int row);
bool UpdateNativeChomper(Plant* plant);
bool FireNativePeaChomper(Plant* plant);
bool ApplyNativeFumeAreaDamage(Plant* plant, int damage, unsigned int flags);

// Board coordinates of the existing native shadows; no camera/projection or
// second actor positions. Geometry is the native-art adaptation of source units.
bool GetNativeBasePlantGroundAnchor(Plant* plant, float& x, float& y);
bool GetNativeBaseZombieGroundAnchor(Zombie* zombie, float& x, float& y);
bool IsNativePuffProjectile(const Projectile* projectile);
void CreateNativePuffProjectile(Plant* plant, Projectile* projectile);
bool GetNativePuffProjectileRect(const Projectile* projectile, Sexy::Rect& result);
bool NativePuffProjectileCanHit(const Projectile* projectile, Zombie* zombie);
bool NativePuffProjectileExpired(const Projectile* projectile);
}
#endif
