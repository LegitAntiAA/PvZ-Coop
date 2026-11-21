#ifndef LAWN_FUSION_DEATH_RULES_H
#define LAWN_FUSION_DEATH_RULES_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace LawnFusion
{
enum class FusionDeathBlastPolicy { Cherry, SmallPotato };
struct FusionDeathProfile
{
    uint16_t kind;
    int maxHealth;
    FusionDeathBlastPolicy policy;
    bool createsMines;
};
const FusionDeathProfile* FindFusionDeathProfile(uint16_t kind);
bool FusionDeathFlagsValid(uint16_t kind, uint8_t flags);
bool FusionDeathReasonHasEffects(int sourceReason);
// Invalid arguments leave flags and damage unchanged. Consumed bands never reset.
bool TryConsumeFusionPotatoNutThreshold(int health, int maxHealth, uint8_t& flags, int& damage);
bool TryApplyFusionDeathHeal(int health, int maxHealth, int amount, int& healedHealth);

struct NativeBlastSpec
{
    FusionDeathBlastPolicy policy = FusionDeathBlastPolicy::Cherry;
    double centerX = 0.0, centerY = 0.0;
    int row = 0;
    int damage = 1800;
    bool shake = true;
};
bool FusionDeathBlastValid(const NativeBlastSpec& spec);
// The bare-grass cell center is supplied by the native Board, without skin offsets.
bool TryMakeFusionDeathBlast(uint16_t kind, double cellCenterX, double cellCenterY,
    int row, bool thresholdBlast, NativeBlastSpec& spec);
bool FusionDeathBlastAffectsRow(const NativeBlastSpec& spec, int row);
bool FusionDeathBlastRectOverlaps(const NativeBlastSpec& spec,
    double x, double y, double width, double height);
// Two independently enabled source CherryNut colliders; not a fixed x2 reward.
int FusionCherryNutHealOverlapCount(const NativeBlastSpec& spec,
    double cellCenterX, double cellCenterY, int row);

struct FusionDeathSpawnCell { int column = 0, row = 0; };
bool TryGetFusionDeathSpawnCells(int column, int row, int columns, int rows,
    std::array<FusionDeathSpawnCell, 9>& cells, size_t& count);
constexpr int kFusionDeathMinePreparationTicks = 501;
}
#endif
