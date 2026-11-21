#include "FusionGloveRules.h"
#include "FusionDefenderRules.h"

namespace LawnFusion
{
bool FusionGloveIdentityValid(FusionGloveIdentity identity, bool allowEmpty)
{
    return (identity.plantId && identity.instanceToken) ||
        (allowEmpty && !identity.plantId && !identity.instanceToken);
}

bool FusionGloveIdentityMatches(FusionGloveIdentity first, FusionGloveIdentity second)
{
    return FusionGloveIdentityValid(first) && first.plantId == second.plantId &&
        first.instanceToken == second.instanceToken;
}

bool CanReserveFusionGlove(const std::array<FusionGloveIdentity, kFusionBankPlayers>& holds,
    uint8_t activeMask, int slot, FusionGloveIdentity candidate, int cooldownTicks)
{
    if (!activeMask || (activeMask & 0xf0) || slot < 0 || slot >= static_cast<int>(holds.size()) ||
        !(activeMask & (1U << slot)) || !FusionGloveIdentityValid(candidate) ||
        cooldownTicks < 0 || cooldownTicks > kFusionGloveCooldownTicks) return false;
    for (size_t i = 0; i < holds.size(); ++i)
    {
        if (!FusionGloveIdentityValid(holds[i], true)) return false;
        if (static_cast<int>(i) != slot && FusionGloveIdentityMatches(holds[i], candidate)) return false;
    }
    // Repeated pickup acknowledgement is idempotent, even after another
    // holder's successful move has started the shared tool cooldown.
    if (FusionGloveIdentityMatches(holds[slot], candidate)) return true;
    return !holds[slot].plantId && cooldownTicks == 0;
}

int FusionGloveSuccessCooldown(FusionPlantId movedKind)
{
    const auto* defender = FindFusionDefenderProfile(movedKind);
    if (defender && defender->layer == FusionDefenderLayer::Under) return 334;
    // Source TypeMgr's final-kind Puff trait, not an OR of the parents. Keep
    // this verified set explicit; flying/Travel are outside this slice.
    switch (movedKind)
    {
    case 6: case 19: case 908: case 1018: case 1019: case 1021: case 1035: case 1065: case 1153: return 334;
    default: return kFusionGloveCooldownTicks;
    }
}

int AdvanceFusionGloveCooldown(int remainingTicks, int previousTick, int currentTick)
{
    if (remainingTicks <= 0) return 0;
    if (previousTick < 0 || currentTick <= previousTick) return remainingTicks;
    const auto elapsed = static_cast<int64_t>(currentTick) - previousTick;
    return elapsed >= remainingTicks ? 0 : remainingTicks - static_cast<int>(elapsed);
}
}
