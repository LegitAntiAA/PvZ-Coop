#pragma once
#include "FusionNightZombie281Rules.h"
#include <string>
class Zombie;class Board;
namespace LawnFusion
{
struct NativeNightZombie281State
{
    // Replicated controller view. Native raw body Reanimation is the phase clock.
    FusionNightZombie281Phase phase=FusionNightZombie281Phase::Moonwalk;
    bool lookBack=true,moonWalkFinished=false,canAttack=false,loseDancer=false;
    // HOST ONLY: never encode/hash or tick on ghosts.
    float moonWalkElapsed=0;
    std::array<uint32_t,4> dancers{}; // Actual full-generation DataArray UIDs.
    int tick=-1,motionTick=-1;
    FusionZombieClipPosition eventPosition{},motionPosition{};
    FusionNightZombie281Phase motionPhase=FusionNightZombie281Phase::Moonwalk;
    bool initialized=false,entering=true,motionValid=false,summonPending=false,summonSoundPlayed=false,failed=false;
    bool wasControlled=false;
    // Source outgoing clip continues callbacks during Animator crossfade.
    FusionNightZombie281Phase outgoing=FusionNightZombie281Phase::Moonwalk;
    FusionZombieClipPosition outgoingPosition{};
    float transitionRemaining=0;
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
};
bool NativeNightZombie281StateValid(const NativeNightZombie281State&);
bool InitializeNativeJackson281(Zombie*,NativeNightZombie281State&,const NativeNightZombie281Hooks&);
bool UpdateNativeJackson281(Zombie*,NativeNightZombie281State&,const NativeNightZombie281Hooks&);
bool UpdateNativeJackson281Animation(Zombie*,const NativeNightZombie281State&);
bool MoveNativeJackson281(Zombie*,NativeNightZombie281State&);
bool GetNativeJackson281Presentation(const Zombie*,const NativeNightZombie281State&,const char*&,float&);
// Call once after genuine SetMindControl, clearing references without controlling old dancers.
void NativeJackson281MindControlled(NativeNightZombie281State&);
// Bridge's existing Day273JumpOver callback calls this only for source6 after base landing.
// False is a bounded refusal. Partial birth latches failed; never replay that batch.
bool SpawnNativeDancePole281Children(Zombie*,const NativeNightZombie281Hooks&,bool& failed,std::string* error=nullptr);
bool SpawnNativeDancePole284Children(Zombie*,int birthRow,float birthShadowX,
    const NativeNightZombie281Hooks&,bool& failed,std::string* error=nullptr);
}
