#pragma once
#include <memory>
#include <string>
class LawnApp;class TodParticle;class TodEmitterDefinition;
namespace Sexy {class Graphics;}
namespace LawnFusion {
// Source visual adaptation uses existing native particles and native Trail20.
// No actor, particle trajectory, age or gameplay clock is owned by this cache.
class NativeSourceTrails360 {
public:
 NativeSourceTrails360();~NativeSourceTrails360();
 bool Load(LawnApp*,const std::string& sourceTrailJson,float pixels,std::string& error);
 bool Register(TodEmitterDefinition* normal,TodEmitterDefinition* muted);
private:
 struct Impl;std::unique_ptr<Impl> mImpl;
 friend void ObserveNativeSourceParticleTrail360(TodParticle*);
 friend void RemoveNativeSourceParticleTrail360(TodParticle*);
 friend void DrawNativeSourceParticleTrail360(TodParticle*,Sexy::Graphics*);
};
// Narrow TodParticle.Update/Delete hooks. Lookup is by actual immutable source
// emitter-definition pointer; Classic emitters never acquire source trails.
void ObserveNativeSourceParticleTrail360(TodParticle*);
void RemoveNativeSourceParticleTrail360(TodParticle*);
void DrawNativeSourceParticleTrail360(TodParticle*,Sexy::Graphics*);
}
