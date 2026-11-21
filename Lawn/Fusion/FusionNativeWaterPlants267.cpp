#include "FusionNativeWaterPlants267.h"
#include "FusionWaterPlants267Rules.h"
#include "FusionNative.h"
#include "FusionNativePlacement.h"
#include "FusionDefenderRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{const auto* d=GetNativePlantData(p);return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&d&&d->kind==12&&p->mIsOnBoard;}
bool Host(const Plant* p)
{return Current(p)&&!p->mDead&&!p->mNetGhost&&!p->mApp->NetIsClient()&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
bool Alive(Plant* p){return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
bool Flying(uint16_t kind){return kind==241||kind==912||kind==1082;}
bool Support(uint16_t kind)
{const auto* profile=FindFusionDefenderProfile(kind);return kind==12||(profile&&profile->layer==FusionDefenderLayer::Under);}
}
bool InitializeNativeWater267(Plant* p)
{
    if(!Host(p))return false;auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(12);if(!definition||!definition->hasStats)return false;
    p->mPlantHealth=p->mPlantMaxHealth=definition->maxHealth;p->mLaunchCounter=kFusionLily267CycleTicks;p->mDoSpecialCountdown=0;p->mShootingCounter=0;
    d->shootSeconds=-1;d->animationSpeed=1;d->batchMode=0;d->batchShotCount=0;d->batchRageTicks=0;d->batchBagCount=65535;d->productionTicks=0;d->pendingProductionTicks=0;return true;
}
bool SetNativeLilyType267(Plant* p,uint16_t kind)
{
    if(!Host(p)||(kind!=65535&&!FusionLily267CanMimic(kind)))return false;
    GetNativePlantData(p)->batchBagCount=kind;return true;
}
bool UpdateNativeWater267(Plant* p)
{
    if(!Current(p))return false;if(!Host(p))return true;auto* d=GetNativePlantData(p);bool cover=false;Plant* other=nullptr;
    while(p->mBoard->IteratePlants(other))
    {
        const auto* od=GetNativePlantData(other);if(other==p||!Alive(other)||!od||other->mRow!=p->mRow)continue;
        // Source checks the left cell for a double-box plant covering this cell.
        if(p->mPlantCol>0&&other->mPlantCol==p->mPlantCol-1&&FusionLily267DoubleBoxKind(od->kind)){cover=true;continue;}
        if(other->mPlantCol!=p->mPlantCol||Flying(od->kind)||Support(od->kind))continue;
        cover=true;if(d->batchBagCount!=od->kind&&FusionLily267CanMimic(od->kind))SetNativeLilyType267(p,od->kind);
    }
    d->batchMode=cover?1:0;
    if(d->batchBagCount==65535)return true;
    if(--p->mLaunchCounter<=0)
    {
        p->mLaunchCounter=kFusionLily267CycleTicks;
        NativePlantData gift;gift.kind=d->batchBagCount;gift.owner=LawnNet::NET_SHARED_OWNER;
        auto* child=TrySpawnNativeFusionPlant(p->mBoard,p->mPlantCol,p->mRow,gift);
        LawnNet::NetTrace("Fusion native Lily267 mimic plant=%u kind=%u child=%u tick=%d",p->mNetId,gift.kind,child?child->mNetId:0,p->mBoard->mMainCounter);
    }
    return true;
}
bool UpdateNativeWater267Phase(Plant* p)
{if(!Current(p))return false;if(Host(p))GetNativePlantData(p)->idleSeconds+=.01f;return true;}
bool NativeLily267Covered(const Plant* p)
{return Current(p)&&GetNativePlantData(p)->batchMode==1;}
}
