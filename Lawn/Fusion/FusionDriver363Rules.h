#pragma once
#include "FusionSourceGraph313.h"
#include <cstdint>
namespace LawnFusion {
constexpr bool FusionDriver363Kind(uint16_t k){return k==201||k==222;}
bool FusionDriver363BodyStage(uint16_t,float health,int maximum,uint8_t previous,uint8_t& next);
bool FusionDriver363Velocity(bool right,bool stopped,bool beforeIce,float& vx);
bool StepFusionDriver363Turn(int32_t before,int32_t& next,bool& point,bool& flipped);
bool FusionDriver363RawDamage(uint16_t,int incoming,int& resolved);
const FusionSourceGraph313* FindFusionDriver363Graph(uint16_t);
}
