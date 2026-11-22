#pragma once
namespace LawnFusion
{
// HOST AI only. Context key is LOCAL full generation; context owns epoch.
// No fields belong in FNS, animation sampling, or client simulation.
struct NativeNextZombie343State
{
    float attributeCountdown=0;
    float snowTargetX363=0,snowTargetY363=0; //49 source cached world point, host-private.
    bool snowShooting363=false;
    int attributeTick=-1;
    bool initialized=false,failed=false,deathConsumed=false;
};
}
