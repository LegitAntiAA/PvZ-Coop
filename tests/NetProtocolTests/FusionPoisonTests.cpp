#include "FusionPoisonTests.h"
#include "../../Lawn/Fusion/FusionPoisonRules.h"
#include <cstdio>
#include <initializer_list>
#include <limits>

FusionPoisonTestResult RunFusionPoisonTests()
{
    FusionPoisonTestResult result{};using namespace LawnFusion;
#define CHECK(...) do{++result.mChecks;if(!(__VA_ARGS__)){++result.mFailures;std::printf("Poison:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    FusionPoisonState state;
    CHECK(FusionPoisonStateValid(state));
    for(int level=1;level<=5;++level)
    {CHECK(TryAddFusionPoisonLevel(state,state));CHECK(state.level==level&&state.ticks==0&&state.damageTicks==100);}
    CHECK(TryAddFusionPoisonLevel(state,state)&&state==FusionPoisonState{5,500,100});
    CHECK(TryAddFusionPoisonLevel(state,state)&&state==FusionPoisonState{5,1000,100});
    CHECK(TrySetFusionPoison(state,1000,state)&&state==FusionPoisonState{6,2000,100});
    CHECK(TryAddFusionPoisonLevel(state,state)&&state==FusionPoisonState{7,3000,100});
    CHECK(TrySetFusionPoison(state,500,state)&&state==FusionPoisonState{8,3500,100});
    // Every tick is the real100Hz adaptation, with expiration before pulse.
    state={5,500,100};int pulses=0,total=0;
    for(int tick=1;tick<=500;++tick)
    {
        int damage=-1;bool pulse=true;CHECK(TryStepFusionPoison(state,state,damage,pulse));
        CHECK(state.ticks==500-tick&&FusionPoisonStateValid(state));
        if(pulse){++pulses;total+=damage;CHECK(tick%100==0&&tick<500&&damage==100&&state.damageTicks==100);}
        else CHECK(damage==0);
    }
    CHECK(pulses==4&&total==400&&state==FusionPoisonState{0,0,1});
    state={3,10,1};int damage=-1;bool pulse=false;
    CHECK(TryStepFusionPoison(state,state,damage,pulse)&&pulse&&damage==60&&state==FusionPoisonState{3,9,100});
    state={6,1,1};damage=-1;pulse=true;
    CHECK(TryStepFusionPoison(state,state,damage,pulse)&&!pulse&&damage==0&&state==FusionPoisonState{0,0,1});
    // Partial stacks and the residual pulse clock survive arbitrary inactive time.
    state={5,0,37};
    for(int tick=0;tick<150;++tick){CHECK(TryStepFusionPoison(state,state,damage,pulse));CHECK(state==FusionPoisonState{5,0,37}&&!damage&&!pulse);}
    CHECK(TryAddFusionPoisonLevel(state,state)&&state==FusionPoisonState{5,500,37});
    for(int tick=0;tick<36;++tick)CHECK(TryStepFusionPoison(state,state,damage,pulse)&&!pulse);
    CHECK(TryStepFusionPoison(state,state,damage,pulse)&&pulse&&damage==100&&state.damageTicks==100);
    CHECK(TrySetFusionPoison(state,1000,state)&&state.level==6&&state.damageTicks==100);
    state={5,0,31};CHECK(TryWarmFusionPoison(state,state)&&state==FusionPoisonState{5,0,31});
    state={7,1000,31};CHECK(TryWarmFusionPoison(state,state)&&state==FusionPoisonState{0,1000,31});
    for(int tick=0;tick<30;++tick)CHECK(TryStepFusionPoison(state,state,damage,pulse)&&!pulse);
    CHECK(TryStepFusionPoison(state,state,damage,pulse)&&pulse&&damage==0&&state.damageTicks==100&&state.ticks==969);
    // Invalid wire-like states and checked I32 boundaries preserve every output.
    for(const auto& bad:{FusionPoisonState{-1,0,100},{kFusionPoisonMaximumLevel+1,1,100},
        {1,-1,100},{1,1,0},{1,1,101},{6,0,100}})
    {
        FusionPoisonState output{4,500,27};const auto old=output;damage=123;pulse=true;
        CHECK(!FusionPoisonStateValid(bad));
        CHECK(!TryAddFusionPoisonLevel(bad,output)&&output==old);
        CHECK(!TrySetFusionPoison(bad,1000,output)&&output==old);
        CHECK(!TryWarmFusionPoison(bad,output)&&output==old);
        CHECK(!TryStepFusionPoison(bad,output,damage,pulse)&&output==old&&damage==123&&pulse);
    }
    state={kFusionPoisonMaximumLevel,2,1};CHECK(TryStepFusionPoison(state,state,damage,pulse)&&damage==kFusionPoisonMaximumLevel*20&&pulse);
    FusionPoisonState output{2,300,20};const auto old=output;
    CHECK(!TrySetFusionPoison(state,1000,output)&&output==old);
    CHECK(!TryAddFusionPoisonLevel(state,output)&&output==old);
    state={5,(std::numeric_limits<int>::max)(),1};
    CHECK(!TrySetFusionPoison(state,1,output)&&output==old);
    CHECK(!TryAddFusionPoisonLevel(state,output)&&output==old);
    CHECK(!TrySetFusionPoison({},0,output)&&output==old);
    CHECK(!TrySetFusionPoison({},-1,output)&&output==old);
#undef CHECK
    std::printf("Fusion.Poison checks=%d failures=%d\n",result.mChecks,result.mFailures);return result;
}
