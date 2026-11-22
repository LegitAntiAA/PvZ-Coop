#pragma once
#include "FusionQing208Rules.h"
#include <vector>
#include <string>
class Board;class Zombie;
namespace LawnFusion {
struct NativeQing208Contact {uint32_t uid=0;bool zombie=false;bool operator==(const NativeQing208Contact&)const=default;};
struct NativeQing208State:FusionQing208View {
 // HOST ONLY: actual LOCAL DataArray generations. Never wire these refs.
 std::vector<NativeQing208Contact> contacts;int tick=-1;bool initialized=false,failed=false,destroyed=false,dieEventHandled369=false;
};
struct NativeQing208Hooks {
 // Root bridges to the SAME graph313 parameters/events and genuine nativeTod
 // emission module. Client presentation mirrors quickMove; it never runs AI.
 bool (*trailReady)(Board*,std::string*)=nullptr;
 bool (*setTrailEmission)(Zombie*,bool)=nullptr;
 bool (*triggerQuickMove)(Zombie*)=nullptr;
 bool (*triggerShake)(Zombie*)=nullptr;
 bool (*sound)(Zombie*,uint16_t,float volume,float unityPitchRatio)=nullptr;
 // Actual source Destroy(gameObject), AFTER source DieEvent/points. Must not
 // begin another ordinary death/award; callback consumes the real owner.
 bool (*destroyOwner)(Zombie*)=nullptr;
};
enum class NativeQing208Event:uint8_t {StartQuickMove,QuickMoveOver,Bite,DieEvent,Destroy};
bool NativeQing208DieEvent369(Zombie*);
bool EnsureNativeQing208Resources(Board*,const NativeQing208Hooks&,std::string* error=nullptr);
bool InitializeNativeQing208(Zombie*,NativeQing208State&,const NativeQing208Hooks&);
bool UpdateNativeQing208(Zombie*,NativeQing208State&,const NativeQing208Hooks&);
bool ApplyNativeQing208Event(Zombie*,NativeQing208State&,NativeQing208Event,const NativeQing208Hooks&);
}
