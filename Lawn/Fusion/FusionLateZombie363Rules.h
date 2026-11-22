#pragma once
#include "FusionSourceGraph313.h"
#include <array>
namespace LawnFusion {
constexpr bool FusionSuperPogo363Kind(uint16_t k){return k==34||k==209;}
struct FusionLate363Death
{
 bool explosion=false,effect36=false;
 uint8_t count=0;
 std::array<uint16_t,3> children{};
 std::array<float,3> offsets{};
 bool operator==(const FusionLate363Death&)const=default;
};
// Ordinary source branch only: Eve/Travel are separately gated world modes.
bool TryPlanFusionLate363Death(uint16_t,int reason,bool controlled,bool lostJumper,int status,bool consumed,FusionLate363Death&);
bool FusionSuperPogo363Armor(int health,int maximum,uint8_t previous,uint8_t& next);
const FusionSourceGraph313* FindFusionLate363Graph(uint16_t);
bool SourceRootDeltaLate363(uint16_t,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
