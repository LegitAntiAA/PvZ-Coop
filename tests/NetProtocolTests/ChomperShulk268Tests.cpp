#include "ChomperShulk268Tests.h"
#include "../../Lawn/Fusion/FusionChomper268Rules.h"
#include "../../Lawn/Fusion/FusionShulk268Rules.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include "../../Lawn/Fusion/FusionRollingProjectileRules.h"
#include <cstdio>
#include <cmath>
#include <limits>
#include <initializer_list>
ChomperShulk268TestResult RunChomperShulk268Tests()
{
    using namespace LawnFusion;ChomperShulk268TestResult r;
#define CHECK(...) do{++r.mChecks;if(!(__VA_ARGS__)){++r.mFailures;std::printf("ChomperShulk268:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    FusionStrongChomperBite b;
    CHECK(TryPlanFusionStrongChomperBite(1013,500,false,0,true,0,b)&&b.sourceDamage==500&&b.recovery==400&&b.nextShotKind==6&&!b.rechargeTicks);
    CHECK(TryPlanFusionStrongChomperBite(1013,501,false,0,true,4,b)&&b.sourceDamage==1000000&&b.recovery==1000000&&b.nextShotKind==4&&b.rechargeTicks==4000);
    CHECK(TryPlanFusionStrongChomperBite(1013,100,true,1,false,5,b)&&b.sourceDamage==200&&b.recovery==200&&b.nextShotKind==5&&!b.rechargeTicks);
    CHECK(!TryPlanFusionStrongChomperBite(1013,501,false,0,true,6,b));
    for(uint16_t k:{903,921})
    {
        CHECK(TryPlanFusionStrongChomperBite(k,1999,true,1,false,0,b)&&b.sourceDamage==1000000&&b.recovery==8000);
        CHECK(TryPlanFusionStrongChomperBite(k,2000,true,1,false,0,b)&&b.sourceDamage==1000&&b.recovery==1000);
        int hp=0,damage=0;CHECK(TryCapFusionStrongChomperDamage(k,9000,damage)&&damage==2666);
        CHECK(TryCapFusionStrongChomperHealth(k,1000000,hp)&&hp==32000);
        CHECK(TryDecayFusionStrongChomperHealth(k,8001,hp)&&hp==8000);
        CHECK(TryDecayFusionStrongChomperHealth(k,10000,hp)&&hp==9200);
    }
    int hp=0,damage=0;CHECK(TryCapFusionStrongChomperDamage(1013,2000,damage)&&damage==1333);
    CHECK(TryDecayFusionStrongChomperHealth(1013,8000,hp)&&hp==7800);
    CHECK(TryCapFusionStrongChomperHealth(1013,20000,hp)&&hp==16000);
    uint8_t stage=0;CHECK(TryAdvanceFusionStrongChomperStage(1013,1333,1,stage)&&stage==1);
    CHECK(TryAdvanceFusionStrongChomperStage(1013,2666,2,stage)&&stage==2);
    uint16_t projectile=0;uint8_t way=0;CHECK(TryGetFusionStrongChomperShot(1013,6,projectile,way,damage)&&projectile==6&&way==1&&damage==80);
    CHECK(!TryGetFusionStrongChomperShot(1013,0,projectile,way,damage));
    CHECK(TryGetFusionStrongChomperShot(921,0,projectile,way,damage)&&projectile==89&&way==0&&damage==300);
    CHECK(!TryPlanFusionStrongChomperBite(1013,std::numeric_limits<float>::quiet_NaN(),false,0,false,0,b));
    for(float speed:{.9f,1.f,1.1f})
    {int ticks=0;CHECK(TryGetFusionStrongChomperBiteTicks(speed,ticks));int bites=0,shots=0;for(int t=ticks;t>=0;--t){bites+=FusionStrongChomperEventDue(speed,t,false);shots+=FusionStrongChomperEventDue(speed,t,true);}CHECK(bites==1&&shots==1);}
    for(int s=0;s<=24;++s)
    {
        const bool chomper=s!=1&&s!=7&&s!=10&&s!=12&&s!=16&&s<20;
        const bool shulk=chomper&&s!=3&&s!=9;
        CHECK(FusionStrongChomperTarget(s,0,false,false,true)==chomper);
        CHECK(FusionShulk268Target(s,0,false,true,true)==shulk);
        CHECK(!FusionStrongChomperTarget(s,1,false,false,true));
        CHECK(!FusionShulk268Target(s,0,true,true,true));
    }
    int ticks=0;CHECK(TryGetFusionShulk268PhaseTicks(33,1,ticks)&&ticks==63);
    CHECK(TryGetFusionStrongChomperBiteTicks(1,ticks)&&ticks==175);
    CHECK(TryGetFusionShulk268PhaseTicks(1177,1,ticks)&&ticks==64);
    CHECK(TryGetFusionShulk268PhaseTicks(1177,3,ticks)&&ticks==24);
    CHECK(TryGetFusionShulk268PhaseTicks(1177,4,ticks)&&ticks==25);
    CHECK(FusionShulk268StateValid(33,0,0,0,4));CHECK(!FusionShulk268StateValid(33,1,0,1,2));
    const auto* timing=FindFusionShulk268Timing(1177);CHECK(timing&&FusionShooterTimingProfileValid(*timing)&&!timing->speedParameterActive&&timing->eventClipSeconds.size()==1);
    float vx=0,vy=0;CHECK(TryStepFusionShulk268Steering(-2,1,1,0,true,vx,vy));
    CHECK(vx<0&&vy>0&&std::abs(std::hypot(vx,vy)-8)<1e-6); // Smooth turning, not instantaneous homing/gravity.
    float straightX=0,straightY=0;CHECK(TryStepFusionShulk268Steering(1,0,1,0,false,straightX,straightY)&&straightX==8&&straightY==0);
    CHECK(FusionChomper268ProjectileMetadataValid(94,10,16));CHECK(FusionChomper268ProjectileMetadataValid(94,10,2));
    CHECK(!FusionChomper268ProjectileMetadataValid(94,10,6));CHECK(!FusionChomper268ProjectileMetadataValid(94,20,16));
    FusionRollingRect roll;CHECK(TryMakeFusionRollingRect(6,100,200,roll)&&std::abs(roll.width-.537*kFusionRollingPixelsX)<1e-9);
    for(double py:{kFusionBasePixelsY,85./1.45})
    {FusionRecipientRect rect;CHECK(TryMakeFusionChomper268ProjectileRect(89,100,200,0,py,rect));CHECK(std::abs(rect.width-2.2239718*.276256*kFusionBasePixelsX)<1e-9);CHECK(FusionChomper268ProjectileTouchesRect(89,100,200,0,py,rect));CHECK(!FusionChomper268ProjectileTouchesRect(94,100,200,0,py,{300,400,1,1}));}
#undef CHECK
    return r;
}
