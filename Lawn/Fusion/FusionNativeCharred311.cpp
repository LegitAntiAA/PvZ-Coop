#include "FusionNativeCharred311.h"
#include "FusionNativeSourceAnimated317.h"
#include "FusionAnimation.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeNightZombie291Bridge.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <tuple>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <Windows.h>

namespace LawnFusion
{
namespace
{
constexpr float pixelsPerSourceUnit=60;
constexpr size_t maximumSeen=131070;
bool OrdinaryWorld317(const Board* b)
{
    const auto* c=GetNativeContext(b);
    if(!b||!c||!b->mApp||b->mApp->mBoard!=b||!IsNativeFusionBoard(b)||!c->epoch||
       c->epoch!=b->mApp->mNativeFusionEpoch||b->GetColumnCount()!=10||!b->mTDPath.empty())return false;
    const int scene=int(b->mBackground),rows=GetNativeFusionRowCount(b);
    if(scene<0||scene>3||rows!=(scene<2?5:6))return false;
    if(c->adventure.enabled)
    {
        const auto* level=FindFusionSourceAdventure272(c->adventure.run.level);
        if(!b->mApp->IsFusionSourceAdventure()||c->adventure.run.cycle!=1||!level||level->scene!=scene||
           level->rows!=rows||level->columns!=10)return false;
    }
    else if(!c->validation)return false;
    for(int row=0;row<rows;++row)
        if(b->mPlantRow[row]!=((rows==6&&(row==2||row==3))?PLANTROW_POOL:PLANTROW_NORMAL))return false;
    return true;
}
std::filesystem::path ModelPath(uint8_t variant)
{
    const auto* spec=FindSourceAnimated317Spec(variant);if(!spec)throw std::runtime_error("unknown source animated317 variant");
    std::wstring path(32768,L'\0');const DWORD count=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(!count||count>=path.size())throw std::runtime_error("source animated317 executable path unavailable");
    path.resize(count);return std::filesystem::path(path).parent_path()/"fusion"/"pilot"/spec->package/"animation.json";
}
bool CarrierReady(std::string& error)
{
    ReanimatorEnsureDefinitionLoaded(REANIM_ZOMBIE_CHARRED,false);
    if(!gReanimatorDefArray||unsigned(REANIM_ZOMBIE_CHARRED)>=gReanimatorDefCount)
    {error="source Charred311 clock definition unavailable";return false;}
    const auto& tracks=gReanimatorDefArray[int(REANIM_ZOMBIE_CHARRED)].mTracks;
    if(!tracks.tracks||tracks.count<=0||!tracks.tracks[0].mTransforms.mTransforms||tracks.tracks[0].mTransforms.count<=0)
    {error="source Charred311 native clock has no frames";return false;}
    for(int i=0;i<tracks.count;++i)
        if(!tracks.tracks[i].mName||!tracks.tracks[i].mTransforms.mTransforms||
           tracks.tracks[i].mTransforms.count!=tracks.tracks[0].mTransforms.count||
           _strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)
        {error="source Charred311 native clock definition has unbudgeted tracks/attacher";return false;}
    return true;
}
}
struct NativeSourceCharred311::Impl
{
    Board* board=nullptr;LawnApp* app=nullptr;ReanimationHolder* holder=nullptr;uint32_t epoch=0;
    struct Model {std::unique_ptr<FusionAnimation> animation;float shadowX=0,shadowY=0;};
    std::array<Model,4> models;
    std::map<uint32_t,SourceCharred311Birth> leases;
    std::set<decltype(SourceAnimated317SeenKey(SourceCharred311Birth{}))> seen;
    bool Valid(Board* current) const
    {
        const auto* context=GetNativeContext(current);
        return current&&app&&holder&&current==board&&current->mApp==app&&app->mBoard==current&&context&&context->epoch==epoch&&
            app->mEffectSystem&&app->mEffectSystem==gEffectSystem&&app->mEffectSystem->mReanimationHolder==holder;
    }
    void Prune()
    {
        for(auto it=leases.begin();it!=leases.end();)
        {
            const auto* clock=holder->mReanimations.DataArrayTryToGet(it->first);
            if(!clock||clock->mDead)it=leases.erase(it);else ++it;
        }
    }
};
NativeSourceCharred311::NativeSourceCharred311(Board* board,uint32_t epoch):mImpl(std::make_unique<Impl>())
{
    mImpl->board=board;mImpl->app=board?board->mApp:nullptr;mImpl->epoch=epoch;
    mImpl->holder=mImpl->app&&mImpl->app->mEffectSystem?mImpl->app->mEffectSystem->mReanimationHolder:nullptr;
}
NativeSourceCharred311::~NativeSourceCharred311()
{
    auto* app=mImpl->app;
    if(!app||!app->mEffectSystem||app->mEffectSystem->mReanimationHolder!=mImpl->holder||!mImpl->holder)return;
    for(const auto& [uid,birth]:mImpl->leases)
        if(auto* clock=mImpl->holder->mReanimations.DataArrayTryToGet(uid))clock->ReanimationDie();
}
bool NativeSourceCharred311::Ensure(Board* board,size_t peak,std::string& error)
{return EnsureVariant(board,1,peak,error);}
bool NativeSourceCharred311::EnsureVariant(Board* board,uint8_t variant,size_t peak,std::string& error)
{
    try
    {
        const auto* spec=FindSourceAnimated317Spec(variant);
        if(!spec||!mImpl->Valid(board)||!OrdinaryWorld317(board))throw std::runtime_error("source animated317 board/epoch/world/variant unavailable");
        auto& slot=mImpl->models[variant-1];
        if(!slot.animation)
        {
            const auto path=ModelPath(variant);std::ifstream input(path);if(!input)throw std::runtime_error("source animated317 model unavailable");
            using Json=nlohmann::json;const Json model=Json::parse(input);const auto root=model.at("root").get<size_t>();
            if(model.at("sourcePrefab")!=spec->prefab||!model.at("ignoredBindings").empty()||!model.at("omittedInactiveNodes").empty()||
               !model.at("deferredComponents").empty()||model.at("nodes").size()!=spec->nodes||model.at("sprites").size()!=spec->sprites||
               model.at("controller").at("states").size()!=1||model.at("controller").at("layers").size()!=1||root!=0)
                throw std::runtime_error("source animated317 genuine hierarchy differs");
            for(const char* axis:{"x","y","z"})if(std::abs(model.at("nodes").at(root).at("scale").at(axis).get<float>()-spec->rootScale)>1e-6f)
                throw std::runtime_error("source animated317 genuine root scale differs");
            const auto& state=model.at("controller").at("states")[0];const auto& clip=model.at("clips").at(spec->state);
            if(state.at("name")!=spec->state||state.at("clip")!=spec->state||state.at("speedParameterActive").get<bool>()||
               std::abs(state.at("speed").get<float>()-spec->stateSpeed)>1e-6f||clip.at("loop").get<bool>()||
               std::abs(clip.at("duration").get<float>()-spec->clipSeconds)>1e-6f||clip.at("events").size()!=1||
               clip.at("events")[0].at("functionName")!="Die"||std::abs(clip.at("events")[0].at("time").get<float>()-spec->clipSeconds)>1e-6f)
                throw std::runtime_error("source animated317 controller/lifetime differs");
            if(variant==3)
            {
                for(const auto& curve:clip.at("curves"))if(curve.at("path")==""&&curve.at("kind")=="scale")
                    throw std::runtime_error("source water splash root-scale curve conflicts with genuine factory override");
                std::set<int> orders;for(const auto& sprite:model.at("sprites"))
                {if(sprite.at("sortingLayerId")!=0)throw std::runtime_error("source splash sorting layer differs");orders.insert(sprite.at("sortingOrder").get<int>());}
                if(orders!=std::set<int>{1,2,3,4})throw std::runtime_error("source splash child orders differ");
            }
            if(variant==4)for(const auto& sprite:model.at("sprites"))
                if(sprite.at("sortingLayerId")!=327025431||sprite.at("sortingOrder")!=40)throw std::runtime_error("source mind-control child sorting differs");
            auto pending=std::make_unique<FusionAnimation>();if(!pending->Load(mImpl->app,path.string(),error))throw std::runtime_error(error);
            float sx=0,sy=0;
            if(spec->shadowAnchor)
            {
                if(!model.at("anchors").at("shadow").at("relativeToRuntimeRoot").get<bool>()||
                   !pending->TryGetNodeOffset("Shadow",sx,sy,spec->state,0)||!std::isfinite(sx)||!std::isfinite(sy)||
                   std::abs(sx-model.at("anchors").at("shadow").at("x").get<float>())>1e-5f||
                   std::abs(sy-model.at("anchors").at("shadow").at("y").get<float>())>1e-5f)
                    throw std::runtime_error("source animated317 rest Shadow differs");
            }
            for(float seconds:{0.f,spec->PlaySeconds()*.5f,spec->PlaySeconds()})for(const auto& node:model.at("nodes"))
            {
                const auto nodePath=node.at("path").get<std::string>();float x=0,y=0;bool active=false;
                if(!pending->TryGetNodeOffset(nodePath.c_str(),x,y,spec->state,seconds)||!std::isfinite(x)||!std::isfinite(y)||
                   !pending->TryGetNodeActivity(nodePath.c_str(),active,spec->state,seconds))throw std::runtime_error("source animated317 sampled hierarchy unavailable");
                if(nodePath.empty()&&(x!=0||y!=0))throw std::runtime_error("source animated317 editor root position was not removed");
            }
            slot.shadowX=sx;slot.shadowY=sy;slot.animation=std::move(pending);
        }
        if(!CarrierReady(error))return false;
        const auto& pool=mImpl->holder->mReanimations;
        if(!pool.mBlock||pool.mSize>pool.mMaxSize||peak>size_t(pool.mMaxSize-pool.mSize))
            throw std::runtime_error("source animated317 whole-peak native Reanimation capacity unavailable");
        error.clear();return true;
    }
    catch(const std::exception& exception){if(mImpl->app)mImpl->app->CleanSharedImages();error=exception.what();return false;}
}
bool NativeSourceCharred311::Start(Board* board,const SourceCharred311Birth& birth,uint32_t& fullUID,std::string& error)
{
    fullUID=0;
    if(!mImpl->Valid(board)||!OrdinaryWorld317(board)||!SourceCharred311BirthValid(birth)||birth.epoch!=mImpl->epoch||
       birth.sourceRow>=GetNativeFusionRowCount(board)||!FindFusionZombieProfile(birth.sourceKind))
    {error="invalid source Charred311 immutable birth";return false;}
    const auto key=SourceAnimated317SeenKey(birth);
    if(mImpl->seen.contains(key)){error.clear();return true;}
    if(mImpl->seen.size()>=maximumSeen){error="source Charred311 cosmetic seen capacity";return true;}
    mImpl->seen.insert(key); // Consume before resource/native allocation; never replay a cosmetic drop.
    mImpl->Prune();
    if(!EnsureVariant(board,birth.variant,1,error))return true;
    std::map<uint32_t,SourceCharred311Birth> pending;pending.emplace(0,birth); // Reserve map node before native birth.
    auto* clock=mImpl->app->AddReanimation(birth.shadowX,birth.shadowY,birth.order,REANIM_ZOMBIE_CHARRED);
    if(!clock||clock->mFrameCount<=0){if(clock)clock->ReanimationDie();error="source Charred311 unexpected native clock failure";return true;}
    const auto* spec=FindSourceAnimated317Spec(birth.variant);
    clock->mLoopType=REANIM_PLAY_ONCE;clock->mAnimRate=float(clock->mFrameCount)/spec->LifeSeconds();
    clock->mAnimTime=0;clock->mLastFrameTime=-1;clock->mLoopCount=0;
    const uint32_t uid=uint32_t(mImpl->app->ReanimationGetID(clock));
    if(!uid){clock->ReanimationDie();error="source Charred311 missing full native generation";return true;}
    auto reserved=pending.extract(0);reserved.key()=uid;mImpl->leases.insert(std::move(reserved));fullUID=uid;
    LawnNet::NetTrace("Fusion source Charred311 started epoch=%u kind=%u id=%u token=%u clock=%u variant=%u serial=%u row=%u shadow=(%.6g,%.6g) flip=%d order=%d role=%s",
        birth.epoch,birth.sourceKind,birth.ownerNetID,birth.ownerToken,uid,birth.variant,birth.eventSerial,birth.sourceRow,birth.shadowX,birth.shadowY,birth.rootFlip?1:0,birth.order,mImpl->app->NetIsClient()?"client":"host");
    error.clear();return true;
}
bool NativeSourceCharred311::Draw(Sexy::Graphics* graphics,const Reanimation* clock) const
{
    if(!clock||!mImpl->Valid(mImpl->board)||clock->mReanimationHolder!=mImpl->holder)return false;
    const uint32_t uid=mImpl->holder->mReanimations.DataArrayGetID(const_cast<Reanimation*>(clock));
    const auto found=mImpl->leases.find(uid);if(found==mImpl->leases.end())return false;
    const auto& birth=found->second;const auto* spec=FindSourceAnimated317Spec(birth.variant);
    const auto& model=mImpl->models[birth.variant-1];
    if(!graphics||clock->mDead||!model.animation||!spec||clock->mReanimationType!=REANIM_ZOMBIE_CHARRED)return true;
    FusionAnimationDrawOptions options;options.rootFlipX=birth.rootFlip;
    float elapsed=0;if(!SourceAnimated317SampleSeconds(birth.variant,clock->mAnimTime,elapsed))return true;
    const float sx=birth.rootFlip?-model.shadowX:model.shadowX;
    const float pixels=pixelsPerSourceUnit*spec->drawMultiplier;
    // Source scale is inside the genuine prefab, except Splash factory's .4
    // override of its unanimated .3 root. No owner scale/tint, fake PS or timer.
    model.animation->DrawPose(graphics,birth.shadowX-sx*pixels,birth.shadowY+model.shadowY*pixels,pixels/100,
        spec->state,elapsed,nullptr,-1,1,&options);
    return true;
}
bool TryMakeNativeSourceCharred311(Zombie* zombie,int status,bool water,bool beforeDying,bool rootFlip,SourceCharred311Birth& output,std::string* error)
{
    const auto reject=[&](const char* text){if(error)*error=text;return false;};
    const auto* data=GetNativeSourceZombieData(zombie);auto* board=zombie?zombie->mBoard:nullptr;const auto* context=GetNativeContext(board);
    if(!zombie||!board||!context||zombie->mNetGhost||zombie->mApp->NetIsClient()||!OrdinaryWorld317(board)||
       !data||!FindFusionZombieProfile(data->kind)||!SourceCharred311SourceVariant(data->kind,status,true,water,beforeDying))
        return reject("source Charred311 owner/family visual conditions unsupported");
    SourceCharred311Birth birth;birth.epoch=context->epoch;birth.ownerNetID=zombie->mNetId;birth.ownerToken=data->instanceToken;
    birth.sourceKind=data->kind;birth.sourceRow=uint8_t(zombie->mRow);birth.rootFlip=rootFlip;
    birth.variant=SourceCharred311SourceVariant(data->kind,status,true,water,beforeDying);
    birth.order=Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,zombie->mRow,0);
    if(!GetNativeSourceZombie269NodePoint(zombie,"Shadow",birth.shadowX,birth.shadowY)||!SourceCharred311BirthValid(birth))
        return reject("source Charred311 exact source Shadow/identity unavailable");
    output=birth;if(error)error->clear();return true;
}
namespace
{
NativeSourceCharred311* Cache(Board* board,bool create)
{
    auto* context=GetNativeContext(board);if(!context||!board)return nullptr;
    if(!context->sourceCharred311&&create)context->sourceCharred311=std::make_unique<NativeSourceCharred311>(board,context->epoch);
    return context->sourceCharred311.get();
}
}
bool EnsureNativeSourceCharred311(Board* board,size_t peak,std::string* error)
{
    auto* cache=Cache(board,true);std::string failure;const bool ok=cache&&cache->Ensure(board,peak,failure);
    if(!cache)failure="source Charred311 context unavailable";if(error)*error=failure;return ok;
}
bool StartNativeSourceCharred311(Board* board,const SourceCharred311Birth& birth,uint32_t* fullUID,std::string* error)
{
    auto* cache=Cache(board,true);std::string failure;uint32_t uid=0;const bool ok=cache&&cache->Start(board,birth,uid,failure);
    if(!cache)failure="source Charred311 context unavailable";if(fullUID)*fullUID=uid;if(error)*error=failure;return ok;
}
bool DrawNativeSourceCharred311(Sexy::Graphics* graphics,const Reanimation* clock)
{
    auto* cache=gLawnApp?Cache(gLawnApp->mBoard,false):nullptr;return cache&&cache->Draw(graphics,clock);
}
bool EnsureNativeSourceAnimated317(Board* board,uint8_t variant,size_t peak,std::string* error)
{
    auto* cache=Cache(board,true);std::string failure;const bool ok=cache&&cache->EnsureVariant(board,variant,peak,failure);
    if(!cache)failure="source animated317 context unavailable";if(error)*error=failure;return ok;
}
bool EmitNativeSourceAnimated317(Zombie* z,uint8_t variant,float x,float y,int order,std::string* error)
{
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    auto* board=z?z->mBoard:nullptr;auto* context=GetNativeContext(board);const auto* data=GetNativeSourceZombieData(z);
    if(!z||!context||z->mNetGhost||z->mApp->NetIsClient()||!OrdinaryWorld317(board)||!data||!FindSourceAnimated317Spec(variant))
        return reject("source animated317 requires an authoritative actual source owner/world");
    SourceCharred311Birth birth;birth.epoch=context->epoch;birth.ownerNetID=z->mNetId;birth.ownerToken=data->instanceToken;
    birth.sourceKind=data->kind;birth.sourceRow=uint8_t(z->mRow);birth.variant=variant;birth.shadowX=x;birth.shadowY=y;birth.order=order;
    if(variant<=2)
    {
        std::array<float,6> matrix;
        if(GetNativeSourceZombieMatrix313(z,"",matrix))birth.rootFlip=matrix[0]<0;
        else if(const auto* jackson=GetNativeJackson291(z))birth.rootFlip=z->mMindControlled!=jackson->lookBack;
        else birth.rootFlip=z->IsWalkingBackwards(); // Existing legacy source Draw predicate.
    }
    else
    {
        birth.eventSerial=1; // Validate the immutable caller data before consuming a serial.
    }
    if(!SourceCharred311BirthValid(birth))return reject("source animated317 immutable birth identity/order invalid");
    if(variant>=3)
    {
        uint32_t serial=0;
        if(!TryNextSourceAnimated317Serial(context->sourceAnimatedSerial317,serial))
        {if(error)*error="source animated317 serial exhausted; cosmetic drop acknowledged";return true;}
        context->sourceAnimatedSerial317=serial;birth.eventSerial=serial;
    }
    if(!StartNativeSourceCharred311(board,birth,nullptr,error))return false;
    LawnNet::NetBroadcastSourceCharred315(z->mApp,birth);return true;
}

}
