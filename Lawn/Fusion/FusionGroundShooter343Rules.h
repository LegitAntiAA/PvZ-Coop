#pragma once
#include <cstdint>
#include "FusionSourceGraph313.h"
namespace LawnFusion
{
constexpr bool FusionGroundShooter343Kind(uint16_t k){return k==114||k==118||k==119;}
constexpr bool FusionBalloon343Kind(uint16_t k){return k==25||k==29||k==119;}
struct FusionGround343Shot
{uint16_t bullet=0;int damage=0;uint8_t way=0;bool hostile=false,emit=false;float sourceOffsetX=0;};
bool TryPlanFusionGround343Shot(uint16_t kind,int status,bool beforeDying,bool changingRow,
 bool facingRight,bool controlled,bool muzzlePresent,FusionGround343Shot&);
bool TryGetFusionGround343Attribute(uint16_t kind,bool initial,float& countdown);
uint8_t FusionGround343ArmorStage(uint16_t kind,int hp,int maximum,uint8_t previous);
const FusionSourceGraph313* FindFusionGround343Graph(uint16_t kind);
bool SourceRootDeltaGround343(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
