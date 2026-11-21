#ifndef LAWN_FUSION_WORLD_PARTICLE269_RULES_H
#define LAWN_FUSION_WORLD_PARTICLE269_RULES_H
#include <span>
#include <vector>
#include <array>
namespace LawnFusion
{
struct FusionWorldParticle269Key {float time=0,value=0,inSlope=0,outSlope=0;};
struct FusionWorldParticle269Sample {float time=0,low=0,high=0;};
bool SampleFusionWorldParticle269Curve(std::span<const FusionWorldParticle269Key>,float time,float& value);
// Produces native FIELD_FRICTION coefficients from source speed multipliers.
// Native particle age is the only time: v(n)=v(n-1)*(1-coefficient(n)).
// A zero speed may remain zero; resurrection cannot be represented by friction.
bool BuildFusionWorldParticle269Friction(std::span<const FusionWorldParticle269Key>,int actualParticleTicks,std::vector<FusionWorldParticle269Sample>&);
// Native CIRCLE followed by AWAY equals one planar rotation without Euler
// radius drift. The caller maps Unity Z orientation into Board orientation.
bool TryGetFusionWorldParticle269Orbit(float radiansPerSecond,float nativeStep,float& circle,float& away);
struct FusionWorldParticle269Transform
{
    std::array<float,3> position={0,0,0},scale={1,1,1};
    std::array<float,4> rotation={0,0,0,1}; // Unity quaternion x/y/z/w.
};
struct FusionWorldParticle269Rectangle
{
    float halfX=0,halfY=0,launchDegrees=0,launchSpeedScale=0;
    bool operator==(const FusionWorldParticle269Rectangle&) const=default;
};
// Source Local scaling mode: compose ALL ancestor rotations, but only the PS
// transform's local scale. Matrix is row-major, source XYZ -> source world XYZ.
bool ComposeFusionWorldParticle269LocalBasis(std::span<const FusionWorldParticle269Transform>,std::array<float,9>&);
// Preserve two independent uniform birth samples. A projected skew rectangle,
// or two nonzero samples collapsing onto one axis, cannot be a native BOX.
bool ProjectFusionWorldParticle269Rectangle(const std::array<float,9>& basis,
    const std::array<float,2>& halfSize,float pixelsX,float pixelsY,FusionWorldParticle269Rectangle&);
}
#endif
