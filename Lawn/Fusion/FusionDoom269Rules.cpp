#include "FusionDoom269Rules.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr double iceEvents[]={double(.6f),double(.8666667f),double(1.1333333f),double(1.4f)},doomEvents[]={double(.3f)};
const FusionShooterTimingProfile timings[]={
    {910,1.9f,iceEvents,double(1.3f),double(2.0166667f),true,65535,"attack"},
    {930,1.5f,doomEvents,1,.5,true,23,"shoot"}};
}
const FusionShooterTimingProfile* FindFusionDoom269Timing(uint16_t kind)
{for(const auto& t:timings)if(t.kind==kind)return &t;return nullptr;}
bool FusionDoom269LandTarget(int status,bool mc)
{
    if(mc||status<0)return false;
    switch(status){case 3:case 7:case 9:case 10:case 12:case 16:case 20:case 21:case 22:case 23:case 24:return false;default:return true;}
}
bool FusionSuperDoom269FearTarget(int status,bool mc)
{return !mc&&status>=0&&(status<22||status>24);}
int FusionIceDoom269PulseDelayTicks()
{
    // StartCoroutine immediately executes the first increment/yield. Each
    // resume tests BEFORE incrementing. Float accumulation needs51 increments.
    float elapsed=0;int ticks=0;while(elapsed<kFusionIceDoom269PulseDelay){elapsed+=.01f;++ticks;}return ticks;
}
bool TryMakeFusionIceDoom269Deadline(int tick,int& deadline)
{const int delay=FusionIceDoom269PulseDelayTicks();if(tick<0||tick>(std::numeric_limits<int>::max)()-delay)return false;deadline=tick+delay;return true;}
bool FusionSuperDoom269StateValid(float interval,int ordinal,int power)
{
    if(!std::isfinite(interval)||ordinal<0||ordinal>=16||power<0||power>=10000)return false;
    float actual=1.5f;for(int i=0;i<16;++i){if(interval==actual)return true;actual=actual>.2f?actual-.1f:.2f;}return false;
}
bool TryPlanFusionSelfTransition269(const FusionSelfTransition269Counters& before,bool trigger,
    bool tickAdvanced,int initial,int eventCounter,int lockTicks,FusionSelfTransition269Plan& result)
{
    if(before.incoming<0||before.lock<0||before.outgoing<0||initial<=0||initial>100000||eventCounter<0||eventCounter>=initial||lockTicks<=0||
       before.incoming>initial||before.lock>lockTicks||before.outgoing>initial+1)return false;
    FusionSelfTransition269Plan plan;plan.counters=before;
    if(!tickAdvanced){result=plan;return true;}
    if(plan.counters.outgoing==1){plan.counters.outgoing=0;plan.outgoingDue=true;}
    plan.counters.queued|=trigger;
    if(plan.counters.queued&&!plan.counters.lock)
    {
        const int remaining=before.incoming>eventCounter?before.incoming-eventCounter:0;
        if(remaining&&plan.counters.outgoing)return false; // No silent event overwrite.
        if(remaining)plan.counters.outgoing=remaining+1; // This frame's later native--.
        plan.counters.incoming=initial;plan.counters.lock=lockTicks;plan.counters.queued=false;plan.begin=true;
    }
    result=plan;return true;
}
bool TryPrepareFusionSuperDoom269Shot(float interval,int ordinal,int damage,
    bool basicTravel3,bool advancedTravel2,FusionSuperDoom269Shot& result)
{
    const int threshold=basicTravel3?4:16;
    if(!std::isfinite(interval)||interval<0||ordinal<0||ordinal>=16||damage<0)return false;
    FusionSuperDoom269Shot next;
    // Preserve the source subtraction, including its one-shot undershoot of.2.
    next.interval=interval>.2f?interval-.1f:.2f;
    next.ordinal=ordinal+1;
    next.enhanced=next.ordinal>=threshold;
    if(next.enhanced&&damage>(std::numeric_limits<int>::max)()/6)return false;
    next.damage=next.enhanced?damage*6:damage;
    if(next.enhanced)
    {
        next.ordinal=0;
        next.overrideRecovery=!advancedTravel2;
        if(next.overrideRecovery)next.recoverySeconds=next.interval*3.f;
        if(!std::isfinite(next.recoverySeconds))return false;
    }
    result=next;
    return true;
}

bool TryPrepareFusionIceDoom269Damage(int power,int damage,int damageType,
    bool expertTravel11,FusionIceDoom269Damage& result)
{
    if(power<0||power>=10000||damage<0)return false;
    FusionIceDoom269Damage next{power,false,false};
    if(expertTravel11&&(damageType==1||damageType==2))
    {
        if(damage>(std::numeric_limits<int>::max)()-power)return false;
        next.absorbed=true;
        next.power=power+damage;
        next.doom=next.power>=10000;
        if(next.doom)next.power=0; // Source discards excess instead of carrying it.
    }
    result=next;
    return true;
}
int FusionIceDoom269DeathDamage(bool expertTravel10)
{return expertTravel10?1000000:1800;}
}
