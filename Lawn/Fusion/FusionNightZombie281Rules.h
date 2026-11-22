#pragma once
#include "FusionZombieRules.h"
#include <array>
#include <span>
namespace LawnFusion
{
enum class FusionNightZombie281Phase:uint8_t
{Moonwalk,Point,Walk,ArmRaise,EatFromWalk,EatFromRaise,Death};
// Canonical host view only. The incoming clock is the actual native body.
struct FusionNightZombie281View
{
    FusionNightZombie281Phase phase=FusionNightZombie281Phase::Moonwalk;
    bool lookBack=true,moonWalkFinished=false,canAttack=false,loseDancer=false;
    FusionNightZombie281Phase outgoing=FusionNightZombie281Phase::Moonwalk;
    float outgoingSourceSeconds=0,transitionElapsed=0,transitionTotal=0;
};
bool FusionNightZombie281ViewValid(const FusionNightZombie281View&);
// Source controller state names differ from clip names (walk/walk1/two eat states).
const char* FusionJackson281ControllerState(FusionNightZombie281Phase);
struct FusionNightZombie281Clip
{const char* name=nullptr;const char* nativeTrack=nullptr;float seconds=0,multiplier=0;bool loop=true;};
bool TryGetFusionNightZombie281Clip(FusionNightZombie281Phase,FusionNightZombie281Clip&);
struct FusionNightZombie281Child
{uint16_t kind=0;uint8_t slot=0;int row=0;float sourceX=0;bool controlled=false;};
struct FusionNightZombie281Children
{std::array<FusionNightZombie281Child,4> actors{};uint8_t count=0;};
// Source rowTypes==1 is water. All inputs/outputs use genuine source world X.
// Output is atomic. Dance6 intentionally does not inherit the parent's allegiance.
bool TryPlanFusionDancePole281(int row,std::span<const uint8_t> rowTypes,float shadowX,
    bool eveBalanced,int status,bool beforeDying,FusionNightZombie281Children&);
bool TryPlanFusionJackson281(int row,std::span<const uint8_t> rowTypes,float shadowX,
    bool eveBalanced,int status,bool beforeDying,bool controlled,
    const std::array<bool,4>& childObjectPresent,FusionNightZombie281Children&);
bool FusionJackson281MissingDancer(int row,int rows,const std::array<bool,4>& present);
// Callbacks in (before,after]; time0 runs on actual entry or a genuine new loop.
// Native held ends are the same final source instant and never replay events.
enum FusionNightZombie281Event:uint16_t
{Night281Forward=1,Night281Back=2,Night281Summon=4,Night281PointOver=8,
 Night281Bite1=16,Night281Bite2=32,Night281Fall=64,Night281Destroy=128};
bool TryGetFusionJackson281Events(FusionNightZombie281Phase,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool entering,uint16_t& events);
// First strictly later callback; loop-time0 uses the next genuine loop.
bool TryGetFusionJackson281NextEvent(FusionNightZombie281Phase,FusionZombieClipPosition,
    FusionZombieClipPosition& next);
// animatorSpeed is originSpeed * current source Speed factor; state multiplier
// is applied exactly once. Canonical nonloop held ends never grow another loop.
bool TryAdvanceFusionJackson281Clock(FusionNightZombie281Phase,FusionZombieClipPosition,
    float animatorSpeed,float gameplaySeconds,FusionZombieClipPosition&);
struct FusionNightZombie281Transition
{FusionNightZombie281Phase target=FusionNightZombie281Phase::Moonwalk;float duration=0;bool fixedSeconds=true;};
// Exact controller priority/exit thresholds. Native body clock remains authoritative.
bool TryGetFusionJackson281Transition(FusionNightZombie281Phase,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool speedPositive,bool attacking,bool loseDancer,
    bool summonTrigger,FusionNightZombie281Transition&);
bool TryGetFusionJackson281TransitionSeconds(FusionNightZombie281Phase from,
    const FusionNightZombie281Transition&,float animatorSpeed,float& totalSeconds);
// Explicit native motion projection: midpoint weight blends DELTAS only.
bool TryGetFusionJackson281BlendRootDelta(FusionNightZombie281Phase incoming,
    FusionZombieClipPosition incomingBefore,FusionZombieClipPosition incomingAfter,
    FusionNightZombie281Phase outgoing,FusionZombieClipPosition outgoingBefore,
    FusionZombieClipPosition outgoingAfter,float weightBefore,float weightAfter,
    bool flipped,double pixelsPerSourceUnit,double&);
bool TryGetFusionNightZombie281RootDelta(FusionNightZombie281Phase,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool rootFlipped,double pixelsPerSourceUnit,double&);
bool TryGetFusionNightZombie281ArmorStage(uint16_t kind,int hp,int maximum,uint8_t previous,
    uint8_t& stage,int& sourceSprite);
}
