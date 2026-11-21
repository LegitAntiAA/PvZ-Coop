#include "FusionFireActionBatchTests.h"
#include "../../Lawn/Fusion/FusionFireActionBatchRules.h"
#include "../../Lawn/Fusion/FusionFireRules.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

FusionFireActionBatchTestResult RunFusionFireActionBatchTests()
{
    FusionFireActionBatchTestResult result{};
    using namespace LawnFusion;
#define CHECK(x) do{++result.mChecks;if(!(x)){++result.mFailures;std::printf("FireActionBatch:%d %s\n",__LINE__,#x);}}while(0)
    const uint16_t enabled[]={20,22,1052,1053,1055,1059,1061,1062,1063,1064,1080,1091,1173,1179,1182};
    for(auto kind:enabled)CHECK(IsFusionFireActionSupported(kind));
    // Real FireAction initialization rolls this source Speed before the first
    // snapshot. It must use the same finite interval as other native families.
    for(auto kind:enabled)
    {
        for(float speed:{.9f,1.0f,1.1f})CHECK(FusionNativeAnimationSpeedValid(kind,speed));
        for(float speed:{0.0f,std::nextafter(.9f,0.0f),std::nextafter(1.1f,2.0f),
            (std::numeric_limits<float>::infinity)(),-(std::numeric_limits<float>::infinity)(),
            (std::numeric_limits<float>::quiet_NaN)()})CHECK(!FusionNativeAnimationSpeedValid(kind,speed));
    }
    CHECK(!IsFusionFireActionSupported(30)&&!IsFusionFireActionSupported(1178));
    CHECK(!FindFusionFireActionProfile(65535));
    CHECK(FindFusionFireActionProfile(1173)->maximumHealth==32000);
    CHECK(FindFusionFireActionProfile(1091)->maximumHealth==4000);
    CHECK(FusionFireActionInvincible(22)&&FusionFireActionInvincible(1080)&&FusionFireActionInvincible(1179));
    CHECK(!FusionFireActionInvincible(1173)); // Instead35B050 returns false.

    auto convert=[](uint16_t torch,int damage,uint8_t level,FusionTorchConversion& plan,bool smallProjectile=false,
        uint8_t special=0,ProjectileMotion motion=MOTION_STRAIGHT,bool same=true)
    {return TryResolveFusionTorchConversion(torch,0,damage,0,level,special,same,false,motion,0,0,7,0x10001,plan,smallProjectile);};
    FusionTorchConversion plan;
    CHECK(convert(18,20,0,plan)&&plan.damage==40&&plan.fireState==1&&!plan.replace);
    CHECK(convert(1053,20,0,plan)&&plan.damage==60&&plan.fireState==2);
    CHECK(convert(1053,40,1,plan)&&plan.damage==80&&plan.fireState==2);
    const int prior[]={20,40,60,80},red[]={80,100,120,140};
    const uint8_t levels[]={0,1,2,2};
    for(int i=0;i<4;++i)CHECK(convert(1052,prior[i],levels[i],plan)&&plan.damage==red[i]&&plan.fireState==3);
    CHECK(convert(1055,20,0,plan)&&plan.damage==30&&plan.fireState==1&&plan.smallProjectile&&plan.replace&&plan.emittedCount==3);
    CHECK(convert(1053,30,1,plan,true)&&plan.damage==55&&plan.fireState==2&&plan.smallProjectile);
    CHECK(convert(1052,30,1,plan,true)&&plan.damage==70&&plan.fireState==3&&plan.smallProjectile);
    CHECK(convert(1052,55,2,plan,true)&&plan.damage==95&&plan.fireState==3&&plan.smallProjectile);
    CHECK(convert(1055,20,0,plan,false,3,MOTION_THREEPEATER,false));
    CHECK(!convert(18,20,0,plan,false,3,MOTION_THREEPEATER,false));
    for(uint8_t tier:{16,17,18})CHECK(convert(1053,20,0,plan,false,tier));
    CHECK(convert(1053,20,0,plan,false,15,MOTION_STAR,false));
    CHECK(!convert(1182,95,3,plan,true));
    CHECK(convert(1182,140,3,plan)&&plan.action==FusionTorchConversionAction::RemoveHeat&&plan.kind==0&&plan.damage==20&&!plan.fireState&&!plan.smallProjectile);
    CHECK(convert(1182,20,0,plan)&&plan.kind==15&&plan.replace);
    CHECK(TryResolveFusionTorchConversion(1182,15,20,0,0,1,true,false,MOTION_BACKWARDS,0,0,7,9,plan)&&plan.kind==97&&plan.replace);
    CHECK(TryResolveFusionTorchConversion(1182,15,20,0,0,15,false,false,MOTION_STAR,0,0,7,9,plan)&&plan.kind==97&&plan.replace);
    CHECK(FusionFireProjectileMetadataValid(15,20,0,0,7,9,PROJECTILE_PEA));
    CHECK(FusionFireProjectileMetadataValid(97,20,0,0,7,9,PROJECTILE_PEA));
    CHECK(TryResolveFusionTorchConversion(1182,15,20,0,0,3,true,false,MOTION_THREEPEATER,8,10,7,9,plan)&&plan.kind==97&&plan.replace);
    const auto extreme=plan;
    CHECK(!TryResolveFusionTorchConversion(1182,97,20,0,0,15,false,false,MOTION_STAR,0,0,7,9,plan)&&plan==extreme);
    CHECK(TryResolveFusionTorchConversion(1052,1,60,0,0,0,true,false,MOTION_STRAIGHT,0,0,7,9,plan)&&plan.damage==120&&plan.fireState==1);
    CHECK(TryResolveFusionTorchConversion(1061,10,20,0,0,0,true,false,MOTION_STRAIGHT,0,0,7,9,plan)&&plan.damage==40&&plan.kind==10);
    CHECK(TryResolveFusionTorchConversion(1053,8,100,5,0,0,true,false,MOTION_STRAIGHT,0,0,7,9,plan)&&plan.damage==400&&plan.sunOnHit==25&&!plan.stamp);
    auto old=plan;
    CHECK(!convert(1053,40,2,plan)&&plan==old); // Invalid source level/damage pair.
    CHECK(!TryResolveFusionTorchConversion(18,0,20,0,0,0,true,false,MOTION_STRAIGHT,7,9,7,9,plan)&&plan==old);
    CHECK(!TryResolveFusionTorchConversion(1052,8,100,5,0,3,true,false,MOTION_STRAIGHT,0,0,7,9,plan)&&plan==old);
    CHECK(!TryResolveFusionTorchConversion(1052,0,20,0,0,0,true,true,MOTION_STRAIGHT,0,0,7,9,plan)&&plan==old);
    CHECK(!convert(1052,30,1,plan,true,16)&&plan==old);
    CHECK(!TryResolveFusionTorchConversion(30,0,20,0,0,0,true,false,MOTION_STRAIGHT,0,0,7,9,plan)&&plan==old);
    for(uint16_t kind:{2,9,11,12,13,18,28,65535})
        CHECK(!TryResolveFusionTorchConversion(1052,kind,20,0,0,0,true,false,MOTION_STRAIGHT,0,0,7,9,plan)&&plan==old);

    struct Heat {int damage;uint8_t state;bool smallProjectile;};
    const Heat heats[]={{40,1,false},{60,2,false},{80,2,false},{80,3,false},{100,3,false},{120,3,false},{140,3,false},
        {30,1,true},{55,2,true},{70,3,true},{95,3,true}};
    for(const auto& h:heats)
    {
        CHECK(FusionFireProjectileMetadataValid(0,h.damage,0,h.state,7,9,PROJECTILE_FIREBALL,h.smallProjectile));
        CHECK(!FusionFireProjectileMetadataValid(0,h.damage+1,0,h.state,7,9,PROJECTILE_FIREBALL,h.smallProjectile));
        CHECK(!FusionFireProjectileMetadataValid(0,h.damage,0,h.state,7,0,PROJECTILE_FIREBALL,h.smallProjectile));
    }
    CHECK(!FusionFireProjectileMetadataValid(0,30,0,1,7,9,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(0,40,0,1,7,9,PROJECTILE_FIREBALL,true));
    CHECK(FusionFireProjectileMetadataValid(1,120,0,1,7,9,PROJECTILE_FIREBALL));
    CHECK(FusionFireProjectileMetadataValid(10,40,0,1,7,9,PROJECTILE_FIREBALL));
    CHECK(FusionFireProjectileMetadataValid(8,400,25,1,0,0,PROJECTILE_PEA));
    CHECK(!FusionFireProjectileMetadataValid(8,400,5,1,0,0,PROJECTILE_PEA));
    CHECK(!FusionFireProjectileMetadataValid(8,400,25,1,7,9,PROJECTILE_PEA));
    int damage=-7;
    CHECK(TryGetFusionFireSplashDamage(1,120,4,damage)&&damage==120);
    CHECK(TryGetFusionFireSplashDamage(0,40,4,damage)&&damage==10);
    CHECK(TryGetFusionFireSplashDamage(0,30,1,damage)&&damage==10);
    CHECK(TryGetFusionFireSplashDamage(0,95,1,damage)&&damage==31);

    for(uint8_t phase:{0,3,4})
        CHECK(FusionFireActionProjectileMetadataValid(12,5,0,phase,0,PROJECTILE_PEA,phase?MOTION_THREEPEATER:MOTION_STRAIGHT));
    CHECK(!FusionFireActionProjectileMetadataValid(12,20,0,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionFireActionProjectileMetadataValid(28,40,0,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionFireActionProjectileMetadataValid(28,40,0,1,0,PROJECTILE_PEA,MOTION_LOBBED));
    CHECK(!FusionFireActionProjectileMetadataValid(28,40,0,1,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionFireActionProjectileMetadataValid(28,40,0,2,0,PROJECTILE_PEA,MOTION_LOBBED));
    CHECK(!FusionFireActionProjectileMetadataValid(28,40,0,0,1,PROJECTILE_PEA,MOTION_STRAIGHT));

    int after=99;bool attempt=false;
    CHECK(TryAdvanceFusionTorchCount(1059,49,false,after,attempt)&&after==50&&!attempt);
    CHECK(TryAdvanceFusionTorchCount(1059,50,false,after,attempt)&&after==51&&attempt);
    CHECK(TryAdvanceFusionTorchCount(1059,51,true,after,attempt)&&after==0&&attempt);
    CHECK(TryAdvanceFusionTorchCount(1061,99,false,after,attempt)&&after==100&&!attempt);
    CHECK(TryAdvanceFusionTorchCount(1061,100,false,after,attempt)&&after==0&&attempt);
    after=99;attempt=true;
    CHECK(!TryAdvanceFusionTorchCount(1052,0,false,after,attempt)&&after==99&&attempt);
    CHECK(!TryAdvanceFusionTorchCount(1059,(std::numeric_limits<int>::max)(),false,after,attempt)&&after==99&&attempt);
    CHECK(FusionFireLightCellAffected(4,2,5,3));
    CHECK(!FusionFireLightCellAffected(4,2,6,2));
    CHECK(!FusionFireLightCellAffected(-1,2,0,2));
    double moved=123;
    CHECK(TryGetFusionFireKnockBack(6.8,9.2,.8,moved)&&std::abs(moved-7.0)<1e-9);
    CHECK(TryGetFusionFireKnockBack(7.1,9.2,.8,moved)&&moved==7.1);
    CHECK(TryGetFusionFireKnockBack(5,9.2,.2,moved)&&std::abs(moved-5.2)<1e-9);
    CHECK(!TryGetFusionFireKnockBack(5,9.2,-1,moved)&&std::abs(moved-5.2)<1e-9);
    CHECK(FusionFireCircleTouchesRect(0,0,1.5,79.999,0,0,0));
    CHECK(!FusionFireCircleTouchesRect(0,0,1.5,80.001,0,0,0));
    CHECK(FusionFireCircleTouchesRect(0,0,.5,0,0,0,0));
    CHECK(FusionFireCherryNutHealOverlapCount(0,-50,2,0,0,2,1.5)==2);
    CHECK(FusionFireCherryNutHealOverlapCount(-80,-50,2,0,0,2,1.0)==1);
    CHECK(FusionFireCherryNutHealOverlapCount(-200,-50,2,0,0,2,1.5)==0);
    CHECK(FusionFireCherryNutHealOverlapCount(0,-50,0,0,0,2,1.5)==0);
    CHECK(FusionFireCherryNutHealOverlapCount(0,-50,2,0,0,2,0)==0);
    int ticks=-1;
    CHECK(TryGetFusionFireActionTicks(static_cast<double>(.48333332f),1,ticks)&&ticks==49);
    CHECK(TryGetFusionFireActionTicks(static_cast<double>(.6666667f)/static_cast<double>(.8f),1,ticks)&&ticks==84);
    ticks=-1;CHECK(!TryGetFusionFireActionTicks(1,0,ticks)&&ticks==-1);
    CHECK(FusionFireActionMaximumDuration(65535,1)==0);
    CHECK(FusionFireActionMaximumDuration(1080,1)>2.48f);
    float z=-9,v=-9;bool bounce=true;
    CHECK(TryStepFusionSquashProjectile(0,1,0,1,z,v,bounce)&&z==1&&v==1&&!bounce);
    CHECK(TryStepFusionSquashProjectile(1,1,0,1,z,v,bounce)&&z==2&&v==-1&&bounce);
    CHECK(TryStepFusionSquashProjectile(0,2,1,1,z,v,bounce)&&z==2&&v==-3&&bounce);
    CHECK(!TryStepFusionSquashProjectile(0,2,-1,1,z,v,bounce)&&z==2&&v==-3&&bounce);
#undef CHECK
    std::printf("Fusion.FireActionBatch checks=%d failures=%d\n",result.mChecks,result.mFailures);
    return result;
}
