#ifndef LAWN_FUSION_NATIVE_CHOMPER268_H
#define LAWN_FUSION_NATIVE_CHOMPER268_H
#include <cstdint>
#include "../../SexyAppFramework/Rect.h"
class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
bool IsNativeChomper268Kind(uint16_t kind);
bool InitializeNativeChomper268Plant(Plant* plant);
bool UpdateNativeChomper268Plant(Plant* plant);
bool UpdateNativeChomper268Phase(Plant* plant);
bool DispatchNativeChomper268Events(Plant* plant);
bool ApplyNativeChomper268Special(Plant* plant);
bool TryCapNativeChomper268IncomingDamage(const Plant* plant,int incoming,int& damage);
void NativeChomper268HealthChanged(Plant* plant);
bool RecoverNativeChomper268(Plant* plant,int amount);
bool GetNativeChomper268VisualPose(const Plant* plant,const char*& state,float& seconds);
bool GetNativeShulk268HeadVisualPose(const Plant* plant,const char*& state,float& seconds);
bool IsNativeChomper268Projectile(const Projectile* projectile);
bool GetNativeChomper268ProjectileRect(const Projectile* projectile,Sexy::Rect& rect);
bool UpdateNativeChomper268Projectile(Projectile* projectile);
bool ApplyNativeChomper268ProjectileImpact(Projectile* projectile,Zombie* target);
}
#endif
