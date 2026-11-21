#ifndef LAWN_FUSION_NATIVE_SUMMON_PLANTS269_H
#define LAWN_FUSION_NATIVE_SUMMON_PLANTS269_H
#include "FusionSummonPlants269Rules.h"
#include "FusionEquipmentRules.h"
class Plant;class Zombie;class Coin;class Projectile;class Board;
namespace LawnFusion
{
bool IsNativeSummonPlants269(const Plant*);
bool InitializeNativeSummonPlants269(Plant*);
bool UpdateNativeSummonPlants269(Plant*);
void UpdateNativeSummonPlants269Phase(Plant*);
bool DispatchNativeSummonPlants269Events(Plant*);
bool GetNativeSummonPlants269VisualPose(const Plant*,const char*&,float&);
int NativeSummonPlants269VisualLayer(const Plant*);
bool CanUseNativeSummonPlants269Item(const Plant*,FusionEquipmentType,const Coin* item=nullptr);
bool TryUseNativeSummonPlants269Item(Plant*,FusionEquipmentType,Coin* item=nullptr);
// Real inherited Magnet capture completion calls this, without restarting its
// already running cooldown. Manual UseItem has distinct random launch velocity.
bool CommitNativeCherryMagnet269Capture(Plant*,Coin*);
bool NativeGoldMagnet269CanTrigger(const Plant* shooter);
bool HandleNativeSummonPlants269Eaten(Zombie*,Plant*);
bool ReceiveNativeHypnoNut269Color(Plant*,Projectile*);
Plant* NativeSummonPlants269EatingCover(Zombie*,Plant* actualTarget);
void NativeSummonPlants269Removing(Plant*,int reason);
// After Coin's existing age increment, before generic equipment update. A real
// Bucket body remains the same Coin; its destroyed Bucket component is modeled
// by data.weapon. Such a Coin must be rejected by equipment pickup/input.
bool UpdateNativeSummonPlants269Coin(Coin*);
bool IsNativeSummonPlants269Weapon(const Coin*);
void NativeSummonPlants269HealthChanged(Plant*);
// Gold capture keeps original sourcekind/collider/velocityX. Root dispatches
// movement first and performs ordinary collision afterward; no second age.
bool MoveNativeGoldMagnet269Projectile(Projectile*);
// All source BombCherry adapters call this for overlapping actual plant
// colliders: Cherry covers heal200 and protect same-cell inner recipients.
void ApplyNativeSummonPlants269CherryArea(Board*,float x,float y,int row);
// Board.CreateCherryExplode: friendly BombCherry, variant0 Charred/radius2;
// variant2 Explode300/radius1.5. Source particle slots2/14 use native FX here.
bool ApplyNativeSourceCherry269ExplosionAt(Board*,float x,float y,int row,int variant);
}
#endif
