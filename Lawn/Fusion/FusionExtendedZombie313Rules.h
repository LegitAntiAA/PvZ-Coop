#pragma once
#include "FusionZombieRules.h"
#include <cstdint>
namespace LawnFusion
{
enum class FusionExtendedZombie313Family:uint8_t {Gargantuar,IronGargantuar,Imp,Machine,Flying};
const FusionZombieProfile* FindFusionExtendedZombie313Profile(uint16_t);
bool TryGetFusionExtendedZombie313Family(uint16_t,FusionExtendedZombie313Family&);
bool FusionExtendedZombie313OwnBodyDamage(uint16_t);
bool FusionExtendedZombie313BaseLimbCallbacks(uint16_t);
struct FusionExtendedZombie313View
{
    int8_t sourceStatus=0;
    uint8_t bodyStage=0,weapon=0,machineType=0,uniqueType=0,firstArmorStage=0;
    bool carriedImpRemoved=false,ballRemoved=false,bombPresent=false,fallen=false,visible=true,adjustingImp=false;
    bool beforeDying=false,rootFlip=false;
    float impVerticalSpeed=1,impMaximumVx=-3,uniqueSpeed=1;
};
bool FusionExtendedZombie313ViewValid(uint16_t,const FusionExtendedZombie313View&);
enum class FusionExtendedEffect313:uint8_t {Cold,Freeze,Butter,Poison,Garlic,Jalapeno,Knockback,MindControl};
enum class FusionExtendedEffectAction313:uint8_t {Normal,Suppress,ColdInstead,CustomMindControl};
bool TryPlanFusionExtendedEffect313(uint16_t kind,int sourceStatus,FusionExtendedEffect313,float seconds,
    FusionExtendedEffectAction313&,float& resultingSeconds);
bool TryCapFusionExtendedZombie313Damage(uint16_t,int incoming,int& output);
struct FusionExtendedBody313Plan
{
    float health=0;
    uint8_t bodyStage=0,uniqueType=0;
    int sourceStatus=0;
    bool triggerThrow=false,removeIronHead=false;
};
// Pure source BodyTakeDamage plan. Caller preflights triggered callbacks and
// commits real source/native health once; no copied game actors or raw HP writes.
bool TryPlanFusionExtendedBody313(uint16_t kind,float health,int maximum,int damage,uint8_t previousBodyStage,
    uint8_t uniqueType,int sourceStatus,bool controlled,float actualSourceShadowX,FusionExtendedBody313Plan&);
struct FusionFlightArmor313Plan
{
    int health=0,maximum=0,overflow=0,sourceArmorType=0;
    bool fallen=false,fallTriggered=false,clearArmor=false,die2=false;
    int sourceStatus=10;
};
bool TryPlanFusionFlightFall313(uint16_t kind,bool alreadyFallen,bool waterRow,int armorMaximum,FusionFlightArmor313Plan&);
bool TryPlanFusionFlightArmor313(uint16_t kind,int sourceStatus,bool fallen,bool waterRow,int health,int maximum,int incoming,FusionFlightArmor313Plan&);
bool TryNormalizeFusionExtendedDeath313(uint16_t kind,int sourceReason,int& normalized);
struct FusionGarg313SpawnPlan
{
    uint16_t childKind=0;
    uint8_t minimumCount=0,maximumCount=0;
    float rootOffsetX=0,rootOffsetY=0,jitterMin=0,jitterMax=0;
    bool thrown=false,defaultFactoryXBeforeReposition=false;
};
// Base35/36 AnimThrow creates37 at factory sentinel9.9 BEFORE re-positioning
// the exact shoot node and invoking Thrown.43 SetImps uses37,212 uses213.
bool TryGetFusionGarg313SpawnPlan(uint16_t kind,bool hammerCallback,FusionGarg313SpawnPlan&);
bool FusionGarg313Target313(int sourceStatus,bool sameRow,bool oppositeSide);
bool FusionKirov313Target313(int sourceStatus,bool sameRow,bool hostile);
// Source scripted Rigidbody velocity; source mode extras are explicit inputs.
bool TryGetFusionKirov313Velocity(float sourceSpeed,bool right,bool travelDebuff6,bool slopeMode,float sourceRootX,float& vx,float& vy);
struct FusionImp313FlightStep {float vx=0,vy=0;bool triggerLand=false;};
bool TryStepFusionImp313Flight(float currentVx,float currentVy,float sourceMaximumVx,float shadowY,float landY,bool beforeDying,float dt,FusionImp313FlightStep&);
}
