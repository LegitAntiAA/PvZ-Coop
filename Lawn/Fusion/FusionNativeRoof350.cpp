#include "FusionNativeRoof350.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativePlacement.h"
#include "FusionNativeVisual.h"
#include "FusionNativeBatchActions.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <array>
#include <cmath>

namespace LawnFusion
{
namespace
{
bool Reject(std::string* error,const char* text){if(error)*error=text;return false;}
bool Boot(Board* b,const FusionSourceAdventure272& level,std::string* error)
{
    const auto* c=GetNativeContext(b);
    return b&&b->mApp&&b->mApp->mBoard==b&&!b->mApp->NetIsClient()&&c&&c->adventure.enabled&&
        b->mApp->IsFusionSourceAdventure()&&c->adventure.run.level==level.level&&
        FindFusionSourceAdventure272(level.level)==&level&&level.scene==BACKGROUND_5_ROOF&&
        level.rows==5&&level.columns==10&&level.roofPreplantColumns==(level.level==37?5:3)&&
        !c->adventure.wave&&!c->adventure.run.over&&!c->adventureScheduler.initialized&&
        !b->mPlants.mSize&&!b->mZombies.mSize&&IsNativeSourceWorld313(b)||
        Reject(error,"Source Roof preplants require their fresh authoritative bootstrap world.");
}
bool Definition(std::string* error)
{
    if(GetNativeSeedForPlant(27)!=SEED_FLOWERPOT)return Reject(error,"Source Pot27 native carrier differs.");
    const auto type=GetPlantDefinition(SEED_FLOWERPOT).mReanimationType;
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return Reject(error,"Native FlowerPot definition unavailable.");
    const auto& tracks=gReanimatorDefArray[type].mTracks;
    if(!tracks.tracks||tracks.count<=0)return Reject(error,"Native FlowerPot has no real body tracks.");
    for(int i=0;i<tracks.count;++i)
        if(tracks.tracks[i].mName&&_strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)
            return Reject(error,"Native FlowerPot has an unbudgeted automatic attacher.");
    return true;
}
}
bool PreflightNativeRoofPreplants350(Board* b,const FusionSourceAdventure272& level,std::string* error)
{
    if(error)error->clear();if(!Boot(b,level,error)||!Definition(error))return false;
    const auto* stats=FusionFindPlant(27);
    const uint16_t kinds[]={27};
    if(!stats||!stats->hasStats||stats->maxHealth<=0||!EnsureNativeVisuals(b,kinds,{},error))return false;
    const auto count=uint64_t(level.roofPreplantColumns)*level.rows;
    if(!b->mPlants.mBlock||b->mPlants.mSize>b->mPlants.mMaxSize||count>uint64_t(b->mPlants.mMaxSize-b->mPlants.mSize))
        return Reject(error,"Source Roof whole fresh Plant peak unavailable; no removal credit.");
    const uint64_t nativeIds=uint64_t(b->mPlants.mSize)+b->mZombies.mSize+b->mProjectiles.mSize+b->mCoins.mSize+
        b->mGridItems.mSize+b->mLawnMowers.mSize+SEEDBANK_CAPACITY;
    if(nativeIds+count>65535)return Reject(error,"Source Roof whole native entity-ID peak unavailable.");
    // Actual native FLOWERPOT initializer: one bodyR, zero head/sleep/attacher.
    // Roof ceramic planting effect allocates no native particle system.
    FusionSourceDeathBudget319 peak;peak.reanimations=count;
    if(!CanAllocateNativeSourceBudget319(b,peak,error))return false;
    for(int col=0;col<level.roofPreplantColumns;++col)for(int row=0;row<level.rows;++row)
    {
        Plant* target=nullptr;
        if(!ResolveNativeFusionPlacementTarget(b,col,row,27,target,true)||target)
            return Reject(error,"Source Roof Pot27 authored placement is refused; no forced duplicate.");
    }
    return true;
}
bool InitializeNativeRoofPreplants350(Board* b,const FusionSourceAdventure272& level,std::string* error)
{
    if(!PreflightNativeRoofPreplants350(b,level,error))return false;
    std::array<Plant*,25> created{};size_t count=0;
    for(int col=0;col<level.roofPreplantColumns;++col)for(int row=0;row<level.rows;++row)
    {
        Plant* target=nullptr;
        if(!ResolveNativeFusionPlacementTarget(b,col,row,27,target,true)||target)goto failed;
        {
            NativePlantData fresh;fresh.kind=27;fresh.owner=LawnNet::NET_SHARED_OWNER;
            auto* p=b->AddPlant(col,row,SEED_FLOWERPOT,SEED_NONE,&fresh);
            if(p)created[count++]=p;
            const auto* d=GetNativePlantData(p);const auto* stats=FusionFindPlant(27);
            if(!p||p->mDead||!p->mNetId||!d||d->kind!=27||d->owner!=LawnNet::NET_SHARED_OWNER||
               d->packetId||d->randomOrigin||d->firstParent!=kFusionNoPlant||d->secondParent!=kFusionNoPlant||
               d->instanceToken!=b->mPlants.DataArrayGetID(p)||p->mPlantHealth!=stats->maxHealth||p->mPlantMaxHealth!=stats->maxHealth)
                goto failed;
            NativeBatchPlantPlaced(p);
        }
    }
    RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion Roof350 source preplants committed level=%u columns=%u rows=%u plants=%zu kind=27 peakR=%zu peakA=0",level.level,level.roofPreplantColumns,level.rows,count,count);
    return true;
failed:
    for(size_t i=0;i<count;++i)if(created[i]&&!created[i]->mDead)created[i]->Die(255);
    RebuildNativeEquipmentGraph(b);
    return Reject(error,"Source Roof factory invariant failed after preflight; fresh batch retired255, bootstrap stopped.");
}
bool ReconcileNativeRoofZombie350(Zombie* z)
{
    if(!z||!z->mApp||z->mApp->NetIsClient()||z->mNetGhost||!GetNativeSourceZombieData(z))return false;
    if(!z->mBoard->StageHasRoof())return true;
    float y=0;if(!GetNativeFusionRoofZombieRowY350(z,z->mPosX,z->mRow,y))return false;
    z->mPosY=y;z->mY=int(y);return true;
}
}
