#include "FusionButterTests.h"
#include "../../Lawn/Fusion/FusionButterRules.h"
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionButterTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Butter check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
}

FusionButterTestResult RunFusionButterTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.SourceButterDurationOverwriteAndVisibility\n");
    struct Splash { int count, ticks; };
    for (const auto row : {Splash{1,400}, Splash{2,200}, Splash{3,100}, Splash{4,100},
        Splash{5,50}, Splash{8,50}, Splash{1000,50}, Splash{(std::numeric_limits<int>::max)(),50}})
    {
        int ticks = 77;
        CHECK(TryGetFusionButterSplashTicks(row.count, ticks) && ticks == row.ticks);
    }
    for (const int count : {0, -1, (std::numeric_limits<int>::min)()})
    {
        int ticks = 77;
        CHECK(!TryGetFusionButterSplashTicks(count, ticks) && ticks == 77);
    }

    CHECK(FusionButterStateValid(0, false));
    CHECK(!FusionButterStateValid(0, true));
    CHECK(FusionButterStateValid(1, false) && FusionButterStateValid(1, true));
    CHECK(FusionButterStateValid(600, false) && FusionButterStateValid(600, true));
    CHECK(FusionButterStateValid(400, false) && FusionButterStateValid(400, true));
    CHECK(!FusionButterStateValid(-1, false) && !FusionButterStateValid(601, false));
    CHECK(!FusionButterStateValid((std::numeric_limits<int>::min)(), false));
    CHECK(!FusionButterStateValid((std::numeric_limits<int>::max)(), true));

    bool visible = true;
    CHECK(TryPrepareFusionButter(0, false, 50, visible) && !visible); // Fresh half-second has no sprite.
    CHECK(TryPrepareFusionButter(0, false, 51, visible) && visible); // Strict source >.5 threshold.
    CHECK(TryPrepareFusionButter(0, false, 400, visible) && visible);
    CHECK(TryPrepareFusionButter(0, false, 600, visible) && visible);
    CHECK(TryPrepareFusionButter(399, true, 50, visible) && visible); // Existing sprite survives shortening.
    CHECK(TryPrepareFusionButter(1, true, 50, visible) && visible);
    CHECK(TryPrepareFusionButter(30, false, 50, visible) && !visible);
    CHECK(TryPrepareFusionButter(30, false, 100, visible) && visible);
    CHECK(TryPrepareFusionButter(0, false, 50, visible) && !visible); // Actual expiry resets visibility.
    CHECK(TryPrepareFusionButter(0,false,400,visible,false)&&!visible); // Garlic butter suppresses a fresh sprite.
    CHECK(TryPrepareFusionButter(100,true,400,visible,false)&&visible); // An existing source sprite survives.

    for (const int ticks : {0, -1, 601, (std::numeric_limits<int>::max)()})
    {
        visible = true;
        CHECK(!TryPrepareFusionButter(20, true, ticks, visible) && visible);
        visible = false;
        CHECK(!TryPrepareFusionButter(20, false, ticks, visible) && !visible);
    }
    visible = true;
    CHECK(!TryPrepareFusionButter(0, true, 50, visible) && visible);
    CHECK(!TryPrepareFusionButter(-1, false, 100, visible) && visible);
    CHECK(!TryPrepareFusionButter(601, false, 100, visible) && visible);

    std::printf("[ %s ] Fusion.SourceButterDurationOverwriteAndVisibility checks=%d failures=%d\n",
        result.mFailures ? "FAILED" : "OK", result.mChecks, result.mFailures);
    return result;
}
