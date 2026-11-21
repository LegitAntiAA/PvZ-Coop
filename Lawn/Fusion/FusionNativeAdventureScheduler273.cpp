#include "FusionNativeAdventureScheduler273.h"
#include "FusionNativeAdventure273.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionSourceZombieClosure280.h"
#include "FusionCampaign272Rules.h"
#include "FusionNativeZombieVisual.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace LawnFusion
{
namespace
{
using Result=FusionAdventureScheduler273Result;
struct Adapter
{
    Board* board;
    std::string* error;
    std::vector<uint16_t> prepared;
};
bool Reject(Adapter& a,const char* message)
{if(a.error)*a.error=message;return false;}
bool Current(Board* b)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||!IsNativeFusionBoard(b)||
        b->mBackground!=BACKGROUND_1_DAY||b->GetColumnCount()!=10||GetNativeFusionRowCount(b)!=5)return false;
    for(int row=0;row<5;++row)if(b->mPlantRow[row]!=PLANTROW_NORMAL)return false;
    return true;
}
bool Ready(void* user,uint16_t kind)
{
    auto& a=*static_cast<Adapter*>(user);
    return std::find(a.prepared.begin(),a.prepared.end(),kind)!=a.prepared.end()||
        Reject(a,"Source scheduler kind is absent from its prepared descriptor dependency closure.");
}
bool Health(void* user,float& output)
{
    auto& a=*static_cast<Adapter*>(user);std::vector<FusionAdventureScheduler273Health> actors;
    Zombie* z=nullptr;
    // Source zombieArray keeps dying/non-destroyed records. Native mDead is
    // not the DataArray destruction boundary and must not filter the cache.
    while(a.board->mZombies.IterateNext(z))if(!z->mMindControlled)
    {
        const auto* source=GetNativeSourceZombieData(z);
        if(!source)return Reject(a,"Untyped enemy leaked into source Adventure health cache.");
        actors.push_back({source->health,z->mHelmHealth,z->mShieldHealth,false});
    }
    return TrySumFusionAdventureHealth273(actors,output);
}
bool CanSpawn(void* user,std::span<const uint16_t> kinds)
{
    auto& a=*static_cast<Adapter*>(user);auto* b=a.board;
    if(!Current(b)||b->mApp->NetIsClient()||b->mPaused||b->mApp->mGameScene!=SCENE_PLAYING)
        return Reject(a,"Source scheduler cannot spawn outside its active host Board.");
    return CanAllocateNativeSourceZombieBatch280(b,kinds,a.error);
}
bool Spawn(void* user,int wave,std::span<const FusionAdventureScheduler273Spawn> batch,float& output)
{
    auto& a=*static_cast<Adapter*>(user);auto* b=a.board;
    // Source SetZombie sentinel9.9 => last cell's source X +2.6. Existing Board
    // cell spacing projects the recovered source1.35 world-unit column step.
    const float sourceRootX=float(b->GetCellCenterX(b->GetColumnCount()-1,0))+
        2.6f*(float(b->GetCellPitchX())/1.35f);
    if(!std::isfinite(sourceRootX))return Reject(a,"Source scheduler spawn projection is invalid.");
    std::vector<FusionAdventureScheduler273Health> health;health.reserve(batch.size());
    for(const auto& spawn:batch)
    {
        auto* z=SpawnNativeSourceZombie(b,spawn.kind,spawn.row,wave);
        const auto* source=GetNativeSourceZombieData(z);float rootX=0,rootY=0;
        if(!z||!source||source->kind!=spawn.kind||!z->mNetId||!source->instanceToken||
            !GetNativeSourceZombie269NodePoint(z,"",rootX,rootY))
            return Reject(a,"Source scheduler actual native source actor/root initialization failed; batch must not retry.");
        // Birth-only translation of the actual posed source ROOT. A shadow
        // anchor has a nonzero source offset and must not be mistaken for root.
        z->mPosX+=sourceRootX-rootX;z->mX=int(z->mPosX);
        health.push_back({source->health,z->mHelmHealth,z->mShieldHealth,false});
    }
    if(!TrySumFusionAdventureBirthHealth273(health,output))return Reject(a,"Source scheduler actual birth health is invalid.");
    LawnNet::NetTrace("Fusion source Adventure273 wave=%d count=%zu birthHP=%.9g rootX=%.9g sourceRows=1 nativeSlotAdaptation=0",
        wave,batch.size(),output,sourceRootX);
    return true;
}
}
Result InitializeNativeAdventureScheduler273(Board* board,FusionAdventureScheduler273State& state,
    int level,int cycle,int difficulty,const FusionAdventureScheduler273Random& random,std::string* error)
{
    if(error)error->clear();
    if(!board||!board->mApp||board->mApp->NetIsClient())return Result::NotHost;
    Adapter adapter{board,error};
    if(!Current(board)||!NativeSourceAdventureLevelReady273(level,uint32_t(cycle)))
    {Reject(adapter,"Source scheduler requires the shared admitted ordinary Day campaign policy.");return Result::NotReady;}
    const auto* config=FindFusionSourceAdventure272(level);
    if(!config){Reject(adapter,"Source scheduler descriptor is unavailable.");return Result::NotReady;}
    std::vector<uint16_t> roots(config->allowedZombieKinds.begin(),config->allowedZombieKinds.end());
    roots.push_back(0); // Exact source budget fallback, independently of the allow-list.
    if(!BuildFusionSourceZombieClosure280(roots,adapter.prepared,error)||
        !EnsureNativeSourceZombieClosure280(board,adapter.prepared,error))return Result::NotReady;
    return InitializeFusionAdventureScheduler273(state,true,level,cycle,difficulty,random,&adapter,Ready);
}
Result UpdateNativeAdventureScheduler273(Board* board,FusionAdventureScheduler273State& state,
    const FusionAdventureScheduler273Input& input,const FusionAdventureScheduler273Random& random,
    FusionAdventureScheduler273Events& events,std::string* error)
{
    events={};if(error)error->clear();
    if(!board||!board->mApp||board->mApp->NetIsClient())return Result::NotHost;
    Adapter adapter{board,error};
    if(!Current(board)||!NativeSourceAdventureLevelReady273(state.level,uint32_t(state.cycle)))
    {Reject(adapter,"Source scheduler lost its admitted native Day Board.");return Result::NotReady;}
    auto effective=input;effective.host=input.host&&!board->mApp->NetIsClient();
    effective.paused=input.paused||board->mPaused||board->mApp->mGameScene!=SCENE_PLAYING;
    FusionAdventureScheduler273Callbacks callbacks;
    callbacks.user=&adapter;callbacks.random=random;callbacks.health=Health;
    callbacks.canSpawn=CanSpawn;callbacks.spawn=Spawn;
    return UpdateFusionAdventureScheduler273(state,effective,callbacks,events);
}
}
