#include "FusionCampaign272Rules.h"
#include "FusionCampaign342Rules.h"
#include <algorithm>

namespace LawnFusion
{
namespace
{
#include "generated/FusionSourceLevels272.inc"
}

const FusionSourceAdventure272* FindFusionSourceAdventure272(int level)
{return level>=1&&level<=45?&adventure272[level-1]:nullptr;}
bool NativeSourceAdventureLevelReady280(int level,uint32_t cycle,std::string* error)
{return NativeSourceAdventureLevelReady342(level,cycle,error);}

const FusionSourceChallenge272* FindFusionSourceChallenge272(int level)
{return level>=1&&level<=122?&challenges272[level-1]:nullptr;}
std::span<const FusionSourceZombieWeight272> FusionSourceZombieWeights272()
{return zombieWeights272;}

bool TryGetFusionSourceAdventureWaves272(int level,int cycle,int& waves)
{
    const auto* config=FindFusionSourceAdventure272(level);
    if(!config||cycle<1)return false;
    waves=config->firstCycleWaves+(cycle>1?10:0);return true;
}
bool TryProjectFusionSourceAdventure272(int level,FusionSourceNativeProjection272& output)
{
    const auto* config=FindFusionSourceAdventure272(level);if(!config)return false;
    output={((level-1)/9)*10+(level-1)%9+1,config->scene};return true;
}
bool TryGetFusionSourceBaseUnlock272(uint16_t kind,int& completed)
{
    if(kind<std::size(baseUnlocks272)){completed=baseUnlocks272[kind];return true;}
    const auto rule=std::find_if(std::begin(adventureSpecialUnlocks272),std::end(adventureSpecialUnlocks272),
        [=](const auto& entry){return entry.kind==kind;});
    if(rule==std::end(adventureSpecialUnlocks272))return false;
    completed=rule->completed;return true;
}
bool FusionSourceAdventureAllowsZombie272(int level,uint16_t kind)
{
    const auto* config=FindFusionSourceAdventure272(level);
    return config&&std::find(config->allowedZombieKinds.begin(),config->allowedZombieKinds.end(),kind)!=config->allowedZombieKinds.end();
}
bool TryGetFusionSourceWaveBudget272(int level,int cycle,int wave,int sourceDifficulty,int& output)
{
    int count=0;
    if(!TryGetFusionSourceAdventureWaves272(level,cycle,count)||wave<1||wave>count||sourceDifficulty<0||sourceDifficulty>5)return false;
    constexpr int multiplier[]={1,3,5,7,10,10};
    const int budget=(std::max)(1,wave*multiplier[sourceDifficulty]/3);
    output=budget*(wave%10==0?2:1);return true;
}
bool TryGetFusionSourceZombieWeightTotal272(int level,int& total)
{
    if(!FindFusionSourceAdventure272(level))return false;
    int sum=0;
    for(const auto& row:zombieWeights272)if(FusionSourceAdventureAllowsZombie272(level,row.kind))sum+=row.weight;
    if(sum<=0)return false;
    total=sum;return true;
}
bool TryPickFusionSourceAdventureZombie272(int level,int draw,uint16_t& kind)
{
    int total=0;if(!TryGetFusionSourceZombieWeightTotal272(level,total)||draw<1||draw>total)return false;
    int sum=0;
    for(const auto& row:zombieWeights272)
        if(FusionSourceAdventureAllowsZombie272(level,row.kind))
        {
            sum+=row.weight;
            if(draw<=sum){kind=row.kind;return true;}
        }
    return false;
}
bool TryResolveFusionSourceAdventurePick272(int remaining,uint16_t chosen,FusionSourceWavePick272& pick)
{
    if(remaining<=0)return false;
    for(const auto& row:zombieWeights272)
        if(row.kind==chosen)
        {pick={remaining<row.cost?uint16_t(0):chosen,remaining-row.cost};return true;}
    return false;
}
bool TryGetFusionSourceFinalWaveKinds272(int level,int cycle,int wave,std::span<const uint16_t>& kinds)
{
    int count=0;
    if(!TryGetFusionSourceAdventureWaves272(level,cycle,count)||wave<1||wave>count)return false;
    // All ordinary Adventure allow-lists are below212. Source separately skips
    //212 and218..224 during the guaranteed append; none occur in these tables.
    kinds=wave==count?FindFusionSourceAdventure272(level)->allowedZombieKinds:std::span<const uint16_t>{};
    return true;
}
}
