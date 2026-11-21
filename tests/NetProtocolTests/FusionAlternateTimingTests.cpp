#include "FusionAlternateTimingTests.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include <array>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionAlternateTimingTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    AlternateTiming check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
}

FusionAlternateTimingTestResult RunFusionAlternateTimingTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.AlternateStateTimingProfiles\n");
    // The old22 default rows must retain the exact original lookup identity.
    const uint16_t oldKinds[] = {6,7,1001,1004,1030,1008,1032,1017,1018,1019,1065,
        1090,1108,1153,1034,1105,1106,1035,908,26,32,1116};
    for (uint16_t kind : oldKinds)
    {
        const auto* legacy = FindFusionShooterTimingProfile(kind);
        CHECK(legacy);
        if (!legacy) continue;
        CHECK(legacy == FindFusionShooterTimingProfile(kind,legacy->visualState));
        CHECK(!FindFusionShooterTimingProfile(kind,"shoot2"));
        for (float speed : {.9f,1.0f,1.1f})
            CHECK(FusionShooterMaximumDuration(kind,speed) == FusionShooterDuration(*legacy,speed));
    }
    // SOURCE151. Root promotes the six buffered reference rows before registering
    // this suite; profile presence alone still does not enable a native ability.
    struct Vector {uint16_t kind,normal,alternate;double event;};
    const Vector vectors[] = {
        {28,54,55,static_cast<double>(.33333334f)},
        {1113,56,57,static_cast<double>(.31666666f)},
        {1121,66,67,static_cast<double>(.33333334f)}};
    const std::array<float,3> speeds = {.9f,1.0f,1.1f};
    const std::array<int,3> cornCounters = {39,35,32},cabbageCounters = {37,33,30};
    for (const auto& v : vectors)
    {
        const auto* normal = FindFusionShooterTimingProfile(v.kind,"shoot");
        const auto* alternate = FindFusionShooterTimingProfile(v.kind,"shoot2");
        CHECK(normal && alternate);
        if (!normal || !alternate) continue;
        CHECK(FindFusionShooterTimingProfile(v.kind) == normal);
        CHECK(normal != alternate && normal->projectileKind == v.normal && alternate->projectileKind == v.alternate);
        CHECK(normal->eventClipSeconds.size()==1 && normal->eventClipSeconds[0]==v.event);
        CHECK(alternate->eventClipSeconds.size()==1 && alternate->eventClipSeconds[0]==v.event);
        CHECK(normal->attackIntervalSeconds==3 && alternate->attackIntervalSeconds==3);
        CHECK(normal->clipDurationSeconds==1 && alternate->clipDurationSeconds==1);
        CHECK(normal->stateSpeed==1 && alternate->stateSpeed==1 && normal->speedParameterActive);
        CHECK(alternate->speedParameterActive == (v.kind!=1113));
        CHECK(!FindFusionShooterTimingProfile(v.kind,"idle"));
        CHECK(!FindFusionShooterTimingProfile(v.kind,"Shoot"));
        CHECK(!FindFusionShooterTimingProfile(v.kind,"shoot2 "));
        for (size_t i=0;i<speeds.size();++i)
        {
            int normalCounter=0,alternateCounter=0;
            const int expectedNormal=v.kind==1113?cabbageCounters[i]:cornCounters[i];
            const int expectedAlternate=v.kind==1113?33:cornCounters[i];
            CHECK(TryGetFusionShooterShotCounter(*normal,speeds[i],normalCounter) && normalCounter==expectedNormal);
            CHECK(TryGetFusionShooterShotCounter(*alternate,speeds[i],alternateCounter) && alternateCounter==expectedAlternate);
            const float a=FusionShooterDuration(*normal,speeds[i]);
            const float b=FusionShooterDuration(*alternate,speeds[i]);
            CHECK(FusionShooterMaximumDuration(v.kind,speeds[i]) == (a>b?a:b));
            if (v.kind==1113) CHECK(b==1.0f);
            int shots=0;
            for (int counter=alternateCounter-1;counter>=0;--counter)
            {
                int count=-1;
                CHECK(TryGetFusionShooterEventsAtCounter(*alternate,speeds[i],counter,count));
                if (count) CHECK(counter==1 && count==1);
                shots+=count;
            }
            CHECK(shots==1);
        }
    }
    CHECK(FusionShooterMaximumDuration(1113,.9f)==1.1111111640930176f);
    CHECK(FusionShooterMaximumDuration(1113,1)==1);
    CHECK(FusionShooterMaximumDuration(1113,1.1f)==1);
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    for (float speed : {0.0f,.89f,1.11f,nan,(std::numeric_limits<float>::infinity)()})
        CHECK(FusionShooterMaximumDuration(1113,speed)==0);
    CHECK(!FindFusionShooterTimingProfile(1113,nullptr));
    CHECK(!FindFusionShooterTimingProfile(1113,""));
    CHECK(!FindFusionShooterTimingProfile(65535,"shoot"));
    CHECK(FusionShooterMaximumDuration(65535,1)==0);
    std::printf(result.mFailures?"[  FAILED  ] Fusion.AlternateStateTimingProfiles\n":
        "[       OK ] Fusion.AlternateStateTimingProfiles\n");
    return result;
}
