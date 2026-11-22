#include "../../Lawn/Fusion/FusionNightZombie281Rules.h"
#include "../../Lawn/Fusion/SpecialZombie269Rules.h"
#include "../../Lawn/Fusion/FusionDayZombie273Rules.h"
#include "../../Lawn/Fusion/FusionSourceZombieClosure280.h"
#include "../../ConstEnums.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

// Source oracles: InitZombieData1803BA5C0 packed stores; actual serialized
// controllers/clips in next-zombie40-art-common2/{5,6,7,9,10}/runtime/animation.json;
// JumpOver180352FF0, AnimSummon18037D2C0, FixedUpdate18037DF60, PointOver18037EA90.
// These tests do not validate native hooks, Unity rendering or multiplayer admission.
int RunFusionNightZombie281Tests(int& checks)
{
    using namespace LawnFusion;
    using Phase=FusionNightZombie281Phase;
    int failures=0;
    const auto check=[&](bool okay,int line)
    {++checks;if(!okay){++failures;std::printf("FusionNightZombie281Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    const auto closeEnough281=[](double a,double b){return std::abs(a-b)<2e-6;};
    const auto samePlan=[](const FusionNightZombie281Children& a,const FusionNightZombie281Children& b)
    {
        if(a.count!=b.count)return false;
        for(size_t i=0;i<a.actors.size();++i)
        {
            const auto& x=a.actors[i];const auto& y=b.actors[i];
            if(x.kind!=y.kind||x.slot!=y.slot||x.row!=y.row||x.sourceX!=y.sourceX||x.controlled!=y.controlled)return false;
        }
        return true;
    };
    const auto child=[](const FusionNightZombie281Children& p,size_t i,uint16_t kind,uint8_t slot,int row,float x,bool controlled)
    {return i<p.count&&p.actors[i].kind==kind&&p.actors[i].slot==slot&&p.actors[i].row==row&&
        p.actors[i].sourceX==x&&p.actors[i].controlled==controlled;};
    const float nan=(std::numeric_limits<float>::quiet_NaN)();

    // Five distinct source identities/stats, rather than carrier aliases.
    struct Profile {uint16_t kind;int body,first,second,carrier;};
    constexpr Profile profiles[]={{5,270,0,200,ZOMBIE_NEWSPAPER},{6,720,0,0,ZOMBIE_POLEVAULTER},
        {7,270,0,0,ZOMBIE_POLEVAULTER},{9,270,1400,0,ZOMBIE_FOOTBALL},{10,720,0,0,ZOMBIE_NORMAL}};
    for(const auto& oracle:profiles)
    {
        const auto* p=FindFusionZombieProfile(oracle.kind);
        CHECK(p&&p->kind==oracle.kind);
        CHECK(p&&p->maxHealth==oracle.body&&p->firstArmor==oracle.first&&p->secondArmor==oracle.second&&p->biteDamage==50);
        CHECK(GetNativeSourceZombieCarrier(oracle.kind)==oracle.carrier);
    }
    CHECK(FindSpecialZombie269Profile(5)!=FindSpecialZombie269Profile(15));
    CHECK(FindSpecialZombie269Profile(5)->maxHealth!=FindSpecialZombie269Profile(15)->maxHealth);
    FusionZombieSpawn spawn;
    for(uint16_t kind:{9,10})CHECK(TryPrepareFusionZombieSpawn(kind,2,1.8f,spawn)&&!spawn.walk2);

    // Paper5 owns ordinary paper speed/bite clocks, not SuperPaper15's angry speed.
    SpecialZombie269PaperClip paper;
    CHECK(TryGetSpecialZombie269PaperClip(5,4,false,paper)&&paper.duration==2&&paper.speed==.4f&&std::strcmp(paper.name,"walk")==0);
    CHECK(TryGetSpecialZombie269PaperClip(5,5,false,paper)&&paper.duration==2&&paper.speed==1&&std::strcmp(paper.name,"losePaper")==0);
    CHECK(TryGetSpecialZombie269PaperClip(5,6,false,paper)&&paper.duration==2.0166667f&&paper.speed==1);
    CHECK(TryGetSpecialZombie269PaperClip(5,6,true,paper)&&paper.duration==2&&paper.speed==3);
    CHECK(TryGetSpecialZombie269PaperClip(15,6,false,paper)&&paper.speed==4);
    CHECK(TryGetSpecialZombie269PaperClip(15,6,true,paper)&&paper.speed==6);
    CHECK(!SpecialZombie269RejectEffect(5,5,SpecialZombie269StatusEffect::Freeze));
    CHECK(SpecialZombie269RejectEffect(15,5,SpecialZombie269StatusEffect::Freeze));
    uint32_t bites=99;
    CHECK(TryCountSpecialZombie269PaperBites(5,{0,1.0833f},{0,1.0833334f},bites)&&bites==1);
    CHECK(TryCountSpecialZombie269PaperBites(5,{0,1.0833334f},{0,1.0833334f},bites)&&bites==0);
    CHECK(TryCountSpecialZombie269PaperBites(5,{0,0},{3,0},bites)&&bites==3);
    double delta=99;
    CHECK(TryGetSpecialZombie269PaperRootDelta(5,4,{0,0},{1,0},false,1,delta)&&closeEnough281(delta,double(-2.1259258f-1.12f)));

    // Real C# integer thresholds: equality retains the previous sprite; damage
    // stages persist after healing. Paper uses second armor; Football uses first.
    uint8_t stage=9;int sprite=99;
    CHECK(TryGetFusionNightZombie281ArmorStage(5,133,200,0,stage,sprite)&&stage==0&&sprite==-1);
    CHECK(TryGetFusionNightZombie281ArmorStage(5,132,200,0,stage,sprite)&&stage==1&&sprite==6);
    CHECK(TryGetFusionNightZombie281ArmorStage(5,66,200,1,stage,sprite)&&stage==1&&sprite==6);
    CHECK(TryGetFusionNightZombie281ArmorStage(5,65,200,1,stage,sprite)&&stage==2&&sprite==7);
    CHECK(TryGetFusionNightZombie281ArmorStage(9,933,1400,0,stage,sprite)&&stage==0&&sprite==-1);
    CHECK(TryGetFusionNightZombie281ArmorStage(9,932,1400,0,stage,sprite)&&stage==1&&sprite==20);
    CHECK(TryGetFusionNightZombie281ArmorStage(9,466,1400,1,stage,sprite)&&stage==1&&sprite==20);
    CHECK(TryGetFusionNightZombie281ArmorStage(9,465,1400,1,stage,sprite)&&stage==2&&sprite==21);
    CHECK(TryGetFusionNightZombie281ArmorStage(9,1400,1400,2,stage,sprite)&&stage==2&&sprite==21);
    stage=1;sprite=20;
    CHECK(!TryGetFusionNightZombie281ArmorStage(109,100,1400,1,stage,sprite)&&stage==1&&sprite==20);
    CHECK(!TryGetFusionNightZombie281ArmorStage(9,-1,1400,1,stage,sprite)&&stage==1&&sprite==20);
    CHECK(!TryGetFusionNightZombie281ArmorStage(9,1401,1400,1,stage,sprite)&&stage==1&&sprite==20);
    CHECK(!TryGetFusionNightZombie281ArmorStage(9,100,0,1,stage,sprite)&&stage==1&&sprite==20);
    CHECK(!TryGetFusionNightZombie281ArmorStage(9,100,1400,3,stage,sprite)&&stage==1&&sprite==20);

    // Pole7 shares base Pole3 curves; DancePole6's walk curve is genuinely different.
    FusionDayZombie273Clip pole;
    for(uint16_t kind:{6,7})
    {
        CHECK(FusionDayZombie273Pole(kind));
        CHECK(TryGetFusionDayZombie273Clip(kind,2,false,pole)&&pole.seconds==3&&pole.multiplier==2.5f);
        CHECK(std::strcmp(pole.sourceName,"Zombie_polevaulter_run")==0);
        CHECK(TryGetFusionDayZombie273Clip(kind,3,false,pole)&&pole.seconds==3.5f&&pole.multiplier==2&&!pole.loop);
        CHECK(std::strcmp(pole.sourceName,kind==6?"Zombie_polevaulter_jump 1":"Zombie_polevaulter_jump")==0);
        CHECK(TryCountFusionDayZombie273Bites(kind,{0,0},{1,0},bites)&&bites==2);
        CHECK(TryCountFusionDayZombie273Bites(kind,{0,.9833f},{0,.98333335f},bites)&&bites==1);
        CHECK(TryGetFusionDayZombie273RootDelta(kind,3,{0,0},{0,3.5f},false,1,delta)&&closeEnough281(delta,double(2.2784f-4.1048f)));
    }
    CHECK(TryGetFusionDayZombie273Clip(6,1,false,pole)&&pole.multiplier==2);
    CHECK(TryGetFusionDayZombie273Clip(7,1,false,pole)&&pole.multiplier==1.8f);
    CHECK(TryGetFusionDayZombie273RootDelta(6,0,{0,0},{1,0},false,1,delta)&&closeEnough281(delta,double(4.4944f-5.131f)));
    CHECK(TryGetFusionDayZombie273RootDelta(7,0,{0,0},{1,0},false,1,delta)&&closeEnough281(delta,double(4.56f-5.131f)));
    CHECK(TryGetFusionDayZombie273RootDelta(6,0,{0,0},{0,.35f},false,1,delta)&&closeEnough281(delta,double(5.0512f-5.131f)));
    CHECK(TryGetFusionDayZombie273RootDelta(7,0,{0,0},{0,.35f},false,1,delta)&&closeEnough281(delta,double(5.0848f-5.131f)));
    CHECK(TryCountFusionZombieAttackEvents(9,{0,0},{0,1.0166667f},bites)&&bites==1);
    CHECK(TryCountFusionZombieAttackEvents(9,{0,1.0166667f},{0,2.4833333f},bites)&&bites==1);
    CHECK(TryCountFusionZombieAttackEvents(9,{0,0},{2,0},bites)&&bites==4);
    CHECK(TryCountFusionZombieAttackEvents(10,{0,0},{0,2.4f},bites)&&bites==2);

    constexpr uint8_t grass[]={0,0,0,0,0},waterNeighbors[]={0,1,0,1,0};
    FusionNightZombie281Children plan;
    CHECK(TryPlanFusionDancePole281(2,grass,4,false,0,false,plan)&&plan.count==4);
    CHECK(child(plan,0,7,0,2,5.5f,false)&&child(plan,1,7,1,2,2.5f,false));
    CHECK(child(plan,2,7,2,1,4,false)&&child(plan,3,7,3,3,4,false));
    CHECK(TryPlanFusionDancePole281(0,grass,4,false,0,false,plan)&&plan.count==3&&child(plan,2,7,3,1,4,false));
    CHECK(TryPlanFusionDancePole281(4,grass,4,false,0,false,plan)&&plan.count==3&&child(plan,2,7,2,3,4,false));
    CHECK(TryPlanFusionDancePole281(2,grass,4,true,0,false,plan)&&plan.count==2);
    CHECK(TryPlanFusionDancePole281(2,waterNeighbors,4,false,0,false,plan)&&plan.count==2);
    CHECK(TryPlanFusionDancePole281(2,grass,4,false,1,false,plan)&&plan.count==0);
    CHECK(TryPlanFusionDancePole281(2,grass,4,false,0,true,plan)&&plan.count==0);

    std::array<bool,4> present{};
    CHECK(TryPlanFusionJackson281(2,grass,4,false,0,false,false,present,plan)&&plan.count==4);
    CHECK(child(plan,0,6,0,1,4,false)&&child(plan,1,6,1,2,5,false));
    CHECK(child(plan,2,6,2,2,2.5f,false)&&child(plan,3,6,3,3,4,false));
    CHECK(TryPlanFusionJackson281(2,grass,4,false,0,false,true,present,plan)&&plan.count==4);
    for(size_t i=0;i<plan.count;++i)CHECK(plan.actors[i].controlled&&plan.actors[i].kind==6);
    // A live/dying-but-not-destroyed child is still present. Only the missing
    // original slot refills; never wholesale replace all four or convert old ones.
    present={true,true,false,true};
    CHECK(TryPlanFusionJackson281(2,grass,4,false,0,false,true,present,plan)&&plan.count==1&&child(plan,0,6,2,2,2.5f,true));
    present={true,true,true,true};
    CHECK(TryPlanFusionJackson281(2,grass,4,false,0,false,false,present,plan)&&plan.count==0);
    present={};
    CHECK(TryPlanFusionJackson281(0,grass,4,false,0,false,false,present,plan)&&plan.count==3&&plan.actors[0].slot==1);
    CHECK(TryPlanFusionJackson281(4,grass,4,false,0,false,false,present,plan)&&plan.count==3&&plan.actors[2].slot==2);
    CHECK(TryPlanFusionJackson281(2,grass,1.499f,true,0,false,false,present,plan)&&plan.count==0);
    CHECK(TryPlanFusionJackson281(2,grass,1.5f,true,0,false,false,present,plan)&&plan.count==4);
    CHECK(TryPlanFusionJackson281(2,grass,-4,false,0,false,false,present,plan)&&plan.count==4);
    CHECK(TryPlanFusionJackson281(2,waterNeighbors,4,false,0,false,false,present,plan)&&plan.count==2);
    CHECK(TryPlanFusionJackson281(2,grass,4,false,1,false,false,present,plan)&&plan.count==0);
    CHECK(TryPlanFusionJackson281(2,grass,4,false,0,true,false,present,plan)&&plan.count==0);
    present={false,true,true,true};CHECK(!FusionJackson281MissingDancer(0,5,present));
    CHECK(FusionJackson281MissingDancer(2,5,present));
    present={true,true,true,false};CHECK(!FusionJackson281MissingDancer(4,5,present));
    CHECK(FusionJackson281MissingDancer(2,5,present));
    present={true,true,true,true};CHECK(!FusionJackson281MissingDancer(2,5,present));
    present[1]=false;CHECK(FusionJackson281MissingDancer(0,5,present));
    // FixedUpdate does not consider water when deciding loseDancer: unavailable
    // neighbor references can keep it true even though AnimSummon skips the row.
    present={false,true,true,false};CHECK(FusionJackson281MissingDancer(2,5,present));
    CHECK(!FusionJackson281MissingDancer(-1,5,present)&&!FusionJackson281MissingDancer(0,0,present));
    const auto savedPlan=plan;
    CHECK(!TryPlanFusionJackson281(-1,grass,4,false,0,false,false,present,plan)&&samePlan(plan,savedPlan));
    CHECK(!TryPlanFusionJackson281(2,{},4,false,0,false,false,present,plan)&&samePlan(plan,savedPlan));
    CHECK(!TryPlanFusionJackson281(2,grass,nan,false,0,false,false,present,plan)&&samePlan(plan,savedPlan));
    CHECK(!TryPlanFusionDancePole281(5,grass,4,false,0,false,plan)&&samePlan(plan,savedPlan));
    constexpr uint8_t invalidRows[]={0,0,3,0,0};
    CHECK(!TryPlanFusionDancePole281(2,invalidRows,4,false,0,false,plan)&&samePlan(plan,savedPlan));

    // Exact Jackson clip/controller metadata. In particular walk is NOT looping.
    FusionNightZombie281Clip clip;
    CHECK(TryGetFusionNightZombie281Clip(Phase::Moonwalk,clip)&&clip.seconds==2&&clip.multiplier==1.5f&&clip.loop);
    CHECK(TryGetFusionNightZombie281Clip(Phase::Point,clip)&&clip.seconds==2.3333333f&&clip.multiplier==.9f&&!clip.loop);
    CHECK(TryGetFusionNightZombie281Clip(Phase::Walk,clip)&&clip.seconds==3.3333333f&&clip.multiplier==.8f&&!clip.loop);
    CHECK(TryGetFusionNightZombie281Clip(Phase::ArmRaise,clip)&&clip.seconds==3.3333333f&&clip.multiplier==.9f&&!clip.loop);
    CHECK(TryGetFusionNightZombie281Clip(Phase::EatFromWalk,clip)&&clip.seconds==2.7333333f&&clip.multiplier==2&&clip.loop);
    CHECK(TryGetFusionNightZombie281Clip(Phase::Death,clip)&&clip.seconds==2.8333333f&&clip.multiplier==1.6f&&!clip.loop);
    const auto savedClip=clip;
    CHECK(!TryGetFusionNightZombie281Clip(Phase(255),clip)&&clip.name==savedClip.name&&clip.seconds==savedClip.seconds);

    FusionNightZombie281Transition transition;
    CHECK(!TryGetFusionJackson281Transition(Phase::Moonwalk,{0,0},{0,.5f},false,false,true,true,transition));
    CHECK(TryGetFusionJackson281Transition(Phase::Moonwalk,{0,0},{0,.5f},true,false,true,true,transition)&&transition.target==Phase::Point&&transition.duration==.25f&&transition.fixedSeconds);
    CHECK(!TryGetFusionJackson281Transition(Phase::Point,{0,0},{0,1.9f},true,false,true,false,transition));
    CHECK(TryGetFusionJackson281Transition(Phase::Point,{0,1.9215f},{0,1.9217f},true,false,true,false,transition)&&transition.target==Phase::ArmRaise&&transition.duration==.25f&&transition.fixedSeconds);
    // Actual transition list order: no-missing exit outranks attack; attack
    // outranks missing-dancer point at the same tick. Freeze blocks Speed>0 guards.
    CHECK(TryGetFusionJackson281Transition(Phase::Walk,{0,3.08f},{0,3.09f},true,true,false,false,transition)&&transition.target==Phase::ArmRaise);
    CHECK(TryGetFusionJackson281Transition(Phase::Walk,{0,2.99f},{0,3.01f},true,true,true,false,transition)&&transition.target==Phase::EatFromWalk);
    CHECK(TryGetFusionJackson281Transition(Phase::Walk,{0,2.99f},{0,3.01f},true,false,true,false,transition)&&transition.target==Phase::Point&&transition.duration==.1f&&!transition.fixedSeconds);
    CHECK(!TryGetFusionJackson281Transition(Phase::Walk,{0,0},{0,.5f},false,true,true,false,transition));
    CHECK(TryGetFusionJackson281Transition(Phase::ArmRaise,{0,3.32f},{0,3.3333333f},true,true,true,false,transition)&&transition.target==Phase::Walk&&transition.duration==.02f);
    CHECK(TryGetFusionJackson281Transition(Phase::ArmRaise,{0,0},{0,.5f},true,true,true,false,transition)&&transition.target==Phase::EatFromRaise);
    CHECK(TryGetFusionJackson281Transition(Phase::EatFromWalk,{0,0},{0,.5f},true,false,true,false,transition)&&transition.target==Phase::Walk);
    CHECK(TryGetFusionJackson281Transition(Phase::EatFromRaise,{0,0},{0,.5f},true,false,true,false,transition)&&transition.target==Phase::ArmRaise);
    CHECK(!TryGetFusionJackson281Transition(Phase::EatFromRaise,{0,0},{0,.5f},false,false,true,false,transition));
    CHECK(!TryGetFusionJackson281Transition(Phase::Death,{0,0},{0,.5f},true,true,true,true,transition));
    const auto savedTransition=transition;
    CHECK(!TryGetFusionJackson281Transition(Phase::Walk,{0,0},{1,0},true,true,true,true,transition)&&transition.target==savedTransition.target&&transition.duration==savedTransition.duration);

    uint16_t events=999;
    CHECK(TryGetFusionJackson281Events(Phase::Point,{0,0},{0,0},true,events)&&events==Night281Forward);
    CHECK(TryGetFusionJackson281Events(Phase::Point,{0,0},{0,0},false,events)&&events==0);
    CHECK(TryGetFusionJackson281Events(Phase::Point,{0,.6666f},{0,.6666667f},false,events)&&events==Night281Summon);
    CHECK(TryGetFusionJackson281Events(Phase::Point,{0,.6666667f},{0,.6666667f},false,events)&&events==0);
    // Exit occurs at clip1.9215686, before PointOver2.0. Its OUTGOING clip
    // callbacks must continue across the .25s blend; incoming armraise cannot
    // replace them. This tests the pure spans, not native blend dispatch wiring.
    CHECK(TryGetFusionJackson281Events(Phase::Point,{0,1.9217f},{0,2},false,events)&&events==Night281PointOver);
    CHECK(TryGetFusionJackson281Events(Phase::ArmRaise,{0,0},{0,.2f},true,events)&&events==Night281Forward);
    CHECK(TryGetFusionJackson281Events(Phase::Point,{0,2},{0,2.1f},false,events)&&events==0);
    CHECK(TryGetFusionJackson281Events(Phase::ArmRaise,{0,.8333f},{0,.8333333f},false,events)&&events==Night281Back);
    CHECK(TryGetFusionJackson281Events(Phase::ArmRaise,{0,1.6666f},{0,1.6666666f},false,events)&&events==Night281Forward);
    CHECK(TryGetFusionJackson281Events(Phase::ArmRaise,{0,2.49f},{0,2.5f},false,events)&&events==Night281Back);
    CHECK(TryGetFusionJackson281Events(Phase::EatFromWalk,{0,1},{0,1.0833334f},false,events)&&events==Night281Bite1);
    CHECK(TryGetFusionJackson281Events(Phase::EatFromRaise,{0,2.39f},{0,2.4f},false,events)&&events==Night281Bite2);
    CHECK(TryGetFusionJackson281Events(Phase::Death,{0,2.49f},{0,2.5f},false,events)&&events==Night281Fall);
    CHECK(TryGetFusionJackson281Events(Phase::Death,{0,2.5f},{1,2.8333333f},false,events)&&events==Night281Destroy);
    CHECK(TryGetFusionJackson281Events(Phase::Death,{1,2.8333333f},{1,2.8333333f},false,events)&&events==0);
    CHECK(TryGetFusionJackson281Events(Phase::EatFromWalk,{0,2.7333333f},{1,0},false,events)&&events==Night281Forward);
    events=999;
    CHECK(!TryGetFusionJackson281Events(Phase::Point,{0,0},{1,0},false,events)&&events==999);
    CHECK(!TryGetFusionJackson281Events(Phase::Death,{0,0},{2,2.8333333f},false,events)&&events==999);
    CHECK(!TryGetFusionJackson281Events(Phase::Walk,{0,1},{0,.5f},false,events)&&events==999);
    CHECK(!TryGetFusionJackson281Events(Phase::Point,{0,0},{0,nan},false,events)&&events==999);

    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Walk,{0,0},{0,3.3333333f},false,1,delta)&&closeEnough281(delta,double(2.432f-3.7106f)));
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Walk,{0,0},{1,3.3333333f},false,1,delta)&&closeEnough281(delta,double(2.432f-3.7106f)));
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Walk,{0,3.3333333f},{1,3.3333333f},false,1,delta)&&delta==0);
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Walk,{0,.33333334f},{0,.5f},false,1,delta)&&delta==0);
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Walk,{0,3},{0,3.3333333f},false,1,delta)&&delta==0);
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Moonwalk,{0,0},{1,0},false,1,delta)&&closeEnough281(delta,double(5.1099997f-4.1067133f)));
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Moonwalk,{0,0},{1,0},true,1,delta)&&delta<0);
    CHECK(TryGetFusionNightZombie281RootDelta(Phase::Point,{0,0},{0,1},false,1,delta)&&delta==0);
    delta=99;
    CHECK(!TryGetFusionNightZombie281RootDelta(Phase::Walk,{0,0},{1,0},false,1,delta)&&delta==99);
    CHECK(!TryGetFusionNightZombie281RootDelta(Phase::Moonwalk,{0,1},{0,0},false,1,delta)&&delta==99);
    CHECK(!TryGetFusionNightZombie281RootDelta(Phase::Moonwalk,{0,0},{0,1},false,-1,delta)&&delta==99);

    // Canonical transitive identities, stable de-duplication, alias-safe and
    // atomic failure. Missing actors are never replaced with original PvZ types.
    std::vector<uint16_t> closure{999};std::string error;
    const uint16_t jackson[]={10};
    // Jackson now requires its complete genuine controller/child closure.
    CHECK(BuildFusionSourceZombieClosure280(jackson,closure,&error)&&closure==std::vector<uint16_t>({10,6,7}));
    CHECK(std::vector<uint16_t>(FusionDayZombie273NpcDependencies(10).begin(),FusionDayZombie273NpcDependencies(10).end())==std::vector<uint16_t>({6,7}));
    const uint16_t overlap[]={6,7,6,5,9};
    CHECK(BuildFusionSourceZombieClosure280(overlap,closure,&error)&&closure==std::vector<uint16_t>({6,7,5,9}));
    CHECK(BuildFusionSourceZombieClosure280(closure,closure,&error)&&closure==std::vector<uint16_t>({6,7,5,9}));
    // New registry coverage does not prove a resource/native/runtime gate.
    const uint16_t goldOverlap[]={6,203};
    CHECK(BuildFusionSourceZombieClosure280(goldOverlap,closure,&error)&&closure==std::vector<uint16_t>({6,7,203}));
    const auto savedClosure=closure;const uint16_t unsupported[]={6,44};
    CHECK(!BuildFusionSourceZombieClosure280(unsupported,closure,&error)&&closure==savedClosure);
    CHECK(error.find("rootKind=44")!=std::string::npos&&error.find("dependencyKind=44")!=std::string::npos);
    const uint16_t all[]={0,2,4,8,11,12,13,14,15,18,41,42,104,109,111,3,21,22,23,100,106,107,108,5,6,7,9};
    CHECK(BuildFusionSourceZombieClosure280(all,closure,&error)&&closure.size()==27&&error.empty());
#undef CHECK
    return failures;
}
