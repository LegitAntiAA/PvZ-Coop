#ifndef LAWN_FUSION_BUTTER_RULES_H
#define LAWN_FUSION_BUTTER_RULES_H

namespace LawnFusion
{
// CornUmbrella.BlockEffect: a live UmbrellaPot extends4s to6s.
constexpr int kFusionButterMaximumTicks = 600;
constexpr int kFusionButterSpriteThresholdTicks = 50;

bool FusionButterStateValid(int ticks, bool visible);
bool TryGetFusionButterSplashTicks(int targetCount, int& ticks);
bool TryPrepareFusionButter(int beforeTicks, bool beforeVisible, int ticks, bool& visible, bool showSprite = true);
}
#endif
