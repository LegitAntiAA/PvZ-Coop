#pragma once
#include "FusionJackbox300Rules.h"
#include <string>
class Zombie;class Board;class LawnApp;

namespace LawnFusion
{
struct NativeJackbox300State:FusionJackbox300View
{
    // HOST ONLY. Never serialize local UID, RNG/event positions or retry latches.
    uint32_t localUID=0;
    int tick=-1;
    FusionZombieClipPosition incomingPosition{},outgoingPosition{};
    bool initialized=false,failed=false,deathEventHandled=false,popEventHandled=false;
    // Actual source MoveRow coroutine continuation, HOST ONLY. Its passed8
    // parameter is not a duration clock; movement consumes real root deltas.
    bool rowMoving=false;
    int rowTarget=-1,rowNext=-1;
    float rowRootX=0,rowStartShadowY=0,rowTargetShadowY=0;
};
struct NativeJackbox300Input
{
    // Actual source theSpeed and animator Speed, not screen velocity/Draw time.
    float sourceSpeed=0,animatorSpeed=0;
    bool sourcePlaying=false,beforeDying=false,attacking=false,instant=false;
    int targetRow=-1;
};
struct NativeJackbox300Explosion
{
    uint16_t ownerId=0,sourceKind=0;
    uint32_t ownerToken=0;
    int row=0,fromWave=0,targetRow=-1,renderOrder=0;
    float shadowX=0,shadowY=0,centerX=0,centerY=0;
    bool controlled=false;
    // Lawnf.ZombieExplode4017D0 source operands. Native bridge projects the
    // source circle accurately; it must not substitute a native pixel circle.
    static constexpr float sourceRadius=2;
    static constexpr int plantDamage=1000000,zombieCharredDamage=1800,sourceDamageType=2;
};
enum class NativeJackbox300Operation:uint8_t {Initialize,Pop,Explosion,Death,ArmorFall};
enum class NativeJackbox300Audio:uint8_t {Play,Pause,Unpause};
struct NativeJackbox300Hooks
{
    NativeJackbox300State* (*stateFor)(Zombie*)=nullptr;
    bool (*input)(Zombie*,NativeJackbox300Input&)=nullptr;
    int (*randomInt)(Board*,int minimum,int exclusiveMaximum)=nullptr;
    float (*randomFloat)(Board*,float minimum,float maximum)=nullptr;
    // Read-only resources/whole recipient callback/allocator closure BEFORE
    // RNG/destructive Die. potentialChild24 includes a fresh real actor/body,
    // source armor/PS/collision and exact targetRow continuation (or refusal).
    bool (*preflight)(Zombie*,NativeJackbox300Operation,const NativeJackbox300Explosion*,bool potentialChild24,std::string*)=nullptr;
    bool (*canPlayLoop)(const Zombie*)=nullptr; // Other Jackbox audioPlaying scan, source368300.
    void (*audio)(Zombie*,NativeJackbox300Audio)=nullptr; // Genuine source loop89.
    void (*sound)(Zombie*,uint16_t sourceSound,float volume)=nullptr;
    // Shared source target/bite/EatEffect/callback pipeline, source PlayEatSound.
    void (*bite)(Zombie*)=nullptr;
    // Uses existing native movement/row physics and the model's actual source
    // projection. This is an unscaled source root DELTA, mirrored only once.
    bool (*moveRootDelta)(Zombie*,double sourceDeltaX)=nullptr;
    // Existing shared source circle/collision/damage/FX42/sound43/shake. Return
    // true for acknowledged cosmetic capacity drops. No owner pointer required.
    bool (*explode)(Board*,const NativeJackbox300Explosion&)=nullptr;
    // Common source Die2 bookkeeping; may delete owner AND its controller.
    void (*retire)(Zombie*,int sourceReason)=nullptr;
    bool (*moveChildToTargetRow)(Zombie*,int sourceRow)=nullptr;
    // Actual first-armor source PS callback: Leave unrelated armor semantics
    // to the existing source pipeline; source node name/PS snapshot is genuine.
    bool (*armorFall)(Zombie*)=nullptr;
    void (*failed)(Board*,const char*)=nullptr;
};
bool InitializeNativeJackbox300(Zombie*,NativeJackbox300State&,const NativeJackbox300Hooks&);
bool UpdateNativeJackbox300(Zombie*,NativeJackbox300State&,const NativeJackbox300Hooks&);
bool UpdateNativeJackbox300Animation(Zombie*,const NativeJackbox300State&,const NativeJackbox300Hooks&);
// Root calls BEFORE stock PlayDeathAnim/ApplyBurn reset any real body span.
// It must re-resolve its captured full UID before continuing ordinary Die:
// source death explosion callbacks can synchronously retire the owner.
enum class NativeJackbox300DeathResult:uint8_t {NotHandled,Proceed,Refused,OwnerRetired};
NativeJackbox300DeathResult BeginNativeJackbox300Death(Zombie*,NativeJackbox300State&,int sourceReason,const NativeJackbox300Hooks&);
// Pause callback BEFORE actual Freeze/Buttered; resume AFTER Unfreeze/UnButter.
// The source loop eligibility predicate does not invent extra head/status tests.
bool NativeJackbox300AudioStatus(Zombie*,NativeJackbox300State&,bool pause,const NativeJackbox300Hooks&);
bool NativeJackbox300LoseHead(Zombie*,NativeJackbox300State&,const NativeJackbox300Hooks&);
bool NativeJackbox300FirstArmorFall(Zombie*,NativeJackbox300State&,const NativeJackbox300Hooks&);
// Only AFTER successful real shared Magnet capture/ownership transfer. Before
// allocating the capture item, use TryDisarm... on a copy as eligibility check.
// Caller hides/removes genuine box+handle nodes via boxPresent; no free item.
bool CommitNativeJackbox300Disarm(Zombie*,NativeJackbox300State&,const NativeJackbox300Hooks&);
bool GetNativeJackbox300Presentation(const Zombie*,const NativeJackbox300State&,const char*&,float& normalized);
bool GetNativeJackbox300Blend(const Zombie*,const NativeJackbox300State&,const char*&,float& outgoingSamplerSeconds,float& incomingWeight);
// Typed300 gameplay owns root motion. Root consumes stock movement/attack and
// StartEating/StopEating track switches; common life/HP/armor/freeze stays native.
}
