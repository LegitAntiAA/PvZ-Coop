#include "FusionAction266Tests.h"
#include "../../Lawn/Fusion/FusionAction266Rules.h"
#include "../../Lawn/Fusion/FusionBatchActionRules.h"
#include "../../Lawn/Fusion/FusionDeathRules.h"
#include "../../Lawn/Fusion/FusionGarlicRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <utility>
FusionAction266TestResult RunFusionAction266Tests()
{
    using namespace LawnFusion;FusionAction266TestResult r;
#define CHECK(x) do{++r.mChecks;if(!(x)){++r.mFailures;std::printf("Action266:%d %s\n",__LINE__,#x);}}while(0)
    for(uint16_t kind:{1002,1009,1015,1060,1070,1071,1072,1074,1075,1087,1145,1146,1163,1170,1190,1057,1155,1048,1189,1039})
    {CHECK(IsFusionAction266Kind(kind));CHECK(FusionAction266PlantStateValid(kind,0,0,0,0));CHECK(!FusionAction266PlantStateValid(kind,1,0,0,0));}
    CHECK(!IsFusionAction266Kind(65535));CHECK(!FusionAction266PlantStateValid(1015,0,0,0,3));
    CHECK(FusionAction266PlantStateValid(1015,0,0,0,2));CHECK(FusionAction266PlantStateValid(1170,0,0,0,1));
    for(uint16_t kind:{1060,1074,1075,1070,1071,1072})
    {
        const auto* clock=FindFusionBatchTimingProfile(kind);CHECK(clock&&FindFusionBatchActionProfile(kind));
        if(!clock)continue;
        CHECK(FusionShooterTimingProfileValid(*clock));
        CHECK(clock->stateSpeed==static_cast<double>(IsFusionAction266Gloom(kind)?1.3f:1.2f));
        CHECK(clock->clipDurationSeconds==static_cast<double>(IsFusionAction266Gloom(kind)?(kind==1071?2.f:2.0166667f):.9f));
        CHECK(clock->eventClipSeconds.size()==(IsFusionAction266Gloom(kind)?4:2));
        for(float speed:{.9f,1.f,1.1f})
        {
            int counter=0;CHECK(TryGetFusionShooterShotCounter(*clock,speed,counter));
            int total=0;
            int untouched=77;CHECK(!TryGetFusionShooterEventsAtCounter(*clock,speed,counter,untouched)&&untouched==77);
            for(int tick=counter-1;tick>=0;--tick){int due=0;CHECK(TryGetFusionShooterEventsAtCounter(*clock,speed,tick,due));total+=due;}
            CHECK(total==int(clock->eventClipSeconds.size()));
            CHECK(FusionAction266MaximumDuration(kind,speed)>FusionShooterDuration(*clock,speed));
        }
    }
    CHECK(!FusionAction266GloomTouches(1070,2,0,0,0,0,1,1));
    CHECK(FusionAction266GloomTouches(1070,1,0,0,0,-30,0,0));
    CHECK(FusionAction266GloomTouches(1072,-1,0,0,-80,-40,20,20));
    CHECK(!FusionAction266GloomTouches(1070,0,0,0,120,-30,0,0));
    CHECK(!FusionAction266GloomTouches(1070,0,0,0,0,0,-1,2));
    uint8_t stage=99;
    for(const auto pair:{std::pair<int,int>{450,0},{301,0},{300,1},{151,1},{150,2},{0,2}})
    {CHECK(TryGetFusionAction266RockStage(pair.first,450,stage)&&stage==pair.second);}
    CHECK(TryGetFusionAction266RockStage(1500000000,2000000000,stage)&&stage==0);
    CHECK(!TryGetFusionAction266RockStage(451,450,stage));
    int amount=99;CHECK(TryGetFusionAction266LanternHeal(0,amount)&&amount==0);
    CHECK(TryGetFusionAction266LanternHeal(1,amount)&&amount==0);CHECK(TryGetFusionAction266LanternHeal(2,amount)&&amount==150);
    CHECK(TryGetFusionAction266LanternHeal(4,amount)&&amount==450);CHECK(!TryGetFusionAction266LanternHeal(-1,amount));
    int ticks=99;CHECK(TryGetFusionAction266Ticks(.5,1,ticks)&&ticks==50);CHECK(TryGetFusionAction266Ticks(20,1,ticks)&&ticks==2000);
    CHECK(!TryGetFusionAction266Ticks(20,0,ticks));CHECK(!TryGetFusionAction266Ticks((std::numeric_limits<double>::infinity)(),1,ticks));
    for(uint16_t kind:{1015,1163,1190}){CHECK(FindFusionDeathProfile(kind));CHECK(FusionDeathFlagsValid(kind,0));}
    NativeBlastSpec spec;CHECK(TryMakeFusionDeathBlast(1190,100,200,2,true,spec)&&spec.damage==500);
    CHECK(TryMakeFusionDeathBlast(1190,100,200,2,false,spec)&&spec.damage==1800);
    CHECK(!TryMakeFusionDeathBlast(1163,100,200,2,true,spec));
    CHECK(TryMakeFusionDeathBlast(1170,100,200,2,false,spec)&&spec.damage==300);
    GarlicActionState garlic;CHECK(TryBeginFusionGarlicTimedAction(5,10,false,garlic));
    CHECK(garlic.waitTicks==10&&!garlic.certainRow);GarlicActionStep step;
    for(int i=0;i<9;++i)CHECK(TryAdvanceFusionGarlicAction(garlic,5,false,1,step)&&!step.rowStartDue);
    CHECK(TryAdvanceFusionGarlicAction(garlic,5,false,1,step)&&step.rowStartDue);
    CHECK(!TryBeginFusionGarlicTimedAction(5,10,false,garlic));
    CHECK(IsFusionGarlicKind(1145)&&IsFusionGarlicKind(1146));
#undef CHECK
    return r;
}
