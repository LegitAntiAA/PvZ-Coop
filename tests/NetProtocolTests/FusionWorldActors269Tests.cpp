#include "FusionWorldActors269Tests.h"
#include "../../Lawn/Fusion/FusionWorldActors269Rules.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
int RunFusionWorldActors269Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool pass,int line){++checks;if(!pass){++failures;std::printf("    WorldActors269 line=%d\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    // The native Begin/Phase bridge rejects unregistered profiles. A private
    // native Prism profile previously left34 permanently idle despite a target.
    const auto* prism=FindFusionWorldActors269Timing(34);
    CHECK(prism&&prism==FindFusionShooterTimingProfile(34));
    CHECK(prism==FindFusionShooterTimingProfile(34,"shoot"));
    CHECK(!FindFusionShooterTimingProfile(34,"idle"));
    CHECK(!FindFusionWorldActors269Timing(937));
    if(prism)
    {
        CHECK(FusionShooterTimingProfileValid(*prism));
        for(const float speed:{.9f,1.1f})
        {
            int counter=0;CHECK(TryGetFusionShooterShotCounter(*prism,speed,counter)&&counter==51);
            CHECK(FusionShooterDuration(*prism,speed)==1.f&&FusionShooterMaximumDuration(34,speed)==1.f);
            bool due=false;
            CHECK(TryGetFusionShooterEventAtCounter(*prism,speed,counter-49,0,due)&&!due);
            CHECK(TryGetFusionShooterEventAtCounter(*prism,speed,counter-50,0,due)&&due);
            CHECK(TryGetFusionShooterEventAtCounter(*prism,speed,counter-51,0,due)&&!due);
        }
    }
    CHECK(FusionWorld269ReferenceValid({}));CHECK(FusionWorld269ReferenceValid({3,44}));
    CHECK(!FusionWorld269ReferenceValid({3,0}));CHECK(!FusionWorld269ReferenceValid({0,44}));
    int value=-7;CHECK(TryGetFusionPrism269Damage(20,0,value)&&value==20);
    CHECK(TryGetFusionPrism269Damage(20,1,value)&&value==50);
    CHECK(TryGetFusionPrism269Damage(20,3,value)&&value==110);
    CHECK(TryGetFusionPrism269Damage(3,1,value)&&value==7);
    value=18;CHECK(!TryGetFusionPrism269Damage(std::numeric_limits<int>::max(),2,value)&&value==18);
    CHECK(!TryGetFusionPrism269Damage(20,-1,value));CHECK(!TryGetFusionPrism269Damage(20,1025,value));
    CHECK(TryGetFusionLaser269Damage(100,false,false,0,value)&&value==100);
    CHECK(TryGetFusionLaser269Damage(100,true,false,10,value)&&value==600);
    CHECK(TryGetFusionLaser269Damage(100,true,true,10,value)&&value==1800);
    CHECK(!TryGetFusionLaser269Damage(std::numeric_limits<int>::max(),true,false,0,value));
    CHECK(FusionLaser269CanAcquire(10,false,true,2,2,false,true));
    CHECK(!FusionLaser269CanAcquire(10,false,true,3,2,false,true));
    CHECK(FusionLaser269CanAcquire(0,false,true,5,0,true,false));
    CHECK(!FusionLaser269CanAcquire(21,false,true,0,0,false,false));
    CHECK(!FusionLaser269CanAcquire(12,false,true,0,0,false,false));
    CHECK(!FusionLaser269CanAcquire(0,true,true,0,0,false,false));
    CHECK(!FusionLaser269CanAcquire(0,false,false,0,0,false,false));
    CHECK(!FusionLaser269CanAcquire(0,false,true,0,0,false,true));
    CHECK(FusionLaser269Healing(300)==60);CHECK(FusionLaser269Healing(16000)==2000);
    CHECK(FusionLaser269Healing(0)==0);
    bool depleted=false;CHECK(TryAdjustFusionLaser269Energy(100,2000,-50,value,depleted)&&value==150&&!depleted);
    CHECK(TryAdjustFusionLaser269Energy(1990,2000,-50,value,depleted)&&value==2000&&!depleted);
    CHECK(TryAdjustFusionLaser269Energy(100,2000,100,value,depleted)&&value==0&&depleted);
    CHECK(TryAdjustFusionLaser269Energy(100,2000,200,value,depleted)&&value==0&&depleted);
    CHECK(!TryAdjustFusionLaser269Energy(2001,2000,20,value,depleted));
    CHECK(!TryAdjustFusionLaser269Energy(0,0,0,value,depleted));
    CHECK(FusionPrism269Alpha(0)==1&&FusionPrism269Alpha(20)==1);
    CHECK(std::abs(FusionPrism269Alpha(21)-.95f)<1e-6f);
    CHECK(FusionPrism269Alpha(40)==0&&FusionPrism269Alpha(-1)==0);
    CHECK(!FusionPrism269Expired(49)&&FusionPrism269Expired(50));
#undef CHECK
    return failures;
}
