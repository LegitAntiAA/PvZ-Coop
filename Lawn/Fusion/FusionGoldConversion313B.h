#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
struct FusionGoldConversion313BActor
{
    uint32_t localUID=0;
    uint16_t kind=0xffff;
    uint8_t row=0,rowType=0;
    float axisX=0;
    bool exists=false;
};
// Input MUST be source birth/list order. Source traverses backwards, not raw
// native DataArray index or numeric generation order. No actor is copied here.
bool BuildFusionGoldConversion313B(std::span<const FusionGoldConversion313BActor> ordered,
    std::vector<FusionGoldConversion313BActor>& output);
struct NativeGoldConversion313BHooks
{
    bool (*birthOrder)(Board*,std::vector<uint32_t>&)=nullptr;
    bool (*axisPoint)(const Zombie*,float&,float&)=nullptr;
    // Exact full transitive203 dependencies and complete batch peak before RNG.
    bool (*prepareChildren)(Board*,std::span<const uint16_t>,std::string*)=nullptr;
    // Includes every selected old virtualDie2/reward/points callback; no old
    // dead R/A slots may be credited before the engine actually frees them.
    bool (*prepareRetire)(Zombie*,std::span<const uint32_t>,std::string*)=nullptr;
    // Local presentation/trigger/status5 leaf ONLY. Must not call the old public
    // BeginNativeGoldPaper269Fall(223), which recursively launches conversion.
    bool (*beginPaperFall)(Zombie*)=nullptr;
    void (*sourceDie2)(Zombie*)=nullptr;
};
struct NativeGoldConversion313BPlan
{
    std::vector<FusionGoldConversion313BActor> actors;
    uint32_t parentLocalUID=0;
    int preparedTick=-1;
};
bool PrepareNativeGoldConversion313B(Zombie*,const NativeGoldConversion313BHooks&,
    NativeGoldConversion313BPlan&,std::string* error=nullptr);
// Execute immediately in the same native update, before message pumping. This
// makes callback order BeginFall(parent)->birth203->SetHP5->fall(child)->oldDie2.
// A violated preflight contract latches failed; it never skips/retries a partial
// conversion list and never invokes old Die2 after a failed corresponding birth.
bool CommitNativeGoldConversion313B(Zombie*,const NativeGoldConversion313BHooks&,
    const NativeGoldConversion313BPlan&,bool& failed,std::string* error=nullptr);
}
