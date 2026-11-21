#include "FusionGarlicRules.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
bool RowsValid(int rows) { return rows >= 2 && rows <= 6; }
void ClearMove(GarlicActionState& state)
{
    state.phase = GarlicActionPhase::Idle; state.certainRow = false;
    state.waitTicks = 0; state.moveElapsedTicks = 0;
    state.originRow = -1; state.targetRow = -1; state.startY = 0; state.targetY = 0;
}
}
bool IsFusionGarlicKind(uint16_t kind) { return kind == 29 || kind == 1130 || kind == 1145 || kind == 1146; }
bool ValidateFusionGarlicAction(const GarlicActionState& s, int rows)
{
    if (!RowsValid(rows) || s.faceTicks > kFusionGarlicFaceTicks || !std::isfinite(s.startY) || !std::isfinite(s.targetY)) return false;
    if (s.phase == GarlicActionPhase::Moving)
        return !s.waitTicks && s.moveElapsedTicks < kFusionGarlicMoveTicks && s.originRow >= 0 &&
            s.originRow < rows && s.targetRow >= 0 && s.targetRow < rows && std::abs(int(s.targetRow)-s.originRow) == 1;
    if (s.originRow != -1 || s.targetRow != -1 || s.startY != 0 || s.targetY != 0 || s.moveElapsedTicks) return false;
    if (s.phase == GarlicActionPhase::Waiting) return s.waitTicks > 0 && s.waitTicks <= kFusionGarlicWaitTicks;
    return s.phase == GarlicActionPhase::Idle && !s.waitTicks && !s.certainRow;
}
bool FusionGarlicActionActive(const GarlicActionState& s) { return s.phase != GarlicActionPhase::Idle || s.faceTicks != 0; }
bool FusionGarlicStopsWalking(const GarlicActionState& s) { return s.phase == GarlicActionPhase::Waiting; }
bool FusionGarlicPreventsEating(const GarlicActionState& s)
{
    // Source garlicSpeed0 pauses attack events during Waiting even if the
    // original Garlic died and the native target changed to another plant.
    return s.phase == GarlicActionPhase::Waiting || s.phase == GarlicActionPhase::Moving;
}
bool TryGetFusionGarlicBitePlan(uint16_t kind, uint16_t potKind, GarlicBitePlan& plan)
{
    if (!IsFusionGarlicKind(kind)) return false;
    const bool certain = kind == 1130 || potKind == 1130; // The pot is its own under-slot recipient.
    plan = {certain ? 10 : 15,certain};
    return true;
}
bool TryBeginFusionGarlicAction(uint16_t kind, uint16_t potKind, int rows, GarlicActionState& s, int& damage)
{
    GarlicBitePlan plan;
    if (!ValidateFusionGarlicAction(s,rows) || s.phase != GarlicActionPhase::Idle || !TryGetFusionGarlicBitePlan(kind,potKind,plan)) return false;
    s.phase = GarlicActionPhase::Waiting; s.certainRow = plan.certainRow; s.waitTicks = kFusionGarlicWaitTicks;
    damage = plan.plantDamage; // Existing face-recovery schedule is independent of the next bite.
    return true;
}
bool TryBeginFusionGarlicTimedAction(int rows,uint16_t wait,bool certain,GarlicActionState& s)
{
    if (!ValidateFusionGarlicAction(s,rows) || s.phase!=GarlicActionPhase::Idle ||
        !wait || wait>kFusionGarlicWaitTicks) return false;
    s.phase=GarlicActionPhase::Waiting; s.waitTicks=wait; s.certainRow=certain; return true;
}
bool TryPlanFusionGarlicRow(int row, int rows, bool up, bool down, bool certain, GarlicRowPlan& plan)
{
    if (!RowsValid(rows) || row < 0 || row >= rows) return false;
    up = up && row > 0; down = down && row < rows-1;
    GarlicRowPlan next{row,row};
    if (up && down)
    {
        if (!certain) next = {row-1,row+1};
        else if (rows%2 && row == (rows-1)/2) next = {row+1,row-1};
        else next = {row < rows/2 ? row+1 : row-1,row < rows/2 ? row+1 : row-1};
    }
    else if (up) next = {row-1,row-1};
    else if (down) next = {row+1,row+1};
    plan = next;
    return true;
}
bool TryStartFusionGarlicRow(GarlicActionState& s, int rows, int from, int target, float startY, float targetY)
{
    if (!ValidateFusionGarlicAction(s,rows) || s.phase != GarlicActionPhase::Waiting || s.waitTicks != 1 ||
        from < 0 || from >= rows || target < 0 || target >= rows || std::abs(target-from) > 1 ||
        !std::isfinite(startY) || !std::isfinite(targetY)) return false;
    s.faceTicks = kFusionGarlicFaceTicks;
    if (from == target) ClearMove(s);
    else
    {
        s.phase = GarlicActionPhase::Moving; s.waitTicks = 0; s.moveElapsedTicks = 0;
        s.originRow = static_cast<int8_t>(from); s.targetRow = static_cast<int8_t>(target);
        s.startY = startY; s.targetY = targetY;
    }
    return true;
}
bool TryAdvanceFusionGarlicAction(GarlicActionState& s, int rows, bool paused, float speed, GarlicActionStep& step)
{
    if (!ValidateFusionGarlicAction(s,rows) || !std::isfinite(speed) || speed < 0) return false;
    GarlicActionStep next;
    if (!paused)
    {
        if (s.faceTicks && !--s.faceTicks) next.faceEnded = true;
        if (s.phase == GarlicActionPhase::Waiting)
        {
            if (s.waitTicks > 1) --s.waitTicks;
            else next.rowStartDue = true; // Caller selects/starts the row in this same host update.
        }
        else if (s.phase == GarlicActionPhase::Moving && speed != 0)
        {
            ++s.moveElapsedTicks; next.positionChanged = true; next.targetRow = s.targetRow;
            next.positionY = static_cast<float>(double(s.startY)+(double(s.targetY)-s.startY)*s.moveElapsedTicks/kFusionGarlicMoveTicks);
            if (s.moveElapsedTicks == kFusionGarlicMoveTicks)
            { next.rowFinished = true; next.positionY = s.targetY; ClearMove(s); }
        }
    }
    step = next;
    return true;
}
int FusionGarlicLogicalRow(const GarlicActionState& s, int row)
{ return s.phase == GarlicActionPhase::Moving ? (std::max)(int(s.originRow),int(s.targetRow)) : row; }
int FusionGarlicSortingRow(const GarlicActionState& s, int row)
{ return s.phase == GarlicActionPhase::Moving ? (std::min)(int(s.originRow),int(s.targetRow)) : row; }
}
