#ifndef LAWN_FUSION_NATIVE_WORLD_ACTORS269_H
#define LAWN_FUSION_NATIVE_WORLD_ACTORS269_H
#include "FusionWorldActors269Rules.h"
#include "FusionShooterTiming.h"
#include <array>
class Board;class Plant;class Projectile;class Zombie;
namespace Sexy {class Graphics;}
namespace LawnFusion
{
struct NativeWorld269PlantState
{
    std::array<FusionWorld269Reference,6> zombies{};
    std::array<FusionWorld269Reference,2> plants{};
    FusionWorld269Reference receiver{},connectedPlant{};
    int energy=0,maximumEnergy=0,contributors=0;
    bool connected=false,main=false;
    bool connectedAttackBonus369=false; // Source modifier1 sampled on Connected, not at buff award.
    int explosionDeadline=0; // Absolute real Board tick; no independent clock.
};
struct NativeWorld269BeamState
{
    uint32_t instanceToken=0;
    uint16_t ownerId=0;uint32_t ownerToken=0;
    float startX=0,startY=0,endX=0,endY=0,width=.2f;
    int sortingRow=0;
    int ageTicks=0; // Snapshot mirror of actual Projectile.mProjectileAge.
};
NativeWorld269PlantState* EditNativeWorld269PlantState(Plant*,bool create=false);
const NativeWorld269PlantState* GetNativeWorld269PlantState(const Plant*);
const NativeWorld269BeamState* GetNativeWorld269BeamState(const Projectile*);
// Shared AUX factory: actual Projectile + typed metadata, never BulletID alias.
Projectile* CreateNativeWorld269BeamCarrier(Board*,const NativeWorld269BeamState&);
// Actual source99 endpoint /100 connected explosion; preflight precedes damage.
bool CanCreateNativeWorld269Effects(const Plant*,bool connectedExplosion);
bool CreateNativeWorld269Effect(Plant*,unsigned sourceParticle,float x,float y,int row);
// Source ballPrefab + two connecting lines must exist before connection commits.
bool CreateNativeLaser269Connection(Plant*,Plant*);
void RemoveNativeLaser269Connection(Plant*);
// Root provides the canonical heal/stage/death-blocker path used by companions.
bool RecoverNativeCompanions269Plant(Plant*,int amount);
enum class NativeWorld269Trait:uint8_t {Advanced46,Advanced47};
bool HasNativeWorld269Trait(const Plant*,NativeWorld269Trait);
bool IsNativeWorld269Plant(const Plant*);
const FusionShooterTimingProfile* GetNativeWorld269Timing(const Plant*);
bool InitializeNativeWorld269Plant(Plant*);
// Called in native UpdateShooter chain after mStateCountdown--, once per tick.
// Skip vanilla DoSpecial for this family; energy uses its native counter below.
bool UpdateNativeWorld269Plant(Plant*);
void UpdateNativeWorld269PlantPhase(Plant*);
bool DispatchNativeWorld269Shot(Plant*);
bool ConnectNativeLaser269(Plant*,Plant*);
void DisconnectNativeLaser269(Plant*);
// Canonical source TakeDamage override for Lantern/Laser armor-energy.
bool AdjustNativeLaser269Damage(Plant*,int& incomingDamage);
bool GetNativeWorld269VisualPose(const Plant*,const char*& state,float& seconds);
bool GetNativeLaser269Segment(const Plant*,unsigned index,float& startX,float& startY,float& endX,float& endY,int& sortingRow);
bool IsNativeWorld269Beam(const Projectile*);
bool UpdateNativeWorld269Beam(Projectile*);
bool DrawNativeWorld269Beam(Sexy::Graphics*,Projectile*);
}
#endif
