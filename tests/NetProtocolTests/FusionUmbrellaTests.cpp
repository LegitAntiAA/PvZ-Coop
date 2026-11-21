#include "FusionUmbrellaTests.h"
#include "../../Lawn/Fusion/FusionUmbrellaRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionUmbrellaTestResult result;
void Check(bool value, int line)
{ ++result.mChecks; if (!value) { ++result.mFailures; std::printf("    Umbrella check failed line=%d\n",line); } }
#define CHECK(value) Check(!!(value),__LINE__)
}
FusionUmbrellaTestResult RunFusionUmbrellaTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.SourceUmbrellaCoverageLatchAndDelayedDeflection\n");
    // 0 is a normalized ordinary source Bullet kind, not a newly enabled
    // native hostile adapter. Native entry still requires the caller's view.
    const auto covers = [](int x,int y,bool awake=true,float animator=1.0f)
    { return FusionUmbrellaCanIntercept(0,13,true,false,2,0,4,2,x,y,awake,animator); };
    for (int y : {1,2,3}) for (int x : {3,4,5}) CHECK(covers(x,y));
    CHECK(!covers(2,2) && !covers(6,2) && !covers(4,0) && !covers(4,4));
    CHECK(!covers(4,2,false) && !covers(4,2,true,0));
    CHECK(covers(4,2,true,-1)); // Source tests Animator.speed != 0, not a positive rate.
    CHECK(!covers(4,2,true,std::numeric_limits<float>::quiet_NaN()));
    CHECK(!FusionUmbrellaCanIntercept(65535,13,true,false,2,0,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(3,13,true,false,2,0,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,12,true,false,2,0,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,13,false,false,2,0,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,13,true,true,2,0,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,13,true,false,1,0,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,13,true,false,2,254,4,2,4,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,13,true,false,2,0,-1,2,0,2,true,1));
    CHECK(!FusionUmbrellaCanIntercept(0,13,true,false,2,0,0,2,std::numeric_limits<int>::max(),2,true,1));
    CHECK(std::ceil(static_cast<double>(.02f*2.5f)*100) == kFusionUmbrellaDelayTicks);

    FusionUmbrellaBlockState state; FusionUmbrellaPoseState pose; bool start = false;
    CHECK(FusionUmbrellaBlockStateValid(state));
    CHECK(!TryBeginFusionUmbrellaBlock(0,65537,100,state,pose,start) && !state.harmless && !start);
    CHECK(TryBeginFusionUmbrellaBlock(65536,65537,100,state,pose,start));
    CHECK(start && state.harmless && state.pending && pose.blocking && !pose.sortAtRoot);
    CHECK(state.projectileGeneration == 65536 && state.umbrellaGeneration == 65537 && state.blockedAtTick == 100);
    CHECK(!TryBeginFusionUmbrellaBlock(131072,65537,101,state,pose,start)); // A recycled slot cannot steal this state.
    float x = -3, y = 2;
    CHECK(AdvanceFusionUmbrellaBlock(state,100,true,true,x,y) == FusionUmbrellaAdvance::Unchanged && x == -3 && y == 2);
    CHECK(AdvanceFusionUmbrellaBlock(state,104,true,true,x,y) == FusionUmbrellaAdvance::Unchanged && state.pending);
    CHECK(AdvanceFusionUmbrellaBlock(state,105,true,true,x,y) == FusionUmbrellaAdvance::Deflected);
    CHECK(x == -6 && y == -2 && state.harmless && !state.pending);
    CHECK(AdvanceFusionUmbrellaBlock(state,106,true,true,x,y) == FusionUmbrellaAdvance::Unchanged && x == -6 && y == -2);

    FusionUmbrellaBlockState other; bool anotherStart = true;
    CHECK(TryBeginFusionUmbrellaBlock(65538,65537,103,other,pose,anotherStart) && !anotherStart);
    CHECK(AdvanceFusionUmbrellaBlock(other,108,true,false,x,y) == FusionUmbrellaAdvance::Cancelled);
    CHECK(other.harmless && !other.pending && x == -6 && y == -2);
    FusionUmbrellaBlockState gravity; FusionUmbrellaPoseState gravityPose;
    CHECK(TryBeginFusionUmbrellaBlock(65545,65546,200,gravity,gravityPose,start));
    x = -2; y = 1;
    CHECK(AdvanceFusionUmbrellaBlock(gravity,204,true,true,x,y) == FusionUmbrellaAdvance::Unchanged);
    y = 3; // The owning motion advanced gravity while the callback waited.
    CHECK(AdvanceFusionUmbrellaBlock(gravity,205,true,true,x,y) == FusionUmbrellaAdvance::Deflected && x == -4 && y == -3);
    FusionUmbrellaBlockState freshProjectile; bool unchanged = true;
    CHECK(!TryBeginFusionUmbrellaBlock(65547,131073,206,freshProjectile,pose,unchanged));
    CHECK(!freshProjectile.projectileGeneration && unchanged && pose.plantGeneration == 65537);
    FusionUmbrellaBlockState retired; FusionUmbrellaPoseState fresh;
    CHECK(TryBeginFusionUmbrellaBlock(65539,65540,0,retired,fresh,start));
    CHECK(AdvanceFusionUmbrellaBlock(retired,1,false,true,x,y) == FusionUmbrellaAdvance::Cancelled && retired.harmless);
    FusionUmbrellaBlockState overflow; FusionUmbrellaPoseState overflowPose;
    CHECK(TryBeginFusionUmbrellaBlock(65541,65542,std::numeric_limits<int>::max()-5,overflow,overflowPose,start));
    x = std::numeric_limits<float>::max(); y = 2;
    CHECK(AdvanceFusionUmbrellaBlock(overflow,std::numeric_limits<int>::max(),true,true,x,y) == FusionUmbrellaAdvance::Invalid);
    CHECK(overflow.pending && x == std::numeric_limits<float>::max() && y == 2);
    x = 2;
    CHECK(AdvanceFusionUmbrellaBlock(overflow,std::numeric_limits<int>::max(),true,true,x,y) == FusionUmbrellaAdvance::Deflected && x == 4 && y == -2);
    FusionUmbrellaBlockState invalid; invalid.pending = true;
    CHECK(!FusionUmbrellaBlockStateValid(invalid));
    CHECK(AdvanceFusionUmbrellaBlock(invalid,0,true,true,x,y) == FusionUmbrellaAdvance::Invalid && invalid.pending);

    std::size_t count = 0; const auto* events = GetFusionUmbrellaPoseEvents(count);
    CHECK(count == 3 && events[0].event == FusionUmbrellaPoseEvent::BlockStart && events[0].clipSeconds == 0);
    CHECK(events[1].event == FusionUmbrellaPoseEvent::BlockOver && events[2].event == FusionUmbrellaPoseEvent::AllBlockOver);
    CHECK(events[1].clipSeconds == .75 && events[2].clipSeconds == .75);
    int tick = -99;
    CHECK(TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent::BlockStart,1,tick) && tick == 0);
    CHECK(TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent::BlockOver,.9f,tick) && tick == 70);
    CHECK(TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent::BlockOver,1,tick) && tick == 63);
    CHECK(TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent::AllBlockOver,1.1f,tick) && tick == 57);
    CHECK(!TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent::BlockOver,0,tick) && tick == 57);
    CHECK(!ApplyFusionUmbrellaPoseEvent(pose,65538,FusionUmbrellaPoseEvent::BlockStart) && !pose.sortAtRoot);
    CHECK(ApplyFusionUmbrellaPoseEvent(pose,65537,FusionUmbrellaPoseEvent::BlockStart) && pose.sortAtRoot && pose.blocking);
    CHECK(ApplyFusionUmbrellaPoseEvent(pose,65537,FusionUmbrellaPoseEvent::BlockOver) && !pose.sortAtRoot && pose.blocking);
    CHECK(ApplyFusionUmbrellaPoseEvent(pose,65537,FusionUmbrellaPoseEvent::AllBlockOver) && !pose.blocking);
    CHECK(state.harmless); // Ending the plant pose never clears any projectile hit latch.
    std::printf("[ %s ] Fusion.SourceUmbrella checks=%d failures=%d\n",result.mFailures?" FAILED ":"      OK",result.mChecks,result.mFailures);
    return result;
}
