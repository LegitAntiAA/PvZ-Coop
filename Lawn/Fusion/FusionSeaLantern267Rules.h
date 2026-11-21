#ifndef LAWN_FUSION_SEA_LANTERN267_RULES_H
#define LAWN_FUSION_SEA_LANTERN267_RULES_H
#include <cstdint>
namespace LawnFusion
{
constexpr int kFusionSeaLantern267GrowthTicks=3001;
bool FusionSeaLantern267StateValid(uint16_t kind,uint32_t count,uint16_t rage,uint16_t bag,
    uint8_t mode,int productionTicks,int pendingTicks);
int FusionSeaLantern267LightRadius(uint8_t mode);
}
#endif
