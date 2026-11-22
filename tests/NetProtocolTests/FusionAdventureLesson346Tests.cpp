#include "FusionAdventureLesson346Tests.h"
#include "../../Lawn/Fusion/FusionAdventureLesson346Rules.h"
#include "../../Lawn/Fusion/FusionAdventureSession273.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

using namespace LawnFusion;
FusionAdventureLesson346TestResults RunFusionAdventureLesson346Tests()
{
    FusionAdventureLesson346TestResults result;
#define CHECK(x) do{++result.mChecks;if(!(x)){++result.mFailures;std::printf("Lesson346 line%d failed: %s\n",__LINE__,#x);}}while(0)
    const std::array<int,5> levels={22,24,30,35,42};
    for(int level:levels)
    {
        const auto steps=FusionAdventureLesson346Steps(level);CHECK(!steps.empty());
        FusionAdventureProfile273 profile;profile.frontierLevel=uint16_t(level);
        FusionAdventureRun273 run;CHECK(BeginFusionAdventureRun273(profile,level,false,run)&&run.tutorial.active);
        CHECK(FusionAdventureRunValid273(run));
        FusionAdventureRun273 disabled;CHECK(BeginFusionAdventureRun273(profile,level,true,disabled)&&!disabled.tutorial.active);
        profile.cycle=2;CHECK(BeginFusionAdventureRun273(profile,level,false,disabled)&&!disabled.tutorial.active);
        FusionAdventureLessonInput274 input;FusionAdventureTutorialEffects273 effect;
        for(size_t stage=0;stage<steps.size();++stage)
        {
            CHECK(run.tutorial.stage==stage&&run.tutorial.active);
            const auto& step=steps[stage];
            if(run.tutorial.timerTicks)
            {
                const auto before=run;CHECK(StepFusionAdventureTutorial273(run,run.tutorial.timerTicks-1,input,effect));
                CHECK(run.tutorial.stage==before.tutorial.stage&&run.tutorial.timerTicks==1&&!effect.messageChanged);
            }
            if(step.condition!=FusionLesson346Condition::Always)
            {
                input.hasMissionZombie=true;input.sourceEnemyCount=1;input.hasGoldCabbage1135=false;
                const uint8_t before=run.tutorial.stage;
                CHECK(StepFusionAdventureTutorial273(run,100000,input,effect));
                CHECK(run.tutorial.stage==before&&run.tutorial.active&&!effect.completed&&!effect.messageChanged);
                // beforeDying/status1/HP do not substitute for Unity destruction.
                input.missionZombieBeforeDying=input.missionZombieDying=true;
                CHECK(StepFusionAdventureTutorial273(run,0,input,effect)&&run.tutorial.stage==before);
                if(step.condition==FusionLesson346Condition::NoEnemies)
                {input.sourceEnemyCount=-1;CHECK(StepFusionAdventureTutorial273(run,0,input,effect)&&run.tutorial.stage==before);}
            }
            input.hasMissionZombie=false;input.sourceEnemyCount=0;input.hasGoldCabbage1135=true;
            CHECK(StepFusionAdventureTutorial273(run,100000,input,effect));
            CHECK(effect.messageChanged&&run.tutorial.message==step.message&&run.tutorial.messageTicks==step.messageTicks);
            CHECK(run.tutorial.timerTicks==step.timerTicks);
            CHECK(FusionAdventureMessageDuration273(step.message)==step.messageTicks);
            CHECK((effect.missionAction==FusionAdventureMissionAction274::ExecuteSourceLesson346)==!step.operations.empty());
            CHECK(FusionAdventureRunValid273(run));
            if(step.complete)
            {CHECK(!run.tutorial.active&&effect.completed&&effect.nextWaveTicks==500&&run.tutorial.stage==stage);}
            else CHECK(run.tutorial.active&&run.tutorial.stage==stage+1&&!effect.completed);
        }
        CHECK(!run.tutorial.active);
        CHECK(StepFusionAdventureTutorial273(run,500,input,effect)&&!effect.completed&&run.tutorial.messageTicks==0);
        auto invalid=run;invalid.tutorial.stage=255;CHECK(!FusionAdventureRunValid273(invalid));
        invalid=run;invalid.tutorial.message=FusionAdventureMessage273::MoneyLink;invalid.tutorial.messageTicks=65535;
        CHECK(!FusionAdventureRunValid273(invalid));
        CHECK(TryArmFusionAdventureReward273(run,1,1,0,false));
        CHECK(BindFusionAdventureTrophy273(run,72,98304));
        profile.cycle=1;CHECK(TryCollectFusionAdventureReward273(profile,run,72,98304));
        CHECK(profile.frontierLevel==level+1&&FusionAdventureLevelCompleted273(profile,level));
    }
    using K=FusionLesson346OpKind;
    const auto count=[](const auto& step,K operation){return std::count_if(step.operations.begin(),step.operations.end(),[=](auto op){return op.operation==operation;});};
    const auto t22=FusionAdventureLesson346Steps(22);CHECK(count(t22[0],K::Clear)==4&&count(t22[0],K::Plant)==4);
    CHECK(t22[1].operations.size()==1&&t22[1].operations[0].kind==14&&t22[1].operations[0].row==2&&t22[1].operations[0].value==1000000);
    const auto t24=FusionAdventureLesson346Steps(24);CHECK(count(t24[1],K::Zombie)==2&&count(t24[2],K::Zombie)==30);
    CHECK(t24[2].operations.front().kind==1108&&t24[2].operations.front().operation==K::Plant);
    const auto t30=FusionAdventureLesson346Steps(30);CHECK(t30[4].timerTicks==1500&&count(t30[4],K::Plant)==5&&count(t30[4],K::Zombie)==2);
    const auto t35=FusionAdventureLesson346Steps(35);CHECK(t35[0].timerTicks==500&&t35[1].timerTicks==1200&&t35[4].timerTicks==600);
    // Preserve the authored asymmetry, not a tidied diagonal arrangement.
    CHECK(t35[3].operations[0].column==3&&t35[3].operations[0].row==3);
    CHECK(t35[3].operations[2].column==3&&t35[3].operations[2].row==4);
    CHECK(t35[3].operations[3].column==4&&t35[3].operations[3].row==5);
    const auto t42=FusionAdventureLesson346Steps(42);CHECK(t42[3].timerTicks==400&&t42[3].messageTicks==500);
    CHECK(count(t42[2],K::Zombie)==5&&count(t42[2],K::Plant)==2&&t42[2].operations.back().value==1000);
    CHECK(count(t42[4],K::Plant)==10&&count(t42[4],K::Zombie)==5&&t42[4].operations.back().value==4000);
    CHECK(t42[4].operations[t42[4].operations.size()-2].operation==K::ClampDifficulty);
    CHECK(count(t42[5],K::Clear)==5&&t42[5].complete);
    for(int difficulty=0;difficulty<6;++difficulty)
    {
        FusionAdventureSession273 session;session.enabled=true;session.sourceDifficulty=uint8_t(difficulty);
        FusionAdventureProfile273 profile;profile.frontierLevel=42;CHECK(BeginFusionAdventureRun273(profile,42,false,session.run));
        FusionAdventureLessonInput274 input;input.hasGoldCabbage1135=true;FusionAdventureTutorialEffects273 effect;
        for(int stage=0;stage<5;++stage)
        {
            CHECK(FusionAdventureRuntimeDifficulty346(session)==difficulty);
            CHECK(StepFusionAdventureTutorial273(session.run,100000,input,effect));
        }
        CHECK(session.run.tutorial.stage==5&&FusionAdventureRuntimeDifficulty346(session)==(std::min)(difficulty,4));
        CHECK(session.sourceDifficulty==difficulty); // Immutable generation/wave provenance.
    }
    CHECK(FusionAdventureLesson346Steps(23).empty()&&!FusionAdventureLesson346Supported(0));
#undef CHECK
    return result;
}
