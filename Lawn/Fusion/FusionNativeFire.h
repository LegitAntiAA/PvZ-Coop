#ifndef LAWN_FUSION_NATIVE_FIRE_H
#define LAWN_FUSION_NATIVE_FIRE_H
class Board;
class Plant;
class Projectile;
class Zombie;
#include <cstdint>
namespace LawnFusion
{
bool HandleNativeTorchwood(Plant* plant);
// One already-observed physical contact. Policy must be the REAL source kind;
// success commits actual metadata/fresh allocation before retiring the input.
bool TryConvertNativeTorchProjectile(Plant*,Projectile*,uint16_t sourceTorchKind,bool& counted);
bool IsNativeHotProjectile(const Projectile* projectile);
bool ApplyNativeFireImpact(Projectile* projectile, Zombie* primary);
void NativeWarmAfterDamage(Zombie* target);
// Source Zombie.JalaedExplode: radius1 at source Shadow+Y.7, rows+/-1,
// excludes the source, shieldless damage, optional Warm after each hit.
void ApplyNativeFireSourceExplosion(Zombie* source,int damage = 100,bool warm = true);
// Source BombCherry.Explode: one heal per overlapping CherryNut collider.
void HealNativeCherryNutFromCircle(Board* board,double centerX,double centerY,int row,
    int amount = 100,double radius = 1.5);
}
#endif
