#ifndef LAWN_FUSION_NATIVE_SHOOTER_267_H
#define LAWN_FUSION_NATIVE_SHOOTER_267_H
#include "FusionShooter267Rules.h"
#include "../../SexyAppFramework/Rect.h"
class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
bool IsNativeShooter267(const Plant* plant);
const FusionShooterTimingProfile* GetNativeShooter267Timing(const Plant* plant);
void InitializeNativeShooter267(Plant* plant);
bool UpdateNativeShooter267(Plant* plant);
void UpdateNativeShooter267Phase(Plant* plant);
bool DispatchNativeShooter267Shot(Plant* plant);
bool GetNativeShooter267VisualPose(const Plant* plant,const char*& state,float& elapsed);
void NativeShooter267PlantRemoving(Plant* plant,int sourceReason);
bool IsNativeShooter267Projectile(const Projectile* projectile);
bool MoveNativeShooter267Projectile(Projectile* projectile);
bool GetNativeShooter267ProjectileRect(const Projectile* projectile,Sexy::Rect& rect);
bool NativeShooter267ProjectileCanHit(const Projectile* projectile,Zombie* zombie);
bool HandleNativeShooter267ProjectileCollision(Projectile* projectile);
bool ApplyNativeShooter267ProjectileImpact(Projectile* projectile,Zombie* zombie);
}
#endif
