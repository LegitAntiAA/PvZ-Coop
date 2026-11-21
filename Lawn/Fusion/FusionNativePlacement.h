#ifndef LAWN_FUSION_NATIVE_PLACEMENT_H
#define LAWN_FUSION_NATIVE_PLACEMENT_H

#include <cstdint>

class Board;
class Plant;
namespace LawnFusion
{
struct NativePlantData;
inline bool IsFusionSourceFlyingKind(uint16_t kind){return kind==241||kind==912||kind==1082;}
// TypeMgr.IsPuff0x18040D650; CheckPuff0x180446D00 limits real
// same-cell Puff actors to three before trying a source recipe.
inline bool IsFusionSourcePuffKind(uint16_t kind)
{
    switch(kind)
    {
    case 6:case 908:case 929:case 1018:case 1019:case 1021:case 1022:case 1031:
    case 1035:case 1036:case 1044:case 1065:case 1152:case 1153:case 1170:case 1183:return true;
    default:return false;
    }
}

// Read-only shared host/client policy. A true/null target is a direct base
// placement; a true/non-null target is the deterministic supported recipe.
// Failure leaves target unchanged. Other native layers are never removed.
bool ResolveNativeFusionPlacementTarget(Board* board, int column, int row,
    uint16_t incomingKind, Plant*& target, bool worldSpawn=false);
// Same layer/recipe policy for an actual existing actor. Direct relocation
// preserves mapped hybrids and waives only the native card-upgrade prerequisite.
bool ResolveNativeFusionMoveTarget(Board* board, int column, int row,
    const Plant* moving, Plant*& target);

// Host-only shared world placement. Provenance contributes kind/packet/owner/
// random origin only; result ancestry, token and ability state are fresh.
// Native paid input requires the observed parent ID/full-generation token,
// or0/0 for direct placement. World callers may omit the optional token.
// requestingSlot=-1 is a world effect and rejects every glove reservation.
// Rejection preserves the old parent and never charges sun or packet recharge.
// movingSource is accepted only for that slot's canonical glove lease; a direct
// move returns the same actor, while a mix consumes both parents after binding.
Plant* TryPlaceNativeFusionPlant(Board* board, int column, int row,
    const NativePlantData& incomingProvenance, uint16_t expectedTargetId, int requestingSlot,
    uint32_t expectedTargetToken = 0, Plant* movingSource = nullptr);

// Observe the current cell, then use the same placement policy with slot=-1.
// nullptr rejects only this attempt; a caller's surrounding cell loop continues.
Plant* TrySpawnNativeFusionPlant(Board* board, int column, int row,
    const NativePlantData& incomingProvenance);
}
#endif
