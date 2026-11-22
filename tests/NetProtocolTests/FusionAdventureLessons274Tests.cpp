#include "FusionAdventureLessons274Tests.h"
#include "../../Lawn/Fusion/FusionAdventureProgress273.h"
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <type_traits>

namespace
{
using namespace LawnFusion;
using M=FusionAdventureMessage273;
using A=FusionAdventureMissionAction274;
FusionAdventureLessons274TestResult result;
void Check(bool ok,int line)
{++result.mChecks;if(!ok){++result.mFailures;std::printf("    AdventureLessons274 failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
FusionAdventureRun273 Begin(int level,bool disabled=false,uint32_t cycle=1)
{
    FusionAdventureProfile273 profile;profile.frontierLevel=uint16_t(level);profile.cycle=cycle;
    FusionAdventureRun273 run;CHECK(BeginFusionAdventureRun273(profile,level,disabled,run));
    CHECK(FusionAdventureRunValid273(run));return run;
}
void Step(FusionAdventureRun273& run,uint32_t ticks,const FusionAdventureLessonInput274& input,
    FusionAdventureTutorialEffects273& effects)
{CHECK(StepFusionAdventureTutorial273(run,ticks,input,effects));CHECK(FusionAdventureRunValid273(run));}
bool NoAction(const FusionAdventureTutorialEffects273& e)
{return e.missionAction==A::None&&!e.grantMissionCards&&!e.completed&&!e.nextWaveTicks;}
}

FusionAdventureLessons274TestResult RunFusionAdventureLessons274Tests()
{
    result={};std::printf("[ RUN      ] Fusion.AdventureLessons274\n");
    static_assert(std::is_trivially_copyable_v<FusionAdventureRun273>);
    static_assert(uint8_t(M::Ready)==7); // Existing source1 message wire values stay unchanged.
    static_assert(uint8_t(M::EquipmentReady)==21&&uint8_t(M::PuffIntro)==22&&uint8_t(M::PuffReady)==25);
    static_assert(uint8_t(M::FertilizeIntro)==26&&uint8_t(M::IceReady)==38&&uint8_t(A::Prepare13Nut)==7&&uint8_t(A::Clear15)==10);
    static_assert(sizeof(M)==1&&sizeof(A)==1);
    FusionAdventureLessonInput274 input;FusionAdventureTutorialEffects273 e;
    // The full source activation subset, independently from public campaign admission.
    for(int level=1;level<=45;++level)
    {
        const bool lesson=level==1||level==4||level==7||level==8||level==10||level==13||level==15||
            level==22||level==24||level==30||level==35||level==42;
        auto run=Begin(level);CHECK(run.tutorial.active==lesson);
        run=Begin(level,true);CHECK(!run.tutorial.active&&run.tutorial.stage==0);
        run=Begin(level,false,2);CHECK(!run.tutorial.active&&run.tutorial.message==M::None);
    }
    FusionAdventureProfile273 profile;profile.frontierLevel=9;profile.completedLevels=uint64_t{1}<<3;
    FusionAdventureRun273 replay;CHECK(BeginFusionAdventureRun273(profile,4,false,replay));
    CHECK(replay.replay&&replay.tutorial.active); // Source does not suppress first-cycle replay tutorials.

    // Source4:400+500 ticks, then exact original mission actor column<=4.
    auto run=Begin(4);CHECK(FusionAdventureCardsIgnoreExpiry273(run));
    Step(run,0,input,e);CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==400&&run.tutorial.message==M::GloveUnlocked);
    CHECK(e.messageChanged&&NoAction(e));
    Step(run,399,input,e);CHECK(run.tutorial.timerTicks==1&&!e.messageChanged);
    Step(run,1,input,e);CHECK(run.tutorial.stage==2&&run.tutorial.timerTicks==500&&run.tutorial.message==M::GloveMove);
    Step(run,499,input,e);CHECK(run.tutorial.timerTicks==1&&NoAction(e));
    Step(run,1,input,e);CHECK(run.tutorial.stage==3&&run.tutorial.messageTicks==6000&&run.tutorial.timerTicks==0);
    CHECK(run.tutorial.message==M::MoveTwinFlower&&e.missionAction==A::Prepare4&&!e.grantMissionCards);
    input.missionPlantColumn=0;input.hasBucketPlant1020or1029=true;
    Step(run,1,input,e);CHECK(run.tutorial.active&&run.tutorial.stage==3&&NoAction(e)); // A destroyed tracked actor is not another flower.
    input.hasMissionPlant=true;input.missionPlantColumn=9;
    Step(run,1,input,e);CHECK(run.tutorial.active&&NoAction(e));
    input.missionPlantColumn=5;Step(run,1,input,e);CHECK(run.tutorial.active); // Column, not HP or mere actor presence.
    CHECK(!TryArmFusionAdventureReward273(run,20,20,0,false));
    input.missionPlantColumn=4;Step(run,1,input,e);
    CHECK(!run.tutorial.active&&run.tutorial.stage==3&&run.tutorial.message==M::GloveReady);
    CHECK(e.completed&&e.nextWaveTicks==500&&e.missionAction==A::None&&run.tutorial.messageTicks==500);
    CHECK(!FusionAdventureCardsIgnoreExpiry273(run));
    Step(run,500,input,e);CHECK(!e.messageChanged&&NoAction(e)&&!run.tutorial.messageTicks);
    CHECK(TryArmFusionAdventureReward273(run,20,20,0,false));

    // Large dt cannot consume multiple source switch cases or repeat prepare effects.
    input={};run=Begin(4);
    Step(run,(std::numeric_limits<uint32_t>::max)(),input,e);CHECK(run.tutorial.stage==1&&NoAction(e));
    Step(run,(std::numeric_limits<uint32_t>::max)(),input,e);CHECK(run.tutorial.stage==2&&NoAction(e));
    Step(run,(std::numeric_limits<uint32_t>::max)(),input,e);CHECK(run.tutorial.stage==3&&e.missionAction==A::Prepare4);
    Step(run,0,input,e);CHECK(run.tutorial.stage==3&&NoAction(e));
    input.hasMissionPlant=true;input.missionPlantColumn=-1;Step(run,0,input,e);
    CHECK(e.completed); // Source predicate is literally<=4, without a row/owner/range condition.

    // Source7: exact missionPlant destruction, then actual missionZombie destruction.
    input={};run=Begin(7);Step(run,1,input,e);
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==600&&run.tutorial.messageTicks==600&&run.tutorial.message==M::PresentIntro);
    Step(run,599,input,e);CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==1);
    Step(run,1,input,e);CHECK(run.tutorial.stage==2&&run.tutorial.message==M::UsePresent&&run.tutorial.messageTicks==6000);
    CHECK(e.missionAction==A::Prepare7&&!e.grantMissionCards);
    input.hasMissionPlant=true;input.hasBucketPlant1020or1029=true;
    Step(run,100000,input,e);CHECK(run.tutorial.stage==2&&run.tutorial.active&&NoAction(e));
    input.hasMissionPlant=false;Step(run,1,input,e);
    CHECK(run.tutorial.stage==3&&run.tutorial.timerTicks==300&&run.tutorial.message==M::PresentReady&&NoAction(e));
    Step(run,299,input,e);CHECK(run.tutorial.stage==3&&run.tutorial.timerTicks==1);
    Step(run,1,input,e);CHECK(run.tutorial.stage==4&&run.tutorial.message==M::CherryPaperWarning&&run.tutorial.messageTicks==500);
    CHECK(e.missionAction==A::Spawn7EnemyAndCard&&!e.grantMissionCards);
    input.hasMissionZombie=true;input.missionZombieBeforeDying=true;input.missionZombieDying=true;
    Step(run,100000,input,e);CHECK(run.tutorial.active&&run.tutorial.stage==4&&NoAction(e));
    input.hasMissionZombie=false;Step(run,1,input,e);
    CHECK(!run.tutorial.active&&run.tutorial.message==M::PresentDefenseReady&&run.tutorial.messageTicks==500);
    CHECK(e.completed&&e.nextWaveTicks==500&&e.missionAction==A::None);
    Step(run,1,input,e);CHECK(NoAction(e)&&!e.messageChanged);

    // Source8's early death gate is intentionally different: null must NOT pass.
    input={};run=Begin(8);Step(run,0,input,e);
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==400&&run.tutorial.message==M::EquipmentIntro);
    Step(run,400,input,e);CHECK(run.tutorial.stage==2&&run.tutorial.message==M::BucketNutWarning&&run.tutorial.messageTicks==6000);
    CHECK(e.missionAction==A::Prepare8&&!e.grantMissionCards);
    input.missionZombieBeforeDying=true;input.missionZombieDying=true;input.hasBucketPlant1020or1029=true;
    Step(run,1,input,e);CHECK(run.tutorial.active&&run.tutorial.stage==2&&NoAction(e));
    input.hasMissionZombie=true;input.missionZombieBeforeDying=false;input.missionZombieDying=false;
    Step(run,1,input,e);CHECK(run.tutorial.stage==2&&NoAction(e));
    input.missionZombieBeforeDying=true;Step(run,1,input,e);
    CHECK(run.tutorial.stage==3&&run.tutorial.message==M::UseBucket&&run.tutorial.timerTicks==0&&NoAction(e));
    // Existing Iron plant cannot skip the separate stage3 update after early death.
    input.hasMissionZombie=false;Step(run,0,input,e);
    CHECK(run.tutorial.stage==4&&run.tutorial.timerTicks==300&&run.tutorial.message==M::IronPlants);
    CHECK(run.tutorial.active&&NoAction(e)&&!TryArmFusionAdventureReward273(run,20,20,0,false));
    Step(run,299,input,e);CHECK(run.tutorial.stage==4&&run.tutorial.timerTicks==1&&run.tutorial.active);
    Step(run,1,input,e);CHECK(!run.tutorial.active&&run.tutorial.message==M::EquipmentReady&&run.tutorial.messageTicks==500);
    CHECK(e.completed&&e.nextWaveTicks==500&&e.missionAction==A::None);
    Step(run,500,input,e);CHECK(NoAction(e)&&!run.tutorial.messageTicks);
    // status1 alone also passes; Iron readiness is its own independent gate.
    input={};run=Begin(8);Step(run,0,input,e);Step(run,400,input,e);
    input.hasMissionZombie=true;input.missionZombieDying=true;Step(run,0,input,e);
    CHECK(run.tutorial.stage==3&&run.tutorial.message==M::UseBucket);
    Step(run,100000,input,e);CHECK(run.tutorial.stage==3&&NoAction(e));
    input.hasBucketPlant1020or1029=true;Step(run,0,input,e);CHECK(run.tutorial.stage==4&&run.tutorial.timerTicks==300);

    // Source10 is a timed demonstration: one case per update, even with a
    // large delta. Native actions must commit before the candidate is adopted.
    input={};run=Begin(10);Step(run,0,input,e);
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==400&&run.tutorial.message==M::PuffIntro);
    CHECK(NoAction(e)&&e.messageChanged&&FusionAdventureCardsIgnoreExpiry273(run));
    Step(run,399,input,e);CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==1&&NoAction(e));
    Step(run,1,input,e);
    CHECK(run.tutorial.stage==2&&run.tutorial.timerTicks==400&&run.tutorial.message==M::PuffStack&&run.tutorial.messageTicks==400);
    CHECK(e.missionAction==A::Prepare10&&!e.grantMissionCards&&!e.completed&&!e.nextWaveTicks);
    auto beforeNativeCommit=run;auto candidate=run;Step(candidate,400,input,e);
    CHECK(e.missionAction==A::Mix10&&candidate.tutorial.stage==3&&candidate.tutorial.timerTicks==400);
    CHECK(run.tutorial.stage==beforeNativeCommit.tutorial.stage&&run.tutorial.timerTicks==400);
    run=candidate;CHECK(run.tutorial.message==M::PuffMix&&run.tutorial.messageTicks==400&&run.tutorial.active);
    Step(run,399,input,e);CHECK(run.tutorial.stage==3&&run.tutorial.timerTicks==1&&NoAction(e));
    Step(run,1,input,e);
    CHECK(!run.tutorial.active&&run.tutorial.stage==3&&run.tutorial.timerTicks==0&&run.tutorial.message==M::PuffReady);
    CHECK(run.tutorial.messageTicks==500&&e.completed&&e.nextWaveTicks==500&&e.missionAction==A::None&&!e.grantMissionCards);
    CHECK(!FusionAdventureCardsIgnoreExpiry273(run));Step(run,500,input,e);CHECK(!run.tutorial.messageTicks&&NoAction(e));
    run=Begin(10);Step(run,100000,input,e);
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==400&&NoAction(e));
    Step(run,100000,input,e);CHECK(run.tutorial.stage==2&&run.tutorial.timerTicks==400&&e.missionAction==A::Prepare10);
    Step(run,100000,input,e);CHECK(run.tutorial.stage==3&&run.tutorial.timerTicks==400&&e.missionAction==A::Mix10);
    Step(run,100000,input,e);CHECK(!run.tutorial.active&&run.tutorial.messageTicks==500&&e.completed&&e.nextWaveTicks==500);
    for(const auto message:{M::PuffIntro,M::PuffStack,M::PuffMix})CHECK(FusionAdventureMessageDuration273(message)==400);
    CHECK(FusionAdventureMessageDuration273(M::PuffReady)==500);
    // Source13: four timed messages, real Nut/Fertilize action, any1027,
    // then damage/repair of the EXACT tracked1012, not another healthy actor.
    input={};run=Begin(13);Step(run,0,input,e);
    const M timed13[]={M::FertilizeIntro,M::FertilizeGrow,M::FertilizeUpgrade,M::FertilizeLesson};
    for(unsigned i=0;i<4;++i)
    {
        CHECK(run.tutorial.stage==i+1&&run.tutorial.message==timed13[i]&&run.tutorial.timerTicks==400&&NoAction(e));
        Step(run,399,input,e);CHECK(run.tutorial.timerTicks==1&&NoAction(e));
        Step(run,1,input,e);
    }
    CHECK(run.tutorial.stage==5&&run.tutorial.message==M::UseFertilizeNut&&run.tutorial.timerTicks==0&&run.tutorial.messageTicks==6000);
    CHECK(e.missionAction==A::Prepare13Nut&&!e.completed&&!e.grantMissionCards);
    input.hasMissionPlant=true;input.missionPlantFullyRepaired=true;Step(run,60000,input,e);
    CHECK(run.tutorial.stage==5&&run.tutorial.active&&NoAction(e)); // A healthy actor is not the1027 condition.
    input.hasTallNut1027=true;Step(run,0,input,e);
    CHECK(run.tutorial.stage==6&&run.tutorial.message==M::FertilizeNutReady&&run.tutorial.timerTicks==400&&NoAction(e));
    Step(run,399,input,e);CHECK(run.tutorial.timerTicks==1&&NoAction(e));
    beforeNativeCommit=run;candidate=run;Step(candidate,1,input,e);
    CHECK(e.missionAction==A::Prepare13Chomper&&candidate.tutorial.stage==7&&candidate.tutorial.message==M::RepairNutChomper);
    CHECK(run.tutorial.stage==beforeNativeCommit.tutorial.stage&&run.tutorial.timerTicks==1);run=candidate;
    input.hasMissionPlant=false;input.missionPlantFullyRepaired=true;Step(run,60000,input,e);
    CHECK(run.tutorial.stage==7&&run.tutorial.active&&NoAction(e)); // Replacement/another healthy1012 must not pass.
    input.hasMissionPlant=true;input.missionPlantFullyRepaired=false;Step(run,1,input,e);
    CHECK(run.tutorial.stage==7&&NoAction(e));
    input.missionPlantFullyRepaired=true;Step(run,0,input,e);
    CHECK(run.tutorial.stage==8&&run.tutorial.message==M::FertilizeRepairReady&&run.tutorial.timerTicks==400&&NoAction(e));
    Step(run,399,input,e);CHECK(run.tutorial.active&&run.tutorial.timerTicks==1);
    Step(run,1,input,e);CHECK(!run.tutorial.active&&run.tutorial.message==M::FertilizeReady&&e.completed&&e.nextWaveTicks==500&&e.missionAction==A::None);
    CHECK(!FusionAdventureTutorialShowsZombieHealth284(run));
    Step(run,500,input,e);CHECK(run.tutorial.messageTicks==0&&NoAction(e));
    input={};run=Begin(13);
    for(int stage=1;stage<=5;++stage){Step(run,100000,input,e);CHECK(run.tutorial.stage==stage);}
    CHECK(e.missionAction==A::Prepare13Nut); // Large dt still cannot prepare both source groups at once.
    // Source15 waits on the whole source enemy count, not just its three Cone
    // identities. HP visibility is derived and clears at the actual count gate.
    input={};input.sourceEnemyCount=7;run=Begin(15);
    CHECK(!FusionAdventureTutorialShowsZombieHealth284(run));Step(run,0,input,e);
    CHECK(run.tutorial.stage==1&&run.tutorial.message==M::IceIntro&&run.tutorial.timerTicks==400&&NoAction(e));
    Step(run,399,input,e);CHECK(run.tutorial.timerTicks==1&&!FusionAdventureTutorialShowsZombieHealth284(run));
    Step(run,1,input,e);CHECK(run.tutorial.stage==2&&run.tutorial.message==M::IceDefense&&run.tutorial.timerTicks==0&&run.tutorial.messageTicks==6000);
    CHECK(e.missionAction==A::Prepare15&&FusionAdventureTutorialShowsZombieHealth284(run));
    for(int enemies:{7,3,1,-1})
    {input.sourceEnemyCount=enemies;Step(run,100000,input,e);CHECK(run.tutorial.stage==2&&NoAction(e)&&FusionAdventureTutorialShowsZombieHealth284(run));}
    input.sourceEnemyCount=0;Step(run,0,input,e);
    CHECK(run.tutorial.stage==3&&run.tutorial.message==M::IceLessonReady&&run.tutorial.timerTicks==400&&NoAction(e));
    CHECK(!FusionAdventureTutorialShowsZombieHealth284(run));Step(run,399,input,e);CHECK(run.tutorial.timerTicks==1);
    beforeNativeCommit=run;candidate=run;Step(candidate,1,input,e);
    CHECK(e.missionAction==A::Clear15&&e.completed&&e.nextWaveTicks==500&&!candidate.tutorial.active&&candidate.tutorial.message==M::IceReady);
    CHECK(run.tutorial.active&&run.tutorial.stage==beforeNativeCommit.tutorial.stage&&run.tutorial.timerTicks==1);
    run=candidate;CHECK(!FusionAdventureTutorialShowsZombieHealth284(run));Step(run,500,input,e);CHECK(!run.tutorial.messageTicks&&NoAction(e));
    input={};run=Begin(15);Step(run,100000,input,e);CHECK(run.tutorial.stage==1&&NoAction(e));
    Step(run,100000,input,e);CHECK(run.tutorial.stage==2&&e.missionAction==A::Prepare15); // No immediate zero-count shortcut in the same case.
    Step(run,100000,input,e);CHECK(run.tutorial.stage==3&&NoAction(e));Step(run,100000,input,e);CHECK(e.completed&&e.missionAction==A::Clear15);
    for(const auto message:{M::FertilizeIntro,M::FertilizeGrow,M::FertilizeUpgrade,M::FertilizeLesson,
        M::FertilizeNutReady,M::FertilizeRepairReady,M::IceIntro,M::IceLessonReady})CHECK(FusionAdventureMessageDuration273(message)==400);
    for(const auto message:{M::UseFertilizeNut,M::RepairNutChomper,M::IceDefense})CHECK(FusionAdventureMessageDuration273(message)==6000);
    CHECK(FusionAdventureMessageDuration273(M::FertilizeReady)==500&&FusionAdventureMessageDuration273(M::IceReady)==500);
    // Level-aware validation rejects wrong stage/message/timer combinations.
    for(int level:{4,7,8,10,13,15})
    {
        input={};auto valid=Begin(level);Step(valid,0,input,e);
        auto bad=valid;bad.tutorial.message=M::Welcome;CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.tutorial.timerTicks=uint16_t(valid.tutorial.timerTicks+1);CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.tutorial.messageTicks=uint16_t(FusionAdventureMessageDuration273(valid.tutorial.message)+1);CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.tutorial.active=false;CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.cycle=2;CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.level=2;CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.tutorial.stage=6;CHECK(!FusionAdventureRunValid273(bad));
        bad=valid;bad.tutorial.message=M(255);CHECK(!FusionAdventureRunValid273(bad));
        e.nextWaveTicks=17;e.missionAction=A::Prepare8;e.messageChanged=true;
        CHECK(!StepFusionAdventureTutorial273(bad,1000,input,e));
        CHECK(bad.tutorial.message==M(255)&&bad.tutorial.stage==1&&e.nextWaveTicks==17&&e.missionAction==A::Prepare8&&e.messageChanged);
        // The unchanged native source1 adapter may not silently consume a new lesson's mission action.
        CHECK(!StepFusionAdventureTutorial273(valid,1000,true,true,true,e));
        CHECK(valid.tutorial.stage==1&&valid.tutorial.timerTicks==FusionAdventureMessageDuration273(valid.tutorial.message)&&e.nextWaveTicks==17);
    }
    const uint16_t durations[]={400,500,6000,500,600,6000,300,500,500,400,6000,6000,300,500};
    for(unsigned i=0;i<sizeof(durations)/sizeof(durations[0]);++i)
        CHECK(FusionAdventureMessageDuration273(M(uint8_t(M::GloveUnlocked)+i))==durations[i]);
    CHECK(FusionAdventureMessageDuration273(M(255))==0);
    // Existing source1 API still advances only its own original message/stage contract.
    run=Begin(1);e={};CHECK(StepFusionAdventureTutorial273(run,1,false,false,false,e));
    CHECK(run.tutorial.stage==1&&run.tutorial.message==M::Welcome&&run.tutorial.timerTicks==300&&e.missionAction==A::None);
    e.nextWaveTicks=17;CHECK(!StepFusionAdventureTutorial273(run,100000,input,e));
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==300&&e.nextWaveTicks==17);
    std::printf("[ %s ] Fusion.AdventureLessons274 checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
