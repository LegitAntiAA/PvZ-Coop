#ifndef LAWN_FUSION_NATIVE_LAST_PLANTS269_H
#define LAWN_FUSION_NATIVE_LAST_PLANTS269_H
#include "FusionLastPlants269Rules.h"
#include "../../SexyAppFramework/Rect.h"
class Board;class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
// Root binds real source mode/Travel state; unavailable factories remain gates.
bool NativeLastPlants269Advanced(const Plant*,unsigned trait);
bool NativeLastPlants269Shooting(const Board*);
bool NativeLastPlants269Balanced(const Board*);
bool NativeLastPlants269EmperorFastStart(const Board*);
bool NativeLastPlants269GoldSummoned(const Board*);
void MarkNativeLastPlants269GoldSummoned(Board*);
void NotifyNativeLastPlants269GoldAlreadySummoned(Plant*);
// Exact source awardLevel2/1, then Board.SetHealthInTravel; preflight is pure.
bool CanApplyNativeLastPlants269GoldPost(Board*,uint16_t npcKind);
void ApplyNativeLastPlants269GoldPost(Zombie*,int awardLevel);
// Original source11 birth at Shadow+Y.5. Ensure before actor/flag consumption.
bool EnsureNativeLastPlants269BirthEffect(Board*);
void CreateNativeLastPlants269BirthEffect(Board*,float x,float y,int row);
// Existing native fire conversion, preserving source kind/way/full generation.
// counted follows the source success return (SnowPea's void callback counts).
// false means no commit; true may set counted=false for an ordinary no-op.
bool TryConvertNativeLastPlants269Torch(Plant*,Projectile*,bool& counted);
// Includes actual source1156 world-spawn provenance admission, plus source30
// factory/metadata/impact/resources. Mere model presence is insufficient.
bool EnsureNativeLastPlants269TorchConversions(Board*);
// Wheat is the shared source SetEvePlants chain, NOT a filtered native pool.
// Root's transaction retires reason1, creates FX11 and exact source Eve actor,
// marks the replacement isFromWheat and preserves that generic lifecycle.
bool NativeLastPlants269WheatFastStart(const Board*);
bool NativeLastPlants269WheatClockRunning(const Plant*);
bool CanReplaceNativeLastPlants269Wheat(const Plant*);
bool ReplaceNativeLastPlants269Wheat(Plant*);
// Full original Emperor roster/terrain/resources before paid placement or RNG.
// This is factory admission, not runtime verification of its NPC outcomes.
bool CanPlaceNativeEmperor269(Board*,int row);
bool CanCreateNativeLastPlants269(Plant*);
bool IsNativeLastPlants269(const Plant*);
bool InitializeNativeLastPlants269(Plant*);
bool UpdateNativeLastPlants269(Plant*);
void UpdateNativeLastPlants269Phase(Plant*);
bool DispatchNativeLastPlants269Events(Plant*);
bool HandleNativeLastPlants269Eaten(Zombie*,Plant*);
void NativeLastPlants269Removing(Plant*,int sourceReason);
bool GetNativeLastPlants269VisualPose(const Plant*,const char*& state,float& seconds);
// Real trigger-enter hook; Update also scans current native collider contacts.
bool HandleNativeLastPlants269TorchContact(Plant*,Projectile*);
// Source30 real Bullet (native PEA carrier), STRAIGHT0/STAR2, damage40,
// fireState1 + exact nonzero source torch stamp; unrelated extras/flags zero.
Projectile* CreateNativeLastPlants269FireKelp(Plant*,Projectile* original29);
bool IsNativeLastPlants269KelpProjectile(const Projectile*);
bool MoveNativeLastPlants269KelpProjectile(Projectile*);
bool ApplyNativeLastPlants269KelpImpact(Projectile*,Zombie* primary);
// Calls use exact source collider data from the strict source30 export.
bool GetNativeLastPlants269KelpProjectileRect(const Projectile*,Sexy::Rect&);
}
#endif
