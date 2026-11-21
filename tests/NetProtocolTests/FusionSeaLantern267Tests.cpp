#include "FusionSeaLantern267Tests.h"
#include "../../Lawn/Fusion/FusionSeaLantern267Rules.h"
#include <cstdio>
#include <initializer_list>
FusionSeaLantern267TestResult RunFusionSeaLantern267Tests()
{
    using namespace LawnFusion;FusionSeaLantern267TestResult result{};
#define CHECK(...) do{++result.mChecks;if(!(__VA_ARGS__)){++result.mFailures;std::printf("SeaLantern267:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    CHECK(kFusionSeaLantern267GrowthTicks==3001);
    CHECK(FusionSeaLantern267LightRadius(0)==1&&FusionSeaLantern267LightRadius(1)==2);
    CHECK(FusionSeaLantern267LightRadius(2)==0&&FusionSeaLantern267LightRadius(255)==0);
    for(int remaining:{1,1500,3001})CHECK(FusionSeaLantern267StateValid(1078,0,0,0,0,remaining,0));
    CHECK(FusionSeaLantern267StateValid(1078,0,0,0,1,0,0));
    for(int invalid=0;invalid<11;++invalid)
        CHECK(!FusionSeaLantern267StateValid(invalid==0?20:1078,invalid==1?1:0,invalid==2?1:0,invalid==3?1:0,
            invalid==4?2:invalid==5?255:invalid==6?1:0,invalid==6?1:invalid==7?0:invalid==8?-1:invalid==9?3002:3001,invalid==10?1:0));
#undef CHECK
    return result;
}
