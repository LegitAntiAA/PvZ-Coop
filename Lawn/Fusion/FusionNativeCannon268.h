#ifndef LAWN_FUSION_NATIVE_CANNON_268_H
#define LAWN_FUSION_NATIVE_CANNON_268_H
#include "FusionCannon268Rules.h"
#include "../../SexyAppFramework/Rect.h"
class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
bool IsNativeCannon268(const Plant* plant);
bool CanAimNativeCannon268(const Plant* plant);
// Root authenticates full plant ID/token and player ownership before this call.
// Target is an unshifted native Board point; this function owns readiness commit.
bool CommitAimNativeCannon268(Plant* plant,int boardTargetX,int boardTargetY);
bool InitializeNativeCannon268(Plant* plant);
// After common native mStateCountdown decrement. No parallel timer is owned.
bool UpdateNativeCannon268(Plant* plant);
// After common native mShootingCounter decrement; skips Classic Cob events.
bool DispatchNativeCannon268Events(Plant* plant);
void UpdateNativeCannon268Phase(Plant* plant);
bool GetNativeCannon268VisualPose(const Plant* plant,const char*& state,float& seconds);
// Root routes these ahead of ordinary Lobber callbacks. Money authorization/fee
// remains outside: this helper only commits the source 2-second flash/animation.
bool CanActivateNativeCannon268Money(const Plant* plant);
bool TryActivateNativeCannon268Money(Plant* plant);
bool FireNativeCannon268Money(Plant* plant);
void UpdateNativeCannon268Money(Plant* plant);
// Called once after ordinary emission has captured its current pot traits.
void NativeCannon268LobberEmitted(Plant* plant,Projectile* projectile);
// Before retiring payload72, after its normal primary+splash callback.
void NativeCannon268LobberChildren(Projectile* projectile,Zombie* primary);
// In shared secondary loop, before ordinary damage; handles source72 type=-1.
bool ApplyNativeCannon268SplashDamage(Projectile* projectile,Zombie* target,int distributedDamage);
// Payload80 is a source MidBomb, never ordinary melon distributed splash.
bool ApplyNativeCannon268LittleGoldImpact(Projectile* projectile);
// After shared Lobber motion; source child0 Quaternion.Slerp(dt*15).
bool UpdateNativeCannon268LittleGoldRotation(Projectile* projectile);
// After the existing mDoSpecialCountdown decrement, independently of main shot.
bool DispatchNativeCannon268SubEvents(Plant* plant);
bool GetNativeCannon268SubVisualPose(const Plant* plant,const char*& state,float& seconds);
bool IsNativeCannon268Kernel(const Projectile* projectile);
bool GetNativeCannon268KernelRect(const Projectile* projectile,Sexy::Rect& rect);
bool MoveNativeCannon268Kernel(Projectile* projectile);
bool ApplyNativeCannon268KernelImpact(Projectile* projectile,Zombie* target);
// Exact source kinds65/68/69/73/83/84, way14 and native Cob carrier only.
bool IsNativeCannon268Projectile(const Projectile* projectile);
bool MoveNativeCannon268Projectile(Projectile* projectile);
bool GetNativeCannon268ProjectileCenter(const Projectile* projectile,float& x,float& y);
}
#endif
