#ifndef LAWN_FUSION_GARLIC_RULES_H
#define LAWN_FUSION_GARLIC_RULES_H
#include <cstdint>

namespace LawnFusion
{
enum class GarlicActionPhase : uint8_t { Idle, Waiting, Moving };
struct GarlicActionState
{
    GarlicActionPhase phase = GarlicActionPhase::Idle;
    bool certainRow = false;
    uint16_t waitTicks = 0, moveElapsedTicks = 0, faceTicks = 0;
    int8_t originRow = -1, targetRow = -1;
    float startY = 0.0f, targetY = 0.0f;
    uint16_t moveDurationTicks = 100; // Appended to preserve existing aggregate callers.
};
struct GarlicBitePlan { int plantDamage = 0; bool certainRow = false; };
struct GarlicRowPlan { int randomZeroRow = -1, randomOneRow = -1; };
struct GarlicActionStep
{
    bool rowStartDue = false, positionChanged = false, rowFinished = false, faceEnded = false;
    float positionY = 0.0f;
    int targetRow = -1;
};
constexpr uint16_t kFusionGarlicWaitTicks = 100, kFusionGarlicMoveTicks = 100, kFusionGarlicFaceTicks = 150;
bool IsFusionGarlicKind(uint16_t kind);
bool ValidateFusionGarlicAction(const GarlicActionState& state, int rows);
bool FusionGarlicActionActive(const GarlicActionState& state);
bool FusionGarlicStopsWalking(const GarlicActionState& state);
bool FusionGarlicPreventsEating(const GarlicActionState& state);
bool TryGetFusionGarlicBitePlan(uint16_t kind, uint16_t potKind, GarlicBitePlan& plan);
bool TryBeginFusionGarlicAction(uint16_t kind, uint16_t potKind, int rows, GarlicActionState& state, int& plantDamage);
bool TryBeginFusionGarlicTimedAction(int rows,uint16_t waitTicks,bool certainRow,GarlicActionState& state);
// Zero/one fields preserve the source RNG order. Equal rows consume no RNG.
bool TryPlanFusionGarlicRow(int row, int rows, bool upCompatible, bool downCompatible, bool certainRow, GarlicRowPlan& plan);
bool TryStartFusionGarlicRow(GarlicActionState& state, int rows, int originRow, int targetRow, float startY, float targetY);
// Only exact speed0 pauses movement; a nonzero slow does not rescale its1s.
bool TryAdvanceFusionGarlicAction(GarlicActionState& state, int rows, bool paused, float effectiveMovementSpeed, GarlicActionStep& step);
bool TryStartFusionSourceRowDuration313(GarlicActionState&,int rows,int from,int target,float startY,float targetY,float seconds);
int FusionGarlicLogicalRow(const GarlicActionState& state, int currentRow);
int FusionGarlicSortingRow(const GarlicActionState& state, int currentRow);
}
#endif
