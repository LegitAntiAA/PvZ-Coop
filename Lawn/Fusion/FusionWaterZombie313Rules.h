#pragma once
#include "FusionZombieRules.h"
#include "../Net/NetCommon.h"
#include <cstdint>
#include <span>
namespace LawnFusion
{
enum class FusionWaterZombie313Phase:uint8_t
{
    DriverIdle,DriverDamaged,SnorkelSwim,SnorkelUp,SnorkelEat,SnorkelDown,SnorkelDeath,
    DolphinRide,DolphinJump,DolphinSwim,DolphinEat,DolphinDeath,
    DrownWalk,DrownWalk2,DrownThrow,DrownEat,DrownDeath
};
struct FusionWaterZombie313View
{
    FusionWaterZombie313Phase phase=FusionWaterZombie313Phase::DriverIdle;
    FusionWaterZombie313Phase outgoing=FusionWaterZombie313Phase::DriverIdle;
    uint8_t sourceStatus=0,driverStage=0;
    bool facingRight=false,colliderEnabled=true,auxiliaryActive=false;
    float driverSpeed=0;
    float outgoingSourceSeconds=0,transitionElapsed=0,transitionTotal=0;
    float auxiliaryTransitionElapsed=0,auxiliaryTransitionTotal=0;
    // Actual auxiliary native Reanimation: Driver additiveShake, or water
    // Animator layer. No private age/Draw clock is used by ghosts.
    LawnNet::NetZombieAnimState auxiliary;
};
struct FusionWaterZombie313Clip
{
    const char* state=nullptr;
    const char* nativeTrack=nullptr;
    float seconds=0,multiplier=1;
    bool loop=false,speedParameter=true;
};
struct FusionWaterZombie313Transition
{FusionWaterZombie313Phase to;float exitSeconds=0,duration=0;bool needsExit=false;};
const FusionZombieProfile* FindFusionWaterZombie313Profile(uint16_t);
bool FusionWaterZombie313Kind(uint16_t);
bool FusionWaterZombie313SourceInWater(uint16_t);
bool FusionWaterZombie313ViewValid(uint16_t,const FusionWaterZombie313View&);
// Canonical snapshot additionally requires the real auxiliary clock. Host
// controller maps keep that raw clock in the native Reanimation until capture.
bool FusionWaterZombie313WireViewValid(uint16_t,const FusionWaterZombie313View&);
bool InitializeFusionWaterZombie313View(uint16_t,FusionWaterZombie313View&);
bool TryGetFusionWaterZombie313Clip(uint16_t,FusionWaterZombie313Phase,FusionWaterZombie313Clip&);
// Source controller transition priority; jump/throw are real pending triggers.
bool TrySelectFusionWaterZombie313Transition(uint16_t,const FusionWaterZombie313View&,bool attacking,bool jump,
    bool throwing,float speed,FusionWaterZombie313Transition&);
bool TryAdvanceFusionWaterZombie313Clock(uint16_t,FusionWaterZombie313Phase,FusionZombieClipPosition,
    float animatorSpeed,float deltaSeconds,FusionZombieClipPosition&);
bool TryGetFusionWaterZombie313RootDelta(uint16_t,FusionWaterZombie313Phase,FusionZombieClipPosition,
    FusionZombieClipPosition,double&);
enum FusionWaterZombie313Event:uint16_t
{Water313Bite=1,Water313JumpFail=2,Water313Splash=4,Water313JumpOver=8,Water313Throw=16,Water313Fall=32,Water313Destroy=64};
bool TryGetFusionWaterZombie313NextEvent(uint16_t,FusionWaterZombie313Phase,FusionZombieClipPosition,FusionZombieClipPosition&,uint16_t&);
bool TryGetFusionWaterZombie313Events(uint16_t,FusionWaterZombie313Phase,FusionZombieClipPosition,FusionZombieClipPosition,uint16_t&);
bool TryStepFusionDriver313Speed(float current,bool beforeDying,bool facingRight,float deltaSeconds,float& next,float& velocity);
bool TryStepFusionDriver313Damage(float health,int maximum,int damage,float& after,uint8_t& stage,bool& retire);
bool TryStepFusionDrown313Countdown(float before,bool throwing,bool controlled,bool beforeDying,float deltaSeconds,float& after,bool& trigger);
bool FusionDolphin313CanProbe(uint16_t,int status,bool controlled,bool frozen);
// TypeMgr.IsTangkelp40D920, includes the genuine1050 member as well.
constexpr bool FusionDolphin313KelpKind(uint16_t kind)
{return kind==15||(kind>=1049&&kind<=1051)||kind==1056||kind==1066||kind==1156;}
bool FusionDolphin313CanJumpPlant(bool flying,bool kelp,bool sameRow);
bool FusionDrown313PlantEligible(bool invincible,bool pumpkin,bool flying,bool pot);
bool TryMakeFusionDrown313Velocity(double sourceDX,double sourceDY,bool hasTarget,double& vx,double& vy);
bool TryGetFusionDrown313Damage(int currentHealth,uint16_t targetKind,bool eveBalanced,bool travel10,bool strongerBoard,int& damage);
// Ordinary source capability, not campaign/level admission. Actual geometry
// and terrain are supplied by the caller; no fake scene/row transformation.
bool FusionWaterZombie313WorldValid(uint16_t,int rows,bool poolOrFog,std::span<const uint8_t> waterRows,int row,bool controlled=false);
}
