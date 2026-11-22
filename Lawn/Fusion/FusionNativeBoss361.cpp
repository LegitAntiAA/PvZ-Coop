#include "FusionNativeBoss361.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeSourceClock313.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
bool Hooks(const NativeBoss361Hooks& h)
{return h.stateFor&&h.graph&&h.world&&h.ensure&&h.facts&&h.definedSourcePrefabs&&h.preflight&&h.apply&&h.randomInt&&h.failed&&h.graph->parametersForLayer361;}
bool Host(const Zombie* z,const NativeBoss361Hooks& h)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* c=z?GetNativeContext(z->mBoard):nullptr;
 return Hooks(h)&&z&&z->mApp&&z->mApp->mBoard==z->mBoard&&c&&!c->adventureTeardown&&!z->mDead&&
  !z->mNetGhost&&!z->mApp->NetIsClient()&&d&&FusionBoss361Kind(d->kind)&&h.world(z->mBoard,d->kind)&&
  d->instanceToken==z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z))&&z->mZombieType==ZOMBIE_BOSS;
}
struct Call
{
 LawnApp* app;Board* board;uint32_t epoch,uid;uint16_t kind;const NativeBoss361Hooks& h;
 Zombie* Resolve()const
 {
  if(app->mBoard!=board)return nullptr;const auto* c=GetNativeContext(board);if(!c||c->epoch!=epoch||c->adventureTeardown)return nullptr;
  auto* z=board->mZombies.DataArrayTryToGet(uid);const auto* d=GetNativeSourceZombieData(z);
  return Host(z,h)&&d&&d->kind==kind&&d->instanceToken==uid?z:nullptr;
 }
};
Call Capture(Zombie* z,const NativeBoss361Hooks& h)
{return{z->mApp,z->mBoard,GetNativeContext(z->mBoard)->epoch,z->mBoard->mZombies.DataArrayGetID(z),GetNativeSourceZombieData(z)->kind,h};}
NativeBoss361State* State(const Call& c)
{auto* z=c.Resolve();auto* s=z?c.h.stateFor(z):nullptr;return s&&s->initialized&&!s->failed&&s->localUID==c.uid?s:nullptr;}
bool Fail(const Call& c,const char* text)
{if(auto* s=State(c))s->failed=true;if(c.app->mBoard==c.board)c.h.failed(c.board,text);return false;}
bool Apply(const Call& c,NativeBoss361Operation op,const NativeBoss361Action& action)
{auto* z=c.Resolve();if(!z)return true;if(!c.h.apply(z,op,action))return Fail(c,"Boss361 source callback failed after consumed progress; no replay.");return true;}
NativeBoss361Operation Operation(FusionSourceGraph313Event e)
{
 using E=FusionSourceGraph313Event;using O=NativeBoss361Operation;
 switch(e){case E::BossSpawn:return O::SpawnChild;case E::BossBungi:return O::BungiTriple;
 case E::BossCrash:return O::Stomp;case E::BossRv:return O::RvCrushAndCards;case E::BossPutBall:return O::PutBall;
 case E::BossFootSound:return O::FootSound;case E::BossMachineSound2:return O::MachineSound2;
 case E::BossDieExplode:return O::DieExplode;case E::BossRemoveDebuff:return O::ResetDebuffs;
 case E::BossHeadLeaveEnter:return O::ClearFreeze;case E::BossDieOver:return O::DeathEnd;default:return O::Decision;}
}
bool Preflight(const Call& c,NativeBoss361Operation op,const NativeBoss361Action& a,std::string* error=nullptr)
{auto* z=c.Resolve();return z&&c.h.preflight(z,op,a,error);}
bool Plan(const Call& c,const FusionBoss361Plan& plan)
{
 NativeBoss361Action a;a.view=plan.next;a.decision=plan.decision;
 if(plan.decision!=FusionBoss361Decision::None&&!Preflight(c,NativeBoss361Operation::Decision,a))return false;
 if(plan.decision==FusionBoss361Decision::Rv&&!Preflight(c,NativeBoss361Operation::AlignRv,a))return false;
 const int row=plan.randomRowUpper?c.h.randomInt(c.board,0,plan.randomRowUpper):0;
 const int col=plan.randomColumnUpper?c.h.randomInt(c.board,0,plan.randomColumnUpper):0;
 const int ball=plan.randomBallType?c.h.randomInt(c.board,0,2):0;
 auto* z=c.Resolve();auto* s=State(c);auto* graph=z?c.h.graph->stateFor(z):nullptr;if(!s||!graph)return true;
 uint64_t main=0,driver=0;FusionBoss361View next;
 if(!ResolveFusionBoss361Plan(plan,row,col,ball,next,main,driver))return Fail(c,"Boss361 RNG callback returned an invalid source interval.");
 s->view=next;graph->layerParameters361[0].triggers|=main;graph->layerParameters361[2].triggers|=driver;a.view=next;
 if(plan.decision==FusionBoss361Decision::Rv&&!Apply(c,NativeBoss361Operation::AlignRv,a))return false;
 return plan.decision==FusionBoss361Decision::None||Apply(c,NativeBoss361Operation::Decision,a);
}
}
bool CanSpawnNativeBoss361(Board* b,uint16_t kind,const NativeBoss361Hooks& h,std::string* error)
{
 if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!FusionBoss361Kind(kind)||!Hooks(h)||!h.world(b,kind))return false;
 return h.ensure(b,kind,error);
}
bool InitializeNativeBoss361(Zombie* z,NativeBoss361State& s,const NativeBoss361Hooks& h)
{
 if(!Host(z,h)||!CanSpawnNativeBoss361(z->mBoard,GetNativeSourceZombieData(z)->kind,h))return false;
 const auto c=Capture(z,h);if(s.initialized)return s.localUID==c.uid&&!s.failed;
 // Native BossSetupReanim allocates BossDriver and one actual Attachment.
 // The RV source Animator is a third genuine native clock, never Normal AI.
 if(!h.graph->clock||h.graph->clock(z,0)!=z->mApp->ReanimationTryToGet(z->mBodyReanimID)||
  h.graph->clock(z,2)!=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID)||
  !h.graph->clock(z,1)||h.graph->clock(z,1)==h.graph->clock(z,0)||h.graph->clock(z,1)==h.graph->clock(z,2)||
  !NativeSourceGraph313ClockTracksReady(z,*h.graph))return false;
 NativeBoss361Action action;action.view.kind=c.kind;if(!Preflight(c,NativeBoss361Operation::Initialize,action))return false;
 s={};s.localUID=c.uid;s.view.kind=c.kind;s.initialized=true;
 auto* graph=h.graph->stateFor(z);if(!graph||!InitializeNativeSourceGraph313(z,*graph,*h.graph))return Fail(c,"Boss361 native body/driver/RV initialization failed.");
 return !State(c)||Apply(c,NativeBoss361Operation::Initialize,action);
}
bool NativeBoss361Parameters(Zombie* z,uint8_t layer,FusionSourceGraph313Parameters& p,const NativeBoss361Hooks& h)
{
 if(!Host(z,h)||layer>=3)return false;const auto* s=h.stateFor(z);NativeBoss361Facts facts;
 if(!s||!s->initialized||s->failed||!h.facts(z,facts))return false;
 p={};p.speed=layer==0?facts.mainAnimatorSpeed:1.f;return std::isfinite(p.speed)&&p.speed>=0;
}
bool UpdateNativeBoss361(Zombie* z,NativeBoss361State& s,const NativeBoss361Hooks& h)
{
 if(!Host(z,h)||!s.initialized||s.failed)return false;
 if(z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING||s.tick==z->mBoard->mMainCounter)return true;
 const auto c=Capture(z,h);NativeBoss361Facts facts;if(!h.facts(z,facts))return false;
 if(s.view.dying)s.view.eyeBlackSeconds=(std::min)(.75f,s.view.eyeBlackSeconds+.01f);
 FusionBoss361Plan plan;if(!PlanFusionBoss361Tick(s.view,facts.input,.01f,plan)||!Plan(c,plan))return false;
 auto* current=State(c);if(!current)return true;current->tick=c.board->mMainCounter;
 z=c.Resolve();auto* graph=h.graph->stateFor(z);return graph&&UpdateNativeSourceGraph313(z,*graph,*h.graph);
}
bool PreflightNativeBoss361Event(Zombie* z,FusionSourceGraph313Event e,const NativeBoss361Hooks& h,std::string* error)
{
 if(uint8_t(e)<uint8_t(FusionSourceGraph313Event::BossSpawn)||uint8_t(e)>uint8_t(FusionSourceGraph313Event::BossRvExit)||!Host(z,h))return false;const auto c=Capture(z,h);const auto* s=State(c);if(!s)return false;
 NativeBoss361Action a;a.view=s->view;return Preflight(c,Operation(e),a,error);
}
bool DispatchNativeBoss361Event(Zombie* z,FusionSourceGraph313Event e,int value,const NativeBoss361Hooks& h)
{
 if(uint8_t(e)<uint8_t(FusionSourceGraph313Event::BossSpawn)||uint8_t(e)>uint8_t(FusionSourceGraph313Event::BossRvExit)||!Host(z,h))return false;const auto c=Capture(z,h);auto* s=State(c);if(!s)return false;
 using E=FusionSourceGraph313Event;using O=NativeBoss361Operation;
 NativeBoss361Action a;a.view=s->view;a.payload=value;
 if(!Preflight(c,Operation(e),a))return false;
 if(e==E::BossHeadAttackExit)
 {
  const int count=h.randomInt(c.board,4,8);s=State(c);if(!s)return true;
  if(!ApplyFusionBoss361Behaviour(s->view,e,count))return Fail(c,"Boss361 invalid HeadAttackExit RNG.");
  z=c.Resolve();auto* graph=h.graph->stateFor(z);if(!graph)return false;
  graph->layerParameters361[0].triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::BossHeadLeave);return true;
 }
 if(e==E::BossHeadLeaveExit)
 {
  if(!ApplyFusionBoss361Behaviour(s->view,e))return false;NativeBoss361Facts facts;z=c.Resolve();
  if(!h.facts(z,facts))return false;FusionBoss361Plan plan;
  return PlanFusionBoss361Skill(s->view,facts.sourceHealth,facts.maximumHealth,plan)&&Plan(c,plan);
 }
 if(e==E::BossDriverDie)
 {auto* graph=h.graph->stateFor(z);if(!graph)return false;graph->layerParameters361[2].triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::BossDriverDie);return true;}
 if(e==E::BossIdleEnter||e==E::BossHeadEnterExit||e==E::BossHeadLeaveEnter||e==E::BossRvEnter||e==E::BossRvExit)
 {
  if(!ApplyFusionBoss361Behaviour(s->view,e))return false;
  if(e==E::BossRvEnter){auto* g=h.graph->stateFor(z);if(!g||!PlayNativeSourceGraph313(z,*g,1,"BossRv::RV_1",0,*h.graph))return false;}
  a.view=s->view;
  return e!=E::BossHeadLeaveEnter||Apply(c,O::ClearFreeze,a);
 }
 if(e==E::BossSpawn)
 {
  uint16_t prefabs[99]{},pool[99]{},n=0,count=0;NativeBoss361Facts facts;
  if(!h.facts(z,facts)||!h.definedSourcePrefabs(c.board,prefabs,n)||n>99||!FusionBoss361BuildChildPool(facts.boardBoss2,std::span(prefabs,n),pool,count))return false;
  if(s->view.summonCount<5)a.childKind=0;else if(s->view.summonCount<8)a.childKind=2;
  else{if(!count)return false;const int index=h.randomInt(c.board,0,count);if(index<0||index>=count)return Fail(c,"Boss361 child pool draw outside exact source interval.");a.childKind=pool[index];}
  // Source factory then counters. No replay after a partial child operation.
  if(!Apply(c,O::SpawnChild,a))return false;s=State(c);if(!s)return true;
  if(s->view.spawnCount<1||s->view.summonCount>=1000000)return Fail(c,"Boss361 source summon counters overflow.");
  --s->view.spawnCount;++s->view.summonCount;return true;
 }
 if(e==E::BossPutBall)
 {if(!Apply(c,O::PutBall,a))return false;s=State(c);if(!s)return true;if(s->view.summonBallCount>=1000000)return Fail(c,"Boss361 source ball count overflow.");++s->view.summonBallCount;return true;}
 if(e==E::BossDieOver)
 {if(s->deathEnded)return true;s->deathEnded=true;return Apply(c,O::DeathEnd,a);}
 return Apply(c,Operation(e),a);
}
bool ApplyNativeBoss361BodyDamage(Zombie* z,int damage,const NativeBoss361Hooks& h)
{
 if(!Host(z,h)||damage<0)return false;const auto c=Capture(z,h);auto* s=State(c);NativeBoss361Facts facts;
 const auto* d=GetNativeSourceZombieData(z);if(!s||s->view.dying||!d||!h.facts(z,facts))return false;
 const float health=d->health-float(damage);uint8_t stage=0;
 if(!PlanFusionBoss361BodyStage(c.kind,facts.maximumHealth,health,s->view.bodyStage,stage))return false;
 NativeBoss361Action a;a.view=s->view;a.view.bodyStage=stage;uint8_t writtenBand=0;
 if(!PlanFusionBoss361BodyStage(c.kind,facts.maximumHealth,health,0,writtenBand))return false;a.payload=writtenBand;
 if(c.kind==46&&writtenBand&&!Preflight(c,NativeBoss361Operation::GrantBodyTools,a))return false;
 auto* context=GetNativeContext(z->mBoard);if(!context)return false;auto found=context->sourceZombies.find(c.uid);if(found==context->sourceZombies.end())return false;auto* mutableData=&found->second;
 mutableData->health=health;z->mBodyHealth=NativeSourceZombieLivenessHealth(health);s->view.bodyStage=stage;if(writtenBand)s->view.bodyHistoryMask|=uint8_t(1u<<(writtenBand-1));
 return c.kind!=46||!writtenBand||Apply(c,NativeBoss361Operation::GrantBodyTools,a);
}
bool BeginNativeBoss361Death(Zombie* z,const NativeBoss361Hooks& h)
{
 if(!Host(z,h))return false;const auto c=Capture(z,h);auto* s=State(c);if(!s)return false;if(s->deathStarted)return true;
 NativeBoss361Action a;a.view=s->view;if(!Preflight(c,NativeBoss361Operation::DeathBegin,a))return false;
 // Whole retire batch/loot peak must be guaranteed by the thin native bridge.
 // This is source DieEvent, not native BossDie's Classic trophy/progress path.
 s->deathStarted=true;s->view.dying=true;s->view.sourceStatus=1;s->view.colliderEnabled=false;
 auto* graph=h.graph->stateFor(z);if(!graph)return Fail(c,"Boss361 lost actual graph before death commit.");
 graph->layerParameters361[0].triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die);a.view=s->view;
 return Apply(c,NativeBoss361Operation::DeathBegin,a);
}
}
