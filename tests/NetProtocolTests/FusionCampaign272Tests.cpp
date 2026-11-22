#include "FusionCampaign272Tests.h"
#include "../../Lawn/Fusion/FusionCampaign272Rules.h"
#include "../../Lawn/Fusion/FusionSourceZombieClosure280.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>

namespace
{
FusionCampaign272TestResult result;
void Check(bool valid,int line)
{++result.mChecks;if(!valid){++result.mFailures;std::printf("    SourceCampaign272 failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
}
FusionCampaign272TestResult RunFusionCampaign272Tests()
{
    using namespace LawnFusion;
    result={};std::printf("[ RUN      ] Fusion.SourceCampaign272\n");
    std::set<uint16_t> allKinds;
    // Independent source controller/lesson dispatch, including the non-Day
    // requirements that must stay visible while their native worlds are closed.
    const std::set<int> tutorialLevels={1,4,7,8,10,13,15,22,24,30,35,42};
    const std::set<int> conveyorLevels={27,36,45};
    for(int level=1;level<=45;++level)
    {
        const auto* source=FindFusionSourceAdventure272(level);
        CHECK(source&&source->level==level&&source->columns==10);
        CHECK(source->scene==(level-1)/9);
        CHECK(source->rows==((source->scene==2||source->scene==3)?6:5));
        CHECK(source->tutorialId==(tutorialLevels.contains(level)?level:0));
        CHECK(source->conveyor==conveyorLevels.contains(level));
        CHECK(source->roofPreplantColumns==(source->scene==4?(level==37?5:3):0));
        std::string readinessError="old error";
        const bool ready=NativeSourceAdventureLevelReady280(level,1,&readinessError);
        CHECK(ready==readinessError.empty());
        const bool grassWorld=source->scene==0||source->scene==1;
        const bool lessonSupported=source->tutorialId==0||source->tutorialId==1||source->tutorialId==4||
            source->tutorialId==7||source->tutorialId==8||source->tutorialId==10||source->tutorialId==13||source->tutorialId==15;
        const bool controllerSupported=std::find(source->allowedZombieKinds.begin(),source->allowedZombieKinds.end(),uint16_t(10))==source->allowedZombieKinds.end();
        if(grassWorld&&lessonSupported&&controllerSupported)
        {
            // Configuration/closure readiness has no integer level cutoff.
            // Actual models/controllers/pools are still checked at native entry.
            CHECK(ready);
            std::vector<uint16_t> closure;
            CHECK(BuildFusionSourceZombieClosure280(source->allowedZombieKinds,closure));
            for(auto kind:source->allowedZombieKinds)
                CHECK(std::find(closure.begin(),closure.end(),kind)!=closure.end());
        }
        else
        {
            CHECK(!ready);
            const char* category=source->conveyor?"controller:":source->roofPreplantColumns?"preplants:":
                !lessonSupported?"tutorial:":!grassWorld?"world:":"npc:";
            CHECK(readinessError.starts_with(category));
        }
        CHECK(!NativeSourceAdventureLevelReady280(level,0,&readinessError)&&readinessError.starts_with("cycle:"));
        CHECK(!NativeSourceAdventureLevelReady280(level,2,&readinessError)&&readinessError.starts_with("cycle:"));
        CHECK(!source->allowedZombieKinds.empty());
        for(auto kind:source->allowedZombieKinds){allKinds.insert(kind);CHECK(FusionSourceAdventureAllowsZombie272(level,kind));}
        int waves=0,later=0;
        CHECK(TryGetFusionSourceAdventureWaves272(level,1,waves));
        CHECK(TryGetFusionSourceAdventureWaves272(level,2,later)&&later==waves+10);
        FusionSourceNativeProjection272 projection{-1,-1};
        CHECK(TryProjectFusionSourceAdventure272(level,projection));
        CHECK(projection.background==source->scene);
        std::span<const uint16_t> tail;
        CHECK(TryGetFusionSourceFinalWaveKinds272(level,1,waves,tail)&&tail.size()==source->allowedZombieKinds.size());
        CHECK(TryGetFusionSourceFinalWaveKinds272(level,1,waves-1,tail)&&tail.empty());
        int total=0;uint16_t kind=999;
        CHECK(TryGetFusionSourceZombieWeightTotal272(level,total)&&total>0);
        CHECK(TryPickFusionSourceAdventureZombie272(level,1,kind)&&FusionSourceAdventureAllowsZombie272(level,kind));
        CHECK(TryPickFusionSourceAdventureZombie272(level,total,kind)&&FusionSourceAdventureAllowsZombie272(level,kind));
    }
    CHECK(allKinds.size()==40);
    CHECK(FindFusionSourceAdventure272(0)==nullptr&&FindFusionSourceAdventure272(46)==nullptr);
    std::string readinessError;
    for(int malformed:{-1,0,46,(std::numeric_limits<int>::min)(),(std::numeric_limits<int>::max)()})
        CHECK(!NativeSourceAdventureLevelReady280(malformed,1,&readinessError)&&readinessError.starts_with("descriptor:"));
    CHECK(!NativeSourceAdventureLevelReady280(1,(std::numeric_limits<uint32_t>::max)(),&readinessError)&&readinessError.starts_with("cycle:"));
    CHECK(NativeSourceAdventureLevelReady280(9,1,&readinessError)&&readinessError.empty());
    CHECK(NativeSourceAdventureLevelReady280(9,1));
    CHECK(NativeSourceAdventureLevelReady280(10,1)&&NativeSourceAdventureLevelReady280(11,1));
    CHECK(NativeSourceAdventureLevelReady280(13,1)&&NativeSourceAdventureLevelReady280(15,1));
    CHECK(!NativeSourceAdventureLevelReady280(16,1)&&!NativeSourceAdventureLevelReady280(17,1));
    CHECK(!NativeSourceAdventureLevelReady280(19,1));
    CHECK(!NativeSourceAdventureLevelReady280(28,1)&&!NativeSourceAdventureLevelReady280(37,1));
    CHECK(FindFusionSourceAdventure272(1)->initialSun==500);
    CHECK(FindFusionSourceAdventure272(10)->initialSun==150);
    CHECK(FindFusionSourceAdventure272(19)->initialSun==300);
    CHECK(FindFusionSourceAdventure272(28)->initialSun==150);
    CHECK(FindFusionSourceAdventure272(37)->initialSun==300);
    FusionSourceNativeProjection272 projection{-5,-6};
    CHECK(!TryProjectFusionSourceAdventure272(46,projection)&&projection.layoutLevel==-5&&projection.background==-6);
    CHECK(TryProjectFusionSourceAdventure272(10,projection)&&projection.layoutLevel==11);
    CHECK(TryProjectFusionSourceAdventure272(45,projection)&&projection.layoutLevel==49);
    int waves=97;
    CHECK(!TryGetFusionSourceAdventureWaves272(1,0,waves)&&waves==97);
    int completed=97;
    CHECK(TryGetFusionSourceBaseUnlock272(0,completed)&&completed==0);
    CHECK(TryGetFusionSourceBaseUnlock272(1,completed)&&completed==0);
    CHECK(TryGetFusionSourceBaseUnlock272(2,completed)&&completed==1);
    CHECK(TryGetFusionSourceBaseUnlock272(3,completed)&&completed==2);
    CHECK(TryGetFusionSourceBaseUnlock272(6,completed)&&completed==9);
    CHECK(TryGetFusionSourceBaseUnlock272(34,completed)&&completed==45);
    CHECK(TryGetFusionSourceBaseUnlock272(250,completed)&&completed==16);
    CHECK(TryGetFusionSourceBaseUnlock272(254,completed)&&completed==13);
    CHECK(TryGetFusionSourceBaseUnlock272(256,completed)&&completed==6);
    for(uint16_t unknown:{35,249,255,257,65535})
    {completed=97;CHECK(!TryGetFusionSourceBaseUnlock272(unknown,completed)&&completed==97);}
    completed=97;CHECK(!TryGetFusionSourceBaseUnlock272(1000,completed)&&completed==97);
    int total=0;uint16_t chosen=999;
    CHECK(TryGetFusionSourceZombieWeightTotal272(2,total)&&total==7000);
    CHECK(TryPickFusionSourceAdventureZombie272(2,4000,chosen)&&chosen==0);
    CHECK(TryPickFusionSourceAdventureZombie272(2,4001,chosen)&&chosen==2);
    CHECK(TryPickFusionSourceAdventureZombie272(2,7000,chosen)&&chosen==2);
    chosen=999;CHECK(!TryPickFusionSourceAdventureZombie272(2,0,chosen)&&chosen==999);
    CHECK(!TryPickFusionSourceAdventureZombie272(2,7001,chosen)&&chosen==999);
    FusionSourceWavePick272 pick{999,999};
    CHECK(TryResolveFusionSourceAdventurePick272(1,4,pick)&&pick.emittedKind==0&&pick.remainingBudget==-3);
    CHECK(TryResolveFusionSourceAdventurePick272(4,4,pick)&&pick.emittedKind==4&&pick.remainingBudget==0);
    pick={999,999};CHECK(!TryResolveFusionSourceAdventurePick272(0,4,pick)&&pick.emittedKind==999&&pick.remainingBudget==999);
    CHECK(!TryResolveFusionSourceAdventurePick272(1,65535,pick));
    const int firstWaveCounts[]={1,3,5,6,8,10,11,13,15,33};
    int sum=0;
    for(int wave=1;wave<=10;++wave)
    {
        int budget=0;CHECK(TryGetFusionSourceWaveBudget272(1,1,wave,2,budget));
        int count=budget; // Adventure1 has onlyNormal0,cost1,weight4000.
        std::span<const uint16_t> final;
        CHECK(TryGetFusionSourceFinalWaveKinds272(1,1,wave,final));count+=static_cast<int>(final.size());
        CHECK(count==firstWaveCounts[wave-1]);sum+=count;
    }
    CHECK(sum==105);
    int budget=97;CHECK(!TryGetFusionSourceWaveBudget272(1,1,11,2,budget)&&budget==97);
    CHECK(!TryGetFusionSourceWaveBudget272(1,1,1,6,budget)&&budget==97);
    for(int id=1;id<=122;++id)
    {
        const auto* source=FindFusionSourceChallenge272(id);
        CHECK(source&&source->level==id&&source->enumName&&*source->enumName);
        CHECK(source->requiresCustomMap==(id==53));
        CHECK(id==53?(source->scene==-1&&source->maximumWaves==0):(source->scene>=0&&source->scene<=17&&source->maximumWaves>0));
    }
    CHECK(FindFusionSourceChallenge272(38)->scene==6&&FindFusionSourceChallenge272(38)->maximumWaves==40);
    CHECK(FindFusionSourceChallenge272(100)->scene==1&&FindFusionSourceChallenge272(100)->maximumWaves==40);
    CHECK(FindFusionSourceChallenge272(122)->scene==17);
    CHECK(!FindFusionSourceChallenge272(0)&&!FindFusionSourceChallenge272(123));
    std::printf("FusionCampaign272 checks=%d failures=%d\n",result.mChecks,result.mFailures);return result;
}
