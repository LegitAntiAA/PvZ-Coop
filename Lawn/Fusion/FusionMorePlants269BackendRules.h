#ifndef LAWN_FUSION_MORE_PLANTS269_BACKEND_RULES_H
#define LAWN_FUSION_MORE_PLANTS269_BACKEND_RULES_H
#include "../../ConstEnums.h"
#include <cstdint>
namespace LawnFusion
{
bool FusionMore269Payload(uint16_t kind);
bool FusionMore269PlanarLobber(uint16_t kind);
ProjectileType FusionMore269ProjectileType(uint16_t kind,uint8_t fire);
ProjectileMotion FusionMore269ProjectileMotion(uint16_t kind);
bool FusionMore269ProjectileMetadataValid(uint16_t kind,int damage,uint8_t fire,uint8_t way,
    ProjectileType type,ProjectileMotion motion,uint16_t sourceId,uint32_t sourceToken);
}
#endif
