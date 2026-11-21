#pragma once
#include "FusionZombieRules.h"
#include <array>
#include <span>
namespace LawnFusion
{
enum class FusionNightZombie281Phase:uint8_t
{Moonwalk,Point,Walk,ArmRaise,EatFromWalk,EatFromRaise,Death};
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
struct FusionNightZombie281Transition
{FusionNightZombie281Phase target=FusionNightZombie281Phase::Moonwalk;float duration=0;bool fixedSeconds=true;};
// Exact controller priority/exit thresholds. Native body clock remains authoritative.
bool TryGetFusionJackson281Transition(FusionNightZombie281Phase,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool speedPositive,bool attacking,bool loseDancer,
    bool summonTrigger,FusionNightZombie281Transition&);
bool TryGetFusionNightZombie281RootDelta(FusionNightZombie281Phase,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool rootFlipped,double pixelsPerSourceUnit,double&);
bool TryGetFusionNightZombie281ArmorStage(uint16_t kind,int hp,int maximum,uint8_t previous,
    uint8_t& stage,int& sourceSprite);
}
