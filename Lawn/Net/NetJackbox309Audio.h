#pragma once
#include "../Fusion/FusionNativeJackbox300.h"
#include <memory>
class LawnApp;
class Board;
class Zombie;
namespace LawnNet
{
// Board-owned presentation only. No source clock, RNG or gameplay callback.
class NetJackbox309Audio
{
public:
    explicit NetJackbox309Audio(LawnApp*);
    ~NetJackbox309Audio();
    void Apply(Zombie*,LawnFusion::NativeJackbox300Audio);
    void Retire(Zombie*);
    void Update(Board*);
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
bool NetSourceJackbox309Ready(LawnApp*);
void NetSourceJackbox309Loop(Zombie*,LawnFusion::NativeJackbox300Audio);
void NetSourceJackbox309Retire(Zombie*);
// Runs on both peers from the app frame, including pause/menu frames.
void NetUpdateSourceJackbox309Audio(Board*);
}
