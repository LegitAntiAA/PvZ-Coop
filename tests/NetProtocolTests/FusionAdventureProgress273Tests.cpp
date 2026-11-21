#include "FusionAdventureProgress273Tests.h"
#include "../../Lawn/Fusion/FusionAdventureProgress273.h"
#include <cstdio>
#include <limits>
#include <type_traits>

namespace
{
FusionAdventureProgress273TestResult result;
void Check(bool ok,int line)
{++result.mChecks;if(!ok){++result.mFailures;std::printf("    AdventureProgress273 failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
}
FusionAdventureProgress273TestResult RunFusionAdventureProgress273Tests()
{
    using namespace LawnFusion;
    result={};std::printf("[ RUN      ] Fusion.AdventureProgress273\n");
    static_assert(std::is_trivially_copyable_v<FusionAdventureProfile273>);
    static_assert(std::is_trivially_copyable_v<FusionAdventureRun273>);
    FusionAdventureProfile273 profile;FusionAdventureRun273 run;
    CHECK(FusionAdventureProfileValid273(profile)&&FusionAdventureRunValid273(run));
    CHECK(CanEnterFusionAdventureLevel273(profile,1)&&!CanEnterFusionAdventureLevel273(profile,2));
    CHECK(!CanEnterFusionAdventureLevel273(profile,0)&&!CanEnterFusionAdventureLevel273(profile,46));
    CHECK(FusionAdventureBaseUnlocked273(profile,0)&&FusionAdventureBaseUnlocked273(profile,1));
    CHECK(!FusionAdventureBaseUnlocked273(profile,2)&&!FusionAdventureBaseUnlocked273(profile,1000));
    CHECK(!FusionAdventureBaseUnlocked273(profile,256));
    auto presentProfile=profile;presentProfile.frontierLevel=7;
    CHECK(!FusionAdventureBaseUnlocked273(presentProfile,256)); // Frontier alone never grants the card.
    for(int completed=1;completed<=45;++completed)
    {
        presentProfile.completedLevels=uint64_t{1}<<(completed-1);
        CHECK(FusionAdventureBaseUnlocked273(presentProfile,256)==(completed==6));
        CHECK(FusionAdventureBaseUnlocked273(presentProfile,250)==(completed==16));
        CHECK(FusionAdventureBaseUnlocked273(presentProfile,254)==(completed==13));
    }
    CHECK(!BeginFusionAdventureRun273(profile,2,false,run)&&run.level==0);
    CHECK(BeginFusionAdventureRun273(profile,1,false,run)&&run.tutorial.active&&!run.replay);
    CHECK(FusionAdventureCardsIgnoreExpiry273(run));
    FusionAdventureTutorialEffects273 effects;
    CHECK(StepFusionAdventureTutorial273(run,1,true,true,true,effects));
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==300&&run.tutorial.messageTicks==300&&effects.messageChanged);
    CHECK(StepFusionAdventureTutorial273(run,299,true,true,true,effects));
    CHECK(run.tutorial.stage==1&&run.tutorial.timerTicks==1&&!effects.messageChanged);
    CHECK(StepFusionAdventureTutorial273(run,1,true,true,true,effects));
    CHECK(run.tutorial.stage==2&&run.tutorial.timerTicks==400&&run.tutorial.message==FusionAdventureMessage273::NewPlants);
    CHECK(StepFusionAdventureTutorial273(run,400,true,true,true,effects));
    CHECK(run.tutorial.stage==3&&run.tutorial.timerTicks==300&&run.tutorial.message==FusionAdventureMessage273::Resources);
    CHECK(StepFusionAdventureTutorial273(run,300,true,true,true,effects));
    CHECK(run.tutorial.stage==4&&run.tutorial.message==FusionAdventureMessage273::PlantSunflower&&run.tutorial.messageTicks==6000);
    CHECK(StepFusionAdventureTutorial273(run,1,false,true,true,effects));
    CHECK(run.tutorial.stage==4&&!effects.messageChanged); // Later kinds cannot bypass source1 condition.
    CHECK(StepFusionAdventureTutorial273(run,1,true,false,false,effects));
    CHECK(run.tutorial.stage==5&&run.tutorial.message==FusionAdventureMessage273::FusePeaSunflower);
    CHECK(StepFusionAdventureTutorial273(run,1,false,false,true,effects)&&run.tutorial.stage==5);
    CHECK(StepFusionAdventureTutorial273(run,1,false,true,false,effects));
    CHECK(run.tutorial.stage==6&&effects.grantMissionCards&&run.tutorial.messageTicks==60000);
    CHECK(StepFusionAdventureTutorial273(run,1,false,true,false,effects)&&!effects.grantMissionCards);
    CHECK(!TryArmFusionAdventureReward273(run,10,10,0,false)&&!run.over);
    CHECK(StepFusionAdventureTutorial273(run,1,false,false,true,effects));
    CHECK(!run.tutorial.active&&effects.completed&&effects.nextWaveTicks==500&&run.tutorial.messageTicks==500);
    CHECK(!FusionAdventureCardsIgnoreExpiry273(run));
    CHECK(StepFusionAdventureTutorial273(run,500,false,false,true,effects));
    CHECK(!effects.completed&&!effects.grantMissionCards&&run.tutorial.messageTicks==0);
    CHECK(!TryArmFusionAdventureReward273(run,9,10,0,false));
    CHECK(!TryArmFusionAdventureReward273(run,10,10,1,false));
    CHECK(!TryArmFusionAdventureReward273(run,10,10,0,true));
    CHECK(!TryArmFusionAdventureReward273(run,10,0,0,false));
    CHECK(TryArmFusionAdventureReward273(run,10,10,0,false)&&run.over);
    CHECK(profile.frontierLevel==1&&!FusionAdventureLevelCompleted273(profile,1));
    CHECK(!TryArmFusionAdventureReward273(run,10,10,0,false));
    CHECK(!BindFusionAdventureTrophy273(run,0,99)&&!BindFusionAdventureTrophy273(run,9,0));
    CHECK(BindFusionAdventureTrophy273(run,9,99));
    CHECK(!BindFusionAdventureTrophy273(run,10,100));
    CHECK(!TryCollectFusionAdventureReward273(profile,run,9,100));
    CHECK(!TryCollectFusionAdventureReward273(profile,run,10,99));
    CHECK(profile.frontierLevel==1&&!run.rewardCollected);
    CHECK(TryCollectFusionAdventureReward273(profile,run,9,99));
    CHECK(profile.frontierLevel==2&&profile.cycle==1&&profile.completedLevels==1&&run.rewardTicks==425);
    CHECK(FusionAdventureBaseUnlocked273(profile,2)&&!FusionAdventureBaseUnlocked273(profile,3));
    CHECK(!TryCollectFusionAdventureReward273(profile,run,9,99)&&profile.frontierLevel==2);
    FusionAdventureTransition273 transition;
    CHECK(!TakeFusionAdventureTransition273(profile,run,transition));
    CHECK(!StepFusionAdventureReward273(run,124)&&run.rewardTicks==301);
    CHECK(!StepFusionAdventureReward273(run,1)&&run.rewardTicks==300);
    CHECK(!StepFusionAdventureReward273(run,299)&&run.rewardTicks==1);
    CHECK(StepFusionAdventureReward273(run,1)&&run.transitionReady);
    CHECK(!StepFusionAdventureReward273(run,1));
    CHECK(TakeFusionAdventureTransition273(profile,run,transition));
    CHECK(!transition.returnToSelector&&transition.nextLevel==2&&transition.nextCycle==1);
    CHECK(!TakeFusionAdventureTransition273(profile,run,transition));
    // Coherent first-slice1->2->3: only trophy clicks open the next level and
    // source completion flags, without writing Classic/native50 progression.
    for(int level=2;level<=3;++level)
    {
        CHECK(BeginFusionAdventureRun273(profile,level,false,run));
        CHECK(!run.tutorial.active&&!run.replay&&FusionAdventureRunValid273(run));
        CHECK(!CanEnterFusionAdventureLevel273(profile,level+1));
        CHECK(TryArmFusionAdventureReward273(run,10,10,0,false));
        CHECK(BindFusionAdventureTrophy273(run,uint16_t(level),uint32_t(level*100)));
        CHECK(profile.frontierLevel==level);
        CHECK(TryCollectFusionAdventureReward273(profile,run,uint16_t(level),uint32_t(level*100)));
        CHECK(profile.frontierLevel==level+1&&FusionAdventureLevelCompleted273(profile,level));
    }
    CHECK(profile.completedLevels==7&&FusionAdventureBaseUnlocked273(profile,3));
    const auto frontier=profile.frontierLevel;
    CHECK(BeginFusionAdventureRun273(profile,2,false,run)&&run.replay);
    CHECK(TryArmFusionAdventureReward273(run,10,10,-1,false));
    CHECK(BindFusionAdventureTrophy273(run,12,120));
    CHECK(TryCollectFusionAdventureReward273(profile,run,12,120)&&profile.frontierLevel==frontier&&profile.completedLevels==7);
    CHECK(StepFusionAdventureReward273(run,10000));
    CHECK(TakeFusionAdventureTransition273(profile,run,transition)&&transition.returnToSelector&&transition.nextLevel==0);
    // Source45 wraps to level1/cycle2, retaining completion flags.
    profile.frontierLevel=45;
    CHECK(BeginFusionAdventureRun273(profile,45,true,run));
    CHECK(TryArmFusionAdventureReward273(run,40,40,0,false));
    CHECK(BindFusionAdventureTrophy273(run,45,450));
    CHECK(TryCollectFusionAdventureReward273(profile,run,45,450));
    CHECK(profile.frontierLevel==1&&profile.cycle==2&&FusionAdventureLevelCompleted273(profile,45));
    CHECK(FusionAdventureBaseUnlocked273(profile,33)&&FusionAdventureBaseUnlocked273(profile,34));
    CHECK(BeginFusionAdventureRun273(profile,1,false,run)&&!run.tutorial.active);
    // Large delta cannot fast-forward through multiple tutorial switch cases.
    profile={};CHECK(BeginFusionAdventureRun273(profile,1,false,run));
    CHECK(StepFusionAdventureTutorial273(run,100000,true,true,true,effects)&&run.tutorial.stage==1);
    CHECK(StepFusionAdventureTutorial273(run,100000,true,true,true,effects)&&run.tutorial.stage==2);
    CHECK(BeginFusionAdventureRun273(profile,1,true,run)&&!run.tutorial.active);
    CHECK(!FusionAdventureCardsIgnoreExpiry273(run));
    auto invalid=profile;invalid.completedLevels=uint64_t{1}<<45;CHECK(!FusionAdventureProfileValid273(invalid));
    invalid=profile;invalid.frontierLevel=46;CHECK(!FusionAdventureProfileValid273(invalid));
    invalid=profile;invalid.cycle=0;CHECK(!FusionAdventureProfileValid273(invalid));
    invalid=profile;invalid.cycle=uint32_t((std::numeric_limits<int32_t>::max)())+1;CHECK(!FusionAdventureProfileValid273(invalid));
    CHECK(BeginFusionAdventureRun273(profile,1,false,run));
    run.tutorial.timerTicks=1;CHECK(!FusionAdventureRunValid273(run));
    CHECK(BeginFusionAdventureRun273(profile,1,false,run));
    CHECK(StepFusionAdventureTutorial273(run,1,false,false,false,effects));
    run.tutorial.timerTicks=301;CHECK(!FusionAdventureRunValid273(run));
    CHECK(BeginFusionAdventureRun273(profile,1,true,run));
    run.trophyToken=1;CHECK(!FusionAdventureRunValid273(run));
    CHECK(BeginFusionAdventureRun273(profile,1,true,run));
    run.trophySpawnX=(std::numeric_limits<float>::infinity)();CHECK(!FusionAdventureRunValid273(run));
    CHECK(BeginFusionAdventureRun273(profile,1,true,run));
    run.rewardCollected=true;CHECK(!FusionAdventureRunValid273(run));
    // Overflow refuses the complete transaction; no flag/collection half-write.
    profile={};profile.frontierLevel=45;profile.cycle=uint32_t((std::numeric_limits<int32_t>::max)());
    CHECK(BeginFusionAdventureRun273(profile,45,true,run));
    CHECK(TryArmFusionAdventureReward273(run,40,40,0,false)&&BindFusionAdventureTrophy273(run,45,450));
    CHECK(!TryCollectFusionAdventureReward273(profile,run,45,450));
    CHECK(profile.completedLevels==0&&profile.frontierLevel==45&&!run.rewardCollected);
    std::printf("[ %s ] Fusion.AdventureProgress273 checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
