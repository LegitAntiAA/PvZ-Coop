#include "../../Lawn/Fusion/FusionNightZombie281Rules.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>

// Source data: original seven-state controller/clips (contract289), callback
// primary281. These pure tests do not validate native hooks, sampler or wire.
int RunFusionJackson291Tests(int& checks)
{
 using namespace LawnFusion;using Phase=FusionNightZombie281Phase;
 int failures=0;const auto check=[&](bool value,int line)
 {++checks;if(!value){++failures;std::printf("FusionJackson291Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
 const auto closeEnough291=[](double x,double y){return std::abs(x-y)<3e-6;};
 const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
 const char* states[]={"walk","point","walk1","armraise","eatfromwalk","eatfromraise","death"};
 for(int i=0;i<7;++i)CHECK(FusionJackson281ControllerState(Phase(i))&&std::strcmp(FusionJackson281ControllerState(Phase(i)),states[i])==0);
 CHECK(!FusionJackson281ControllerState(Phase(255)));
 FusionNightZombie281View view;
 CHECK(FusionNightZombie281ViewValid(view));
 view.canAttack=true;CHECK(!FusionNightZombie281ViewValid(view));view.moonWalkFinished=true;CHECK(FusionNightZombie281ViewValid(view));
 view.phase=Phase::Point;view.outgoing=Phase::Moonwalk;view.outgoingSourceSeconds=1.5f;
 CHECK(!FusionNightZombie281ViewValid(view)); // Duration0 requires a cleared outgoing time.
 view.transitionTotal=.25f;CHECK(FusionNightZombie281ViewValid(view));
 view.transitionElapsed=.125f;CHECK(FusionNightZombie281ViewValid(view));
 view.transitionElapsed=.25f;CHECK(FusionNightZombie281ViewValid(view)); // Exact incoming endpoint is valid.
 view.transitionElapsed=.250001f;CHECK(!FusionNightZombie281ViewValid(view));
 view.transitionElapsed=-.001f;CHECK(!FusionNightZombie281ViewValid(view));view.transitionElapsed=0;
 for(float bad:{nan,inf,-inf})
 {
  auto v=view;v.outgoingSourceSeconds=bad;CHECK(!FusionNightZombie281ViewValid(v));
  v=view;v.transitionElapsed=bad;CHECK(!FusionNightZombie281ViewValid(v));
  v=view;v.transitionTotal=bad;CHECK(!FusionNightZombie281ViewValid(v));
 }
 view.outgoingSourceSeconds=2.00001f;CHECK(!FusionNightZombie281ViewValid(view));
 view.outgoingSourceSeconds=2;CHECK(FusionNightZombie281ViewValid(view));
 view.transitionTotal=.02f;CHECK(!FusionNightZombie281ViewValid(view));view.transitionTotal=.25f;
 view.phase=Phase::Moonwalk;view.outgoing=Phase::Walk;CHECK(!FusionNightZombie281ViewValid(view));
 view.phase=Phase::Point;view.outgoing=Phase::Point;CHECK(!FusionNightZombie281ViewValid(view));
 view.outgoing=Phase(255);CHECK(!FusionNightZombie281ViewValid(view));
 view=FusionNightZombie281View{};view.outgoing=Phase::Walk;CHECK(!FusionNightZombie281ViewValid(view));
 view=FusionNightZombie281View{};view.phase=Phase(255);CHECK(!FusionNightZombie281ViewValid(view));

 // Fixed crossfades use game seconds, independent of source Speed, including0.
 float total=99;FusionNightZombie281Transition fixed{Phase::Death,.25f,true};
 for(float speed:{0.f,.5f,1.f,1.8f})CHECK(TryGetFusionJackson281TransitionSeconds(Phase::Walk,fixed,speed,total)&&total==.25f);
 FusionNightZombie281Transition normalized{Phase::Point,.1f,false};
 CHECK(TryGetFusionJackson281TransitionSeconds(Phase::Walk,normalized,1,total)&&closeEnough291(total,.1*double(3.3333333f)/.8f));
 CHECK(TryGetFusionJackson281TransitionSeconds(Phase::Walk,normalized,.5f,total)&&closeEnough291(total,.1*double(3.3333333f)/(.8f*.5f)));
 total=99;CHECK(!TryGetFusionJackson281TransitionSeconds(Phase::Walk,normalized,0,total)&&total==99);
 CHECK(!TryGetFusionJackson281TransitionSeconds(Phase::Walk,normalized,nan,total)&&total==99);
 CHECK(!TryGetFusionJackson281TransitionSeconds(Phase::Death,fixed,1,total)&&total==99);

 // Outgoing point crosses PointOver2 during its .25s crossfade after the
 // source exit1.9215686; it must keep its speed and dispatch exactly once.
 FusionZombieClipPosition a{0,2.3333333f*.8235294f},b{};uint16_t events=0;
 CHECK(TryAdvanceFusionJackson281Clock(Phase::Point,a,1,.1f,b)&&b.seconds>2);
 CHECK(TryGetFusionJackson281Events(Phase::Point,a,b,false,events)&&events==Night281PointOver);
 CHECK(TryGetFusionJackson281Events(Phase::Point,b,b,false,events)&&events==0);
 // ALL states apply their multiplier once, including source Death1.6.
 CHECK(TryAdvanceFusionJackson281Clock(Phase::Death,{0,0},.5f,1,b)&&closeEnough291(b.seconds,.8f));
 CHECK(TryAdvanceFusionJackson281Clock(Phase::Death,b,0,1,a)&&a.seconds==b.seconds);
 CHECK(TryAdvanceFusionJackson281Clock(Phase::Walk,{0,3},1,1,b)&&b.loop==0&&b.seconds==3.3333333f);
 CHECK(TryAdvanceFusionJackson281Clock(Phase::Walk,{1,3.3333333f},1,1,b)&&b.loop==0&&b.seconds==3.3333333f);
 CHECK(TryAdvanceFusionJackson281Clock(Phase::Moonwalk,{0,1.5f},1,1,b)&&b.loop==1&&closeEnough291(b.seconds,1));
 b={17,1};CHECK(!TryAdvanceFusionJackson281Clock(Phase::Point,{0,0},-1,1,b)&&b.loop==17&&b.seconds==1);
 CHECK(!TryAdvanceFusionJackson281Clock(Phase::Point,{0,0},1,nan,b)&&b.loop==17&&b.seconds==1);
 CHECK(!TryAdvanceFusionJackson281Clock(Phase::Walk,{1,0},1,1,b)&&b.loop==17&&b.seconds==1);

 // Genuine Eat loop-time0 callback is Forward even when entering=false.
 FusionNightZombie281Clip eat;CHECK(TryGetFusionNightZombie281Clip(Phase::EatFromWalk,eat));
 CHECK(TryGetFusionJackson281NextEvent(Phase::EatFromWalk,{0,2.5f},b)&&b.loop==1&&b.seconds==0);
 CHECK(TryAdvanceFusionJackson281Clock(Phase::EatFromWalk,{0,eat.seconds},0,1,a)&&a.loop==0&&a.seconds==eat.seconds);
 a={1,0}; // Actual native loop increment, not an invented frozen wrap.
 CHECK(TryGetFusionJackson281Events(Phase::EatFromWalk,{0,eat.seconds},a,false,events)&&events==Night281Forward);
 CHECK(TryGetFusionJackson281Events(Phase::EatFromWalk,a,a,false,events)&&events==0);
 CHECK(TryGetFusionJackson281NextEvent(Phase::ArmRaise,{0,0},b)&&b.seconds==.8333333f);
 CHECK(TryGetFusionJackson281NextEvent(Phase::ArmRaise,b,a)&&a.seconds==1.6666666f);
 CHECK(TryGetFusionJackson281NextEvent(Phase::ArmRaise,a,b)&&b.seconds==2.5f);
 a={77,1};CHECK(!TryGetFusionJackson281NextEvent(Phase::Death,{0,2.8333333f},a)&&a.loop==77&&a.seconds==1);
 CHECK(!TryGetFusionJackson281NextEvent(Phase::Moonwalk,{0,0},a)&&a.loop==77&&a.seconds==1);

 // Explicit native root policy blends deltas, never disjoint absolute roots.
 // Point is constant; outgoing full Walk contributes 2.432-3.7106 only.
 double delta=99;const double walkDelta=double(2.432f)-3.7106f;
 CHECK(TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,3.3333333f},0,1,false,1,delta)&&closeEnough291(delta,walkDelta*.5));
 CHECK(TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,3.3333333f},0,0,false,1,delta)&&closeEnough291(delta,walkDelta));
 CHECK(TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,3.3333333f},1,1,false,1,delta)&&delta==0);
 CHECK(TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,0},Phase::Walk,{0,1},{0,1},0,1,false,1,delta)&&delta==0);
 const double projection=.31*(72.0/1.35)*1.2;
 CHECK(TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,3.3333333f},.25f,.75f,true,projection,delta)&&closeEnough291(delta,-walkDelta*.5*projection));
 delta=99;CHECK(!TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,1},.8f,.2f,false,1,delta)&&delta==99);
 CHECK(!TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,1},0,nan,false,1,delta)&&delta==99);
 CHECK(!TryGetFusionJackson281BlendRootDelta(Phase::Point,{0,0},{0,.2f},Phase::Walk,{0,0},{0,1},0,1,false,inf,delta)&&delta==99);
#undef CHECK
 return failures;
}
