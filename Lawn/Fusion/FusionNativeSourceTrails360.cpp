#include "FusionNativeSourceTrails360.h"
#include "FusionWorldParticle269Rules.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/Trail.h"
#include "../../Sexy.TodLib/Definition.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include <stdexcept>
namespace LawnFusion {namespace {
using Json=nlohmann::json;
std::unordered_map<const TodEmitterDefinition*,NativeSourceTrails360*>& Registry(){static std::unordered_map<const TodEmitterDefinition*,NativeSourceTrails360*> map;return map;}
float Number(const Json& j,float a,float b){float v=j.get<float>();if(!std::isfinite(v)||v<a||v>b)throw std::runtime_error("Trail360 scalar out of range");return v;}
struct Key {TodParticleHolder* holder=nullptr;uint32_t uid=0;bool operator==(const Key&)const=default;};
struct KeyHash{size_t operator()(const Key& k)const{return (reinterpret_cast<size_t>(k.holder)>>4)^k.uid;}};
struct Lease {
 uint32_t trailUID=0;TrailHolder* holder=nullptr;TrailDefinition definition;
 FloatParameterTrackNode width{},one{},life{};
 Lease(){width={0,0,0,TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR};one={0,1,1,TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR};life={0,2,2,TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR};
  // Existing native loop keeps this presentation lease alive. Its two-frame
  // phase has constant width/alpha and no source meaning; the actual parent
  // particle's age/deletion is the sole source TTL, including system pause.
  definition.mTrailFlags=1<<int(TrailFlags::TRAIL_FLAG_LOOPS);
  definition.mWidthOverTime={&width,1};definition.mAlphaOverLength={&one,1};definition.mAlphaOverTime={&one,1};definition.mTrailDuration={&life,1};definition.mMaxPoints=20;}
};
}
struct NativeSourceTrails360::Impl {
 LawnApp* app=nullptr;std::unique_ptr<Sexy::MemoryImage> material;
 std::vector<FloatParameterTrackNode> widths;float minimumDistance=0;
 const TodEmitterDefinition* normal=nullptr;const TodEmitterDefinition* muted=nullptr;
 std::unordered_map<Key,std::unique_ptr<Lease>,KeyHash> leases;
 Trail* Get(const Lease& l)const{auto* t=app==gLawnApp&&app&&app->mEffectSystem&&app->mEffectSystem->mTrailHolder==l.holder?l.holder->mTrails.DataArrayTryToGet(l.trailUID):nullptr;return t&&t->mDefinition==&l.definition?t:nullptr;}
 void Remove(const Key& k){auto i=leases.find(k);if(i==leases.end())return;if(auto* t=Get(*i->second))t->mDead=true;leases.erase(i);}
 ~Impl(){for(auto& [key,l]:leases)if(auto* t=Get(*l))t->mDead=true;Registry().erase(normal);Registry().erase(muted);}
 void Observe(TodParticle* p){
  auto* emitter=p?p->mParticleEmitter:nullptr;auto* system=emitter?emitter->mParticleSystem:nullptr;auto* ph=system?system->mParticleHolder:nullptr;
  if(!ph||app!=gLawnApp||!app||!app->mEffectSystem||app->mEffectSystem->mParticleHolder!=ph||!app->mEffectSystem->mTrailHolder)return;
  const uint32_t uid=ph->mParticles.DataArrayGetID(p);if(!uid||ph->mParticles.DataArrayTryToGet(uid)!=p)return;Key key{ph,uid};
  auto i=leases.find(key);if(i==leases.end()){
   auto lease=std::make_unique<Lease>();lease->holder=app->mEffectSystem->mTrailHolder;lease->definition.mImage=material.get();lease->definition.mMinPointDistance=minimumDistance;lease->definition.mWidthOverLength={widths.data(),int(widths.size())};
   auto* trail=lease->holder->AllocTrailFromDef(system->mRenderOrder,&lease->definition);if(!trail)return; // Cosmetic pool pressure: no gameplay mutation/retry state.
   lease->trailUID=lease->holder->mTrails.DataArrayGetID(trail);i=leases.emplace(key,std::move(lease)).first;
  }
  auto& lease=*i->second;auto* trail=Get(lease);if(!trail||trail->mDead){Remove(key);return;}
  ParticleRenderParams render;if(!TodParticleEmitter::GetRenderParams(p,&render))return;
  if(!std::isfinite(render.mPosX+render.mPosY+render.mParticleScale+render.mRed+render.mGreen+render.mBlue+render.mAlpha)){Remove(key);return;}
  const float half=std::abs(render.mParticleScale)*.5f;lease.width.mLowValue=lease.width.mHighValue=half;
  trail->mRenderOrder=system->mRenderOrder;trail->mColorOverride={std::clamp(int(render.mRed),0,255),std::clamp(int(render.mGreen),0,255),std::clamp(int(render.mBlue),0,255),std::clamp(int(render.mAlpha),0,255)};
  trail->AddPoint(render.mPosX,render.mPosY);
 }
};
NativeSourceTrails360::NativeSourceTrails360()=default;NativeSourceTrails360::~NativeSourceTrails360()=default;
bool NativeSourceTrails360::Load(LawnApp* app,const std::string& text,float pixels,std::string& error){try{
 if(mImpl||!app||!std::isfinite(pixels)||pixels<=0)throw std::runtime_error("invalid Trail360 resource context");const Json d=Json::parse(text);
 if(d.at("schema")!=1||d.at("policy").get<std::string>()!="nativeTrail20FlatAlpha360"||d.at("mode")!=0||d.at("ratio")!=1||d.at("lifetime")!=1||d.at("nativeMaximumPoints")!=20||d.at("sourceRendererMode")!=5||d.at("sourceBuiltinMaterial")!=10308||
  !d.at("worldSpace").get<bool>()||!d.at("dieWithParticles").get<bool>()||!d.at("sizeAffectsWidth").get<bool>()||d.at("sizeAffectsLifetime").get<bool>()||!d.at("inheritParticleColor").get<bool>()||d.at("textureMode")!=0||d.at("textureScale")!=Json::array({1,1})||d.at("serializedTrailModule").get<std::string>().empty()||d.at("serializedParticleRenderer").get<std::string>().empty())throw std::runtime_error("unsupported Trail360 source subset");
 std::vector<FusionWorldParticle269Key> source;for(const auto& k:d.at("widthCurve")){if(k.size()!=4)throw std::runtime_error("invalid Trail360 width key");source.push_back({Number(k[0],0,1),Number(k[1],0,20),Number(k[2],-10000,10000),Number(k[3],-10000,10000)});}if(source.size()<2||source.size()>128)throw std::runtime_error("invalid Trail360 width curve");
 auto next=std::make_unique<Impl>();next->app=app;next->minimumDistance=Number(d.at("minVertexDistance"),.000001f,1)*pixels;
 // Native flat-alpha material is an explicit supported projection of builtin
 // 10308, not an exported/invented Unity PNG. SDL consumes this native1px texel.
 next->material=std::make_unique<Sexy::MemoryImage>(app);next->material->Create(1,1);next->material->GetBits()[0]=0xffffffff;next->material->BitsChanged();
 for(int n=0;n<=100;++n){float value=0;if(!SampleFusionWorldParticle269Curve(source,n*.01f,value)||value<0||value>20)throw std::runtime_error("invalid sampled Trail360 width");next->widths.push_back({n*.01f,value,value,TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR});}
 mImpl=std::move(next);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool NativeSourceTrails360::Register(TodEmitterDefinition* normal,TodEmitterDefinition* muted){if(!mImpl||!normal||!muted||Registry().contains(normal)||Registry().contains(muted))return false;Registry()[normal]=this;Registry()[muted]=this;mImpl->normal=normal;mImpl->muted=muted;return true;}
void ObserveNativeSourceParticleTrail360(TodParticle* p){if(!p||!p->mParticleEmitter||Registry().empty())return;auto i=Registry().find(p->mParticleEmitter->mEmitterDef);if(i!=Registry().end())i->second->mImpl->Observe(p);}
void RemoveNativeSourceParticleTrail360(TodParticle* p){if(!p||!p->mParticleEmitter||Registry().empty())return;auto* e=p->mParticleEmitter;auto i=Registry().find(e->mEmitterDef);if(i==Registry().end()||!e->mParticleSystem||!e->mParticleSystem->mParticleHolder)return;auto* holder=e->mParticleSystem->mParticleHolder;const uint32_t uid=holder->mParticles.DataArrayGetID(p);if(uid)i->second->mImpl->Remove({holder,uid});}
void DrawNativeSourceParticleTrail360(TodParticle* p,Sexy::Graphics* g){if(!g||!p||!p->mParticleEmitter||Registry().empty())return;auto* e=p->mParticleEmitter;auto i=Registry().find(e->mEmitterDef);if(i==Registry().end()||!e->mParticleSystem||!e->mParticleSystem->mParticleHolder)return;auto* holder=e->mParticleSystem->mParticleHolder;auto& state=*i->second->mImpl;auto lease=state.leases.find({holder,holder->mParticles.DataArrayGetID(p)});if(lease!=state.leases.end())if(auto* trail=state.Get(*lease->second))trail->Draw(g);}
}
