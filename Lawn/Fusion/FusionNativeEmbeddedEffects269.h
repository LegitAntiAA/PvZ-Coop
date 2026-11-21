#ifndef LAWN_FUSION_NATIVE_EMBEDDED_EFFECTS269_H
#define LAWN_FUSION_NATIVE_EMBEDDED_EFFECTS269_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
class Board;class Plant;class Projectile;class Zombie;class LawnApp;class TodParticleSystem;
namespace LawnNet { struct NetFusionLimbEvent276; }
namespace LawnFusion
{
class NativeWorldParticle269Definition;
// Root callbacks into the existing NativeSourceEffects269 resource cache.
NativeWorldParticle269Definition* EnsureNativeEmbeddedParticle269Resource(Board*,uint16_t resourceKind,const std::string& effectJson,std::string& error);
TodParticleSystem* CreateNativeEmbeddedParticle269System(Board*,uint16_t resourceKind,NativeWorldParticle269Definition*,float x,float y,int order,bool emissionEnabled);
// Same sampled hierarchy/options as the existing native plant Draw. This must
// include parent activity, source curves and current runtime node overrides.
bool GetNativeSourcePlantNodeActivity(const Plant*,const char* node,bool& active);
// Root delegates this wrapper to its existing SourceEffects269-owned helper.
// Global manifest is fusion/effects/embedded269/manifest.json beside pilot.
bool EnsureNativeEmbedded269Model(Board*,uint16_t plantKind,const std::string& modelJson,std::string* error=nullptr);
bool EnsureNativeEmbedded269ProjectileModel(Board*,uint16_t modelKey,const std::string& modelJson,std::string* error=nullptr);
bool EnsureNativeEmbedded269ZombieModel(Board*,uint16_t kind,const std::string& modelJson,std::string* error=nullptr);
// E1's source visual cache resolves only actual typed AUX/Bullet models.
uint16_t GetNativeEmbedded269ProjectileModelKey(const Projectile*);
bool GetNativeSourceProjectile269NodeActivity(const Projectile*,const char*,bool&);
bool GetNativeSourceProjectile269NodePoint(const Projectile*,const char*,float&,float&);
// External source models derived from a real Plant and canonical FNS state.
uint16_t GetNativeEmbedded269DerivedPlantModelKey(const Plant*);
bool GetNativeSourceDerivedPlant269NodeActivity(const Plant*,const char*,bool&);
bool GetNativeSourceDerivedPlant269NodePoint(const Plant*,const char*,float&,float&);
// Own this presentation lease/cache inside existing NativeSourceEffects269.
// Actual emitters, particles, ages, draw order and ticking remain native Tod.
class NativeEmbeddedEffects269
{
public:
    explicit NativeEmbeddedEffects269(LawnApp*);
    ~NativeEmbeddedEffects269();
    bool Ensure(Board*,uint16_t plantKind,const std::string& modelJson,const std::string& bindingManifest,std::string& error);
    bool EnsureProjectile(Board*,uint16_t modelKey,const std::string& modelJson,const std::string& bindingManifest,std::string& error);
    bool EnsureZombie(Board*,uint16_t kind,const std::string& modelJson,const std::string& bindingManifest,std::string& error);
    bool EnsureField277(Board*,uint16_t scene,const std::string& model,const std::string& manifest,std::vector<int64_t>& proof,std::string& error);
    bool FieldResourceAdmitted277(uint16_t resource) const;
    void ReconcileField277(Board*,uint16_t scene,bool enabled,float x,float y,int order);
    bool QueryField277(const Board*,uint16_t& resource,uint32_t& system,float& x,float& y) const;
    // Canonical host event, including before owner birth/after owner deletion.
    // Consume once per epoch/id/token/kind/part; no actor lookup or phase clock.
    bool PlayZombieLimbEvent(Board*,const LawnNet::NetFusionLimbEvent276&,bool& newEvent);
    void Update(Board*);
    // Exact source emission-module callbacks may override a dormant module;
    // absence of this callback never silently enables source-disabled emission.
    bool SetEmission(Plant*,const char* node,bool enabled);
    bool Play(Plant*,const char* node);
    bool StopAndClear(Plant*,const char* node);
private:
    struct Impl;std::unique_ptr<Impl> mImpl;
};
}
#endif
