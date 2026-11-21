#include "FusionWireTests.h"
#include "../../Lawn/Fusion/FusionWire.h"
#include "../../Lawn/Fusion/FusionRecipe.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <limits>

namespace
{
FusionWireTestResult result;
void Check(bool condition, int line)
{
    ++result.mChecks;
    if (!condition) { ++result.mFailures; std::printf("    Fusion wire check failed line=%d\n", line); }
}
#define CHECK(condition) Check(!!(condition), __LINE__)

LawnFusion::FusionSimulationState Example()
{
    using namespace LawnFusion;
    FusionSimulationState state;
    state.columns = 10; state.rows = 5; state.sun = 350; state.nextId = 9; state.rngState = 123;
    FusionPlantState plant;
    plant.id = 5; plant.type = 1000; plant.firstParent = 0; plant.secondParent = 1;
    plant.column = 3; plant.row = 2; plant.health = 300; plant.owner = 1; plant.packetId = 17; plant.randomOrigin = 1;
    plant.attackCountdown = 3.0f; plant.produceCountdown = 5.0f;
    state.plants.push_back(plant);
    FusionZombieState zombie;
    zombie.id = 6; zombie.row = 2; zombie.x = 6.0f;
    state.zombies.push_back(zombie);
    FusionProjectileState projectile;
    projectile.id = 7; projectile.type = 8; projectile.row = 2;
    projectile.x = 1.0f; projectile.y = FusionRowY(2); projectile.damage = 100;
    state.projectiles.push_back(projectile);
    FusionSunState sun;
    sun.id = 8; sun.row = 2; sun.x = 1.0f; sun.y = sun.startY = FusionRowY(2);
    sun.amount = 5; sun.scale = 0.5f;
    state.suns.push_back(sun);
    return state;
}
}

FusionWireTestResult RunFusionWireTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result = {};
    std::printf("[ RUN      ] Fusion.SnapshotFieldsU16TransactionalBounds\n");
    const auto source = Example();
    CHECK(FusionSimulation::ValidateSnapshot(source));
    NetBuffer encoded;
    CHECK(EncodeFusionSnapshot(source, encoded));
    CHECK(encoded.Size() > 100);
    FusionSimulationState decoded;
    CHECK(DecodeFusionSnapshot(encoded, decoded));
    CHECK(encoded.Remaining() == 0);
    CHECK(decoded.plants.size() == 1 && decoded.plants[0].type == 1000);
    CHECK(decoded.plants[0].firstParent == 0 && decoded.plants[0].secondParent == 1);
    CHECK(decoded.plants[0].owner == 1 && decoded.plants[0].packetId == 17 && decoded.plants[0].randomOrigin == 1);
    CHECK(decoded.sun == 350 && decoded.columns == 10 && decoded.rows == 5);
    NetBuffer roundTrip;
    CHECK(EncodeFusionSnapshot(decoded, roundTrip));
    CHECK(roundTrip.mData == encoded.mData);

    for (size_t length = 0; length < encoded.Size(); ++length)
    {
        NetBuffer truncated;
        truncated.mData.assign(encoded.mData.begin(), encoded.mData.begin() + length);
        FusionSimulationState previous;
        previous.sun = 12345;
        CHECK(!DecodeFusionSnapshot(truncated, previous));
        CHECK(truncated.mReadPos == 0 && previous.sun == 12345 && previous.plants.empty());
    }
    for (size_t byte : {size_t(0), size_t(4)})
    {
        NetBuffer invalid = roundTrip;
        invalid.mData[byte] ^= 0x80;
        FusionSimulationState previous;
        previous.sun = 12345;
        CHECK(!DecodeFusionSnapshot(invalid, previous));
        CHECK(invalid.mReadPos == 0 && previous.sun == 12345);
    }
    NetBuffer trailing = roundTrip;
    trailing.WriteU8(0);
    CHECK(!DecodeFusionSnapshot(trailing, decoded));
    CHECK(trailing.mReadPos == 0);

    NetBuffer oldSunSchema = roundTrip;
    oldSunSchema.mData[4] = 1; oldSunSchema.mData[5] = 0;
    const auto previousSun = decoded.sun;
    CHECK(!DecodeFusionSnapshot(oldSunSchema, decoded));
    CHECK(oldSunSchema.mReadPos == 0 && decoded.sun == previousSun);

    auto malformed = source;
    malformed.plants[0].type = 1192; // Known source ID, ability not implemented by this kernel.
    NetBuffer untouched;
    untouched.WriteU8(0xa5);
    CHECK(!EncodeFusionSnapshot(malformed, untouched));
    CHECK(untouched.Size() == 1 && untouched.mData[0] == 0xa5);
    malformed = source;
    malformed.zombies[0].id = malformed.plants[0].id;
    CHECK(!EncodeFusionSnapshot(malformed, untouched));
    malformed = source;
    malformed.suns[0].x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!EncodeFusionSnapshot(malformed, untouched));
    malformed = source;
    malformed.plants.resize(kFusionMaxPlants + 1);
    CHECK(!EncodeFusionSnapshot(malformed, untouched));

    NetBuffer prefixTooLarge;
    prefixTooLarge.mData.resize(LawnNet::NET_MAX_FRAME - 8, 0x5a);
    const auto prefixBefore = prefixTooLarge.mData;
    CHECK(!EncodeFusionSnapshot(source, prefixTooLarge));
    CHECK(prefixTooLarge.mData == prefixBefore);
    NetBuffer badCount = roundTrip;
    badCount.mData[28] = 0xff; badCount.mData[29] = 0xff;
    CHECK(!DecodeFusionSnapshot(badCount, decoded));
    CHECK(badCount.mReadPos == 0 && decoded.plants[0].type == 1000);
    NetBuffer badBoolean = roundTrip;
    badBoolean.mData[107] = 2;
    CHECK(!DecodeFusionSnapshot(badBoolean, decoded));
    CHECK(badBoolean.mReadPos == 0);
    NetBuffer unsupported = roundTrip;
    unsupported.mData[34] = 0xa8; unsupported.mData[35] = 0x04; // 1192, U16 LE
    CHECK(!DecodeFusionSnapshot(unsupported, decoded));
    CHECK(unsupported.mReadPos == 0 && decoded.plants[0].type == 1000);

    NetBuffer offset;
    offset.WriteU8(0x77);
    offset.WriteBytes(roundTrip.Data(), roundTrip.Size());
    offset.mReadPos = 1;
    CHECK(DecodeFusionSnapshot(offset, decoded));
    CHECK(offset.Remaining() == 0 && decoded.plants[0].type == 1000);
    offset.mReadPos = 1;
    offset.mData.pop_back();
    CHECK(!DecodeFusionSnapshot(offset, decoded));
    CHECK(offset.mReadPos == 1 && decoded.plants[0].type == 1000);

    // Primary InitBasic4 supplies these directions only. Layer-aware placement
    // must not synthesize their reverse when the incoming card is a Pot.
    struct PotRecipe { uint16_t body, result; };
    for (const auto sample : {PotRecipe{26,1112},PotRecipe{28,1114},PotRecipe{32,1125}})
    {
        const auto* recipe=FindFusionRecipe(27,sample.body);
        CHECK(recipe && recipe->result==sample.result);
        CHECK(!FindFusionRecipe(sample.body,27));
    }
    CHECK(!FindFusionRecipe(1112,26)); // Native CanPlantAt may retain that Under and add a normal Cabbage.

    // The existing native token field now accompanies Plant commands too.
    // Generic reference IDs remainU32; the native host validates its own
    // U16 ID plus exact live generation before allocation or recharge.
    FusionCommand placement;
    placement.epoch=7; placement.inputEpoch=11; placement.sequence=13;
    placement.action=FusionCommandKind::Plant; placement.packetId=23;
    placement.column=9; placement.row=4; placement.targetId=321; placement.targetToken=0x12340004u;
    NetBuffer placementWire;
    CHECK(EncodeFusionCommand(placement,placementWire) && placementWire.Size()==31);
    FusionCommand placementCopy;
    NetBuffer placementInput=placementWire;
    CHECK(DecodeFusionCommand(placementInput,placementCopy) && placementInput.Remaining()==0 &&
        placementCopy.packetId==23 && placementCopy.column==9 && placementCopy.row==4 &&
        placementCopy.targetId==321 && placementCopy.targetToken==0x12340004u);
    NetBuffer placementRoundTrip;
    CHECK(EncodeFusionCommand(placementCopy,placementRoundTrip) && placementRoundTrip.mData==placementWire.mData);
    for (bool direct : {false,true})
    {
        auto reference=placement;
        reference.targetId=direct?0:0xffffffffu; reference.targetToken=0;
        NetBuffer encoded;
        CHECK(EncodeFusionCommand(reference,encoded));
        FusionCommand copy;
        CHECK(DecodeFusionCommand(encoded,copy) && copy.targetId==reference.targetId && copy.targetToken==0);
    }
    for (size_t length=0;length<placementWire.Size();++length)
    {
        NetBuffer prefixed; prefixed.WriteU8(0x77);
        if (length) prefixed.WriteBytes(placementWire.Data(),length);
        prefixed.mReadPos=1;
        FusionCommand previous; previous.epoch=999; previous.targetId=42; previous.targetToken=777;
        CHECK(!DecodeFusionCommand(prefixed,previous));
        CHECK(prefixed.mReadPos==1 && previous.epoch==999 && previous.targetId==42 && previous.targetToken==777);
    }
    for (int field=0;field<4;++field)
    {
        auto invalid=placement;
        if (field==0) invalid.targetId=0; // A token cannot describe an empty direct placement.
        if (field==1) invalid.packetId=0;
        if (field==2) invalid.column=32;
        if (field==3) invalid.row=10;
        NetBuffer unchanged; unchanged.WriteU8(0x5a);
        CHECK(!EncodeFusionCommand(invalid,unchanged) && unchanged.Size()==1 && unchanged.mData[0]==0x5a);
    }
    NetBuffer orphanedToken=placementWire;
    for (size_t byte=23;byte<27;++byte) orphanedToken.mData[byte]=0; // TargetID0, unchanged nonzero token.
    NetBuffer prefixedCommand; prefixedCommand.WriteU8(0x77);
    prefixedCommand.WriteBytes(orphanedToken.Data(),orphanedToken.Size()); prefixedCommand.mReadPos=1;
    FusionCommand previousCommand; previousCommand.epoch=999; previousCommand.targetId=42; previousCommand.targetToken=777;
    CHECK(!DecodeFusionCommand(prefixedCommand,previousCommand));
    CHECK(prefixedCommand.mReadPos==1 && previousCommand.epoch==999 && previousCommand.targetId==42 && previousCommand.targetToken==777);
    // Authenticated native plant activation uses the same opaque full-generation identity.
    FusionCommand activation; activation.epoch=19; activation.inputEpoch=7; activation.sequence=11;
    activation.action=FusionCommandKind::ActivatePlant; activation.sourceId=42; activation.sourceToken=0x1234002au;
    NetBuffer activationWire;
    CHECK(EncodeFusionCommand(activation,activationWire));
    FusionCommand activationCopy;
    CHECK(DecodeFusionCommand(activationWire,activationCopy) && activationWire.Remaining()==0 &&
        activationCopy.action==FusionCommandKind::ActivatePlant && activationCopy.sourceId==42 &&
        activationCopy.sourceToken==0x1234002au && activationCopy.inputEpoch==7);
    for (int field=0;field<2;++field)
    {
        auto invalid=activation; if(field) invalid.sourceToken=0; else invalid.sourceId=0;
        NetBuffer previous;previous.WriteU8(0x77);
        CHECK(!EncodeFusionCommand(invalid,previous) && previous.Size()==1 && previous.mData[0]==0x77);
    }
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.Wire\n" : "[       OK ] Fusion.Wire\n");
    return result;
}
