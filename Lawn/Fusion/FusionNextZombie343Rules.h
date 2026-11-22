#pragma once
#include "FusionZombieRules.h"
#include "FusionSourceGraph313.h"
#include <array>
namespace LawnFusion
{
// Source inheritance/factory contracts; these do not register/admit any NPC.
const FusionZombieProfile* FindFusionNextZombie343Profile(uint16_t kind);
bool TryGetFusionNextZombie343BaseKind(uint16_t kind,uint16_t& baseKind);
constexpr bool FusionCatapult343Kind(uint16_t k){return k==31||k==112||k==216;}
constexpr bool FusionSubmarine343Kind(uint16_t k){return k==14||k==200||k==211;}
constexpr bool FusionDancePole343Kind(uint16_t k){return k==6||k==217;}
constexpr bool FusionDriver343Kind(uint16_t k){return k==18||k==201||k==222;}
bool TryApplyFusionLandSubmarine343Drain(float health,int maximum,bool controlled,float dt,float&);
struct FusionCatapult343Shot
{
 uint16_t projectile=58;uint8_t remaining=0;
 bool walk2=false,cherryStage=false;
 uint8_t hiddenRootChildren=0,damagedRootRenderers=0;
};
bool TryPlanFusionCatapult343Shot(uint16_t kind,uint8_t ammunition,FusionCatapult343Shot&);
uint8_t FusionDrownCatapult343ArmorStage(int hp,int maximum,uint8_t previous);
struct FusionNext343DeathChild {uint16_t kind=0;float shadowOffset=0;bool controlled=false;};
struct FusionNext343DeathPlan
{std::array<FusionNext343DeathChild,3> children{};uint8_t count=0;bool explode=false,driverEffect=false;};
// Ordinary world only: Travel/Eve policy is rejected by the native world gate.
bool TryPlanFusionNext343Death(uint16_t kind,int reason,bool controlled,bool lostJumper,int status,FusionNext343DeathPlan&);
const FusionSourceGraph313* FindFusionSourceGraph343(uint16_t kind);
bool SourceRootDelta343(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
