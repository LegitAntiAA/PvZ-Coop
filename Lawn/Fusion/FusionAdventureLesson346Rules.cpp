#include "FusionAdventureLesson346Rules.h"
#include "FusionAdventureSession273.h"
#include <algorithm>
namespace LawnFusion
{
namespace
{
using K=FusionLesson346OpKind;using M=FusionAdventureMessage273;using C=FusionLesson346Condition;
constexpr FusionLesson346Op Clear(int c,int r){return {K::Clear,0,c,r};}
constexpr FusionLesson346Op Plant(uint16_t k,int c,int r){return {K::Plant,k,c,r};}
constexpr FusionLesson346Op Zombie(uint16_t k,int r,float x,int hp=0){return {K::Zombie,k,0,r,hp,x};}
constexpr FusionLesson346Op Money(int amount){return {K::Money,0,0,0,amount};}
constexpr FusionLesson346Op Clamp(){return {K::ClampDifficulty};}
constexpr FusionLesson346Op kelp22[]={Clear(1,2),Clear(1,3),Clear(2,2),Clear(2,3),Plant(1051,1,2),Plant(1051,1,3),Plant(1051,2,2),Plant(1051,2,3)};
constexpr FusionLesson346Op sub22[]={Zombie(14,2,9.9f,1000000)};
constexpr FusionLesson346Op warm24[]={Clear(9,1),Clear(1,0),Clear(1,1),Plant(1062,9,0),Plant(17,9,1),Plant(0,1,0),Plant(0,1,1),Zombie(0,0,9),Zombie(0,1,9)};
constexpr FusionLesson346Op chain24[]={Plant(1108,1,0),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9),Zombie(0,0,9),Zombie(0,1,9)};
constexpr FusionLesson346Op lamp30[]={Clear(2,1),Plant(20,2,1)};
constexpr FusionLesson346Op cactus30[]={Clear(1,1),Plant(1079,1,1)};
constexpr FusionLesson346Op light30[]={Clear(2,4),Clear(2,5),Clear(1,4),Clear(1,5),Clear(6,0),Plant(20,2,4),Plant(20,2,5),Plant(20,1,4),Plant(1079,1,5),Plant(22,6,0),Zombie(0,1,9.9f),Zombie(0,5,9.9f)};
constexpr FusionLesson346Op near35[]={Clear(1,0),Clear(2,1),Clear(9,0),Plant(22,9,0),Plant(25,1,0),Plant(25,2,1)};
constexpr FusionLesson346Op many35[]={Clear(1,2),Clear(2,3),Plant(1101,1,2),Plant(1101,2,3)};
constexpr FusionLesson346Op chain35[]={Clear(3,3),Clear(4,4),Plant(1093,3,4),Plant(1093,4,5)};
constexpr FusionLesson346Op separate35[]={Clear(6,3),Clear(7,4),Clear(9,0),Plant(22,9,0),Plant(1103,6,4),Plant(25,7,5)};
constexpr FusionLesson346Op connect35[]={Clear(4,3),Clear(5,3),Plant(1101,4,3),Plant(1101,5,3)};
constexpr FusionLesson346Op enable42[]={Clear(2,2),Plant(27,2,2),Plant(31,2,2)};
constexpr FusionLesson346Op silver42[]={Zombie(0,0,8),Zombie(0,1,8),Zombie(0,2,8),Zombie(0,3,8),Zombie(0,4,8),Clear(2,1),Plant(27,2,1),Plant(1134,2,1),Money(1000)};
constexpr FusionLesson346Op gold42[]={Plant(1137,3,0),Plant(1139,3,0),Plant(1137,3,1),Plant(1139,3,1),Plant(1137,3,2),Plant(1139,3,2),Plant(1137,3,3),Plant(1139,3,3),Plant(1137,3,4),Plant(1139,3,4),Zombie(36,2,9),Zombie(36,2,9),Zombie(36,2,9),Zombie(36,2,9),Zombie(36,2,9),Clamp(),Money(4000)};
constexpr FusionLesson346Op clear42[]={Clear(3,0),Clear(3,1),Clear(3,2),Clear(3,3),Clear(3,4)};
const FusionLesson346Step lesson22[]={
    {M::KelpBullets,400,400,C::Always,false, kelp22},
    {M::KelpMillion,0,400,C::Always,false, sub22},
    {M::KelpPercent,400,400,C::MissionZombieDestroyed,false, {}},
    {M::Ready,0,500,C::Always,true, {}},
};
const FusionLesson346Step lesson24[]={
    {M::WarmIntro,400,400,C::Always,false, {}},
    {M::WarmBonus,0,400,C::Always,false, warm24},
    {M::WarmChain,0,400,C::NoEnemies,false, chain24},
    {M::Ready,0,500,C::NoEnemies,true, {}},
};
const FusionLesson346Step lesson30[]={
    {M::LightIntro,400,400,C::Always,false, {}},
    {M::LightSystem,400,400,C::Always,false, {}},
    {M::LightOne,500,500,C::Always,false, lamp30},
    {M::LightFusion,400,400,C::Always,false, cactus30},
    {M::LightStack,1500,1500,C::Always,false, light30},
    {M::LightReady,0,500,C::Always,true, {}},
};
const FusionLesson346Step lesson35[]={
    {M::MagnetIntro,500,500,C::Always,false, {}},
    {M::MagnetNear,1200,1200,C::Always,false, near35},
    {M::MagnetUnlimited,1200,1200,C::Always,false, many35},
    {M::MagnetChain,1200,1200,C::Always,false, chain35},
    {M::MagnetSeparate,600,600,C::Always,false, separate35},
    {M::MagnetConnect,600,600,C::Always,false, connect35},
    {M::MagnetReady,0,500,C::Always,true, {}},
};
const FusionLesson346Step lesson42[]={
    {M::MoneyIntro,500,500,C::Always,false, {}},
    {M::MoneyEnable,0,500,C::Always,false, enable42},
    {M::MoneyUpgrade,0,6000,C::Always,false, silver42},
    {M::MoneyAbility,400,500,C::GoldCabbage,false, {}},
    {M::MoneyLink,0,6000,C::Always,false, gold42},
    {M::Ready,0,500,C::NoEnemies,true, clear42},
};
uint16_t Minus(uint16_t value,uint32_t ticks){return ticks>=value?0:uint16_t(value-ticks);}
}
std::span<const FusionLesson346Step> FusionAdventureLesson346Steps(int level)
{switch(level){case 22:return lesson22;case 24:return lesson24;case 30:return lesson30;case 35:return lesson35;case 42:return lesson42;default:return {};}}
bool FusionAdventureLesson346Supported(int level){return !FusionAdventureLesson346Steps(level).empty();}
bool FusionAdventureLesson346TutorialValid(const FusionAdventureTutorial273& t,int level)
{
    const auto steps=FusionAdventureLesson346Steps(level);if(steps.empty())return false;
    if(!t.stage)return !t.timerTicks&&!t.messageTicks&&t.message==M::None;
    if(t.stage>=steps.size())return false;
    if(!t.active)return t.stage==steps.size()-1&&t.message==steps.back().message&&!t.timerTicks&&t.messageTicks<=steps.back().messageTicks;
    const auto& previous=steps[t.stage-1];
    return !previous.complete&&t.message==previous.message&&t.timerTicks<=previous.timerTicks&&t.messageTicks<=previous.messageTicks;
}
uint16_t FusionAdventureLesson346MessageDuration(M message)
{
    for(int level:{22,24,30,35,42})for(const auto& step:FusionAdventureLesson346Steps(level))
        if(step.message==message)return step.messageTicks;
    return 0;
}
bool StepFusionAdventureLesson346(FusionAdventureRun273& r,uint32_t ticks,const FusionAdventureLessonInput274& in,FusionAdventureTutorialEffects273& out)
{
    if(!FusionAdventureLesson346Supported(r.level)||!FusionAdventureRunValid273(r))return false;
    auto next=r;auto& t=next.tutorial;FusionAdventureTutorialEffects273 effect;
    t.messageTicks=Minus(t.messageTicks,ticks);
    if(t.active)
    {
        t.timerTicks=Minus(t.timerTicks,ticks);
        if(!t.timerTicks)
        {
            const auto& step=FusionAdventureLesson346Steps(r.level)[t.stage];
            const bool ready=step.condition==C::Always||(step.condition==C::MissionZombieDestroyed&&!in.hasMissionZombie)||
                (step.condition==C::NoEnemies&&in.sourceEnemyCount==0)||(step.condition==C::GoldCabbage&&in.hasGoldCabbage1135);
            if(ready)
            {
                t.message=step.message;t.timerTicks=step.timerTicks;t.messageTicks=step.messageTicks;effect.messageChanged=true;
                if(!step.operations.empty())effect.missionAction=FusionAdventureMissionAction274::ExecuteSourceLesson346;
                if(step.complete){t.active=false;effect.completed=true;effect.nextWaveTicks=500;}
                else ++t.stage;
            }
        }
    }
    if(!FusionAdventureRunValid273(next))return false;r=next;out=effect;return true;
}
uint8_t FusionAdventureRuntimeDifficulty346(const FusionAdventureSession273& s)
{return s.enabled&&s.run.level==42&&s.run.tutorial.stage>=5?uint8_t((std::min)(unsigned(s.sourceDifficulty),4u)):s.sourceDifficulty;}
}
