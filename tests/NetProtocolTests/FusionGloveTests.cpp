#include "FusionGloveTests.h"
#include "../../Lawn/Fusion/FusionGloveRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Fusion/FusionWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <limits>

namespace
{
FusionGloveTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Glove check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)

const std::vector<LawnFusion::FusionBankEligibleCard> eligible = {
    {0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};

LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;
    FusionSeedBank bank;
    bank.Open(29,1,eligible,3,kNativeFusionBankCapacity);
    bank.Choose(29,0,0,false); bank.Choose(29,0,1,false);
    bank.Choose(29,1,0,true); bank.Choose(29,1,1,true);
    bank.SetReady(29,0,true); bank.SetReady(29,1,true);
    bank.CommitIntro(29); bank.BeginPlaying(29);
    NativeSnapshotFrame frame;
    frame.epoch=29; frame.selectionCommitted=true; frame.selection=bank.State();
    for (const auto& packet:frame.selection.packets)
    {
        NativePacketData data;
        data.selectionId=packet.id; data.kind=packet.kind; data.isExtra=packet.isExtra;
        data.owner=packet.owner; data.randomOrigin=packet.randomOrigin;
        data.price=packet.price; data.rechargeSeconds=packet.rechargeSeconds;
        frame.packets.push_back({static_cast<uint16_t>(packet.id+10),data});
    }
    NativePlantData plant;
    plant.kind=0; plant.packetId=11; plant.owner=0; plant.instanceToken=0x10021;
    frame.plants.push_back({21,plant});
    plant.kind=1; plant.packetId=13; plant.owner=1; plant.instanceToken=0x10022;
    frame.plants.push_back({22,plant});
    frame.gloveHolds[0]={21,0x10021}; frame.gloveHolds[1]={22,0x10022};
    frame.gloveCooldownTicks=334;
    return frame;
}
}

FusionGloveTestResult RunFusionGloveTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result={};
    std::printf("[ RUN      ] Fusion.NativeGloveLeasesCooldownAndWire\n");
    const FusionGloveIdentity first{21,0x10021}, recycled{21,0x20021}, other{22,0x10022};
    CHECK(FusionGloveIdentityValid(first));
    CHECK(!FusionGloveIdentityValid({}) && FusionGloveIdentityValid({},true));
    CHECK(!FusionGloveIdentityValid({21,0},true) && !FusionGloveIdentityValid({0,0x10021},true));
    CHECK(FusionGloveIdentityMatches(first,first));
    CHECK(!FusionGloveIdentityMatches(first,recycled) && !FusionGloveIdentityMatches(first,other));
    std::array<FusionGloveIdentity,4> holds{};
    CHECK(CanReserveFusionGlove(holds,3,0,first,0));
    CHECK(!CanReserveFusionGlove(holds,3,0,first,1));
    CHECK(!CanReserveFusionGlove(holds,3,2,first,0));
    CHECK(!CanReserveFusionGlove(holds,3,-1,first,0) && !CanReserveFusionGlove(holds,3,4,first,0));
    CHECK(!CanReserveFusionGlove(holds,0x13,0,first,0));
    CHECK(!CanReserveFusionGlove(holds,3,0,{},0));
    holds[0]=first;
    CHECK(CanReserveFusionGlove(holds,3,0,first,1000)); // Idempotent ack, no cooldown gate.
    CHECK(!CanReserveFusionGlove(holds,3,0,other,0)); // Keep the original lease.
    CHECK(!CanReserveFusionGlove(holds,3,1,first,0));
    CHECK(CanReserveFusionGlove(holds,3,1,other,0));
    CHECK(CanReserveFusionGlove(holds,3,1,recycled,0)); // Reused NetId is a distinct generation.
    CHECK(holds[0].plantId==21 && holds[0].instanceToken==0x10021 && holds[1].plantId==0);
    CHECK(FusionGloveSuccessCooldown(6)==334 && FusionGloveSuccessCooldown(0)==1000 &&
        FusionGloveSuccessCooldown(1030)==1000);
    // Source Pot trait uses the same2/3 recharge advance as Puff. Ordinary
    // Body/Pumpkin recipients keep the full timer, regardless of ancestry.
    for (uint16_t kind : {uint16_t(27),uint16_t(1112),uint16_t(1114),uint16_t(1125),uint16_t(1130),uint16_t(1133)})
        CHECK(FusionGloveSuccessCooldown(kind)==334);
    for (uint16_t kind : {uint16_t(24),uint16_t(1088),uint16_t(1027),uint16_t(1033),uint16_t(1116)})
        CHECK(FusionGloveSuccessCooldown(kind)==1000);
    CHECK(AdvanceFusionGloveCooldown(1000,-1,500)==1000);
    CHECK(AdvanceFusionGloveCooldown(1000,500,500)==1000);
    CHECK(AdvanceFusionGloveCooldown(1000,500,499)==1000);
    CHECK(AdvanceFusionGloveCooldown(1000,500,501)==999);
    CHECK(AdvanceFusionGloveCooldown(334,100,433)==1);
    CHECK(AdvanceFusionGloveCooldown(334,100,434)==0);
    CHECK(AdvanceFusionGloveCooldown(1000,0,(std::numeric_limits<int>::max)())==0);
    int cooldown=1000;
    for (int tick=1;tick<=1000;++tick) cooldown=AdvanceFusionGloveCooldown(cooldown,tick-1,tick);
    CHECK(cooldown==0 && AdvanceFusionGloveCooldown(0,1000,1001)==0);

    FusionCommand command;
    command.epoch=29; command.inputEpoch=11; command.sequence=1;
    command.action=FusionCommandKind::GlovePickup;
    command.sourceId=21; command.sourceToken=0x10021;
    NetBuffer pickup;
    CHECK(EncodeFusionCommand(command,pickup));
    FusionCommand decoded;
    auto incoming=pickup;
    CHECK(DecodeFusionCommand(incoming,decoded) && incoming.Remaining()==0 &&
        decoded.action==FusionCommandKind::GlovePickup && decoded.sourceId==21 && decoded.sourceToken==0x10021);
    for (size_t count=0;count<pickup.Size();++count)
    {
        NetBuffer partial; partial.mData.assign(pickup.mData.begin(),pickup.mData.begin()+count);
        FusionCommand unchanged; unchanged.epoch=999; unchanged.sourceToken=0xabcde;
        CHECK(!DecodeFusionCommand(partial,unchanged) && partial.mReadPos==0 &&
            unchanged.epoch==999 && unchanged.sourceToken==0xabcde);
    }
    NetBuffer preserved; preserved.WriteU8(0x5a);
    command.sourceToken=0;
    CHECK(!EncodeFusionCommand(command,preserved) && preserved.Size()==1 && preserved.mData[0]==0x5a);
    command.sourceToken=0x10021;
    command.action=FusionCommandKind::GlovePlace; command.column=9; command.row=4;
    NetBuffer emptyPlace;
    CHECK(EncodeFusionCommand(command,emptyPlace));
    command.targetId=22;
    CHECK(!EncodeFusionCommand(command,preserved)); // Half destination tuple.
    command.targetToken=0x10022;
    NetBuffer mixPlace;
    CHECK(EncodeFusionCommand(command,mixPlace));
    incoming=mixPlace;
    CHECK(DecodeFusionCommand(incoming,decoded) && decoded.targetId==22 && decoded.targetToken==0x10022);
    command.targetId=65536;
    CHECK(!EncodeFusionCommand(command,preserved));
    command.targetId=22; command.column=32;
    CHECK(!EncodeFusionCommand(command,preserved));
    command.column=9; command.row=10;
    CHECK(!EncodeFusionCommand(command,preserved));
    command.action=FusionCommandKind::GloveCancel;
    command.sourceId=0; command.sourceToken=0;
    NetBuffer cancel;
    CHECK(EncodeFusionCommand(command,cancel)); // Cancel pickup-in-flight, own slot only.
    command.sourceToken=1;
    CHECK(!EncodeFusionCommand(command,preserved));

    const auto frame=Example();
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    NetBuffer body; body.WriteU8(0x6a);
    CHECK(EncodeNativeSnapshot(frame,eligible,body));
    NativeSnapshotFrame snapshot;
    NetBuffer native;
    incoming=body;
    CHECK(DecodeNativeSnapshot(incoming,eligible,29,snapshot,native) && incoming.Remaining()==0);
    CHECK(snapshot.gloveCooldownTicks==334 && snapshot.gloveHolds[0].plantId==21 &&
        snapshot.gloveHolds[1].instanceToken==0x10022 && snapshot.plants[0].data.instanceToken==0x10021);
    CHECK(native.Size()==1 && native.mData[0]==0x6a);
    auto forged=frame;
    forged.gloveHolds[1]=forged.gloveHolds[0];
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged=frame; forged.gloveHolds[0].instanceToken=0x20021;
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged=frame; forged.gloveHolds[0].plantId=0;
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged=frame; forged.gloveHolds[2]={21,0x10021};
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged=frame; forged.plants[1].data.instanceToken=forged.plants[0].data.instanceToken;
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged=frame; forged.plants[0].data.instanceToken=0;
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged=frame; forged.gloveCooldownTicks=1001;
    CHECK(!ValidateNativeSnapshotFrame(forged,eligible));
    forged.gloveCooldownTicks=-1;
    CHECK(!EncodeNativeSnapshot(forged,eligible,preserved) && preserved.Size()==1 && preserved.mData[0]==0x5a);
    auto legacy=body;
    legacy.mData[4]=3; legacy.mData[5]=0;
    snapshot.epoch=999; native.Clear(); native.WriteU8(0x91);
    CHECK(!DecodeNativeSnapshot(legacy,eligible,29,snapshot,native) && legacy.mReadPos==0 &&
        snapshot.epoch==999 && native.Size()==1 && native.mData[0]==0x91);
    std::printf("[ %s ] Fusion.NativeGloveLeasesCooldownAndWire checks=%d failures=%d\n",
        result.mFailures ? "  FAILED  " : "       OK ",result.mChecks,result.mFailures);
    return result;
}
