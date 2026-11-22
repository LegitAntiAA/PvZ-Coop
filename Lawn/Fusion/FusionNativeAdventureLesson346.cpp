#include "FusionNativeAdventureLesson346.h"
#include "FusionNativeAdventure273.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeVisual.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeGlove.h"
#include "FusionNativePlacement.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace LawnFusion
{
namespace
{
using K=FusionLesson346OpKind;
bool Reject(std::string* e,const char* text){if(e)*e=text;return false;}
bool World(Board* b,int level,std::string* error)
{
    const auto* c=GetNativeContext(b);const auto* source=FindFusionSourceAdventure272(level);
    return b&&b->mApp&&b->mApp->mBoard==b&&!b->mApp->NetIsClient()&&c&&source&&
        c->adventure.enabled&&c->adventure.run.level==level&&c->adventure.run.cycle==1&&
        source->tutorialId==level&&int(b->mBackground)==source->scene&&IsNativeSourceWorld313(b)||
        Reject(error,"Source lesson requires its actual cycle1 source scene/terrain capability.");
}
bool Definition(ReanimationType type,std::string* error)
{
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return Reject(error,"Source lesson native definition unavailable.");
    const auto& t=gReanimatorDefArray[int(type)].mTracks;
    if(!t.tracks||t.count<=0)return false;
    for(int i=0;i<t.count;++i)if(t.tracks[i].mName&&_strnicmp(t.tracks[i].mName,"attacher__",10)==0)
        return Reject(error,"Source lesson initializer has an unbudgeted native attacher.");
    return true;
}
bool ParticleBudget(ParticleEffect type,FusionSourceDeathBudget319& budget,std::string* error)
{
    if(!gParticleDefArray||unsigned(type)>=unsigned(gParticleDefCount))return Reject(error,"Source lesson native particle definition unavailable.");
    const auto& def=gParticleDefArray[type];if(def.mEmitterDefCount<0||(def.mEmitterDefCount&&!def.mEmitterDefs))return false;
    ++budget.particleSystems;budget.emitters+=unsigned(def.mEmitterDefCount);return true;
}
bool Peak(Board* b,std::span<const FusionLesson346Op> ops,std::string* error)
{
    FusionSourceDeathBudget319 peak;size_t plants=0;std::vector<uint16_t> kinds,zombies;
    int64_t money=GetNativeContext(b)->money;
    std::vector<std::pair<int,int>> cleared;
    for(const auto& op:ops)
    {
        if(op.operation==K::Clear)cleared.emplace_back(op.column,op.row);
        if(op.operation==K::Money){money+=op.value;if(op.value<0||money>(std::numeric_limits<int>::max)())return Reject(error,"Source lesson wallet overflow.");}
        if(op.operation==K::Clear||op.operation==K::Plant)
            if(!b->IsValidGridCell(op.column,op.row))return Reject(error,"Source lesson authored cell is outside the actual Board.");
        if(op.operation==K::Zombie){zombies.push_back(op.kind);continue;}
        if(op.operation!=K::Plant)continue;
        if(std::find(cleared.begin(),cleared.end(),std::pair{op.column,op.row})==cleared.end())
        {
            Plant* target=nullptr;
            if(ResolveNativeFusionPlacementTarget(b,op.column,op.row,op.kind,target,true)&&target)
                return Reject(error,"Source lesson live mixing callback needs whole peak admission before mutation.");
        }
        const auto* stats=FusionFindPlant(op.kind);const auto seed=GetNativeSeedForPlant(op.kind);
        if(!stats||!stats->hasStats||stats->maxHealth<=0||seed==SEED_NONE)return Reject(error,"Source lesson plant has no real native factory/stats.");
        const auto type=GetPlantDefinition(seed).mReanimationType;if(type==REANIM_NONE||!Definition(type,error))return false;
        ++plants;++peak.reanimations;kinds.push_back(op.kind);
        // Exact native PlantInitialize cases for this source-authored cohort.
        switch(seed)
        {
        case SEED_PEASHOOTER:case SEED_SNOWPEA:case SEED_REPEATER:case SEED_GATLINGPEA:
            ++peak.reanimations;++peak.attachments;break;
        case SEED_SPLITPEA:peak.reanimations+=2;peak.attachments+=2;break;
        case SEED_TANGLEKELP:case SEED_JALAPENO:case SEED_SPIKEWEED:case SEED_PLANTERN:
        case SEED_CACTUS:case SEED_BLOVER:case SEED_MAGNETSHROOM:case SEED_STARFRUIT:
        case SEED_FLOWERPOT:case SEED_MARIGOLD:case SEED_CABBAGEPULT:case SEED_KERNELPULT:break;
        default:return Reject(error,"Source lesson native initializer is outside its audited factory cohort.");
        }
        if(seed==SEED_MAGNETSHROOM&&b->mBackground!=BACKGROUND_2_NIGHT&&b->mBackground!=BACKGROUND_4_FOG)
        {if(!Definition(REANIM_SLEEPING,error))return false;++peak.reanimations;}
        if(seed==SEED_PLANTERN)
        {++peak.attachments;if(!ParticleBudget(PARTICLE_LANTERN_SHINE,peak,error))return false;}
        if(!(seed==SEED_FLOWERPOT&&b->StageHasRoof()))
            if(!ParticleBudget(b->IsPoolSquare(op.column,op.row)?PARTICLE_PLANTING_POOL:PARTICLE_PLANTING,peak,error))return false;
    }
    if(!EnsureNativeVisuals(b,kinds,{},error))return false;
    if(!zombies.empty())
    {
        if(!EnsureNativeSourceZombieClosure280(b,zombies,error))return false;
        FusionSourceDeathBudget319 own;if(!GetNativeSourceZombieBirthBudget319(b,zombies,own,error)||!AddFusionSourceDeathBudget319(peak,own,peak))return false;
    }
    const auto& pool=b->mPlants;
    if(!pool.mBlock||pool.mSize>pool.mMaxSize||plants>size_t(pool.mMaxSize-pool.mSize))return Reject(error,"Source lesson whole fresh Plant peak unavailable; no removal credit.");
    return CanAllocateNativeSourceBudget319(b,peak,error);
}
bool SafeRemoval(uint16_t kind)
{
    // Source removal-only bodies plus these demonstrated light/magnet/money
    // kinds. Kelp1051 is excluded: Die0 can kill its real captured target.
    switch(kind)
    {
    case 0:case 1:case 2:case 3:case 20:case 22:case 25:case 27:case 31:
    case 1000:case 1030:case 1032:case 1033:case 1079:case 1101:case 1103:
    case 1108:case 1134:case 1135:case 1137:case 1139:return true;
    default:return false;
    }
}
bool CollectOld(Board* b,std::span<const FusionLesson346Op> ops,std::vector<uint32_t>& out,std::string* error)
{
    for(const auto& cell:ops)if(cell.operation==K::Clear)
    {
        Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard&&p->mRow==cell.row&&
            (p->mPlantCol==cell.column||(p->mSeedType==SEED_COBCANNON&&p->mPlantCol+1==cell.column)))
        {
            const auto* d=GetNativePlantData(p);const auto uid=b->mPlants.DataArrayGetID(p);
            if(!d||!uid||p->mNetGhost||d->instanceToken!=uid||!SafeRemoval(d->kind)||
               p->mSeedType!=GetNativeSeedForPlant(d->kind)||IsNativeGlovePlantReserved(b,p,-1))
                return Reject(error,"Source lesson cell has an unaudited Die0 callback or foreign/glove actor.");
            if(std::find(out.begin(),out.end(),uid)==out.end())out.push_back(uid);
        }
    }
    return true;
}
bool Stop(Board* b,std::string* e,const char* text)
{auto* c=GetNativeContext(b);c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;return Reject(e,text);}
}
bool PreflightNativeFusionAdventureLesson346(Board* b,int level,std::string* error)
{
    if(error)error->clear();const auto steps=FusionAdventureLesson346Steps(level);
    if(steps.empty()||!World(b,level,error))return false;
    for(const auto& step:steps)if(!step.operations.empty()&&!Peak(b,step.operations,error))return false;
    return true;
}
bool ExecuteNativeFusionAdventureLesson346(Board* b,int level,uint8_t stage,uint32_t& mission,std::string* error)
{
    const auto steps=FusionAdventureLesson346Steps(level);
    if(!World(b,level,error)||stage>=steps.size()||!CanApplyNativeFusionGameplay(b)||
       GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing)return false;
    const auto ops=steps[stage].operations;std::vector<uint32_t> old;
    if(!Peak(b,ops,error)||!CollectOld(b,ops,old,error))return false;
    for(const auto& op:ops)
    {
        if(op.operation==K::Clear)
        {
            for(auto uid:old)if(auto* p=b->mPlants.DataArrayTryToGet(uid);p&&!p->mDead&&p->mRow==op.row&&
                (p->mPlantCol==op.column||(p->mSeedType==SEED_COBCANNON&&p->mPlantCol+1==op.column)))p->Die(0);
        }
        else if(op.operation==K::Plant)
        {
            NativePlantData fresh;fresh.kind=op.kind;fresh.owner=LawnNet::NET_SHARED_OWNER;
            Plant* target=nullptr;
            if(!ResolveNativeFusionPlacementTarget(b,op.column,op.row,op.kind,target,true))continue;
            // Source SetPlant(isFreeSet=false,withEffect=true) returns null on
            // CheckBox/CheckMix refusal; these authored callers ignore it.
            if(target)return Stop(b,error,"Source lesson placement changed to an unbudgeted mix after preflight.");
            auto* p=TrySpawnNativeFusionPlant(b,op.column,op.row,fresh);
            const auto* d=GetNativePlantData(p);
            if(!p||p->mDead||!p->mNetId||!d||d->kind!=op.kind||d->instanceToken!=b->mPlants.DataArrayGetID(p)||
               d->owner!=LawnNet::NET_SHARED_OWNER||d->packetId||d->randomOrigin||
               d->firstParent!=kFusionNoPlant||d->secondParent!=kFusionNoPlant)
                return Stop(b,error,"Source lesson fresh default Plant factory failed after preflight.");
        }
        else if(op.operation==K::Zombie)
        {
            auto* z=SpawnNativeSourceZombie(b,op.kind,op.row,0);auto* d=GetNativeSourceZombieData(z);float x=0,y=0;
            if(!z||!d||d->kind!=op.kind||!z->mNetId||!d->instanceToken||
               !GetNativeSourceZombie269NodePoint(z,"",x,y))return Stop(b,error,"Source lesson actual Zombie/root factory failed after preflight.");
            const float target=op.sourceX==9.9f?float(b->GetCellCenterX(b->GetColumnCount()-1,op.row))+2.6f*float(kFusionBasePixelsX):
                float(b->GetCellCenterX(0,op.row))+(op.sourceX+4.8f)*float(kFusionBasePixelsX);
            z->mPosX+=target-x;z->mX=int(z->mPosX);
            if(op.value){z->mBodyHealth=z->mBodyMaxHealth=op.value;d->health=float(op.value);mission=b->mZombies.DataArrayGetID(z);}
        }
        else if(op.operation==K::Money)GetNativeContext(b)->money+=op.value;
        // ClampDifficulty has no second field/clock. Canonical next stage5
        // activates RuntimeDifficulty346 after this entire action commits.
    }
    RebuildNativeEquipmentGraph(b);return true;
}
void ObserveNativeFusionAdventureLesson346(Board* b,FusionAdventureLessonInput274& input)
{
    Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard)
        if(const auto* d=GetNativePlantData(p);d&&d->kind==1135&&d->instanceToken==b->mPlants.DataArrayGetID(p))input.hasGoldCabbage1135=true;
}
}
