#ifndef LAWN_FUSION_NATIVE_SHOOTER_BATCH_H
#define LAWN_FUSION_NATIVE_SHOOTER_BATCH_H

#include "FusionShooterBatchRules.h"
#include "../../SexyAppFramework/Rect.h"
class Plant;
class Projectile;
class Zombie;

namespace LawnFusion
{
bool IsNativeShooterBatch(const Plant* plant);
const FusionShooterTimingProfile* GetNativeShooterBatchTimingProfile(const Plant* plant);
void InitializeNativeShooterBatch(Plant* plant);
// Called after the existing native counters decrement. true suppresses the
// vanilla family branch; common native collision/death still runs normally.
bool UpdateNativeShooterBatch(Plant* plant);
void UpdateNativeShooterBatchPhase(Plant* plant);
bool DispatchNativeShooterBatchShot(Plant* plant);
bool ApplyNativeShooterBatchSpecial(Plant* plant);
bool GetNativeShooterBatchVisualPose(const Plant* plant,const char*& state,float& elapsed);
bool GetNativeShooterBatchBaseVisualPose(const Plant* plant,const char*& state,float& elapsed);
bool NativeShooterBatchInvulnerable(const Plant* plant);
// Hook after the existing Chomper controller accepts a real swallowed target.
void NativeShooterBatchChomperSwallowed(Plant* plant);

// Shared source92 primitive. The caller owns source initial20s/self75 policy;
// this creates exactly one real projectile and never damages its source plant.
bool LaunchNativePotatoPuff(Plant* plant,int damage);
bool IsNativeShooterBatchProjectile(const Projectile* projectile);
bool GetNativeShooterBatchProjectileCenter(const Projectile*,float& x,float& y);
bool GetNativeShooterBatchProjectileRect(const Projectile*,Sexy::Rect& rect);
bool MoveNativeShooterBatchProjectile(Projectile* projectile);
bool NativeShooterBatchProjectileExpired(const Projectile* projectile);
bool NativeShooterBatchProjectileCanHit(const Projectile* projectile,Zombie* zombie);
bool HandleNativeShooterBatchProjectileCollision(Projectile* projectile);
bool ApplyNativeShooterBatchProjectileImpact(Projectile* projectile,Zombie* zombie);
// CheckPlant's immediate Rebound and HitPlant's target selection precede the
// source BombCherry.Start continuation. Root queues that continuation on the
// real native carrier age, retaining selected target UID/full generation.
bool TryReflectNativeSuperCherryProjectile(Projectile*,Plant* attacked);
Plant* SelectNativeSuperCherryPlantTarget(Projectile*,Plant* attacked);
bool ApplyNativeSuperCherryProjectileImpact(Projectile*,Plant* selectedTarget,bool plantContact=true);
}
#endif
