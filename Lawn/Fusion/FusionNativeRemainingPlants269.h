#ifndef LAWN_FUSION_NATIVE_REMAINING_PLANTS269_H
#define LAWN_FUSION_NATIVE_REMAINING_PLANTS269_H
#include "FusionRemainingPlants269Rules.h"
class Board;class Plant;class Zombie;class Projectile;
namespace LawnFusion
{
// Root implements these narrow source leaf bridges on real native Coin/items;
// false admission preserves the source actor and never invents a seed alias.
bool CanCreateNativeRemaining269DroppedCard(Board*,uint16_t kind);
bool CreateNativeRemaining269DroppedCard(Plant*,uint16_t kind,float x,float y);
bool GetNativeRemaining269PlantTags(const Plant*,bool& flying,bool& pumpkin,bool& pot,bool& lily);
bool CreateNativeRemaining269Fertilize(Plant*,float x,float y);
bool IsNativeRemaining269Plant(const Plant*);
bool InitializeNativeRemaining269Plant(Plant*);
bool UpdateNativeRemaining269Plant(Plant*);
void UpdateNativeRemaining269Phase(Plant*);
bool DispatchNativeRemaining269Events(Plant*);
void NativeRemaining269PlantRemoving(Plant*,int reason);
void NativeRemaining269ZombieRemoving(Zombie*);
bool GetNativeRemaining269VisualPose(const Plant*,const char*& state,float& seconds);
bool GetNativeSeaShroom269GrabPose(const Plant*,const char*& state,float& seconds);
bool GetNativeSeaShroom269GrabAnchor(const Plant*,float& x,float& y);
bool GetNativeRemaining269VisualLayer(const Plant*,unsigned layer,const char*& state,float& seconds);
bool NativeRemaining269Invulnerable(const Plant*);
uint16_t NativeRemaining269SnowGatlingPayload(const Plant*);
bool CapNativeRemaining269Damage(const Plant*,int& damage);
void NativeRemaining269HealthChanged(Plant*);
bool TryUpgradeNativeHamburger269(Plant*,uint16_t donorKind);
// Exact BigChomper leaf/range extension to the existing native Chomper clock;
// Root must route1174 through its real bite/digest controller, not an idle nut.
bool GetNativeBigChomper269Range(const Plant*,bool area,float& x,float& y,float& width,float& height);
bool ApplyNativeBigChomper269Bite(Plant*,Zombie* primary);
}
#endif
