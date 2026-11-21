#ifndef LAWN_FUSION_NATIVE_HYPNO269_H
#define LAWN_FUSION_NATIVE_HYPNO269_H
#include "FusionSummon269Rules.h"
#include "../../SexyAppFramework/Rect.h"
#include <array>
class Plant;class Projectile;class Zombie;class Coin;
namespace LawnFusion
{
// Root resolves existing level/Travel traits. Normal D2 defaults are level0
// and false; neither path requires a source NPC factory.
int GetNativeHypno269PlantLevel(const Plant*);
bool HasNativeHypno269Ultimate(const Plant*,unsigned sourceTrait);
bool HasNativeHypno269ProjectileUltimate(const Projectile*,unsigned sourceTrait);
bool IsNativeHypno269Plant(const Plant*);
const FusionShooterTimingProfile* GetNativeHypno269Timing(const Plant*);
bool InitializeNativeHypno269Plant(Plant*);
bool UpdateNativeHypno269Plant(Plant*);
// Once after existing native mShootingCounter/mStateCountdown decrement.
// Do not also run legacy shooter phase or stock mushroom launch logic.
void UpdateNativeHypno269Phase(Plant*);
bool DispatchNativeHypno269Shot(Plant*);
bool CanUseNativeHypno269Item(const Plant*);
// Real item is consumed only after validation; null is the source's valid
// programmatic UseItem path. UI equipment routing supplies the actual Coin.
bool TryUseNativeHypno269Item(Plant*,Coin* item=nullptr);
struct NativeHypno269Layer {const char* state=nullptr;float seconds=0;int layer=0;};
// Base idle is layer0. Extra layers: active shot1, charge1 magnet2,
// charge2 magnet3. Both magnet states are named "magnet" in their layers.
bool GetNativeHypno269VisualLayers(const Plant*,std::array<NativeHypno269Layer,3>& layers,int& count);
bool GetNativeHypno269ChargeNodes(const Plant*,bool& first,bool& second);
bool IsNativeHypno269Projectile(const Projectile*);
bool MoveNativeHypno269Projectile(Projectile*);
bool GetNativeHypno269ProjectileRect(const Projectile*,Sexy::Rect&);
bool NativeHypno269ProjectileCanHit(const Projectile*,Zombie*);
bool HandleNativeHypno269ProjectileCollision(Projectile*);
// nullptr is actual land impact. HitZombie's base virtual is sound, not damage;
// the area pass includes the primary and applies NormalAll1 exactly once.
bool ApplyNativeHypno269ProjectileImpact(Projectile*,Zombie*);
}
#endif
