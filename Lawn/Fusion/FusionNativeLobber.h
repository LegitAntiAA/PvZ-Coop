#ifndef LAWN_FUSION_NATIVE_LOBBER_H
#define LAWN_FUSION_NATIVE_LOBBER_H
#include "FusionLobberRules.h"
#include "../../SexyAppFramework/Rect.h"
class Plant;
class Board;
class Projectile;
class Zombie;
namespace LawnFusion
{
// Shared real Projectile factory for source child volleys and subcontrollers.
Projectile* EmitNativeLobberChild(Board* board,int row,float baseline,const FusionLobberLaunch& launch,
    uint16_t kind,int damage,bool enhanced,uint8_t variant=0);
struct FusionShooterTimingProfile;
bool IsNativeLobber(const Plant* plant);
bool TryActivateNativeGoldCabbage(Plant* plant);
void FireNativeGoldCabbageVolley(Plant* plant);
const FusionShooterTimingProfile* GetNativeLobberTimingProfile(const Plant* plant);
void InitializeNativeLobber(Plant* plant);
bool ResetNativeLobberLaunchCounter(Plant* plant);
void BeginNativeLobberShot(Plant* plant, Zombie* target);
void UpdateNativeLobberPhase(Plant* plant);
Zombie* FindNativeLobberTarget(Plant* plant, int row);
bool FireNativeLobberProjectile(Plant* plant);
bool IsNativeLobberProjectile(const Projectile* projectile);
bool GetNativeLobberProjectileCenter(const Projectile* projectile, float& x, float& y);
bool GetNativeLobberProjectileRect(const Projectile* projectile, Sexy::Rect& rect);
bool UpdateNativeLobberMotion(Projectile* projectile);
bool ApplyNativeLobberImpact(Projectile* projectile, Zombie* target);
// One existing native vertical axis: legacy heightVelZ or source-planarVelY.
// Changing lanes preserves the world root while rebasing its ground shadow.
bool SetNativeLobberRetarget(Projectile* projectile,int targetRow,float velocityX,float downwardVelocity,float gravity);
void NativeLobberPlantRemoved(Plant* plant);
}
#endif
