#include "FusionNativeRollingActors269.h"
#include "FusionNative269RemainderCallbacks.h"
#include "FusionNative.h"
#include "FusionNativePlacement.h"
#include "FusionNativeVisual.h"
#include "FusionEquipmentRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Host(const Plant* p){return p&&Host(p->mBoard)&&!p->mNetGhost;}
bool Present(const Plant* p)
{return p&&!p->mDead&&p->mIsOnBoard&&GetNativePlantData(p);}
template<class T>T* FindState(std::unordered_map<uint32_t,T>& states,uint32_t generation,bool create)
{
    if(!generation)return nullptr;auto found=states.find(generation);
    if(found!=states.end())return &found->second;
    return create?&states.try_emplace(generation).first->second:nullptr;
}
bool GraphHasColdImmunity(Plant* source)
{
    std::vector<Plant*> nodes;Plant* p=nullptr;
    while(source->mBoard->IteratePlants(p))
        if(Present(p)&&FusionEquipmentMagnetMember(GetNativePlantData(p)->kind))nodes.push_back(p);
    const auto at=std::find(nodes.begin(),nodes.end(),source);if(at==nodes.end())return false;
    std::vector<bool> connected(nodes.size());const size_t first=size_t(at-nodes.begin());connected[first]=true;
    std::vector<size_t> pending{first};for(size_t next=0;next<pending.size();++next)
    {
        const size_t i=pending[next];const auto kind=GetNativePlantData(nodes[i])->kind;if(kind==935||kind==1098)return true;
        for(size_t j=0;j<nodes.size();++j)
            if(!connected[j]&&FusionEquipmentGraphAdjacent(nodes[i]->mPlantCol,nodes[i]->mRow,nodes[j]->mPlantCol,nodes[j]->mRow))
            {connected[j]=true;pending.push_back(j);}
    }
    return false;
}
bool ColdImmunity(Plant* victim)
{
    // Source Plant.TakeDamage468FB0 checks ALL actual same-cell layers before
    // virtual Instead (BigWallNut.Instead35B050 always returns false).
    Plant* p=nullptr;while(victim->mBoard->IteratePlants(p))
    {
        if(!Present(p)||p->mPlantCol!=victim->mPlantCol||p->mRow!=victim->mRow)continue;
        const auto kind=GetNativePlantData(p)->kind;
        if(kind==935||kind==1098||kind==1169)return true;
        if((kind==911||kind==922)&&GraphHasColdImmunity(p))return true;
    }
    return false;
}
}
NativeRollingActor269State* EditNativeRollingActor269State(Plant* p,bool create)
{
    auto* context=GetNativeContext(p?p->mBoard:nullptr);const auto* data=GetNativePlantData(p);
    if(!context||!data||!FindFusionRollingActor269Profile(data->kind))return nullptr;
    auto* state=FindState(context->rollingPlants269,p->mBoard->mPlants.DataArrayGetID(p),create&&Host(p)&&!p->mDead);
    if(state&&state->instanceToken&&state->instanceToken!=data->instanceToken)return nullptr;
    return state;
}
const NativeRollingActor269State* GetNativeRollingActor269State(const Plant* p)
{return EditNativeRollingActor269State(const_cast<Plant*>(p));}
NativeWheat269State* EditNativeWheat269State(Plant* p,bool create)
{
    auto* context=GetNativeContext(p?p->mBoard:nullptr);
    if(!context||!GetNativePlantData(p))return nullptr;
    return FindState(context->wheatPlants269,p->mBoard->mPlants.DataArrayGetID(p),create&&Host(p)&&!p->mDead);
}
const NativeWheat269State* GetNativeWheat269State(const Plant* p)
{return EditNativeWheat269State(const_cast<Plant*>(p));}
bool CanCreateNativeRollingActor269(Board* b,uint16_t factory)
{
    if(!Host(b)||!CanApplyNativeFusionGameplay(b)||!FindFusionRollingActor269Profile(factory)||
       b->mPlants.mSize>=b->mPlants.mMaxSize||GetNativeSeedForPlant(255)!=SEED_WALLNUT)return false;
    // Only the existing native Day/Pool/Fog foundation is admitted. Source
    // RoofEntity cannot be approximated by a terrain offset on a flat board.
    if(!NativeFusionBackgroundValid(static_cast<uint8_t>(b->mBackground)))return false;
    return EnsureNativePlantVisuals(b,factory)&&EnsureNativePlantVisuals(b,255);
}
Plant* CreateNativeRollingActor269Plant(Board* b,uint16_t factory,int column,int row)
{
    if(!CanCreateNativeRollingActor269(b,factory)||column<0||column>=b->GetColumnCount()||row<0||row>=GetNativeFusionRowCount(b))return nullptr;
    Plant* target=nullptr;
    if(!ResolveNativeFusionPlacementTarget(b,column,row,255,target,true)||target)return nullptr;
    NativePlantData fresh;fresh.kind=255;fresh.owner=LawnNet::NET_SHARED_OWNER;
    // One real native body, with authoritative NetID/typed data installed by
    // Board.AddPlant before OnCreate. No fake parents or copied packet lease.
    auto* p=b->AddPlant(column,row,SEED_WALLNUT,SEED_NONE,&fresh);if(!p)return nullptr;
    auto* data=GetNativePlantData(p);
    if(!data||data->kind!=255||!data->instanceToken||!p->mNetId||
       (!IsNativeRollingActor269(p)&&!InitializeNativeRollingActor269(p,255)))
    {p->Die(255);return nullptr;}
    // Source prefab251 is BigSunNut but its constructor promotes type255.
    // Its actor already initialized once; selecting its artwork consumes no RNG.
    auto* state=EditNativeRollingActor269State(p);if(!state){p->Die(255);return nullptr;}
    state->factoryKind=factory;
    LawnNet::NetTrace("Fusion RollingActor269 factory requested=%u physical=255 id=%u token=%u worldShared=1",factory,p->mNetId,data->instanceToken);
    return p;
}
void NativeRollingActor269Started(Plant* p)
{
    if(!Host(p))return;
    p->mApp->PlaySample(Sexy::SOUND_BOWLING);
    LawnNet::NetTrace("Fusion RollingActor269 audio id=%u nativeCompatibility=bowling source53VolumePitchNotClaimed=1",p->mNetId);
}
void NativeRollingActor269Entered(Plant* p)
{
    if(!Host(p))return;
    p->mApp->PlayFoley(FOLEY_BOWLINGIMPACT);
    LawnNet::NetTrace("Fusion RollingActor269 impact id=%u nativeCompatibility=bowlingImpact source54_55VolumePitchAndShakeNotClaimed=1",p->mNetId);
}
void NativeRollingActor269Achievement(Board* b,int achievement)
{
    if(Host(b))LawnNet::NetTrace("Fusion RollingActor269 sourceAchievement=%d NotAvailable nativeAchievementNotInvented=1",achievement);
}
void DestroyNativeRollingActor269Plant(Plant* p)
{
    if(!Host(p)||p->mDead||!IsNativeRollingActor269(p))return;
    p->Die(255); // Explicit direct destroy; Root Die hook permits internal255.
}
bool ResolveNativeRollingActor269IncomingDamage(Plant* p,int input,int type,int& actual)
{
    if(!Host(p)||!CanApplyNativeFusionGameplay(p->mBoard)||!Present(p)||!IsNativeRollingActor269(p))return false;
    if(type==2&&ColdImmunity(p)){actual=0;return true;}
    // Exact admitted normalD2: source Advanced33/isConnected and timers[1]
    // are globally inactive (same Native269World trait gates). Future Travel
    // half/triple damage must use the common source GetDamage adapter here.
    actual=(std::max)(1,input);return true;
}
bool NativeWheat269SpecialMode(const Board*)
{return false;} // WheatProtection/SuperRandom board modes are not yet ported.
bool NativeWheat269CanAdvance(const Plant* p)
{
    if(!Host(p)||!CanApplyNativeFusionGameplay(p->mBoard)||!Present(p))return false;
    // Source PlantUpdate checks Board.enermyCount, not source theWave.
    // Normal-D2 native pause/time-stop/animation guards retain the same gate.
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&GetNativeFusionZombieHealth(z)>0)return true;
    return false;
}
bool NativeWheat269ReplacementReady(const Board* b)
{return NativeEve269PoolReady(const_cast<Board*>(b));}
Plant* NativeWheat269Replace(Plant* p)
{
    bool committed=false;
    return ReplaceNativeEve269Plant(p,committed); // Shared transaction already marks a real offspring once.
}
void NativeWheat269ApplyMaterial(Plant* p)
{
    if(Host(p))LawnNet::NetTrace("Fusion Wheat269 material NotAvailable id=%u sourceWheatColorModeGated=1",p->mNetId);
}
}
