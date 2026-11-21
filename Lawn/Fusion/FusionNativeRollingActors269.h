#ifndef LAWN_FUSION_NATIVE_ROLLING_ACTORS269_H
#define LAWN_FUSION_NATIVE_ROLLING_ACTORS269_H
#include "FusionRollingActors269Rules.h"
#include <vector>
class Board;class Plant;
namespace Sexy {template<class T>class TRect;typedef TRect<int> Rect;}
namespace LawnFusion
{
struct NativeRollingActor269Damage {uint32_t zombieGeneration=0;int damage=0;};
struct NativeRollingActor269State
{
    uint16_t kind=255,factoryKind=255;uint32_t instanceToken=0;
    int bornTick=0; // Age is Board.mainCounter-bornTick, never another clock.
    float originX=0,originY=0,rootOffsetX=0,rootOffsetY=0;
    int lastTick=-1; // Host-only callback guard; never adopted from the wire.
    bool directDestroy=false; // Host-only reentrancy guard, never wire-authored.
    // Actual source list/contact history, host-only. Never decoded as actors.
    std::vector<uint32_t> touching;
    std::vector<NativeRollingActor269Damage> damageHistory;
    int enterCount=0;
};
struct NativeWheat269State {bool isFromWheat=false;float wheatTime=0;int lastTick=-1;};
NativeRollingActor269State* EditNativeRollingActor269State(Plant*,bool create=false);
const NativeRollingActor269State* GetNativeRollingActor269State(const Plant*);
NativeWheat269State* EditNativeWheat269State(Plant*,bool create=false);
const NativeWheat269State* GetNativeWheat269State(const Plant*);
// Root supplies ONE real Board.Plant with fresh identity. Factory251 selects
// BigSunNut art but the source ctor's physical kind is255 for BOTH requests.
// Attach metadata/NetID before OnCreate; retain factoryKind as provenance/art.
bool CanCreateNativeRollingActor269(Board*,uint16_t kind);
Plant* CreateNativeRollingActor269Plant(Board*,uint16_t factoryKind,int column,int row);
void NativeRollingActor269Started(Plant*); // Actual sound53(.5,1), optional RoofEntity.
void NativeRollingActor269Entered(Plant*); // Sound54/55(.5,1), shake.02.
void NativeRollingActor269Achievement(Board*,int sourceAchievement);
// Direct native retirement must bypass SOURCE Die/Crashed no-op, but still
// execute one ordinary NativePlantRemoving/DataArray/net lifecycle.
void DestroyNativeRollingActor269Plant(Plant*);
// Canonical source TakeDamage type2-immunity/GetDamage step, with Instead
// forcibly false for BigWallNut. actual=0 represents proven type2 immunity.
bool ResolveNativeRollingActor269IncomingDamage(Plant*,int input,int sourceType,int& actual);
// Real source modes/anim-active and enemy count. No inferred wave timers.
bool NativeWheat269SpecialMode(const Board*);
bool NativeWheat269CanAdvance(const Plant*);
// Must cover the COMPLETE source SetEve(fromWheat) roster + dropped-card path;
// filtering to native-supported kinds would change the source distribution.
bool NativeWheat269ReplacementReady(const Board*);
Plant* NativeWheat269Replace(Plant* old); // reason1 -> FX11 -> SetEve(true), world provenance.
void NativeWheat269ApplyMaterial(Plant*); // Source GameAPP.wheatColor conditional.

Plant* SpawnNativeRollingActor269(Board*,uint16_t factoryKind,int column,int row);
bool InitializeNativeRollingActor269(Plant*,uint16_t factoryKind);
bool IsNativeRollingActor269(const Plant*);
bool UpdateNativeRollingActor269(Plant*);
// Gate the virtual SOURCE Die/Crashed calls, never native DataArray retirement.
// Internal reason255 is direct world/scene destruction, not a source Die call.
bool NativeRollingActor269SuppressSourceDie(const Plant*,int sourceReason=0);
bool ApplyNativeRollingActor269Damage(Plant*,int input,int sourceType);
bool GetNativeRollingActor269Rect(const Plant*,Sexy::Rect&);
bool GetNativeRollingActor269VisualPose(const Plant*,const char*& state,float& seconds);
bool GetNativeRollingActor269ShadowCenter(const Plant*,float& x,float& y);
uint16_t GetNativeRollingActor269ArtworkKind(const Plant*);
bool NativeRollingActor269StateValid(const NativeRollingActor269State&);
bool InitializeNativeWheat269(Plant*);
bool MarkNativeWheat269Replacement(Plant*);
bool UpdateNativeWheat269(Plant*);
bool NativeWheat269StateValid(const NativeWheat269State&);
}
#endif
