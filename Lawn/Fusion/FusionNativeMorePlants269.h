#ifndef LAWN_FUSION_NATIVE_MORE_PLANTS269_H
#define LAWN_FUSION_NATIVE_MORE_PLANTS269_H
#include "FusionMorePlants269Rules.h"
class Plant;class Board;
namespace LawnFusion
{
// Narrow engine bridges: no callbacks that fabricate a second actor world.
// Preflight the complete weighted source roster, including all4 fire outcomes.
bool CanEmitNativeMore269HamburgerShot(const Board*,uint16_t kind,uint8_t fire);
bool EmitNativeMore269HamburgerShot(Plant*,const FusionHamburger269Shot&,float sourceX,float sourceY);
// Actual source TypeMgr table and native CellBox/boardTag flags, not RNG filters.
bool NativeMore269SourceWaterPlant(uint16_t kind);
bool NativeMore269PresentWaterCell(const Plant*);
bool NativeMore269PresentSuperRandom(const Plant*);
bool CanSpawnNativeMore269Plant(const Board*,uint16_t kind);
// Preserve Squash's actual native movement controller; it must call the two
// typed leaves below instead of ordinary1800+Die. No ordinarySquash alias.
bool UpdateNativeMore269SquashMotion(Plant*);
bool NativeMore269SqualourTravel(const Plant*);

bool IsNativeMorePlants269(const Plant*);
bool InitializeNativeMorePlants269(Plant*);
bool UpdateNativeMorePlants269(Plant*);
void UpdateNativeMorePlants269Phase(Plant*);
bool DispatchNativeMorePlants269Events(Plant*);
bool GetNativeMorePlants269VisualPose(const Plant*,const char*&,float&);
bool GetNativeHamburger269NodeActive(const Plant*,const char* path,bool& active);
// Source Squalour.AttackZombie/animationLourDie. NativeRoot calls these only
// from actual source-equivalent Squash contact/landing animation boundaries.
bool ApplyNativeSqualour269Attack(Plant*,bool sourcePop);
bool FinishNativeSqualour269(Plant*);
}
#endif
