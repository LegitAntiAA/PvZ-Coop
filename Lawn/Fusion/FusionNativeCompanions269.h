#ifndef LAWN_FUSION_NATIVE_COMPANIONS269_H
#define LAWN_FUSION_NATIVE_COMPANIONS269_H
#include "FusionCompanions269Rules.h"
#include <vector>
class Board;class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
struct NativeCompanion269Reference {uint16_t id=0;uint32_t token=0;};
struct NativeCompanion269State
{
    FusionCompanion269Family family=FusionCompanion269Family::LittleFly;
    uint32_t instanceToken=0;
    uint16_t parentId=0,targetId=0;
    uint32_t parentToken=0,targetToken=0;
    float offsetX=0,offsetY=0,positionOffset=0,shootCountdown=1;
    bool onTarget=false,smallVariant=false,shotEmitted=false;
    int ageTicks=0; // Snapshot mirror of real Projectile.mProjectileAge; never ticked separately.
    int shootStartAge=-1; // Native Projectile.mProjectileAge is the only age.
};
struct NativeCompanions269PlantState
{
    std::vector<NativeCompanion269Reference> children; // Actual actor references only.
    // Host-only source Invoke queue: absolute Board ticks, never a second clock.
    std::vector<int> recreateDeadlines;
};
// Root supplies full native-generation Context maps and authenticated FNS
// adoption. Client accessors never allocate or consume source RNG.
NativeCompanion269State* EditNativeCompanion269State(Projectile*,bool create=false);
const NativeCompanion269State* GetNativeCompanion269State(const Projectile*);
NativeCompanions269PlantState* EditNativeCompanions269PlantState(Plant*,bool create=false);
const NativeCompanions269PlantState* GetNativeCompanions269PlantState(const Plant*);
// Shared auxiliary factory: one real native Projectile, NO BulletID alias or
// NativeProjectileData. Root assigns the opaque instanceToken/full generation.
Projectile* CreateNativeCompanion269Carrier(Board*,NativeCompanion269State,int sourceRow,float centerX,float centerY);
bool CanCreateNativeCompanion269Carrier(Board*,FusionCompanion269Family);
bool GetNativeCompanion269NodePoint(const Projectile*,const char* node,float& centerX,float& centerY);
enum class NativeCompanions269Trait:uint8_t {Common5,Advanced4,Ultimate24,Ultimate25};
bool HasNativeCompanions269Trait(const Plant*,NativeCompanions269Trait);
bool NativeCompanions269ShootingBoard(const Board*);
bool ApplyNativeCompanion269CherryExplosion(Projectile*,int targetRow,int sourceVariant);
bool ApplyNativeCompanions269PlantCherryExplosion(Plant*,float centerX,float centerY,int row);
void NativeCompanions269BirthEffect(Plant* created,int sourceSortingRow);
// Caller supplies source strict sun>cost admission and the real native debit.
bool TrySpendNativeSuperSun269Sun(Plant*,int cost);
// Canonical source Recover: native health/stages plus existing heal blockers/FX.
bool RecoverNativeCompanions269Plant(Plant*,int amount);
// Existing real same-cell source next/pre graph; no whole-board replacement.
bool NativeCompanions269HasLayerNeighbour(const Plant*);
Plant* ReplaceNativeCompanions269Plant(Plant* old,uint16_t newKind);

bool IsNativeCompanions269Plant(const Plant*);
bool InitializeNativeCompanions269Plant(Plant*);
bool UpdateNativeCompanions269Plant(Plant*);
void UpdateNativeCompanions269PlantPhase(Plant*);
bool DispatchNativeCompanions269Events(Plant*);
void NativeCompanions269PlantMoved(Plant*);
void NativeCompanions269PlantRemoving(Plant*,int reason);
// true means source override handled; false continues canonical plant damage.
bool AdjustNativeCompanions269Damage(Plant*,int& damage,int sourceType);
bool TryActivateNativeSuperSun269(Plant*);
bool TrySupplementNativeCompanions269(Plant*);
// Called only after a proven actual Torch projectile contact, not prediction.
bool TrySummonNativeCherryTorch269(Plant*);
void NativeCherryTorch269ConversionCommitted(Plant*);
bool IsNativeCompanion269(const Projectile*);
bool UpdateNativeCompanion269(Projectile*);
void NativeCompanion269Removing(Projectile*,bool sourceDie);
bool GetNativeCompanion269VisualPose(const Projectile*,const char*& state,float& seconds);
bool GetNativeCompanion269ShadowCenter(const Projectile*,float& centerX,float& centerY);
bool IsNativeCompanion269Shot(const Projectile*);
bool UpdateNativeCompanion269Shot(Projectile*);
bool HandleNativeCompanion269ShotCollision(Projectile*);
bool NativeCompanion269StateValid(const NativeCompanion269State&);
bool NativeCompanions269PlantStateValid(uint16_t kind,const NativeCompanions269PlantState&);
}
#endif
