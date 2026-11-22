#pragma once
#include <cstdint>
namespace LawnFusion
{
// Extensions of the same source Board, not an independently ticking world.
// Source ultimateCount also controls ordinary GetDamage when fix=false.
struct FusionExtendedBoard313State
{
    int32_t ultimateCount=0;
    float punishTime=0;
};
}
