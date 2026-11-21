#ifndef LAWN_FUSION_CHERRY_RULES_H
#define LAWN_FUSION_CHERRY_RULES_H

namespace LawnFusion
{
// Fusion's source setting is0..5; the existing co-op setting is1..10.
// The current native slice uses the original source default, normal2.
constexpr int kFusionSourceNormalDifficulty = 2;
constexpr int kFusionCherryFuseTicks = 100;

struct FusionCherryDamage
{
    bool instantBurn = false;
    int damage = 0;
};

// Ordinary source Charred1800, without Travel/buffs/special zombie overrides.
// Invalid input leaves the output unchanged.
bool TryResolveFusionCherryDamage(int sourceDifficulty, float bodyHealth,
    int firstArmorHealth, FusionCherryDamage& result);

struct FusionCherryArea
{
    double centerX = 0.0;
    double centerY = 0.0;
};

// Bare grass10x5: source circle2 is projected by the actual72x100 cell grid.
// The native cell center comes from Board; no viewport or skin offset is used.
bool TryMakeFusionCherryArea(double cellCenterX, double cellCenterY, FusionCherryArea& result);
// The real native zombie/grid-item rectangle is the deliberate collider
// adaptation. This does not substitute Unity colliders or create another world.
bool FusionCherryRectOverlaps(const FusionCherryArea& area,
    double x, double y, double width, double height);
// Source GridItem.SetGridItem3 places Image/Ladder/Ladder at box+(.3,.6).
bool FusionCherryLadderOverlaps(const FusionCherryArea& area,
    double cellCenterX, double cellCenterY);
}

#endif
