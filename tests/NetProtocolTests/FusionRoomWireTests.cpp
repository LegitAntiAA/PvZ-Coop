#include "FusionRoomWireTests.h"
#include "../../Lawn/Fusion/FusionWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>

namespace
{
FusionRoomWireTestResult result;
void Check(bool condition, int line)
{
    ++result.mChecks;
    if (!condition) { ++result.mFailures; std::printf("    Fusion room wire check failed line=%d\n", line); }
}
#define CHECK(condition) Check(!!(condition), __LINE__)

LawnFusion::FusionStartDescriptor Descriptor()
{
    LawnFusion::FusionStartDescriptor descriptor;
    descriptor.epoch = 7; descriptor.activeMask = 3;
    for (int kind : {0, 1})
    {
        for (bool extra : {false, true})
        {
            LawnFusion::FusionBankEligibleCard card;
            CHECK(LawnFusion::TryMakeFusionEligibleCard(static_cast<uint16_t>(kind), extra, card));
            descriptor.eligible.push_back(card);
        }
    }
    return descriptor;
}

template <typename Decode> void EveryTruncation(const LawnNet::NetBuffer& full, Decode decode)
{
    for (size_t length = 0; length < full.Size(); ++length)
    {
        LawnNet::NetBuffer truncated;
        truncated.WriteU8(0x55);
        truncated.WriteBytes(full.Data(), length);
        truncated.mReadPos = 1;
        CHECK(!decode(truncated));
        CHECK(truncated.mReadPos == 1);
    }
}
}

FusionRoomWireTestResult RunFusionRoomWireTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result = {};
    std::printf("[ RUN      ] Fusion.StartRoomCommandDomainAndRollback\n");
    auto descriptor = Descriptor();
    NetBuffer start;
    CHECK(EncodeFusionStart(descriptor, start));
    FusionStartDescriptor decodedStart;
    CHECK(DecodeFusionStart(start, decodedStart) && start.Remaining() == 0);
    CHECK(decodedStart.epoch == 7 && decodedStart.columns == 10 && decodedStart.initialSun == 500 && decodedStart.eligible == descriptor.eligible);
    EveryTruncation(start, [&](NetBuffer& buffer) { auto old = descriptor; old.epoch = 99; bool ok = DecodeFusionStart(buffer, old); CHECK(old.epoch == 99); return ok; });
    auto invalidDescriptor = descriptor;
    invalidDescriptor.eligible[0].baseCost = 1;
    NetBuffer unchanged; unchanged.WriteU8(0xa5);
    CHECK(!EncodeFusionStart(invalidDescriptor, unchanged) && unchanged.Size() == 1);
    invalidDescriptor = descriptor;
    invalidDescriptor.eligible.push_back(invalidDescriptor.eligible[0]);
    CHECK(!EncodeFusionStart(invalidDescriptor, unchanged));

    FusionSimulation simulation;
    FusionSeedBank bank;
    CHECK(simulation.Reset(10, 5, 500, 1));
    CHECK(bank.Open(7, 1, descriptor.eligible, 3));
    CHECK(bank.Choose(7, 0, 0, false) && bank.Choose(7, 0, 1, false));
    CHECK(bank.ToggleMystery(7, 1, true));
    CHECK(bank.SetReady(7, 0, true) && bank.SetReady(7, 1, true) && bank.CommitIntro(7));
    FusionRoomState room;
    room.epoch = 7; room.revision = 3; room.world = simulation.State(); room.bank = bank.State();
    NetBuffer encoded;
    CHECK(EncodeFusionRoom(room, descriptor, encoded));
    FusionRoomState decoded;
    CHECK(DecodeFusionRoom(encoded, descriptor, decoded) && encoded.Remaining() == 0);
    CHECK(decoded.bank == room.bank && decoded.world.sun == 500);
    for (const auto& packet : decoded.bank.packets)
        if (packet.mysteryPending) CHECK(packet.kind == kFusionNoPlant && !packet.isExtra && packet.price == 0 && packet.rechargeSeconds == 0);
    EveryTruncation(encoded, [&](NetBuffer& buffer) { FusionRoomState old; old.revision = 999; bool ok = DecodeFusionRoom(buffer, descriptor, old); CHECK(old.revision == 999); return ok; });
    auto wrongEpoch = descriptor; wrongEpoch.epoch = 8;
    encoded.mReadPos = 0;
    CHECK(!DecodeFusionRoom(encoded, wrongEpoch, decoded) && encoded.mReadPos == 0);
    auto partial = room;
    for (auto& packet : partial.bank.packets)
        if (packet.mysteryPending) { packet.kind = 0; break; }
    CHECK(!EncodeFusionRoom(partial, descriptor, unchanged));

    for (int action = 1; action <= 7; ++action)
    {
        FusionCommand command;
        command.epoch = 7; command.inputEpoch = 12; command.sequence = 1;
        command.action = (FusionCommandKind)action; command.kind = 1000; command.isExtra = true;
        command.enabled = true; command.packetId = 23; command.column = 9; command.row = 4; command.targetId = 123456;
        NetBuffer wire;
        CHECK(EncodeFusionCommand(command, wire));
        FusionCommand read;
        CHECK(DecodeFusionCommand(wire, read) && wire.Remaining() == 0);
        CHECK(read.action == command.action && read.epoch == 7 && read.inputEpoch == 12 && read.sequence == 1);
        if (action == 1) CHECK(read.kind == 1000 && read.isExtra);
        if (action == 5) CHECK(read.column == 9 && read.row == 4 && read.packetId == 23 && read.targetId == 123456 && read.targetToken == 0);
        EveryTruncation(wire, [&](NetBuffer& buffer) { FusionCommand old; old.sequence = 999; bool ok = DecodeFusionCommand(buffer, old); CHECK(old.sequence == 999); return ok; });
        wire.mReadPos = 0; wire.WriteU8(0);
        CHECK(!DecodeFusionCommand(wire, read) && wire.mReadPos == 0);
    }
    FusionCommand invalid;
    invalid.epoch = 7; invalid.inputEpoch = 12; invalid.sequence = 0;
    CHECK(!EncodeFusionCommand(invalid, unchanged));
    invalid.sequence = 1; invalid.action = (FusionCommandKind)255;
    CHECK(!EncodeFusionCommand(invalid, unchanged));
    CHECK(unchanged.Size() == 1 && unchanged.mData[0] == 0xa5);
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.RoomWire\n" : "[       OK ] Fusion.RoomWire\n");
    return result;
}
