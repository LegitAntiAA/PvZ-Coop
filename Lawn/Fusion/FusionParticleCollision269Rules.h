#ifndef LAWN_FUSION_PARTICLE_COLLISION269_RULES_H
#define LAWN_FUSION_PARTICLE_COLLISION269_RULES_H
namespace LawnFusion
{
struct FusionParticleCollision269Profile
{
    float damping=0, bounce=0, lifetimeLoss=0, minimumSpeed=0, maximumSpeed=10000;
};
struct FusionParticleCollision269State
{
    float y=0, velocityX=0, velocityY=0;
    int age=0, duration=1;
};
bool FusionParticleCollision269ProfileValid(const FusionParticleCollision269Profile&);
// Native Board Y points down. Velocities are pixels per existing 10ms tick.
// No separate clock: lifetime loss advances the actual native particle age.
bool StepFusionParticleCollision269(const FusionParticleCollision269Profile&,float pixelsX,float pixelsY,
    float groundY,float radius,bool settled,FusionParticleCollision269State&,bool& hit,bool& killed);
}
#endif
