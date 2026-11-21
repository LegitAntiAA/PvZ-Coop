#include "FusionButterRules.h"
#include <algorithm>

namespace LawnFusion
{
bool FusionButterStateValid(int ticks, bool visible)
{
    return ticks >= 0 && ticks <= kFusionButterMaximumTicks && (ticks > 0 || !visible);
}

bool TryGetFusionButterSplashTicks(int targetCount, int& ticks)
{
    if (targetCount <= 0) return false;
    // Source divides the integers BEFORE converting the quotient to float.
    const float seconds = (std::max)(.5f, static_cast<float>(4 / targetCount));
    ticks = static_cast<int>(seconds * 100.0f);
    return true;
}

bool TryPrepareFusionButter(int beforeTicks, bool beforeVisible, int ticks, bool& visible, bool showSprite)
{
    if (!FusionButterStateValid(beforeTicks, beforeVisible) || ticks <= 0 ||
        ticks > kFusionButterMaximumTicks) return false;
    // A short refresh does not remove a sprite already present in source.
    visible = beforeVisible || (showSprite && ticks > kFusionButterSpriteThresholdTicks);
    return true;
}
}
