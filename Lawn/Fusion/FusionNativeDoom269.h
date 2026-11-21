#ifndef LAWN_FUSION_NATIVE_DOOM_269_H
#define LAWN_FUSION_NATIVE_DOOM_269_H
#include "FusionDoom269Rules.h"
#include <array>
#include <vector>
class Board;class Plant;
namespace LawnFusion
{
enum class NativeDoom269Trait:uint8_t {Basic3,Advanced2,Expert10,Expert11,EveningBalanced};
struct NativeDoom269Point {float x=0,y=0;}; // Captured native Board points.
struct NativeDoom269PlantState
{
    float interval=1.5f;int ordinal=0,power=0;bool queuedTrigger=false;
    int lastUpdateTick=-1; // Host-only callback guard; DO NOT encode as source state.
};
struct NativeDoom269Cloud
{
    uint32_t sourceToken=0;
    int deadline=0;
    std::array<NativeDoom269Point,8> centers{};
};
inline bool NativeDoom269PlantStateValid(uint16_t kind,const NativeDoom269PlantState& state)
{
    if(kind==910)return state.interval==1.9f&&!state.ordinal&&!state.queuedTrigger&&state.power>=0&&state.power<10000;
    return kind==930&&!state.power&&FusionSuperDoom269StateValid(state.interval,state.ordinal,state.power);
}
// Root owns full Plant-generation Context maps and FNS. Clocks remain the real
// native Plant counters; deadline is an absolute native Board tick, not ticked.
NativeDoom269PlantState* EditNativeDoom269PlantState(Plant*,bool create=false);
const NativeDoom269PlantState* GetNativeDoom269PlantState(const Plant*);
std::vector<NativeDoom269Cloud>* EditNativeDoom269Clouds(Plant*,bool create=false);
bool HasNativeDoom269Trait(const Plant*,NativeDoom269Trait);
// Must instantiate ONE actual source particle52, return its actual GetChild0..7
// birth points, and replicate its visual birth. No inferred ring/FX substitute.
bool CreateNativeIceDoom269Cloud(Plant*,float shootX,float shootY,std::array<NativeDoom269Point,8>& centers);
// Root reuses/extends Board.SetDoom adapter, including source ice/Travel damage,
// collateral layers, pit policy, defender pulses and native co-op presentation.
bool ApplyNativeDoom269World(Plant*,int damage,bool ice,bool setPit);
bool IsNativeDoom269Plant(const Plant*);
bool InitializeNativeDoom269Plant(Plant*);
bool UpdateNativeDoom269Plant(Plant*);
void UpdateNativeDoom269Phase(Plant*);
bool DispatchNativeDoom269Events(Plant*);
bool GetNativeDoom269VisualPose(const Plant*,const char*& state,float& seconds);
void NativeDoom269PlantRemoving(Plant*,int reason);
// true suppresses the ordinary source Plant.TakeDamage; power consumed before
// the synchronous Doom callback. False lets Root continue canonical damage.
bool TryHandleNativeDoom269Damage(Plant*,int damage,int sourceType);
}
#endif
