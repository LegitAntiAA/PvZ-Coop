#pragma once
namespace LawnFusion
{
// HOST AI only. Context key is LOCAL full generation; context owns epoch.
// No fields belong in FNS, animation sampling, or client simulation.
struct NativeNextZombie343State
{
    float attributeCountdown=0;
    int attributeTick=-1;
    bool initialized=false,failed=false,deathConsumed=false;
};
}
