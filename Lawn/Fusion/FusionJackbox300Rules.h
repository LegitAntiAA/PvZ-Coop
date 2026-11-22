#pragma once
#include "FusionZombieRules.h"

namespace LawnFusion
{
enum class FusionJackbox300Phase:uint8_t {Walk,Eat,Pop,Death};
struct FusionJackbox300View
{
    FusionJackbox300Phase phase=FusionJackbox300Phase::Walk,outgoing=FusionJackbox300Phase::Walk;
    bool boxPresent=true,popTriggered=false,audioPlaying=false;
    // HP/armor stage/originSpeed/allegiance already live in sourceData/raw.
    float countdown=4.4f,originalCountdown=4.4f;
    float outgoingSeconds=0,transitionElapsed=0,transitionTotal=0;
};
struct FusionJackbox300Clip
{const char* state;const char* nativeTrack;float seconds,multiplier;bool loop;};
struct FusionJackbox300Conditions
{
    bool sourcePlaying=true,beforeDying=false,speedNonzero=true,attacking=false;
};
bool FusionJackbox300Kind(uint16_t);
bool FusionJackbox300ViewValid(uint16_t,const FusionJackbox300View&);
bool TryGetFusionJackbox300Clip(FusionJackbox300Phase,FusionJackbox300Clip&);
// Awake draws integer chance0..99 then a genuine float in the selected range.
// Start's source Board.instantJackbox branch overwrites ONLY current countdown.
bool TryInitializeFusionJackbox300(uint16_t,int chance,float countdownDraw,bool instant,FusionJackbox300View&);
bool TryStepFusionJackbox300Countdown(uint16_t,const FusionJackbox300View&,const FusionJackbox300Conditions&,
    float deltaSeconds,FusionJackbox300View&);
// Source status11 is set by Pop immediately, before a Speed-gated transition.
int FusionJackbox300SourceStatus(const FusionJackbox300View&);
bool TryFilterFusionJackbox300Damage(uint16_t,const FusionJackbox300View&,int incoming,int&);
bool TrySelectFusionJackbox300Phase(uint16_t,const FusionJackbox300View&,bool attacking,bool speedPositive,FusionJackbox300Phase&);
bool TryAdvanceFusionJackbox300Clock(FusionJackbox300Phase,FusionZombieClipPosition,float speed,float delta,FusionZombieClipPosition&);
enum FusionJackbox300Event:uint8_t {Jackbox300Bite=1,Jackbox300Explode=2,Jackbox300Destroy=4};
bool TryGetFusionJackbox300Events(FusionJackbox300Phase,FusionZombieClipPosition,FusionZombieClipPosition,uint8_t&);
bool TryGetFusionJackbox300NextEvent(FusionJackbox300Phase,FusionZombieClipPosition,FusionZombieClipPosition&);
bool TryGetFusionJackbox300RootDelta(FusionJackbox300Phase,FusionZombieClipPosition,FusionZombieClipPosition,double&);
// DieEvent30 ALWAYS draws Range(0,original), even if reason!=0/box absent.
bool TryPlanFusionJackbox300Death(uint16_t,const FusionJackbox300View&,int reason,float genuineDeathRoll,bool& explodeAndSpawn24);
bool TryGetFusionJackbox300ArmorStage(uint16_t,int health,int maximum,uint8_t previous,uint8_t& stage,int& sourceSprite);
bool TryDoubleFusionJackbox300Speed(uint16_t,float before,float& after);
// Magnet's actual successful capture clears box/handle and pauses audio, but
// preserves BOTH source countdowns and never doubles the actor's speed.
bool TryDisarmFusionJackbox300(uint16_t,const FusionJackbox300View&,FusionJackbox300View&);
// Extension to the existing source runtime validator, NOT the spawn RNG range.
bool FusionJackbox300RuntimeSpeedValid(uint16_t,const FusionJackbox300View&,float);
}
