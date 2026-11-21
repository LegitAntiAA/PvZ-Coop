#ifndef LAWN_FUSION_NATIVE_EFFECTS269_H
#define LAWN_FUSION_NATIVE_EFFECTS269_H
#include "FusionNativeDoom269.h"
#include <cstdint>
#include <memory>
#include <string>
class Board;class LawnApp;class TodParticleSystem;class TodParticleDefinition;
namespace LawnFusion
{
class NativeWorldParticle269Definition;
// Resource cache only. Native TodParticleSystem owns emitters, particles and clocks.
class NativeSourceEffects269
{
public:
    explicit NativeSourceEffects269(LawnApp*);
    ~NativeSourceEffects269();
    NativeSourceEffects269(const NativeSourceEffects269&)=delete;
    NativeSourceEffects269& operator=(const NativeSourceEffects269&)=delete;
    bool Ensure(Board*,uint16_t sourceKind,std::string& error);
    TodParticleSystem* Create(Board*,uint16_t sourceKind,float x,float y,int order,bool emission=true);
    NativeWorldParticle269Definition* WorldDefinition(Board*,uint16_t,const std::string& path,std::string& error);
    bool EnsureEmbedded(Board*,uint16_t,const std::string& modelPath,std::string& error);
    bool EnsureEmbeddedProjectile(Board*,uint16_t,const std::string& modelPath,std::string& error);
    bool EnsureEmbeddedZombie(Board*,uint16_t,const std::string& modelPath,std::string& error);
    void UpdateEmbedded(Board*);
    bool BirthPoints(Board*,uint16_t sourceKind,float x,float y,std::array<NativeDoom269Point,8>&) const;
private:
    struct Impl;std::unique_ptr<Impl> mImpl;
};
// Root owns NativeContext unique_ptr and mode/receiver admission. Both validated
// host and reliable cosmetic receiver may create this resource cache.
NativeSourceEffects269* GetNativeSourceEffects269(Board*,bool create=false);
// Explicit reliable-event mapping; dynamic cache resource IDs never enter the
// native stock array or the network presentation event.
int NativeSourceEffectParticle269(uint16_t sourceKind);
bool NativeSourceEffectKind269(int particleEffect,uint16_t& sourceKind);
TodParticleSystem* SpawnNativeSourceEffect269(Board*,uint16_t sourceKind,float x,float y,int order,bool broadcast);
bool EnsureNativeSourceEffect269(Board*,uint16_t sourceKind,std::string* error=nullptr);
// Actual native system creation succeeds before any source birth points publish.
bool CreateNativeIceDoom269Cloud(Plant*,float x,float y,std::array<NativeDoom269Point,8>&);
}
#endif
