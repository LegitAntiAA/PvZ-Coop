#include "../../Lawn/Fusion/FusionJackbox300Rules.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace LawnFusion;
int RunFusionJackbox300Tests(int& checks)
{
    using Phase=FusionJackbox300Phase;
    int failed=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failed;std::printf("FAIL line=%d\n",line);}};
#define C(x) check(bool(x),__LINE__)
    const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
    for(uint16_t kind:std::initializer_list<uint16_t>{24,30})
    {
        FusionJackbox300View v,keep;
        C(TryInitializeFusionJackbox300(kind,4,4.4f,false,v)&&v.countdown==4.4f&&v.originalCountdown==4.4f);
        C(TryInitializeFusionJackbox300(kind,4,7.54f,true,v)&&v.countdown==.01f&&v.originalCountdown==7.54f);
        C(TryInitializeFusionJackbox300(kind,5,13.22f,false,v));
        C(TryInitializeFusionJackbox300(kind,99,22.68f,false,v));
        keep=v;C(!TryInitializeFusionJackbox300(kind,5,7.54f,false,v)&&v.originalCountdown==keep.originalCountdown);
        C(!TryInitializeFusionJackbox300(kind,4,13.22f,false,v));
        C(!TryInitializeFusionJackbox300(kind,-1,4.4f,false,v));
        C(!TryInitializeFusionJackbox300(kind,100,4.4f,false,v));
        C(!TryInitializeFusionJackbox300(kind,0,nan,false,v));
        C(TryInitializeFusionJackbox300(kind,0,4.4f,true,v));
        v.audioPlaying=true;
        FusionJackbox300Conditions conditions;
        FusionJackbox300View next;
        conditions.speedNonzero=false;C(TryStepFusionJackbox300Countdown(kind,v,conditions,5,next)&&next.countdown==.01f&&!next.popTriggered);
        conditions.speedNonzero=true;conditions.sourcePlaying=false;
        C(TryStepFusionJackbox300Countdown(kind,v,conditions,5,next)&&next.audioPlaying);
        conditions.sourcePlaying=true;conditions.beforeDying=true;
        C(TryStepFusionJackbox300Countdown(kind,v,conditions,5,next)&&next.countdown==.01f);
        conditions.beforeDying=false;C(TryStepFusionJackbox300Countdown(kind,v,conditions,.01f,next)&&next.countdown==0&&next.popTriggered&&!next.audioPlaying);
        C(FusionJackbox300SourceStatus(next)==11);
        int damage=77;C(TryFilterFusionJackbox300Damage(kind,next,1800,damage)&&damage==0);
        C(TryFilterFusionJackbox300Damage(kind,v,1800,damage)&&damage==1800);
        C(!TryFilterFusionJackbox300Damage(kind,next,-1,damage)&&damage==1800);
        Phase phase=Phase::Death;
        C(!TrySelectFusionJackbox300Phase(kind,next,false,false,phase)&&phase==Phase::Death);
        C(TrySelectFusionJackbox300Phase(kind,next,true,true,phase)&&phase==Phase::Eat); // Source priority precedes pop.
        C(TrySelectFusionJackbox300Phase(kind,next,false,true,phase)&&phase==Phase::Pop);
        next.phase=Phase::Eat;
        C(TrySelectFusionJackbox300Phase(kind,next,false,true,phase)&&phase==Phase::Walk);
        C(TrySelectFusionJackbox300Phase(kind,next,true,true,phase)&&phase==Phase::Pop);
        next.phase=Phase::Pop;next.audioPlaying=true;
        C(FusionJackbox300ViewValid(kind,next)); // Real Unfreeze can resume while popping.
        next.phase=Phase::Death;
        C(TryFilterFusionJackbox300Damage(kind,next,1800,damage)&&damage==1800);
        C(FusionJackbox300SourceStatus(next)==1);
        C(TryStepFusionJackbox300Countdown(kind,v,{},0,next)&&next.countdown==v.countdown);
        next=keep;C(!TryStepFusionJackbox300Countdown(kind,v,{},nan,next)&&next.originalCountdown==keep.originalCountdown);
        C(!TryStepFusionJackbox300Countdown(kind,v,{},-1,next));
        bool child=true;
        C(TryPlanFusionJackbox300Death(kind,v,0,0,child)&&!child);
        C(TryPlanFusionJackbox300Death(kind,v,0,4.4f,child)&&child==(kind==30));
        C(TryPlanFusionJackbox300Death(kind,v,2,4.4f,child)&&!child);
        C(TryPlanFusionJackbox300Death(kind,v,9,4.4f,child)&&!child);
        C(!TryPlanFusionJackbox300Death(kind,v,0,4.4001f,child));
        C(!TryPlanFusionJackbox300Death(kind,v,0,nan,child));
        C(FusionJackbox300RuntimeSpeedValid(kind,v,.9f)&&FusionJackbox300RuntimeSpeedValid(kind,v,1.8f));
        C(!FusionJackbox300RuntimeSpeedValid(kind,v,1.80001f));
        if(kind==30)
        {
            next=v;next.boxPresent=false;next.audioPlaying=false;next.countdown=0;
            C(FusionJackbox300ViewValid(kind,next));
            C(FusionJackbox300RuntimeSpeedValid(kind,next,1.8f)&&FusionJackbox300RuntimeSpeedValid(kind,next,3.6f));
            C(FusionJackbox300RuntimeSpeedValid(kind,next,1.79f)&&!FusionJackbox300RuntimeSpeedValid(kind,next,3.6001f));
            C(TryPlanFusionJackbox300Death(kind,next,0,next.originalCountdown,child)&&!child);
        }
        for(float bad:{nan,inf,-inf})
        {next=v;next.countdown=bad;C(!FusionJackbox300ViewValid(kind,next));next=v;next.originalCountdown=bad;C(!FusionJackbox300ViewValid(kind,next));}
        C(TryInitializeFusionJackbox300(kind,50,15,false,v));v.audioPlaying=true;
        C(TryDisarmFusionJackbox300(kind,v,next)&&!next.boxPresent&&!next.audioPlaying&&next.countdown==15&&next.originalCountdown==15);
        C(TryStepFusionJackbox300Countdown(kind,next,{},30,next)&&next.countdown==15&&!next.popTriggered);
        C(FusionJackbox300RuntimeSpeedValid(kind,next,.9f));
        C(TryPlanFusionJackbox300Death(kind,next,0,15,child)&&!child);
        C(!TryDisarmFusionJackbox300(kind,next,next));
        v.popTriggered=true;v.countdown=0;C(!TryDisarmFusionJackbox300(kind,v,next));
    }
    FusionJackbox300View v;
    C(!TryInitializeFusionJackbox300(23,0,4.4f,false,v));
    FusionZombieClipPosition p{77,1};uint8_t bits=255;
    C(TryAdvanceFusionJackbox300Clock(Phase::Walk,{0,1.5f},0,1,p)&&p.loop==0&&p.seconds==1.5f); // No invented frozen loop.
    C(TryAdvanceFusionJackbox300Clock(Phase::Walk,{0,1.5f},1,0,p)&&p.loop==0&&p.seconds==1.5f);
    C(TryAdvanceFusionJackbox300Clock(Phase::Walk,{0,1},1,1,p)&&p.loop==1&&std::abs(p.seconds-1.1f)<1e-6);
    C(TryAdvanceFusionJackbox300Clock(Phase::Pop,{0,0},1,1,p)&&p.loop==0&&p.seconds==2);
    C(TryAdvanceFusionJackbox300Clock(Phase::Pop,p,1,1,p)&&p.seconds==2.1666667f);
    C(TryGetFusionJackbox300Events(Phase::Pop,{0,2},p,bits)&&bits==Jackbox300Explode);
    C(TryGetFusionJackbox300Events(Phase::Pop,p,p,bits)&&bits==0);
    C(TryGetFusionJackbox300Events(Phase::Pop,{0,2},{1,2.1666667f},bits)&&bits==Jackbox300Explode);
    C(TryGetFusionJackbox300Events(Phase::Death,{0,0},{0,1.6666666f},bits)&&bits==Jackbox300Destroy);
    C(TryGetFusionJackbox300Events(Phase::Eat,{0,.7f},{0,.73333335f},bits)&&bits==Jackbox300Bite);
    C(TryGetFusionJackbox300Events(Phase::Eat,{0,.8f},{1,.8f},bits)&&bits==Jackbox300Bite);
    C(TryGetFusionJackbox300NextEvent(Phase::Eat,{0,.8f},p)&&p.loop==1&&p.seconds==.73333335f);
    C(!TryGetFusionJackbox300NextEvent(Phase::Pop,{0,2.1666667f},p));
    p={77,1};C(!TryAdvanceFusionJackbox300Clock(Phase::Pop,{1,0},1,1,p)&&p.loop==77);
    C(!TryAdvanceFusionJackbox300Clock(Phase::Pop,{0,0},-1,1,p));
    double delta=7;C(TryGetFusionJackbox300RootDelta(Phase::Walk,{0,0},{0,1.5f},delta)&&std::abs(delta-(-.3124999794))<1e-7);
    C(TryGetFusionJackbox300RootDelta(Phase::Walk,{0,0},{3,0},delta)&&std::abs(delta-3*(-.3124999794))<2e-7);
    C(TryGetFusionJackbox300RootDelta(Phase::Pop,{0,0},{0,2},delta)&&delta==0);
    C(TryGetFusionJackbox300RootDelta(Phase::Walk,{0,0},{0,.083333336f},delta)&&delta==0);
    C(!TryGetFusionJackbox300RootDelta(Phase::Walk,{0,1},{0,.5f},delta));
    uint8_t stage=9;int sprite=99;
    C(TryGetFusionJackbox300ArmorStage(30,247,370,0,stage,sprite)&&stage==0&&sprite==-1);
    C(TryGetFusionJackbox300ArmorStage(30,246,370,0,stage,sprite)&&stage==0&&sprite==-1);
    C(TryGetFusionJackbox300ArmorStage(30,245,370,0,stage,sprite)&&stage==1&&sprite==64);
    C(TryGetFusionJackbox300ArmorStage(30,123,370,0,stage,sprite)&&stage==1&&sprite==64);
    C(TryGetFusionJackbox300ArmorStage(30,122,370,0,stage,sprite)&&stage==2&&sprite==65);
    C(TryGetFusionJackbox300ArmorStage(30,370,370,2,stage,sprite)&&stage==2);
    C(!TryGetFusionJackbox300ArmorStage(24,100,370,0,stage,sprite));
    float speed=99;C(TryDoubleFusionJackbox300Speed(30,1.8f,speed)&&speed==3.6f);
    C(!TryDoubleFusionJackbox300Speed(30,3.6f,speed)&&speed==3.6f);
    C(!TryDoubleFusionJackbox300Speed(24,1,speed));
    return failed;
}
