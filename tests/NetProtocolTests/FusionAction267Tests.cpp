#include "FusionAction267Tests.h"
#include "../../Lawn/Fusion/FusionAction267Rules.h"
#include "../../Lawn/Fusion/FusionUmbrellaBatchRules.h"
#include "../../Lawn/Fusion/FusionWaterPlants267Rules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
FusionAction267TestResult RunFusionAction267Tests()
{
    using namespace LawnFusion;FusionAction267TestResult result{};
#define CHECK(x) do{++result.mChecks;if(!(x)){++result.mFailures;std::printf("Action267:%d %s\n",__LINE__,#x);}}while(0)
    // Source ObsidianTallNut caps BEFORE the sourceType3 recursion terminator.
    int applied=-1;bool shared=false;
    CHECK(TryGetFusionObsidian267Damage(913,500,3,0,applied,shared)&&applied==200&&!shared);
    CHECK(TryGetFusionObsidian267Damage(925,500,0,3,applied,shared)&&applied==66&&shared);
    CHECK(TryGetFusionObsidian267Damage(913,1,0,4,applied,shared)&&applied==1&&shared);
    CHECK(!TryGetFusionObsidian267Damage(913,500,0,0,applied,shared));
    CHECK(TryGetFusionObsidian267Damage(931,3200,0,0,applied,shared)&&applied==1000&&!shared);
    CHECK(TryGetFusionObsidian267Damage(906,3200,0,0,applied,shared)&&applied==3200&&!shared);
    CHECK(!TryGetFusionObsidian267Damage(931,-1,0,0,applied,shared));
    uint32_t stored=999;bool explode=false;
    CHECK(TryAdvanceFusionObsidian267Store(999,1,stored,explode)&&stored==1000&&!explode);
    CHECK(TryAdvanceFusionObsidian267Store(1000,1,stored,explode)&&stored==0&&explode);
    CHECK(TryAdvanceFusionObsidian267Store(900,500,stored,explode)&&stored==0&&explode);
    CHECK(!TryAdvanceFusionObsidian267Store(1001,0,stored,explode));
    CHECK(FusionAction267PlantStateValid(925,1000,0,0,0));
    CHECK(!FusionAction267PlantStateValid(925,1001,0,0,0));
    CHECK(!FusionAction267PlantStateValid(913,1,0,0,0));
    int cycle=0;CHECK(TryGetFusionSolar267Cycle(0,1,cycle)&&cycle==300);
    CHECK(TryGetFusionSolar267Cycle(15000,1,cycle)&&cycle==1500);
    CHECK(TryGetFusionSolar267Cycle(30000,1,cycle)&&cycle==1500);
    CHECK(!TryGetFusionSolar267Cycle(0,1.2f,cycle));
    uint16_t item=99;
    CHECK(TryGetFusionSeaSun267Item(0,item)&&item==2);
    CHECK(TryGetFusionSeaSun267Item(1,item)&&item==0);
    CHECK(TryGetFusionSeaSun267Item(2,item)&&item==1);
    CHECK(!TryGetFusionSeaSun267Item(3,item));
    CHECK(FusionAction267PlantStateValid(1077,0,0,0,2));
    CHECK(!FusionAction267PlantStateValid(1077,0,0,0,3));
    CHECK(FusionAction267PlantStateValid(1086,0,0,0,1));
    CHECK(FusionAction267MaximumDuration(1086,.9f)==FusionAction267MaximumDuration(1086,1.1f));
    for(uint16_t kind:{30,916,923,1148,1115,1127,1128,1131,1143,1144,1185,1186})CHECK(IsFusionUmbrellaBatchKind(kind));
    for(uint16_t kind:{915,924,934,65535})CHECK(!IsFusionUmbrellaBatchKind(kind));
    CHECK(FusionUmbrellaSelfDamage267(0)==0&&FusionUmbrellaSelfDamage267(1)==100);
    CHECK(FusionUmbrellaSelfDamage267(2)==150&&FusionUmbrellaSelfDamage267(5)==150&&FusionUmbrellaSelfDamage267(6)==250);
    uint16_t energy=99;
    CHECK(TryApplyFusionLanternUmbrella267Damage(399,2000,applied,energy)&&applied==0&&energy==5);
    CHECK(TryApplyFusionLanternUmbrella267Damage(400,2000,applied,energy)&&applied==0&&energy==0);
    CHECK(TryApplyFusionLanternUmbrella267Damage(401,2000,applied,energy)&&applied==1&&energy==0);
    CHECK(TryApplyFusionLanternUmbrella267Damage(10,0,applied,energy)&&applied==10&&energy==0);
    CHECK(!TryApplyFusionLanternUmbrella267Damage(1,2001,applied,energy));
    CHECK(FusionUmbrellaBatchPlantStateValid(1185,0,0,2000,3));
    CHECK(!FusionUmbrellaBatchPlantStateValid(1185,0,0,2001,3));
    CHECK(FusionUmbrellaBatchPlantStateValid(1144,0,750,0,4));
    CHECK(!FusionUmbrellaBatchPlantStateValid(1144,0,751,0,4));
    CHECK(!FusionUmbrellaBatchPlantStateValid(1143,0,1,0,4));
    CHECK(FusionUmbrellaBatchMoneySkillFee(1144)==2000&&FusionUmbrellaBatchMoneySkillFee(1143)==0);
    CHECK(TryGetFusionUmbrella267Reinforcement(40,false,applied)&&applied==70);
    CHECK(TryGetFusionUmbrella267Reinforcement(41,false,applied)&&applied==71);
    CHECK(TryGetFusionUmbrella267Reinforcement(40,true,applied)&&applied==90);
    CHECK(!TryGetFusionUmbrella267Reinforcement(0,false,applied));
    float vx=0,vy=0,g=0;
    CHECK(TryMakeFusionUmbrella267Retarget(100,200,250,200,60,vx,vy,g)&&vx==3&&std::abs(vy+2.20725f)<.00001f&&std::abs(g-.08829f)<.000001f);
    CHECK(!TryMakeFusionUmbrella267Retarget(100,200,250,200,0,vx,vy,g));
    CHECK(!TryMakeFusionUmbrella267Retarget((std::numeric_limits<double>::infinity)(),200,250,200,60,vx,vy,g));
    for(uint16_t kind:{1,13,14,16,18})CHECK(FusionLily267CanMimic(kind));
    for(uint16_t kind:{0,12,19,1077,65535})CHECK(!FusionLily267CanMimic(kind));
    CHECK(FusionWater267PlantStateValid(12,0,0,65535,0));
    CHECK(FusionWater267PlantStateValid(12,0,0,1,1));
    CHECK(!FusionWater267PlantStateValid(12,0,0,0,0));
    CHECK(!FusionWater267PlantStateValid(12,0,0,1,2));
    CHECK(FusionLily267DoubleBoxKind(927)&&FusionLily267DoubleBoxKind(1174)&&!FusionLily267DoubleBoxKind(931));
    CHECK(kFusionLily267CycleTicks==9001);
#undef CHECK
    return result;
}
