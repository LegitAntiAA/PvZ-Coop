#include "FusionWaterZombie313Rules.h"
#include "FusionNextZombie343Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>

namespace LawnFusion
{
namespace
{
using Phase=FusionWaterZombie313Phase;
bool Range(uint16_t k,Phase p)
{
    if(k==16)return p==Phase::DriverIdle||p==Phase::DriverDamaged;
    if(k==17)return p>=Phase::SnorkelSwim&&p<=Phase::SnorkelDeath;
    if(FusionDolphin343Kind(k))return p>=Phase::DolphinRide&&p<=Phase::DolphinDeath;
    if(k==20)return p>=Phase::DrownWalk&&p<=Phase::DrownDeath;
    return false;
}
bool Position(uint16_t k,Phase p,FusionZombieClipPosition& a,FusionZombieClipPosition& b)
{
    FusionWaterZombie313Clip c;if(!TryGetFusionWaterZombie313Clip(k,p,c))return false;
    const auto valid=[&](FusionZombieClipPosition& v)
    {if(!c.loop&&v.loop==1&&v.seconds==c.seconds)v.loop=0;
     return (c.loop||!v.loop)&&std::isfinite(v.seconds)&&v.seconds>=0&&(!c.loop||v.seconds<=c.seconds);};
    return valid(a)&&valid(b)&&b.loop>=a.loop&&(b.loop>a.loop||b.seconds>=a.seconds);
}
struct Key {float time,value,inSlope,outSlope;};
struct Root {uint16_t kind;Phase phase;const Key* keys;size_t count;};
#include "FusionWaterRoot313.inc"
double Sample(const Root& r,float time)
{
    if(time<=r.keys[0].time)return r.keys[0].value;
    if(time>=r.keys[r.count-1].time)return r.keys[r.count-1].value;
    size_t i=1;while(r.keys[i].time<=time)++i;
    const auto& a=r.keys[i-1];const auto& b=r.keys[i];
    if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.value;
    const double h=b.time-a.time,u=(time-a.time)/h,u2=u*u,u3=u2*u;
    return (2*u3-3*u2+1)*a.value+(u3-2*u2+u)*h*a.outSlope+(-2*u3+3*u2)*b.value+(u3-u2)*h*b.inSlope;
}
struct Event {Phase phase;float seconds;uint16_t bits;};
constexpr Event events[]={
    {Phase::SnorkelEat,.6666667f,Water313Bite},{Phase::SnorkelEat,1.5f,Water313Bite},{Phase::SnorkelDeath,2.1666667f,Water313Destroy},
    {Phase::DolphinJump,.73333335f,Water313JumpFail},{Phase::DolphinJump,1.25f,Water313Splash},{Phase::DolphinJump,1.9166666f,Water313JumpOver},
    {Phase::DolphinEat,1.25f,Water313Bite},{Phase::DolphinEat,2.9166667f,Water313Bite},{Phase::DolphinDeath,3.f,Water313Destroy},
    {Phase::DrownThrow,.68333334f,Water313Throw},{Phase::DrownEat,.98333335f,Water313Bite},{Phase::DrownEat,2.f,Water313Bite},
    {Phase::DrownDeath,1.5f,Water313Fall},{Phase::DrownDeath,2.1666667f,Water313Destroy}};
}
bool FusionWaterZombie313Kind(uint16_t k){return k==16||k==17||k==19||k==20||k==27||FusionWaterShooter343Kind(k);}
bool FusionWaterZombie313SourceInWater(uint16_t k){return k==17||FusionDolphin343Kind(k);}
bool TryGetFusionWaterZombie313Clip(uint16_t k,Phase p,FusionWaterZombie313Clip& out)
{
    if(!Range(k,p))return false;FusionWaterZombie313Clip c;
    switch(p)
    {
    case Phase::DriverIdle:c={"idle","anim_drive",2,2,true,true};break;
    case Phase::DriverDamaged:c={"die","anim_wheelie1",1,.9f,false,true};break;
    case Phase::SnorkelSwim:c={"swim","anim_walk",1.0166667f,1,true,true};break;
    case Phase::SnorkelUp:c={"uptoeat","anim_eat",.8333333f,.75f,false,true};break;
    case Phase::SnorkelEat:c={"eat","anim_eat",2,1.5f,true,true};break;
    case Phase::SnorkelDown:c={"backtoswim","anim_eat",.84999996f,.75f,false,true};break;
    case Phase::SnorkelDeath:c={"die","anim_death",2.1666667f,1,false,true};break;
    case Phase::DolphinRide:c={"ride","anim_walk",.33333334f,.8f,true,true};break;
    case Phase::DolphinJump:c={"dolphinjump","anim_eat",1.9166666f,1,false,false};break;
    case Phase::DolphinSwim:c={"swim","anim_walk",3.4166667f,1,true,true};break;
    case Phase::DolphinEat:c={"eat","anim_eat",3.4166667f,1.5f,true,true};break;
    case Phase::DolphinDeath:c={"die","anim_death",3.0166667f,1.2f,false,true};break;
    case Phase::DrownWalk:c={"walk","anim_walk",3,2.5f,true,true};break;
    case Phase::DrownWalk2:c={"walk2","anim_walk",3.3333333f,.5f,true,true};break;
    case Phase::DrownThrow:c={"throw","anim_eat",1,1,false,true};break;
    case Phase::DrownEat:c={"attack","anim_eat",2.25f,1,true,true};break;
    case Phase::DrownDeath:c={"die","anim_death",2.1666667f,1.8f,false,true};break;
    default:return false;
    }
    out=c;return true;
}
bool FusionWaterZombie313ViewValid(uint16_t k,const FusionWaterZombie313View& v)
{
    if(k!=214&&v.gatlingReady343)return false;
    if(!Range(k,v.phase)||!Range(k,v.outgoing)||!std::isfinite(v.driverSpeed)||
        !std::isfinite(v.outgoingSourceSeconds)||!std::isfinite(v.transitionElapsed)||!std::isfinite(v.transitionTotal)||
        v.outgoingSourceSeconds<0||v.transitionElapsed<0||v.transitionTotal<0||v.transitionElapsed>v.transitionTotal||v.transitionTotal>1||
        !std::isfinite(v.auxiliaryTransitionElapsed)||!std::isfinite(v.auxiliaryTransitionTotal)||v.auxiliaryTransitionElapsed<0||
        v.auxiliaryTransitionTotal<0||v.auxiliaryTransitionElapsed>v.auxiliaryTransitionTotal||v.auxiliaryTransitionTotal>.25f)return false;
    if(k==16){if(v.driverSpeed<0||v.driverSpeed>.8f||v.driverStage>2||v.sourceStatus>1)return false;}
    else if(v.driverSpeed||v.driverStage||!v.colliderEnabled)return false;
    if(k==17&&v.sourceStatus!=0&&v.sourceStatus!=1&&v.sourceStatus!=7)return false;
    if((FusionDolphin343Kind(k))&&v.sourceStatus!=0&&v.sourceStatus!=1&&v.sourceStatus!=8&&v.sourceStatus!=9)return false;
    if(k==20&&v.sourceStatus>1)return false;
    if(!v.transitionTotal&&(v.transitionElapsed||v.outgoingSourceSeconds||v.outgoing!=v.phase))return false;

    const auto zeroAnim=[](const LawnNet::NetZombieAnimState& a){return !a.mPresent&&!a.mDead&&!a.mFrameStart&&!a.mFrameCount&&a.mAnimTime==0&&a.mAnimRate==0&&!a.mLoopType&&!a.mLoopCount;};
    if(FusionWaterShooter343Kind(k))
    {const auto* graph=FindFusionSourceGraph313(k);const auto& a=v.shootClock343;
     if(!graph||graph->layerCount!=3||!FusionSourceGraph313ViewValid(*graph,2,v.shoot343)||
        !std::isfinite(a.mAnimTime)||a.mAnimTime<0||a.mAnimTime>1||!std::isfinite(a.mAnimRate)||std::abs(a.mAnimRate)>10000||
        a.mLoopType>5||!LawnNet::NetZombieAnimStateFitsFrames(a,65535)||(!a.mPresent&&!zeroAnim(a)))return false;}
    else if(!zeroAnim(v.shootClock343)||v.shoot343.state||v.shoot343.outgoing||v.shoot343.outgoingSeconds||v.shoot343.fadeElapsed||v.shoot343.fadeDuration||v.shoot343.fadeNormalized)return false;
    const auto& h=v.auxiliary;
    if(!std::isfinite(h.mAnimTime)||h.mAnimTime<0||h.mAnimTime>1||!std::isfinite(h.mAnimRate)||std::abs(h.mAnimRate)>10000||
        h.mLoopType>5||!LawnNet::NetZombieAnimStateFitsFrames(h,65535))return false;
    if(!h.mPresent&&(h.mDead||h.mFrameStart||h.mFrameCount||h.mAnimTime||h.mAnimRate||h.mLoopType||h.mLoopCount))return false;
    if(k==20&&(h.mPresent||v.auxiliaryActive||v.auxiliaryTransitionElapsed||v.auxiliaryTransitionTotal))return false;
    if(k!=16&&(v.auxiliaryTransitionElapsed||v.auxiliaryTransitionTotal))return false;
    return true;
}
bool FusionWaterZombie313WireViewValid(uint16_t k,const FusionWaterZombie313View& v)
{return FusionWaterZombie313ViewValid(k,v)&&(k==20?!v.auxiliary.mPresent:v.auxiliary.mPresent&&!v.auxiliary.mDead)&&(!FusionWaterShooter343Kind(k)||(v.shootClock343.mPresent&&!v.shootClock343.mDead));}
bool InitializeFusionWaterZombie313View(uint16_t k,FusionWaterZombie313View& out)
{
    FusionWaterZombie313View v;
    switch(k){case 16:v.phase=Phase::DriverIdle;v.driverSpeed=.8f;break;
        case 17:v.phase=Phase::SnorkelSwim;v.sourceStatus=7;break;
        case 19:case 27:case 113:case 214:v.phase=Phase::DolphinRide;v.sourceStatus=8;break;
        case 20:v.phase=Phase::DrownWalk;break;default:return false;}
    if(FusionWaterShooter343Kind(k)){const auto* g=FindFusionSourceGraph313(k);if(!g||g->layerCount!=3)return false;v.shoot343.state=v.shoot343.outgoing=g->layers[2].defaultState;}
    v.auxiliaryActive=k==17||FusionDolphin343Kind(k);v.outgoing=v.phase;out=v;return true;
}
bool TrySelectFusionWaterZombie313Transition(uint16_t k,const FusionWaterZombie313View& v,bool attack,bool jump,bool throwing,float speed,FusionWaterZombie313Transition& out)
{
    if(!FusionWaterZombie313ViewValid(k,v)||!std::isfinite(speed)||speed<0)return false;
    using T=FusionWaterZombie313Transition;T t;
    switch(v.phase)
    {
    case Phase::SnorkelSwim:if(!attack)return false;t={Phase::SnorkelUp,0,.25f,false};break;
    case Phase::SnorkelUp:t=attack?T{Phase::SnorkelEat,.8333333f,0,true}:T{Phase::SnorkelDown,.8333333f*1.0034704f,0,true};break;
    case Phase::SnorkelEat:if(attack)return false;t={Phase::SnorkelDown,0,.48648548f,false};break;
    case Phase::SnorkelDown:t=attack?T{Phase::SnorkelUp,.84999996f*.99367386f,0,true}:T{Phase::SnorkelSwim,0,.05340302f,false};break;
    case Phase::DolphinRide:if(!jump||(k==19&&speed==0))return false;t={Phase::DolphinJump,0,.02f,false};break;
    case Phase::DolphinJump:t={Phase::DolphinSwim,1.9166666f*.86956525f,.25f,true};break;
    case Phase::DolphinSwim:if(!attack||speed==0)return false;t={Phase::DolphinEat,0,.25f,false};break;
    case Phase::DolphinEat:if(attack||speed==0)return false;t={Phase::DolphinSwim,0,.25f,false};break;
    case Phase::DrownWalk:if(!throwing||speed==0)return false;t={Phase::DrownThrow,0,.25f,false};break;
    case Phase::DrownThrow:t={Phase::DrownWalk2,1,0,true};break;
    case Phase::DrownWalk2:if(!attack||speed==0)return false;t={Phase::DrownEat,0,.25f,false};break;
    case Phase::DrownEat:if(attack||speed==0)return false;t={Phase::DrownWalk2,0,.25f,false};break;
    default:return false;
    }
    out=t;return true;
}
bool TryAdvanceFusionWaterZombie313Clock(uint16_t k,Phase p,FusionZombieClipPosition a,float speed,float dt,FusionZombieClipPosition& out)
{
    auto b=a;FusionWaterZombie313Clip c;
    if(!Position(k,p,a,b)||!TryGetFusionWaterZombie313Clip(k,p,c)||!std::isfinite(speed)||speed<0||!std::isfinite(dt)||dt<0)return false;
    double time=double(a.seconds)+dt*c.multiplier*(c.speedParameter?speed:1);
    // Unity state normalized time can continue beyond the last held source
    // key (Snorkel exit1.0034704). The native LOOP is a clock-only carrier;
    // source geometry sampling clamps its nonloop keys, not this state time.
    if(!c.loop){if(!std::isfinite(time)||time>(std::numeric_limits<float>::max)())return false;out={0,float(time)};return true;}
    const double loops=std::floor(time/c.seconds);
    if(loops>double((std::numeric_limits<uint32_t>::max)()-a.loop))return false;
    out={a.loop+uint32_t(loops),float(time-loops*c.seconds)};return true;
}
bool TryGetFusionWaterZombie313RootDelta(uint16_t k,Phase p,FusionZombieClipPosition a,FusionZombieClipPosition b,double& out)
{
    if(!Position(k,p,a,b))return false;double delta=0;
    for(const auto& r:roots)if(r.kind==(FusionWaterShooter343Kind(k)?27:k)&&r.phase==p)
    {delta=double(b.loop-a.loop)*(r.keys[r.count-1].value-r.keys[0].value)+Sample(r,b.seconds)-Sample(r,a.seconds);break;}
    if(!std::isfinite(delta))return false;out=delta;return true;
}
bool TryGetFusionWaterZombie313NextEvent(uint16_t k,Phase p,FusionZombieClipPosition a,FusionZombieClipPosition& out,uint16_t& bits)
{
    auto b=a;FusionWaterZombie313Clip c;if(!Position(k,p,a,b)||!TryGetFusionWaterZombie313Clip(k,p,c))return false;
    for(const auto& event:events)if(event.phase==p&&event.seconds>a.seconds){out={a.loop,event.seconds};bits=event.bits;return true;}
    if(c.loop&&a.loop<(std::numeric_limits<uint32_t>::max)())for(const auto& event:events)if(event.phase==p){out={a.loop+1,event.seconds};bits=event.bits;return true;}
    return false;
}
bool TryGetFusionWaterZombie313Events(uint16_t k,Phase p,FusionZombieClipPosition a,FusionZombieClipPosition b,uint16_t& out)
{
    if(!Position(k,p,a,b))return false;uint16_t bits=0;
    for(const auto& event:events)if(event.phase==p&&((b.loop==a.loop&&a.seconds<event.seconds&&b.seconds>=event.seconds)||
        (b.loop>a.loop&&(a.seconds<event.seconds||b.seconds>=event.seconds||b.loop>a.loop+1))))bits|=event.bits;
    out=bits;return true;
}
bool TryStepFusionDriver313Speed(float speed,bool beforeDying,bool right,float dt,float& next,float& velocity)
{
    if(!std::isfinite(speed)||speed<0||!std::isfinite(dt)||dt<0)return false;
    const float after=!beforeDying&&speed>.2f?speed-dt*.05f:speed;if(!std::isfinite(after)||after<0)return false;
    velocity=beforeDying?0:(right?speed:-speed);next=after;return true;
}
bool TryStepFusionDriver313Damage(float hp,int maximum,int damage,float& after,uint8_t& stage,bool& retire)
{
    if(!std::isfinite(hp)||maximum<=0||damage<0||hp>maximum)return false;
    const float next=hp-float(damage);if(!std::isfinite(next))return false;
    uint8_t value=stage;if(value>2)return false;
    if(next<float(maximum)/3)value=2;
    else if(next<float(maximum)*2/3)value=1;
    after=next;stage=value;retire=next<=0;return true;
}
bool TryStepFusionDrown313Countdown(float before,bool throwing,bool controlled,bool beforeDying,float dt,float& after,bool& trigger)
{
    if(!std::isfinite(before)||!std::isfinite(dt)||dt<0)return false;float next=before;bool due=false;
    if(before>0&&!throwing&&!controlled&&!beforeDying){next=before-dt;due=next<=0;}
    after=next;trigger=due;return std::isfinite(next);
}
bool FusionDolphin313CanProbe(uint16_t k,int status,bool controlled,bool frozen)
{return (FusionDolphin343Kind(k))&&status==8&&!controlled&&!frozen;}
bool FusionDolphin313CanJumpPlant(bool flying,bool kelp,bool sameRow){return !flying&&!kelp&&sameRow;}
bool FusionDrown313PlantEligible(bool invincible,bool pumpkin,bool flying,bool pot){return !invincible&&!pumpkin&&!flying&&!pot;}
bool TryMakeFusionDrown313Velocity(double dx,double dy,bool target,double& vx,double& vy)
{
    if(!std::isfinite(dx)||!std::isfinite(dy)||dx<0||dy<0)return false;
    if(!target){vx=-10;vy=5;return true;}
    const double duration=(std::sqrt(20*dy+16)+4)/10;const double x=-dx/duration;
    if(!std::isfinite(x))return false;vx=x;vy=4;return true;
}
bool TryGetFusionDrown313Damage(int hp,uint16_t kind,bool eve,bool travel,bool stronger,int& out)
{
    if(hp<0)return false;if(eve){out=20;return true;}
    int64_t damage=(std::max)(int64_t(60),int64_t(double(hp)*.2));
    if(travel)damage*=5;if(stronger)damage*=5;if(kind==916||kind==923)damage/=4;
    if(damage>(std::numeric_limits<int>::max)())return false;out=int(damage);return true;
}
bool FusionWaterZombie313WorldValid(uint16_t k,int rows,bool pool,std::span<const uint8_t> water,int row,bool controlled,bool ordinaryDirectNonIZ)
{
    if(!FusionWaterZombie313Kind(k)||row<0||row>=rows||water.size()!=size_t(rows))return false;
    if(rows==5){if(pool||(!ordinaryDirectNonIZ&&!controlled&&FusionWaterZombie313SourceInWater(k)))return false;for(auto v:water)if(v)return false;return true;}
    if(rows!=6||!pool)return false;
    for(int r=0;r<6;++r)if(water[r]!=(r==2||r==3?1:0))return false;
    // The genuine controlled factory450B00 has only a row bounds check;
    // hostile451130 filters WATER rows; its dry restriction is IZ-only.
    // Wave NewZombieUpdate row repair remains a separate caller policy.
    return controlled||(ordinaryDirectNonIZ&&!water[row])||((k==16||k==20)?!water[row]:water[row]!=0);
}
}

namespace LawnFusion
{
// InitZombieData 1803BA5C0: preset2 HP/first/second/attack, not carrier HP.
// Water-only admission is conditional on allegiance, so it is not a profile flag.
const FusionZombieProfile* FindFusionWaterZombie313Profile(uint16_t kind)
{
    static constexpr FusionZombieProfile rows[]={
        {16,1350,0,0,500,false,false,true},{17,270,0,0,50},
        {19,720,0,0,50},{20,720,0,0,50},{27,720,0,0,50}};
    if(FusionWaterShooter343Kind(kind))return FindFusionNextZombie343Profile(kind);
    for(const auto& row:rows)if(row.kind==kind)return &row;return nullptr;
}
}
