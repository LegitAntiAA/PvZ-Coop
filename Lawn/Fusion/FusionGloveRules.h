#ifndef LAWN_FUSION_GLOVE_RULES_H
#define LAWN_FUSION_GLOVE_RULES_H

#include "FusionSeedBank.h"
#include <array>
#include <cstdint>

namespace LawnFusion
{
constexpr int kFusionGloveCooldownTicks = 1000;
struct FusionGloveIdentity
{
    uint16_t plantId = 0;
    uint32_t instanceToken = 0;
};

bool FusionGloveIdentityValid(FusionGloveIdentity identity, bool allowEmpty = false);
bool FusionGloveIdentityMatches(FusionGloveIdentity first, FusionGloveIdentity second);
bool CanReserveFusionGlove(const std::array<FusionGloveIdentity, kFusionBankPlayers>& holds,
    uint8_t activeMask, int slot, FusionGloveIdentity candidate, int cooldownTicks);
int FusionGloveSuccessCooldown(FusionPlantId movedKind);
// Board's real100Hz simulation counter is the only clock. A reset/backwards
// counter does not consume recharge; the caller rebases its cached tick.
int AdvanceFusionGloveCooldown(int remainingTicks, int previousTick, int currentTick);
}

#endif
