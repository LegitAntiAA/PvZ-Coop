#include "NetJackbox309Audio.h"
#include "NetCommon.h"
#include "NetPresentation.h"
#include "../Fusion/FusionNative.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/SoundManager.h"
#include "../../SexyAppFramework/SoundInstance.h"
#include "../../SexyAppFramework/DSoundManager.h"
#include <map>
#include <set>

namespace LawnNet
{
namespace
{
using Operation=LawnFusion::NativeJackbox300Audio;
struct Channel309
{
    uint32_t token=0;
    Sexy::SoundInstance* instance=nullptr;
    bool wanted=false,paused=false,attempted=false;
    ~Channel309(){if(instance)instance->Release();}
};
bool Identity(Zombie* z,uint32_t& uid,uint32_t& token)
{
    const auto* data=LawnFusion::GetNativeSourceZombieData(z);
    if(!z||!z->mBoard||!z->mApp||z->mApp->mBoard!=z->mBoard||z->mDead||
        !data||!LawnFusion::FusionJackbox300Kind(data->kind)||!data->instanceToken)return false;
    uid=z->mBoard->mZombies.DataArrayGetID(z);token=data->instanceToken;
    return uid!=0;
}
}
struct NetJackbox309Audio::Impl
{
    LawnApp* app;
    uint32_t epoch=0;
    std::map<uint32_t,std::unique_ptr<Channel309>> channels;
    explicit Impl(LawnApp* value):app(value){}
    Channel309& Get(uint32_t uid,uint32_t token)
    {
        auto& channel=channels[uid];
        if(!channel||channel->token!=token)
        {channel=std::make_unique<Channel309>();channel->token=token;}
        return *channel;
    }
    void SetWanted(Channel309& c,bool wanted)
    {
        if(c.wanted!=wanted)c.attempted=false;
        c.wanted=wanted;
    }
    void Present(Channel309& c,bool suspended)
    {
        const bool audible=c.wanted&&!suspended;
        if(!audible)
        {
            if(c.instance&&!c.paused)
            {
                c.paused=c.instance->Pause();
                if(c.paused)NetTrace("Fusion Jackbox309 audio pause token=%u",c.token);
            }
            return;
        }
        if(c.instance)
        {
            if(c.paused)
            {
                if(c.instance->Resume())
                {c.paused=false;NetTrace("Fusion Jackbox309 audio resume token=%u",c.token);}
                else if(!c.instance->IsPaused())
                {
                    // StopAllSounds may reset a buffer while the source still
                    // wants a paused loop. Its cursor is gone. A failed Resume
                    // of a genuinely paused buffer must retain that cursor.
                    c.paused=false;c.instance->Play(true,false);
                }
            }
            else if(!c.instance->IsPlaying())c.instance->Play(true,false);
            return;
        }
        if(c.attempted)return;
        c.attempted=true;
        // Channel exhaustion is cosmetic. Never replay a source callback or
        // repeatedly allocate channels while the same desired period persists.
        auto* instance=app->mSoundManager->GetSoundInstance(Sexy::SOUND_JACKINTHEBOX);
        if(!instance)return;
        // Source Awake writes gameSoundVolume; native's mixer already owns
        // that user volume. Apply relative1 rather than multiplying it twice.
        instance->SetVolume(1);
        if(!instance->Play(true,false)){instance->Release();return;}
        c.instance=instance;c.paused=false;
        NetTrace("Fusion Jackbox309 audio start token=%u source=89 relativeVolume=1",c.token);
    }
};
NetJackbox309Audio::NetJackbox309Audio(LawnApp* app):mImpl(std::make_unique<Impl>(app)){}
NetJackbox309Audio::~NetJackbox309Audio()=default;
bool NetSourceJackbox309Ready(LawnApp* app)
{
    // DSound is the game's actual backend; its pause preserves the buffer
    // cursor. Unsupported backends cannot silently replace Pause with Stop.
    return app&&app->mSoundManager&&dynamic_cast<Sexy::DSoundManager*>(app->mSoundManager)&&
        Sexy::SOUND_JACKINTHEBOX>=0&&NetSourceSound284Ready(app,90)&&
        NetSourceSound284Ready(app,91)&&NetSourceSound284Ready(app,43);
}
void NetJackbox309Audio::Apply(Zombie* z,Operation operation)
{
    uint32_t uid=0,token=0;
    if(!Identity(z,uid,token)||z->mApp!=mImpl->app)return;
    const auto* context=LawnFusion::GetNativeContext(z->mBoard);
    if(!context)return;
    if(mImpl->epoch!=context->epoch)
    {mImpl->channels.clear();mImpl->epoch=context->epoch;}
    auto& c=mImpl->Get(uid,token);
    mImpl->SetWanted(c,operation!=Operation::Pause);
    if(operation==Operation::Play&&c.instance)
    {
        // Actual AudioSource.Play restarts; UnPause below keeps its cursor.
        if(c.instance->Play(true,false))c.paused=false;
    }
    mImpl->Present(c,z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING||z->mApp->mMuteSoundsForCutscene);
}
void NetJackbox309Audio::Retire(Zombie* z)
{
    if(!z||!z->mBoard||z->mApp!=mImpl->app)return;
    const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);
    const auto it=mImpl->channels.find(uid);
    if(it==mImpl->channels.end())return;
    NetTrace("Fusion Jackbox309 audio retire id=%u token=%u",z->mNetId,it->second->token);
    mImpl->channels.erase(it);
}
void NetJackbox309Audio::Update(Board* board)
{
    auto* context=LawnFusion::GetNativeContext(board);
    if(!context||!board||board->mApp!=mImpl->app)return;
    if(mImpl->epoch!=context->epoch)
    {mImpl->channels.clear();mImpl->epoch=context->epoch;}
    std::set<uint32_t> keep;
    for(const auto& [uid,state]:context->jackboxZombies300)
    {
        auto* z=board->mZombies.DataArrayTryToGet(uid);uint32_t actual=0,token=0;
        if(!Identity(z,actual,token)||actual!=uid||!state.initialized||state.failed)continue;
        keep.insert(uid);auto& c=mImpl->Get(uid,token);
        mImpl->SetWanted(c,state.audioPlaying);
        mImpl->Present(c,board->mPaused||board->mApp->mGameScene!=SCENE_PLAYING||board->mApp->mMuteSoundsForCutscene);
    }
    for(auto it=mImpl->channels.begin();it!=mImpl->channels.end();)
        if(!keep.contains(it->first))it=mImpl->channels.erase(it);else ++it;
}
void NetSourceJackbox309Loop(Zombie* z,Operation operation)
{
    auto* context=LawnFusion::GetNativeContext(z?z->mBoard:nullptr);
    if(!context||!NetSourceJackbox309Ready(z->mApp))return;
    if(!context->jackboxAudio309)context->jackboxAudio309=std::make_unique<NetJackbox309Audio>(z->mApp);
    context->jackboxAudio309->Apply(z,operation);
}
void NetSourceJackbox309Retire(Zombie* z)
{
    auto* context=LawnFusion::GetNativeContext(z?z->mBoard:nullptr);
    if(context&&context->jackboxAudio309)context->jackboxAudio309->Retire(z);
}
void NetUpdateSourceJackbox309Audio(Board* board)
{
    auto* context=LawnFusion::GetNativeContext(board);
    if(!context)return;
    if(!context->jackboxAudio309&&!context->jackboxZombies300.empty()&&NetSourceJackbox309Ready(board->mApp))
        context->jackboxAudio309=std::make_unique<NetJackbox309Audio>(board->mApp);
    if(context->jackboxAudio309)context->jackboxAudio309->Update(board);
}
}
