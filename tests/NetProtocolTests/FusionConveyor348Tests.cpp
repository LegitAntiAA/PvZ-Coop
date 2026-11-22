#include "FusionConveyor348Tests.h"
#include "../../Lawn/Fusion/FusionConveyor348Rules.h"
#include "../../Lawn/Fusion/FusionNativeConveyor308.h"
#include <array>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
using namespace LawnFusion;
// Same structural metadata contract used by the production template; IDs here
// intentionally differ from the monotonic selection identity space.
struct Packet {uint16_t selectionId=0,kind=0;bool isExtra=false;uint8_t owner=255,randomOrigin=0;int price=0;float rechargeSeconds=0;};
struct Record {uint16_t id=0;Packet data;};
struct Fixture
{
    FusionSeedBank bank;
    std::vector<Record> metadata;
    std::array<LawnNet::NetMysterySeedSnapshot,13> raw;
    Fixture(int count,uint16_t kind=0)
    {
        bank.OpenConveyor(7,5,{{0,false,0,0},{6,false,0,0}},3);bank.BeginPlaying(7);
        for(int i=0;i<count;++i)
        {
            bank.AppendConveyor(7,kind);
            const auto& selected=bank.State().packets.back();
            metadata.push_back({uint16_t(200+i),{selected.id,selected.kind}});
            raw[i].mId=uint16_t(200+i);raw[i].mType=kind==6?8:0;raw[i].mActive=1;
        }
    }
    bool Valid() const
    {return FusionConveyorRaw348Valid(bank.State(),std::span<const Record>(metadata),std::span<const LawnNet::NetMysterySeedSnapshot>(raw),[](uint16_t k){return k==6?8:0;});}
};
}
FusionConveyor348TestResult RunFusionConveyor348Tests()
{
    FusionConveyor348TestResult r;
#define CHECK(v) do{++r.mChecks;if(!(v)){++r.mFailures;std::printf("Conveyor348 failed line%d: %s\n",__LINE__,#v);}}while(0)
    for(int level=1;level<=45;++level)
    {
        FusionAdventureSession273 session;session.enabled=true;session.profile.frontierLevel=uint16_t(level);
        CHECK(BeginFusionAdventureRun273(session.profile,level,false,session.run));
        int maximum=0;CHECK(TryGetFusionSourceAdventureWaves272(level,1,maximum));session.maximumWaves=uint16_t(maximum);
        CHECK(FusionAdventureSessionValid273(session));
        const auto expected=(level==27||level==36||level==45)?FusionBankPolicy::SourceConveyor:FusionBankPolicy::Selected;
        CHECK(FusionAdventureBankPolicy308(session)==expected);
        ++session.maximumWaves;CHECK(FusionAdventureBankPolicy308(session)==FusionBankPolicy::Selected);
    }
    CHECK(FusionAdventureBankPolicy308({})==FusionBankPolicy::Selected);
    for(int count=0;count<=13;++count)
    {
        Fixture f(count);CHECK(f.Valid());
        auto meta=f.metadata;std::reverse(f.metadata.begin(),f.metadata.end());CHECK(f.Valid());f.metadata=meta;
        for(int i=0;i<13;++i)
        {
            const auto original=f.raw[i];
            f.raw[i].mOwner=0;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mCooldown=0;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mOriginLevel=1;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mRefreshTime=1;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mRefreshing=1;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mImitater=0;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mSlotCountdown=1;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mSlotNextSeed=0;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mSlotPosition=std::numeric_limits<float>::quiet_NaN();CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mActive=2;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mOffsetX=-1;CHECK(!f.Valid());f.raw[i]=original;
            f.raw[i].mOffsetX=1025;CHECK(!f.Valid());f.raw[i]=original;
            if(i<count)
            {
                f.raw[i].mActive=0;f.raw[i].mOffsetX=1024;CHECK(f.Valid());f.raw[i]=original;
                f.raw[i].mType=8;CHECK(!f.Valid());f.raw[i]=original;
                ++f.metadata[i].data.selectionId;CHECK(!f.Valid());f.metadata=meta;
                f.metadata[i].data.kind=6;CHECK(!f.Valid());f.metadata=meta;
                f.metadata[i].data.owner=0;CHECK(!f.Valid());f.metadata=meta;
                f.metadata[i].data.isExtra=true;CHECK(!f.Valid());f.metadata=meta;
                f.metadata[i].data.price=1;CHECK(!f.Valid());f.metadata=meta;
                f.metadata[i].data.rechargeSeconds=1;CHECK(!f.Valid());f.metadata=meta;
            }
            else
            {
                f.raw[i].mOffsetX=1;CHECK(!f.Valid());f.raw[i]=original;
                f.raw[i].mActive=1;CHECK(!f.Valid());f.raw[i]=original;
                f.raw[i].mId=1;CHECK(!f.Valid());f.raw[i]=original;
            }
        }
        if(count>1)
        {
            std::swap(f.raw[0],f.raw[1]);CHECK(!f.Valid());std::swap(f.raw[0],f.raw[1]);
            f.raw[1].mId=f.raw[0].mId;CHECK(!f.Valid());
        }
    }
    Fixture puff(2,6);CHECK(puff.Valid());
    for(int use=0;use<3;++use)
    {
        CHECK(puff.bank.State().packets.size()==2);
        CHECK(puff.bank.ConsumeConveyor(7,1,1));
        if(use<2)CHECK(puff.Valid()&&puff.bank.State().packets.front().conveyorUses==use+1);
    }
    // Canonical compaction preserves the second identity. Physical raw/native
    // records must compact too, not greedily match a same-kind stale order.
    CHECK(!puff.Valid());puff.raw[0]=puff.raw[1];puff.raw[1]={};puff.metadata.erase(puff.metadata.begin());CHECK(puff.Valid());
    auto state=puff.bank.State();state.packets.front().conveyorUses=3;
    CHECK(!FusionConveyorRaw348Valid(state,std::span<const Record>(puff.metadata),std::span<const LawnNet::NetMysterySeedSnapshot>(puff.raw),[](uint16_t){return 8;}));
#undef CHECK
    return r;
}
