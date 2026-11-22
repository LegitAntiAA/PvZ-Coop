#pragma once
#include "FusionSourceObstacle313.h"
#include "FusionNativeJackbox300.h"
#include <string>
class Plant;class Zombie;class Projectile;class Board;
namespace LawnFusion
{
struct NativeDrownWeapon313Hooks
{
    bool (*plantTraits)(const Plant*,bool& invincible,bool& pumpkin,bool& flying,bool& pot)=nullptr;
    bool (*plantShadow)(Plant*,float& x,float& y)=nullptr;
    bool (*prepare)(Board*,const FusionSourceObstacle313Spawn&,std::string*)=nullptr;
    SpawnSourceObstacle313 spawn=nullptr;
    // Actual source weapon rotated BoxCollider2D against source plant colliders.
    // Includes actual enabled/contactCaptureLayers, not a generic pea rectangle.
    bool (*touches)(Projectile*,Plant*)=nullptr;
    // Mouse.GetLandY == GetBoxYFromRow for this nonRoof capability; Root
    // projects that row ground through the same native source world transform.
    bool (*groundY)(Projectile*,float& nativeY)=nullptr;
    void (*sound)(Zombie*,uint16_t,float)=nullptr;
    int (*randomInt)(Board*,int minimum,int exclusiveMaximum)=nullptr;
    float (*randomFloat)(Board*,float minimum,float maximum)=nullptr;
    bool (*touchesBullet)(Projectile*,Projectile*)=nullptr;
    bool (*strongerBoard)(const Board*)=nullptr;
    bool (*bigExplosionReady)(Board*,const NativeJackbox300Explosion&,std::string*)=nullptr;
    bool (*bigExplosion)(Board*,const NativeJackbox300Explosion&)=nullptr;
};
bool ThrowNativeDrownWeapon313(Zombie*,const NativeDrownWeapon313Hooks&);
//216 real AnimShoot: whole14-P/model preflight precedes all28 jitter draws.
bool CanSpawnNativeDrownVolley343(Zombie*,float shootX,float shootY,const NativeDrownWeapon313Hooks&,std::string* error=nullptr);
bool SpawnNativeDrownVolley343(Zombie*,float shootX,float shootY,const NativeDrownWeapon313Hooks&);
// Root common typed-obstacle dispatcher advances Projectile.mProjectileAge
// exactly once before this call. Ghosts adopt native fields and never tick.
bool UpdateNativeDrownWeapon313(Projectile*,FusionSourceObstacle313View&,const NativeDrownWeapon313Hooks&);
}
