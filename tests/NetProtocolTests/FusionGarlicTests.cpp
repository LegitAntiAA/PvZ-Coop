#include "FusionGarlicTests.h"
#include "../../Lawn/Fusion/FusionGarlicRules.h"
#include "../../Lawn/Fusion/FusionDefenderRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

FusionGarlicTestResult RunFusionGarlicTests()
{
    using namespace LawnFusion;
    FusionGarlicTestResult result{};
#define CHECK(value) do { ++result.mChecks; if (!(value)) { ++result.mFailures; std::printf("Garlic failed line%d: %s\n",__LINE__,#value); } } while (0)
    const auto same = [](const GarlicActionState& a, const GarlicActionState& b)
    {
        return a.phase==b.phase && a.certainRow==b.certainRow && a.waitTicks==b.waitTicks &&
            a.moveElapsedTicks==b.moveElapsedTicks && a.faceTicks==b.faceTicks && a.originRow==b.originRow &&
            a.targetRow==b.targetRow && a.startY==b.startY && a.targetY==b.targetY;
    };
    GarlicBitePlan bite{99,false};
    CHECK(TryGetFusionGarlicBitePlan(29,0xffff,bite) && bite.plantDamage==15 && !bite.certainRow);
    CHECK(TryGetFusionGarlicBitePlan(29,1130,bite) && bite.plantDamage==10 && bite.certainRow);
    CHECK(TryGetFusionGarlicBitePlan(1130,1130,bite) && bite.plantDamage==10 && bite.certainRow);
    CHECK(TryGetFusionGarlicBitePlan(29,1112,bite) && bite.plantDamage==15 && !bite.certainRow);
    bite={99,true}; CHECK(!TryGetFusionGarlicBitePlan(30,1130,bite) && bite.plantDamage==99 && bite.certainRow);
    CHECK(!IsFusionGarlicKind(3) && !IsFusionGarlicKind(1133) && IsFusionGarlicKind(29) && IsFusionGarlicKind(1130));

    struct Choice { int row, normal0, normal1, certain0, certain1; };
    for (const auto row : {Choice{0,1,1,1,1},Choice{1,0,2,2,2},Choice{2,1,3,3,1},Choice{3,2,4,2,2},Choice{4,3,3,3,3}})
    {
        GarlicRowPlan plan;
        CHECK(TryPlanFusionGarlicRow(row.row,5,true,true,false,plan) && plan.randomZeroRow==row.normal0 && plan.randomOneRow==row.normal1);
        CHECK(TryPlanFusionGarlicRow(row.row,5,true,true,true,plan) && plan.randomZeroRow==row.certain0 && plan.randomOneRow==row.certain1);
    }
    GarlicRowPlan row{77,88};
    CHECK(TryPlanFusionGarlicRow(2,5,false,true,false,row) && row.randomZeroRow==3 && row.randomOneRow==3);
    CHECK(TryPlanFusionGarlicRow(2,5,true,false,true,row) && row.randomZeroRow==1 && row.randomOneRow==1);
    CHECK(TryPlanFusionGarlicRow(2,5,false,false,true,row) && row.randomZeroRow==2 && row.randomOneRow==2);
    CHECK(TryPlanFusionGarlicRow(2,6,true,true,true,row) && row.randomZeroRow==3 && row.randomOneRow==3);
    CHECK(TryPlanFusionGarlicRow(3,6,true,true,true,row) && row.randomZeroRow==2 && row.randomOneRow==2);
    row={77,88}; CHECK(!TryPlanFusionGarlicRow(-1,5,true,true,false,row) && row.randomZeroRow==77 && row.randomOneRow==88);
    CHECK(!TryPlanFusionGarlicRow(5,5,true,true,false,row));
    CHECK(!TryPlanFusionGarlicRow(0,1,true,true,false,row) && !TryPlanFusionGarlicRow(0,7,true,true,false,row));

    GarlicActionState state;
    GarlicActionStep step;
    int damage=99;
    CHECK(ValidateFusionGarlicAction(state,5) && !FusionGarlicActionActive(state) && !FusionGarlicPreventsEating(state));
    CHECK(TryBeginFusionGarlicAction(29,0xffff,5,state,damage) && damage==15 && state.waitTicks==100);
    CHECK(FusionGarlicStopsWalking(state) && FusionGarlicPreventsEating(state));
    const auto waiting=state; damage=99;
    CHECK(!TryBeginFusionGarlicAction(29,1130,5,state,damage) && damage==99 && same(state,waiting));
    // The global eat gate has no victim/kind input: all100 waiting ticks
    // protect a newly selected ordinary plant after the Garlic was removed.
    for (int tick=0;tick<99;++tick)
        CHECK(FusionGarlicPreventsEating(state) && TryAdvanceFusionGarlicAction(state,5,false,1,step) && !step.rowStartDue && !step.positionChanged);
    CHECK(state.waitTicks==1);
    CHECK(FusionGarlicPreventsEating(state) && TryAdvanceFusionGarlicAction(state,5,false,1,step) && step.rowStartDue && state.waitTicks==1);
    CHECK(TryStartFusionGarlicRow(state,5,1,2,120,220) && state.faceTicks==150 && state.moveElapsedTicks==0);
    CHECK(!FusionGarlicStopsWalking(state) && FusionGarlicPreventsEating(state));
    CHECK(FusionGarlicLogicalRow(state,1)==2 && FusionGarlicSortingRow(state,1)==1);
    CHECK(TryAdvanceFusionGarlicAction(state,5,false,.4f,step) && step.positionChanged && step.positionY==121 && !step.rowFinished);
    for (int tick=1;tick<99;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,false,.4f,step) && !step.rowFinished);
    CHECK(state.moveElapsedTicks==99 && step.positionY==219);
    CHECK(TryAdvanceFusionGarlicAction(state,5,false,.4f,step) && step.rowFinished && step.positionY==220 && step.targetRow==2);
    CHECK(state.phase==GarlicActionPhase::Idle && state.faceTicks==50 && ValidateFusionGarlicAction(state,5) && !FusionGarlicPreventsEating(state));
    damage=99; CHECK(TryBeginFusionGarlicAction(29,1130,5,state,damage) && damage==10 && state.certainRow && state.faceTicks==50);

    const auto paused=state;
    for (int tick=0;tick<200;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,true,1,step) && same(state,paused) && !step.rowStartDue);
    for (int tick=0;tick<50;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,false,1,step));
    CHECK(state.faceTicks==0 && state.waitTicks==50 && step.faceEnded);
    for (int tick=0;tick<50;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,false,1,step));
    CHECK(step.rowStartDue && TryStartFusionGarlicRow(state,5,2,1,220,120));
    CHECK(FusionGarlicLogicalRow(state,2)==2 && FusionGarlicSortingRow(state,2)==1);
    for (int tick=0;tick<150;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,false,0,step) && !step.positionChanged && state.moveElapsedTicks==0);
    CHECK(state.faceTicks==0 && step.faceEnded && state.phase==GarlicActionPhase::Moving && FusionGarlicPreventsEating(state));
    for (int tick=0;tick<100;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,false,1,step));
    CHECK(step.rowFinished && step.positionY==120 && step.targetRow==1 && !FusionGarlicActionActive(state));

    const auto before=state; step.targetRow=77; damage=99;
    CHECK(!TryAdvanceFusionGarlicAction(state,5,false,-1,step) && same(state,before) && step.targetRow==77);
    CHECK(!TryAdvanceFusionGarlicAction(state,5,false,(std::numeric_limits<float>::quiet_NaN)(),step) && same(state,before));
    CHECK(!TryAdvanceFusionGarlicAction(state,5,false,(std::numeric_limits<float>::infinity)(),step) && same(state,before));
    CHECK(!TryBeginFusionGarlicAction(30,1130,5,state,damage) && same(state,before) && damage==99);
    state.waitTicks=1; CHECK(!ValidateFusionGarlicAction(state,5)); state=before;
    state.phase=static_cast<GarlicActionPhase>(3); CHECK(!ValidateFusionGarlicAction(state,5)); state=before;
    state.faceTicks=151; CHECK(!ValidateFusionGarlicAction(state,5)); state=before;
    state.startY=(std::numeric_limits<float>::quiet_NaN)(); CHECK(!ValidateFusionGarlicAction(state,5)); state=before;
    CHECK(TryBeginFusionGarlicAction(29,0xffff,5,state,damage));
    CHECK(!TryStartFusionGarlicRow(state,5,1,2,120,220)); // Not due: no early row jump.
    for (int tick=0;tick<100;++tick) CHECK(TryAdvanceFusionGarlicAction(state,5,false,1,step));
    const auto due=state;
    CHECK(!TryStartFusionGarlicRow(state,5,1,3,120,320) && same(state,due));
    CHECK(!TryStartFusionGarlicRow(state,5,1,2,120,(std::numeric_limits<float>::infinity)()) && same(state,due));
    CHECK(TryStartFusionGarlicRow(state,5,1,1,120,120) && state.phase==GarlicActionPhase::Idle && state.faceTicks==150);

    uint8_t stage=0;
    CHECK(TryGetFusionDefenderDamageStage(200,300,stage,stage) && stage==0);
    CHECK(TryGetFusionDefenderDamageStage(199,300,stage,stage) && stage==1);
    CHECK(TryGetFusionDefenderDamageStage(100,300,stage,stage) && stage==1);
    CHECK(TryGetFusionDefenderDamageStage(99,300,stage,stage) && stage==2);
    CHECK(TryGetFusionDefenderDamageStage(100,300,stage,stage) && stage==2);
    std::printf("[ %s ] Fusion.SourceGarlicBiteRowAndIndependentRecovery checks=%d failures=%d\n",
        result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
#undef CHECK
    return result;
}
