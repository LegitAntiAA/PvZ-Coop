#ifndef LAWN_FUSION_WATER_PLANTS267_RULES_H
#define LAWN_FUSION_WATER_PLANTS267_RULES_H
#include <cstdint>
namespace LawnFusion
{
bool FusionLily267CanMimic(uint16_t kind);
bool FusionLily267DoubleBoxKind(uint16_t kind);
bool FusionWater267PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t lilyType,uint8_t cover);
constexpr int kFusionLily267CycleTicks=9001; // Source strict growTime>90 seconds.
}
#endif
