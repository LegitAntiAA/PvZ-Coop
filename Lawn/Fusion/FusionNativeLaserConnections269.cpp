#include "FusionNativeLaserConnections269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNative.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Host(Board* b)
{return b&&b->mApp&&!b->mApp->NetIsClient()&&IsNativeFusionBoard(b)&&GetNativeContext(b);}
bool LiveLaser(const Plant* p)
{
    const auto* d=GetNativePlantData(p);if(!p||p->mDead||!p->mIsOnBoard||!d||d->kind!=937||!d->instanceToken||!p->mNetId||!GetNativeWorld269PlantState(p))return false;
    const auto* context=GetNativeContext(p->mBoard);if(!context)return false;
    for(const auto& hold:context->gloveHolds)if(hold.plantId==p->mNetId&&hold.instanceToken==d->instanceToken)return false;
    return true;
}
Plant* Resolve(Board* b,FusionWorld269Reference ref)
{
    if(!b||!ref.id||!ref.token)return nullptr;Plant* p=nullptr;
    while(b->IteratePlants(p))if(LiveLaser(p)&&p->mNetId==ref.id&&GetNativePlantData(p)->instanceToken==ref.token)return p;
    return nullptr;
}
}
bool RebuildNativeLaserConnections269Column(Board* b,int column)
{
    if(!Host(b)||column<0||!b->IsValidGridCell(column,0))return false;
    const int rows=GetNativeFusionRowCount(b);std::vector<Plant*> columnPlants;
    Plant* p=nullptr;while(b->IteratePlants(p))if(LiveLaser(p)&&p->mPlantCol==column)columnPlants.push_back(p);
    // Keep the native Board list order within a cell. Source SameLower takes
    // the first same-type entry in BoxInfo.plants, not a viewer-relative ID.
    for(auto* member:columnPlants)
        if(GetNativeWorld269PlantState(member)->connected)DisconnectNativeLaser269(member);
    bool complete=true;
    for(int row=0;row+1<rows;++row)
        for(auto* upper:columnPlants)
        {
            if(upper->mRow!=row||!LiveLaser(upper)||GetNativeWorld269PlantState(upper)->connected)continue;
            const auto lower=std::find_if(columnPlants.begin(),columnPlants.end(),[&](const Plant* candidate){return candidate->mRow==row+1&&LiveLaser(candidate);});
            if(lower!=columnPlants.end()&&!ConnectNativeLaser269(upper,*lower))complete=false;
        }
    return complete;
}
void NativeLaserConnections269PlantPlaced(Plant* p)
{if(p&&Host(p->mBoard))RebuildNativeLaserConnections269Column(p->mBoard,p->mPlantCol);}
void NativeLaserConnections269PlantRemoving(Plant* p)
{if(p&&Host(p->mBoard))RebuildNativeLaserConnections269Column(p->mBoard,p->mPlantCol);}
void NativeLaserConnections269PlantMoved(Plant* p,int previousColumn)
{
    if(!p||!Host(p->mBoard))return;
    RebuildNativeLaserConnections269Column(p->mBoard,previousColumn);
    if(p->mPlantCol!=previousColumn)RebuildNativeLaserConnections269Column(p->mBoard,p->mPlantCol);
}
bool GetNativeLaserConnections269BallPoint(const Plant* p,float& x,float& y,int& sortingRow)
{
    if(!LiveLaser(p)||!IsNativeFusionBoard(p->mBoard))return false;
    const auto* state=GetNativeWorld269PlantState(p);if(!state->connected)return false;
    auto* other=Resolve(p->mBoard,state->connectedPlant);if(!other)return false;
    const auto* peer=GetNativeWorld269PlantState(other);const auto* identity=GetNativePlantData(p);
    if(!peer->connected||peer->connectedPlant.id!=p->mNetId||peer->connectedPlant.token!=identity->instanceToken)return false;
    float ax=0,ay=0,bx=0,by=0;if(!const_cast<Plant*>(p)->GetShadowCenter(ax,ay)||!other->GetShadowCenter(bx,by))return false;
    x=(ax+bx)*.5f-float(.4*kFusionBasePixelsX);
    y=(ay+by)*.5f-float(GetNativeFusionPixelsY(p->mBoard));sortingRow=(std::max)(p->mRow,other->mRow);
    return true;
}
}
