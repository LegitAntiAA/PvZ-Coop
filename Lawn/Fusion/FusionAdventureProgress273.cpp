#include "FusionAdventureProgress273.h"
#include "FusionCampaign272Rules.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr uint64_t mask=(uint64_t{1}<<kFusionAdventureLevels273)-1;
bool Level(int level){return level>=1&&level<=kFusionAdventureLevels273;}
bool Cycle(uint32_t cycle){return cycle>0&&cycle<=uint32_t((std::numeric_limits<int32_t>::max)());}
uint16_t Minus(uint16_t value,uint32_t ticks){return ticks>=value?0:uint16_t(value-ticks);}
bool TutorialValid(const FusionAdventureTutorial273& t)
{
    if(t.stage>6||t.timerTicks>400||uint8_t(t.message)>uint8_t(FusionAdventureMessage273::Ready)||
        t.messageTicks>FusionAdventureMessageDuration273(t.message))return false;
    if(t.stage==0)return t.timerTicks==0&&t.message==FusionAdventureMessage273::None&&t.messageTicks==0;
    if(t.timerTicks>FusionAdventureMessageDuration273(t.message))return false;
    if(!t.active)return t.stage==6&&t.message==FusionAdventureMessage273::Ready&&t.timerTicks==0;
    return uint8_t(t.message)==t.stage&&(t.stage<=3||t.timerTicks==0);
}
}
bool FusionAdventureProfileValid273(const FusionAdventureProfile273& p)
{return !(p.completedLevels&~mask)&&Level(p.frontierLevel)&&Cycle(p.cycle);}
bool FusionAdventureRunValid273(const FusionAdventureRun273& r)
{
    if(!TutorialValid(r.tutorial))return false;
    if(!std::isfinite(r.trophySpawnX)||!std::isfinite(r.trophySpawnY)||
        std::abs(r.trophySpawnX)>8192||std::abs(r.trophySpawnY)>8192)return false;
    if(!r.level)return !r.cycle&&!r.tutorial.active&&!r.over&&!r.rewardCollected&&!r.trophyNetId&&
        !r.trophyToken&&!r.rewardTicks&&!r.replay&&!r.transitionReady&&!r.transitionTaken&&r.tutorial.stage==0&&
        r.trophySpawnX==0&&r.trophySpawnY==0;
    if(!Level(r.level)||!Cycle(r.cycle)||((r.level!=1||r.cycle!=1)&&r.tutorial.stage))return false;
    if((r.level!=1||r.cycle!=1)&&r.tutorial.active)return false;
    if(bool(r.trophyNetId)!=bool(r.trophyToken)||r.rewardTicks>kFusionAdventureRewardTicks273)return false;
    if(!r.over)return !r.trophyNetId&&!r.rewardCollected&&!r.rewardTicks&&!r.transitionReady&&!r.transitionTaken&&
        r.trophySpawnX==0&&r.trophySpawnY==0;
    if(r.tutorial.active)return false;
    if(!r.rewardCollected)return !r.rewardTicks&&!r.transitionReady&&!r.transitionTaken;
    return r.trophyNetId&&((r.rewardTicks==0)==r.transitionReady)&&(!r.transitionTaken||r.transitionReady);
}
bool FusionAdventureLevelCompleted273(const FusionAdventureProfile273& p,int level)
{return FusionAdventureProfileValid273(p)&&Level(level)&&(p.completedLevels&(uint64_t{1}<<(level-1)));}
bool CanEnterFusionAdventureLevel273(const FusionAdventureProfile273& p,int level)
{return FusionAdventureProfileValid273(p)&&Level(level)&&(level==p.frontierLevel||FusionAdventureLevelCompleted273(p,level));}
bool FusionAdventureBaseUnlocked273(const FusionAdventureProfile273& p,uint16_t kind)
{
    int completed=0;
    return FusionAdventureProfileValid273(p)&&TryGetFusionSourceBaseUnlock272(kind,completed)&&
        (!completed||FusionAdventureLevelCompleted273(p,completed));
}
bool BeginFusionAdventureRun273(const FusionAdventureProfile273& p,int level,bool disabled,FusionAdventureRun273& out)
{
    if(!CanEnterFusionAdventureLevel273(p,level))return false;
    FusionAdventureRun273 r;r.level=uint16_t(level);r.cycle=p.cycle;r.replay=level!=p.frontierLevel;
    r.tutorial.active=level==1&&p.cycle==1&&!disabled;out=r;return true;
}
uint16_t FusionAdventureMessageDuration273(FusionAdventureMessage273 message)
{
    switch(message)
    {
    case FusionAdventureMessage273::None:return 0;
    case FusionAdventureMessage273::Welcome:case FusionAdventureMessage273::Resources:return 300;
    case FusionAdventureMessage273::NewPlants:return 400;
    case FusionAdventureMessage273::PlantSunflower:case FusionAdventureMessage273::FusePeaSunflower:return 6000;
    case FusionAdventureMessage273::BuildGatling:return 60000;
    case FusionAdventureMessage273::Ready:return 500;
    }
    return 0;
}
bool StepFusionAdventureTutorial273(FusionAdventureRun273& r,uint32_t ticks,bool sunflower,bool peaSunflower,
    bool gatling,FusionAdventureTutorialEffects273& effects)
{
    if(!FusionAdventureRunValid273(r)||!r.level)return false;
    auto next=r;FusionAdventureTutorialEffects273 e;auto& t=next.tutorial;
    t.messageTicks=Minus(t.messageTicks,ticks);
    if(t.active)
    {
        t.timerTicks=Minus(t.timerTicks,ticks);
        if(!t.timerTicks)
        {
            auto message=FusionAdventureMessage273::None;
            switch(t.stage)
            {
            case 0:message=FusionAdventureMessage273::Welcome;t.timerTicks=300;++t.stage;break;
            case 1:message=FusionAdventureMessage273::NewPlants;t.timerTicks=400;++t.stage;break;
            case 2:message=FusionAdventureMessage273::Resources;t.timerTicks=300;++t.stage;break;
            case 3:message=FusionAdventureMessage273::PlantSunflower;++t.stage;break;
            case 4:if(sunflower){message=FusionAdventureMessage273::FusePeaSunflower;++t.stage;}break;
            case 5:if(peaSunflower){message=FusionAdventureMessage273::BuildGatling;++t.stage;e.grantMissionCards=true;}break;
            case 6:if(gatling){message=FusionAdventureMessage273::Ready;t.active=false;e.completed=true;e.nextWaveTicks=500;}break;
            }
            if(message!=FusionAdventureMessage273::None)
            {t.message=message;t.messageTicks=FusionAdventureMessageDuration273(message);e.messageChanged=true;}
        }
    }
    if(!FusionAdventureRunValid273(next))return false;
    r=next;effects=e;return true;
}
bool FusionAdventureCardsIgnoreExpiry273(const FusionAdventureRun273& r)
{return FusionAdventureRunValid273(r)&&r.level&&r.tutorial.active;}
bool TryArmFusionAdventureReward273(FusionAdventureRun273& r,int wave,int maximum,int enemies,bool iz)
{
    if(!FusionAdventureRunValid273(r)||!r.level||r.over||r.tutorial.active||iz||maximum<=0||wave<maximum||enemies>0)return false;
    r.over=true;return true;
}
bool BindFusionAdventureTrophy273(FusionAdventureRun273& r,uint16_t id,uint32_t token)
{
    if(!FusionAdventureRunValid273(r)||!r.over||r.trophyNetId||!id||!token)return false;
    r.trophyNetId=id;r.trophyToken=token;return true;
}
bool IsFusionAdventureTrophy273(const FusionAdventureRun273& r,uint16_t id,uint32_t token)
{return FusionAdventureRunValid273(r)&&r.over&&id&&token&&r.trophyNetId==id&&r.trophyToken==token;}
bool TryCollectFusionAdventureReward273(FusionAdventureProfile273& p,FusionAdventureRun273& r,uint16_t id,uint32_t token)
{
    if(!FusionAdventureProfileValid273(p)||!IsFusionAdventureTrophy273(r,id,token)||r.rewardCollected||
        r.cycle!=p.cycle||!CanEnterFusionAdventureLevel273(p,r.level)||r.replay!=(r.level!=p.frontierLevel))return false;
    auto profile=p;
    profile.completedLevels|=uint64_t{1}<<(r.level-1);
    if(!r.replay)
    {
        if(profile.frontierLevel==45)
        {
            if(profile.cycle==uint32_t((std::numeric_limits<int32_t>::max)()))return false;
            profile.frontierLevel=1;++profile.cycle;
        }
        else ++profile.frontierLevel;
    }
    r.rewardCollected=true;r.rewardTicks=kFusionAdventureRewardTicks273;p=profile;return true;
}
bool StepFusionAdventureReward273(FusionAdventureRun273& r,uint32_t ticks)
{
    if(!FusionAdventureRunValid273(r)||!r.rewardCollected||r.transitionReady)return false;
    r.rewardTicks=Minus(r.rewardTicks,ticks);
    if(r.rewardTicks)return false;
    r.transitionReady=true;return true;
}
bool TakeFusionAdventureTransition273(const FusionAdventureProfile273& p,FusionAdventureRun273& r,FusionAdventureTransition273& out)
{
    if(!FusionAdventureProfileValid273(p)||!FusionAdventureRunValid273(r)||!r.transitionReady||r.transitionTaken)return false;
    FusionAdventureTransition273 transition;transition.returnToSelector=r.replay;
    transition.nextCycle=p.cycle;transition.nextLevel=r.replay?0:p.frontierLevel;
    r.transitionTaken=true;out=transition;return true;
}
}
