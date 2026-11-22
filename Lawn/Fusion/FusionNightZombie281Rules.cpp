#include "FusionNightZombie281Rules.h"
#include <cmath>
#include <algorithm>
#include <limits>
namespace LawnFusion
{
namespace
{
using Phase=FusionNightZombie281Phase;
struct RootKey {float time,value,inSlope,outSlope;};
constexpr RootKey walk[]={
    {0,3.7106f,0,0},
    {0.33333334f,3.445f,1.4247892e-07f,(std::numeric_limits<float>::infinity()*-1)},
    {0.8333333f,3.445f,0,0},
    {1.1666666f,3.068f,2.3130579e-07f,(std::numeric_limits<float>::infinity()*-1)},
    {1.3333334f,3.068f,0,0},
    {1.6666666f,3.074f,-4.0539367e-09f,0},
    {2.0f,2.808f,2.528821e-07f,(std::numeric_limits<float>::infinity()*-1)},
    {2.5f,2.808f,0,0},
    {2.8333333f,2.432f,2.9372958e-07f,0}};
constexpr RootKey moonwalk[]={
    {0,4.1067133f,0,0},
    {0.33333334f,4.2f,0.39323997f,0.39323997f},
    {0.5f,4.3033333f,0.38333344f,0.38333324f},
    {0.8333333f,4.3916664f,0.6700001f,0.6700001f},
    {1.0f,4.6383333f,-2.6305605e-07f,0},
    {1.1666666f,4.633333f,-3.179929e-09f,0},
    {1.3333334f,4.6825f,0.39749902f,0.39749902f},
    {1.5f,4.765833f,0.38249877f,0.38249883f},
    {1.6666666f,4.8099995f,0.5299988f,0.5299987f},
    {1.8333334f,4.9624996f,0.9000005f,0.90000045f},
    {2.0f,5.1099997f,8.185782e-08f,0}};

bool Span(FusionZombieClipPosition& a,FusionZombieClipPosition& b,const FusionNightZombie281Clip& clip)
{
 // Native PLAY_ONCE_AND_HOLD reports loop1/time1 at its held final frame.
 // That is the same source instant as loop0/duration, not another source loop.
 const auto normalize=[&](FusionZombieClipPosition& p)
 {
  if(!clip.loop&&p.loop==1&&p.seconds==clip.seconds)p.loop=0;
  return clip.loop||p.loop==0;
 };
 return normalize(a)&&normalize(b)&&std::isfinite(a.seconds)&&std::isfinite(b.seconds)&&
  a.seconds>=0&&b.seconds>=0&&a.seconds<=clip.seconds&&b.seconds<=clip.seconds&&
  b.loop>=a.loop&&(b.loop!=a.loop||b.seconds>=a.seconds);
}
bool Cross(FusionZombieClipPosition a,FusionZombieClipPosition b,float t)
{return b.loop>a.loop?(a.seconds<t||b.seconds>=t||b.loop>a.loop+1):a.seconds<t&&b.seconds>=t;}
float Sample(std::span<const RootKey> keys,float t)
{
 if(t<=keys.front().time)return keys.front().value;if(t>=keys.back().time)return keys.back().value;
 size_t i=1;while(keys[i].time<=t)++i;const auto& a=keys[i-1];const auto& b=keys[i];
 if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.value;
 const float h=b.time-a.time,u=(t-a.time)/h,u2=u*u,u3=u2*u;
 return (2*u3-3*u2+1)*a.value+(u3-2*u2+u)*h*a.outSlope+(-2*u3+3*u2)*b.value+(u3-u2)*h*b.inSlope;
}
struct Callback {float seconds;uint16_t bit;};
constexpr Callback pointEvents[]={{0,Night281Forward},{.6666667f,Night281Summon},{2,Night281PointOver}};
constexpr Callback walkEvents[]={{0,Night281Forward}};
constexpr Callback raiseEvents[]={{0,Night281Forward},{.8333333f,Night281Back},{1.6666666f,Night281Forward},{2.5f,Night281Back}};
constexpr Callback eatEvents[]={{0,Night281Forward},{1.0833334f,Night281Bite1},{2.4f,Night281Bite2}};
constexpr Callback deathEvents[]={{0,Night281Forward},{2.5f,Night281Fall},{2.8333333f,Night281Destroy}};
std::span<const Callback> Callbacks(Phase phase)
{
 switch(phase){case Phase::Point:return pointEvents;case Phase::Walk:return walkEvents;
 case Phase::ArmRaise:return raiseEvents;case Phase::EatFromWalk:case Phase::EatFromRaise:return eatEvents;
 case Phase::Death:return deathEvents;default:return {};}
}
bool TransitionSpec(Phase from,Phase to,FusionNightZombie281Transition& output)
{
 FusionNightZombie281Transition t{to,.25f,true};bool allowed=false;
 if(to==Phase::Death&&from!=Phase::Death)allowed=FusionJackson281ControllerState(from)!=nullptr;
 else switch(from)
 {
 case Phase::Moonwalk:allowed=to==Phase::Point;break;
 case Phase::Point:allowed=to==Phase::ArmRaise;break;
 case Phase::Walk:
  allowed=to==Phase::ArmRaise||to==Phase::EatFromWalk||to==Phase::Point;
  if(to==Phase::Point){t.duration=.1f;t.fixedSeconds=false;}break;
 case Phase::ArmRaise:allowed=to==Phase::Walk||to==Phase::EatFromRaise;if(to==Phase::Walk)t.duration=.02f;break;
 case Phase::EatFromWalk:allowed=to==Phase::Walk;break;
 case Phase::EatFromRaise:allowed=to==Phase::ArmRaise;break;
 default:break;
 }
 if(!allowed)return false;output=t;return true;
}
bool Inputs(int row,std::span<const uint8_t> rows,float x)
{if(rows.empty()||rows.size()>6||row<0||size_t(row)>=rows.size()||!std::isfinite(x))return false;
 for(auto type:rows)if(type>2)return false;return true;}
}
bool TryGetFusionNightZombie281Clip(Phase phase,FusionNightZombie281Clip& output)
{
 FusionNightZombie281Clip c;
 switch(phase)
 {
 case Phase::Moonwalk:c={"moonwalk","anim_walk",2,1.5f,true};break;
 case Phase::Point:c={"point","anim_walk",2.3333333f,.9f,false};break;
 case Phase::Walk:c={"walk","anim_walk2",3.3333333f,.8f,false};break;
 case Phase::ArmRaise:c={"armraise","anim_walk",3.3333333f,.9f,false};break;
 case Phase::EatFromWalk:case Phase::EatFromRaise:c={"eat","anim_eat",2.7333333f,2,true};break;
 case Phase::Death:c={"death","anim_death",2.8333333f,1.6f,false};break;
 default:return false;
 }output=c;return true;
}
bool TryPlanFusionDancePole281(int row,std::span<const uint8_t> rows,float x,bool balanced,
 int status,bool pending,FusionNightZombie281Children& output)
{
 if(!Inputs(row,rows,x))return false;FusionNightZombie281Children p;
 if(status==0&&!pending)
 {
  p.actors[p.count++]={7,0,row,x+1.5f,false};p.actors[p.count++]={7,1,row,x-1.5f,false};
  if(!balanced)
  {if(row>0&&rows[row-1]!=1)p.actors[p.count++]={7,2,row-1,x,false};
   if(size_t(row+1)<rows.size()&&rows[row+1]!=1)p.actors[p.count++]={7,3,row+1,x,false};}
 }
 output=p;return true;
}
bool TryPlanFusionJackson281(int row,std::span<const uint8_t> rows,float x,bool balanced,
 int status,bool pending,bool controlled,const std::array<bool,4>& present,FusionNightZombie281Children& output)
{
 if(!Inputs(row,rows,x))return false;FusionNightZombie281Children p;
 if(!(balanced&&x<1.5f)&&status==0&&!pending)
 {
  if(!present[0]&&row>0&&rows[row-1]!=1)p.actors[p.count++]={6,0,row-1,x,controlled};
  if(!present[1])p.actors[p.count++]={6,1,row,x+1,controlled};
  if(!present[2])p.actors[p.count++]={6,2,row,x-1.5f,controlled};
  if(!present[3]&&size_t(row+1)<rows.size()&&rows[row+1]!=1)p.actors[p.count++]={6,3,row+1,x,controlled};
 }
 output=p;return true;
}
bool FusionJackson281MissingDancer(int row,int rows,const std::array<bool,4>& present)
{
 if(rows<1||row<0||row>=rows)return false;
 for(int i=0;i<4;++i)if((i!=0||row!=0)&&(i!=3||row!=rows-1)&&!present[i])return true;
 return false;
}
bool TryGetFusionJackson281Events(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b,
 bool entering,uint16_t& output)
{
 FusionNightZombie281Clip c;if(!TryGetFusionNightZombie281Clip(phase,c)||!Span(a,b,c))return false;
 uint16_t bits=0;
 for(const auto& event:Callbacks(phase))
  if((event.seconds==0&&entering)||Cross(a,b,event.seconds))bits|=event.bit;
 output=bits;return true;
}
bool TryGetFusionJackson281NextEvent(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition& output)
{
 FusionNightZombie281Clip clip;auto b=a;
 if(!TryGetFusionNightZombie281Clip(phase,clip)||!Span(a,b,clip))return false;
 const auto events=Callbacks(phase);
 for(const auto& event:events)if(event.seconds>a.seconds){output={a.loop,event.seconds};return true;}
 if(clip.loop&&!events.empty()&&a.loop<(std::numeric_limits<uint32_t>::max)())
 {output={a.loop+1,events.front().seconds};return true;}
 return false;
}
bool TryAdvanceFusionJackson281Clock(Phase phase,FusionZombieClipPosition a,float speed,float dt,FusionZombieClipPosition& output)
{
 FusionNightZombie281Clip clip;auto b=a;
 if(!TryGetFusionNightZombie281Clip(phase,clip)||!Span(a,b,clip)||!std::isfinite(speed)||speed<0||!std::isfinite(dt)||dt<0)return false;
 if(speed==0||dt==0){output=a;return true;}
 const double seconds=double(a.seconds)+double(dt)*speed*clip.multiplier;
 if(!std::isfinite(seconds))return false;
 if(clip.loop)
 {
  const double loops=std::floor(seconds/clip.seconds);
  if(loops>double((std::numeric_limits<uint32_t>::max)()-a.loop))return false;
  b={a.loop+uint32_t(loops),float(seconds-loops*clip.seconds)};
 }
 else b={0,float((std::min)(double(clip.seconds),seconds))};
 output=b;return true;
}
bool TryGetFusionJackson281Transition(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b,
 bool positive,bool attack,bool missing,bool summon,FusionNightZombie281Transition& output)
{
 FusionNightZombie281Clip c;if(!TryGetFusionNightZombie281Clip(phase,c)||!Span(a,b,c))return false;
 FusionNightZombie281Transition t;bool found=false;
 switch(phase)
 {
 case Phase::Moonwalk:if(summon&&positive){t={Phase::Point,.25f,true};found=true;}break;
 case Phase::Point:if(Cross(a,b,c.seconds*.8235294f)){t={Phase::ArmRaise,.25f,true};found=true;}break;
 case Phase::Walk:
  if(!missing&&Cross(a,b,c.seconds*.925f)){t={Phase::ArmRaise,.25f,true};found=true;}
  else if(attack&&positive){t={Phase::EatFromWalk,.25f,true};found=true;}
  else if(missing&&Cross(a,b,c.seconds*.9f)){t={Phase::Point,.1f,false};found=true;}break;
 case Phase::ArmRaise:
  if(Cross(a,b,c.seconds)){t={Phase::Walk,.02f,true};found=true;}
  else if(attack&&positive){t={Phase::EatFromRaise,.25f,true};found=true;}break;
 case Phase::EatFromWalk:if(!attack&&positive){t={Phase::Walk,.25f,true};found=true;}break;
 case Phase::EatFromRaise:if(!attack&&positive){t={Phase::ArmRaise,.25f,true};found=true;}break;
 case Phase::Death:break;
 default:return false;
 }
 if(!found)return false;output=t;return true;
}
bool TryGetFusionNightZombie281RootDelta(Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b,
 bool flipped,double pixels,double& output)
{
 FusionNightZombie281Clip clip;if(!TryGetFusionNightZombie281Clip(phase,clip)||!Span(a,b,clip)||
 !std::isfinite(pixels)||pixels<0)return false;
 std::span<const RootKey> keys;if(phase==Phase::Walk)keys=walk;else if(phase==Phase::Moonwalk)keys=moonwalk;
 else{output=0;return true;}
 const double dx=double(b.loop-a.loop)*(double(keys.back().value)-keys.front().value)+Sample(keys,b.seconds)-Sample(keys,a.seconds);
 const double value=dx*pixels*(flipped?-1:1);if(!std::isfinite(value))return false;output=value;return true;
}
const char* FusionJackson281ControllerState(Phase phase)
{
 switch(phase){case Phase::Moonwalk:return "walk";case Phase::Point:return "point";
 case Phase::Walk:return "walk1";case Phase::ArmRaise:return "armraise";
 case Phase::EatFromWalk:return "eatfromwalk";case Phase::EatFromRaise:return "eatfromraise";
 case Phase::Death:return "death";default:return nullptr;}
}
bool FusionNightZombie281ViewValid(const FusionNightZombie281View& view)
{
 FusionNightZombie281Clip incoming,outgoing;
 if(!TryGetFusionNightZombie281Clip(view.phase,incoming)||!TryGetFusionNightZombie281Clip(view.outgoing,outgoing)||
    (view.canAttack&&!view.moonWalkFinished)||!std::isfinite(view.outgoingSourceSeconds)||
    !std::isfinite(view.transitionElapsed)||!std::isfinite(view.transitionTotal))return false;
 if(view.transitionTotal==0)return view.outgoing==Phase::Moonwalk&&view.outgoingSourceSeconds==0&&view.transitionElapsed==0;
 FusionNightZombie281Transition spec;
 return TransitionSpec(view.outgoing,view.phase,spec)&&(!spec.fixedSeconds||view.transitionTotal==spec.duration)&&
    view.transitionTotal>0&&view.transitionElapsed>=0&&view.transitionElapsed<=view.transitionTotal&&
    view.outgoingSourceSeconds>=0&&view.outgoingSourceSeconds<=outgoing.seconds;
}
bool TryGetFusionJackson281TransitionSeconds(Phase from,const FusionNightZombie281Transition& t,float speed,float& output)
{
 FusionNightZombie281Clip oldClip,newClip;FusionNightZombie281Transition spec;
 if(!TransitionSpec(from,t.target,spec)||spec.duration!=t.duration||spec.fixedSeconds!=t.fixedSeconds||
    !TryGetFusionNightZombie281Clip(from,oldClip)||!TryGetFusionNightZombie281Clip(t.target,newClip)||
    !std::isfinite(t.duration)||t.duration<=0||!std::isfinite(speed)||speed<0||(!t.fixedSeconds&&speed<=0))return false;
 const double total=t.fixedSeconds?double(t.duration):double(t.duration)*oldClip.seconds/(oldClip.multiplier*double(speed));
 if(!std::isfinite(total)||total<=0||total>(std::numeric_limits<float>::max)())return false;
 output=float(total);return true;
}
bool TryGetFusionJackson281BlendRootDelta(Phase incoming,FusionZombieClipPosition ia,FusionZombieClipPosition ib,
 Phase outgoing,FusionZombieClipPosition oa,FusionZombieClipPosition ob,float wa,float wb,bool flipped,double pixels,double& output)
{
 if(!std::isfinite(wa)||!std::isfinite(wb)||wa<0||wa>1||wb<wa||wb>1)return false;
 double next=0,old=0;
 if(!TryGetFusionNightZombie281RootDelta(incoming,ia,ib,flipped,pixels,next)||
    !TryGetFusionNightZombie281RootDelta(outgoing,oa,ob,flipped,pixels,old))return false;
 const double w=(double(wa)+wb)/2,delta=(1-w)*old+w*next;
 if(!std::isfinite(delta))return false;output=delta;return true;
}
bool TryGetFusionNightZombie281ArmorStage(uint16_t kind,int hp,int maximum,uint8_t previous,uint8_t& output,int& sprite)
{
 if((kind!=5&&kind!=9)||maximum<=0||hp<0||hp>maximum||previous>2)return false;
 uint8_t stage=previous;const uint32_t twice=uint32_t(maximum)*2U;
 const int64_t wrapped=twice&0x80000000U?int64_t(twice)-0x100000000LL:twice;
 if(hp<wrapped/3&&stage<1)stage=1;if(hp<maximum/3&&stage<2)stage=2;
 output=stage;sprite=stage?(kind==5?6:20)+stage-1:-1;return true;
}
}
