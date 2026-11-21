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
bool TutorialLevel(int level){return level==1||level==4||level==7||level==8||level==10;}
bool TutorialValid(const FusionAdventureTutorial273& t,int level)
{
    if(uint8_t(t.message)>uint8_t(FusionAdventureMessage273::PuffReady)||
        t.messageTicks>FusionAdventureMessageDuration273(t.message))return false;
    if(t.stage==0)return t.timerTicks==0&&t.message==FusionAdventureMessage273::None&&t.messageTicks==0;
    if(level==1)
    {
        if(t.stage>6||t.timerTicks>400||t.timerTicks>FusionAdventureMessageDuration273(t.message))return false;
        if(!t.active)return t.stage==6&&t.message==FusionAdventureMessage273::Ready&&t.timerTicks==0;
        return uint8_t(t.message)==t.stage&&(t.stage<=3||t.timerTicks==0);
    }
    using M=FusionAdventureMessage273;
    if(!t.active)return t.timerTicks==0&&
        ((level==4&&t.stage==3&&t.message==M::GloveReady)||
         (level==7&&t.stage==4&&t.message==M::PresentDefenseReady)||
         (level==8&&t.stage==4&&t.message==M::EquipmentReady)||
         (level==10&&t.stage==3&&t.message==M::PuffReady));
    M expected=M::None;uint16_t maximumTimer=0;
    switch(level)
    {
    case 4:
        switch(t.stage){case 1:expected=M::GloveUnlocked;maximumTimer=400;break;
        case 2:expected=M::GloveMove;maximumTimer=500;break;case 3:expected=M::MoveTwinFlower;break;}
        break;
    case 7:
        switch(t.stage){case 1:expected=M::PresentIntro;maximumTimer=600;break;case 2:expected=M::UsePresent;break;
        case 3:expected=M::PresentReady;maximumTimer=300;break;case 4:expected=M::CherryPaperWarning;break;}
        break;
    case 8:
        switch(t.stage){case 1:expected=M::EquipmentIntro;maximumTimer=400;break;case 2:expected=M::BucketNutWarning;break;
        case 3:expected=M::UseBucket;break;case 4:expected=M::IronPlants;maximumTimer=300;break;}
        break;
    case 10:
        switch(t.stage){case 1:expected=M::PuffIntro;maximumTimer=400;break;
        case 2:expected=M::PuffStack;maximumTimer=400;break;case 3:expected=M::PuffMix;maximumTimer=400;break;}
        break;
    }
    return expected!=M::None&&t.message==expected&&t.timerTicks<=maximumTimer;
}
}
bool FusionAdventureProfileValid273(const FusionAdventureProfile273& p)
{return !(p.completedLevels&~mask)&&Level(p.frontierLevel)&&Cycle(p.cycle);}
bool FusionAdventureRunValid273(const FusionAdventureRun273& r)
{
    if(!TutorialValid(r.tutorial,r.level))return false;
    if(!std::isfinite(r.trophySpawnX)||!std::isfinite(r.trophySpawnY)||
        std::abs(r.trophySpawnX)>8192||std::abs(r.trophySpawnY)>8192)return false;
    if(!r.level)return !r.cycle&&!r.tutorial.active&&!r.over&&!r.rewardCollected&&!r.trophyNetId&&
        !r.trophyToken&&!r.rewardTicks&&!r.replay&&!r.transitionReady&&!r.transitionTaken&&r.tutorial.stage==0&&
        r.trophySpawnX==0&&r.trophySpawnY==0;
    if(!Level(r.level)||!Cycle(r.cycle)||((!TutorialLevel(r.level)||r.cycle!=1)&&r.tutorial.stage))return false;
    if((!TutorialLevel(r.level)||r.cycle!=1)&&r.tutorial.active)return false;
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
    r.tutorial.active=TutorialLevel(level)&&p.cycle==1&&!disabled;out=r;return true;
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
    case FusionAdventureMessage273::GloveUnlocked:case FusionAdventureMessage273::EquipmentIntro:return 400;
    case FusionAdventureMessage273::PuffIntro:case FusionAdventureMessage273::PuffStack:case FusionAdventureMessage273::PuffMix:return 400;
    case FusionAdventureMessage273::PresentIntro:return 600;
    case FusionAdventureMessage273::PresentReady:case FusionAdventureMessage273::IronPlants:return 300;
    case FusionAdventureMessage273::MoveTwinFlower:case FusionAdventureMessage273::UsePresent:
    case FusionAdventureMessage273::BucketNutWarning:case FusionAdventureMessage273::UseBucket:return 6000;
    case FusionAdventureMessage273::GloveMove:case FusionAdventureMessage273::GloveReady:
    case FusionAdventureMessage273::CherryPaperWarning:case FusionAdventureMessage273::PresentDefenseReady:
    case FusionAdventureMessage273::EquipmentReady:case FusionAdventureMessage273::PuffReady:return 500;
    }
    return 0;
}
bool StepFusionAdventureTutorial273(FusionAdventureRun273& r,uint32_t ticks,bool sunflower,bool peaSunflower,
    bool gatling,FusionAdventureTutorialEffects273& effects)
{
    if(!FusionAdventureRunValid273(r)||!r.level||(r.tutorial.active&&r.level!=1))return false;
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
bool StepFusionAdventureTutorial273(FusionAdventureRun273& r,uint32_t ticks,
    const FusionAdventureLessonInput274& input,FusionAdventureTutorialEffects273& effects)
{
    if(!FusionAdventureRunValid273(r)||!r.level||(r.tutorial.active&&r.level==1))return false;
    auto next=r;FusionAdventureTutorialEffects273 e;auto& t=next.tutorial;
    t.messageTicks=Minus(t.messageTicks,ticks);
    if(t.active)
    {
        t.timerTicks=Minus(t.timerTicks,ticks);
        if(!t.timerTicks)
        {
            using M=FusionAdventureMessage273;using A=FusionAdventureMissionAction274;
            M message=M::None;
            switch(r.level)
            {
            case 4:
                switch(t.stage)
                {
                case 0:message=M::GloveUnlocked;t.timerTicks=400;++t.stage;break;
                case 1:message=M::GloveMove;t.timerTicks=500;++t.stage;break;
                case 2:message=M::MoveTwinFlower;e.missionAction=A::Prepare4;++t.stage;break;
                case 3:if(input.hasMissionPlant&&input.missionPlantColumn<=4)
                    {message=M::GloveReady;t.active=false;e.completed=true;}break;
                }
                break;
            case 7:
                switch(t.stage)
                {
                case 0:message=M::PresentIntro;t.timerTicks=600;++t.stage;break;
                case 1:message=M::UsePresent;e.missionAction=A::Prepare7;++t.stage;break;
                case 2:if(!input.hasMissionPlant){message=M::PresentReady;t.timerTicks=300;++t.stage;}break;
                case 3:message=M::CherryPaperWarning;e.missionAction=A::Spawn7EnemyAndCard;++t.stage;break;
                case 4:if(!input.hasMissionZombie){message=M::PresentDefenseReady;t.active=false;e.completed=true;}break;
                }
                break;
            case 8:
                switch(t.stage)
                {
                case 0:message=M::EquipmentIntro;t.timerTicks=400;++t.stage;break;
                case 1:message=M::BucketNutWarning;e.missionAction=A::Prepare8;++t.stage;break;
                case 2:if(input.hasMissionZombie&&(input.missionZombieBeforeDying||input.missionZombieDying))
                    {message=M::UseBucket;++t.stage;}break;
                case 3:if(input.hasBucketPlant1020or1029){message=M::IronPlants;t.timerTicks=300;++t.stage;}break;
                case 4:message=M::EquipmentReady;t.active=false;e.completed=true;break;
                }
                break;
            case 10:
                switch(t.stage)
                {
                case 0:message=M::PuffIntro;t.timerTicks=400;++t.stage;break;
                case 1:message=M::PuffStack;e.missionAction=A::Prepare10;t.timerTicks=400;++t.stage;break;
                case 2:message=M::PuffMix;e.missionAction=A::Mix10;t.timerTicks=400;++t.stage;break;
                case 3:message=M::PuffReady;t.active=false;e.completed=true;break;
                }
                break;
            }
            if(message!=M::None)
            {t.message=message;t.messageTicks=FusionAdventureMessageDuration273(message);e.messageChanged=true;}
            if(e.completed)e.nextWaveTicks=500;
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
