#ifndef LAWN_FUSION_NATIVE_EQUIPMENT268_H
#define LAWN_FUSION_NATIVE_EQUIPMENT268_H
#include "FusionEquipmentRules.h"
#include <cstdint>
#include <limits>
class Plant;class Board;class Coin;
namespace LawnFusion
{
enum NativeEquipmentPlantPose : uint8_t
{kEquipmentIdle=0,kEquipmentAttract=1,kEquipmentGrow=2,kEquipmentBlow=3,kEquipmentBlowLoop=4,kEquipmentHolding=5};
// Root stores this on the real plant's full DataArray generation and replicates
// it. These are source milestones/history, not another entity container/clock.
struct NativeEquipmentPlantState
{
    uint32_t seaCallbacks=0;
    uint16_t componentSize=1;
    uint16_t targetItemId=0;
    uint32_t targetItemToken=0;
    FusionEquipmentType lastCapturedType=FusionEquipmentType::Bucket;
    uint8_t retainedRowRange=2;
    bool seaGrown=false;
    int8_t pendingItemDraw=-1;
};
NativeEquipmentPlantState* GetNativeEquipmentPlantState(Plant* plant);
const NativeEquipmentPlantState* GetNativeEquipmentPlantState(const Plant* plant);
inline bool NativeEquipmentPlantStateValid(uint16_t kind,const NativeEquipmentPlantState& s)
{
    const auto* profile=FindFusionEquipmentProfile(kind);
    if(!profile||s.componentSize<1||s.componentSize>1024||s.retainedRowRange<2||s.retainedRowRange>5||s.seaCallbacks>static_cast<uint32_t>((std::numeric_limits<int>::max)())||!FusionEquipmentTypeValid(int(s.lastCapturedType))||((s.targetItemId==0)!=(s.targetItemToken==0)))return false;
    if(s.pendingItemDraw< -1||s.pendingItemDraw>9||(kind!=1154&&s.pendingItemDraw!=-1))return false;
    if(profile->family!=FusionEquipmentFamily::Capture&&(s.targetItemId||s.lastCapturedType!=FusionEquipmentType::Bucket))return false;
    if(kind==1101)return s.seaGrown==(s.seaCallbacks>=3);
    return !s.seaCallbacks&&!s.seaGrown&&(kind==1100||s.retainedRowRange==2);
}
bool IsNativeEquipmentPlant(const Plant* plant);
void InitializeNativeEquipmentPlant(Plant* plant);
// Intercept before Classic magnet/producer family updates. The1154 normal
// producer uses existing productionTicks/pendingProductionTicks independently.
bool UpdateNativeEquipmentPlant(Plant* plant);
void UpdateNativeEquipmentPlantPhase(Plant* plant);
bool ApplyNativeEquipmentPlantSpecial(Plant* plant);
void NativeEquipmentPlantRemoving(Plant* plant,int reason);
void NativeEquipmentCoinRemoving268(Coin* coin);
void RebuildNativeEquipmentGraph(Board* board);
int GetNativeEquipmentGraphCount(const Plant* plant);
bool CanUseNativeEquipmentOnPlant(const Plant* plant,FusionEquipmentType type);
// Root authenticates the lease. Null item is reserved for source graph calls.
bool TryUseNativeEquipmentOnPlant(Plant* plant,FusionEquipmentType type,Coin* item);
bool GetNativeEquipmentPlantVisualPose(const Plant* plant,const char*& state,float& elapsed);
}
#endif
