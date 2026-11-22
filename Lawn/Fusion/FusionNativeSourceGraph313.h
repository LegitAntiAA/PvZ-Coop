#pragma once
#include "FusionSourceGraph313.h"
#include <string>
class Zombie;class Board;class Reanimation;
namespace LawnFusion
{
struct NativeSourceGraph313State
{
    FusionSourceGraph313View layers[3]{}; // Actual cohort max:223 base+shoot+shootback.
    // HOST ONLY: event progress, triggers and local generation are not wire data.
    FusionSourceGraph313Cursor cursors[3]{},outgoingCursors[3]{};
    FusionSourceGraph313Parameters parameters{};
    uint32_t auxiliaryUIDs[2]{}; // LOCAL full native R generations; actual clocks wire separately.
    uint32_t localUID=0;
    int tick=-1;
    uint32_t revisions[3]{};
    bool initialized=false,failed=false;
    FusionSourceGraph313Parameters layerParameters361[3]{}; // HOST ONLY, used only by the optional per-layer provider.
    uint8_t initializedLayerMask343=0; // HOSTONLY ownership; never wire/clock.
};
struct NativeSourceGraph313Hooks
{
    NativeSourceGraph313State* (*stateFor)(Zombie*)=nullptr;
    Reanimation* (*clock)(Zombie*,uint8_t layer)=nullptr; // Actual body / attached aux R.
    bool (*parameters)(Zombie*,FusionSourceGraph313Parameters&)=nullptr;
    bool (*preflightEvent)(Zombie*,FusionSourceGraph313Event,std::string*)=nullptr;
    bool (*event)(Zombie*,FusionSourceGraph313Event,int32_t sourceInt)=nullptr;
    // Source root curve integral, same genuine sampler/phase/loop contract.
    // Returns SOURCE root delta. No screen distance/visual PPU guess here.
    bool (*rootDelta)(Zombie*,uint8_t state,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&)=nullptr;
    bool (*moveRootDelta)(Zombie*,double)=nullptr;
    bool (*nullExitReady)(Zombie*,uint8_t layer,uint8_t state)=nullptr;
    void (*failed)(Board*,const char*)=nullptr;
    bool (*parametersForLayer361)(Zombie*,uint8_t,FusionSourceGraph313Parameters&)=nullptr;
    bool (*layerActive366)(Zombie*,uint8_t)=nullptr; // Optional actual Animator GameObject activity. // Optional; old shared-parameter path unchanged.
};
bool InitializeNativeSourceGraph313(Zombie*,NativeSourceGraph313State&,const NativeSourceGraph313Hooks&);
bool NativeSourceGraph313ClockTracksReady(Zombie*,const NativeSourceGraph313Hooks&,std::string* error=nullptr);
// Root implements O(1) full-generation/holder/epoch lease metadata and the
// narrow Reanimation::Update early guard. No source/Classic global time switch.
bool RegisterNativeSourceGraphClock313(Reanimation*,bool owned);
bool NativeSourceGraph313OwnsClock(const Reanimation*);
void ReleaseNativeSourceGraph313Clocks(Zombie*,const NativeSourceGraph313Hooks&);
// Sole advancement owner: Root's native Reanimation::Update typed lease guard
// must skip these HOST body/aux full generations. Reanimation has no mDontUpdate.
// The holder and stock Zombie progression must not also advance these clocks.
// Incoming state starts from actual R each tick; private cursors only consume
// gameplay events. Clients adopt canonical actual R metadata, never run this.
// Direct source Animator.Play, including source Start walk / Imp Thrown.
// Captures genuine normalized offset and never replays historical markers.
bool PlayNativeSourceGraph313(Zombie*,NativeSourceGraph313State&,uint8_t layer,const char* sourceState,float normalizedOffset,const NativeSourceGraph313Hooks&);
bool UpdateNativeSourceGraph313(Zombie*,NativeSourceGraph313State&,const NativeSourceGraph313Hooks&);
bool UpdateNativeSourceGraph313Rates(Zombie*,const NativeSourceGraph313State&,const NativeSourceGraph313Hooks&);
bool ReadNativeSourceGraph313Clock(Zombie*,uint8_t layer,uint8_t sourceState,const NativeSourceGraph313Hooks&,FusionSourceGraph313Cursor&);
bool GetNativeSourceGraph313Presentation(Zombie*,const NativeSourceGraph313State&,uint8_t layer,const NativeSourceGraph313Hooks&,
    const char*& incoming,float& incomingSamplerSeconds,const char*& outgoing,float& outgoingSamplerSeconds,float& incomingWeight);
// Partial source layers share the SAME segmented evaluator. A caller may own
// layer2 alone while the existing Water controller owns body/water clocks.
// Masks never imply another clock or permit an uninitialized layer to tick.
bool InitializeNativeSourceGraph313Masked(Zombie*,NativeSourceGraph313State&,const NativeSourceGraph313Hooks&,uint8_t layerMask);
bool NativeSourceGraph313ClockTracksReadyMasked(Zombie*,const NativeSourceGraph313Hooks&,uint8_t layerMask,std::string* error=nullptr);
bool UpdateNativeSourceGraph313Masked(Zombie*,NativeSourceGraph313State&,const NativeSourceGraph313Hooks&,uint8_t layerMask);
bool UpdateNativeSourceGraph313RatesMasked(Zombie*,const NativeSourceGraph313State&,const NativeSourceGraph313Hooks&,uint8_t layerMask);
void ReleaseNativeSourceGraph313ClocksMasked(Zombie*,const NativeSourceGraph313Hooks&,uint8_t layerMask);

}
