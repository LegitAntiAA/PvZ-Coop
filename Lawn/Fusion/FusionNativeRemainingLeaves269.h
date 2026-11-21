#ifndef LAWN_FUSION_NATIVE_REMAINING_LEAVES269_H
#define LAWN_FUSION_NATIVE_REMAINING_LEAVES269_H
#include "FusionRemainingLeaves269Rules.h"
#include <cmath>
#include "../../SexyAppFramework/Rect.h"
class Plant;class Zombie;class Projectile;class Board;class Coin;
namespace LawnFusion
{
bool IsNativeRemainingLeaves269Plant(const Plant*);
bool InitializeNativeRemainingLeaves269Plant(Plant*);
bool UpdateNativeRemainingLeaves269Plant(Plant*);
void UpdateNativeRemainingLeaves269Phase(Plant*);
bool DispatchNativeRemainingLeaves269Events(Plant*);
bool GetNativeRemainingLeaves269VisualPose(const Plant*,const char*&,float&);
bool IsNativeRemainingLeaves269Projectile(const Projectile*);
bool GetNativeRemainingLeaves269ProjectileRect(const Projectile*,Sexy::Rect&);
bool MoveNativeRemainingLeaves269Projectile(Projectile*);
bool HandleNativeRemainingLeaves269Collision(Projectile*);
bool ApplyNativeRemainingLeaves269Impact(Projectile*,Zombie*);
void NativeRemainingLeaves269ProjectileRemoving(Projectile*);
enum class NativeFertilize269Phase:uint8_t {Falling,World,Cursor,Using};
struct NativeFertilize269Data
{
 uint32_t instanceToken=0;uint16_t targetId=0;uint32_t targetToken=0;
 NativeFertilize269Phase phase=NativeFertilize269Phase::Falling;uint8_t heldSlot=255;
 int phaseStartAge=0;float anchorX=0,anchorY=0;
};
inline bool NativeFertilize269DataValid(const NativeFertilize269Data& d)
{return d.instanceToken&&uint8_t(d.phase)<=3&&d.phaseStartAge>=0&&std::isfinite(d.anchorX)&&std::isfinite(d.anchorY)&&
 std::abs(d.anchorX)<=4096&&std::abs(d.anchorY)<=4096&&((d.targetId==0)==(d.targetToken==0))&&
 (d.phase==NativeFertilize269Phase::Cursor?d.heldSlot<4:d.heldSlot==255)&&
 (d.phase==NativeFertilize269Phase::Using||(!d.targetId&&!d.targetToken));}
NativeFertilize269Data* EditNativeFertilize269Data(Coin*,bool create=false);
const NativeFertilize269Data* GetNativeFertilize269Data(const Coin*);
// Exact Ferilize.prefab package preflight, never a vanilla Zen fertiliser icon.
bool EnsureNativeFertilize269Visuals(Board*);
void ReduceNativeFertilize269Attribute(Plant*,int maximumTicks);
bool CreateNativeRemaining269Fertilize(Plant*,float x,float y);
bool IsNativeFertilize269(const Coin*);
bool PickNativeFertilize269(Coin*,int authenticatedSlot);
bool UseNativeFertilize269(Coin*,int authenticatedSlot,int column,int row);
bool PutDownNativeFertilize269(Coin*,int authenticatedSlot);
// BEFORE stock CoinAge++; owns one real Coin age, never an auxiliary timer.
bool UpdateNativeFertilize269(Coin*);
void NativeFertilize269PlantRemoving(Plant*);
bool GetNativeFertilize269VisualPose(const Coin*,const char*&,float&);
}
#endif
