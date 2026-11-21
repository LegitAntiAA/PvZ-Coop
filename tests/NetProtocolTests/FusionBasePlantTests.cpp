#include "FusionBasePlantTests.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionBasePlantTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    BasePlant check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionBasePlantTestResult RunFusionBasePlantTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.BasePlantSourceRulesAndWire\n");
    CHECK(GetNativeSeedForPlant(5) == SEED_CHOMPER && GetNativeSeedForPlant(6) == SEED_PUFFSHROOM && GetNativeSeedForPlant(7) == SEED_FUMESHROOM);
    CHECK(GetNativeSeedForPlant(8) == SEED_HYPNOSHROOM);
    CHECK(GetNativeSeedForPlant(65000) == SEED_NONE);
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    FusionChomperTiming timing;
    CHECK(TryGetFusionChomperTiming(1, timing) && timing.bite == 65 && timing.chew == 85 && timing.biteEnd == 100 && timing.swallow == 261);
    CHECK(kFusionChomperDigestTicks == 3000);
    for (float speed : {.9f, 1.0f, 1.1f})
    {
        CHECK(TryGetFusionChomperTiming(speed, timing));
        CHECK(timing.bite < timing.chew && timing.chew < timing.biteEnd);
        CHECK(timing.bite * .01 >= .65 / speed && (timing.bite - 1) * .01 < .65 / speed);
        for (uint16_t kind : {uint16_t(6), uint16_t(7)})
        {
            int counter = 0;
            CHECK(TryGetFusionBaseShotCounter(kind, speed, counter));
            const double event = (kind == 6 ? .38333333 : .35) / speed;
            CHECK((counter - 1) * .01 >= event && (counter - 2) * .01 < event);
            CHECK(FusionBaseShootDuration(kind, speed) > event);
            CHECK(FusionNativeAnimationSpeedValid(kind, speed));
        }
        CHECK(FusionNativeAnimationSpeedValid(5, speed));
    }
    for (float speed : {0.0f, .89f, 1.11f, nan})
    {
        int counter = 77;
        CHECK(!TryGetFusionBaseShotCounter(6, speed, counter) && counter == 77);
        CHECK(!TryGetFusionChomperTiming(speed, timing));
        CHECK(!FusionNativeAnimationSpeedValid(5, speed));
    }
    for (uint16_t kind : {uint16_t(0), uint16_t(1), uint16_t(2), uint16_t(3), uint16_t(4), uint16_t(1000)})
        CHECK(FusionNativeAnimationSpeedValid(kind, 1) && !FusionNativeAnimationSpeedValid(kind, 1.01f));
    CHECK(!FusionBaseCanAcquire(6, 0, true, false, false, 0));
    CHECK(FusionBaseCanAcquire(6, 0, true, false, false, .001));
    CHECK(FusionBaseCanAcquire(6, 0, true, false, false, 240 - .001));
    CHECK(!FusionBaseCanAcquire(6, 0, true, false, false, 240));
    const double fumeEnd = 7 * kFusionBasePixelsX;
    CHECK(FusionBaseCanAcquire(7, 0, true, false, false, fumeEnd - .001));
    CHECK(!FusionBaseCanAcquire(7, 0, true, false, false, fumeEnd));
    CHECK(FusionFumeCanHit(0, true, false, false, 0));
    CHECK(FusionFumeCanHit(0, true, false, false, fumeEnd));
    CHECK(!FusionFumeCanHit(0, true, false, false, fumeEnd + .001));
    CHECK(!FusionFumeCanHit(0, false, false, false, 1));
    CHECK(!FusionFumeCanHit(0, true, true, false, 1));
    CHECK(!FusionFumeCanHit(0, true, false, true, 1));
    CHECK(!FusionFumeCanHit(0, true, false, false, nan));
    for (int status : {1,3,7,9,10,12,16,20,21,22,23,24})
        CHECK(!FusionBaseCanAcquire(6, status, true, false, false, 1));
    CHECK(FusionChomperCanAcquire(0, true, false, false));
    for (int status : {1,7,10,12,16,20,21,22,23,24}) CHECK(!FusionChomperCanAcquire(status, true, false, false));
    CHECK(!FusionChomperCanAcquire(0, false, false, false));
    CHECK(!FusionChomperCanAcquire(0, true, true, false));
    CHECK(!FusionChomperCanAcquire(0, true, false, true));
    CHECK(!FusionPuffLifetimeExpired(74) && !FusionPuffLifetimeExpired(75) && FusionPuffLifetimeExpired(76));
    FusionBaseRect rect;
    CHECK(TryMakeFusionChomperRect(100, 100, rect));
    CHECK(std::abs(rect.x - 140) < 1e-9 && std::abs(rect.width - 80) < 1e-9);
    CHECK(std::abs(rect.y + rect.height * .5 - (100 - .5 * kFusionBasePixelsY)) < 1e-9);
    CHECK(TryMakeFusionPuffRect(100, 100, rect));
    CHECK(std::abs(rect.width - .32 * kFusionBasePixelsX) < 1e-9);
    CHECK(std::abs(rect.y + rect.height * .5 - (100 + .08048099 * kFusionBasePixelsY)) < 1e-9);
    CHECK(!TryMakeFusionPuffRect(nan, 100, rect));

    const auto* iceFume=FindFusionFumeProfile(1037);
    const auto* hypnoFume=FindFusionFumeProfile(1023);
    CHECK(iceFume && iceFume->coldLevelsAfterDamage==5 && !iceFume->hypnotize);
    CHECK(hypnoFume && hypnoFume->hypnotize && hypnoFume->coldLevelsAfterDamage==0);
    CHECK(!FindFusionFumeProfile(1172));
    CHECK(FusionBaseCanAcquire(1037,0,true,false,false,fumeEnd-.001));
    CHECK(!FusionBaseCanAcquire(1023,0,true,false,false,fumeEnd));
    const auto* sourceFume=FindFusionBaseTimingProfile(1037);
    CHECK(sourceFume && sourceFume->eventClipSeconds[0]==static_cast<double>(.35f));
    CHECK(FindFusionBaseTimingProfile(7)->eventClipSeconds[0]==.35); // Original7 decimal double remains exact.
    float threshold=77;
    CHECK(TryGetFusionHypnoFumeThreshold(300,300,0,0,0,threshold) && threshold==1);
    CHECK(TryGetFusionHypnoFumeThreshold(75,300,0,0,0,threshold) && threshold==.75f);
    CHECK(TryGetFusionHypnoFumeThreshold(150,300,0,0,127,threshold) && threshold==.8062257766723633f);
    CHECK(TryGetFusionHypnoFumeThreshold(0,300,0,0,127,threshold) && std::isnan(threshold));
    threshold=77;
    CHECK(!TryGetFusionHypnoFumeThreshold(300,0,0,0,0,threshold) && threshold==77);
    CHECK(!TryGetFusionHypnoFumeThreshold(300,300,0,0,128,threshold) && threshold==77);
    CHECK(!TryGetFusionHypnoFumeThreshold(300,(std::numeric_limits<int>::max)(),0,1,0,threshold) && threshold==77);

    // A small semantic chooser vector is independent of the production pool.
    const std::vector<FusionBankEligibleCard> eligible = {{5,false,150,7.5f},{6,false,0,7.5f},{7,false,75,7.5f}};
    FusionSeedBank bank;
    CHECK(bank.Open(49,1,eligible,1));
    for (const auto& card : eligible) CHECK(bank.Choose(49,0,card.kind,false));
    CHECK(bank.SetReady(49,0,true) && bank.CommitIntro(49) && bank.BeginPlaying(49));
    NativeSnapshotFrame frame;
    frame.epoch = 49; frame.selectionCommitted = true; frame.selection = bank.State();
    for (const auto& selected : frame.selection.packets)
    {
        NativePacketData packet;
        packet.selectionId=selected.id; packet.kind=selected.kind; packet.owner=selected.owner;
        packet.price=selected.price; packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,packet});
        NativePlantData plant;plant.instanceToken=0x10000u+selected.id; plant.kind=selected.kind; plant.owner=selected.owner; plant.packetId=selected.id; plant.animationSpeed=1.03f;
        frame.plants.push_back({static_cast<uint16_t>(selected.id + 20),plant});
    }
    NativeProjectileData puff; puff.kind=9;
    frame.projectiles.push_back({100,puff});
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    auto invalid = frame;
    invalid.projectiles[0].data.damage=60; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.projectiles[0].data.sunOnHit=5; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.projectiles[0].data.kind=11; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.plants[0].data.shootSeconds=0; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.plants[1].data.shootSeconds=FusionBaseShootDuration(6,1.03f); CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame; invalid.plants[1].data.firstParent=0; CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    LawnNet::NetBuffer body; body.WriteU32(49);
    CHECK(EncodeNativeSnapshot(frame,eligible,body));
    NativeSnapshotFrame decoded; LawnNet::NetBuffer native, input=body;
    CHECK(DecodeNativeSnapshot(input,eligible,49,decoded,native));
    CHECK(decoded.projectiles.size()==1 && decoded.projectiles[0].data.kind==9 && decoded.projectiles[0].data.damage==20);
    CHECK(decoded.plants.size()==3 && decoded.plants[1].data.animationSpeed==1.03f);
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.BasePlantSourceRulesAndWire\n" : "[       OK ] Fusion.BasePlantSourceRulesAndWire\n");
    return result;
}
