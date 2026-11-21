#ifndef LAWN_FUSION_NATIVE_WATER268_H
#define LAWN_FUSION_NATIVE_WATER268_H
#include <cstdint>
#include "../../SexyAppFramework/Rect.h"
class Board;class Plant;class Zombie;class Projectile;
namespace LawnFusion
{
// Admission is deliberately narrower than the source family catalog until
// every additional source controller/leaf has a production implementation.
bool IsNativeWater268ImplementedKind(uint16_t kind);
bool InitializeNativeWater268Plant(Plant* plant);
bool UpdateNativeWater268Plant(Plant* plant);
bool UpdateNativeWater268Phase(Plant* plant);
bool DispatchNativeWater268Events(Plant* plant);
bool ApplyNativeWater268Special(Plant* plant);
void NativeWater268PlantRemoving(Plant* plant,int sourceReason);
bool NativeWater268ZombieCaptured(const Zombie* zombie);
void NativeWater268ZombieRemoving(Zombie* zombie);
bool GetNativeWater268VisualPose(const Plant* plant,const char*& state,float& seconds);
bool GetNativeWater268GrabAnchor(const Plant* plant,float& x,float& y);
bool GetNativeWater268GrabVisualPose(const Plant* plant,const char*& state,float& seconds);
bool GetNativeWater268ShootingVisualPose(const Plant* plant,const char*& state,float& seconds);
const struct FusionShooterTimingProfile* GetNativeWater268Timing(const Plant* plant);
bool SetNativeWater268Grap(Zombie* zombie);
void TickNativeWater268ZombieState(Zombie* zombie);
float GetNativeWater268GrapSpeedFactor(const Zombie* zombie);
Projectile* SpawnNativeWater268Homing(Board* board,uint16_t kind,int row,int damage,
    float centerX,float centerY,float angleRadians);
bool IsNativeWater268Projectile(const Projectile* projectile);
bool GetNativeWater268ProjectileRect(const Projectile* projectile,Sexy::Rect& rect);
bool UpdateNativeWater268Projectile(Projectile* projectile);
bool ApplyNativeWater268ProjectileImpact(Projectile* projectile,Zombie* target);
}
#endif
