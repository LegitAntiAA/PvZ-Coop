#ifndef LAWN_FUSION_NATIVE_BUTTER_H
#define LAWN_FUSION_NATIVE_BUTTER_H

class Zombie;

namespace LawnFusion
{
bool ApplyNativeFusionButter(Zombie* zombie, int ticks, bool showSprite = true);
bool ShouldDrawNativeButter(const Zombie* zombie);
}
#endif
