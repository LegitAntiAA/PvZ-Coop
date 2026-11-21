#include "FusionDoubleShooterTests.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>

namespace
{
FusionDoubleShooterTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    DoubleShooter check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionDoubleShooterTestResult RunFusionDoubleShooterTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.SharedDoubleShooterTimelineAndWire\n");
    struct Vector { float speed; int initial, firstTick, lastTick; };
    const Vector vectors[]={{.9f,61,30,60},{1,55,27,54},{1.1f,50,25,49}};
    for (uint16_t kind : {uint16_t(1030),uint16_t(1008)})
    {
        CHECK(GetNativeSeedForPlant(kind)==SEED_PEASHOOTER);
        const auto* p=FindFusionShooterTimingProfile(kind);
        CHECK(p && p->eventClipSeconds.size()==2 && p->eventClipSeconds[0]==.5333333611488342 &&
            p->eventClipSeconds[1]==1.0666667222976685 && p->clipDurationSeconds==1.3333333730697632);
        if (!p) continue;
        CHECK(p->projectileKind==(kind==1030 ? 0 : 1));
        for (const auto v : vectors)
        {
            int counter=0;
            CHECK(TryGetFusionShooterShotCounter(*p,v.speed,counter) && counter==v.initial);
            CHECK(FusionNativeAnimationSpeedValid(kind,v.speed));
            int total=0;
            for (int tick=1;counter>0;++tick)
            {
                --counter; // The real native UpdateShooting ordering.
                int count=-1;
                CHECK(TryGetFusionShooterEventsAtCounter(*p,v.speed,counter,count));
                CHECK(count==((tick==v.firstTick || tick==v.lastTick) ? 1 : 0));
                total+=count;
            }
            CHECK(total==2);
        }
    }
    // Same quantized tick and exactly equal source events each retain both
    // callbacks, with no extra event index, counter or pending-shot clock.
    auto coincident=*FindFusionShooterTimingProfile(1030);
    const double roundedTogether[]={.51,.5101}, equal[]={.5,.5}, reversed[]={.5,.4};
    for (auto events : {std::span<const double>(roundedTogether),std::span<const double>(equal)})
    {
        coincident.eventClipSeconds=events;
        int counter=0,total=0;
        CHECK(TryGetFusionShooterShotCounter(coincident,1,counter));
        while (counter>0)
        {
            --counter;
            int count=-1;
            CHECK(TryGetFusionShooterEventsAtCounter(coincident,1,counter,count));
            CHECK(count==(counter==1 ? 2 : 0)); total+=count;
        }
        CHECK(total==2);
    }
    coincident.eventClipSeconds=reversed;
    int count=77;
    CHECK(!TryGetFusionShooterEventsAtCounter(coincident,1,1,count) && count==77);
    coincident.eventClipSeconds={};
    CHECK(!FusionShooterTimingProfileValid(coincident));
    const auto& single=*FindFusionShooterTimingProfile(1001);
    int initial=0;
    CHECK(TryGetFusionShooterShotCounter(single,1,initial) && initial==35);
    CHECK(!TryGetFusionShooterEventsAtCounter(single,1,initial,count) && count==77);
    CHECK(TryGetFusionShooterEventsAtCounter(single,1,1,count) && count==1);
    CHECK(TryGetFusionShooterEventsAtCounter(single,1,0,count) && count==0);
    CHECK(GetNativeSeedForPlant(1004)==SEED_PEASHOOTER);
    CHECK(GetNativeSeedForPlant(65532)==SEED_NONE);

    // Isolated semantic bank; production still offers all sixteen base cards.
    const std::vector<FusionBankEligibleCard> eligible={{0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};
    FusionSeedBank bank;
    CHECK(bank.Open(59,1,eligible,3,kNativeFusionBankCapacity));
    CHECK(bank.Choose(59,0,0,false) && bank.Choose(59,0,1,false));
    CHECK(bank.Choose(59,1,0,true) && bank.Choose(59,1,1,true));
    CHECK(bank.SetReady(59,0,true) && bank.SetReady(59,1,true) && bank.CommitIntro(59) && bank.BeginPlaying(59));
    NativeSnapshotFrame frame; frame.epoch=59;frame.selectionCommitted=true;frame.selection=bank.State();
    for (const auto& selected:frame.selection.packets)
    {
        NativePacketData packet;packet.selectionId=selected.id;packet.kind=selected.kind;packet.isExtra=selected.isExtra;
        packet.owner=selected.owner;packet.price=selected.price;packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,packet});
    }
    NativePlantData plant;plant.instanceToken=0x10001u;plant.kind=1030;plant.firstParent=0;plant.secondParent=0;plant.owner=0;plant.packetId=1;
    plant.animationSpeed=1.03f;plant.shootSeconds=.1f;
    frame.plants.push_back({100,plant});
    NativeProjectileData pea;pea.kind=0;
    frame.projectiles.push_back({101,pea});
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    const uint16_t ancestry[][2]={{0,1001},{1001,0},{2,1030},{1030,2}};
    for (const auto& parents:ancestry)
    {
        auto hybrid=frame;
        hybrid.plants[0].data.kind=1008;hybrid.plants[0].data.firstParent=parents[0];hybrid.plants[0].data.secondParent=parents[1];
        hybrid.projectiles[0].data.kind=1;hybrid.projectiles[0].data.damage=60;
        CHECK(ValidateNativeSnapshotFrame(hybrid,eligible));
    }
    auto invalid=frame;invalid.plants[0].data.firstParent=kFusionNoPlant;invalid.plants[0].data.secondParent=kFusionNoPlant;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.plants[0].data.secondParent=1;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.plants[0].data.kind=0;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.projectiles[0].data.damage=21;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.projectiles[0].data.sunOnHit=5;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.plants[0].data.shootSeconds=FusionShooterDuration(*FindFusionShooterTimingProfile(1030),1.03f);
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    LawnNet::NetBuffer wire;wire.WriteU32(59);
    CHECK(EncodeNativeSnapshot(frame,eligible,wire));
    NativeSnapshotFrame decoded;LawnNet::NetBuffer native,input=wire;
    CHECK(DecodeNativeSnapshot(input,eligible,59,decoded,native));
    CHECK(decoded.plants[0].data.kind==1030 && decoded.projectiles[0].data.kind==0 && decoded.projectiles[0].data.damage==20);
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.SharedDoubleShooterTimelineAndWire\n" : "[       OK ] Fusion.SharedDoubleShooterTimelineAndWire\n");
    return result;
}
