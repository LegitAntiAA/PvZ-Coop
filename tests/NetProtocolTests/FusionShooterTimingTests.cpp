#include "FusionShooterTimingTests.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionNative.h"
#include <cstdio>
#include <limits>

namespace
{
FusionShooterTimingTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    ShooterTiming check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionShooterTimingTestResult RunFusionShooterTimingTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.SharedShooterTimingProfiles\n");
    struct Vector { uint16_t kind; float speed; int counter; float duration; };
    // Goldens from the existing source-bound controllers and pre-extraction
    // counters. The last two expose a one-tick regression if double events
    // are silently narrowed to float while extracting the table.
    const Vector vectors[] = {
        {6,.9f,44,.5555555820465088f},{6,1,40,.5f},{6,1.1f,36,.45454543828964233f},
        {7,.9f,40,1.1111111640930176f},{7,1,36,1},{7,1.1f,33,.9090908765792847f},
        {1001,.9f,39,.5555555820465088f},{1001,1,35,.5f},{1001,1.1f,32,.45454543828964233f},
        {1004,.9f,76,1},{1004,1,76,1},{1004,1.1f,76,1},
        {7,.9722222089767456f,38,0},{6,1.0360360145568848f,39,0},
    };
    for (const auto& v : vectors)
    {
        const auto* profile = FindFusionShooterTimingProfile(v.kind);
        CHECK(profile && FusionShooterTimingProfileValid(*profile));
        if (!profile) continue;
        int counter = 0;
        CHECK(TryGetFusionShooterShotCounter(*profile,v.speed,counter) && counter==v.counter);
        if (v.duration) CHECK(FusionShooterDuration(*profile,v.speed)==v.duration);
        if (v.kind==1001)
        {
            int previous = 0;
            CHECK(TryGetFusionCherryShotCounter(v.speed,previous) && previous==v.counter);
            CHECK(FusionCherryShootDuration(v.speed)==v.duration);
        }
        else if (v.kind==6 || v.kind==7)
        {
            int previous = 0;
            CHECK(TryGetFusionBaseShotCounter(v.kind,v.speed,previous) && previous==v.counter);
            if (v.duration) CHECK(FusionBaseShootDuration(v.kind,v.speed)==v.duration);
        }
    }
    for (uint16_t kind : {uint16_t(6),uint16_t(7),uint16_t(1001),uint16_t(1004)})
    {
        const auto* p = FindFusionShooterTimingProfile(kind);
        CHECK(p);
        if (!p) continue;
        int ticks = 0;
        CHECK(TryGetFusionShooterInitialTicks(*p,.2f,ticks) && ticks==31);
        CHECK(TryGetFusionShooterInitialTicks(*p,.5f,ticks) && ticks==76);
        CHECK(TryGetFusionShooterInitialTicks(*p,.8f,ticks) && ticks==121);
        CHECK(TryGetFusionShooterCycleTicks(*p,.95f,ticks) && ticks==143);
        CHECK(TryGetFusionShooterCycleTicks(*p,1,ticks) && ticks==151);
        CHECK(TryGetFusionShooterCycleTicks(*p,1.05f,ticks) && ticks==158);
    }
    const auto* future = FindFusionShooterTimingProfile(1004);
    CHECK(future && !future->speedParameterActive && future->eventClipSeconds.size()==1 && future->eventClipSeconds[0]==.75 && future->stateSpeed==1);
    CHECK(GetNativeSeedForPlant(1004)==SEED_PEASHOOTER); // Native rolling/pierce ability supplies this alias.
    int counter = 77;
    CHECK(!TryGetFusionBaseShotCounter(1004,1,counter) && counter==77);
    for (uint16_t kind : {uint16_t(0),uint16_t(1),uint16_t(5),uint16_t(1000),uint16_t(1002),uint16_t(65535)})
        CHECK(!FindFusionShooterTimingProfile(kind));
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    const float infinity = (std::numeric_limits<float>::infinity)();
    const auto& p = *FindFusionShooterTimingProfile(1001);
    for (float speed : {0.0f,.89f,1.11f,nan,infinity})
    {
        counter=77;
        CHECK(!TryGetFusionShooterShotCounter(p,speed,counter) && counter==77);
        CHECK(FusionShooterDuration(p,speed)==0);
    }
    for (float fraction : {.19f,.81f,nan,infinity})
    { counter=77; CHECK(!TryGetFusionShooterInitialTicks(p,fraction,counter) && counter==77); }
    for (float multiplier : {.94f,1.06f,nan,infinity})
    { counter=77; CHECK(!TryGetFusionShooterCycleTicks(p,multiplier,counter) && counter==77); }
    auto malformed = p;
    malformed.stateSpeed=0; counter=77;
    CHECK(!FusionShooterTimingProfileValid(malformed));
    CHECK(!TryGetFusionShooterShotCounter(malformed,1,counter) && counter==77);
    malformed=p; malformed.attackIntervalSeconds=nan;
    CHECK(!TryGetFusionShooterInitialTicks(malformed,.5f,counter) && counter==77);
    const double pastEnd[] = {2};
    malformed=p; malformed.eventClipSeconds=pastEnd;
    CHECK(!FusionShooterTimingProfileValid(malformed));
    malformed=p; malformed.clipDurationSeconds=(std::numeric_limits<double>::infinity)();
    CHECK(!FusionShooterTimingProfileValid(malformed));
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.SharedShooterTimingProfiles\n" : "[       OK ] Fusion.SharedShooterTimingProfiles\n");
    return result;
}
