#pragma once
#include "FusionNightZombie281Rules.h"
#include <string>
class Zombie;class Board;
namespace LawnFusion
{
struct NativeNightZombie281State : FusionNightZombie281View
{
    // HOST ONLY: never encode/hash, run on ghosts or use as presentation time.
    float moonWalkElapsed=0;
    std::array<uint32_t,4> dancers{};
    int tick=-1;
    FusionZombieClipPosition eventPosition{},outgoingPosition{};
    bool initialized=false,entering=true,summonPending=false,summonSoundPlayed=false,failed=false;
    bool wasControlled=false,transitionDeferred=false;
    FusionNightZombie281Transition deferredTransition{}; // Only normalized entry at Speed0.
};
struct NativeNightZombie281Hooks
{
    bool (*eveBalanced)(const Board*)=nullptr;
    bool (*travelDebuff13)(const Board*)=nullptr;
    // Must reserve/validate every child, model/root, FX11 and all R/A before RNG/birth.
    bool (*prepareChildren)(Zombie*,std::span<const uint16_t>,std::string*)=nullptr;
    // Called after actual factory birth. Dance6: child ROOT+.5; Jackson: Shadow+.7.
    bool (*childEffect)(Zombie* parent,Zombie* child,bool dancePole)=nullptr;
    // Mirrors source AdjustPosition: rotate actual source root, retaining Shadow.
    bool (*facingKeepShadow)(Zombie*,bool lookBack)=nullptr;
    // Real same-row eligible collider; when canAttack=true, retain the normal
    // source ID/full-generation target. Exclude Plant Caltrop and live Ladder.
    bool (*contact)(Zombie*,bool canAttack)=nullptr;
    void (*bite)(Zombie*)=nullptr;
    void (*sound)(Zombie*,uint16_t sourceSound)=nullptr;
    void (*fallSound)(Zombie*)=nullptr;
    void (*destroy)(Zombie*)=nullptr;
    // Exact source deadLeft/deadRight operands; Jackson.FixedUpdate calls Die(2).
    bool (*outsideSourceBounds)(const Zombie*)=nullptr;
    void (*dieWithoutAnimation)(Zombie*)=nullptr;
    // Source virtual overrides; absent for original10. All source207 uses
    // the SAME clocks/phase machine with its proven additional root movement.
    bool (*finishMoonWalk)(Zombie*)=nullptr;
    bool (*positionOverride)(Zombie*,bool& stopMoonWalk)=nullptr;
};
bool NativeNightZombie281StateValid(const NativeNightZombie281State&);
bool InitializeNativeJackson281(Zombie*,NativeNightZombie281State&,const NativeNightZombie281Hooks&);
// Owns source events AND root motion, once per actual unpaused native update.
// Invoke before generic movement/bites; the native body is the incoming clock.
bool UpdateNativeJackson281(Zombie*,NativeNightZombie281State&,const NativeNightZombie281Hooks&);
// Consume before stock PlayDeathAnim resets the actual outgoing body clock.
bool BeginNativeJackson281Death(Zombie*,NativeNightZombie281State&,const NativeNightZombie281Hooks&);
bool UpdateNativeJackson281Animation(Zombie*,const NativeNightZombie281State&);
// Consuming guard only: Update already applied root deltas; never move twice.
bool MoveNativeJackson281(Zombie*,NativeNightZombie281State&);
bool GetNativeJackson281Presentation(const Zombie*,const NativeNightZombie281State&,const char*&,float&);
// Optional outgoing sampler input. No transition => nullptr/0/1, atomically.
// outgoingElapsedSeconds=sourceSeconds/stateMultiplier, animatorSpeed=1.
bool GetNativeJackson281BlendPresentation(const Zombie*,const NativeNightZombie281State&,
    const char*& outgoingState,float& outgoingElapsedSeconds,float& incomingWeight);
// Call once after genuine SetMindControl, clearing references without controlling old dancers.
void NativeJackson281MindControlled(NativeNightZombie281State&);
// Bridge's existing Day273JumpOver callback calls this only for source6 after base landing.
// False is a bounded refusal. Partial birth latches failed; never replay that batch.
bool SpawnNativeDancePole281Children(Zombie*,const NativeNightZombie281Hooks&,bool& failed,std::string* error=nullptr);
bool SpawnNativeDancePole284Children(Zombie*,int birthRow,float birthShadowX,
    const NativeNightZombie281Hooks&,bool& failed,std::string* error=nullptr);
}
