#include "FusionNativeEmbeddedEffects269.h"
#include "FusionNativeWorldParticle269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeLaserConnections269.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeDayZombie273.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../third_party/nlohmann/json.hpp"
#include <Windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace LawnFusion
{
namespace
{
using Json=nlohmann::json;
Json Read(const std::string& path){std::ifstream file(path);if(!file)throw std::runtime_error("missing embedded source particle metadata");return Json::parse(file);}
std::string Digest(const std::string& value)
{
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))throw std::runtime_error("embedded SHA provider unavailable");
    bool okay=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
    for(std::size_t i=0;okay&&i<value.size();i+=65536)okay=CryptHashData(hash,reinterpret_cast<const BYTE*>(value.data()+i),DWORD((std::min)(size_t(65536),value.size()-i)),0)!=0;
    BYTE bytes[32]{};DWORD count=32;if(okay)okay=CryptGetHashParam(hash,HP_HASHVAL,bytes,&count,0)!=0&&count==32;if(hash)CryptDestroyHash(hash);CryptReleaseContext(provider,0);
    if(!okay)throw std::runtime_error("embedded SHA validation failed");std::string out;for(BYTE b:bytes){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;
}
struct Binding
{
    int64_t component=0,renderer=0;uint16_t resource=0;std::string path,package;bool emission=true;int sortingOrder=0;NativeWorldParticle269Definition* definition=nullptr;
};
void ValidateOwnerDestroyedParticle273(uint16_t kind,const Json& entry,const std::string& modelPath)
{
    // Source Doll.FirstArmorFall synchronously activates Cone.LoseCone and
    // destroys its owning root with Die(2), before any rendering. Retain and
    // verify both serialized components; this does not choose a null-Sprite
    // fallback or permit other dormant/unimplemented particle systems.
    const char* proofHash=kind==21?"4cb918fe380100c4a163eef3bc4ad3dcb24c4b7fa4f3537618bddff7ea7568b5":
        kind==22?"a797f4452feeff9ca28937ece6a5d3054a31a4630277377b646b3b8cbbb8416f":
        kind==23?"a468ea2e0ec6ff7f78bf911b74d9cd460953e3a01801116550e2d82f5360b807":nullptr;
    const char* modelHash=kind==21?"d83ea3808a7c29e19c1898bc64d8ec7ab4b50b1bd72ca1762ea39c01a8df381c":
        kind==22?"339a11cdf48a1be7899f9368ee00cabcbcb7442ea4ca8159796cd1cee9082703":
        kind==23?"24acb26ca0edb834836f978ad8e84dd3a4cf4411665708b98bf852832b4fb0b4":nullptr;
    const auto& proof=entry.at("lifecycleProof");
    if(!proofHash||entry.at("nativeLifecycle")!="ownerDestroyedBeforeRender"||
        entry.at("canonicalProofSha256")!=proofHash||Digest(proof.dump())!=proofHash||
        entry.at("sourceModelSha256")!=modelHash)
        throw std::runtime_error("source Doll particle lifecycle proof differs");
    for(const char* field:{"sourceZombieKind","sourcePrefab","sourceParticleFileId","sourceRendererFileId",
        "serializedParticleSha256","serializedRendererSha256","nodePath"})
        if(proof.at(field)!=entry.at(field))throw std::runtime_error("source Doll lifecycle binding differs");
    std::ifstream file(modelPath,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(file)),{});
    if(!file||Digest(bytes)!=modelHash)throw std::runtime_error("source Doll lifecycle model hierarchy differs");
}
std::vector<Binding> LoadBindings(uint16_t kind,const std::string& modelPath,const std::string& manifestPath,bool projectile=false,bool zombie=false,bool field=false)
{
    const Json model=Read(modelPath),manifest=Read(manifestPath);if(manifest.at("schema")!=1)throw std::runtime_error("unknown embedded source manifest");
    const auto& families=manifest.at(field?"fields":zombie?"zombies":projectile?"projectiles":"plants");const char* key=field?"sourceSceneType":projectile?"modelKey":"kind";
    const auto plant=std::find_if(families.begin(),families.end(),[=](const Json& p){return p.at(key)==kind;});
    if(plant==families.end()||plant->at("status")!="Passed")throw std::runtime_error("source embedded particle family remains blocked");
    if(field)
    {
        const char* prefab=kind==1?"Resources/background/background2.prefab":kind==2?"Resources/background/background3.prefab":nullptr;
        const char* background=kind==1?"BACKGROUND_2_NIGHT":"BACKGROUND_3_POOL";
        std::ifstream file(modelPath,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(file)),{});
        if(!prefab||manifest.at("ownerScope")!="nativeBoardField277"||plant->at("nativeBackground").get<std::string>()!=background||
            model.at("sourcePrefab").get<std::string>()!=prefab||plant->at("sourcePrefab").get<std::string>()!=prefab||!file||Digest(bytes)!=plant->at("sourceModelSha256").get<std::string>())
            throw std::runtime_error("source field model identity differs");
    }
    std::map<std::pair<int,int64_t>,std::string> actual;
    for(const auto& component:model.at("nonRenderingComponents"))
    {
        const int type=component.at("typeId").get<int>();if(type!=198&&type!=199)continue;
        const auto key=std::make_pair(type,component.at("fileId").get<int64_t>());
        if(!actual.emplace(key,component.at("serializedComponent").get<std::string>()).second)throw std::runtime_error("duplicate source particle component");
    }
    if(actual.empty()&&!field)throw std::runtime_error("model has no source particle components");
    std::set<std::pair<int,int64_t>> covered;std::vector<Binding> result;size_t bindingCount=0;
    for(const auto& entry:manifest.at("bindings"))if(entry.at(field?"sourceSceneType":zombie?"sourceZombieKind":projectile?"modelKey":"plantKind")==kind)
    {
        if(entry.at("status")!="Passed"||entry.at("sourcePrefab")!=model.at("sourcePrefab"))throw std::runtime_error("embedded source binding differs");
        const int64_t ps=entry.at("sourceParticleFileId").get<int64_t>(),renderer=entry.at("sourceRendererFileId").get<int64_t>();
        for(const auto pair:{std::make_pair(198,ps),std::make_pair(199,renderer)})
        {
            const auto found=actual.find(pair);if(found==actual.end()||!covered.insert(pair).second)throw std::runtime_error("foreign/duplicate embedded particle component");
            const auto hash=entry.at(pair.first==198?"serializedParticleSha256":"serializedRendererSha256").get<std::string>();
            if(Digest(found->second)!=hash)throw std::runtime_error("source particle component hash differs");
        }
        ++bindingCount;
        if(entry.contains("nativeLifecycle"))
        {
            if(!zombie||projectile||field)throw std::runtime_error("foreign source particle lifecycle proof");
            ValidateOwnerDestroyedParticle273(kind,entry,modelPath);continue;
        }
        Binding binding;binding.component=ps;binding.renderer=renderer;const int id=entry.at("resourceKind").get<int>();if(id<1||id>65535)throw std::runtime_error("invalid native particle resource key");binding.resource=uint16_t(id);
        if(field&&(kind!=2||id!=6000))throw std::runtime_error("unregistered native field particle resource");
        binding.path=entry.at("nodePath").get<std::string>();binding.emission=entry.at("defaultEmissionEnabled").get<bool>();
        if(std::none_of(model.at("nodes").begin(),model.at("nodes").end(),[&](const Json& n){return n.at("path")==binding.path;}))throw std::runtime_error("missing actual source particle node");
        const auto resource=std::find_if(manifest.at("resources").begin(),manifest.at("resources").end(),[&](const Json& r){return r.at("resourceKind")==id;});
        if(resource==manifest.at("resources").end()||resource->at("status")!="Passed")throw std::runtime_error("missing compiled source particle resource");
        const auto relative=std::filesystem::path(resource->at("package").get<std::string>());if(relative.is_absolute()||relative.lexically_normal().string().find("..")!=std::string::npos)throw std::runtime_error("invalid embedded particle resource path");
        binding.package=(std::filesystem::path(manifestPath).parent_path()/relative).string();std::ifstream file(binding.package,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(file)),{});
        if(!file||Digest(bytes)!=resource->at("effectSha256").get<std::string>())throw std::runtime_error("compiled source particle resource hash differs");
        if(field)
        {
            const Json effect=Json::parse(bytes);const auto& rectangle=effect.at("emitters").at(0).at("shape").at("projectedRectangle");
            if(entry.at("sourceModelSha256")!=plant->at("sourceModelSha256")||
                rectangle.at("sourceModelSha256")!=plant->at("sourceModelSha256")||
                effect.at("sourceComponentIds")!=Json::array({ps})||effect.at("sourcePrefab")!=model.at("sourcePrefab")||
                effect.at("sourceKind")!=id||!effect.at("ignoredBindings").empty()||!effect.at("deferredComponents").empty()||
                rectangle.at("sourcePrefabSha256")!=entry.at("sourcePrefabSha256")||effect.at("sourcePrefabSha256")!=entry.at("sourcePrefabSha256"))
                throw std::runtime_error("source field particle/model proof differs");
            const auto& chain=entry.at("sourceHierarchy");
            if(chain.size()!=rectangle.at("ancestorTrs").size()||chain.size()!=rectangle.at("ancestorPaths").size())
                throw std::runtime_error("source field particle ancestry count differs");
            int parent=-1;
            for(size_t i=0;i<chain.size();++i)
            {
                const auto& record=chain.at(i);const auto& nodes=model.at("nodes");
                const auto node=std::find_if(nodes.begin(),nodes.end(),[&](const Json& n){return n.at("path")==record.at("path");});
                if(node==nodes.end()||node->at("parent")!=parent||record.at("path")!=rectangle.at("ancestorPaths").at(i))
                    throw std::runtime_error("source field particle ancestry differs");
                for(const char* transform:{"position","rotation","scale"})
                    if(node->at(transform)!=record.at(transform)||record.at(transform)!=rectangle.at("ancestorTrs").at(i).at(transform))
                        throw std::runtime_error("source field particle transform differs");
                parent=int(node-nodes.begin());
            }
            if(chain.empty()||chain.back().at("path")!=binding.path)
                throw std::runtime_error("source field particle node differs");
        }
        const auto& raw=actual.at({199,renderer});const std::string tag="  m_SortingOrder: ";const auto at=raw.find(tag);if(at!=std::string::npos)binding.sortingOrder=std::stoi(raw.substr(at+tag.size()));
        result.push_back(std::move(binding));
    }
    if(covered.size()!=actual.size()||bindingCount!=plant->at("count").get<std::size_t>())throw std::runtime_error("unbound source particle/renderer component");return result;
}
}
struct NativeEmbeddedEffects269::Impl
{
    struct Lease {uint32_t system=0;bool active=false,stopped=false,detached=false;int emission=-1;};
    LawnApp* app=nullptr;TodParticleHolder* holder=nullptr;
    std::map<uint16_t,std::vector<Binding>> bindings;
    std::map<std::pair<uint32_t,int64_t>,Lease> leases;
    // Separate real DataArray domain: a Plant and Projectile may have the same
    // generation. Model key also distinguishes source AUX families/components.
    std::map<uint16_t,std::vector<Binding>> projectileBindings;
    std::map<std::tuple<uint32_t,uint16_t,int64_t>,Lease> projectileLeases;
    std::map<std::tuple<uint32_t,uint16_t,int64_t>,Lease> derivedPlantLeases;
    std::map<uint16_t,std::vector<Binding>> zombieBindings;
    std::map<uint16_t,std::vector<Binding>> fieldBindings;
    uint16_t fieldScene=0,fieldResource=0;uint32_t fieldSystem=0;float fieldX=0,fieldY=0;
    std::map<std::tuple<uint32_t,uint16_t,int64_t>,Lease> zombieLeases;
    // Separate from actor-owned polling leases: a reliable loss can arrive
    // before the first actor snapshot or after its native array slot is gone.
    using LimbKey=std::tuple<uint32_t,uint16_t,uint32_t,uint16_t,bool>;
    std::map<LimbKey,uint32_t> limbEvents; // Seen key -> optional real native PS ID.
    uint32_t limbEpoch=0;
    // Never evict canonical keys to make old packets replayable. At this board
    // budget (16-bit actor ID space x two parts), further cosmetics only drop.
    static constexpr size_t maximumLimbEvents=2u*(std::numeric_limits<uint16_t>::max)();
    TodParticleSystem* System(uint32_t id) const
    {return id&&app&&app->mEffectSystem&&app->mEffectSystem->mParticleHolder==holder?app->ParticleTryToGet(static_cast<ParticleSystemID>(id)):nullptr;}
    void Kill(Lease& lease){if(auto* system=System(lease.system))system->ParticleSystemDie();lease.system=0;}
    void StopField(){if(auto* system=System(fieldSystem))system->ParticleSystemDie();fieldScene=fieldResource=0;fieldSystem=0;fieldX=fieldY=0;}
    bool Valid(Board* board) const{return board&&app&&app->mBoard==board&&board->mApp==app&&IsNativeFusionBoard(board)&&app->mEffectSystem&&app->mEffectSystem->mParticleHolder==holder&&holder;}
};
NativeEmbeddedEffects269::NativeEmbeddedEffects269(LawnApp* app):mImpl(std::make_unique<Impl>())
{mImpl->app=app;mImpl->holder=app&&app->mEffectSystem?app->mEffectSystem->mParticleHolder:nullptr;}
NativeEmbeddedEffects269::~NativeEmbeddedEffects269(){mImpl->StopField();for(auto& entry:mImpl->leases)mImpl->Kill(entry.second);for(auto& entry:mImpl->projectileLeases)mImpl->Kill(entry.second);for(auto& entry:mImpl->derivedPlantLeases)mImpl->Kill(entry.second);for(auto& entry:mImpl->zombieLeases)mImpl->Kill(entry.second);for(auto& entry:mImpl->limbEvents)if(auto* system=mImpl->System(entry.second))system->ParticleSystemDie();}
bool NativeEmbeddedEffects269::Ensure(Board* board,uint16_t kind,const std::string& model,const std::string& manifest,std::string& error)
{
    try
    {
        if(!mImpl->Valid(board))throw std::runtime_error("native embedded particle board unavailable");if(mImpl->bindings.contains(kind)){error.clear();return true;}
        auto bindings=LoadBindings(kind,model,manifest);for(auto& binding:bindings){binding.definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);if(!binding.definition)return false;}
        mImpl->bindings.emplace(kind,std::move(bindings));error.clear();return true;
    }
    catch(const std::exception& ex){error=ex.what();return false;}
}
bool NativeEmbeddedEffects269::EnsureProjectile(Board* board,uint16_t kind,const std::string& model,const std::string& manifest,std::string& error)
{
    try
    {
        if(!kind||!mImpl->Valid(board))throw std::runtime_error("native embedded projectile board unavailable");
        if(mImpl->projectileBindings.contains(kind)){error.clear();return true;}
        auto bindings=LoadBindings(kind,model,manifest,true);
        for(auto& binding:bindings){binding.definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);if(!binding.definition)return false;}
        mImpl->projectileBindings.emplace(kind,std::move(bindings));error.clear();return true;
    }
    catch(const std::exception& ex){error=ex.what();return false;}
}
bool NativeEmbeddedEffects269::EnsureZombie(Board* board,uint16_t kind,const std::string& model,const std::string& manifest,std::string& error)
{
    try
    {
        if(!mImpl->Valid(board))throw std::runtime_error("native embedded zombie board unavailable");
        if(mImpl->zombieBindings.contains(kind)){error.clear();return true;}
        auto bindings=LoadBindings(kind,model,manifest,false,true);
        for(auto& binding:bindings){binding.definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);if(!binding.definition)return false;}
        mImpl->zombieBindings.emplace(kind,std::move(bindings));error.clear();return true;
    }
    catch(const std::exception& ex){error=ex.what();return false;}
}
bool NativeEmbeddedEffects269::EnsureField277(Board* board,uint16_t scene,const std::string& model,const std::string& manifest,std::vector<int64_t>& proof,std::string& error)
{
    proof.clear();
    try
    {
        if(!mImpl->Valid(board))throw std::runtime_error("native field particle Board unavailable");
        if(!((scene==1&&board->mBackground==BACKGROUND_2_NIGHT)||(scene==2&&board->mBackground==BACKGROUND_3_POOL)))
            throw std::runtime_error("native field scene/background differs");
        auto found=mImpl->fieldBindings.find(scene);
        if(found==mImpl->fieldBindings.end())
        {
            auto bindings=LoadBindings(scene,model,manifest,false,false,true);
            for(auto& binding:bindings){binding.definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);if(!binding.definition)return false;}
            found=mImpl->fieldBindings.emplace(scene,std::move(bindings)).first;
        }
        for(const auto& b:found->second){proof.push_back(b.component);proof.push_back(b.renderer);}
        error.clear();return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
bool NativeEmbeddedEffects269::FieldResourceAdmitted277(uint16_t resource) const
{
    const auto found=mImpl->fieldBindings.find(2);
    return resource==6000&&found!=mImpl->fieldBindings.end()&&found->second.size()==1&&
        found->second.front().resource==resource&&found->second.front().definition;
}
void NativeEmbeddedEffects269::ReconcileField277(Board* board,uint16_t scene,bool enabled,float x,float y,int order)
{
    if(!enabled||scene!=2||!mImpl->Valid(board)||board->mBackground!=BACKGROUND_3_POOL||!std::isfinite(x)||!std::isfinite(y))
    {mImpl->StopField();return;}
    const auto found=mImpl->fieldBindings.find(scene);
    if(found==mImpl->fieldBindings.end()||found->second.size()!=1){mImpl->StopField();return;}
    const auto& b=found->second.front();
    if(mImpl->fieldScene!=scene||mImpl->fieldResource!=b.resource)mImpl->StopField();
    mImpl->fieldScene=scene;mImpl->fieldResource=b.resource;mImpl->fieldX=x;mImpl->fieldY=y;
    auto* system=mImpl->System(mImpl->fieldSystem);
    if(!system||system->mDead)
    {
        system=CreateNativeEmbeddedParticle269System(board,b.resource,b.definition,x,y,order,b.emission);
        mImpl->fieldSystem=system?uint32_t(mImpl->app->ParticleGetID(system)):0;
        if(system)LawnNet::NetTrace("Fusion field277 water started background=%d resource=%u system=%u anchor=(%.3f,%.3f) order=%d localOnly=1",int(board->mBackground),b.resource,mImpl->fieldSystem,x,y,order);
    }
    if(system){system->mDontUpdate=board->mPaused;system->mRenderOrder=order;system->SystemMove(x,y);}
}
bool NativeEmbeddedEffects269::QueryField277(const Board* board,uint16_t& resource,uint32_t& id,float& x,float& y) const
{
    resource=0;id=0;x=y=0;
    if(!mImpl->Valid(const_cast<Board*>(board))||mImpl->fieldScene!=2)return false;
    resource=mImpl->fieldResource;x=mImpl->fieldX;y=mImpl->fieldY;
    const auto* system=mImpl->System(mImpl->fieldSystem);
    const auto found=mImpl->fieldBindings.find(2);
    if(system&&!system->mDead&&system->mEffectType==PARTICLE_FUSION_EMBEDDED269&&found!=mImpl->fieldBindings.end()&&
        found->second.size()==1&&system->mParticleDef==found->second.front().definition->Data(found->second.front().emission))id=mImpl->fieldSystem;
    return true;
}
bool NativeEmbeddedEffects269::PlayZombieLimbEvent(Board* board,const LawnNet::NetFusionLimbEvent276& event,bool& newEvent)
{
    newEvent=false;
    const auto* context=GetNativeContext(board);
    if(!mImpl->Valid(board)||!context||context->epoch!=event.mEpoch||
        board->mBackground!=BACKGROUND_1_DAY||GetNativeFusionRowCount(board)!=5||
        !LawnNet::NetFusionLimbEvent276Valid(event))return false;
    if(mImpl->limbEpoch!=event.mEpoch)
    {
        for(auto& entry:mImpl->limbEvents)if(auto* system=mImpl->System(entry.second))system->ParticleSystemDie();
        mImpl->limbEvents.clear();mImpl->limbEpoch=event.mEpoch;
    }
    const Impl::LimbKey key{event.mEpoch,event.mZombieId,event.mZombieToken,event.mSourceKind,event.mHead};
    if(mImpl->limbEvents.contains(key))
    {
        LawnNet::NetTrace("Fusion Day limb276 duplicate epoch=%u kind=%u id=%u token=%u part=%s role=%s",
            event.mEpoch,event.mSourceKind,event.mZombieId,event.mZombieToken,event.mHead?"head":"arm",
            mImpl->app->NetIsClient()?"client":"host");return true;
    }
    if(mImpl->limbEvents.size()>=Impl::maximumLimbEvents)
    {
        LawnNet::NetTrace("Fusion Day limb276 dropped epoch=%u kind=%u id=%u token=%u part=%s row=%u birth=(%.3f,%.3f) reason=board-seen-capacity role=%s",
            event.mEpoch,event.mSourceKind,event.mZombieId,event.mZombieToken,event.mHead?"head":"arm",event.mRow,
            event.mX,event.mY,mImpl->app->NetIsClient()?"client":"host");return true;
    }
    // Consume BEFORE any resource/particle allocation. A cosmetic failure is
    // terminal for this event, independent of gameplay and owner lifetime.
    auto& systemId=mImpl->limbEvents.emplace(key,0).first->second;newEvent=true;
    const char* path=event.mHead?"LoseHead":"LoseArm";
    const auto drop=[&](const char* reason)
    {
        LawnNet::NetTrace("Fusion Day limb276 dropped epoch=%u kind=%u id=%u token=%u part=%s node=%s row=%u birth=(%.3f,%.3f) reason=%s role=%s",
            event.mEpoch,event.mSourceKind,event.mZombieId,event.mZombieToken,event.mHead?"head":"arm",path,event.mRow,
            event.mX,event.mY,reason,mImpl->app->NetIsClient()?"client":"host");return true;
    };
    std::string error;
    // Resources are admitted without requiring the owner to exist. The selected
    // model's exact source bindings retain their genuine scales/lifetimes.
    if(!EnsureNativeSourceZombieVisuals(board,event.mSourceKind,&error))return drop("source-resource");
    const auto found=mImpl->zombieBindings.find(event.mSourceKind);
    if(found==mImpl->zombieBindings.end())return drop("source-binding");
    const auto binding=std::find_if(found->second.begin(),found->second.end(),[&](const Binding& b){return b.path==path;});
    if(binding==found->second.end())return drop("source-loss-node");
    auto* definition=EnsureNativeEmbeddedParticle269Resource(board,binding->resource,binding->package,error);
    if(!definition)return drop("particle-resource");
    // AnimLoseActive37EF60 overwrites the serialized PS renderer order(19/20)
    // with owner SortingGroup+1. Do not add that discarded source default.
    auto* system=CreateNativeEmbeddedParticle269System(board,binding->resource,definition,event.mX,event.mY,
        event.mRenderOrder+1,binding->emission);
    if(!system)return drop("particle-capacity");
    if(!definition->BindSourceCollision(system,float(board->GridToPixelY(0,event.mRow)+80),true,&error))
    {system->ParticleSystemDie();return drop("collision-plane");}
    systemId=uint32_t(mImpl->app->ParticleGetID(system));
    LawnNet::NetTrace("Fusion Day limb276 started epoch=%u kind=%u id=%u token=%u part=%s node=%s row=%u resource=%u system=%u birth=(%.3f,%.3f) order=%d role=%s",
        event.mEpoch,event.mSourceKind,event.mZombieId,event.mZombieToken,event.mHead?"head":"arm",path,event.mRow,binding->resource,systemId,event.mX,event.mY,event.mRenderOrder+1,
        mImpl->app->NetIsClient()?"client":"host");
    return true;
}
void NativeEmbeddedEffects269::Update(Board* board)
{
    if(!mImpl->Valid(board)||board->mPaused||mImpl->app->mGameScene!=SCENE_PLAYING)return;
    // Raw-body resource preflight can precede background adoption. Resolve the
    // current immutable projection definition on the real native update phase.
    for(auto& entry:mImpl->bindings)for(auto& binding:entry.second)
    {
        std::string error;auto* definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);
        if(definition==binding.definition)continue;
        for(auto& held:mImpl->leases)if(held.first.second==binding.component)
        {
            auto* owner=board->mPlants.DataArrayTryToGet(held.first.first);const auto* data=owner?GetNativePlantData(owner):nullptr;
            if(data&&data->kind==entry.first){mImpl->Kill(held.second);held.second.active=false;}
        }
        binding.definition=definition; // Failed projection admission cannot use old geometry.
    }
    std::set<std::pair<uint32_t,int64_t>> observed;Plant* plant=nullptr;const auto* context=GetNativeContext(board);
    while(board->IteratePlants(plant))
    {
        const auto* data=GetNativePlantData(plant);if(!data||plant->mDead||!plant->IsOnBoard())continue;
        bool held=false;for(const auto& hold:context->gloveHolds)if(hold.plantId==plant->mNetId&&hold.instanceToken==data->instanceToken)held=true;if(held)continue;
        const auto found=mImpl->bindings.find(data->kind);if(found==mImpl->bindings.end())continue;const uint32_t generation=board->mPlants.DataArrayGetID(plant);
        for(const auto& binding:found->second)
        {
            const auto key=std::make_pair(generation,binding.component);observed.insert(key);auto& lease=mImpl->leases[key];bool active=false;float x=0,y=0;
            if(!binding.definition){mImpl->Kill(lease);lease.active=false;continue;}
            if(!GetNativeSourcePlantNodeActivity(plant,binding.path.c_str(),active)||!GetNativeSourcePlantNodePoint(plant,binding.path.c_str(),x,y)||!std::isfinite(x)||!std::isfinite(y))active=false;
            if(!active){mImpl->Kill(lease);lease.active=false;lease.stopped=false;continue;}
            const bool emission=lease.emission<0?binding.emission:lease.emission!=0;active=active&&!lease.stopped;
            if(!active){mImpl->Kill(lease);lease.active=false;continue;}
            if(!lease.active)
            {
                if(auto* system=CreateNativeEmbeddedParticle269System(board,binding.resource,binding.definition,x,y,plant->mRenderOrder+binding.sortingOrder+1,emission))lease.system=uint32_t(mImpl->app->ParticleGetID(system));
                else continue; // Actual failed allocation may retry; no fake accepted birth.
                lease.active=true;
            }
            if(auto* system=mImpl->System(lease.system)){binding.definition->SetEmission(system,emission);system->SystemMove(x,y);}
            // Natural one-shot completion keeps active=true. Only a real source
            // node re-enable/Play rearms it; a draw/update never restarts the clock.
        }
    }
    for(auto it=mImpl->leases.begin();it!=mImpl->leases.end();)if(!observed.contains(it->first)){mImpl->Kill(it->second);it=mImpl->leases.erase(it);}else ++it;
    for(auto& entry:mImpl->projectileBindings)for(auto& binding:entry.second)
    {
        std::string error;auto* definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);
        if(definition==binding.definition)continue;
        for(auto& held:mImpl->projectileLeases)if(std::get<1>(held.first)==entry.first&&std::get<2>(held.first)==binding.component)
        {mImpl->Kill(held.second);held.second.active=false;}
        for(auto& held:mImpl->derivedPlantLeases)if(std::get<1>(held.first)==entry.first&&std::get<2>(held.first)==binding.component)
        {mImpl->Kill(held.second);held.second.active=false;}
        binding.definition=definition;
    }
    std::set<std::tuple<uint32_t,uint16_t,int64_t>> projectileObserved;Projectile* projectile=nullptr;
    while(board->IterateProjectiles(projectile))
    {
        if(projectile->mDead)continue;const uint16_t kind=GetNativeEmbedded269ProjectileModelKey(projectile);if(!kind)continue;
        const auto found=mImpl->projectileBindings.find(kind);if(found==mImpl->projectileBindings.end())continue;
        const uint32_t generation=board->mProjectiles.DataArrayGetID(projectile);if(!generation)continue;
        for(const auto& binding:found->second)
        {
            const auto key=std::make_tuple(generation,kind,binding.component);projectileObserved.insert(key);auto& lease=mImpl->projectileLeases[key];
            bool active=false;float x=0,y=0;
            if(!binding.definition||!GetNativeSourceProjectile269NodeActivity(projectile,binding.path.c_str(),active)||
               !GetNativeSourceProjectile269NodePoint(projectile,binding.path.c_str(),x,y)||!std::isfinite(x)||!std::isfinite(y))active=false;
            if(!active){mImpl->Kill(lease);lease.active=false;continue;}
            if(!lease.active)
            {
                if(auto* system=CreateNativeEmbeddedParticle269System(board,binding.resource,binding.definition,x,y,projectile->mRenderOrder+binding.sortingOrder+1,binding.emission))
                    lease.system=uint32_t(mImpl->app->ParticleGetID(system));
                else continue;
                lease.active=true;
            }
            if(auto* system=mImpl->System(lease.system)){binding.definition->SetEmission(system,binding.emission);system->SystemMove(x,y);}
        }
    }
    for(auto it=mImpl->projectileLeases.begin();it!=mImpl->projectileLeases.end();)
        if(!projectileObserved.contains(it->first)){mImpl->Kill(it->second);it=mImpl->projectileLeases.erase(it);}else ++it;
    // External LightBall belongs to the actual connected main Plant generation.
    // Resource cache is shared; leases are a separate real Plant identity domain.
    std::set<std::tuple<uint32_t,uint16_t,int64_t>> derivedObserved;plant=nullptr;
    while(board->IteratePlants(plant))
    {
        if(plant->mDead)continue;const uint16_t kind=GetNativeEmbedded269DerivedPlantModelKey(plant);if(!kind)continue;
        const auto found=mImpl->projectileBindings.find(kind);if(found==mImpl->projectileBindings.end())continue;
        const uint32_t generation=board->mPlants.DataArrayGetID(plant);if(!generation)continue;
        float ballX=0,ballY=0;int row=plant->mRow;
        if(kind==4937&&!GetNativeLaserConnections269BallPoint(plant,ballX,ballY,row))continue;
        for(const auto& binding:found->second)
        {
            const auto key=std::make_tuple(generation,kind,binding.component);derivedObserved.insert(key);auto& lease=mImpl->derivedPlantLeases[key];
            bool active=false;float x=0,y=0;
            if(!binding.definition||!GetNativeSourceDerivedPlant269NodeActivity(plant,binding.path.c_str(),active)||
               !GetNativeSourceDerivedPlant269NodePoint(plant,binding.path.c_str(),x,y)||!std::isfinite(x)||!std::isfinite(y))active=false;
            if(!active){mImpl->Kill(lease);lease.active=false;continue;}
            if(!lease.active)
            {
                if(auto* system=CreateNativeEmbeddedParticle269System(board,binding.resource,binding.definition,x,y,
                   Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row,binding.sortingOrder+1),binding.emission))lease.system=uint32_t(mImpl->app->ParticleGetID(system));
                else continue;
                lease.active=true;
            }
            if(auto* system=mImpl->System(lease.system)){binding.definition->SetEmission(system,binding.emission);system->SystemMove(x,y);}
        }
    }
    for(auto it=mImpl->derivedPlantLeases.begin();it!=mImpl->derivedPlantLeases.end();)
        if(!derivedObserved.contains(it->first)){mImpl->Kill(it->second);it=mImpl->derivedPlantLeases.erase(it);}else ++it;
    std::set<std::tuple<uint32_t,uint16_t,int64_t>> zombieObserved;Zombie* zombie=nullptr;
    while(board->IterateZombies(zombie))
    {
        const auto* data=GetNativeSourceZombieData(zombie);if(!data||zombie->mDead)continue;
        const auto found=mImpl->zombieBindings.find(data->kind);if(found==mImpl->zombieBindings.end())continue;
        const uint32_t generation=board->mZombies.DataArrayGetID(zombie);if(!generation)continue;
        for(auto& binding:found->second)
        {
            const auto key=std::make_tuple(generation,data->kind,binding.component);zombieObserved.insert(key);
            // Day body-part effects have a synchronous authoritative loss
            // callback. A first already-headless/armless sample is a baseline,
            // not a new event; keep existing15/104 polling unchanged.
            if(IsNativeDayZombie273(zombie)&&(binding.path=="LoseHead"||binding.path.starts_with("LoseHead/")||
                binding.path=="LoseArm"||binding.path.starts_with("LoseArm/")))continue;
            auto& lease=mImpl->zombieLeases[key];bool active=false;float x=0,y=0;
            // AnimLoseActive detached the real source subtree. Its native Tod
            // particles now keep their birth transform and own lifetime.
            if(lease.detached)continue;
            std::string error;auto* definition=EnsureNativeEmbeddedParticle269Resource(board,binding.resource,binding.package,error);
            if(definition!=binding.definition){mImpl->Kill(lease);lease.active=false;binding.definition=definition;}
            if(!definition||!GetNativeSourceZombie269NodeActivity(zombie,binding.path.c_str(),active)||
               !GetNativeSourceZombie269NodePoint(zombie,binding.path.c_str(),x,y))active=false;
            if(!active){mImpl->Kill(lease);lease.active=false;continue;}
            if(!lease.active)
            {
                auto* system=CreateNativeEmbeddedParticle269System(board,binding.resource,definition,x,y,
                    zombie->mRenderOrder+binding.sortingOrder+1,binding.emission);
                if(!system)continue;
                const bool detached=binding.path=="LoseHead"||binding.path.starts_with("LoseHead/")||
                    binding.path=="LoseArm"||binding.path.starts_with("LoseArm/");
                const bool sourcePlane=binding.path=="LoseHead"||binding.path=="LoseArm";
                // Native flat-row floor projection; roof is not admitted here.
                // Nested PS did not receive AnimLoseActive's AddPlane callback.
                if(sourcePlane&&!definition->BindSourceCollision(system,float(board->GridToPixelY(0,zombie->mRow)+80),true,&error))
                {system->ParticleSystemDie();continue;}
                lease.system=uint32_t(mImpl->app->ParticleGetID(system));lease.active=true;
                lease.detached=detached;
            }
            if(auto* system=mImpl->System(lease.system))
            {if(!lease.detached){definition->SetEmission(system,binding.emission);system->SystemMove(x,y);}}
        }
    }
    for(auto it=mImpl->zombieLeases.begin();it!=mImpl->zombieLeases.end();)
        if(!zombieObserved.contains(it->first)){if(!it->second.detached)mImpl->Kill(it->second);it=mImpl->zombieLeases.erase(it);}else ++it;
}
bool NativeEmbeddedEffects269::SetEmission(Plant* plant,const char* path,bool enabled)
{
    const auto* data=GetNativePlantData(plant);if(!data||!path||!mImpl->Valid(plant->mBoard))return false;const auto found=mImpl->bindings.find(data->kind);if(found==mImpl->bindings.end())return false;
    for(const auto& binding:found->second)if(binding.path==path){auto& lease=mImpl->leases[{plant->mBoard->mPlants.DataArrayGetID(plant),binding.component}];lease.emission=enabled?1:0;return true;}return false;
}
bool NativeEmbeddedEffects269::Play(Plant* plant,const char* path)
{
    const auto* data=GetNativePlantData(plant);if(!data||!path||!mImpl->Valid(plant->mBoard))return false;const auto found=mImpl->bindings.find(data->kind);if(found==mImpl->bindings.end())return false;
    for(const auto& binding:found->second)if(binding.path==path){auto& lease=mImpl->leases[{plant->mBoard->mPlants.DataArrayGetID(plant),binding.component}];if(mImpl->System(lease.system)&&!lease.stopped)return true;mImpl->Kill(lease);lease.active=false;lease.stopped=false;return true;}return false;
}
bool NativeEmbeddedEffects269::StopAndClear(Plant* plant,const char* path)
{
    const auto* data=GetNativePlantData(plant);if(!data||!path||!mImpl->Valid(plant->mBoard))return false;const auto found=mImpl->bindings.find(data->kind);if(found==mImpl->bindings.end())return false;
    for(const auto& binding:found->second)if(binding.path==path){auto& lease=mImpl->leases[{plant->mBoard->mPlants.DataArrayGetID(plant),binding.component}];mImpl->Kill(lease);lease.stopped=true;return true;}return false;
}
}
