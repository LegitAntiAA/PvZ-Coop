#include "FusionZombieMotion.h"
#include <cmath>
#include <limits>
#include <span>

namespace LawnFusion
{
namespace
{
struct RootKey { float time, value, inSlope, outSlope; };
// Source237: walk8ba53789... and walk2 0/2 share the verified controller.
constexpr RootKey walkKeys[] = {
    {0,-.81666666f,0,-.7999996f},
    {.16666667f,-.9499999f,-1.5999992f,-1.5999992f},
    {.33333334f,-1.39f,-2.3400002f,-2.3400002f},
    {.5f,-1.7299999f,-1.57f,-1.5699999f},
    {.6666667f,-1.9133333f,-.3599997f,-.35999972f},
    {.8333333f,-1.9433333f,4.0829256e-8f,-(std::numeric_limits<float>::infinity)()},
    {1,-1.9433333f,0,0},
    {1.1666666f,-2.4266665f,-2.9500003f,-2.95f},
    {1.3333334f,-2.9266667f,-2.1599998f,-2.1599998f},
    {1.5f,-3.1466665f,-1.5299997f,-1.5299996f},
    {1.6666666f,-3.4366665f,2.6702745e-7f,0}};
constexpr RootKey walk2Keys[] = {
    {0,3.1064515f,0,-1.9102385f},
    {.38333333f,2.3741934f,-1.7032251f,-1.7032256f},
    {.6666667f,1.9709678f,-9.8540255e-8f,0},
    {.81666666f,2.0225806f,-2.9481981e-8f,0},
    {.96666664f,1.9612902f,-.865276f,-.8652759f},
    {1.1f,1.6935483f,-2.5161293f,-2.5161285f},
    {1.3333334f,1.0387096f,-2.1977112f,-2.1977108f},
    {1.6166667f,.55806446f,-1.6963955f,0}};
bool Supported(uint16_t kind) { return kind==0 || kind==2; }
std::span<const RootKey> Keys(bool walk2)
{ return walk2 ? std::span<const RootKey>(walk2Keys) : std::span<const RootKey>(walkKeys); }
bool PositionValid(FusionZombieClipPosition position)
{ return std::isfinite(position.seconds) && position.seconds>=0 && position.seconds<=kFusionZombieWalkClipSeconds; }
float Sample(std::span<const RootKey> keys, float seconds)
{
    if(seconds<=keys.front().time)return keys.front().value;
    if(seconds>=keys.back().time)return keys.back().value;
    size_t next=1;while(keys[next].time<=seconds)++next;
    const auto& a=keys[next-1];const auto& b=keys[next];
    if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.value;
    // Same Hermite/step convention as FusionAnimation::Sample, without an
    // image/renderer/parser dependency or omission of the source Infinity key.
    const float duration=b.time-a.time,t=(seconds-a.time)/duration,t2=t*t,t3=t2*t;
    return (2*t3-3*t2+1)*a.value+(t3-2*t2+t)*duration*a.outSlope+
        (-2*t3+3*t2)*b.value+(t3-t2)*duration*b.inSlope;
}
}

bool TryGetFusionZombieWalkRate(uint16_t kind, bool walk2, float originSpeed, float statusFactor, float& output)
{
    if(!Supported(kind)||!std::isfinite(originSpeed)||originSpeed<kFusionZombieMinimumOriginSpeed||
        originSpeed>kFusionZombieMaximumOriginSpeed||!std::isfinite(statusFactor)||
        (statusFactor!=0 && statusFactor!=.5f && statusFactor!=1))return false;
    const float animatorSpeed=originSpeed*statusFactor;
    output=(walk2?.5f:.4f)*animatorSpeed;
    return true;
}

bool TrySampleFusionZombieRootX(uint16_t kind, bool walk2, float clipSeconds, float& output)
{
    if(!Supported(kind)||!PositionValid({0,clipSeconds}))return false;
    const float sample=Sample(Keys(walk2),clipSeconds);
    if(!std::isfinite(sample))return false;
    output=sample;
    return true;
}

bool TryGetFusionZombieRootDelta(uint16_t kind, bool walk2, FusionZombieClipPosition previous,
    FusionZombieClipPosition current, bool facingRight, double& output, double explicitScale)
{
    if(!Supported(kind)||!PositionValid(previous)||!PositionValid(current)||current.loop<previous.loop||
        (current.loop==previous.loop&&current.seconds<previous.seconds)||!std::isfinite(explicitScale)||explicitScale<0)return false;
    float before=0,after=0;
    if(!TrySampleFusionZombieRootX(kind,walk2,previous.seconds,before)||!TrySampleFusionZombieRootX(kind,walk2,current.seconds,after))return false;
    const auto keys=Keys(walk2);
    const double cycle=static_cast<double>(keys.back().value)-keys.front().value;
    // Subtract loop counters before multiplying to avoid cancellation of huge
    // absolute positions when only the current local frame is needed.
    const double raw=static_cast<double>(current.loop-previous.loop)*cycle+(static_cast<double>(after)-before);
    const double scaled=(facingRight?-raw:raw)*explicitScale;
    if(!std::isfinite(scaled))return false;
    output=scaled;
    return true;
}
}
