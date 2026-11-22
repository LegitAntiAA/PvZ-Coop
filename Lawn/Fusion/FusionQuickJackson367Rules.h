#pragma once
#include <cstdint>
namespace LawnFusion {
constexpr bool FusionJacksonFamily367Kind(uint16_t kind){return kind==10||kind==207;}
// Quick207 virtual GetDamage modifies RAW input before the existing common
// source GetDamage pipeline. MC does not skip this first source integer divide.
int FusionQuickJackson367RawDamage(int raw,bool fix,bool moonWalkFinished);
bool FusionQuickJackson367Motion(float actualSourceSpeed,float elapsedSeconds,float& sourceDeltaX);
}
