#include "FusionPoisonRules.h"

namespace LawnFusion
{
bool FusionPoisonStateValid(int level,int ticks,int damageTicks)
{
    return level>=0&&level<=kFusionPoisonMaximumLevel&&ticks>=0&&
        damageTicks>0&&damageTicks<=kFusionPoisonDamageIntervalTicks&&
        (ticks>0||level<=5);
}
bool FusionPoisonStateValid(const FusionPoisonState& s)
{return FusionPoisonStateValid(s.level,s.ticks,s.damageTicks);}
bool TrySetFusionPoison(const FusionPoisonState& before,int ticks,FusionPoisonState& after)
{
    if(!FusionPoisonStateValid(before)||ticks<=0||before.level==kFusionPoisonMaximumLevel||
        before.ticks>(std::numeric_limits<int>::max)()-ticks)return false;
    auto next=before;++next.level;next.ticks+=ticks;after=next;return true;
}
bool TryAddFusionPoisonLevel(const FusionPoisonState& before,FusionPoisonState& after)
{
    if(!FusionPoisonStateValid(before))return false;
    if(before.level>=6)return TrySetFusionPoison(before,kFusionPoisonDefaultDurationTicks,after);
    auto next=before;
    if(before.level<5)++next.level;
    else
    {
        if(before.ticks>(std::numeric_limits<int>::max)()-kFusionPoisonStackExtensionTicks)return false;
        next.ticks+=kFusionPoisonStackExtensionTicks;
    }
    after=next;return true;
}
bool TryStepFusionPoison(const FusionPoisonState& before,FusionPoisonState& after,int& damage,bool& pulse)
{
    if(!FusionPoisonStateValid(before))return false;
    auto next=before;int dealt=0;bool due=false;
    if(next.ticks>0)
    {
        --next.ticks;
        if(next.ticks==0)next.level=0;
        else if(--next.damageTicks==0)
        {dealt=next.level*20;due=true;next.damageTicks=kFusionPoisonDamageIntervalTicks;}
    }
    after=next;damage=dealt;pulse=due;return true;
}
bool TryWarmFusionPoison(const FusionPoisonState& before,FusionPoisonState& after)
{
    if(!FusionPoisonStateValid(before))return false;
    auto next=before;if(next.ticks>0)next.level=0;after=next;return true;
}
}
