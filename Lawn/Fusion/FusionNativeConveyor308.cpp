#include "FusionNativeConveyor308.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "../Board.h"
#include "../SeedPacket.h"
#include "../Plant.h"
#include "../CursorObject.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace LawnFusion
{
namespace
{
bool Fail(std::string* error,const char* reason){if(error)*error=reason;return false;}
bool Host(const Board* b){return b&&b->mApp&&!b->mApp->NetIsClient();}
bool BirthBudget(const Board* b)
{
    const auto& state=GetNativeContext(b)->selection.State();
    const uint64_t live=uint64_t(b->mPlants.mSize)+b->mZombies.mSize+b->mProjectiles.mSize+b->mCoins.mSize+
        b->mLawnMowers.mSize+b->mGridItems.mSize+SEEDBANK_CAPACITY;
    return SEEDBANK_CAPACITY>=kFusionSourceConveyor305Capacity&&live<65535&&
        state.nextPacketId<65536&&state.revision<(std::numeric_limits<uint32_t>::max)();
}
int CursorSlot(const Board* b,const CursorObject* cursor)
{
    for(int slot=0;slot<4;++slot)if(b->mNetPlayerCursors[slot]==cursor)return slot;
    return b->mApp->mNetManager?b->mApp->mNetManager->GetActingPlayerSlot():0;
}
void Layout(Board* b)
{
    auto* bank=b->mSeedBank;const auto& g=kNativeConveyor308Geometry;
    bank->mNumPackets=int(kFusionSourceConveyor305Capacity);
    bank->mWidth=(std::max)(bank->mWidth,g.clipLeft+g.clipWidth+5);
    for(int i=0;i<bank->mNumPackets;++i)
    {auto& p=bank->mSeedPackets[i];p.mIndex=i;p.mX=g.PacketX(i);p.mY=g.top;p.mWidth=g.width;p.mHeight=g.height;}
}
void RestoreCursors(Board* b,const NativeConveyor308CursorState& saved)
{
    const auto& state=GetNativeContext(b)->selection.State();
    for(size_t n=0;n<saved.cursors.size();++n)
    {
        auto* cursor=saved.cursors[n];if(!cursor||cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK)continue;
        const int slot=CursorSlot(b,cursor);int found=-1;
        if(slot>=0&&slot<4&&(state.activeMask&(1U<<slot)))
            for(int i=0;i<b->mSeedBank->mNumPackets;++i)
                if(saved.packetIds[n]&&b->mSeedBank->mSeedPackets[i].mNetId==saved.packetIds[n]){found=i;break;}
        if(found>=0){cursor->mSeedBankIndex=found;continue;}
        // Native bank cursor cancellation fields; no tool/world callback or new allocation.
        cursor->mType=cursor->mImitaterType=SEED_NONE;cursor->mCursorType=CURSOR_TYPE_NORMAL;
        cursor->mSeedBankIndex=-1;cursor->mCoinID=COINID_NULL;cursor->mDuplicatorPlantID=PLANTID_NULL;
        cursor->mCobCannonPlantID=PLANTID_NULL;cursor->mGlovePlantID=PLANTID_NULL;
    }
}
bool ViewMatches(const Board* b,std::string* error)
{
    const auto* c=GetNativeContext(b);const auto* bank=b?b->mSeedBank:nullptr;
    if(!IsNativeSourceConveyor308(b)||!bank||bank->mNumPackets<0||bank->mNumPackets>13)
        return Fail(error,"source conveyor native bank unavailable");
    const auto& state=c->selection.State();
    if(!FusionSeedBank::ValidateSnapshot(state,c->eligible,13,FusionBankPolicy::SourceConveyor)||c->packets.size()!=state.packets.size())
        return Fail(error,"source conveyor canonical bank/metadata invalid");
    int occupied=0;
    while(occupied<bank->mNumPackets&&bank->mSeedPackets[occupied].mPacketType!=SEED_NONE)++occupied;
    if(size_t(occupied)!=state.packets.size())return Fail(error,"source conveyor native count differs from canonical queue");
    for(int i=0;i<bank->mNumPackets;++i)
    {
        const auto& p=bank->mSeedPackets[i];
        if(i>=occupied){if(p.mPacketType!=SEED_NONE||p.mNetId)return Fail(error,"source conveyor has a noncontiguous native queue");continue;}
        const auto& selected=state.packets[i];const auto found=c->packets.find(p.mNetId);
        if(!p.mNetId||found==c->packets.end())return Fail(error,"source conveyor native packet identity is absent");
        const auto& m=found->second;
        if(m.selectionId!=selected.id||m.kind!=selected.kind||m.isExtra||m.owner!=0xff||m.randomOrigin||m.price||m.rechargeSeconds!=0||
            p.mPacketType!=GetNativeSeedForPlant(selected.kind)||p.mImitaterType!=SEED_NONE||p.mNetOwnerSlot!=0xff||p.mRandomDiscountLevel)
            return Fail(error,"source conveyor native/selection identity differs");
    }
    return true;
}
}

FusionBankPolicy GetNativeBankPolicy308(const Board* b)
{
    if(!b||!b->mApp||!b->mApp->IsFusionSourceAdventure())return FusionBankPolicy::Selected;
    return FusionAdventureBankPolicy308(b->mApp->mSourceAdventure);
}
size_t GetNativeBankCapacity308(const Board* b)
{return GetNativeBankPolicy308(b)==FusionBankPolicy::SourceConveyor?kFusionSourceConveyor305Capacity:kNativeFusionBankCapacity;}
bool IsNativeSourceConveyor308(const Board* b)
{
    const auto* c=GetNativeContext(b);
    return c&&b->mApp->mBoard==b&&!c->adventureTeardown&&GetNativeBankPolicy308(b)==FusionBankPolicy::SourceConveyor&&c->adventure.enabled&&
        c->adventure.run.level==b->mApp->mSourceAdventure.run.level&&c->epoch==b->mApp->mNativeFusionEpoch&&
        c->selection.Policy()==FusionBankPolicy::SourceConveyor&&c->selection.State().epoch==c->epoch;
}

NativeConveyor308CursorState CaptureNativeConveyorCursors308(const Board* b)
{
    NativeConveyor308CursorState result;if(!b||!b->mSeedBank)return result;
    const auto capture=[&](CursorObject* cursor)
    {
        if(!cursor||cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK)return;
        for(auto* previous:result.cursors)if(previous==cursor)return;
        const int i=cursor->mSeedBankIndex;
        const bool valid=i>=0&&i<b->mSeedBank->mNumPackets&&i<SEEDBANK_CAPACITY;
        for(size_t n=0;n<result.cursors.size();++n)if(!result.cursors[n])
        {result.cursors[n]=cursor;result.packetIds[n]=valid?b->mSeedBank->mSeedPackets[i].mNetId:0;break;}
    };
    capture(b->mCursorObject);for(auto* cursor:b->mNetPlayerCursors)capture(cursor);return result;
}
bool NativeConveyorPacketHeldByOther308(const Board* b,uint16_t netId,int slot)
{
    if(!IsNativeSourceConveyor308(b)||!netId)return false;
    const auto saved=CaptureNativeConveyorCursors308(b);const auto mask=GetNativeContext(b)->selection.State().activeMask;
    for(size_t n=0;n<saved.cursors.size();++n)if(saved.cursors[n]&&saved.packetIds[n]==netId)
    {const int holder=CursorSlot(b,saved.cursors[n]);if(holder>=0&&holder<4&&holder!=slot&&(mask&(1U<<holder)))return true;}
    return false;
}
bool AdoptNativeConveyor308(Board* b,const NativeConveyor308CursorState& saved,std::string* error)
{
    if(!b||!b->mApp||!b->mApp->NetIsClient()||!ViewMatches(b,error))return false;
    Layout(b);
    const auto& packets=GetNativeContext(b)->selection.State().packets;
    for(int i=0;i<13;++i)
    {
        auto& packet=b->mSeedBank->mSeedPackets[i];packet.mRefreshCounter=packet.mRefreshTime=0;packet.mRefreshing=false;
        if(size_t(i)<packets.size()){packet.mTimesUsed=packets[size_t(i)].conveyorUses;packet.mActive=true;}
        else{packet.mPacketType=SEED_NONE;packet.mNetId=0;packet.mTimesUsed=0;packet.mActive=false;packet.mOffsetX=0;}
    }
    RestoreCursors(b,saved);if(error)error->clear();return true;
}
bool CanUseNativeConveyorPacket308(const Board* b,uint16_t netId,int slot)
{
    if(!IsNativeSourceConveyor308(b)||!netId||NativeConveyorPacketHeldByOther308(b,netId,slot))return false;
    const auto* c=GetNativeContext(b);const auto found=c->packets.find(netId);
    if(found==c->packets.end())return false;
    const auto& d=found->second;const auto* packet=c->selection.PeekReadyPacket(c->epoch,slot,d.selectionId);
    return packet&&packet->kind==d.kind&&!d.isExtra&&d.owner==0xff&&!d.randomOrigin&&!d.price&&d.rechargeSeconds==0;
}

struct NativeConveyor308Plan
{
    Board* owner=nullptr;NativeContext* context=nullptr;uint32_t revision=0;
    FusionSeedBank selection;
    std::unordered_map<uint16_t,NativePacketData> metadata;
    std::array<uint16_t,13> oldIds{},netIds{};size_t oldCount=0;
};

namespace
{
std::unique_ptr<NativeConveyor308Plan> PreparePlan(Board* b,const FusionSeedBank& next,std::string* error)
{
    if(!Host(b)||!ViewMatches(b,error))return {};
    auto* c=GetNativeContext(b);if(next.Policy()!=FusionBankPolicy::SourceConveyor||!FusionSeedBank::ValidateSnapshot(next.State(),c->eligible,13,FusionBankPolicy::SourceConveyor))
    {Fail(error,"source conveyor pending controller invalid");return {};}
    const uint16_t oldAllocator=b->mNextNetId;
    try
    {
        auto plan=std::make_unique<NativeConveyor308Plan>();plan->owner=b;plan->context=c;plan->revision=c->selection.State().revision;
        plan->selection=next;plan->oldCount=c->selection.State().packets.size();plan->metadata.reserve(13);
        for(size_t i=0;i<plan->oldCount;++i)plan->oldIds[i]=b->mSeedBank->mSeedPackets[i].mNetId;
        for(size_t i=0;i<next.State().packets.size();++i)
        {
            const auto& selected=next.State().packets[i];uint16_t id=0;
            for(const auto& [oldId,data]:c->packets)if(data.selectionId==selected.id){id=oldId;break;}
            if(!id)
            {
                // Every native live entity family is counted. Fewer than65535
                // live objects proves NetAllocId's exhaustive search has a slot.
                if(!BirthBudget(b))throw std::runtime_error("source conveyor entity ID budget exhausted");
                id=b->NetAllocId();if(!id)throw std::runtime_error("source conveyor entity ID allocation failed");
            }
            NativePacketData data;data.selectionId=selected.id;data.kind=selected.kind;data.owner=0xff;
            if(GetNativeSeedForPlant(data.kind)==SEED_NONE||!plan->metadata.emplace(id,data).second)
                throw std::runtime_error("source conveyor pending identity collision");
            plan->netIds[i]=id;
        }
        return plan;
    }
    catch(const std::exception& e){b->mNextNetId=oldAllocator;if(error)*error=e.what();return {};}
}
void CommitPlan(Board* b,NativeConveyor308Plan& plan) noexcept
{
    auto* c=GetNativeContext(b);auto* bank=b->mSeedBank;
    assert(c==plan.context&&b==plan.owner&&c->selection.State().revision==plan.revision);
    const auto saved=CaptureNativeConveyorCursors308(b);bank->mNumPackets=13;
    for(int i=int(plan.oldCount)-1;i>=0;--i)
        if(!plan.metadata.contains(plan.oldIds[size_t(i)]))bank->RemoveSeed(i);
    int occupied=bank->GetNumSeedsOnConveyorBelt();
    while(size_t(occupied)<plan.selection.State().packets.size())
    {bank->AddSeed(GetNativeSeedForPlant(plan.selection.State().packets[size_t(occupied)].kind));++occupied;}
    for(int i=0;i<13;++i)
    {
        auto& p=bank->mSeedPackets[i];p.mApp=b->mApp;p.mBoard=b;p.mNetGhost=false;p.mNetOwnerSlot=0xff;p.mRandomDiscountLevel=0;
        p.mRefreshCounter=p.mRefreshTime=0;p.mRefreshing=false;p.mImitaterType=SEED_NONE;
        if(size_t(i)<plan.selection.State().packets.size())
        {p.mNetId=plan.netIds[size_t(i)];p.mTimesUsed=plan.selection.State().packets[size_t(i)].conveyorUses;p.mActive=true;}
        else{p.mNetId=0;p.mPacketType=SEED_NONE;p.mTimesUsed=0;p.mActive=false;p.mOffsetX=0;}
    }
    c->packets.swap(plan.metadata);c->selection=std::move(plan.selection);Layout(b);RestoreCursors(b,saved);
}
}

NativeConveyor308Use::NativeConveyor308Use()=default;
NativeConveyor308Use::~NativeConveyor308Use()=default;
NativeConveyor308Use::NativeConveyor308Use(NativeConveyor308Use&&) noexcept=default;
NativeConveyor308Use& NativeConveyor308Use::operator=(NativeConveyor308Use&&) noexcept=default;
bool PrepareNativeConveyorUse308(Board* b,int slot,uint16_t netId,NativeConveyor308Use& output,std::string* error)
{
    if(!Host(b)||!CanUseNativeConveyorPacket308(b,netId,slot))return Fail(error,"source conveyor packet is reserved or unavailable");
    auto* c=GetNativeContext(b);const auto data=c->packets.find(netId);
    if(data==c->packets.end())return Fail(error,"source conveyor packet identity absent");
    auto next=c->selection;if(!next.ConsumeConveyor(c->epoch,slot,data->second.selectionId))return Fail(error,"source conveyor use rejected");
    auto plan=PreparePlan(b,next,error);if(!plan)return false;
    output.plan=std::move(plan);if(error)error->clear();return true;
}
void CommitNativeConveyorUse308(Board* b,NativeConveyor308Use& use) noexcept
{assert(use.plan);CommitPlan(b,*use.plan);use.plan.reset();}
bool SyncNativeConveyor308(Board* b,std::string* error)
{
    if(!Host(b)||!IsNativeSourceConveyor308(b))return false;
    auto plan=PreparePlan(b,GetNativeContext(b)->selection,error);if(!plan)return false;
    CommitPlan(b,*plan);if(error)error->clear();return true;
}
bool PreflightNativeSourceConveyor308(Board* b,std::string* error)
{
    if(!IsNativeSourceConveyor308(b))return Fail(error,"source conveyor descriptor/context policy differs");
    const auto* profile=FindFusionConveyor305Profile(GetNativeContext(b)->adventure.run.level);
    if(!profile)return Fail(error,"source conveyor feed profile unavailable");
    std::vector<uint16_t> kinds(profile->orderedPool.begin(),profile->orderedPool.end());
    std::sort(kinds.begin(),kinds.end());kinds.erase(std::unique(kinds.begin(),kinds.end()),kinds.end());
    // Existing shared visual admission recursively covers natural projectile,
    // child-model and embedded particle dependencies. Weights remain untouched.
    if(!EnsureNativeVisuals(b,kinds,{},error))return false;
    const auto definition=[](ReanimationType type)
    {
        ReanimatorEnsureDefinitionLoaded(type,false);
        return gReanimatorDefArray&&unsigned(type)<gReanimatorDefCount&&
            gReanimatorDefArray[int(type)].mTracks.tracks&&gReanimatorDefArray[int(type)].mTracks.count>0;
    };
    for(const auto kind:kinds)
    {
        const auto seed=GetNativeSeedForPlant(kind);
        if(seed==SEED_NONE||!FusionFindPlant(kind)||!definition(GetPlantDefinition(seed).mReanimationType))
            return Fail(error,"source conveyor native initializer definition unavailable");
        if((seed==SEED_PEASHOOTER||seed==SEED_REPEATER||seed==SEED_THREEPEATER)&&!definition(REANIM_PEASHOOTER))
            return Fail(error,"source conveyor native head definition unavailable");
        if(Plant::IsNocturnal(seed)&&!b->StageIsNight()&&!definition(REANIM_SLEEPING))
            return Fail(error,"source conveyor native sleep definition unavailable");
    }
    if(error)error->clear();return true;
}
bool InitializeNativeConveyor308(Board* b,std::string* error)
{
    if(!PreflightNativeSourceConveyor308(b,error))return false;
    if(Host(b))
    {
        if(!SyncNativeConveyor308(b,error))return false;
        GetNativeContext(b)->conveyor308=std::make_unique<NativeConveyor308State>(GetNativeContext(b)->seed);
    }
    return true;
}
bool UpdateNativeSourceConveyor308(Board* b)
{
    if(!IsNativeSourceConveyor308(b))return false;
    auto* c=GetNativeContext(b);
    if(!Host(b)||!CanApplyNativeFusionGameplay(b)||!c->conveyor308||c->conveyor308->failed||c->conveyor308->lastTick==b->mMainCounter)return true;
    auto& live=*c->conveyor308;live.lastTick=b->mMainCounter;std::string error;
    if(!ViewMatches(b,&error))
    {live.failed=true;LawnNet::NetTrace("Fusion source conveyor308 invalid canonical view epoch=%u reason=%s",c->epoch,error.c_str());return true;}
    auto feed=live.feed;auto random=live.random;auto bank=c->selection;
    std::array<uint16_t,13> kinds{};size_t count=0;for(const auto& packet:bank.State().packets)kinds[count++]=packet.kind;
    const auto* profile=FindFusionConveyor305Profile(c->adventure.run.level);
    if(!profile){error="source conveyor profile unavailable";live.failed=true;}
    else if(count<13&&feed.elapsedSeconds+.01f>profile->intervalSeconds)
    {
        if(!BirthBudget(b)){error="source conveyor packet/ID/revision budget exhausted";live.failed=true;}
        else if(!PreflightNativeSourceConveyor308(b,&error))live.failed=true;
    }
    if(!live.failed)
    {
        FusionConveyor305Step step;
        if(!StepFusionConveyor305(profile->sourceLevel,feed,std::span<const uint16_t>(kinds.data(),count),.01f,false,
            [&](size_t maximum,size_t& index){if(!maximum||maximum>(std::numeric_limits<unsigned long>::max)())return false;index=random.Next(static_cast<unsigned long>(maximum));return true;},step))
        {error="source conveyor feed transaction rejected";live.failed=true;}
        else if(!step.birth){live.feed=feed;live.random=random;return true;}
        else if(!bank.AppendConveyor(c->epoch,step.kind))
        {error="source conveyor append transaction rejected";live.failed=true;}
        else if(auto plan=PreparePlan(b,bank,&error))
        {CommitPlan(b,*plan);live.feed=feed;live.random=random;return true;}
        else live.failed=true;
    }
    if(live.failed)LawnNet::NetTrace("Fusion source conveyor308 stopped epoch=%u level=%u reason=%s",c->epoch,c->adventure.run.level,error.c_str());
    return true;
}
}
