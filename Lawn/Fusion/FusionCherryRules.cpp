#include "FusionCherryRules.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr double kPixelsPerSourceX = 72.0 / 1.35;
constexpr double kPixelsPerSourceY = 100.0 / 1.67;
}

bool TryResolveFusionCherryDamage(int sourceDifficulty, float bodyHealth,
    int firstArmorHealth, FusionCherryDamage& result)
{
    if (sourceDifficulty < 0 || sourceDifficulty > 5 || !std::isfinite(bodyHealth) ||
        bodyHealth < 0.0f || firstArmorHealth < 0) return false;
    const float damage = sourceDifficulty == 4 ? 1800.0f * 0.7f :
        sourceDifficulty == 5 ? 1800.0f * 0.4f : 1800.0f;
    FusionCherryDamage candidate;
    candidate.instantBurn = sourceDifficulty < 4 ? bodyHealth <= damage :
        bodyHealth + static_cast<float>(firstArmorHealth) <= damage;
    candidate.damage = static_cast<int>(damage);
    result = candidate;
    return true;
}

bool TryMakeFusionCherryArea(double cellCenterX, double cellCenterY, FusionCherryArea& result)
{
    if (!std::isfinite(cellCenterX) || !std::isfinite(cellCenterY)) return false;
    FusionCherryArea candidate;
    // GetPlantPosition puts Shadow at landY+.3. The source prefab has
    // scale.33 and Shadow(-.1,.33); Bomb appears at rootY+.5.
    candidate.centerX = cellCenterX + 0.033 * kPixelsPerSourceX;
    candidate.centerY = cellCenterY - 0.6911 * kPixelsPerSourceY;
    if (!std::isfinite(candidate.centerX) || !std::isfinite(candidate.centerY)) return false;
    result = candidate;
    return true;
}

bool FusionCherryRectOverlaps(const FusionCherryArea& area,
    double x, double y, double width, double height)
{
    if (!std::isfinite(area.centerX) || !std::isfinite(area.centerY) ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height) || width < 0.0 || height < 0.0 ||
        !std::isfinite(x + width) || !std::isfinite(y + height)) return false;
    const double dx = (std::max)({x - area.centerX, 0.0, area.centerX - (x + width)}) / kPixelsPerSourceX;
    const double dy = (std::max)({y - area.centerY, 0.0, area.centerY - (y + height)}) / kPixelsPerSourceY;
    // Keep mathematical tangency inclusive after affine subtraction/roundoff.
    return dx * dx + dy * dy <= 4.0 + 1.0e-12;
}

bool FusionCherryLadderOverlaps(const FusionCherryArea& area,
    double cellCenterX, double cellCenterY)
{
    // SetGridItem0x180458E90; source prefab BoxCollider size(.98,2.09),
    // root scale.7, zero collider offset. This is the existing ladder's shape.
    const double width = 0.98 * 0.7 * kPixelsPerSourceX;
    const double height = 2.09 * 0.7 * kPixelsPerSourceY;
    return FusionCherryRectOverlaps(area,
        cellCenterX + 0.3 * kPixelsPerSourceX - width * 0.5,
        cellCenterY - 0.6 * kPixelsPerSourceY - height * 0.5,
        width, height);
}
}
