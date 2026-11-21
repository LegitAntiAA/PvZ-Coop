#ifndef LAWN_FUSION_NATIVE_FIRE_ACTION_BATCH_H
#define LAWN_FUSION_NATIVE_FIRE_ACTION_BATCH_H
#include "FusionFireActionBatchRules.h"
#include "../../SexyAppFramework/Rect.h"
class Board;
class Plant;
class Projectile;
class Zombie;
namespace LawnFusion
{
bool IsNativeFireAction(const Plant* plant);
void InitializeNativeFireAction(Plant* plant);
// Host only, after common stateCountdown decrement; false lets the shared
// Caltrop family continue. No independent simulation/entity storage is owned.
bool UpdateNativeFireAction(Plant* plant);
void UpdateNativeFireActionPhase(Plant* plant);
bool ApplyNativeFireActionSpecial(Plant* plant);
void NativeFireActionPlantRemoving(Plant* plant,int sourceReason);
bool NativeFireActionUntargetable(const Plant* plant);
bool NativeFireActionInvincible(const Plant* plant);
bool GetNativeFireActionVisualPose(const Plant* plant,const char*& state,float& elapsed);
void RestoreNativeFireActionPresentation(Plant* plant);
void NativeFireActionHealthChanged(Plant* plant);
int GetNativeFireLightLevel(const Plant* plant);
// Source Board.TempAddLightLevel leases survive the originating Blover. Host
// deadlines use actual Board ticks; clients read the authoritative count only.
void UpdateNativeFireLight(Board* board);
bool AddNativeFireLight(Board* board,int ticks = 3000);
// Shared Caltrop dispatcher invokes this before its ordinary HitArea fallback.
bool ApplyNativeFireCaltropStrike(Plant* plant);
bool EmitNativeFireThree(Plant* plant);
void NativeFireConversionCommitted(Plant* plant);
bool IsNativeFireActionProjectile(const Projectile* projectile);
bool GetNativeFireActionProjectileCenter(const Projectile* projectile,float& x,float& y);
bool GetNativeFireActionProjectileRect(const Projectile* projectile,Sexy::Rect& rect);
bool MoveNativeFireActionProjectile(Projectile* projectile);
bool HandleNativeFireActionProjectileCollision(Projectile* projectile);
bool ApplyNativeFireActionProjectileImpact(Projectile* projectile,Zombie* target);
}
#endif
