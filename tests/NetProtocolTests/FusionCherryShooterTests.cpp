#include "FusionCherryShooterTests.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionCherryShooterTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    CherryShooter check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionCherryShooterTestResult RunFusionCherryShooterTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result = {};
    std::printf("[ RUN      ] Fusion.CherryShooterSourceRulesAndWire\n");
    int counter = 0;
    CHECK(TryGetFusionCherryInitialTicks(0.2f,counter) && counter == 31);
    CHECK(TryGetFusionCherryInitialTicks(0.5f,counter) && counter == 76);
    CHECK(TryGetFusionCherryInitialTicks(0.8f,counter) && counter == 121);
    CHECK(TryGetFusionCherryCycleTicks(0.95f,counter) && counter == 143);
    CHECK(TryGetFusionCherryCycleTicks(1.0f,counter) && counter == 151);
    CHECK(TryGetFusionCherryCycleTicks(1.05f,counter) && counter == 158);
    struct Shot { float speed; int initial; };
    for (const auto sample : {Shot{0.9f,39},Shot{1.0f,35},Shot{1.1f,32}})
    {
        CHECK(TryGetFusionCherryShotCounter(sample.speed,counter) && counter == sample.initial);
        const int elapsedTicks = counter-1;
        const double sourceSeconds = static_cast<double>(kFusionCherryShooterEvent)/(2.0*sample.speed);
        CHECK(elapsedTicks*.01 >= sourceSeconds && (elapsedTicks-1)*.01 < sourceSeconds);
        // The actual native counter decrements first and fires at1. It must
        // cross the source event once, never on the preceding native tick.
        for (int tick = 1; tick <= elapsedTicks; ++tick)
        {
            --counter;
            CHECK((counter == 1) == (tick == elapsedTicks));
        }
    }
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    const float infinity = (std::numeric_limits<float>::infinity)();
    for (float invalid : {-1.0f,0.0f,0.89f,1.11f,nan,infinity})
    {
        counter = 77;
        CHECK(!TryGetFusionCherryShotCounter(invalid,counter) && counter == 77);
        CHECK(!FusionNativeAnimationSpeedValid(1001,invalid));
    }
    counter = 77;
    CHECK(!TryGetFusionCherryInitialTicks(0.19f,counter) && counter == 77);
    CHECK(!TryGetFusionCherryCycleTicks(1.06f,counter) && counter == 77);
    for (uint16_t oldKind : {0,1,2,3,4,1000})
    {
        CHECK(FusionNativeAnimationSpeedValid(oldKind,1.0f));
        CHECK(!FusionNativeAnimationSpeedValid(oldKind,1.01f));
    }
    CHECK(GetNativeSeedForPlant(1001) == SEED_PEASHOOTER && GetNativeSeedForPlant(1002) == SEED_CHERRYBOMB);
    CHECK(GetNativeSeedForPlant(3) == SEED_WALLNUT && GetNativeSeedForPlant(65534) == SEED_NONE);
    CHECK(FusionCherryProjectileCanHit(0,true,false,270));
    CHECK(FusionCherryProjectileCanHit(1,true,false,1)); // collision differs from acquisition
    for (int status : {3,7,9,10,12,13,16,20,21}) CHECK(!FusionCherryProjectileCanHit(status,true,false,270));
    CHECK(!FusionCherryProjectileCanHit(0,false,false,270));
    CHECK(!FusionCherryProjectileCanHit(0,true,true,270));
    CHECK(!FusionCherryProjectileCanHit(0,true,false,0));
    CHECK(!FusionCherryProjectileCanHit(0,true,false,nan));
    FusionCherryProjectileRect rect;
    CHECK(TryMakeFusionCherryProjectileRect(100,100,rect));
    CHECK(std::abs(rect.width-22.72) < 1e-9);
    CHECK(std::abs(rect.x+rect.width*.5-100) < 1e-9);
    CHECK(std::abs(rect.height-1.9312102*.2*100/1.67) < 1e-9);
    const auto savedRect = rect;
    CHECK(!TryMakeFusionCherryProjectileRect(nan,100,rect) && rect.x == savedRect.x);
    for (double x : {-12.0,11.2}) CHECK(!FusionCherryProjectileExpired(x,0,2000,-10,9.2,-4.98,6.5));
    CHECK(FusionCherryProjectileExpired(-12.0001,0,0,-10,9.2,-4.98,6.5));
    CHECK(FusionCherryProjectileExpired(11.2001,0,0,-10,9.2,-4.98,6.5));
    CHECK(!FusionCherryProjectileExpired(0,-4.98,2000,-10,9.2,-4.98,6.5));
    CHECK(!FusionCherryProjectileExpired(0,6.5,2000,-10,9.2,-4.98,6.5));
    CHECK(FusionCherryProjectileExpired(0,0,2001,-10,9.2,-4.98,6.5));
    CHECK(FusionCherryProjectileExpired(nan,0,0,-10,9.2,-4.98,6.5));

    // Hybrid1001 remains a fusion result; all eight real base identities stay
    // in the chooser/random pool, without filtering out explosive cards.
    const std::vector<FusionBankEligibleCard> eligible = {{0,false,100,7.5f},{0,true,200,7.5f},
        {1,false,50,7.5f},{1,true,100,7.5f},{2,false,150,50},{2,true,300,50},{4,false,25,30},{4,true,50,30}};
    FusionSeedBank bank;
    CHECK(bank.Open(4242,1,eligible,3));
    for (FusionPlantId kind : {FusionPlantId(0),FusionPlantId(1),FusionPlantId(2),FusionPlantId(4)})
    { CHECK(bank.Choose(4242,0,kind,false)); CHECK(bank.Choose(4242,1,kind,true)); }
    CHECK(bank.SetReady(4242,0,true) && bank.SetReady(4242,1,true) && bank.CommitIntro(4242) && bank.BeginPlaying(4242));
    NativeSnapshotFrame frame;
    frame.epoch = 4242; frame.selectionCommitted = true; frame.selection = bank.State();
    uint16_t usedPeaPacket = 0;
    for (const auto& selected : frame.selection.packets)
    {
        NativePacketData packet;
        packet.selectionId=selected.id; packet.kind=selected.kind; packet.isExtra=selected.isExtra;
        packet.owner=selected.owner; packet.randomOrigin=selected.randomOrigin; packet.price=selected.price; packet.rechargeSeconds=selected.rechargeSeconds;
        const uint16_t nativeId = static_cast<uint16_t>(selected.id+100);
        frame.packets.push_back({nativeId,packet});
        if (packet.kind == 0 && !packet.isExtra) usedPeaPacket = nativeId;
    }
    NativePlantData plant;plant.instanceToken=0x10001u;
    plant.kind=1001; plant.firstParent=2; plant.secondParent=0; plant.packetId=usedPeaPacket; plant.owner=0;
    plant.animationSpeed=1.03f; plant.idleSeconds=2; plant.shootSeconds=.1f;
    frame.plants.push_back({201,plant});
    NativeProjectileData projectile; projectile.kind=1; projectile.damage=60; projectile.sunOnHit=0;
    frame.projectiles.push_back({202,projectile});
    CHECK(usedPeaPacket && ValidateNativeSnapshotFrame(frame,eligible));
    auto invalid = frame;
    invalid.plants[0].data.firstParent=0; invalid.plants[0].data.secondParent=2;
    CHECK(ValidateNativeSnapshotFrame(invalid,eligible));
    invalid.plants[0].data.secondParent=1;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    for (float speed : {nan,infinity,0.0f,0.89f,1.11f})
    { invalid=frame; invalid.plants[0].data.animationSpeed=speed; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible)); }
    invalid=frame; invalid.plants[0].data.productionTicks=1; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.plants[0].data.shootSeconds=-.1f; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.projectiles[0].data.damage=61; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.projectiles[0].data.sunOnHit=5; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.projectiles[0].data.kind=3; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    NetBuffer wire; wire.WriteU32(0x12345678);
    CHECK(EncodeNativeSnapshot(frame,eligible,wire));
    CHECK(wire.Size()>6 && wire.mData[4]==28 && wire.mData[5]==0);
    NativeSnapshotFrame decoded; NetBuffer body,input=wire;
    CHECK(DecodeNativeSnapshot(input,eligible,4242,decoded,body) && input.Remaining()==0 && body.Size()==4);
    CHECK(decoded.plants[0].data.animationSpeed == plant.animationSpeed && decoded.plants[0].data.shootSeconds == .1f);
    CHECK(decoded.projectiles[0].data.kind == 1 && decoded.projectiles[0].data.damage == 60);
    for (size_t length=0;length<wire.Size();++length)
    {
        NetBuffer cut; cut.mData.assign(wire.mData.begin(),wire.mData.begin()+length);
        NativeSnapshotFrame previous; previous.epoch=7; NetBuffer previousBody; previousBody.WriteU8(99);
        CHECK(!DecodeNativeSnapshot(cut,eligible,4242,previous,previousBody));
        CHECK(cut.mReadPos==0 && previous.epoch==7 && previousBody.Size()==1 && previousBody.mData[0]==99);
    }
    input=wire; input.mData[4]=2;
    NativeSnapshotFrame previous; previous.epoch=7; NetBuffer previousBody; previousBody.WriteU8(99);
    CHECK(!DecodeNativeSnapshot(input,eligible,4242,previous,previousBody));
    CHECK(input.mReadPos==0 && previous.epoch==7 && previousBody.Size()==1);
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.CherryShooterSourceRulesAndWire\n" : "[       OK ] Fusion.CherryShooterSourceRulesAndWire\n");
    return result;
}
