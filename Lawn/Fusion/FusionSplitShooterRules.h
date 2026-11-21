#ifndef LAWN_FUSION_SPLIT_SHOOTER_RULES_H
#define LAWN_FUSION_SPLIT_SHOOTER_RULES_H

#include "../../ConstEnums.h"
#include <cstdint>
#include <span>

namespace LawnFusion
{
// Source callback order within each unique clip deadline. Positions are the
// nominal source Shoot-Shadow offsets; native artwork uses the existing60px scale.
struct FusionSplitEmissionSpec
{
    uint16_t plantKind;
    uint8_t eventIndex;
    uint16_t projectileKind;
    float muzzleX, muzzleY;
    uint8_t specialType;
    bool operator==(const FusionSplitEmissionSpec&) const = default;
};

std::span<const FusionSplitEmissionSpec> GetFusionSplitEmissionSpecs(uint16_t plantKind);
bool FusionSplitEmissionValid(uint16_t plantKind, const FusionSplitEmissionSpec& emission);
bool FusionSplitProjectilePhaseValid(uint16_t projectileKind, uint8_t specialType, ProjectileMotion motion);
bool TryAdvanceFusionSplitProjectilePhase(double sourceX, double sourceMinX,
    uint8_t specialType, uint8_t& nextSpecialType);
bool TryGetFusionSplitProjectileDamageFlags(uint16_t projectileKind, uint8_t specialType, unsigned int& flags);
bool FusionSplitShooterCanTarget(int sourceStatus, bool sameRow, bool mindControlled,
    float bodyHealth, double sourceX, double sourceVisionMaxX);
}

#endif
