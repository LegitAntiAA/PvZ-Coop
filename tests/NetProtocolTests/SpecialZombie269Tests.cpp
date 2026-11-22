#include "SpecialZombie269Tests.h"
#include "../../Lawn/Fusion/SpecialZombie269Rules.h"
#include <cmath>
#include <cstdio>
#include <limits>

int RunSpecialZombie269Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
#define CHECK(...) do{++checks;if(!(__VA_ARGS__)){++failures;std::printf("SpecialZombie269:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    bool caltrop=false,nut=false;int level=9;
    CHECK(TryGetSpecialZombie269SourcePlantTraits(3,caltrop,nut,level)&&!caltrop&&nut&&level==0);
    CHECK(TryGetSpecialZombie269SourcePlantTraits(906,caltrop,nut,level)&&caltrop&&!nut&&level==0);
    CHECK(TryGetSpecialZombie269SourcePlantTraits(922,caltrop,nut,level)&&!caltrop&&nut);
    caltrop=true;nut=true;level=9;
    CHECK(!TryGetSpecialZombie269SourcePlantTraits(65535,caltrop,nut,level)&&caltrop&&nut&&level==9);
    SpecialZombie269CrashPlan crash;
    CHECK(TryPlanSpecialZombie269Crash(14,931,false,true,0,crash)&&crash.damage==500&&crash.killByCaltrop&&crash.knockRight);
    CHECK(TryPlanSpecialZombie269Crash(14,1048,false,true,0,crash)&&crash.action==SpecialZombie269CrashAction::Crush);
    CHECK(TryPlanSpecialZombie269Crash(18,1048,false,true,0,crash)&&crash.damage==500&&crash.killByCaltrop);
    CHECK(TryPlanSpecialZombie269Crash(18,3,false,true,2,crash)&&crash.action==SpecialZombie269CrashAction::Crush);
    CHECK(TryPlanSpecialZombie269Crash(18,3,false,true,3,crash)&&crash.action==SpecialZombie269CrashAction::Damage&&crash.damage==1000);
    CHECK(TryPlanSpecialZombie269Crash(18,1060,true,false,0,crash)&&crash.action==SpecialZombie269CrashAction::Ignore);
    CHECK(!TryPlanSpecialZombie269Crash(0,3,false,true,3,crash));
    // Machine40 uses the same source virtual47 CrashEntity381060 fallback;
    // this does not give it the legacy14/18 vehicle animation/motion policy.
    CHECK(TryPlanSpecialZombie269Crash(40,0,false,false,0,crash)&&crash.action==SpecialZombie269CrashAction::Crush&&!crash.damage&&!crash.knockRight&&!crash.killByCaltrop);
    CHECK(TryPlanSpecialZombie269Crash(40,903,false,true,0,crash)&&crash.action==SpecialZombie269CrashAction::Damage&&crash.damage==500&&crash.knockRight&&!crash.killByCaltrop);
    CHECK(TryPlanSpecialZombie269Crash(40,931,false,true,0,crash)&&crash.damage==500&&crash.knockRight&&crash.killByCaltrop);
    CHECK(TryPlanSpecialZombie269Crash(40,1048,false,true,0,crash)&&crash.damage==500&&crash.knockRight&&crash.killByCaltrop);
    CHECK(TryPlanSpecialZombie269Crash(40,3,false,true,2,crash)&&crash.action==SpecialZombie269CrashAction::Crush);
    CHECK(TryPlanSpecialZombie269Crash(40,3,false,true,3,crash)&&crash.action==SpecialZombie269CrashAction::Damage&&crash.damage==1000&&crash.knockRight&&!crash.killByCaltrop);
    CHECK(TryPlanSpecialZombie269Crash(40,1060,true,false,0,crash)&&crash.action==SpecialZombie269CrashAction::Ignore&&!crash.damage&&!crash.knockRight&&!crash.killByCaltrop);
    const auto savedCrash=crash;
    CHECK(!TryPlanSpecialZombie269Crash(40,3,false,true,-1,crash)&&crash.action==savedCrash.action&&crash.damage==savedCrash.damage&&crash.knockRight==savedCrash.knockRight&&crash.killByCaltrop==savedCrash.killByCaltrop);
    CHECK(!FindSpecialZombie269Profile(40)&&SpecialZombie269RequiredCapabilities(40)==0);
    CHECK(SpecialZombie269CrashZombieAllowed(true,false,0)&&!SpecialZombie269CrashZombieAllowed(true,false,10)&&
        !SpecialZombie269CrashZombieAllowed(true,false,12)&&!SpecialZombie269CrashZombieAllowed(false,false,0));
    SpecialZombie269CaltropPlan stop;
    CHECK(TryPlanSpecialZombie269DriverCaltrop(1351,2700,stop)&&stop.damage==1350&&!stop.stop);
    CHECK(TryPlanSpecialZombie269DriverCaltrop(1350,2700,stop)&&!stop.damage&&stop.stop);
    CHECK(!TryPlanSpecialZombie269DriverCaltrop(0,2700,stop));
    float vx=77;
    CHECK(TryGetSpecialZombie269DriverVelocity(false,false,false,.8f,vx)&&vx==-.8f);
    CHECK(TryGetSpecialZombie269DriverVelocity(false,false,true,.8f,vx)&&vx==-.2f);
    CHECK(TryGetSpecialZombie269DriverVelocity(true,false,false,.8f,vx)&&vx==.8f);
    CHECK(TryGetSpecialZombie269DriverVelocity(true,true,false,.8f,vx)&&vx==0);
    CHECK(!TryGetSpecialZombie269DriverVelocity(false,false,false,-1,vx)&&vx==0);
    uint8_t stage=9;
    CHECK(!TryGetSpecialZombie269BodyStage(40,900,1350,stage)&&stage==9);
    CHECK(TryGetSpecialZombie269BodyStage(14,900,1350,stage)&&stage==0);
    CHECK(TryGetSpecialZombie269BodyStage(14,899,1350,stage)&&stage==1);
    CHECK(TryGetSpecialZombie269BodyStage(18,900,2700,stage)&&stage==1);
    CHECK(TryGetSpecialZombie269BodyStage(18,899,2700,stage)&&stage==2);
    SpecialZombie269PaperClip clip;
    CHECK(!TryGetSpecialZombie269VehicleClip(40,false,clip)&&clip.name==nullptr&&clip.duration==0&&clip.speed==0);
    CHECK(TryGetSpecialZombie269VehicleClip(14,false,clip)&&clip.duration==1.6833333f&&clip.speed==1);
    CHECK(TryGetSpecialZombie269VehicleClip(18,true,clip)&&clip.duration==1&&clip.speed==.9f);
    CHECK(TryGetSpecialZombie269VehicleClip(18,false,clip,true)&&clip.duration==.15f);
    double dx=100;
    CHECK(TryGetSpecialZombie269SubmarineRootDelta({0,0},{1,0},false,1,dx)&&std::abs(dx-double(6.53f-7.32f))<1e-6);
    CHECK(TryGetSpecialZombie269SubmarineRootDelta({0,0},{1,0},true,1,dx)&&dx>0);
    CHECK(TryGetSpecialZombie269SubmarineRootDelta({4,.8f},{4,.8f},false,1,dx)&&dx==0);
    CHECK(!TryGetSpecialZombie269SubmarineRootDelta({4,.8f},{3,.8f},false,1,dx)&&dx==0);
    // Exact source polygon, including the upper-right area its AABB would
    // falsely admit. The source14 polygon is deliberately not replaced by SAT
    // on its convex hull or by the stock Zamboni rectangle.
    CHECK(SpecialZombie269VehicleTouchesRect(14,0,0,1,false,1,1,-.1,-1,.2,.2));
    CHECK(!SpecialZombie269VehicleTouchesRect(14,0,0,1,false,1,1,1.6,-2,.05,.05));
    CHECK(SpecialZombie269VehicleTouchesRect(18,0,0,1,false,1,1,1.4,-1,.1,.1));
    CHECK(!SpecialZombie269VehicleTouchesRect(18,0,0,1,true,1,1,1.4,-1,.1,.1));
    CHECK(!SpecialZombie269VehicleTouchesRect(18,0,0,1,false,1,1,0,-1,0,1));
    CHECK(!SpecialZombie269VehicleTouchesRect(18,0,0,std::numeric_limits<double>::quiet_NaN(),false,1,1,0,-1,1,1));
    CHECK(!SpecialZombie269VehicleTouchesRect(40,0,0,1,false,1,1,0,-1,1,1));
    CHECK(!SpecialZombie269CapabilitiesReady(14,Special269Model|Special269Crash|Special269BodyParts|Special269DeathEffect));
    CHECK(SpecialZombie269CapabilitiesReady(14,SpecialZombie269RequiredCapabilities(14)));
    CHECK(!SpecialZombie269CapabilitiesReady(18,0));
#undef CHECK
    return failures;
}
