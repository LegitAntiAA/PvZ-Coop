#include "FusionNativeDryGround353.h"
#include "FusionNativeNightZombie291Bridge.h"
#include "FusionNativeQuickJackson367.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeZombies.h"
#include "FusionNativeEffects269.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeWire.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>
#include <cstring>

namespace LawnFusion
{
namespace
{
bool Typed(const Zombie* z)
{const auto* data=GetNativeSourceZombieData(z);return data&&FusionJacksonFamily367Kind(data->kind);}
bool Authority(const Zombie* z)
{return Typed(z)&&z->mBoard&&z->mApp&&z->mApp->mBoard==z->mBoard&&!z->mNetGhost&&
    !z->mApp->NetIsClient()&&NativeSourceDryRow353(z->mBoard,z->mRow);}
bool Playing(const Zombie* z)
{return Authority(z)&&!z->mDead&&!z->mBoard->mPaused&&z->mApp->mGameScene==SCENE_PLAYING&&
    CanApplyNativeFusionGameplay(z->mBoard);}
void Failed(Zombie* z,const char* reason)
{
    auto* context=GetNativeContext(z?z->mBoard:nullptr);
    if(!context||!context->adventure.enabled||context->adventure.schedulerFailed)return;
    context->adventure.schedulerFailed=true;context->adventureScheduler.failed=true;
    const auto* data=GetNativeSourceZombieData(z);
    LawnNet::NetTrace("Fusion Jackson291 stopped id=%u token=%u reason=%s",z->mNetId,data?data->instanceToken:0,reason);
}
bool ClockReady(uint16_t kind,std::string* error)
{
    if(!FusionJacksonFamily367Kind(kind)||GetNativeSourceZombieCarrier(kind)!=ZOMBIE_NORMAL)
    {if(error)*error="Jackson requires the covered NORMAL clock-only carrier.";return false;}
    const auto type=GetZombieDefinition(ZOMBIE_NORMAL).mReanimationType;
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)
    {if(error)*error="Jackson raw clock definition unavailable.";return false;}
    const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
    if(!tracks.tracks||tracks.count<=0)
    {if(error)*error="Jackson raw clock definition has no tracks.";return false;}
    for(uint8_t phase=0;phase<=uint8_t(FusionNightZombie281Phase::Death);++phase)
    {
        FusionNightZombie281Clip clip;
        if(!TryGetFusionNightZombie281Clip(FusionNightZombie281Phase(phase),clip))return false;
        bool found=false;
        for(int i=0;i<tracks.count;++i)
            if(tracks.tracks[i].mName&&stricmp(tracks.tracks[i].mName,clip.nativeTrack)==0&&tracks.tracks[i].mTransforms.count>0)found=true;
        if(!found){if(error)*error=std::string("Jackson phase clock track unavailable: ")+clip.nativeTrack;return false;}
    }
    return true;
}
bool OrdinaryFalse(const Board*)
{return false;} // Hook table is reachable only through the genuine ordinary grass world.
bool PrepareChildren(Zombie* z,std::span<const uint16_t> kinds,std::string* error)
{
    return Playing(z)&&NativeSourceEffectParticle269(11)>=0&&EnsureNativeSourceEffect269(z->mBoard,11,error)&&
        CanAllocateNativeSourceZombieBatch280(z->mBoard,kinds,error);
}
bool FacingKeepShadow(Zombie* z,bool)
{
    if(!Authority(z))return false;
    float rootX=0,rootY=0,shadowX=0,shadowY=0;
    // NativeZombieVisual's mirrored-root normalizer already pins source Shadow
    // to actual native GetShadowCenter for either lookBack. The Core sets the
    // new view flag AFTER this callback; do not apply another root-position delta.
    return GetNativeSourceZombie269NodePoint(z,"",rootX,rootY)&&
        GetNativeSourceZombie269NodePoint(z,"Shadow",shadowX,shadowY)&&
        std::isfinite(rootX)&&std::isfinite(rootY)&&std::isfinite(shadowX)&&std::isfinite(shadowY);
}
void Sound(Zombie* z,uint16_t kind)
{if(Playing(z))LawnNet::NetPlaySourceSound284(z->mApp,kind,.5f);}
void FallSound(Zombie* z)
{if(Playing(z))z->mApp->PlayFoley(FOLEY_ZOMBIE_FALLING);}
void Destroy(Zombie* z)
{if(Authority(z)&&!z->mDead)z->DieNoLoot();} // Existing source DestoryZombie/Die(2) retirement.
bool OutsideBounds(const Zombie* z)
{
    if(!Authority(z))return false;
    float x=0,y=0;
    if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
    const double source=(x-z->mBoard->GetCellCenterX(0,z->mRow))/kFusionBasePixelsX-4.8;
    const double maximum=1.35*(z->mBoard->GetColumnCount()-1)-4.8+4;
    // Primary Board.Awake: [-10,last source MouseColumnX+4], inclusive.
    return std::isfinite(source)&&(source < -10 || source > maximum);
}
}
NativeNightZombie281State* EditNativeJackson291(Zombie* z,bool create)
{
    auto* context=GetNativeContext(z?z->mBoard:nullptr);const auto* data=GetNativeSourceZombieData(z);
    if(!context||!data||!FusionJacksonFamily367Kind(data->kind))return nullptr;
    const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);
    const bool ghost=z->mNetGhost||z->mApp->NetIsClient();
    // Ghost metadata keeps the opaque host token; this map uses the current
    // LOCAL full generation. Snapshot adoption has already bound both identities.
    if(!uid||!data->instanceToken||(ghost&&create)||(!ghost&&data->instanceToken!=uid))return nullptr;
    auto found=context->nightZombies281.find(uid);
    if(found==context->nightZombies281.end())
    {
        if(!create)return nullptr;
        found=context->nightZombies281.emplace(uid,NativeNightZombie281State{}).first;
    }
    return &found->second;
}
const NativeNightZombie281State* GetNativeJackson291(const Zombie* z)
{return EditNativeJackson291(const_cast<Zombie*>(z),false);}
bool CanSpawnNativeJackson291(Board* b,std::string* error){return CanSpawnNativeJackson291(b,10,error);}
bool CanSpawnNativeJackson291(Board* b,uint16_t kind,std::string* error)
{
 if(error)error->clear();if(kNativeFusionSnapshotVersion<27||!FusionJacksonFamily367Kind(kind))return false;
 if(kind==10)return CanSpawnNativeGrassZombie284(b,10,error)&&ClockReady(kind,error);
 if(!EnsureNativeQuickJackson367Resources(b,error)||!ClockReady(kind,error))return false;
 const uint16_t parent[]={207};FusionSourceDeathBudget319 peak;
 return GetNativeSourceZombieBirthBudget319(b,parent,peak,error)&&CanAllocateNativeSourceBudget319(b,peak,error);
}
const NativeNightZombie281Hooks& GetNativeJackson291Hooks()
{
    static const NativeNightZombie281Hooks hooks=[]
    {
        NativeNightZombie281Hooks value;value.eveBalanced=OrdinaryFalse;value.travelDebuff13=OrdinaryFalse;
        value.prepareChildren=PrepareChildren;value.childEffect=PlayNativeNightChildEffect291;
        value.facingKeepShadow=FacingKeepShadow;value.contact=NativeJackson291Contact;value.bite=NativeJackson291Bite;
        value.sound=Sound;value.fallSound=FallSound;value.destroy=Destroy;
        value.outsideSourceBounds=OutsideBounds;value.dieWithoutAnimation=Destroy;
        value.finishMoonWalk=FinishNativeQuickJackson367MoonWalk;value.positionOverride=StepNativeQuickJackson367Position;return value;
    }();
    return hooks;
}
bool InitializeNativeJackson291(Zombie* z)
{
    if(kNativeFusionSnapshotVersion<27||!Authority(z)||!ClockReady(GetNativeSourceZombieData(z)->kind,nullptr))return false;
    auto* state=EditNativeJackson291(z,true);if(!state)return false;
    if(state->initialized)return !state->failed&&NativeNightZombie281StateValid(*state);
    // No repeated free-capacity demand AFTER this real actor/body was born.
    const bool ready=InitializeNativeJackson281(z,*state,GetNativeJackson291Hooks())&&InitializeNativeQuickJackson367Presentation(z);
    if(!ready){state->failed=true;Failed(z,"source initialization/body clock");}return ready;
}
bool UpdateNativeJackson291(Zombie* z)
{
    if(!Typed(z))return false;if(!Playing(z))return true;
    auto* state=EditNativeJackson291(z);if(!state){Failed(z,"missing full-generation controller");return true;}
    if(!UpdateNativeJackson281(z,*state,GetNativeJackson291Hooks())||state->failed)
    {state->failed=true;Failed(z,"source controller step");}return true;
}
bool UpdateNativeJackson291Animation(Zombie* z)
{
    if(!Typed(z))return false;
    if(Authority(z))if(const auto* state=GetNativeJackson291(z))UpdateNativeJackson281Animation(z,*state);
    return true;
}
bool StartNativeJackson291Walk(Zombie* z)
{return UpdateNativeJackson291Animation(z);} // Never select a generic stock track.
bool MoveNativeJackson291(Zombie* z)
{
    if(!Typed(z))return false;
    if(Playing(z))if(auto* state=EditNativeJackson291(z))MoveNativeJackson281(z,*state);
    return true; // The Core update already applied source root deltas.
}
bool BeginNativeJackson291Death(Zombie* z)
{
    if(!Typed(z))return false;if(!Authority(z))return true;
    auto* state=EditNativeJackson291(z);
    if(!state||!BeginNativeJackson281Death(z,*state,GetNativeJackson291Hooks())||state->failed)
        Failed(z,"source death transition before stock reset");
    ClearNativeJackson291Target(z);
    return true;
}
bool GetNativeJackson291Presentation(const Zombie* z,const char*& state,float& phase)
{
    const auto* controller=GetNativeJackson291(z);
    return controller&&GetNativeJackson281Presentation(z,*controller,state,phase);
}
}
