#ifndef LAWN_FUSION_POTATO_RULES_H
#define LAWN_FUSION_POTATO_RULES_H

namespace LawnFusion
{
constexpr int kFusionPotatoRiseTicks = 84; // ceil(source AnimRiseOver .8333333s / .01s)
constexpr int kFusionPotatoDeathTicks = 10;
constexpr int kFusionPotatoDamage = 1800;

// Source Update tests countdown before PlantUpdate subtracts deltaTime.
// Native UpdateAbilities subtracts first, so retain that one-tick ordering.
bool TryGetFusionPotatoPreparationTicks(float sourceSeconds, int& ticks);
bool FusionPotatoCanTrigger(int sourceStatus, bool sameRow, bool mindControlled,
    bool beforeDying, bool armed, bool alreadyExploded);
bool FusionPotatoCanDamage(int sourceStatus, bool sameRow, bool mindControlled);

struct FusionPotatoArea
{
    double centerX = 0.0, centerY = 0.0;
    double triggerX = 0.0, triggerY = 0.0;
};

// Actual source Shadow/trigger offsets projected onto the native72x100 grid.
// Zombies retain their native rectangles; this is an explicit adaptation.
bool TryMakeFusionPotatoArea(double cellCenterX, double cellCenterY, FusionPotatoArea& result);
bool FusionPotatoTriggerOverlaps(const FusionPotatoArea& area,
    double x, double y, double width, double height);
bool FusionPotatoBlastOverlaps(const FusionPotatoArea& area,
    double x, double y, double width, double height);
}
#endif
