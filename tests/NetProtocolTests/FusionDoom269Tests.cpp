#include "FusionDoom269Tests.h"
#include "../../Lawn/Fusion/FusionDoom269Rules.h"
#include "../../Lawn/Fusion/FusionNativeDoom269.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <vector>

FusionDoom269TestResult RunFusionDoom269Tests()
{
    using namespace LawnFusion;FusionDoom269TestResult result;
#define CHECK(...) do{++result.mChecks;if(!(__VA_ARGS__)){++result.mFailures;std::printf("Doom269:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    for(bool basic:{false,true})for(bool advanced:{false,true})
    {
        const int threshold=basic?4:16;int ordinal=0,enhanced=0;float interval=1.5f;
        for(int i=1;i<=threshold*2;++i)
        {
            FusionSuperDoom269Shot shot;
            CHECK(TryPrepareFusionSuperDoom269Shot(interval,ordinal,20,basic,advanced,shot));
            CHECK(shot.enhanced==(i%threshold==0)&&shot.damage==(i%threshold==0?120:20));
            CHECK(shot.overrideRecovery==(shot.enhanced&&!advanced));
            if(shot.overrideRecovery)CHECK(shot.recoverySeconds==shot.interval*3.f);
            ordinal=shot.ordinal;interval=shot.interval;enhanced+=shot.enhanced?1:0;
        }
        CHECK(enhanced==2&&ordinal==0);
    }
    FusionSuperDoom269Shot shot;
    CHECK(TryPrepareFusionSuperDoom269Shot(.25f,0,20,false,false,shot)&&shot.interval<.2f);
    CHECK(TryPrepareFusionSuperDoom269Shot(shot.interval,1,20,false,false,shot)&&shot.interval==.2f);
    CHECK(TryPrepareFusionSuperDoom269Shot(.2f,0,20,false,false,shot)&&shot.interval==.2f);
    CHECK(!TryPrepareFusionSuperDoom269Shot(std::numeric_limits<float>::quiet_NaN(),0,20,false,false,shot));
    CHECK(!TryPrepareFusionSuperDoom269Shot(1.f,16,20,false,false,shot));
    CHECK(TryPrepareFusionSuperDoom269Shot(1.f,4,20,true,false,shot)&&shot.enhanced&&shot.ordinal==0);
    CHECK(!TryPrepareFusionSuperDoom269Shot(1.f,15,std::numeric_limits<int>::max(),false,false,shot));
    FusionIceDoom269Damage damage;
    CHECK(TryPrepareFusionIceDoom269Damage(9999,1,1,true,damage)&&damage.absorbed&&damage.doom&&damage.power==0);
    CHECK(TryPrepareFusionIceDoom269Damage(9000,2000,2,true,damage)&&damage.doom&&damage.power==0);
    CHECK(TryPrepareFusionIceDoom269Damage(0,9999,2,true,damage)&&!damage.doom&&damage.power==9999);
    for(int type:{0,3,4})CHECK(TryPrepareFusionIceDoom269Damage(9,10000,type,true,damage)&&!damage.absorbed&&!damage.doom&&damage.power==9);
    CHECK(TryPrepareFusionIceDoom269Damage(9,10000,1,false,damage)&&!damage.absorbed&&damage.power==9);
    CHECK(!TryPrepareFusionIceDoom269Damage(10000,1,1,true,damage));
    CHECK(FusionIceDoom269DeathDamage(false)==1800&&FusionIceDoom269DeathDamage(true)==1000000);
    CHECK(FindFusionDoom269Timing(910)->attackIntervalSeconds==1.9f&&FindFusionDoom269Timing(910)->eventClipSeconds.size()==4&&FindFusionDoom269Timing(910)->stateSpeed==double(1.3f));
    CHECK(FindFusionDoom269Timing(930)->eventClipSeconds.size()==1&&FindFusionDoom269Timing(930)->clipDurationSeconds==.5&&!FindFusionDoom269Timing(0));
    CHECK(FusionIceDoom269PulseDelayTicks()==51);int deadline=0;
    CHECK(TryMakeFusionIceDoom269Deadline(100,deadline)&&deadline==151);
    CHECK(!TryMakeFusionIceDoom269Deadline(-1,deadline)&&!TryMakeFusionIceDoom269Deadline(std::numeric_limits<int>::max()-50,deadline));
    CHECK(TryMakeFusionIceDoom269Deadline(std::numeric_limits<int>::max()-51,deadline)&&deadline==std::numeric_limits<int>::max());
    for(int status:{3,7,9,10,12,16,20,21,22,23,24})CHECK(!FusionDoom269LandTarget(status,false));
    CHECK(FusionDoom269LandTarget(0,false)&&FusionDoom269LandTarget(1,false)&&!FusionDoom269LandTarget(0,true));
    CHECK(FusionSuperDoom269FearTarget(10,false)&&FusionSuperDoom269FearTarget(1,false)&&!FusionSuperDoom269FearTarget(0,true));
    for(int status:{22,23,24})CHECK(!FusionSuperDoom269FearTarget(status,false));
    CHECK(FusionSuperDoom269StateValid(1.5f,15,9999)&&!FusionSuperDoom269StateValid(.15f,0,0));
    CHECK(!FusionSuperDoom269StateValid(1.5f,16,0)&&!FusionSuperDoom269StateValid(1.5f,0,10000));
    NativeDoom269PlantState s;CHECK(NativeDoom269PlantStateValid(930,s)&&!NativeDoom269PlantStateValid(910,s));
    s.interval=1.9f;s.power=9999;CHECK(NativeDoom269PlantStateValid(910,s));s.ordinal=1;CHECK(!NativeDoom269PlantStateValid(910,s));
    // Source AnyState self-transition has a fixed.25s noninterruptible blend.
    // Every accepted state must keep its.3s event even when the next CD trigger
    // arrived at.2s. Counters are decremented by the native driver, never planner.
    for(float speed:{.9f,1.f,1.1f})for(int triggerEvery:{20,25,30,50})
    {
        const auto& timing=*FindFusionDoom269Timing(930);int initial=0;
        CHECK(TryGetFusionShooterShotCounter(timing,speed,initial));
        const int eventTicks=int(std::ceil(double(.3f)/double(speed)*100)),eventCounter=initial-eventTicks;
        FusionSelfTransition269Counters counters;std::vector<int> due;int actual=0,expected=0,begins=0;bool valid=true;
        for(int tick=0;tick<=400;++tick)
        {
            if(counters.incoming>0){--counters.incoming;if(counters.incoming==eventCounter)++actual;}
            if(counters.lock>0)--counters.lock;
            FusionSelfTransition269Plan plan;
            valid&=TryPlanFusionSelfTransition269(counters,tick%triggerEvery==0,true,initial,eventCounter,25,plan);
            if(plan.outgoingDue)++actual;
            if(plan.begin){due.push_back(tick+eventTicks);++begins;}
            counters=plan.counters;
            for(int deadline:due)if(deadline==tick)++expected;
            valid&=actual==expected;
            if(tick==triggerEvery+1)
            {
                FusionSelfTransition269Plan paused,repeated;
                valid&=TryPlanFusionSelfTransition269(counters,true,false,initial,eventCounter,25,paused)&&!paused.begin&&!paused.outgoingDue&&paused.counters.incoming==counters.incoming&&paused.counters.lock==counters.lock&&paused.counters.outgoing==counters.outgoing&&paused.counters.queued==counters.queued;
                valid&=TryPlanFusionSelfTransition269(counters,false,false,initial,eventCounter,25,repeated)&&!repeated.begin&&!repeated.outgoingDue;
            }
            if(counters.outgoing>0)--counters.outgoing;
        }
        CHECK(valid&&begins>1&&actual>1&&actual<=begins);
    }
    FusionSelfTransition269Plan unchanged;
    CHECK(!TryPlanFusionSelfTransition269({40,0,5,true},false,true,50,20,25,unchanged)); // Would overwrite a pending event.
#undef CHECK
    return result;
}
