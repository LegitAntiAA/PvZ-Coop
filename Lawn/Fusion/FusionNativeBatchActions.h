#ifndef LAWN_FUSION_NATIVE_BATCH_ACTIONS_H
#define LAWN_FUSION_NATIVE_BATCH_ACTIONS_H
#include "FusionBatchActionRules.h"
#include "../../SexyAppFramework/Rect.h"
class Board;
class Plant;
class Projectile;
class Zombie;
namespace LawnFusion
{
bool IsNativeBatchAction(const Plant* plant);
const FusionShooterTimingProfile* GetNativeBatchTimingProfile(const Plant* plant);
void InitializeNativeBatchAction(Plant* plant);
// Once after a fresh real factory commit, never from initialization previews,
// ghost binds, snapshot apply, or ordinary glove movement.
void NativeBatchPlantPlaced(Plant* plant);
// After UpdateShooting and native mStateCountdown--, before vanilla family/
// UpdateShooter. true suppresses those family branches, not common DoSpecial.
bool UpdateNativeBatchAction(Plant* plant);
void UpdateNativeBatchActionPhase(Plant* plant);
bool DispatchNativeBatchShot(Plant* plant);
bool ApplyNativeBatchSpecial(Plant* plant);
// Existing native counters own the source60s manual cooldown; no auto-fire.
bool TryActivateNativeDoomFume(Plant* plant);
// First canonical removal, after mDead=true, with255 as silent rollback.
void NativeBatchPlantRemoving(Plant* plant,int sourceReason);
// First canonical native death entry; consumes the source tag before any blast.
void NativeBatchZombieDying(Zombie* zombie);
// Synchronous continuations may finish after their first kill awards the level.
void ApplyNativeBatchFireRow(Board* board,int row,int damage=1800,bool fix=false);
void ApplyNativeBatchDoomAt(Board* board,float nativeShadowX,float nativeShadowY,int col,int row,
    int damage=1800,bool setPit=true,bool iceDoom=false);
bool HandleNativeBatchPlantEaten(Zombie* zombie, Plant* plant);
bool NativeBatchPlantUntargetable(const Plant* plant);
bool NativeBatchPlantInvincible(const Plant* plant);
// Contact actors use their actual source collider envelope for native chewing.
bool GetNativeBatchContactPlantRect(const Plant* plant,Sexy::Rect& rect);
bool GetNativeBatchVisualPose(const Plant* plant, const char*& state, float& elapsed);
bool IsNativeBatchProjectile(const Projectile* projectile);
bool GetNativeBatchProjectileCenter(const Projectile* projectile, float& x, float& y);
// Called from Projectile.UpdateNormalMotion's movement branch. Only changes
// existing velocity/position once; normal shared collision remains its caller.
bool MoveNativeBatchProjectile(Projectile* projectile);
bool ApplyNativeBatchProjectileImpact(Projectile* projectile, Zombie* zombie);
bool GetNativeBatchProjectileRect(const Projectile* projectile, Sexy::Rect& rect);
bool NativeBatchProjectileCanHit(const Projectile* projectile, Zombie* zombie);
bool NativeBatchProjectileExpired(const Projectile* projectile);
}
#endif
