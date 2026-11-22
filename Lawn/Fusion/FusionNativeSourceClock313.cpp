#include "FusionNativeSourceClock313.h"
#include "FusionNative.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeWaterZombie313Bridge.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../GridItem.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <utility>

namespace LawnFusion
{
namespace
{
ReanimationHolder* Holder(Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&b->mApp->mEffectSystem?b->mApp->mEffectSystem->mReanimationHolder:nullptr;}
uint32_t LiveClock(const Reanimation* r,ReanimationHolder* holder)
{
    if(!r||!holder||r->mReanimationHolder!=holder)return 0;
    const uint32_t uid=holder->mReanimations.DataArrayGetID(const_cast<Reanimation*>(r));
    return uid&&holder->mReanimations.DataArrayTryToGet(uid)==r?uid:0;
}
}
bool IsNativeSourceWorld313(const Board* b)
{
    const auto* c=GetNativeContext(b);
    if(!b||!b->mApp||b->mApp->mBoard!=b||!IsNativeFusionBoard(b)||!c||c->adventureTeardown||
       !c->epoch||c->epoch!=b->mApp->mNativeFusionEpoch||int(b->mBackground)<0||int(b->mBackground)>4||
       b->GetColumnCount()!=10||!b->mTDPath.empty())return false;
    const int rows=GetNativeFusionRowCount(b),expected=NativeFusionRowsForBackground(uint8_t(b->mBackground));
    if(rows!=expected)return false;
    for(int row=0;row<rows;++row)
    {
        const bool water=expected==6&&(row==2||row==3);
        if(b->mPlantRow[row]!=(water?PLANTROW_POOL:PLANTROW_NORMAL))return false;
        for(int col=0;col<10;++col)
            if(b->GetGridSquareTypeAt(col,row)!=(water?GRIDSQUARE_POOL:GRIDSQUARE_GRASS))return false;
    }
    GridItem* item=nullptr;
    while(const_cast<Board*>(b)->mGridItems.IterateNext(item))
        if(!item->mDead&&item->mGridItemType!=GRIDITEM_LADDER)return false;
    return true;
}
bool RegisterNativeSourceGraphClock313(Reanimation* r,bool owned)
{
    auto* b=gLawnApp?gLawnApp->mBoard:nullptr;auto* context=GetNativeContext(b);auto* holder=Holder(b);
    const uint32_t uid=LiveClock(r,holder);if(!context||!uid)return false;
    if(owned){if(r->mDead)return false;context->sourceGraphClocks313.insert(uid);}
    else context->sourceGraphClocks313.erase(uid);
    return true;
}
NativeSourceGraph313State* EditNativeSourceGraph313(Zombie* z,bool create)
{
    auto* b=z?z->mBoard:nullptr;auto* context=GetNativeContext(b);const auto* data=GetNativeSourceZombieData(z);
    if(!context||!data||!FindFusionSourceGraph313(data->kind))return nullptr;
    const uint32_t uid=b->mZombies.DataArrayGetID(z);
    if(!uid||b->mZombies.DataArrayTryToGet(uid)!=z)return nullptr;
    auto it=context->sourceGraphs313.find(uid);
    if(it==context->sourceGraphs313.end())
    {if(!create)return nullptr;it=context->sourceGraphs313.emplace(uid,NativeSourceGraph313State{}).first;}
    return &it->second;
}
const NativeSourceGraph313State* GetNativeSourceGraph313(const Zombie* z)
{return EditNativeSourceGraph313(const_cast<Zombie*>(z),false);}
NativeSourceZombieInitialization313::NativeSourceZombieInitialization313(Zombie* z)
{
    if(!z||!z->mApp||z->mApp->NetIsClient()||z->mNetGhost)return;
    auto* context=GetNativeContext(z->mBoard);const auto* data=GetNativeSourceZombieData(z);
    if(!context||!data||z->mApp->mBoard!=z->mBoard)return;
    const uint32_t local=z->mBoard->mZombies.DataArrayGetID(z);
    if(!local||local!=data->instanceToken)return;
    board=z->mBoard;app=z->mApp;uid=local;epoch=context->epoch;
    inserted=context->sourceZombieInitializations313.insert(uid).second;
}
NativeSourceZombieInitialization313::~NativeSourceZombieInitialization313()
{
    if(!inserted||gLawnApp!=app||app->mBoard!=board)return;
    auto* context=GetNativeContext(board);if(!context||context->epoch!=epoch)return;
    if(!committed)
    {
        // An unadmitted constructor is an abort, not a source Die event.
        // In particular, a failed/uninitialized controller cannot refuse the
        // cleanup and leave an unregistered real actor alive in the Board.
        if(auto* z=board->mZombies.DataArrayTryToGet(uid);z&&!z->mDead)
        {
            NativeJackbox309Removing(z);
            RemoveNativeWaterZombie313Actor(z);
            if(const auto graph=context->sourceGraphs313.find(uid);graph!=context->sourceGraphs313.end())
                for(const uint32_t auxiliary:graph->second.auxiliaryUIDs)
                    if(auxiliary)
                    {
                        context->sourceGraphClocks313.erase(auxiliary);
                        if(auto* r=app->ReanimationTryToGet(ReanimationID(auxiliary)))r->ReanimationDie();
                    }
            context->sourceGraphClocks313.erase(uint32_t(z->mBodyReanimID));
            context->sourceZombies.erase(uid);context->sourceZombieCursors.erase(uid);
            context->sourceGraphs313.erase(uid);context->dayZombies273.erase(uid);
            context->nightZombies281.erase(uid);context->jackboxZombies300.erase(uid);
            context->waterZombies313.erase(uid);context->extendedZombies313.erase(uid);
            context->nextZombies343.erase(uid);context->heavyZombies313B.erase(uid);context->sourceZombieBirthOrdinals.erase(uid);
            z->mDroppedLoot=true;z->DieNoLoot();
        }
    }
    if(app->mBoard==board)
        if(auto* current=GetNativeContext(board);current&&current->epoch==epoch)current->sourceZombieInitializations313.erase(uid);
}
bool IsNativeSourceZombieInitializing313(const Zombie* z)
{
    if(!z||!z->mApp||z->mApp->NetIsClient()||z->mNetGhost||z->mApp->mBoard!=z->mBoard)return false;
    const auto* context=GetNativeContext(z->mBoard);const auto* data=GetNativeSourceZombieData(z);
    if(!context||!data)return false;
    const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z));
    return uid&&uid==data->instanceToken&&context->sourceZombieInitializations313.contains(uid);
}
bool ReadNativeSourceStatusLevel313(const Zombie* z,int& out)
{
    const auto* data=GetNativeSourceZombieData(z);
    if(!z||!data||!IsNativeSourceZombie(z)||(data->sourceStatusLevel<0&&!(data->kind==205&&data->sourceStatusLevel== -10))||data->sourceStatusLevel>1000000)return false;
    out=data->sourceStatusLevel;return true;
}
bool NativeSourceGraph313OwnsClock(const Reanimation* r)
{
    auto* b=gLawnApp?gLawnApp->mBoard:nullptr;const auto* context=GetNativeContext(b);
    // Graph/ghost clocks are driven elsewhere. Water host clocks use the
    // ordinary holder, which also updates while the native pause menu draws.
    // Hold only those exact source-owned clocks during a gameplay pause.
    if(!context||(context->sourceGraphClocks313.empty()&&
       (!b->mPaused||context->waterZombies313.empty())))return false;
    const uint32_t uid=LiveClock(r,Holder(b));if(!uid)return false;
    if(context->sourceGraphClocks313.contains(uid))return true;
    if(!b->mPaused)return false;
    for(const auto& [owner,state]:context->waterZombies313)
    {
        if(state.auxiliaryID==uid)return true;
        const auto* z=b->mZombies.DataArrayTryToGet(owner);
        if(z&&!z->mDead&&uint32_t(z->mBodyReanimID)==uid)return true;
    }
    return false;
}
void PruneNativeSourceGraph313Clocks(Board* b)
{
    auto* context=GetNativeContext(b);auto* holder=Holder(b);if(!context||!holder)return;
    for(auto it=context->sourceGraphClocks313.begin();it!=context->sourceGraphClocks313.end();)
    {auto* r=holder->mReanimations.DataArrayTryToGet(*it);if(!r||r->mDead)it=context->sourceGraphClocks313.erase(it);else ++it;}
}
bool GetNativeSourceZombieBirthOrder313(Board* b,std::vector<uint32_t>& out)
{
    const auto* context=GetNativeContext(b);
    if(!context||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient())return false;
    std::vector<std::pair<uint64_t,uint32_t>> ordered;ordered.reserve(context->sourceZombieBirthOrdinals.size());
    for(const auto& [uid,ordinal]:context->sourceZombieBirthOrdinals)
    {
        auto* z=b->mZombies.DataArrayTryToGet(uid);
        if(z&&!z->mDead&&GetNativeSourceZombieData(z))ordered.emplace_back(ordinal,uid);
    }
    std::sort(ordered.begin(),ordered.end());std::vector<uint32_t> result;result.reserve(ordered.size());
    for(const auto& entry:ordered)result.push_back(entry.second);out.swap(result);return true;
}
}
