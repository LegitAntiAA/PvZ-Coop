#ifndef LAWN_FUSION_NATIVE_WORLD_PARTICLE269_H
#define LAWN_FUSION_NATIVE_WORLD_PARTICLE269_H
#include <memory>
#include <cstdint>
#include <string>
class LawnApp;class TodParticleDefinition;class TodParticleSystem;
namespace LawnFusion
{
// Resource only. The existing Tod holder owns actual systems/particles/clocks.
// Root's NativeSourceEffects269 cache retains this beside its source52 data.
class NativeWorldParticle269Definition
{
public:
    NativeWorldParticle269Definition();
    ~NativeWorldParticle269Definition();
    NativeWorldParticle269Definition(const NativeWorldParticle269Definition&)=delete;
    NativeWorldParticle269Definition& operator=(const NativeWorldParticle269Definition&)=delete;
    bool Load(LawnApp*,uint16_t sourceKind,const std::string& packageJsonPath,float pixelsX,float pixelsY,std::string& error);
    TodParticleDefinition* Data(bool emissionEnabled=true) const;
    bool SetEmission(TodParticleSystem*,bool enabled) const;
    // Bind the actual Board row-plane after allocation, before the next native
    // update. The caller supplies the source-backed plane in Board pixels.
    bool BindSourceCollision(TodParticleSystem*,float groundY,bool zombieHeadCallback,std::string* error=nullptr) const;
private:
    struct Impl;std::unique_ptr<Impl> mImpl;
};
}
#endif
