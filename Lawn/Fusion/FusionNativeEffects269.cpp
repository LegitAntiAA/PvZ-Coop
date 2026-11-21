#include "FusionNativeEffects269.h"
#include "FusionNativeWorldParticle269.h"
#include "FusionNativeEmbeddedEffects269.h"
#include <tuple>
#include "FusionNative.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/SharedImage.h"
#include "../../SexyAppFramework/Image.h"
#include "../../third_party/nlohmann/json.hpp"
#include <Windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

namespace LawnFusion
{
namespace
{
using Json=nlohmann::json;
constexpr double pi=3.14159265358979323846;
std::filesystem::path ResourceRoot()
{
    std::wstring buffer(32768,L'\0');const DWORD count=GetModuleFileNameW(nullptr,buffer.data(),DWORD(buffer.size()));
    if(!count||count>=buffer.size())throw std::runtime_error("cannot locate native source effects");
    buffer.resize(count);return std::filesystem::path(buffer).parent_path()/L"fusion"/L"effects";
}
std::string Sha256(const std::filesystem::path& path)
{
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing source particle texture");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))throw std::runtime_error("source particle SHA provider unavailable");
    bool okay=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
    for(size_t i=0;okay&&i<bytes.size();i+=65536)okay=CryptHashData(hash,bytes.data()+i,DWORD((std::min)(size_t(65536),bytes.size()-i)),0)!=0;
    BYTE digest[32]{};DWORD count=32;if(okay)okay=CryptGetHashParam(hash,HP_HASHVAL,digest,&count,0)!=0&&count==32;
    if(hash)CryptDestroyHash(hash);CryptReleaseContext(provider,0);if(!okay)throw std::runtime_error("cannot verify source particle texture");
    std::string out;for(BYTE v:digest){out+="0123456789abcdef"[v>>4];out+="0123456789abcdef"[v&15];}return out;
}
float Number(const Json& value,float minimum,float maximum)
{
    const float x=value.get<float>();if(!std::isfinite(x)||x<minimum||x>maximum)throw std::runtime_error("source particle value outside supported range");return x;
}
int Ticks(float seconds){return (std::max)(2,int(std::ceil(double(seconds)*100-1e-5)));}
struct Definition
{
    TodParticleDefinition native{};
    std::vector<TodEmitterDefinition> emitters;
    std::vector<std::string> names;
    std::vector<std::vector<FloatParameterTrackNode>> tracks;
    std::vector<Sexy::SharedImageRef> images;
    std::array<NativeDoom269Point,8> births{}; // Original source units, no new actor state.
    void Track(FloatParameterTrack& target,std::initializer_list<std::array<float,3>> values)
    {
        tracks.emplace_back();auto& nodes=tracks.back();nodes.reserve(values.size());
        for(const auto& v:values)nodes.push_back({v[0],v[1],v[2],TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR});
        target.mNodes=nodes.data();target.mCountNodes=int(nodes.size());
    }
    void Constant(FloatParameterTrack& target,float x){Track(target,{{0,x,x}});}
};
}
struct NativeSourceEffects269::Impl
{
    LawnApp* app=nullptr;
    TodParticleHolder* holder=nullptr; // Checked against the live EffectSystem before use.
    std::map<std::pair<float,float>,std::unique_ptr<Definition>> definitions;
    std::map<std::tuple<uint16_t,float,float>,std::unique_ptr<NativeWorldParticle269Definition>> worldDefinitions;
    std::unique_ptr<NativeEmbeddedEffects269> embedded;
};
NativeSourceEffects269::NativeSourceEffects269(LawnApp* app):mImpl(std::make_unique<Impl>())
{mImpl->app=app;mImpl->holder=app&&app->mEffectSystem?app->mEffectSystem->mParticleHolder:nullptr;}
NativeSourceEffects269::~NativeSourceEffects269()
{
    // EffectSystemDispose nulls holder and LawnApp nulls EffectSystem after deletion.
    // Never dereference the retained holder if it no longer belongs to the live app.
    auto* app=mImpl->app;
    if(!app||!app->mEffectSystem||app->mEffectSystem->mParticleHolder!=mImpl->holder||!mImpl->holder)return;
    mImpl->embedded.reset();
    TodParticleSystem* system=nullptr;
    while(mImpl->holder->mParticleSystems.IterateNext(system))
        if(!system->mDead)for(const auto& entry:mImpl->definitions)
            if(system->mParticleDef==&entry.second->native){system->ParticleSystemDie();break;}
    system=nullptr;
    while(mImpl->holder->mParticleSystems.IterateNext(system))
        if(!system->mDead)for(const auto& entry:mImpl->worldDefinitions)
            if(system->mParticleDef==entry.second->Data(true)||system->mParticleDef==entry.second->Data(false)){system->ParticleSystemDie();break;}
}
bool NativeSourceEffects269::Ensure(Board* board,uint16_t kind,std::string& error)
{
    if(kind==35||kind==36||kind==49||kind==51||kind==99||kind==100||(kind>=2000&&kind<2100))
    {const auto path=kind<2000?ResourceRoot()/ ("source-"+std::to_string(kind))/"effect.json":ResourceRoot()/"embedded269"/std::to_string(kind)/"effect.json";
     return WorldDefinition(board,kind,path.string(),error)!=nullptr;}

    try
    {
        if(kind!=52||!board||!IsNativeFusionBoard(board)||board->mApp!=mImpl->app||!mImpl->app->mEffectSystem||
           mImpl->app->mEffectSystem->mParticleHolder!=mImpl->holder||!mImpl->holder)throw std::runtime_error("native source effect board/holder unavailable");
        const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(board));const auto key=std::make_pair(px,py);
        if(mImpl->definitions.contains(key)){error.clear();return true;}
        const auto folder=ResourceRoot()/"source-52";std::ifstream input(folder/"effect.json");
        if(!input)throw std::runtime_error("missing source52 effect.json");const Json data=Json::parse(input);
        if(data.at("schema")!=1||data.at("sourceKind")!=52||data.at("emitters").size()!=17||data.at("birthPoints").size()!=8||
           !data.at("ignoredBindings").empty()||!data.at("deferredComponents").empty())throw std::runtime_error("invalid source52 supported-subset definition");
        auto definition=std::make_unique<Definition>();definition->emitters.resize(17);definition->names.resize(17);
        definition->native.mEmitterDefs=definition->emitters.data();definition->native.mEmitterDefCount=17;
        const float nativePixels=std::sqrt(px*py);size_t i=0;
        for(const auto& source:data.at("emitters"))
        {
            auto& out=definition->emitters[i];definition->names[i]=source.at("nodeName").get<std::string>()+"#"+std::to_string(i);out.mName=definition->names[i].c_str();
            out.mEmitterType=EmitterType::EMITTER_CIRCLE;out.mImageFrames=1;out.mOnDuration="";
            const float delay=Number(source.at("delay"),0,10),duration=Number(source.at("duration"),.01f,10),life=Number(source.at("lifetime"),.01f,10);
            const int total=Ticks(delay+duration+life);const float last=float(total-1);
            definition->Constant(out.mSystemDuration,float(total));definition->Constant(out.mParticleDuration,float(Ticks(life)));
            const float rate=source.at("sourceActive").get<bool>()?Number(source.at("rate"),0,10000):0;
            // Sample the original emission interval on the sole native emitter clock.
            const float begin=delay*100/last,end=(delay+duration)*100/last,step=.000001f;
            if(delay>0)
                definition->Track(out.mSpawnRate,{{0,0,0},{begin-step,0,0},{begin,rate,rate},{end-step,rate,rate},{end,0,0},{1,0,0}});
            else
                definition->Track(out.mSpawnRate,{{0,rate,rate},{end-step,rate,rate},{end,0,0},{1,0,0}});
            const int maxActive=source.at("maxActive").get<int>();if(maxActive<1||maxActive>1000)throw std::runtime_error("unsupported source particle capacity");
            definition->Constant(out.mSpawnMinActive,-1);definition->Constant(out.mSpawnMaxActive,float(maxActive));
            const auto& position=source.at("sourcePosition");definition->Constant(out.mEmitterOffsetX,Number(position.at("x"),-40,40)*px);definition->Constant(out.mEmitterOffsetY,-Number(position.at("y"),-40,40)*py);
            const auto& q=source.at("sourceRotation");const float qx=Number(q.at("x"),-1,1),qy=Number(q.at("y"),-1,1),qz=Number(q.at("z"),-1,1),qw=Number(q.at("w"),-1,1);
            const float localScale=Number(source.at("sourceScale").at("x"),.00001f,20);
            const auto& shape=source.at("shape");const int shapeType=shape.at("type").get<int>();
            if(shapeType!= -1&&shapeType!=0&&shapeType!=4)throw std::runtime_error("unsupported native source shape");
            const float shapeScale=Number(shape.at("scale").at("x"),0,20);
            const float radius=Number(shape.at("radius"),0,20)*shapeScale*localScale*nativePixels;
            definition->Track(out.mEmitterRadius,{{0,0,radius}});
            if(shapeType==4)
            {
                const float vx=2*(qx*qz+qw*qy)*px,vy=-2*(qy*qz-qw*qx)*py;
                const float angle=float(std::atan2(vx,vy)*180/pi),cone=Number(shape.at("angle"),0,90);
                definition->Track(out.mLaunchAngle,{{0,angle-cone,angle+cone}});
            }
            definition->Constant(out.mLaunchSpeed,Number(source.at("speed"),0,100)*localScale*nativePixels);
            for(auto* track:{&out.mSystemRed,&out.mSystemGreen,&out.mSystemBlue,&out.mSystemAlpha,&out.mSystemBrightness,&out.mParticleBrightness,&out.mParticleStretch})definition->Constant(*track,1);
            const auto& color=source.at("color");definition->Constant(out.mParticleRed,Number(color.at("r"),0,1));definition->Constant(out.mParticleGreen,Number(color.at("g"),0,1));definition->Constant(out.mParticleBlue,Number(color.at("b"),0,1));definition->Constant(out.mParticleAlpha,Number(color.at("a"),0,1));
            if(source.at("rendererEnabled").get<bool>())
            {
                const auto& sprite=source.at("sprite");const auto relative=std::filesystem::path(sprite.at("texture").get<std::string>());
                if(relative.is_absolute()||relative.lexically_normal().string().find("..")!=std::string::npos)throw std::runtime_error("invalid source particle texture path");
                const auto path=folder/relative;const auto expected=sprite.at("textureSha256").get<std::string>();if(Sha256(path)!=expected)throw std::runtime_error("source particle texture hash differs");
                auto stem=path;stem.replace_extension();const auto variant=Sexy::StringToSexyString("FUSION_SOURCE_PARTICLE_SHA256:"+expected);
                auto image=mImpl->app->GetSharedImage(Sexy::StringToSexyString(stem.string()),variant);out.mImage=static_cast<Sexy::Image*>(image);
                if(!out.mImage||out.mImage->mWidth<=0||out.mImage->mHeight<=0||out.mImage->mWidth!=sprite.at("width").get<int>()||out.mImage->mHeight!=sprite.at("height").get<int>()||Sha256(path)!=expected)throw std::runtime_error("source particle texture decode differs");
                definition->images.push_back(image);
                const float size=Number(source.at("size"),0,20)*localScale*nativePixels/out.mImage->mWidth;
                const auto& curve=source.at("sizeCurve");if(curve.size()!=2||curve[0][0]!=0||curve[1][0]!=1)throw std::runtime_error("unsupported native source size curve");
                definition->Track(out.mParticleScale,{{0,Number(curve[0][1],0,20)*size,Number(curve[0][1],0,20)*size},{1,Number(curve[1][1],0,20)*size,Number(curve[1][1],0,20)*size}});
                definition->Constant(out.mParticleSpinAngle,float(Number(source.at("rotation"),-10,10)*180/pi));
            }
            ++i;
        }
        for(size_t n=0;n<8;++n)
        {const auto& p=data.at("birthPoints")[n];definition->births[n]={Number(p.at("x"),-40,40),Number(p.at("y"),-40,40)};}
        mImpl->definitions.emplace(key,std::move(definition));error.clear();return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
TodParticleSystem* NativeSourceEffects269::Create(Board* board,uint16_t kind,float x,float y,int order,bool emission)
{
    if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>4096||std::abs(y)>4096)return nullptr;
    if((kind>=2000&&kind<3000)||(kind>=5000&&kind<6000))
    {
        // Binding preflight loaded this exact hashed resource already. AUX
        // resource paths belong to their manifest, not the plant-only folder.
        if(!board||board->mApp!=mImpl->app||!IsNativeFusionBoard(board)||!mImpl->holder)return nullptr;
        const auto found=mImpl->worldDefinitions.find({kind,float(kFusionBasePixelsX),float(GetNativeFusionPixelsY(board))});
        return found==mImpl->worldDefinitions.end()?nullptr:mImpl->holder->AllocParticleSystemFromDef(x,y,order,found->second->Data(emission),PARTICLE_FUSION_EMBEDDED269);
    }
    std::string error;if(!Ensure(board,kind,error)){LawnNet::NetTrace("Fusion source effect rejected kind=%u error=%s",kind,error.c_str());return nullptr;}
    if(kind!=52)
    {const auto& resource=mImpl->worldDefinitions.at({kind,float(kFusionBasePixelsX),float(GetNativeFusionPixelsY(board))});
     const auto effect=kind==35?PARTICLE_FUSION_SOURCE35:kind==36?PARTICLE_FUSION_SOURCE36:kind==49?PARTICLE_FUSION_SOURCE49:kind==51?PARTICLE_FUSION_SOURCE51:kind==99?PARTICLE_FUSION_SOURCE99:kind==100?PARTICLE_FUSION_SOURCE100:PARTICLE_FUSION_EMBEDDED269;
     return mImpl->holder->AllocParticleSystemFromDef(x,y,order,resource->Data(emission),effect);}
    auto& definition=mImpl->definitions.at({float(kFusionBasePixelsX),float(GetNativeFusionPixelsY(board))});
    return mImpl->holder->AllocParticleSystemFromDef(x,y,order,&definition->native,ParticleEffect::PARTICLE_FUSION_SOURCE52);
}
NativeWorldParticle269Definition* NativeSourceEffects269::WorldDefinition(Board* board,uint16_t kind,const std::string& path,std::string& error)
{
    if(!board||!IsNativeFusionBoard(board)||board->mApp!=mImpl->app||!mImpl->app->mEffectSystem||
       mImpl->app->mEffectSystem->mParticleHolder!=mImpl->holder||!mImpl->holder){error="native source holder unavailable";return nullptr;}
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(board));const auto key=std::make_tuple(kind,px,py);
    const auto found=mImpl->worldDefinitions.find(key);if(found!=mImpl->worldDefinitions.end()){error.clear();return found->second.get();}
    auto definition=std::make_unique<NativeWorldParticle269Definition>();
    if(!definition->Load(mImpl->app,kind,path,px,py,error))return nullptr;
    auto* result=definition.get();mImpl->worldDefinitions.emplace(key,std::move(definition));return result;
}
bool NativeSourceEffects269::EnsureEmbedded(Board* board,uint16_t kind,const std::string& model,std::string& error)
{
    if(!mImpl->embedded)mImpl->embedded=std::make_unique<NativeEmbeddedEffects269>(mImpl->app);
    return mImpl->embedded->Ensure(board,kind,model,(ResourceRoot()/"embedded269"/"manifest.json").string(),error);
}
void NativeSourceEffects269::UpdateEmbedded(Board* board)
{if(mImpl->embedded)mImpl->embedded->Update(board);}
bool NativeSourceEffects269::EnsureEmbeddedProjectile(Board* board,uint16_t kind,const std::string& model,std::string& error)
{
    if(!mImpl->embedded)mImpl->embedded=std::make_unique<NativeEmbeddedEffects269>(mImpl->app);
    return mImpl->embedded->EnsureProjectile(board,kind,model,(ResourceRoot()/"embedded269"/"projectiles.json").string(),error);
}
bool NativeSourceEffects269::EnsureEmbeddedZombie(Board* board,uint16_t kind,const std::string& model,std::string& error)
{
    if(!mImpl->embedded)mImpl->embedded=std::make_unique<NativeEmbeddedEffects269>(mImpl->app);
    return mImpl->embedded->EnsureZombie(board,kind,model,(ResourceRoot()/"zombies270"/"manifest.json").string(),error);
}
NativeWorldParticle269Definition* EnsureNativeEmbeddedParticle269Resource(Board* board,uint16_t kind,const std::string& path,std::string& error)
{auto* cache=GetNativeSourceEffects269(board,true);if(!cache){error="native source cache unavailable";return nullptr;}return cache->WorldDefinition(board,kind,path,error);}
TodParticleSystem* CreateNativeEmbeddedParticle269System(Board* board,uint16_t kind,NativeWorldParticle269Definition* definition,float x,float y,int order,bool emission)
{auto* cache=GetNativeSourceEffects269(board,false);return cache&&definition?cache->Create(board,kind,x,y,order,emission):nullptr;}
bool EnsureNativeEmbedded269Model(Board* board,uint16_t kind,const std::string& model,std::string* error)
{std::string message;auto* cache=GetNativeSourceEffects269(board,true);const bool result=cache&&cache->EnsureEmbedded(board,kind,model,message);if(error)*error=message;return result;}
bool EnsureNativeEmbedded269ProjectileModel(Board* board,uint16_t kind,const std::string& model,std::string* error)
{std::string message;auto* cache=GetNativeSourceEffects269(board,true);const bool result=cache&&cache->EnsureEmbeddedProjectile(board,kind,model,message);if(error)*error=message;return result;}
bool EnsureNativeEmbedded269ZombieModel(Board* board,uint16_t kind,const std::string& model,std::string* error)
{std::string message;auto* cache=GetNativeSourceEffects269(board,true);const bool result=cache&&cache->EnsureEmbeddedZombie(board,kind,model,message);if(error)*error=message;return result;}
bool NativeSourceEffects269::BirthPoints(Board* board,uint16_t kind,float x,float y,std::array<NativeDoom269Point,8>& output) const
{
    if(kind!=52||!board||board->mApp!=mImpl->app)return false;
    const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(board));const auto found=mImpl->definitions.find({px,py});if(found==mImpl->definitions.end())return false;
    auto points=found->second->births;for(auto& p:points){p.x=x+p.x*px;p.y=y-p.y*py;}output=points;return true;
}
bool EnsureNativeSourceEffect269(Board* board,uint16_t kind,std::string* error)
{
    auto* assets=GetNativeSourceEffects269(board,true);std::string message;
    const bool result=assets&&assets->Ensure(board,kind,message);if(!assets)message="source particle resource cache unavailable";if(error)*error=message;return result;
}
TodParticleSystem* SpawnNativeSourceEffect269(Board* board,uint16_t kind,float x,float y,int order,bool broadcast)
{
    const int effect=kind==35?PARTICLE_FUSION_SOURCE35:kind==36?PARTICLE_FUSION_SOURCE36:kind==49?PARTICLE_FUSION_SOURCE49:kind==51?PARTICLE_FUSION_SOURCE51:kind==52?PARTICLE_FUSION_SOURCE52:kind==99?PARTICLE_FUSION_SOURCE99:kind==100?PARTICLE_FUSION_SOURCE100:-1;
    if(effect<0)return nullptr; // No arbitrary kind may masquerade as source100.
    auto* assets=GetNativeSourceEffects269(board,true);if(!assets)return nullptr;
    auto* system=assets->Create(board,kind,x,y,order);
    if(system&&broadcast)LawnNet::NetBroadcastParticle(board->mApp,x,y,order,effect);
    return system;
}
bool CreateNativeIceDoom269Cloud(Plant* plant,float x,float y,std::array<NativeDoom269Point,8>& output)
{
    if(!plant||plant->mDead||plant->mNetGhost||!plant->mBoard||plant->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(plant->mBoard))return false;
    const int order=Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE,plant->mRow,0);
    auto* system=SpawnNativeSourceEffect269(plant->mBoard,52,x,y,order,false);if(!system)return false;
    auto* assets=GetNativeSourceEffects269(plant->mBoard,false);
    std::array<NativeDoom269Point,8> points;
    if(!assets||!assets->BirthPoints(plant->mBoard,52,x,y,points)){system->ParticleSystemDie();return false;}
    output=points;
    LawnNet::NetBroadcastParticle(plant->mApp,x,y,order,int(ParticleEffect::PARTICLE_FUSION_SOURCE52));
    LawnNet::NetTrace("Fusion source52 birth system=%u emitters=17 points=8 x=%.9g y=%.9g tick=%d",unsigned(plant->mApp->ParticleGetID(system)),x,y,plant->mBoard->mMainCounter);
    return true;
}
}
