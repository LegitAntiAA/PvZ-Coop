#ifndef LAWN_COIN_MOTION_H
#define LAWN_COIN_MOTION_H

#include <cmath>

namespace LawnCoinMotion
{
struct CollectionStep
{
    float x;
    float y;
    float distance;
};

// Original Coin::UpdateCollected math for exactly one native 100 Hz tick.
// distance belongs to the position BEFORE movement: scoring and scale use it.
inline CollectionStep AdvanceCollection(float x, float y, float targetX, float targetY)
{
    const float deltaX = std::fabs(x - targetX);
    const float deltaY = std::fabs(y - targetY);
    if (x > targetX) x -= deltaX / 21.0f;
    else if (x < targetX) x += deltaX / 21.0f;
    if (y > targetY) y -= deltaY / 21.0f;
    else if (y < targetY) y += deltaY / 21.0f;
    return {x, y, std::sqrt(deltaY * deltaY + deltaX * deltaX)};
}

inline float CollectionScale(float distance, float sunScale)
{
    float scale = distance * 0.05f;
    if (scale < 0.5f) scale = 0.5f;
    else if (scale > 1.0f) scale = 1.0f;
    return scale * sunScale;
}
}

#endif
