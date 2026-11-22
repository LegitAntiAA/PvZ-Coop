#ifndef LAWN_FUSION_NATIVE_SPECIAL_ZOMBIE269_H
#define LAWN_FUSION_NATIVE_SPECIAL_ZOMBIE269_H
#include "SpecialZombie269Rules.h"
class Zombie;class Plant;class Projectile;
namespace LawnFusion
{
bool IsNativeSpecialZombie269(const Zombie*);
// Invoke AFTER a real typed factory attaches source identity/HP/armor. This
// sets native paper phase/submarine water state, never advertises admission.
bool InitializeNativeSpecialZombie269(Zombie*);
// Early UpdateActions and UpdateAnimSpeed hooks; the existing body Reanimation
// is the only paper transition clock. Host mutation/client read-only pose.
bool UpdateNativeSpecialZombie269(Zombie*);
bool UpdateNativeSpecialZombie269Animation(Zombie*);
bool BeginNativeSpecialZombie269PaperFall(Zombie*);
bool GetNativeSpecialZombie269Presentation(const Zombie*,const char*& clip,float& normalizedPhase);
bool GetNativeSpecialZombie269BodyStage(const Zombie*,uint8_t& stage);
bool GetNativeSpecialZombie269Status(const Zombie*,int& sourceStatus);
// Root calls at the end of actual Zombie.PlayDeathAnim, after selecting its
// real native frame span; source5/6/7/9 clocks replace only their carrier rate.
bool ApplyNativeSourceZombieDeathClock284(Zombie*);
// Root calls first in Zombie.UpdateDeath. Actual source fall/end callbacks,
// without stock corpse fade, a second award or an independently ticked clock.
bool UpdateNativeSourceZombieDeath284(Zombie*);
bool StartNativeSpecialZombie269Walk(Zombie*,int blendTicks);
bool MoveNativeSpecialZombie269Paper(Zombie*);
bool UpdateNativeSpecialZombie269PaperAttack(Zombie*);
// Reuses the real source target ID/full generation and native body Reanimation
// cursors; call before generic source walkers/bites, exactly once per host tick.
using NativeSpecialZombie269NodeQuery=bool(*)(const Zombie*,const char*,float&,float&);
bool InitializeNativeSpecialZombie269PaperAddon(Zombie*);
bool UpdateNativeSpecialZombie269PaperShoot(Zombie*,NativeSpecialZombie269NodeQuery);
Projectile* CreateNativeSpecialZombie269PaperShot(Zombie*,NativeSpecialZombie269NodeQuery);
bool GetNativeSpecialZombie269AddonPose(const Zombie*,const char*& clip,float& normalizedPhase);
void NativeSpecialZombie269Removing(Zombie*);
// Source scalar operands must come from their real graph, not damageStage.
bool ApplyNativeSpecialZombie269CrashPlant(Zombie*,Plant*,bool sourceCaltrop,bool sourceDefended,int sourceDefense);
// Root's source damage4 adapter is required: squash11 is NOT this operation.
bool ApplyNativeSpecialZombie269CrashZombie(Zombie*,Zombie*);
bool ApplyNativeSpecialZombie269VehicleDamage4(Zombie* target,int damage);
// Maps the native Zamboni stop mechanism, retaining source half-health first.
bool HandleNativeSpecialZombie269DriverCaltrop(Zombie*);
// Genuine model child4/Board ice boundary and currentSpeed are supplied by the
// controller owner. Legacy UpdateZamboni's .25/.05 speed law is NOT reused.
bool MoveNativeSpecialZombie269Driver(Zombie*,float sourceCurrentSpeed,bool behindIceBoundary);

// Transport270 controller: one real Zombie and its existing body Reanimation.
// Node query paths are source paths: root="", Shadow="Shadow"; source18
// GetChild(4)="Zombie_zamboni_wheel_1", inherited ice child2="Zombie_zamboni_4".
using NativeSpecialZombie269PlantTraits=bool(*)(const Plant*,bool& isCaltrop,bool& isNut,int& sourceLevel);
using NativeSpecialZombie269VehicleFx=bool(*)(Zombie*,uint16_t sourceParticle,float x,float y);
bool IsNativeSpecialZombie269Vehicle(const Zombie*);
// Source Start sets inWater, independently of actual native row/mInPool.
bool NativeSubmarine343SourceInWater(const Zombie*);
bool NativeSpecialZombie269VehicleTerrainValid(const Zombie*);
bool NativeSpecialZombie269VehicleColliderEnabled(const Zombie*);
bool StartNativeSpecialZombie269Vehicle(Zombie*);
bool UpdateNativeSpecialZombie269VehicleAnimation(Zombie*);
bool MoveNativeSpecialZombie269Vehicle(Zombie*,NativeSpecialZombie269NodeQuery);
bool UpdateNativeSpecialZombie269VehicleContacts(Zombie*,NativeSpecialZombie269NodeQuery,NativeSpecialZombie269PlantTraits);
// Call before stock TakeBodyDamage/limbs/ZamboniDeath; true consumes its entire
// source vehicle override, not only the mBodyHealth subtraction.
bool ApplyNativeSpecialZombie269VehicleBodyDamage(Zombie*,int damage,NativeSpecialZombie269VehicleFx);
// Call once per native Zombie.Update BEFORE its IsImmobilizied action gate.
// mZombieAge owns the2s deadline; no counter is ticked here or on the ghost.
bool UpdateNativeSpecialZombie269VehiclePendingDeath(Zombie*,NativeSpecialZombie269VehicleFx);
bool DieNativeSpecialZombie269Vehicle(Zombie*,NativeSpecialZombie269VehicleFx);
// Only from an actual source CreateIceRoad event, never a per-tick native
// Zamboni call. Current source18 idle/die/shake clips contain no such event.
bool CreateNativeSpecialZombie269IceRoad(Zombie*,NativeSpecialZombie269NodeQuery);
// Exact source14 polygon / source18 box. Root may use its bounds for broad
// phase, but incoming/contact precision uses this narrow intersection query.
bool NativeSpecialZombie269VehicleTouchesRect(const Zombie*,NativeSpecialZombie269NodeQuery,
    double left,double top,double width,double height);

}
#endif
