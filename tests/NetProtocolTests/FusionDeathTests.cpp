#include "FusionDeathTests.h"
#include "../../Lawn/Fusion/FusionDeathRules.h"
#include "../../Lawn/Fusion/FusionCatalog.h"
#include "../../Lawn/Fusion/FusionRecipe.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionDeathTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Death check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
bool Near(double a, double b) { return std::abs(a-b) < 1.0e-8; }
}

FusionDeathTestResult RunFusionDeathTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.NativeDeathSourceBandsBlastsAndRecovery\n");
    struct Definition { uint16_t kind, first, second; int hp; bool mines; };
    for (const auto v : {Definition{1003,2,3,4000,false},Definition{1010,3,4,4000,false},Definition{1162,2,4,300,true}})
    {
        const auto* profile = FindFusionDeathProfile(v.kind);
        const auto* catalog = FusionFindPlant(v.kind);
        const auto* forward = FindFusionRecipe(v.first,v.second);
        const auto* reverse = FindFusionRecipe(v.second,v.first);
        CHECK(profile && profile->kind == v.kind && profile->maxHealth == v.hp && profile->createsMines == v.mines);
        CHECK(catalog && catalog->hasStats && catalog->maxHealth == v.hp);
        CHECK(forward && forward->result == v.kind && reverse && reverse->result == v.kind);
    }
    for(uint16_t kind:{uint16_t(1015),uint16_t(1163),uint16_t(1170),uint16_t(1190)}) CHECK(FindFusionDeathProfile(kind));
    for (uint16_t kind : {uint16_t(2),uint16_t(3),uint16_t(4),uint16_t(1006),uint16_t(65535)})
        CHECK(!FindFusionDeathProfile(kind)); // No native alias or family-based phantom support.
    for (int reason = 0; reason <= 7; ++reason) CHECK(FusionDeathReasonHasEffects(reason));
    for (int reason : {-1,8,255,256}) CHECK(!FusionDeathReasonHasEffects(reason));
    for (uint8_t flags : {uint8_t(0),uint8_t(1),uint8_t(2),uint8_t(3)}) CHECK(FusionDeathFlagsValid(1010,flags));
    CHECK(!FusionDeathFlagsValid(1010,4) && !FusionDeathFlagsValid(1003,1) && !FusionDeathFlagsValid(1162,2));
    CHECK(FusionDeathFlagsValid(1003,0) && FusionDeathFlagsValid(1162,0));

    struct Band { int health; uint8_t before, after; int damage; };
    for (const auto v : {Band{4000,0,0,0},Band{2667,0,0,0},Band{2666,0,0,0},Band{2665,0,1,500},
        Band{1334,0,1,500},Band{1333,0,0,0},Band{1332,0,2,500},Band{0,0,2,500},Band{-1,0,2,500},
        Band{2665,1,1,0},Band{1332,2,2,0},Band{1332,1,3,500},Band{2000,2,3,500},Band{4000,3,3,0}})
    {
        uint8_t flags = v.before; int damage = -1;
        CHECK(TryConsumeFusionPotatoNutThreshold(v.health,4000,flags,damage) && flags == v.after && damage == v.damage);
    }
    uint8_t flags = 0; int damage = 77;
    CHECK(TryConsumeFusionPotatoNutThreshold(1000,4000,flags,damage) && flags == 2 && damage == 500);
    CHECK(TryConsumeFusionPotatoNutThreshold(4000,4000,flags,damage) && flags == 2 && damage == 0);
    CHECK(TryConsumeFusionPotatoNutThreshold(2000,4000,flags,damage) && flags == 3 && damage == 500);
    CHECK(TryConsumeFusionPotatoNutThreshold(0,4000,flags,damage) && flags == 3 && damage == 0);
    flags = 1; damage = 77;
    CHECK(!TryConsumeFusionPotatoNutThreshold(1000,0,flags,damage) && flags == 1 && damage == 77);
    flags = 4;
    CHECK(!TryConsumeFusionPotatoNutThreshold(1000,4000,flags,damage) && flags == 4 && damage == 77);
    flags = 0;
    CHECK(TryConsumeFusionPotatoNutThreshold(1431655763,(std::numeric_limits<int>::max)(),flags,damage) && flags == 1 && damage == 500);
    CHECK(TryConsumeFusionPotatoNutThreshold(0,4000,flags,damage) && damage == 500); // Lethal low500 is independent of death1800.

    int healed = 77;
    CHECK(TryApplyFusionDeathHeal(1000,4000,1000,healed) && healed == 2000);
    CHECK(TryApplyFusionDeathHeal(3500,4000,1000,healed) && healed == 4000);
    CHECK(TryApplyFusionDeathHeal(4000,4000,1000,healed) && healed == 4000);
    CHECK(TryApplyFusionDeathHeal(1,(std::numeric_limits<int>::max)(),(std::numeric_limits<int>::max)(),healed) && healed == (std::numeric_limits<int>::max)());
    for (const auto v : {Band{-1,0,0,1000},Band{4001,0,0,1000}})
    {
        healed = 77; CHECK(!TryApplyFusionDeathHeal(v.health,4000,v.damage,healed) && healed == 77);
    }
    healed = 77;
    CHECK(!TryApplyFusionDeathHeal(1000,4000,-1,healed) && healed == 77);

    NativeBlastSpec cherry, potato, threshold;
    CHECK(TryMakeFusionDeathBlast(1003,320,250,2,false,cherry) && cherry.policy == FusionDeathBlastPolicy::Cherry &&
        Near(cherry.centerX,320) && Near(cherry.centerY,202.09580838323353) && cherry.damage == 1800 && cherry.shake);
    CHECK(TryMakeFusionDeathBlast(1162,320,250,2,false,potato) && potato.policy == FusionDeathBlastPolicy::SmallPotato &&
        Near(potato.centerY,232.03592814371257) && potato.damage == 1800 && potato.shake);
    CHECK(TryMakeFusionDeathBlast(1010,320,250,2,true,threshold) && threshold.damage == 500 && !threshold.shake &&
        Near(threshold.centerY,potato.centerY));
    CHECK(FusionDeathBlastAffectsRow(cherry,1) && FusionDeathBlastAffectsRow(cherry,3) && !FusionDeathBlastAffectsRow(cherry,0));
    CHECK(FusionDeathBlastAffectsRow(potato,2) && !FusionDeathBlastAffectsRow(potato,1));
    CHECK(FusionDeathBlastRectOverlaps(cherry,cherry.centerX+106.66666666666667,cherry.centerY,0,0));
    CHECK(!FusionDeathBlastRectOverlaps(cherry,cherry.centerX+106.667,cherry.centerY,0,0));
    CHECK(FusionDeathBlastRectOverlaps(cherry,cherry.centerX,cherry.centerY+119.76047904191617,0,0));
    CHECK(!FusionDeathBlastRectOverlaps(cherry,cherry.centerX,cherry.centerY+119.761,0,0));
    CHECK(FusionDeathBlastRectOverlaps(potato,potato.centerX+53.333333333333336,potato.centerY,0,0));
    CHECK(!FusionDeathBlastRectOverlaps(potato,potato.centerX+53.334,potato.centerY,0,0));
    CHECK(!FusionDeathBlastRectOverlaps(potato,potato.centerX,potato.centerY,-1,1));
    CHECK(TryMakeFusionDeathBlast(1003,1,2,5,false,cherry) && cherry.row == 5 &&
        FusionDeathBlastAffectsRow(cherry,4) && FusionDeathBlastAffectsRow(cherry,5) && !FusionDeathBlastAffectsRow(cherry,6));
    NativeBlastSpec before = cherry;
    CHECK(!TryMakeFusionDeathBlast(3,1,2,0,false,cherry) && cherry.centerX == before.centerX);
    CHECK(!TryMakeFusionDeathBlast(1003,1,2,0,true,cherry) && cherry.damage == before.damage);
    CHECK(!TryMakeFusionDeathBlast(1003,1,2,6,false,cherry) && cherry.row == before.row);
    CHECK(!TryMakeFusionDeathBlast(1003,(std::numeric_limits<double>::quiet_NaN)(),2,0,false,cherry) && cherry.centerX == before.centerX);

    // Original two projected boxes have different right edges. At x133 only
    // the larger box intersects radius2; unconditional x1 or x2 both fail.
    cherry.centerX = 0; cherry.centerY = 200; cherry.row = 2;
    CHECK(FusionCherryNutHealOverlapCount(cherry,0,250,2) == 2);
    cherry.centerX = 133;
    CHECK(FusionCherryNutHealOverlapCount(cherry,0,250,2) == 1);
    cherry.centerX = 150;
    CHECK(FusionCherryNutHealOverlapCount(cherry,0,250,2) == 0);
    cherry.centerX = 0;
    CHECK(FusionCherryNutHealOverlapCount(cherry,0,250,0) == 0);
    CHECK(FusionCherryNutHealOverlapCount(potato,0,250,2) == 0);

    std::array<FusionDeathSpawnCell,9> cells{}; size_t count = 77;
    CHECK(TryGetFusionDeathSpawnCells(5,2,10,5,cells,count) && count == 9);
    for (size_t i = 0; i < count; ++i)
        CHECK(cells[i].column == 4+static_cast<int>(i/3) && cells[i].row == 1+static_cast<int>(i%3));
    CHECK(TryGetFusionDeathSpawnCells(0,0,10,5,cells,count) && count == 4 && cells[0].column == 0 && cells[0].row == 0 &&
        cells[1].column == 0 && cells[1].row == 1 && cells[2].column == 1 && cells[2].row == 0);
    CHECK(TryGetFusionDeathSpawnCells(9,2,10,5,cells,count) && count == 6 && cells[5].column == 9 && cells[5].row == 3);
    CHECK(TryGetFusionDeathSpawnCells(9,4,10,5,cells,count) && count == 4 && cells[3].column == 9 && cells[3].row == 4);
    const auto oldCells = cells; const size_t oldCount = count;
    CHECK(!TryGetFusionDeathSpawnCells(10,0,10,5,cells,count) && count == oldCount && cells[0].column == oldCells[0].column);
    CHECK(!TryGetFusionDeathSpawnCells(0,5,10,5,cells,count) && count == oldCount);
    CHECK(!TryGetFusionDeathSpawnCells(0,0,0,5,cells,count) && count == oldCount);
    CHECK(kFusionDeathMinePreparationTicks == 501);
    std::printf("[ %s ] Fusion.NativeDeathSourceBandsBlastsAndRecovery checks=%d failures=%d\n",
        result.mFailures ? " FAILED " : "       OK",result.mChecks,result.mFailures);
    return result;
}
