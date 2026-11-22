#include "../../Lawn/Fusion/FusionSourceGraph313.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace LawnFusion;
namespace {
int count=0,fail=0;void Check(bool v,const char* why){++count;if(!v){++fail;std::cout<<"FAIL "<<why<<'\n';}}
uint8_t State(const FusionSourceGraph313& g,const char* name){for(uint8_t i=0;i<g.stateCount;++i)if(std::string(g.states[i].name)==name)return i;return 255;}
}
int RunFusionSourceGraph313Tests(int& checks)
{
    count=fail=0;
    const uint16_t kinds[]={35,36,38,39,43,37,40,212,213,25,29,26};
    for(auto k:kinds)
    {
        const auto* g=FindFusionSourceGraph313(k);Check(g&&g->kind==k,"all12 exact source graph indices");
        if(!g)continue;
        for(uint8_t l=0;l<g->layerCount;++l)
        {
            const auto& layer=g->layers[l];FusionSourceGraph313View v;v.state=v.outgoing=layer.defaultState;
            Check(FusionSourceGraph313ViewValid(*g,l,v),"actual source default layer valid");
            for(uint8_t x=0;x<layer.stateCount;++x)
            {
                const uint8_t index=layer.states[x];const auto& s=g->states[index];v.state=v.outgoing=index;
                Check(FusionSourceGraph313OwnsState(*g,l,index)&&FusionSourceGraph313ViewValid(*g,l,v),"every genuine state belongs to correct layer");
                FusionSourceGraph313Cursor p{},q;Check(AdvanceFusionSourceGraph313(s,p,1,.01f,q),"source host clip advance finite");
                Check(FusionSourceGraph313CursorValid(s,q),"actual clock valid afterstep");
                FusionSourceGraph313Cursor saved{7,.123f};q=saved;
                Check(!AdvanceFusionSourceGraph313(s,p,1,-1,q)&&q.loop==saved.loop&&q.seconds==saved.seconds,"invalid dt leaves output unchanged");
                for(uint8_t a=0;a<s.edgeCount;++a)Check(FusionSourceGraph313OwnsState(*g,l,s.edges[a].target),"all ordered source edges stay inside layer");
            }
        }
    }
    Check(FindFusionSourceGraph313(44)&&FindFusionSourceGraph313(46),"both source Boss graphs admitted");
    Check(!FindFusionSourceGraph313(1)&&!FindFusionSourceGraph313(65535),"no unrelated graph admitted");
    const auto& g=*FindFusionSourceGraph313(35);FusionSourceGraph313View v;v.state=v.outgoing=State(g,"walk");
    const FusionSourceGraph313Edge* edge=nullptr;uint32_t consumed=255;FusionSourceGraph313Parameters p;p.attacking=true;p.triggers=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Throw);
    Check(SelectFusionSourceGraph313Edge(g,0,v,{},{},p,false,false,edge,consumed)&&edge&&edge->target==State(g,"attack")&&consumed==0,"ordered attack precedes throw on source walk");
    p.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die);
    Check(SelectFusionSourceGraph313Edge(g,0,v,{},{},p,false,false,edge,consumed)&&edge&&edge->target==State(g,"die")&&consumed==FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die),"AnyState death precedes all state edges; consumeonlyusedtrigger");
    v.fadeDuration=.25f;v.fadeElapsed=.1f;v.outgoingSeconds=.2f;
    Check(SelectFusionSourceGraph313Edge(g,0,v,{},{},p,false,false,edge,consumed)&&!edge,"source InterruptionSource0 blocks transition duringfade");
    Check(SelectFusionSourceGraph313Edge(g,0,v,{},{},p,true,false,edge,consumed)&&edge&&edge->target==State(g,"die"),"explicit native B-toDeath policy keeps no invented3pose blend");
    v={};v.state=v.outgoing=State(g,"throw");p={};const auto& throwing=g.states[v.state];
    const float exit=throwing.edges[0].exitTime*throwing.duration;
    Check(SelectFusionSourceGraph313Edge(g,0,v,{0,exit-.001f},{0,exit},p,false,false,edge,consumed)&&edge&&edge->target==State(g,"walk"),"real fractional source exit threshold crossing");
    Check(SelectFusionSourceGraph313Edge(g,0,v,{0,exit},{0,exit+.001f},p,false,false,edge,consumed)&&!edge,"nonloop exit not spuriously re-fired aftercrossing");
    p.speed=0;float wait=0;Check(NextFusionSourceGraph313Boundary(g,0,v,{},p,wait)&&std::isinf(wait),"Speed0 freezes state exit andevents");
    v.fadeDuration=.25f;v.fadeElapsed=.1f;v.outgoing=State(g,"walk");Check(NextFusionSourceGraph313Boundary(g,0,v,{},p,wait)&&std::abs(wait-.15f)<1e-6f,"fixedfade continues when source Speedparameter0");
    auto bad=v;bad.fadeElapsed=.5f;Check(!FusionSourceGraph313ViewValid(g,0,bad),"invalid canonicalfade refused");
    auto state=throwing;state.loop=true;FusionSourceGraph313Cursor q;Check(AdvanceFusionSourceGraph313(state,{15,0},1,.01f,q)&&q.loop==15,"source destinationoffset15 preserved not clampedto1");
    const auto& kirov=*FindFusionSourceGraph313(26);v={};v.state=v.outgoing=kirov.layers[1].defaultState;p={};p.triggers=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Throw);
    Check(kirov.states[v.state].nullMotion,"genuine Kirov null motion is explicit");
    Check(SelectFusionSourceGraph313Edge(kirov,1,v,{},{},p,false,false,edge,consumed)&&!edge,"unknown nullmotion timedexit not invented");
    Check(SelectFusionSourceGraph313Edge(kirov,1,v,{},{},p,false,true,edge,consumed)&&!edge,"typed source null policy retains the real one-second clock");
    Check(SelectFusionSourceGraph313Edge(kirov,1,v,{},{0,1},p,false,true,edge,consumed)&&edge&&edge->target==State(kirov,"throw"),"typed source null exit occurs at its actual clock boundary");
    const auto& iron=*FindFusionSourceGraph313(38);v={};v.state=v.outgoing=State(iron,"walk");p={};p.triggers=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die);
    Check(SelectFusionSourceGraph313Edge(iron,0,v,{},{},p,false,false,edge,consumed)&&edge&&edge->offset==.030835709f&&edge->duration==0,"exact Iron death sourceoffset retained");
    for(auto kind:{uint16_t(25),uint16_t(29)})
    {
        const auto& bg=*FindFusionSourceGraph313(kind);const auto& fall=bg.states[State(bg,"fall")];Check(fall.edges[0].offset==15.000014f,"genuine Balloon fall-toWalk2 offset15 retained");
        FusionSourceGraph313Cursor marker;FusionSourceGraph313Event ev;Check(NextFusionSourceGraph313Marker(fall,{},marker,ev)&&ev==FusionSourceGraph313Event::FallComplete&&marker.seconds==1,"sourceChangeStatus eventbeforelatefall exit");
        Check(!FusionSourceGraph313OwnsState(bg,0,bg.layers[1].defaultState),"body cannot alias independentpropeller/petalsclock");
    }
    for(auto kind:{uint16_t(31),uint16_t(32),uint16_t(33),uint16_t(34),uint16_t(45),uint16_t(203),uint16_t(223)})Check(FindFusionSourceGraph313(kind)!=nullptr,"genuine shared19 sourcecohort tables generated with typedextra params");
    const auto& gold=*FindFusionSourceGraph313(223);Check(gold.layerCount==3&&gold.states[State(gold,"use")].exitEvent==int16_t(FusionSourceGraph313Event::Status6),"actualGold3layers andPaperOnStateExitStatus6 preserved");
    const auto& pogo=*FindFusionSourceGraph313(32);bool jump=false;for(uint8_t i=0;i<pogo.stateCount;++i)if(pogo.states[i].speedParameter==2)
    {p={};p.speed=0;p.jumpSpeed=2;Check(FusionSourceGraph313Rate(pogo.states[i],FusionSourceGraph313Multiplier(pogo.states[i],p))==pogo.states[i].speed*2,"genuine jumpSpeed independentfromSpeed parameter");jump=true;}Check(jump,"Pogo actualjumpSpeedstate compiled");
    Check(FindFusionSourceGraph313(28)!=nullptr,"genuine Miner graph withrise/surprise compiled");
    for(float speed:{0.f,1.1f,1.4f})
    {
        FusionSourceGraph313View fade;fade.state=State(g,"walk");fade.outgoing=State(g,"throw");fade.fadeNormalized=true;fade.fadeDuration=.19166654f*throwing.duration;fade.outgoingSeconds=2.2f;
        FusionSourceGraph313Cursor before{0,2.2f},after;Check(AdvanceFusionSourceGraph313(throwing,before,speed,.01f,after),"normalizedfade usesactualoutgoingclipadvance");
        FusionSourceGraph313View next;Check(AdvanceFusionSourceGraph313Fade(g,fade,before,after,.01f,next),"normalizedfade advancement valid");
        Check(std::abs(next.fadeElapsed-(after.seconds-before.seconds))<1e-7f,"normalizedfade elapsed exactactualsource-clock delta");
        if(speed==0)Check(next.fadeElapsed==0&&after.seconds==before.seconds,"normalizedfade sourceSpeed0 interpretation remainsfrozen");
        fade.fadeNormalized=false;Check(AdvanceFusionSourceGraph313Fade(g,fade,before,after,.01f,next)&&std::abs(next.fadeElapsed-.01f)<1e-7f,"fixedfade usesdt regardless sourceSpeedparameter");
    }
    FusionSourceGraph313View fade;fade.state=State(g,"walk");fade.outgoing=State(g,"throw");fade.fadeNormalized=true;fade.fadeDuration=.5f;fade.outgoingSeconds=2.2f;
    FusionSourceGraph313Cursor a{0,2.2f};float expected=0;int eventCount=0;
    for(int tick=0;tick<10;++tick)
    {
        const float speed=tick<5?1.1f:1.4f;FusionSourceGraph313Cursor b;Check(AdvanceFusionSourceGraph313(throwing,a,speed,.01f,b),"speedchange actualoutgoing clock step");
        FusionSourceGraph313Cursor marker;FusionSourceGraph313Event ev;if(NextFusionSourceGraph313Marker(throwing,a,marker,ev)&&marker.seconds<=b.seconds)++eventCount;
        FusionSourceGraph313View next;Check(AdvanceFusionSourceGraph313Fade(g,fade,a,b,.01f,next),"speedchange normalizedfade update");
        expected+=b.seconds-a.seconds;Check(std::abs(next.fadeElapsed-expected)<1e-6f,"midfade speedchange preserveselapsedsource clock");fade=next;a=b;
    }
    Check(eventCount==1&&fade.fadeElapsed>0&&fade.fadeElapsed<fade.fadeDuration,"outgoing sourceAnimThrow event occurs oncewhileblendactive");
    for(uint16_t k:{uint16_t(32),uint16_t(34)})
    {const auto& pg=*FindFusionSourceGraph313(k);const auto& walk=pg.states[State(pg,"walk")];Check(walk.loop&&walk.edges[0].duration>=0,"normalizedsourceedges haveactualclocked loop source");}
    checks+=count;return fail;
}
