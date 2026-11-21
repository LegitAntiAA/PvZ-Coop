#ifndef LAWN_FUSION_NATIVE_WORLD_REMAINDER269_H
#define LAWN_FUSION_NATIVE_WORLD_REMAINDER269_H
#include "FusionWorldRemainder269Rules.h"
#include <vector>
class Board;class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
struct NativeWorldRemainder269Reference {uint16_t id=0;uint32_t token=0;};
struct NativeWorldRemainder269Pending {int deadline=0;uint8_t type=0;};
struct NativeWorldRemainder269BoardState
{
    bool initialized=false;
    int passiveDeadline=0,activeDeadline=0,ultimateDeadline=0;
    std::vector<NativeWorldRemainder269Pending> pending;
};
enum class NativeWorldRemainder269Family:uint8_t {LourFly,Meteorite};
struct NativeWorldRemainder269AuxState
{
    NativeWorldRemainder269Family family=NativeWorldRemainder269Family::LourFly;
    uint32_t instanceToken=0;NativeWorldRemainder269Reference parent;
    float targetX=0,targetY=0; // Fixed source targetPosition/minY in Board coordinates.
    int parentDamage=0; // Source managed owner.damage survives Unity destruction.
    int ageTicks=0,nextShotAge=0; // age mirrors real Projectile.mProjectileAge.
    uint8_t shots=0,type=0;bool ready=false,sun=false,impacted=false;
};
struct NativeWorldRemainder269ShotState
{NativeWorldRemainder269Reference parent;int ageTicks=0;};
NativeWorldRemainder269BoardState* EditNativeWorldRemainder269BoardState(Board*,bool create=false);
const NativeWorldRemainder269BoardState* GetNativeWorldRemainder269BoardState(const Board*);
NativeWorldRemainder269AuxState* EditNativeWorldRemainder269AuxState(Projectile*,bool create=false);
const NativeWorldRemainder269AuxState* GetNativeWorldRemainder269AuxState(const Projectile*);
NativeWorldRemainder269ShotState* EditNativeWorldRemainder269ShotState(Projectile*,bool create=false);
const NativeWorldRemainder269ShotState* GetNativeWorldRemainder269ShotState(const Projectile*);
// Root owns actual assets/typed AUX renderer/FX49,51,98 and Solar existence.
bool EnsureNativeWorldRemainder269Assets(Board*,NativeWorldRemainder269Family,uint8_t type);
bool QueryNativeWorldRemainder269Solar(const Board*,bool& present);
bool CreateNativeWorldRemainder269ImpactEffect(Board*,uint16_t sourceEffect,float x,float y,int row);
// Source item0: actual native Sun coins with the source random birth offsets.
bool EmitNativeWorldRemainder269MeteoriteSun(Board*,float x,float y,int count);
bool GetNativeWorldRemainder269AuxNodePoint(const Projectile*,const char*,float& x,float& y);
enum class NativeWorldRemainder269Trait:uint8_t {Ultimate8,Ultimate9,Advanced19,Advanced34,Advanced35,Advanced45,Debuff19};
bool HasNativeWorldRemainder269Trait(const Board*,NativeWorldRemainder269Trait);
bool NativeWorldRemainder269ShootingBoard(const Board*);
// Proven source Bullet.TrackUpdate/GetNearestZombie for movingWay6, implemented
// by this module; raw target is the real native Projectile.mTargetZombieID.
bool MoveNativeWorldRemainder269GenericHoming(Projectile*,float sourceTrackSpeed);
// Actual source geometry supplied by the registered source91 package, not the
// native SPIKE carrier rectangle. Collision dispatch may call the impact hook.
bool IsNativeWorldRemainder269Plant(const Plant*);
bool InitializeNativeWorldRemainder269Plant(Plant*);
bool UpdateNativeWorldRemainder269Plant(Plant*);
void UpdateNativeWorldRemainder269PlantPhase(Plant*);
bool DispatchNativeWorldRemainder269Shot(Plant*);
bool GetNativeWorldRemainder269VisualPose(const Plant*,const char*& base,float& seconds,const char*& additive,float& additiveSeconds);
bool GetNativeWorldRemainder269SummonPose(const Plant*,const char*& state,float& seconds);
bool InitializeNativeWorldRemainder269Board(Board*);
void UpdateNativeWorldRemainder269Board(Board*);
bool ActivateNativeWorldRemainder269Meteorite(Board*);
Projectile* CreateNativeWorldRemainder269Meteorite(Board*,uint8_t type);
bool SummonNativeWorldRemainder269LourFly(Plant*);
bool IsNativeWorldRemainder269Aux(const Projectile*);
bool UpdateNativeWorldRemainder269Aux(Projectile*);
bool MoveNativeWorldRemainder269Source91(Projectile*);
bool ApplyNativeWorldRemainder269Source91Impact(Projectile*,Zombie*);
bool NativeWorldRemainder269BoardStateValid(const NativeWorldRemainder269BoardState&);
bool NativeWorldRemainder269AuxStateValid(const NativeWorldRemainder269AuxState&);
bool NativeWorldRemainder269ShotStateValid(const NativeWorldRemainder269ShotState&);
}
#endif
