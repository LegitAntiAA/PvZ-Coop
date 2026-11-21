#include "FusionPotatoRules.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr double kPixelsPerSourceX = 72.0 / 1.35;
constexpr double kPixelsPerSourceY = 100.0 / 1.67;
constexpr double kTriggerWidth = 4.4992228 * .25 * kPixelsPerSourceX;
constexpr double kTriggerHeight = 3.172227 * .25 * kPixelsPerSourceY;
bool ValidRect(double x, double y, double width, double height)
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(width) && std::isfinite(height) &&
        width >= 0 && height >= 0 && std::isfinite(x + width) && std::isfinite(y + height);
}
}

bool TryGetFusionPotatoPreparationTicks(float sourceSeconds, int& ticks)
{
    if (!std::isfinite(sourceSeconds) || sourceSeconds < 14 || sourceSeconds > 16) return false;
    ticks = static_cast<int>(std::ceil(static_cast<double>(sourceSeconds) * 100.0)) + 1;
    return true;
}

bool FusionPotatoCanTrigger(int sourceStatus, bool sameRow, bool mindControlled,
    bool beforeDying, bool armed, bool alreadyExploded)
{
    if (sourceStatus < 0 || sourceStatus > 25 || !sameRow || mindControlled || beforeDying || !armed || alreadyExploded) return false;
    return sourceStatus != 1 && sourceStatus != 2 && sourceStatus != 3 &&
        sourceStatus != 10 && sourceStatus != 14 && sourceStatus != 16;
}

bool FusionPotatoCanDamage(int sourceStatus, bool sameRow, bool mindControlled)
{
    return sourceStatus >= 0 && sourceStatus <= 25 && sameRow && !mindControlled && sourceStatus != 10 && sourceStatus != 16;
}

bool TryMakeFusionPotatoArea(double cellCenterX, double cellCenterY, FusionPotatoArea& result)
{
    if (!std::isfinite(cellCenterX) || !std::isfinite(cellCenterY)) return false;
    FusionPotatoArea candidate;
    candidate.centerX = cellCenterX;
    candidate.centerY = cellCenterY - .3 * kPixelsPerSourceY;
    // Root scale.25; Shadow(.1,1.05) is aligned at mouseY+.3.
    // Root trigger offset(-.0036468506,2.3133245), size(4.4992228,3.172227).
    candidate.triggerX = cellCenterX + (-.1 - .0036468506) * .25 * kPixelsPerSourceX;
    candidate.triggerY = cellCenterY - (.3 + (2.3133245 - 1.05) * .25) * kPixelsPerSourceY;
    if (!std::isfinite(candidate.centerY) || !std::isfinite(candidate.triggerX) || !std::isfinite(candidate.triggerY)) return false;
    result = candidate;
    return true;
}

bool FusionPotatoTriggerOverlaps(const FusionPotatoArea& area, double x, double y, double width, double height)
{
    if (!ValidRect(x, y, width, height) || !std::isfinite(area.triggerX) || !std::isfinite(area.triggerY)) return false;
    return x <= area.triggerX + kTriggerWidth * .5 && x + width >= area.triggerX - kTriggerWidth * .5 &&
        y <= area.triggerY + kTriggerHeight * .5 && y + height >= area.triggerY - kTriggerHeight * .5;
}

bool FusionPotatoBlastOverlaps(const FusionPotatoArea& area, double x, double y, double width, double height)
{
    if (!ValidRect(x, y, width, height) || !std::isfinite(area.centerX) || !std::isfinite(area.centerY)) return false;
    const double dx = (std::max)({x - area.centerX, 0.0, area.centerX - (x + width)}) / kPixelsPerSourceX;
    const double dy = (std::max)({y - area.centerY, 0.0, area.centerY - (y + height)}) / kPixelsPerSourceY;
    return dx * dx + dy * dy <= 1.0 + 1.0e-12;
}
}
