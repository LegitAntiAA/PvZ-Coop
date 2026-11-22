#include "FusionNativeEffects269.h"
#include "FusionNativeWorldParticle269.h"
#include "FusionAnimation.h"
#include "FusionSourceSpriteScript366.h"
#include "FusionNativeEmbeddedEffects269.h"
#include <tuple>
#include "FusionNative.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeDayZombie273.h"
#include "FusionNativeSourceClock313.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
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
    std::unique_ptr<FusionAnimation> overlay75;
    std::array<float,4> overlayColor75{};
    std::string overlayNode75;
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

bool NativeSourceEffects269::EnsureSpriteOverlay75(Board* board,std::string& error)
{
 try
 {
  if(!board||board->mApp!=mImpl->app||!IsNativeFusionBoard(board)||!mImpl->holder||!mImpl->app->mEffectSystem||mImpl->app->mEffectSystem->mParticleHolder!=mImpl->holder)
   throw std::runtime_error("source75 native Board/particle holder unavailable");
  if(mImpl->overlay75)return true;
  const auto root=ResourceRoot()/"source-75";std::ifstream effect(root/"effect.json");if(!effect)throw std::runtime_error("source75 effect missing");
  const Json definition=Json::parse(effect);const auto& overlay=definition.at("spriteOverlay366");
  if(definition.at("sourceKind")!=75||definition.at("emitters").size()!=8||overlay.at("model")!="overlay/animation.json"||overlay.at("sourceRendererFileID")!=212371939378208658LL||overlay.at("fixedStepSeconds")!=.02||overlay.at("alphaDecrementPerStep")!=.04)
   throw std::runtime_error("source75 original mixed Sprite/particle controller differs");
  const auto path=root/"overlay"/"animation.json";const auto hash=overlay.at("modelSha256").get<std::string>();if(Sha256(path)!=hash)throw std::runtime_error("source75 overlay model hash differs");
  std::ifstream modelFile(path);const Json model=Json::parse(modelFile);
  if(model.at("sourcePrefab")!="Resources/particle/prefabs/BossExplosion.prefab"||!model.at("static").get<bool>()||model.at("sprites").size()!=1||model.at("nodes").size()!=9||!model.at("clips").empty()||!model.at("ignoredBindings").empty()||!model.at("omittedInactiveNodes").empty())
   throw std::runtime_error("source75 complete static source hierarchy differs");
  const auto& proof=model.at("sourceBuiltinSpriteProof366");if(proof.at("builtinFileID")!=10905||proof.at("name")!="UISprite"||proof.at("spritePathID")!=7170)
   throw std::runtime_error("source75 genuine builtin Sprite identity missing");
  std::vector<int64_t> particles;bool controller=false;
  for(const auto& c:model.at("nonRenderingComponents"))
  {
   const int kind=c.at("typeId").get<int>();if(kind==198||kind==199)particles.push_back(c.at("fileId").get<int64_t>());
   if(kind==114)controller=c.at("serializedComponent").get<std::string>().find("guid: 4048e352ff94155e9450a15465186b68")!=std::string::npos;
  }
  if(particles.size()!=16||!controller)throw std::runtime_error("source75 full eight particle pairs/IceExplodeControl missing");
  const auto& sprite=model.at("sprites").front();const auto node=model.at("nodes").at(sprite.at("node").get<size_t>()).at("path").get<std::string>();
  if(node!="sprit1"||sprite.at("sortingOrder")!=0||sprite.at("sortingLayerId")!=-1823021693||sprite.at("maskInteraction")!=0)
   throw std::runtime_error("source75 original overlay layer/transform differs");
  const auto& color=sprite.at("color");const std::array<float,4> rgba{color.at("r").get<float>(),color.at("g").get<float>(),color.at("b").get<float>(),color.at("a").get<float>()};
  std::array<float,4> checked;if(!SourceIceExplosionColor366(0,rgba,checked))throw std::runtime_error("source75 initial Sprite color invalid");
  auto pending=std::make_unique<FusionAnimation>();if(!pending->Load(mImpl->app,path.string(),error,particles))return false;
  if(Sha256(path)!=hash)throw std::runtime_error("source75 overlay changed during load");
  mImpl->overlayColor75=rgba;mImpl->overlayNode75=node;mImpl->overlay75=std::move(pending);error.clear();return true;
 }
 catch(const std::exception& exception){error=exception.what();return false;}
}
void NativeSourceEffects269::DrawSpriteOverlay366(Board* board,TodParticleSystem* system,Sexy::Graphics* graphics) const
{
 if(!graphics||!system||system->mDead||!board||board->mApp!=mImpl->app||mImpl->app->mBoard!=board||!mImpl->overlay75||
  !mImpl->holder||system->mParticleHolder!=mImpl->holder||!mImpl->app->mEffectSystem||mImpl->app->mEffectSystem->mParticleHolder!=mImpl->holder)return;
 const auto found=mImpl->worldDefinitions.find({75,float(kFusionBasePixelsX),float(GetNativeFusionPixelsY(board))});
 if(found==mImpl->worldDefinitions.end()||(system->mParticleDef!=found->second->Data(true)&&system->mParticleDef!=found->second->Data(false)))return;
 const auto uid=mImpl->holder->mParticleSystems.DataArrayGetID(system);
 if(!uid||mImpl->holder->mParticleSystems.DataArrayTryToGet(uid)!=system||!system->mEmitterList.mHead)return;
 const auto* emitter=mImpl->holder->mEmitters.DataArrayTryToGet(unsigned(system->mEmitterList.mHead->mValue));
 if(!emitter||emitter->mParticleSystem!=system)return;
 std::array<float,4> rgba;if(!SourceIceExplosionColor366(emitter->mSystemAge,mImpl->overlayColor75,rgba)||rgba[3]<=0)return;
 FusionAnimationDrawOptions options;options.useBoardBasis=true;options.boardBasis={float(kFusionBasePixelsX),0,0,-float(GetNativeFusionPixelsY(board))};
 options.nodeLocalRotations.push_back({"",{0,0,0,1}}); // CreateParticle.SetParticle overwrites prefab -90X.
 options.spriteColorOverrides.push_back({mImpl->overlayNode75,rgba});
 mImpl->overlay75->DrawPose(graphics,emitter->mSystemCenter.x,emitter->mSystemCenter.y,1,nullptr,0,nullptr,-1,1,&options);
}
void DrawNativeSourceEffectOverlay366(TodParticleSystem* system,Sexy::Graphics* graphics)
{
 if(!gLawnApp||!gLawnApp->mBoard||!IsNativeFusionBoard(gLawnApp->mBoard))return;
 if(auto* cache=GetNativeSourceEffects269(gLawnApp->mBoard,false))cache->DrawSpriteOverlay366(gLawnApp->mBoard,system,graphics);
}

bool NativeSourceEffects269::Ensure(Board* board,uint16_t kind,std::string& error)
{
    if(kind==75&&!EnsureSpriteOverlay75(board,error))return false;
    if(kind==23)
        return WorldDefinition(board,5066,(ResourceRoot()/"zombies270"/"5066"/"effect.json").string(),error)!=nullptr;
    if(kind==75||kind==76||kind==77||kind==24||kind==97||kind==0||kind==10||kind==11||kind==16||kind==18||kind==32||kind==34||kind==35||kind==36||kind==41||kind==42||kind==49||kind==51||kind==54||kind==62||kind==63||kind==64||kind==99||kind==100||(kind>=2000&&kind<2100))
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
    const bool field=(kind==6000||kind==6001)&&mImpl->embedded&&mImpl->embedded->FieldResourceAdmitted277(kind);
    if((kind==6000||kind==6001)&&!field)return nullptr; // Only the exact field manifest can admit this family.
    if((kind>=2000&&kind<3000)||(kind>=5000&&kind<6000)||field)
    {
        // Binding preflight loaded this exact hashed resource already. AUX
        // resource paths belong to their manifest, not the plant-only folder.
        if(!board||board->mApp!=mImpl->app||!IsNativeFusionBoard(board)||!mImpl->holder)return nullptr;
        const auto found=mImpl->worldDefinitions.find({kind,float(kFusionBasePixelsX),float(GetNativeFusionPixelsY(board))});
        return found==mImpl->worldDefinitions.end()?nullptr:mImpl->holder->AllocParticleSystemFromDef(x,y,order,found->second->Data(emission),PARTICLE_FUSION_EMBEDDED269);
    }
    std::string error;if(!Ensure(board,kind,error)){LawnNet::NetTrace("Fusion source effect rejected kind=%u error=%s",kind,error.c_str());return nullptr;}
    if(kind!=52)
    {const auto& resource=mImpl->worldDefinitions.at({kind==23?uint16_t(5066):kind,float(kFusionBasePixelsX),float(GetNativeFusionPixelsY(board))});
     const int mapped=NativeSourceEffectParticle269(kind);
     const auto effect=mapped<0?PARTICLE_FUSION_EMBEDDED269:ParticleEffect(mapped);
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
bool NativeSourceEffects269::PlayZombieLimbEvent(Board* board,const LawnNet::NetFusionLimbEvent276& event,bool& newEvent)
{
    if(!mImpl->embedded)mImpl->embedded=std::make_unique<NativeEmbeddedEffects269>(mImpl->app);
    return mImpl->embedded->PlayZombieLimbEvent(board,event,newEvent);
}
bool PlayNativeSourceZombieLimbEvent276(Board* board,const LawnNet::NetFusionLimbEvent276& event)
{
    const auto* context=GetNativeContext(board);
    if(!board||!board->mApp||board->mApp->mBoard!=board||!context||context->epoch!=event.mEpoch||
        !IsNativeSourceWorld313(board)||event.mRow>=GetNativeFusionRowCount(board)||
        !LawnNet::NetFusionLimbEvent276Valid(event))return false;
    auto* cache=GetNativeSourceEffects269(board,true);bool newEvent=false;
    return cache&&cache->PlayZombieLimbEvent(board,event,newEvent);
}
namespace
{
bool CaptureSourcePart284(Zombie* zombie,LawnNet::NetFusionZombiePart284 part,LawnNet::NetFusionLimbEvent276& event)
{
    if(!zombie||zombie->mNetGhost||!zombie->mApp||zombie->mApp->NetIsClient()||
        !IsNativeSourceWorld313(zombie->mBoard)||zombie->mRow<0||
        zombie->mRow>=GetNativeFusionRowCount(zombie->mBoard))return false;
    auto* board=zombie->mBoard;const auto* context=GetNativeContext(board);
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!context||!data||!LawnNet::NetFusionZombiePart284Supported(data->kind,part)||
        !EnsureNativeSourceZombieVisuals(board,data->kind))return false;
    event.mEpoch=context->epoch;event.mZombieId=zombie->mNetId;event.mZombieToken=data->instanceToken;
    event.mSourceKind=data->kind;event.mPart=part;event.mRow=uint8_t(zombie->mRow);event.mRenderOrder=zombie->mRenderOrder;
    if(!GetNativeSourceZombieLimbBirthPoint276(zombie,NativeSourceZombiePartNode313(data->kind,uint8_t(part)),event.mX,event.mY))
    {
        LawnNet::NetTrace("Fusion Day limb276 producer rejected epoch=%u kind=%u id=%u token=%u part=%u reason=body-pose",
            event.mEpoch,event.mSourceKind,event.mZombieId,event.mZombieToken,uint8_t(part));return false;
    }
    return LawnNet::NetFusionLimbEvent276Valid(event);
}
bool CommitSourcePart284(Zombie* zombie,const LawnNet::NetFusionLimbEvent276& event)
{
    auto* board=zombie->mBoard;
    auto* cache=GetNativeSourceEffects269(board,true);bool newEvent=false;
    const bool accepted=cache&&cache->PlayZombieLimbEvent(board,event,newEvent);
    const char* names[]={"arm","head","first-armor","second-armor","glasses"};
    if(accepted&&newEvent)
        LawnNet::NetTrace("Fusion Day limb276 produced epoch=%u kind=%u id=%u token=%u part=%s row=%u birth=(%.3f,%.3f) ownerOrder=%d broadcastRequested=%d",
            event.mEpoch,event.mSourceKind,event.mZombieId,event.mZombieToken,names[uint8_t(event.mPart)],event.mRow,
            event.mX,event.mY,event.mRenderOrder,zombie->mApp->NetIsHost()?1:0);
    if(accepted&&newEvent&&zombie->mApp->NetIsHost())LawnNet::NetBroadcastFusionLimb276(zombie->mApp,event);
    return accepted;
}
}
bool PlayNativeSourceZombieLimb276(Zombie* zombie,bool head)
{
    using Part=LawnNet::NetFusionZombiePart284;
    const auto* data=GetNativeSourceZombieData(zombie);
    if(data&&(data->kind==202||data->kind==204||data->kind==218||data->kind==220)&&
       (head||data->kind==218))return true;
    std::array<LawnNet::NetFusionLimbEvent276,2> events;
    const size_t count=head&&data&&data->kind==5?2:1;
    if(!CaptureSourcePart284(zombie,head?Part::Head:Part::Arm,events[0])||
        (count==2&&!CaptureSourcePart284(zombie,Part::Glasses,events[1])))return false;
    // Capture both original transforms before any head mutation/owner removal.
    // Each source PS has its own reliable identity and exact birth position.
    bool accepted=true;
    for(size_t i=0;i<count;++i)accepted=CommitSourcePart284(zombie,events[i])&&accepted;
    return accepted;
}
bool PlayNativeSourceZombieArmor284(Zombie* zombie,bool firstArmor)
{
    LawnNet::NetFusionLimbEvent276 event;
    return CaptureSourcePart284(zombie,firstArmor?LawnNet::NetFusionZombiePart284::FirstArmor:
        LawnNet::NetFusionZombiePart284::SecondArmor,event)&&CommitSourcePart284(zombie,event);
}
bool EmitNativeSourceZombiePart313(Zombie* zombie,uint8_t sourcePart)
{
    if(sourcePart>uint8_t(LawnNet::NetFusionZombiePart284::Glasses))return false;
    LawnNet::NetFusionLimbEvent276 event;
    return CaptureSourcePart284(zombie,LawnNet::NetFusionZombiePart284(sourcePart),event)&&CommitSourcePart284(zombie,event);
}
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
bool NativeSourceEffects269::EnsureField277(Board* board,uint16_t scene,const std::string& model,std::vector<int64_t>& proof,std::string& error)
{
    if(!mImpl->embedded)mImpl->embedded=std::make_unique<NativeEmbeddedEffects269>(mImpl->app);
    return mImpl->embedded->EnsureField277(board,scene,model,(ResourceRoot()/"fields277"/"manifest.json").string(),proof,error);
}
void NativeSourceEffects269::ReconcileField277(Board* board,uint16_t scene,bool enabled,float x,float y,int order)
{if(mImpl->embedded)mImpl->embedded->ReconcileField277(board,scene,enabled,x,y,order);}
bool NativeSourceEffects269::QueryField277(const Board* board,uint16_t& resource,uint32_t& system,float& x,float& y) const
{return mImpl->embedded&&mImpl->embedded->QueryField277(board,resource,system,x,y);}
bool EnsureNativeField277Resources(Board* board,uint16_t scene,const std::string& model,std::vector<int64_t>& proof,std::string& error)
{
    auto* cache=GetNativeSourceEffects269(board,true);
    return cache&&cache->EnsureField277(board,scene,model,proof,error);
}
void ReconcileNativeField277Particle(Board* board,uint16_t scene,bool enabled,float x,float y,int order)
{if(auto* cache=GetNativeSourceEffects269(board,false))cache->ReconcileField277(board,scene,enabled,x,y,order);}
bool QueryNativeField277Particle(const Board* board,uint16_t& resource,uint32_t& system,float& x,float& y)
{
    resource=0;system=0;x=y=0;
    auto* cache=GetNativeSourceEffects269(const_cast<Board*>(board),false);
    return cache&&cache->QueryField277(board,resource,system,x,y);
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
int NativeSourceEffectParticle269(uint16_t kind)
{
    switch(kind)
    {
    case 0:return PARTICLE_FUSION_SOURCE0;
    case 10:return PARTICLE_FUSION_SOURCE10;
    case 11:return PARTICLE_FUSION_SOURCE11;
    case 16:return PARTICLE_FUSION_SOURCE16;
    case 18:return PARTICLE_FUSION_SOURCE18;
    case 23:return PARTICLE_FUSION_SOURCE23;
    case 75:return PARTICLE_FUSION_SOURCE75;
    case 76:return PARTICLE_FUSION_SOURCE76;
    case 77:return PARTICLE_FUSION_SOURCE77;
    case 24:return PARTICLE_FUSION_SOURCE24;
    case 97:return PARTICLE_FUSION_SOURCE97;
    case 32:return PARTICLE_FUSION_SOURCE32;
    case 34:return PARTICLE_FUSION_SOURCE34;
    case 35:return PARTICLE_FUSION_SOURCE35;
    case 36:return PARTICLE_FUSION_SOURCE36;
    case 41:return PARTICLE_FUSION_SOURCE41;
    case 42:return PARTICLE_FUSION_SOURCE42;
    case 49:return PARTICLE_FUSION_SOURCE49;
    case 51:return PARTICLE_FUSION_SOURCE51;
    case 52:return PARTICLE_FUSION_SOURCE52;
    case 54:return PARTICLE_FUSION_SOURCE54;
    case 62:return PARTICLE_FUSION_SOURCE62;
    case 63:return PARTICLE_FUSION_SOURCE63;
    case 64:return PARTICLE_FUSION_SOURCE64;
    case 99:return PARTICLE_FUSION_SOURCE99;
    case 100:return PARTICLE_FUSION_SOURCE100;
    default:return -1;
    }
}
bool NativeSourceEffectKind269(int effect,uint16_t& kind)
{
    for(uint16_t candidate:{0,10,11,16,18,23,24,75,76,77,97,32,34,35,36,41,42,49,51,52,54,62,63,64,99,100})
        if(NativeSourceEffectParticle269(candidate)==effect){kind=candidate;return true;}
    return false;
}
TodParticleSystem* SpawnNativeSourceEffect269(Board* board,uint16_t kind,float x,float y,int order,bool broadcast)
{
    const int effect=NativeSourceEffectParticle269(kind);
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

namespace LawnFusion {
void NativeSourceEffects269::CapturePlant314(Board* board,uint32_t oldUID,uint32_t ownerUID,uint32_t token,uint16_t kind)
{if(mImpl->embedded)mImpl->embedded->CapturePlant314(board,oldUID,ownerUID,token,kind);}
void NativeSourceEffects269::ReleaseCapturedPlant314(uint32_t ownerUID,uint32_t token)
{if(mImpl->embedded)mImpl->embedded->ReleaseCapturedPlant314(ownerUID,token);}
}

namespace LawnFusion {
bool NativeSourceEffects269::SetZombieEmission(Zombie* z,const char* path,bool enabled)
{return mImpl&&mImpl->embedded&&mImpl->embedded->SetEmission(z,path,enabled);}
bool SetNativeSourceZombieEmission360(Zombie* z,const char* path,bool enabled)
{auto* cache=GetNativeSourceEffects269(z?z->mBoard:nullptr,false);return cache&&cache->SetZombieEmission(z,path,enabled);}
}
