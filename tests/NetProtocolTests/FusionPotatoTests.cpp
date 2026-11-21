#include "FusionPotatoTests.h"
#include "../../Lawn/Fusion/FusionPotatoRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionPotatoTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Fusion Potato check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionPotatoTestResult RunFusionPotatoTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result = {};
    std::printf("[ RUN      ] Fusion.PotatoSourceRulesAndWire\n");
    // Awake0x18047DEA0, pre-base check0x18047E740; source events are
    // quantized to the existing native100Hz clock, not client simulation.
    int ticks = 77;
    CHECK(TryGetFusionPotatoPreparationTicks(14.0f,ticks) && ticks==1401);
    CHECK(TryGetFusionPotatoPreparationTicks(15.0f,ticks) && ticks==1501);
    CHECK(TryGetFusionPotatoPreparationTicks(16.0f,ticks) && ticks==1601);
    CHECK(TryGetFusionPotatoPreparationTicks(14.125f,ticks) && ticks==1414);
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    const float infinity=(std::numeric_limits<float>::infinity)();
    for (float invalid : {13.0f,16.01f,-1.0f,nan,infinity})
    { ticks=77; CHECK(!TryGetFusionPotatoPreparationTicks(invalid,ticks) && ticks==77); }
    CHECK(kFusionPotatoRiseTicks==84 && kFusionPotatoDeathTicks==10 && kFusionPotatoDamage==1800);
    // Exact source trigger exclusions and the intentionally different blast
    // policy. In particular pole/dying contacts cannot trigger but can be hit.
    struct StatusCase { int status; bool trigger, blast; };
    const StatusCase statuses[]={{0,true,true},{1,false,true},{2,false,true},{3,false,true},
        {4,true,true},{10,false,false},{12,true,true},{14,false,true},{16,false,false},
        {17,true,true},{18,true,true},{20,true,true},{25,true,true},{-1,false,false},{26,false,false}};
    for (const auto& sample : statuses)
    {
        CHECK(FusionPotatoCanTrigger(sample.status,true,false,false,true,false)==sample.trigger);
        CHECK(FusionPotatoCanDamage(sample.status,true,false)==sample.blast);
    }
    CHECK(!FusionPotatoCanTrigger(0,false,false,false,true,false));
    CHECK(!FusionPotatoCanTrigger(0,true,true,false,true,false));
    CHECK(!FusionPotatoCanTrigger(0,true,false,true,true,false));
    CHECK(!FusionPotatoCanTrigger(0,true,false,false,false,false));
    CHECK(!FusionPotatoCanTrigger(0,true,false,false,true,true));
    CHECK(!FusionPotatoCanDamage(0,false,false) && !FusionPotatoCanDamage(0,true,true));
    CHECK(GetNativeSeedForPlant(4)==SEED_POTATOMINE && GetNativeSeedForPlant(1007)==SEED_POTATOMINE);

    // Native col9,row2. Independent coordinates from the actual .25-scale
    // prefab Shadow(.1,1.05), trigger offset/size and source mouse mapping.
    FusionPotatoArea area;
    CHECK(TryMakeFusionPotatoArea(724,330,area));
    CHECK(std::abs(area.centerX-724.0)<1e-9 && std::abs(area.centerY-312.0359281437126)<1e-9);
    CHECK(std::abs(area.triggerX-722.618041992)<1e-9 && std::abs(area.triggerY-293.1238847305389)<1e-9);
    const double rx=53.33333333333333,ry=59.880239520958085;
    CHECK(FusionPotatoBlastOverlaps(area,724+rx,area.centerY,0,0));
    CHECK(!FusionPotatoBlastOverlaps(area,724+rx+.001,area.centerY,0,0));
    CHECK(FusionPotatoBlastOverlaps(area,724,area.centerY+ry,0,0));
    CHECK(!FusionPotatoBlastOverlaps(area,724,area.centerY+ry+.001,0,0));
    CHECK(FusionPotatoTriggerOverlaps(area,area.triggerX+29.99481866666667,area.triggerY,0,0));
    CHECK(!FusionPotatoTriggerOverlaps(area,area.triggerX+29.99481866666667+.001,area.triggerY,0,0));
    CHECK(FusionPotatoBlastOverlaps(area,636,250,42,115));
    CHECK(!FusionPotatoTriggerOverlaps(area,636,250,42,115));
    CHECK(FusionPotatoTriggerOverlaps(area,696,250,42,115));
    CHECK(!FusionPotatoBlastOverlaps(area,nan,0,10,10) && !FusionPotatoTriggerOverlaps(area,0,0,-1,10));
    FusionPotatoArea retained{1,2,3,4};
    CHECK(!TryMakeFusionPotatoArea(infinity,0,retained) && retained.centerX==1 && retained.triggerY==4);

    // Source normal/extra mine25/50,30s. Native identity/provenance and all
    // eight real chooser identities survive the unchanged FNSv2 descriptor.
    const std::vector<FusionBankEligibleCard> eligible={{0,false,100,7.5f},{0,true,200,7.5f},
        {1,false,50,7.5f},{1,true,100,7.5f},{2,false,150,50},{2,true,300,50},{4,false,25,30},{4,true,50,30}};
    FusionSeedBank bank;
    CHECK(bank.Open(39,10,eligible,3));
    CHECK(bank.State().targetCount==8 && bank.State().quotas[0]==4 && bank.State().quotas[1]==4);
    for (FusionPlantId kind : {FusionPlantId(0),FusionPlantId(1),FusionPlantId(2),FusionPlantId(4)})
    { CHECK(bank.Choose(39,0,kind,false)); CHECK(bank.Choose(39,1,kind,true)); }
    CHECK(bank.SetReady(39,0,true) && bank.SetReady(39,1,true) && bank.CommitIntro(39) && bank.BeginPlaying(39));
    NativeSnapshotFrame source;
    source.epoch=39; source.selectionCommitted=true; source.selection=bank.State();
    uint16_t minePacket=0;
    for (const auto& packet : source.selection.packets)
    {
        NativePacketData data;
        data.selectionId=packet.id; data.kind=packet.kind; data.isExtra=packet.isExtra;
        data.owner=packet.owner; data.randomOrigin=packet.randomOrigin; data.price=packet.price; data.rechargeSeconds=packet.rechargeSeconds;
        const uint16_t nativeId=static_cast<uint16_t>(packet.id+100);
        source.packets.push_back({nativeId,data});
        if (data.kind==4 && data.isExtra) { minePacket=nativeId; CHECK(data.price==50 && data.rechargeSeconds==30); }
    }
    NativePlantData plant;plant.instanceToken=0x10001u; plant.kind=4; plant.packetId=minePacket; plant.owner=1;
    source.plants.push_back({201,plant});
    CHECK(minePacket && ValidateNativeSnapshotFrame(source,eligible));
    NetBuffer encoded; encoded.WriteU32(0x39415450);
    CHECK(EncodeNativeSnapshot(source,eligible,encoded));
    NativeSnapshotFrame decoded; NetBuffer body,input=encoded;
    CHECK(DecodeNativeSnapshot(input,eligible,39,decoded,body) && input.Remaining()==0 && body.Size()==4);
    CHECK(decoded.plants.size()==1 && decoded.plants[0].data.kind==4 && decoded.plants[0].data.packetId==minePacket);
    auto invalid=source; invalid.plants[0].data.kind=1007;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=source; invalid.plants[0].data.firstParent=0; invalid.plants[0].data.secondParent=4;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=source; invalid.packets.back().data.price+=1;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    for (size_t length=0;length<encoded.Size();++length)
    {
        NetBuffer truncated; truncated.mData.assign(encoded.mData.begin(),encoded.mData.begin()+length);
        NativeSnapshotFrame saved; saved.epoch=77; NetBuffer savedBody; savedBody.WriteU8(99);
        CHECK(!DecodeNativeSnapshot(truncated,eligible,39,saved,savedBody));
        CHECK(truncated.mReadPos==0 && saved.epoch==77 && savedBody.Size()==1 && savedBody.mData[0]==99);
    }
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.PotatoSourceRulesAndWire\n" : "[       OK ] Fusion.PotatoSourceRulesAndWire\n");
    return result;
}
