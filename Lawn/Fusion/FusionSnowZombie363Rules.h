#pragma once
#include "FusionSourceGraph313.h"
#include <cstdint>
namespace LawnFusion
{
constexpr bool FusionSnowZombie363Kind(uint16_t k){return k==47||k==48;}
constexpr float kFusionSnow363MinimumTurn=13.22f,kFusionSnow363MaximumTurn=22.68f;
bool TryGetFusionSnow363ArmorStage(int health,int maximum,uint8_t& stage);
const FusionSourceGraph313* FindFusionSnow363Graph(uint16_t);
bool SourceRootDeltaSnow363(uint16_t,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
