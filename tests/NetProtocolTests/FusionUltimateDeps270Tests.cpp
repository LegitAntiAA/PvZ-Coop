#include "FusionUltimateDeps270Tests.h"
#include "../../Lawn/Fusion/FusionUltimateDeps270Rules.h"
#include "../../Lawn/Fusion/FusionNativeUltimateDeps270.h"
#include "../../Lawn/Fusion/FusionUltimateGargantuar270Rules.h"
#include <cmath>
#include <cstdio>
#include <limits>
int RunFusionUltimateDeps270Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionUltimateDeps270Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    CHECK(FindFusionUltimateDeps270Profile(213)->maxHealth==1500&&FindFusionUltimateDeps270Profile(213)->biteDamage==200);
    CHECK(FindFusionUltimateDeps270Profile(37)->maxHealth==270&&FindFusionUltimateDeps270Profile(40)->maxHealth==1350);
    CHECK(!FindFusionUltimateDeps270Profile(212));
    CHECK(kFusionMachine270OffspringCount==5);
    CHECK(FusionMachine270CanReserveOffspring(95,100,false));
    CHECK(!FusionMachine270CanReserveOffspring(96,100,false));
    CHECK(!FusionMachine270CanReserveOffspring(95,100,true));
    CHECK(FusionMachine270CanReserveOffspring(94,100,true));
    CHECK(!FusionMachine270CanReserveOffspring(-1,100,false));
    CHECK(!FusionMachine270CanReserveOffspring(101,100,false));
    CHECK(FusionMachine270CanReserveOffspring((std::numeric_limits<int>::max)()-5,(std::numeric_limits<int>::max)(),false));
    for(int draw=0;draw<3;++draw){uint8_t type=255;CHECK(ChooseFusionMachine270Type(draw,type)&&type==draw);}
    uint8_t unchanged=7;CHECK(!ChooseFusionMachine270Type(-1,unchanged)&&unchanged==7);CHECK(!ChooseFusionMachine270Type(3,unchanged)&&unchanged==7);
    CHECK(FusionUltimateDeps270RejectEffect(40,0,FusionUltimateDeps270Effect::Butter));
    CHECK(FusionUltimateDeps270RejectEffect(40,0,FusionUltimateDeps270Effect::Poison));
    CHECK(!FusionUltimateDeps270RejectEffect(40,0,FusionUltimateDeps270Effect::Freeze));
    CHECK(FusionUltimateDeps270RejectEffect(213,16,FusionUltimateDeps270Effect::Freeze));
    CHECK(!FusionUltimateDeps270RejectEffect(213,16,FusionUltimateDeps270Effect::Cold));
    CHECK(!FusionUltimateDeps270RejectEffect(213,16,FusionUltimateDeps270Effect::Butter));
    CHECK(FusionUltimateDeps270RejectEffect(37,16,FusionUltimateDeps270Effect::Butter));
    CHECK(!FusionUltimateDeps270RejectEffect(37,0,FusionUltimateDeps270Effect::Freeze));
    CHECK(FusionMachine270DamageStage(900,1350,0)==1);
    CHECK(FusionMachine270DamageStage(450,1350,0)==0); // ValueBetween excludes exact lower boundary.
    CHECK(FusionMachine270DamageStage(450,1350,1)==1);
    CHECK(FusionMachine270DamageStage(449,1350,1)==2);
    CHECK(FusionMachine270DamageStage(1350,1350,1)==1); // Source appearance is not silently healed/reset.
    CHECK(FusionMachine270CanHitProjectile(0,false,true));
    CHECK(!FusionMachine270CanHitProjectile(0,true,true));
    CHECK(!FusionMachine270CanHitProjectile(13,false,true));
    CHECK(FusionMachine270CanHitProjectile(2,true,false)&&FusionMachine270CanHitProjectile(15,true,false)&&FusionMachine270CanHitProjectile(18,true,false));
    double delta=0;
    CHECK(TryGetFusionUltimateDeps270RootDelta(40,false,{0,0},{1,0},false,1,delta)&&std::abs(delta+12)<1e-6);
    CHECK(TryGetFusionUltimateDeps270RootDelta(40,false,{0,0},{1,0},true,1,delta)&&std::abs(delta-12)<1e-6);
    CHECK(TryGetFusionUltimateDeps270RootDelta(213,false,{0,0},{1,0},false,1,delta)&&std::abs(delta+1.8966666)<1e-5);
    CHECK(TryGetFusionUltimateDeps270RootDelta(37,false,{0,0},{1,0},false,1,delta)&&std::abs(delta-double(16.578789f-18.757576f))<1e-5);
    CHECK(TryGetFusionUltimateDeps270RootDelta(213,true,{0,0},{0,.8666667f},false,1,delta)&&std::abs(delta-double(-11.803332f+1.3170004f))<1e-5);
    CHECK(!TryGetFusionUltimateDeps270RootDelta(213,true,{0,0},{1,0},false,1,delta));
    CHECK(!TryGetFusionUltimateDeps270RootDelta(40,false,{1,0},{0,1},false,1,delta));
    uint32_t events=99;
    CHECK(CountFusionUltimateDeps270Events(213,true,{0,0},{0,.81666666f},events)&&events==1);
    CHECK(CountFusionUltimateDeps270Events(213,true,{0,.81666666f},{0,.81666666f},events)&&events==0);
    CHECK(!CountFusionUltimateDeps270Events(37,true,{0,0},{0,.81666666f},events));
    CHECK(CountFusionUltimateDeps270Events(37,false,{0,0},{0,2.1666667f},events)&&events==2);
    CHECK(CountFusionUltimateDeps270Events(37,false,{0,2.1666667f},{1,0},events)&&events==0);
    NativeUltimateDeps270State s;CHECK(NativeUltimateDeps270StateValid(37,s));s.visible=false;CHECK(NativeUltimateDeps270StateValid(37,s));
    CHECK(!NativeUltimateDeps270StateValid(213,s));s.visible=true;s.machineType=2;CHECK(NativeUltimateDeps270StateValid(40,s));CHECK(!NativeUltimateDeps270StateValid(37,s));
    s.moving=true;s.originRow=5;s.targetRow=4;s.rowTicks=200;s.startY=520;CHECK(NativeUltimateDeps270StateValid(40,s));
    s.targetRow=7;CHECK(!NativeUltimateDeps270StateValid(40,s));s.targetRow=4;s.rowTicks=201;CHECK(!NativeUltimateDeps270StateValid(40,s));
    CHECK(std::abs(FusionMachine270BallVisualSeconds(200)-.4975f)<1e-6f&&FusionMachine270BallVisualSeconds(3000)==FusionMachine270BallVisualSeconds(200));
    int capped=-1;CHECK(CapFusionUltimateGarg270Damage(9000,capped)&&capped==5000);CHECK(!CapFusionUltimateGarg270Damage(-1,capped)&&capped==5000);
    CHECK(!FusionUltimateGarg270MustThrow(4000,8000,true,false,4)&&FusionUltimateGarg270MustThrow(3999,8000,true,false,4));
    CHECK(!FusionUltimateGarg270MustThrow(3999,8000,true,false,3));
    CHECK(PlanFusionUltimateGarg270PlantHit(913,false,true,0)==FusionUltimateGarg270PlantHit::Damage500);
    CHECK(PlanFusionUltimateGarg270PlantHit(917,false,false,0)==FusionUltimateGarg270PlantHit::Crush);
#undef CHECK
    return failures;
}
