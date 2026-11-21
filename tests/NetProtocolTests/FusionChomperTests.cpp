#include "FusionChomperTests.h"
#include "../../Lawn/Fusion/FusionChomperRules.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include "../../Lawn/Fusion/FusionProducerRules.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionChomperTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Chomper check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
bool Same(const LawnFusion::FusionChomperTiming& a, const LawnFusion::FusionChomperTiming& b)
{ return a.bite == b.bite && a.chew == b.chew && a.biteEnd == b.biteEnd && a.swallow == b.swallow; }
}

FusionChomperTestResult RunFusionChomperTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.ChomperSourceProfilesAndCallbacks\n");
    // Exact source/controller vectors, not a second implementation of ceil.
    const std::array<float,3> speeds = {.9f,1.0f,1.1f};
    const std::array<FusionChomperTiming,3> old5 = {{{73,95,112,290},{65,85,100,261},{60,78,91,237}}};
    const std::array<FusionChomperTiming,3> nut = {{{136,213,260,139},{122,192,234,125},{111,175,213,114}}};
    for (size_t i = 0; i < speeds.size(); ++i)
    {
        FusionChomperTiming actual, wrapper;
        CHECK(TryGetFusionChomperTiming(5,speeds[i],actual) && Same(actual,old5[i]));
        CHECK(TryGetFusionChomperTiming(speeds[i],wrapper) && Same(actual,wrapper));
        CHECK(TryGetFusionChomperTiming(1012,speeds[i],actual) && Same(actual,nut[i]));
        for (uint16_t kind : {uint16_t(1014),uint16_t(1016)})
            CHECK(TryGetFusionChomperTiming(kind,speeds[i],actual) && Same(actual,old5[i]));
        CHECK(actual.swallow > FindFusionChomperProfile(1014)->sunDelayTicks);
    }
    CHECK(kFusionChomperDigestTicks == 3000);
    CHECK(FindFusionChomperProfile(5)->digestTicks == 3000);
    for (uint16_t kind : {uint16_t(1012),uint16_t(1014),uint16_t(1016)})
        CHECK(FindFusionChomperProfile(kind)->digestTicks == 4000);
    const auto* profile = FindFusionChomperProfile(1012);
    CHECK(profile->biteSeconds == 1.2166666984558105);
    CHECK(profile->chewSeconds == 1.9166666269302368);
    CHECK(profile->biteDuration == 2.3333332538604736);
    CHECK(profile->swallowStateSpeed == .80000001192092896);
    CHECK(profile->maxHealth == 4000 && profile->idleStateSpeed == static_cast<double>(.8f));
    int ticks = 0;
    CHECK(TryGetFusionChomperChewTicks(5,1,ticks) && ticks == 117);
    CHECK(TryGetFusionChomperChewTicks(1012,1,ticks) && ticks == 100);
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    for (uint16_t kind : {uint16_t(1013),uint16_t(0),uint16_t(65535)})
    {
        CHECK(!FindFusionChomperProfile(kind));
        FusionChomperTiming actual{11,12,13,14};
        CHECK(!TryGetFusionChomperTiming(kind,1,actual) && Same(actual,{11,12,13,14}));
    }
    for (float speed : {0.0f,.89f,1.11f,nan,(std::numeric_limits<float>::infinity)()})
    {
        FusionChomperTiming actual{11,12,13,14};
        CHECK(!TryGetFusionChomperTiming(1012,speed,actual) && Same(actual,{11,12,13,14}));
        ticks = 99;
        CHECK(!TryGetFusionChomperChewTicks(1014,speed,ticks) && ticks == 99);
    }

    // Recover is after successful digestion, max-clamped and never resurrection.
    int recovered = 9;
    CHECK(TryRecoverFusionChomper(1012,1,4000,recovered) && recovered == 1001);
    CHECK(TryRecoverFusionChomper(1012,3000,4000,recovered) && recovered == 4000);
    CHECK(TryRecoverFusionChomper(1012,3500,4000,recovered) && recovered == 4000);
    CHECK(TryRecoverFusionChomper(1012,4000,4000,recovered) && recovered == 4000);
    for (const auto& invalid : std::array<std::array<int,3>,6>{{{5,100,300},{1014,100,300},
        {1012,0,4000},{1012,-1,4000},{1012,4001,4000},{1012,1,300}}})
    {
        recovered = 9;
        CHECK(!TryRecoverFusionChomper(static_cast<uint16_t>(invalid[0]),invalid[1],invalid[2],recovered) && recovered == 9);
    }

    // Actual no-write boundaries retain history, including recovery to an
    // exact boundary. Stateless thresholds would lose both source cases.
    uint8_t stage = 0;
    CHECK(TryAdvanceFusionChomperDamageStage(1012,333,stage,stage) && stage == 2);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,1333,stage,stage) && stage == 2);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,1666,stage,stage) && stage == 1);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,2666,stage,stage) && stage == 1);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,2667,stage,stage) && stage == 0);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,1333,stage,stage) && stage == 0);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,1332,stage,stage) && stage == 2);
    CHECK(!TryAdvanceFusionChomperDamageStage(1014,1333,stage,stage) && stage == 2);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,-1,stage,stage) && stage == 2);
    CHECK(TryAdvanceFusionChomperDamageStage(1012,4001,stage,stage) && stage == 0);

    // One delayed callback, current native counter only; capacity consumes it
    // even when the accepted whole-batch-omit adaptation has insufficient slots.
    for (int available : {0,3,4,5})
    {
        int pending = 200, count = -1, emissions = 0, credited = 0;
        for (int elapsed = 1; elapsed <= 400; ++elapsed)
        {
            CHECK(AdvanceFusionChomperSunReward(1014,true,pending,count));
            if (elapsed < 200) CHECK(count == 0 && pending == 200 - elapsed);
            if (elapsed == 200) CHECK(count == 4 && pending == 0);
            if (count)
            {
                ++emissions;
                if (FusionProducerCanFitCallback(100-available,100,count)) credited += count * 25;
            }
            if (elapsed > 200) CHECK(count == 0 && pending == 0);
        }
        CHECK(emissions == 1 && credited == (available >= 4 ? 100 : 0));
    }
    int pending = 17, count = -1;
    CHECK(AdvanceFusionChomperSunReward(1014,false,pending,count) && pending == 0 && count == 0);
    for (int i = 0; i < 250; ++i)
        CHECK(AdvanceFusionChomperSunReward(1014,true,pending,count) && !count && !pending);
    for (int badPending : {-1,201})
    {
        pending = badPending; count = 77;
        CHECK(!AdvanceFusionChomperSunReward(1014,true,pending,count) && pending == badPending && count == 77);
    }
    pending = 200; count = 77;
    CHECK(!AdvanceFusionChomperSunReward(1012,true,pending,count) && pending == 200 && count == 77);

    NativeBlastSpec blast;
    CHECK(TryMakeFusionChomperBlast(1016,400,300,2,blast));
    CHECK(std::abs(blast.centerX - 484.26666666666665) < 1e-10);
    CHECK(std::abs(blast.centerY - 279.64071856287427) < 1e-10);
    CHECK(blast.policy == FusionDeathBlastPolicy::Cherry && blast.damage == 1800 && blast.shake);
    CHECK(FusionDeathBlastAffectsRow(blast,1) && FusionDeathBlastAffectsRow(blast,2) && FusionDeathBlastAffectsRow(blast,3));
    CHECK(!FusionDeathBlastAffectsRow(blast,0) && !FusionDeathBlastAffectsRow(blast,4));
    CHECK(TryMakeFusionChomperBlast(1016,400,300,5,blast) && blast.row == 5 &&
        FusionDeathBlastAffectsRow(blast,4) && FusionDeathBlastAffectsRow(blast,5) && !FusionDeathBlastAffectsRow(blast,6));
    const auto before = blast;
    CHECK(!TryMakeFusionChomperBlast(1014,400,300,2,blast) && blast.centerX == before.centerX);
    CHECK(!TryMakeFusionChomperBlast(1016,nan,300,2,blast) && blast.centerY == before.centerY);
    CHECK(!TryMakeFusionChomperBlast(1016,400,300,6,blast) && blast.row == before.row);
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.ChomperSourceProfilesAndCallbacks\n" :
        "[       OK ] Fusion.ChomperSourceProfilesAndCallbacks\n");
    return result;
}
