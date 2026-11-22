#pragma once
#include "FusionExtendedZombie313Rules.h"
#include "FusionGarlicRules.h"
#include <cstdint>
class Zombie;
namespace LawnFusion
{
// Read-only view of authoritative or adopted A/B/C state, never a new clock.
struct NativeZombie313Semantics
{
 uint16_t kind=0;int status=0;
 bool beforeDying=false,inWater=false,rootFlip=false,colliderEnabled=false;
 bool flying=false,offGround=false,underground=false,submerged=false,ownBodyDamage=false;
};
bool IsNativeZombie313(const Zombie*);
bool GetNativeZombie313Semantics(const Zombie*,NativeZombie313Semantics&);
// True means A/B/C owns the decision; unsupported/Classic callers fall through.
bool NativeZombie313StatusTarget(const Zombie*,FusionExtendedEffect313,bool& eligible);
bool NativeZombie313MindControlAllowed(uint16_t kind,int sourceStatus,int sourceControlLevel);
// Exact virtual SetCold/SetFreeze operation over the existing native counters.
// True consumes A/B/C, including a source override's deliberate no-op.
bool NativeZombie313ApplyCold(Zombie*,bool freeze,int sourceTicks);
inline bool FusionSourceMachineRowMoving323(uint16_t kind,const GarlicActionState& state)
{return (kind==40||kind==210||kind==219)&&state.phase==GarlicActionPhase::Moving;}
bool NativeZombie313Caltrop(Zombie*);
bool NativeZombie313LoseEquipment(Zombie*,int sourceReason=0);
}
