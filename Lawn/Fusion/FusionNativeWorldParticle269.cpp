#include "FusionNativeWorldParticle269.h"
#include "FusionWorldParticle269Rules.h"
#include "FusionParticleCollision269Rules.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/SharedImage.h"
#include "../../SexyAppFramework/Image.h"
#include "../../third_party/nlohmann/json.hpp"
#include <Windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace LawnFusion
{
namespace
{
using Json=nlohmann::json;
struct Collision
{
    bool enabled=false;
    FusionParticleCollision269Profile profile;
    float pixelsX=0,pixelsY=0,radiusPerNativeScale=0;
};
bool UpdateCollision(TodParticleEmitter* emitter,TodParticle* particle)
{
    const auto& c=*static_cast<const Collision*>(emitter->mFusionCollisionData);
    const float scale=TodParticleEmitter::ParticleTrackEvaluate(emitter->mEmitterDef->mParticleScale,particle,ParticleTracks::TRACK_PARTICLE_SCALE);
    FusionParticleCollision269State state{particle->mPosition.y,particle->mVelocity.x,particle->mVelocity.y,particle->mParticleAge,particle->mParticleDuration};
    bool hit=false,killed=false;
    if(!StepFusionParticleCollision269(c.profile,c.pixelsX,c.pixelsY,emitter->mFusionCollisionGroundY,
        std::abs(scale*emitter->mScaleOverride)*c.radiusPerNativeScale,
        emitter->mFusionCollisionZombieHead&&emitter->mFusionCollisionLatched,state,hit,killed))return false;
    particle->mPosition.y=state.y;particle->mVelocity.x=state.velocityX;particle->mVelocity.y=state.velocityY;
    particle->mParticleAge=state.age;
    if(hit)particle->mParticleTimeValue=float(state.age)/float((std::max)(1,state.duration-1));
    if(hit&&emitter->mFusionCollisionZombieHead)emitter->mFusionCollisionLatched=true;
    return !killed;
}
float Number(const Json& x,float low,float high)
{const float v=x.get<float>();if(!std::isfinite(v)||v<low||v>high)throw std::runtime_error("invalid source world particle scalar");return v;}
int Ticks(float seconds){return int(std::ceil(seconds*100))+1;}
std::string Hash(const std::filesystem::path& path)
{
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing source particle image");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});HCRYPTPROV provider=0;HCRYPTHASH hash=0;
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))throw std::runtime_error("SHA provider unavailable");
    bool okay=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=0;
    for(std::size_t i=0;okay&&i<bytes.size();i+=65536)okay=CryptHashData(hash,bytes.data()+i,DWORD((std::min)(size_t(65536),bytes.size()-i)),0)!=0;
    BYTE digest[32]{};DWORD count=32;if(okay)okay=CryptGetHashParam(hash,HP_HASHVAL,digest,&count,0)!=0&&count==32;
    if(hash)CryptDestroyHash(hash);CryptReleaseContext(provider,0);if(!okay)throw std::runtime_error("SHA verification unavailable");
    std::string out;for(BYTE v:digest){out+="0123456789abcdef"[v>>4];out+="0123456789abcdef"[v&15];}return out;
}
std::vector<FusionWorldParticle269Key> Curve(const Json& data)
{
    if(data.size()<2||data.size()>128)throw std::runtime_error("invalid particle curve count");
    std::vector<FusionWorldParticle269Key> keys;for(const auto& k:data){if(k.size()!=4)throw std::runtime_error("invalid particle curve key");keys.push_back({Number(k[0],0,1),Number(k[1],-100000,100000),Number(k[2],-10000,10000),Number(k[3],-10000,10000)});}
    float v=0;if(!SampleFusionWorldParticle269Curve(keys,0,v))throw std::runtime_error("invalid particle Hermite curve");return keys;
}
FusionWorldParticle269Rectangle Rectangle(const Json& source,float px,float py)
{
    const auto& shape=source.at("shape");const auto& descriptor=shape.at("projectedRectangle");
    if(source.at("sourceScalingMode")!=1||descriptor.at("scalingMode")!=1||
       !descriptor.at("staticAncestorRotations").get<bool>()||!descriptor.at("staticParticleScale").get<bool>())
        throw std::runtime_error("rectangle requires static source Local scaling frame");
    for(const char* field:{"offset","shapeRotation"})
    {const auto& v=descriptor.at(field);if(!v.is_array()||v.size()!=3)throw std::runtime_error("invalid rectangle source frame");for(const auto& x:v)if(Number(x,-40,40)!=0)throw std::runtime_error("unsupported rectangle shape offset/rotation");}
    const auto& normal=descriptor.at("normal");
    if(!normal.is_array()||normal.size()!=3||normal[0]!=0||normal[1]!=0||normal[2]!=1)
        throw std::runtime_error("unsupported rectangle source normal");
    const auto& transforms=descriptor.at("ancestorTrs");
    if(!transforms.is_array()||transforms.empty()||transforms.size()>64)throw std::runtime_error("invalid rectangle ancestor proof");
    std::vector<FusionWorldParticle269Transform> chain;chain.reserve(transforms.size());
    for(const auto& t:transforms)
    {
        FusionWorldParticle269Transform v;const char* axes[]={"x","y","z"};
        for(int i=0;i<3;++i)
        {v.position[i]=Number(t.at("position").at(axes[i]),-10000,10000);v.scale[i]=Number(t.at("scale").at(axes[i]),.00001f,1000);v.rotation[i]=Number(t.at("rotation").at(axes[i]),-1.001f,1.001f);}
        v.rotation[3]=Number(t.at("rotation").at("w"),-1.001f,1.001f);chain.push_back(v);
    }
    bool rotatedParent=false;
    for(std::size_t i=0;i+1<chain.size();++i)
        if(std::abs(chain[i].rotation[0])+std::abs(chain[i].rotation[1])+std::abs(chain[i].rotation[2])>1e-6f)rotatedParent=true;
    // Existing local force/velocity compilers use sourceRotation (PS-local).
    // Do not admit a new rectangle whose inherited frame they cannot represent.
    if(rotatedParent)for(const char* field:{"forceCurves","velocityCurves"})
        if(source.contains(field)&&!source.at(field).is_null()&&!source.at(field).at("world").get<bool>())
            throw std::runtime_error("rotated rectangle ancestors require inherited local field projection");
    const char* axes[]={"x","y","z","w"};
    for(int i=0;i<4;++i)if(std::abs(chain.back().rotation[i]-Number(source.at("sourceRotation").at(axes[i]),-1.001f,1.001f))>1e-6f)
        throw std::runtime_error("rectangle PS rotation differs from source");
    for(int i=0;i<3;++i)if(std::abs(chain.back().scale[i]-Number(source.at("sourceScale").at(axes[i]),.00001f,1000))>1e-6f)
        throw std::runtime_error("rectangle PS local scale differs from source");
    std::array<float,9> basis;
    if(!ComposeFusionWorldParticle269LocalBasis(chain,basis))throw std::runtime_error("unsupported rectangle source TRS");
    const auto& recorded=descriptor.at("basis");
    if(!recorded.is_array()||recorded.size()!=3)throw std::runtime_error("invalid rectangle basis");
    for(int row=0;row<3;++row)
    {
        if(!recorded[row].is_array()||recorded[row].size()!=3)throw std::runtime_error("invalid rectangle basis row");
        for(int col=0;col<3;++col)if(std::abs(Number(recorded[row][col],-1000,1000)-basis[row*3+col])>1e-5f*(std::max)(1.f,std::abs(basis[row*3+col])))
            throw std::runtime_error("rectangle basis does not match source TRS");
    }
    const auto& half=descriptor.at("halfSize");
    if(!half.is_array()||half.size()!=2||Number(shape.at("scale").at("z"),0,40)!=0)throw std::runtime_error("unsupported rectangle dimensions");
    const std::array<float,2> size={Number(half[0],.00001f,20),Number(half[1],.00001f,20)};
    for(int i=0;i<2;++i)if(std::abs(size[i]-Number(shape.at("scale").at(axes[i]),.00001f,40)*.5f)>1e-6f)
        throw std::runtime_error("rectangle half-size differs from source shape");
    FusionWorldParticle269Rectangle projection;
    if(!ProjectFusionWorldParticle269Rectangle(basis,size,px,py,projection))throw std::runtime_error("source rectangle projection cannot use native BOX");
    return projection;
}
}
struct NativeWorldParticle269Definition::Impl
{
    TodParticleDefinition native{};
    TodParticleDefinition muted{};
    std::vector<TodEmitterDefinition> emitters;
    std::vector<TodEmitterDefinition> mutedEmitters;
    FloatParameterTrackNode zeroEmission{0,0,0,TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR};
    std::vector<std::string> names;
    std::vector<std::vector<FloatParameterTrackNode>> tracks;
    std::vector<std::vector<ParticleField>> fields;
    std::vector<Sexy::SharedImageRef> images;
    std::vector<Collision> collisions;
    void Track(FloatParameterTrack& target,std::span<const FusionWorldParticle269Sample> values)
    {
        tracks.emplace_back();auto& nodes=tracks.back();nodes.reserve(values.size());
        for(const auto& v:values)nodes.push_back({v.time,v.low,v.high,TodCurves::CURVE_LINEAR,TodCurves::CURVE_LINEAR});
        target.mNodes=nodes.data();target.mCountNodes=int(nodes.size());
    }
    void Track(FloatParameterTrack& target,std::initializer_list<FusionWorldParticle269Sample> values){Track(target,std::span<const FusionWorldParticle269Sample>(values.begin(),values.size()));}
    void Constant(FloatParameterTrack& target,float value){Track(target,{{0,value,value}});}
};
NativeWorldParticle269Definition::NativeWorldParticle269Definition()=default;
NativeWorldParticle269Definition::~NativeWorldParticle269Definition()=default;
TodParticleDefinition* NativeWorldParticle269Definition::Data(bool enabled) const {return mImpl?(enabled?&mImpl->native:&mImpl->muted):nullptr;}
bool NativeWorldParticle269Definition::SetEmission(TodParticleSystem* system,bool enabled) const
{
    if(!mImpl||!system||system->mDead||!system->mParticleHolder||(system->mParticleDef!=&mImpl->native&&system->mParticleDef!=&mImpl->muted))return false;
    auto* target=enabled?&mImpl->native:&mImpl->muted;std::vector<std::pair<TodParticleEmitter*,std::size_t>> pending;
    for(auto* node=system->mEmitterList.mHead;node;node=node->mNext)
    {
        auto* emitter=system->mParticleHolder->mEmitters.DataArrayTryToGet(uint32_t(node->mValue));
        if(!emitter)return false;std::size_t index=0;
        while(index<mImpl->emitters.size()&&emitter->mEmitterDef!=&mImpl->emitters[index]&&emitter->mEmitterDef!=&mImpl->mutedEmitters[index])++index;
        if(index==mImpl->emitters.size())return false;pending.emplace_back(emitter,index);
    }
    for(const auto& change:pending)change.first->mEmitterDef=&target->mEmitterDefs[change.second];system->mParticleDef=target;return true;
}
bool NativeWorldParticle269Definition::BindSourceCollision(TodParticleSystem* system,float ground,bool zombieHead,std::string* error) const
{
    auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!mImpl||!system||system->mDead||!system->mParticleHolder||!std::isfinite(ground)||std::abs(ground)>10000||
       (system->mParticleDef!=&mImpl->native&&system->mParticleDef!=&mImpl->muted))return fail("invalid source collision system/plane");
    std::vector<std::pair<TodParticleEmitter*,std::size_t>> pending;
    for(auto* node=system->mEmitterList.mHead;node;node=node->mNext)
    {
        auto* emitter=system->mParticleHolder->mEmitters.DataArrayTryToGet(uint32_t(node->mValue));
        if(!emitter)return fail("missing source collision emitter");std::size_t index=0;
        while(index<mImpl->emitters.size()&&emitter->mEmitterDef!=&mImpl->emitters[index]&&emitter->mEmitterDef!=&mImpl->mutedEmitters[index])++index;
        if(index==mImpl->emitters.size())return fail("foreign source collision emitter");pending.emplace_back(emitter,index);
    }
    for(const auto& [emitter,index]:pending)if(mImpl->collisions[index].enabled)
    {
        emitter->mFusionCollisionData=&mImpl->collisions[index];emitter->mFusionCollisionUpdate=UpdateCollision;
        emitter->mFusionCollisionGroundY=ground;emitter->mFusionCollisionZombieHead=zombieHead;
        // Rebinding a moving native plane never resets a callback already fired.
    }
    if(error)error->clear();return true;
}
bool NativeWorldParticle269Definition::Load(LawnApp* app,uint16_t kind,const std::string& path,float px,float py,std::string& error)
{
    try
    {
        if(mImpl)throw std::runtime_error("cannot replace a live native particle definition");
        if(!app||!kind||!std::isfinite(px)||!std::isfinite(py)||px<=0||py<=0)throw std::runtime_error("invalid native particle projection");
        std::ifstream input(path);if(!input)throw std::runtime_error("missing world particle package");const Json data=Json::parse(input);
        if(data.at("schema")!=2||data.at("sourceKind")!=kind||!data.at("ignoredBindings").empty()||!data.at("deferredComponents").empty()||data.at("emitters").empty()||data.at("emitters").size()>64)
            throw std::runtime_error("unsupported world particle package");
        auto definition=std::make_unique<Impl>();const auto folder=std::filesystem::path(path).parent_path();const size_t count=data.at("emitters").size();
        definition->emitters.resize(count);definition->names.resize(count);definition->fields.resize(count);definition->collisions.resize(count);
        definition->native.mEmitterDefs=definition->emitters.data();definition->native.mEmitterDefCount=int(count);
        const float pixels=std::sqrt(px*py);std::size_t i=0;
        for(const auto& source:data.at("emitters"))
        {
            for(const auto& module:source.at("enabledModules"))
                if(module!="InitialModule"&&module!="EmissionModule"&&module!="ShapeModule"&&module!="SizeModule"&&module!="UVModule"&&module!="ColorModule"&&module!="VelocityModule"&&module!="ForceModule"&&module!="RotationModule"&&module!="CollisionModule")throw std::runtime_error("unsupported enabled source particle module");
            auto& out=definition->emitters[i];definition->names[i]=source.at("nodeName").get<std::string>()+"#"+std::to_string(i);out.mName=definition->names[i].c_str();out.mOnDuration="";
            out.mEmitterType=EmitterType::EMITTER_CIRCLE;out.mImageFrames=1;
            // Explicit native-compatible material policy, not an inferred Unity
            // shader blend. Source PNG/color/shape/lifetime remain untouched.
            const auto blend=source.value("nativeBlend",std::string("alpha"));
            if(blend=="additive")out.mParticleFlags|=1<<int(ParticleFlags::PARTICLE_ADDITIVE);
            else if(blend!="alpha")throw std::runtime_error("unsupported native particle blend policy");
            // Source Meteorite trails genuinely retain particles for5..25s.
            const float delay=Number(source.at("delay"),0,10),duration=Number(source.at("duration"),.001f,10),life=Number(source.at("lifetime"),.01f,30);const int lifeTicks=Ticks(life);const bool loop=source.value("looping",false);const int total=loop?Ticks(duration):Ticks(delay+duration+life);const float last=float(total-1);
            if(loop&&delay!=0)throw std::runtime_error("looping source delay requires one-shot delay phase");
            if(loop)out.mParticleFlags|=1<<int(ParticleFlags::PARTICLE_SYSTEM_LOOPS);
            const int simulation=source.at("sourceSimulationSpace").get<int>();if(simulation<0||simulation>1)throw std::runtime_error("unsupported custom source simulation space");
            if(simulation==1)out.mParticleFlags|=1<<int(ParticleFlags::PARTICLE_PARTICLES_DONT_FOLLOW);
            definition->Constant(out.mSystemDuration,float(total));
            const auto lifeRange=source.value("lifetimeRange",Json::array({life,life}));const float minimumLife=Number(lifeRange[0],.01f,life);
            definition->Track(out.mParticleDuration,{{0,float(Ticks(minimumLife)),float(lifeTicks)}});
            const bool active=source.at("sourceActive").get<bool>();const float rate=active?Number(data.value("bindingScope",std::string())=="embeddedParticleComponents"?source.at("sourceEmissionRate"):source.at("rate"),0,10000):0;
            const float begin=delay*100/last,end=(delay+duration)*100/last,epsilon=.000001f;
            if(delay>0)definition->Track(out.mSpawnRate,{{0,0,0},{begin-epsilon,0,0},{begin,rate,rate},{end-epsilon,rate,rate},{end,0,0},{1,0,0}});
            else definition->Track(out.mSpawnRate,{{0,rate,rate},{end-epsilon,rate,rate},{end,0,0},{1,0,0}});
            if(loop)definition->Constant(out.mSpawnRate,rate);
            const int maximum=source.at("maxActive").get<int>();if(maximum<1||maximum>10000)throw std::runtime_error("source particle capacity exceeds native bound");
            definition->Constant(out.mSpawnMaxActive,float(maximum));definition->Constant(out.mSpawnMinActive,-1);
            const float burst=active?Number(source.at("burstCount"),0,float(maximum)):0;
            if(burst>0){if(delay!=0||burst!=std::trunc(burst))throw std::runtime_error("unsupported delayed/fractional source burst");definition->Track(out.mSpawnMinActive,{{0,burst,burst},{epsilon,0,0},{1,0,0}});}
            const auto& position=source.at("sourcePosition");definition->Constant(out.mEmitterOffsetX,Number(position.at("x"),-40,40)*px);definition->Constant(out.mEmitterOffsetY,-Number(position.at("y"),-40,40)*py);
            const float scale=Number(source.at("sourceScale").at("x"),.00001f,20);const auto& shape=source.at("shape");const int type=shape.at("type").get<int>();
            if(source.contains("collision"))
            {
                const auto& v=source.at("collision");auto& c=definition->collisions[i];
                if(v.at("type")!=0||v.at("mode")!=0||v.at("planeBinding")!="sourceZombieRowPlane"||v.at("colliderForce")!=0||
                   !v.at("collisionMessages").get<bool>())throw std::runtime_error("unsupported source particle collision binding");
                c.enabled=true;c.pixelsX=px;c.pixelsY=py;
                c.profile={Number(v.at("damping"),0,1),Number(v.at("bounce"),0,1),Number(v.at("lifetimeLoss"),0,1),Number(v.at("minimumSpeed"),0,10000),Number(v.at("maximumSpeed"),0,10000)};
                if(!FusionParticleCollision269ProfileValid(c.profile))throw std::runtime_error("invalid source collision profile");
                const float largest=(std::max)({scale,Number(source.at("sourceScale").at("y"),.00001f,20),Number(source.at("sourceScale").at("z"),.00001f,20)});
                const float width=Number(source.at("sprite").at("normalizationWidth"),1,4096);
                c.radiusPerNativeScale=width*largest*py/(2*scale*pixels)*Number(v.at("radiusScale"),0,10);
            }
            else for(const auto& module:source.at("enabledModules"))if(module=="CollisionModule")throw std::runtime_error("missing source collision descriptor");
            definition->Constant(out.mParticleStretch,Number(source.at("sourceScale").at("y"),.00001f,20)/scale);
            if(type!= -1&&type!=0&&type!=4&&type!=5&&type!=10&&type!=18)throw std::runtime_error("unsupported particle shape");
            if(type==10)
            {
                const auto& rotation=source.at("sourceRotation");
                if(Number(shape.at("arc"),0,360)!=360||Number(shape.at("radiusThickness"),0,1)!=1||Number(source.at("speed"),0,100)!=0||
                   Number(rotation.at("x"),-1,1)!=0||Number(rotation.at("y"),-1,1)!=0||Number(rotation.at("z"),-1,1)!=0||Number(rotation.at("w"),-1,1)!=1)
                    throw std::runtime_error("unsupported moving/partial/rotated source circle");
            }
            const float radius=Number(shape.at("radius"),0,20)*Number(shape.at("scale").at("x"),0,20)*scale*pixels;definition->Track(out.mEmitterRadius,{{0,0,radius}});
            if(type==5){out.mEmitterType=EmitterType::EMITTER_BOX;const float x=Number(shape.at("scale").at("x"),0,40)*scale*px*.5f,y=Number(shape.at("scale").at("y"),0,40)*scale*py*.5f;definition->Track(out.mEmitterBoxX,{{0,-x,x}});definition->Track(out.mEmitterBoxY,{{0,-y,y}});}
            FusionWorldParticle269Rectangle rectangle;
            if(type==18)
            {
                rectangle=Rectangle(source,px,py);out.mEmitterType=EmitterType::EMITTER_BOX;
                definition->Track(out.mEmitterBoxX,{{0,-rectangle.halfX,rectangle.halfX}});
                definition->Track(out.mEmitterBoxY,{{0,-rectangle.halfY,rectangle.halfY}});
                definition->Constant(out.mLaunchAngle,rectangle.launchDegrees);
            }
            if(type==4)
            {
                const auto& q=source.at("sourceRotation");const float x=Number(q.at("x"),-1,1),y=Number(q.at("y"),-1,1),z=Number(q.at("z"),-1,1),w=Number(q.at("w"),-1,1);
                const float angle=float(std::atan2(2*(x*z+w*y)*px,-2*(y*z-w*x)*py)*180/3.141592653589793),cone=Number(shape.at("angle"),0,90);definition->Track(out.mLaunchAngle,{{0,angle-cone,angle+cone}});
                if(shape.contains("nativeCompatibility"))
                {
                    if(shape.at("randomDirectionAmount")!=1)throw std::runtime_error("unsupported random cone projection");
                    const auto& offset=shape.at("position");const float ox=Number(offset.at("x"),-40,40)*scale,oy=Number(offset.at("y"),-40,40)*scale,oz=Number(offset.at("z"),-40,40)*scale;
                    const float tx=(1-2*(y*y+z*z))*ox+2*(x*y-z*w)*oy+2*(x*z+y*w)*oz;
                    const float ty=2*(x*y+z*w)*ox+(1-2*(x*x+z*z))*oy+2*(y*z-x*w)*oz;
                    definition->Constant(out.mEmitterOffsetX,(Number(position.at("x"),-40,40)+tx)*px);
                    definition->Constant(out.mEmitterOffsetY,-(Number(position.at("y"),-40,40)+ty)*py);
                    // Explicit visual compatibility: fully random 3D launch
                    // directions use the native circle's 2D angular RNG.
                    definition->Constant(out.mLaunchAngle,0);
                    for(const char* axis:{"x","y","z"})Number(shape.at("rotation").at(axis),-360,360);
                }
            }
            const float speedHigh=Number(source.at("speed"),0,100),speedScale=type==18?rectangle.launchSpeedScale:scale*pixels;const auto speedRange=source.value("speedRange",Json::array({speedHigh,speedHigh}));definition->Track(out.mLaunchSpeed,{{0,Number(speedRange[0],0,speedHigh)*speedScale,speedHigh*speedScale}});
            for(auto* track:{&out.mSystemRed,&out.mSystemGreen,&out.mSystemBlue,&out.mSystemAlpha,&out.mSystemBrightness,&out.mParticleBrightness})definition->Constant(*track,1);
            const auto& color=source.at("color");const auto& gradient=source.at("colorCurve");
            const auto interpolation=source.value("colorInterpolation",std::string("linear"));
            if(interpolation!="linear"&&interpolation!="fixed")throw std::runtime_error("unsupported source gradient interpolation");
            if(gradient.size()<2||gradient.size()>18)throw std::runtime_error("invalid source particle gradient count");
            const char* channels[]={"r","g","b","a"};FloatParameterTrack* targets[]={&out.mParticleRed,&out.mParticleGreen,&out.mParticleBlue,&out.mParticleAlpha};
            const auto colorRange=source.value("initialColorRange",Json::array({color,color}));
            if(!colorRange.is_array()||colorRange.size()!=2)throw std::runtime_error("invalid initial source color range");
            for(int c=0;c<4;++c)
            {
                const float a=Number(colorRange[0].at(channels[c]),0,64),b=Number(colorRange[1].at(channels[c]),0,64);
                const float low=(std::min)(a,b),high=(std::max)(a,b);
                std::vector<FusionWorldParticle269Sample> values;float before=-1;
                for(const auto& key:gradient){if(key.size()!=5)throw std::runtime_error("invalid source gradient key");const float time=Number(key[0],0,1);if(time<=before)throw std::runtime_error("source gradient time order");before=time;const float multiplier=Number(key[c+1],0,64);values.push_back({time,std::clamp(low*multiplier,0.f,1.f),std::clamp(high*multiplier,0.f,1.f)});}
                if(interpolation=="fixed")
                {
                    // Adjacent float times encode a true held segment. There
                    // is no representable sampled time in the transition gap.
                    std::vector<FusionWorldParticle269Sample> held;
                    for(std::size_t n=0;n<values.size();++n)
                    {
                        if(n){auto left=values[n-1];left.time=std::nextafter(values[n].time,-INFINITY);
                            if(left.time>held.back().time)held.push_back(left);}
                        held.push_back(values[n]);
                    }
                    definition->Track(*targets[c],held);
                }
                else definition->Track(*targets[c],values);
            }
            const auto speed=Curve(source.at("speedCurve"));std::vector<FusionWorldParticle269Sample> friction;
            if(!BuildFusionWorldParticle269Friction(speed,lifeTicks,friction))throw std::runtime_error("speed multiplier requires unsupported acceleration/resurrection");
            const bool forcePresent=source.contains("forceCurves")&&!source.at("forceCurves").is_null();
            const bool velocityPresent=source.contains("velocityCurves")&&!source.at("velocityCurves").is_null();
            bool linearPresent=false,orbitPresent=false;
            if(velocityPresent)
            {
                for(const auto& axis:source.at("velocityCurves").at("axes"))for(const auto& curve:axis)for(const auto& key:curve)if(key[1].get<float>()!=0)linearPresent=true;
                for(const auto& curve:source.at("velocityCurves").at("orbitalZ"))for(const auto& key:curve)if(key[1].get<float>()!=0)orbitPresent=true;
            }
            const int fieldCount=1+(forcePresent?1:0)+(linearPresent?1:0)+(orbitPresent?2:0);if(fieldCount>MAX_PARTICLE_FIELDS)throw std::runtime_error("source particle fields exceed native capacity");
            auto& fields=definition->fields[i];fields.resize(fieldCount);fields[0].mFieldType=ParticleFieldType::FIELD_FRICTION;definition->Track(fields[0].mX,friction);definition->Track(fields[0].mY,friction);int nextField=1;
            if(forcePresent)
            {
                const auto& force=source.at("forceCurves");const auto& axes=force.at("axes");if(axes.size()!=3)throw std::runtime_error("invalid source force axes");
                const auto x0=Curve(axes[0][0]),x1=Curve(axes[0][1]),y0=Curve(axes[1][0]),y1=Curve(axes[1][1]),z0=Curve(axes[2][0]),z1=Curve(axes[2][1]);
                std::vector<FusionWorldParticle269Sample> xs,ys;const bool world=force.at("world").get<bool>();const auto& q=source.at("sourceRotation");
                const float qx=Number(q.at("x"),-1,1),qy=Number(q.at("y"),-1,1),qz=Number(q.at("z"),-1,1),qw=Number(q.at("w"),-1,1);
                for(int n=0;n<lifeTicks;++n)
                {
                    const float t=float(n)/(lifeTicks-1);float low[3],high[3];
                    if(!SampleFusionWorldParticle269Curve(x0,t,low[0])||!SampleFusionWorldParticle269Curve(x1,t,high[0])||!SampleFusionWorldParticle269Curve(y0,t,low[1])||!SampleFusionWorldParticle269Curve(y1,t,high[1])||!SampleFusionWorldParticle269Curve(z0,t,low[2])||!SampleFusionWorldParticle269Curve(z1,t,high[2]))throw std::runtime_error("invalid source force curve");
                    float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
                    for(int bits=0;bits<8;++bits){const float x=(bits&1)?high[0]:low[0],y=(bits&2)?high[1]:low[1],z=(bits&4)?high[2]:low[2];const float rx=world?x:(1-2*(qy*qy+qz*qz))*x+2*(qx*qy-qw*qz)*y+2*(qx*qz+qw*qy)*z,ry=world?y:2*(qx*qy+qw*qz)*x+(1-2*(qx*qx+qz*qz))*y+2*(qy*qz-qw*qx)*z;minX=(std::min)(minX,rx);maxX=(std::max)(maxX,rx);minY=(std::min)(minY,-ry);maxY=(std::max)(maxY,-ry);}
                    // Native FIELD_ACCELERATION adds .01*field to px/tick velocity.
                    xs.push_back({t,minX*px*.01f,maxX*px*.01f});ys.push_back({t,minY*py*.01f,maxY*py*.01f});
                }
                fields[nextField].mFieldType=ParticleFieldType::FIELD_ACCELERATION;definition->Track(fields[nextField].mX,xs);definition->Track(fields[nextField].mY,ys);++nextField;
            }
            if(velocityPresent)
            {
                const auto& velocity=source.at("velocityCurves");const auto& axes=velocity.at("axes");
                const auto x0=Curve(axes[0][0]),x1=Curve(axes[0][1]),y0=Curve(axes[1][0]),y1=Curve(axes[1][1]),z0=Curve(axes[2][0]),z1=Curve(axes[2][1]),omega0=Curve(velocity.at("orbitalZ")[0]),omega1=Curve(velocity.at("orbitalZ")[1]);
                std::vector<FusionWorldParticle269Sample> xs,ys,circles,aways;const bool world=velocity.at("world").get<bool>();const auto& q=source.at("sourceRotation");
                const float qx=Number(q.at("x"),-1,1),qy=Number(q.at("y"),-1,1),qz=Number(q.at("z"),-1,1),qw=Number(q.at("w"),-1,1);
                for(int n=0;n<lifeTicks;++n)
                {
                    const float t=float(n)/(lifeTicks-1);float low[3],high[3],modifier=0,wa=0,wb=0;
                    if(!SampleFusionWorldParticle269Curve(x0,t,low[0])||!SampleFusionWorldParticle269Curve(x1,t,high[0])||!SampleFusionWorldParticle269Curve(y0,t,low[1])||!SampleFusionWorldParticle269Curve(y1,t,high[1])||!SampleFusionWorldParticle269Curve(z0,t,low[2])||!SampleFusionWorldParticle269Curve(z1,t,high[2])||!SampleFusionWorldParticle269Curve(speed,t,modifier)||!SampleFusionWorldParticle269Curve(omega0,t,wa)||!SampleFusionWorldParticle269Curve(omega1,t,wb))throw std::runtime_error("invalid source velocity-over-life curve");
                    float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
                    for(int bits=0;bits<8;++bits){const float x=(bits&1)?high[0]:low[0],y=(bits&2)?high[1]:low[1],z=(bits&4)?high[2]:low[2];const float rx=world?x:(1-2*(qy*qy+qz*qz))*x+2*(qx*qy-qw*qz)*y+2*(qx*qz+qw*qy)*z,ry=world?y:2*(qx*qy+qw*qz)*x+(1-2*(qx*qx+qz*qz))*y+2*(qy*qz-qw*qx)*z;minX=(std::min)(minX,rx);maxX=(std::max)(maxX,rx);minY=(std::min)(minY,-ry);maxY=(std::max)(maxY,-ry);}
                    xs.push_back({t,minX*modifier*px,maxX*modifier*px});ys.push_back({t,minY*modifier*py,maxY*modifier*py});
                    float ca=0,aa=0,cb=0,ab=0;if(!TryGetFusionWorldParticle269Orbit(-wa*modifier,.01f,ca,aa)||!TryGetFusionWorldParticle269Orbit(-wb*modifier,.01f,cb,ab))throw std::runtime_error("source orbital velocity exceeds native step bound");
                    circles.push_back({t,(std::min)(ca,cb),(std::max)(ca,cb)});aways.push_back({t,(std::min)(aa,ab),(std::max)(aa,ab)});
                }
                if(linearPresent){fields[nextField].mFieldType=ParticleFieldType::FIELD_VELOCITY;definition->Track(fields[nextField].mX,xs);definition->Track(fields[nextField].mY,ys);++nextField;}
                if(orbitPresent){fields[nextField].mFieldType=ParticleFieldType::FIELD_CIRCLE;definition->Constant(fields[nextField].mX,0);definition->Track(fields[nextField].mY,circles);++nextField;fields[nextField].mFieldType=ParticleFieldType::FIELD_AWAY;definition->Constant(fields[nextField].mX,0);definition->Track(fields[nextField].mY,aways);++nextField;}
            }
            out.mParticleFields=fields.data();out.mParticleFieldCount=int(fields.size());
            if(source.contains("spinCurves")&&!source.at("spinCurves").is_null())
            {
                const auto a=Curve(source.at("spinCurves")[0]),b=Curve(source.at("spinCurves")[1]);std::vector<FusionWorldParticle269Sample> spins;
                for(int n=0;n<lifeTicks;++n){const float t=float(n)/(lifeTicks-1);float x=0,y=0;if(!SampleFusionWorldParticle269Curve(a,t,x)||!SampleFusionWorldParticle269Curve(b,t,y))throw std::runtime_error("invalid source rotation curve");spins.push_back({t,float(x*180/3.141592653589793),float(y*180/3.141592653589793)});}definition->Track(out.mParticleSpinSpeed,spins);
            }
            if(source.at("rendererEnabled").get<bool>())
            {
                const auto& sprite=source.at("sprite");const auto relative=std::filesystem::path(sprite.at("texture").get<std::string>());if(relative.is_absolute()||relative.lexically_normal().string().find("..")!=std::string::npos)throw std::runtime_error("invalid particle texture path");
                const auto file=folder/relative;const auto hash=sprite.at("textureSha256").get<std::string>();if(Hash(file)!=hash)throw std::runtime_error("source particle image hash differs");
                auto stem=file;stem.replace_extension();auto image=app->GetSharedImage(Sexy::StringToSexyString(stem.string()),Sexy::StringToSexyString("FUSION_WORLD_PARTICLE_SHA256:"+hash));out.mImage=static_cast<Sexy::Image*>(image);
                const int frames=sprite.at("frames").get<int>();if(frames<1||frames>16||!out.mImage||out.mImage->mWidth!=sprite.at("width").get<int>()||out.mImage->mHeight!=sprite.at("height").get<int>()||out.mImage->mWidth%frames||Hash(file)!=hash)throw std::runtime_error("particle sprite sequence decode differs");
                out.mImage->mNumCols=frames;out.mImage->mNumRows=1;out.mImageFrames=frames;out.mAnimated=frames>1;definition->images.push_back(image);
                const auto& range=source.at("sizeRange");if(range.size()!=2)throw std::runtime_error("invalid initial particle range");const float low=Number(range[0],0,20),high=Number(range[1],low,20),factor=scale*pixels/Number(sprite.value("normalizationWidth",Json(out.mImage->mWidth/frames)),1,4096);
                std::vector<FusionWorldParticle269Sample> sizes;
                if(source.contains("sizeHermite"))
                {
                    const auto curve=Curve(source.at("sizeHermite"));for(int n=0;n<lifeTicks;++n){const float t=float(n)/(lifeTicks-1);float value=0;if(!SampleFusionWorldParticle269Curve(curve,t,value)||value< -1e-6f||value>20)throw std::runtime_error("invalid sampled source size");sizes.push_back({t,(std::max)(0.f,value)*low*factor,(std::max)(0.f,value)*high*factor});}
                }
                else sizes={{0,low*factor,high*factor},{1,low*factor,high*factor}};
                definition->Track(out.mParticleScale,sizes);const float rotation=Number(source.at("rotation"),-10,10);const auto rotationRange=source.value("rotationRange",Json::array({rotation,rotation}));definition->Track(out.mParticleSpinAngle,{{0,float(Number(rotationRange[0],-10,rotation)*180/3.141592653589793),float(rotation*180/3.141592653589793)}});
            }
            ++i;
        }
        definition->muted=definition->native;definition->mutedEmitters=definition->emitters;
        for(auto& emitter:definition->mutedEmitters){emitter.mSpawnRate={&definition->zeroEmission,1};emitter.mSpawnMinActive={&definition->zeroEmission,1};}
        definition->muted.mEmitterDefs=definition->mutedEmitters.data();
        mImpl=std::move(definition);error.clear();return true;
    }
    catch(const std::exception& ex){error=ex.what();return false;}
}
}
