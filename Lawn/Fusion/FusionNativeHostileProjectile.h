#ifndef LAWN_FUSION_NATIVE_HOSTILE_PROJECTILE_H
#define LAWN_FUSION_NATIVE_HOSTILE_PROJECTILE_H

#include <cstdint>
class Board;
class Plant;
class Projectile;

namespace LawnFusion
{
struct NativeSuperCherryStart
{
    uint32_t targetGeneration=0;
    uint16_t targetId=0;
    int birthAge=0;
    bool plantContact=false;
};
Projectile* SpawnNativeSourcePlanarProjectile(Board* board,uint16_t kind,int row,int damage,
    float centerX,float centerY,float velocityX,float velocityY,float gravityY,bool isZombieBullet);
Projectile* TryMakeNativeHostileCatapult343(Board*,Plant*,int row,float shootX,float shootY,uint16_t bulletKind);
Projectile* TryMakeNativeHostileBasketball(Board* board,Plant* target,int row,
    float rawShootCenterX,float rawShootY);
// Contact captures the selected target full generation and commits a real
// BombCherry birth. Its Start continuation must not run inside this contact.
using NativeSuperCherryContact=bool(*)(Projectile*,Plant* selectedTarget,bool plantContact);
bool IsNativeSourceSuperCherryProjectile(const Projectile*);
bool UpdateNativeSourceSuperCherryProjectile(Projectile*,NativeSuperCherryContact);
// Existing carrier remains alive until its next native Update; no world clock.
bool QueueNativeSuperCherryStart(Projectile*,Plant* selectedTarget,bool plantContact);
bool UpdateNativeSuperCherryStart(Projectile*);
bool IsNativeSourcePlanarProjectile(const Projectile* projectile);
bool UpdateNativeSourcePlanarProjectile(Projectile* projectile);
}

#endif
