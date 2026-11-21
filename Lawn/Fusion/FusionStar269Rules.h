#ifndef LAWN_FUSION_STAR269_RULES_H
#define LAWN_FUSION_STAR269_RULES_H
#include <cstddef>
#include <cstdint>
#include "FusionShooterTiming.h"
namespace LawnFusion
{
struct FusionStar269Profile
{
    uint16_t kind,projectile;
    uint16_t storageSlots,capacity,pumpkinCapacity;
    float storageSeconds;
    int dispatchDamageMultiplier;
};
const FusionStar269Profile* FindFusionStar269Profile(uint16_t kind);
bool FusionStar269ProjectileClass(uint16_t kind);
bool FusionStar269CanHitStatus(uint8_t sourceWay,int sourceStatus);
const FusionShooterTimingProfile* FindFusionStar269TimingProfile(uint16_t kind);
bool FusionStar269PlantTakesDamage(uint16_t kind);
int FusionStar269Capacity(uint16_t kind,bool sameCellStarPumpkin);
// Array entries are references to real native Projectile generations. A live
// capacity reduction never destroys references beyond the new admission limit.
bool TryFindFusionStar269FreeSlot(uint16_t kind,bool sameCellStarPumpkin,
    const uint32_t* references,std::size_t count,std::size_t& index);
bool FusionStar269CanCapture(bool attracting,bool starComponent,bool accelerated,uint8_t way,bool inRadius3);
bool TryAdvanceFusionStar269Hold(uint16_t kind,float elapsed,float delta,float& after,bool& release);
struct FusionStar269Dispatch {int damage=0;float trackSpeed=0;uint8_t way=6;bool colliderEnabled=true;};
bool TryMakeFusionStar269Dispatch(uint16_t kind,bool ownerDeath,int damage,float trackSpeed,FusionStar269Dispatch& result);
struct FusionStar269Point {float x=0,y=0,z=0;};
bool FusionStar269CaptureTouches(FusionStar269Point center,FusionStar269Point projectile,float rootAngle);
bool TryAdvanceFusionStar269Approach(FusionStar269Point center,FusionStar269Point position,float delta,
    FusionStar269Point& after,bool& movingToCircle,float& angle);
bool TryAdvanceFusionStar269Orbit(FusionStar269Point center,float delta,float& angularSpeed,float& angle,FusionStar269Point& after);
// Caller supplies the real source Collider2D-centre projection and the source
// eligibility predicate. Strict distance comparison keeps the first tie.
struct FusionStar269Target {bool eligible=false,flying=false;float x=0,y=0;};
bool TrySelectFusionStar269Target(const FusionStar269Target* candidates,std::size_t count,
    float x,float y,bool flyingPriority,std::size_t& index);
bool TryMakeFusionStar269HomingVelocity(bool hasTarget,bool validTarget,float x,float y,float targetX,float targetY,
    float trackSpeed,float& velocityX,float& velocityY,bool& colliderEnabled,bool& reacquire);
bool FusionStar269CactusCanAcquire(int status,bool mindControlled,bool sameRow,float shadowX,float zombieX,float visionMax);
bool FusionStar269StartupBlowTarget(int status);
bool FusionStar269MeteoriteDue(int light,float boardCountdown);
struct FusionStar269Shot {uint16_t projectile=0;uint8_t special=0;int damage=0;bool requestMeteorite=false;};
bool TryMakeFusionStar269Shot(uint16_t kind,int light,int magnetCount,int sourceDamage,FusionStar269Shot& shot);
// Jackbox area uses source Explode10. Bullet field0x114 is zombieLayer, not a
// damage type; SmallBomb passes that LayerMask into the shared Bomb overlap.
struct FusionStar269Hit {int damage=0;uint8_t sourceDamageType=0;float radius=0;bool area=false;};
bool TryMakeFusionStar269Hit(uint16_t projectile,uint8_t way,int damage,int firstArmor,int secondArmor,FusionStar269Hit& hit);
// Actual UltimateStarBullet leaf; class-to-payload binding is separately audited.
bool TryAdvanceFusionUltimateStar269Status(int zombieKind,int before,int& after,bool& breakObject);
bool FusionStar269HomingOutsideX(float x,float minimum,float maximum);
}
#endif
