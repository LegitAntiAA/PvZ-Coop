#include "FusionCherryTests.h"
#include "../../Lawn/Fusion/FusionCherryRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionCherryTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Fusion Cherry check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionCherryTestResult RunFusionCherryTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result = {};
    std::printf("[ RUN      ] Fusion.CherrySourceRulesAndWire\n");
    // Source0x180380E60/0x1803854C0. These are source-derived thresholds,
    // including body+first=1260/720 and large second armor (which is ignored).
    // Actual native shield/helmet/body mutation is tested by the game fixture.
    struct Case { int difficulty; float body; int first, second; bool burn; int damage; };
    const Case cases[] = {
        {2, 270, 5000, 5000, true, 1800}, {2, 1800, 5000, 5000, true, 1800},
        {2, 1801, 0, 0, false, 1800}, {2, 2000, 1000, 4000, false, 1800},
        {4, 270, 990, 5000, true, 1260}, {4, 270, 991, 5000, false, 1260},
        {4, 270, 1000, 5000, false, 1260}, {5, 270, 450, 5000, true, 720},
        {5, 270, 451, 5000, false, 720}, {5, 1000, 0, 0, false, 720},
        {4, 1000, 500, 2000, false, 1260}, {2, 3000, 5000, 10000, false, 1800}
    };
    for (const auto& sample : cases)
    {
        FusionCherryDamage damage;
        CHECK(TryResolveFusionCherryDamage(sample.difficulty, sample.body, sample.first, damage));
        CHECK(damage.instantBurn == sample.burn && damage.damage == sample.damage);
    }
    for (int difficulty : {0, 1, 2, 3})
    {
        FusionCherryDamage damage;
        CHECK(TryResolveFusionCherryDamage(difficulty, 1800, 100000, damage) && damage.instantBurn);
    }
    for (int invalid : {-1, 6, 10, 255})
    {
        FusionCherryDamage previous{true, 77};
        CHECK(!TryResolveFusionCherryDamage(invalid, 100000, 0, previous));
        CHECK(previous.instantBurn && previous.damage == 77);
    }
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    const float infinity = (std::numeric_limits<float>::infinity)();
    for (float invalid : {-1.0f, nan, infinity})
    {
        FusionCherryDamage previous{true, 77};
        CHECK(!TryResolveFusionCherryDamage(2, invalid, 0, previous));
        CHECK(previous.instantBurn && previous.damage == 77);
    }
    FusionCherryDamage previous{true, 77};
    CHECK(!TryResolveFusionCherryDamage(2, 270, -1, previous) && previous.damage == 77);
    CHECK(kFusionSourceNormalDifficulty == 2 && kFusionCherryFuseTicks == 100);
    CHECK(GetNativeSeedForPlant(0) == SEED_PEASHOOTER && GetNativeSeedForPlant(1) == SEED_SUNFLOWER &&
        GetNativeSeedForPlant(2) == SEED_CHERRYBOMB && GetNativeSeedForPlant(3) == SEED_WALLNUT && GetNativeSeedForPlant(4) == SEED_POTATOMINE && GetNativeSeedForPlant(1000) == SEED_PEASHOOTER);
    CHECK(GetNativeSeedForPlant(33) == SEED_PEASHOOTER && GetNativeSeedForPlant(1177) == SEED_PEASHOOTER);
    CHECK(GetNativeSeedForPlant(937)==SEED_UMBRELLA);
    for (FusionPlantId invalid : {FusionPlantId(65534), FusionPlantId(938),
        FusionPlantId(65532), FusionPlantId(65533), FusionPlantId(4096), kFusionNoPlant})
        CHECK(GetNativeSeedForPlant(invalid) == SEED_NONE);
    CHECK(GetNativeSeedForPlant(1003) == SEED_WALLNUT && GetNativeSeedForPlant(1010) == SEED_WALLNUT &&
        GetNativeSeedForPlant(1162) == SEED_WALLNUT);

    // Current canonical col9,row4 cell center is724,530 (lawn top80).
    FusionCherryArea area;
    CHECK(TryMakeFusionCherryArea(724, 530, area));
    CHECK(std::abs(area.centerX - 725.76) < 1e-9 && std::abs(area.centerY - 488.6167664670659) < 1e-9);
    const double rx = 106.66666666666667, ry = 119.76047904191617;
    CHECK(FusionCherryRectOverlaps(area, area.centerX, area.centerY, 0, 0));
    CHECK(FusionCherryRectOverlaps(area, area.centerX+rx, area.centerY, 0, 0));
    CHECK(!FusionCherryRectOverlaps(area, area.centerX+rx+.001, area.centerY, 0, 0));
    CHECK(FusionCherryRectOverlaps(area, area.centerX, area.centerY+ry, 0, 0));
    CHECK(!FusionCherryRectOverlaps(area, area.centerX, area.centerY+ry+.001, 0, 0));
    CHECK(FusionCherryLadderOverlaps(area, 724, 530));
    CHECK(!FusionCherryLadderOverlaps(area, 508, 530));
    CHECK(!FusionCherryRectOverlaps(area, 0, 0, -1, 20));
    CHECK(!FusionCherryRectOverlaps(area, nan, 0, 20, 20));
    FusionCherryArea retained{3, 4};
    CHECK(!TryMakeFusionCherryArea(infinity, 530, retained) && retained.centerX == 3 && retained.centerY == 4);

    // Same native wire schema, now with an actual supported Cherry packet and
    // actor. Unknown kinds and forged fusion ancestry still reject atomically.
    const std::vector<FusionBankEligibleCard> eligible = {{0,false,100,7.5f},{0,true,200,7.5f},
        {1,false,50,7.5f},{1,true,100,7.5f},{2,false,150,50},{2,true,300,50}};
    FusionSeedBank bank;
    CHECK(bank.Open(35, 10, eligible, 3));
    for (FusionPlantId kind : {FusionPlantId(0),FusionPlantId(1),FusionPlantId(2)})
    { CHECK(bank.Choose(35, 0, kind, false)); CHECK(bank.Choose(35, 1, kind, true)); }
    CHECK(bank.SetReady(35,0,true) && bank.SetReady(35,1,true) && bank.CommitIntro(35) && bank.BeginPlaying(35));
    NativeSnapshotFrame source;
    source.epoch = 35; source.selectionCommitted = true; source.selection = bank.State();
    uint16_t cherryPacket = 0;
    for (const auto& packet : source.selection.packets)
    {
        NativePacketData data;
        data.selectionId=packet.id; data.kind=packet.kind; data.isExtra=packet.isExtra;
        data.owner=packet.owner; data.randomOrigin=packet.randomOrigin; data.price=packet.price; data.rechargeSeconds=packet.rechargeSeconds;
        const uint16_t nativeId = static_cast<uint16_t>(packet.id+100);
        source.packets.push_back({nativeId,data});
        if (data.kind == 2 && !data.isExtra) cherryPacket = nativeId;
    }
    NativePlantData plant;plant.instanceToken=0x10001u;
    plant.kind=2; plant.packetId=cherryPacket; plant.owner=0;
    source.plants.push_back({201,plant});
    CHECK(cherryPacket && ValidateNativeSnapshotFrame(source,eligible));
    NetBuffer wire; wire.WriteU32(0x12345678);
    CHECK(EncodeNativeSnapshot(source,eligible,wire));
    NativeSnapshotFrame decoded; NetBuffer body, input=wire;
    CHECK(DecodeNativeSnapshot(input,eligible,35,decoded,body) && input.Remaining()==0 && body.Size()==4);
    CHECK(decoded.plants.size()==1 && decoded.plants[0].data.kind==2 && decoded.plants[0].data.packetId==cherryPacket);
    auto invalid=source; invalid.plants[0].data.kind=1003;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=source; invalid.plants[0].data.firstParent=0; invalid.plants[0].data.secondParent=2;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    for (size_t length=0;length<wire.Size();++length)
    {
        NetBuffer truncated; truncated.mData.assign(wire.mData.begin(),wire.mData.begin()+length);
        NativeSnapshotFrame saved; saved.epoch=77; NetBuffer savedBody; savedBody.WriteU8(99);
        CHECK(!DecodeNativeSnapshot(truncated,eligible,35,saved,savedBody));
        CHECK(truncated.mReadPos==0 && saved.epoch==77 && savedBody.Size()==1 && savedBody.mData[0]==99);
    }
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.CherrySourceRulesAndWire\n" : "[       OK ] Fusion.CherrySourceRulesAndWire\n");
    return result;
}
