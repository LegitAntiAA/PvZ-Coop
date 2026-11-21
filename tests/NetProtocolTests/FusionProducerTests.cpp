#include "FusionProducerTests.h"
#include "../../Lawn/Fusion/FusionProducerRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionProducerTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Producer check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
const std::vector<LawnFusion::FusionBankEligibleCard> eligible = {
    {0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};

LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;
    FusionSeedBank bank;
    bank.Open(88,1,eligible,3,kNativeFusionBankCapacity);
    bank.Choose(88,0,0,false); bank.Choose(88,0,1,false);
    bank.Choose(88,1,0,true); bank.Choose(88,1,1,true);
    bank.SetReady(88,0,true); bank.SetReady(88,1,true);
    bank.CommitIntro(88); bank.BeginPlaying(88);
    NativeSnapshotFrame frame;
    frame.epoch=88; frame.selectionCommitted=true; frame.selection=bank.State();
    for (const auto& selected:frame.selection.packets)
    {
        NativePacketData packet;
        packet.selectionId=selected.id; packet.kind=selected.kind; packet.isExtra=selected.isExtra;
        packet.owner=selected.owner; packet.price=selected.price; packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,packet});
    }
    NativePlantData plant;
    plant.kind=1006; plant.firstParent=1; plant.secondParent=3;
    plant.instanceToken=0x10001; plant.owner=0; plant.packetId=2;
    plant.productionTicks=501; plant.pendingProductionTicks=50; plant.producerRewardFlags=2;
    frame.plants.push_back({100,plant});
    return frame;
}
}

FusionProducerTestResult RunFusionProducerTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.NativeProducerSourceTimingAndRewards\n");
    struct Profile { uint16_t kind; int interval, coins, health; };
    for (const auto sample:{Profile{1,25,1,300},Profile{1000,25,1,300},Profile{1006,15,1,4000},Profile{1033,20,2,300},Profile{1187,15,1,300}})
    {
        const auto* profile=FindFusionProducerProfile(sample.kind);
        const auto* definition=FusionFindPlant(sample.kind);
        CHECK(profile && FusionProducerProfileValid(*profile) && IsNativeProducerKind(sample.kind));
        CHECK(profile && profile->intervalSeconds==sample.interval && profile->periodicCoinCount==sample.coins);
        CHECK(definition && definition->hasStats && definition->productionIntervalSeconds==sample.interval && definition->maxHealth==sample.health);
        if (!profile) continue;
        for (int jitter:{-2,0,2})
        {
            int ticks=77;
            CHECK(TryGetFusionProducerCycleTicks(*profile,jitter,ticks) && ticks==(sample.interval+jitter)*100+1);
        }
        int unchanged=77;
        CHECK(!TryGetFusionProducerCycleTicks(*profile,-3,unchanged) && unchanged==77);
        CHECK(!TryGetFusionProducerCycleTicks(*profile,3,unchanged) && unchanged==77);
        CHECK(FusionProducerGateOpen(*profile,true,false,1));
        CHECK(!FusionProducerGateOpen(*profile,false,false,1));
        CHECK(FusionProducerGateOpen(*profile,true,false,0)==(sample.kind==1000));
        CHECK(FusionProducerGateOpen(*profile,true,true,1)==(sample.kind!=1000));
    }
    for (uint16_t kind:{uint16_t(0),uint16_t(1077),uint16_t(65535)})
        CHECK(!FindFusionProducerProfile(kind) && !IsNativeProducerKind(kind));
    struct Initial { float seconds; int ticks; };
    for (const auto v:{Initial{4,401},Initial{4.125f,413},Initial{5,501},Initial{7,701},
        Initial{4.01f,402},Initial{std::nextafter(4.01f,0.0f),401}})
    {
        int ticks=77;
        CHECK(TryGetFusionProducerInitialTicks(v.seconds,ticks) && ticks==v.ticks);
    }
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    for (float seconds:{3.99f,7.01f,nan,(std::numeric_limits<float>::infinity)()})
    {
        int ticks=77;
        CHECK(!TryGetFusionProducerInitialTicks(seconds,ticks) && ticks==77);
    }
    const auto* base=FindFusionProducerProfile(1);
    if (!base) return result;
    for (int variant=0;variant<5;++variant)
    {
        auto bad=*base;
        if (variant==0) bad.intervalSeconds=nan;
        if (variant==1) bad.intervalSeconds=2;
        if (variant==2) bad.intervalSeconds=(std::numeric_limits<float>::max)();
        if (variant==3) bad.periodicCoinCount=3;
        if (variant==4) bad.gate=static_cast<FusionProducerGate>(255);
        int ticks=77;
        CHECK(!FusionProducerProfileValid(bad) && !TryGetFusionProducerCycleTicks(bad,0,ticks) && ticks==77);
    }
    // Source initial4s underflow occurs at401; Invoke.5 emits at451, without
    // consuming its first pending tick on the trigger's own update.
    int counter=401,pending=0,emissions=0,emittedAt=0;
    for (int tick=1;tick<=451;++tick)
    {
        if (AdvanceFusionProducerPending(pending)) { ++emissions; emittedAt=tick; }
        if (AdvanceFusionProducerCounter(counter)) { counter=2501; pending=kFusionProducerDelayTicks; }
        if (tick==400) CHECK(counter==1 && pending==0 && emissions==0);
        if (tick==401) CHECK(counter==2501 && pending==50 && emissions==0);
        if (tick==450) CHECK(pending==1 && emissions==0);
    }
    CHECK(emissions==1 && emittedAt==451 && pending==0);
    pending=1;
    CHECK(!FusionProducerGateOpen(*base,true,false,0) && AdvanceFusionProducerPending(pending) && pending==0);
    counter=-1; pending=51;
    CHECK(!AdvanceFusionProducerCounter(counter) && counter==-1 && !AdvanceFusionProducerPending(pending) && pending==51);
    CHECK(FusionProducerCanFitCallback(8,10,2) && !FusionProducerCanFitCallback(9,10,2));
    CHECK(FusionProducerCanFitCallback(9,10,1) && !FusionProducerCanFitCallback(10,10,1));
    CHECK(!FusionProducerCanFitCallback(-1,10,1) && !FusionProducerCanFitCallback(11,10,1) && !FusionProducerCanFitCallback(0,10,0));

    struct Health { int health; uint8_t before, after; int count, stage; };
    for (const auto v:{Health{4000,0,0,0,0},Health{2666,0,0,0,0},Health{2665,0,1,1,1},
        Health{1334,0,1,1,1},Health{1333,0,0,0,0},Health{1332,0,2,1,2},Health{0,0,2,1,2},
        Health{-1,0,2,1,2},Health{2000,1,1,0,1},Health{1000,3,3,0,2},Health{4000,3,3,0,0},
        Health{2000,2,3,1,1},Health{1000,1,3,1,2}})
    {
        auto flags=v.before; int count=77;
        CHECK(FusionSunNutStage(v.health)==v.stage &&
            TryConsumeFusionSunNutReward(v.health,false,flags,count) && flags==v.after && count==v.count);
    }
    uint8_t flags=0; int count=0;
    CHECK(TryConsumeFusionSunNutReward(1000,false,flags,count) && flags==2 && count==1); // Jump skips band1.
    CHECK(TryConsumeFusionSunNutReward(4000,false,flags,count) && flags==2 && count==0); // Healing retains history.
    CHECK(TryConsumeFusionSunNutReward(2000,false,flags,count) && flags==3 && count==1);
    flags=0;
    CHECK(TryConsumeFusionSunNutReward(0,true,flags,count) && flags==0 && count==0); // Source IZ suppresses flags too.
    CHECK(TryConsumeFusionSunNutReward(0,false,flags,count) && count==1 && (count+2)*25==75);
    flags=4; count=77;
    CHECK(!TryConsumeFusionSunNutReward(1000,false,flags,count) && flags==4 && count==77);

    const auto frame=Example();
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    auto invalid=frame; invalid.plants[0].data.producerRewardFlags=4;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.plants[0].data.pendingProductionTicks=51;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.plants[0].data.kind=1; invalid.plants[0].data.firstParent=invalid.plants[0].data.secondParent=kFusionNoPlant;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible)); // SunNut bits must not migrate to ordinary Sunflower.
    invalid.plants[0].data.producerRewardFlags=0;
    CHECK(ValidateNativeSnapshotFrame(invalid,eligible));
    LawnNet::NetBuffer wire; wire.WriteU8(0x88);
    CHECK(EncodeNativeSnapshot(frame,eligible,wire));
    LawnNet::NetBuffer input=wire,native;
    NativeSnapshotFrame decoded;
    CHECK(DecodeNativeSnapshot(input,eligible,88,decoded,native) && input.Remaining()==0);
    CHECK(decoded.plants[0].data.producerRewardFlags==2 && decoded.plants[0].data.pendingProductionTicks==50 &&
        decoded.plants[0].data.productionTicks==501 && native.Size()==1 && native.mData[0]==0x88);
    std::printf("[ %s ] Fusion.NativeProducerSourceTimingAndRewards checks=%d failures=%d\n",
        result.mFailures ? "  FAILED  " : "       OK ",result.mChecks,result.mFailures);
    return result;
}
