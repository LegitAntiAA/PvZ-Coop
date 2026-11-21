#include "FusionWorldRemainder269Rules.h"
#include "FusionNativeWorldRemainder269.h"
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr double starEvent[]={double(.41666666f)},lourEvents[]={double(.33333334f),double(.6666667f)};
const FusionShooterTimingProfile timings[]={
    {909,1.5f,starEvent,1,double(.6666667f),false,49,"shoot"},
    {926,1.5f,lourEvents,1,double(.8333333f),true,91,"shoot"}};
}
const FusionShooterTimingProfile* FindFusionWorldRemainder269Timing(uint16_t kind)
{for(const auto& p:timings)if(p.kind==kind)return &p;return nullptr;}
bool FusionWorldRemainder269CattailTarget(int s,bool controlled,bool alive)
{return alive&&!controlled&&s>=0&&s!=1&&s!=7&&s!=9&&s!=12&&(s<20||s>24);}
float FusionWorldRemainder269PassiveSeconds(int count)
{return count<4?50.f:count<7?40.f:count<10?35.f:count<13?30.f:25.f;}
bool TryGetFusionWorldRemainder269StarDamage(int base,int light,int magnets,bool ultimate,int& damage)
{
    if(base<0||light<0||magnets<0)return false;
    double d=(double(base)*(ultimate?2:1)+double(light)*20)*(1+double(magnets)*.05);
    if(!ultimate&&d>300)d=300;if(!std::isfinite(d)||d>std::numeric_limits<int>::max())return false;damage=int(d);return true;
}
bool FusionWorldRemainder269PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode,uint8_t stage)
{return (kind==909||kind==926||kind==918)&&!shots&&!rage&&!bag&&!mode&&!stage;}
bool FusionWorldRemainder269ClockValid(uint16_t kind,int production,int pending,float speed,float shoot)
{
    if(production||pending<0||pending>(kind==909?253:0)||!std::isfinite(speed)||speed<.9f||speed>1.1f||!std::isfinite(shoot))return false;
    if(kind==918)return shoot== -1;
    const auto* p=FindFusionWorldRemainder269Timing(kind);return p&&(shoot== -1||(shoot>=0&&shoot<=FusionShooterDuration(*p,speed)+.011f));
}
bool NativeWorldRemainder269BoardStateValid(const NativeWorldRemainder269BoardState& s)
{
    if(s.passiveDeadline<0||s.activeDeadline<0||s.ultimateDeadline<0||s.pending.size()>1024)return false;
    if(!s.initialized)return !s.passiveDeadline&&!s.activeDeadline&&!s.ultimateDeadline&&s.pending.empty();
    for(const auto& p:s.pending)if(p.deadline<0||(p.type!=0&&p.type!=1&&p.type!=2&&p.type!=5))return false;
    return true;
}
bool NativeWorldRemainder269AuxStateValid(const NativeWorldRemainder269AuxState& s)
{
    if(!s.instanceToken||s.ageTicks<0||s.nextShotAge<0||!std::isfinite(s.targetX)||!std::isfinite(s.targetY)||
       std::abs(s.targetX)>4096||std::abs(s.targetY)>4096||((s.parent.id==0)!=(s.parent.token==0)))return false;
    if(s.family==NativeWorldRemainder269Family::LourFly)
    {
        if(s.ready&&int64_t(s.nextShotAge)>int64_t(s.ageTicks)+10)return false;
        return s.parent.id&&s.parentDamage>=0&&s.parentDamage<=(std::numeric_limits<int>::max)()/3&&s.type==0&&!s.sun&&!s.impacted&&s.shots<=60&&s.shots%3==0&&
            (s.ready||(!s.shots&&s.nextShotAge<=s.ageTicks));
    }
    if(s.family==NativeWorldRemainder269Family::Meteorite)
        return !s.parent.id&&!s.parentDamage&&!s.nextShotAge&&!s.shots&&!s.ready&&s.targetX==0&&
            (s.type==0||s.type==1||s.type==2||s.type==5);
    return false;
}
bool NativeWorldRemainder269ShotStateValid(const NativeWorldRemainder269ShotState& s)
{return s.ageTicks>=0&&((s.parent.id==0)==(s.parent.token==0));}
}
