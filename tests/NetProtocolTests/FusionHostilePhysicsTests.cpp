#include "FusionHostilePhysicsTests.h"
#include "../../Lawn/Fusion/FusionHostileProjectileRules.h"
#include "../../Lawn/Fusion/FusionNativeProjectileMetadata.h"
#include <cmath>
#include <cstdio>
#include <limits>

FusionHostilePhysicsTestResult RunFusionHostilePhysicsTests()
{
    FusionHostilePhysicsTestResult result{};using namespace LawnFusion;
#define CHECK(...) do{++result.mChecks;if(!(__VA_ARGS__)){++result.mFailures;std::printf("HostilePhysics:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    const auto almostEqual=[](double a,double b){return std::abs(a-b)<1e-8;};
    // Source AnimShoot3511E0 uses raw Shoot for velocity but spawns one world
    // unit above it; ThrowBulletUpdate424620 applies gravity before movement.
    struct Flight {double pixelsY,originY,velocityY,firstY,middleY,lastY;};
    const Flight flights[]={
        {100./1.67,40.119760479041915,-7.811377245508984,32.396497005988024,-296.04341317365277,248.93113772455092},
        {85./1.45,41.37931034482759,-7.626034482758621,33.839536206896554,-285.6093965517241,250.0053448275862}};
    for(const auto& expected:flights)
    {
        FusionHostilePlanarShot shot;
        CHECK(TryMakeFusionHostileBasketballShot(500,100,100,300,shot,expected.pixelsY));
        CHECK(almostEqual(shot.x,500)&&almostEqual(shot.y,expected.originY)&&almostEqual(shot.velocityX,-2)&&almostEqual(shot.velocityY,expected.velocityY));
        for(int tick=1;tick<=200;++tick)
        {
            CHECK(AdvanceFusionHostilePlanarShot(shot)&&FusionHostilePlanarShotValid(shot));
            if(tick==1)CHECK(almostEqual(shot.x,498)&&almostEqual(shot.y,expected.firstY));
            if(tick==100)CHECK(almostEqual(shot.x,300)&&almostEqual(shot.y,expected.middleY)&&almostEqual(shot.velocityY,1));
            if(tick==200)CHECK(almostEqual(shot.x,100)&&almostEqual(shot.y,expected.lastY));
        }
    }
    // Actual enabled prefab root boxes, including .4 root scale and Y inversion.
    struct Box {uint16_t kind;double x,y,width,height;};
    const Box boxes[]={
        {58,478.22206385066664,85.97448790419162,43.04015573333333,31.460756886227546},
        {59,476.203303488,81.4413348502994,47.44473173333333,40.73311377245509}};
    for(const auto& expected:boxes)
    {
        FusionRecipientRect rect;
        CHECK(TryMakeFusionHostileProjectileRect(expected.kind,500,100,100./1.67,rect));
        CHECK(almostEqual(rect.x,expected.x)&&almostEqual(rect.y,expected.y)&&almostEqual(rect.width,expected.width)&&almostEqual(rect.height,expected.height));
    }
    FusionRecipientRect poolBox;
    CHECK(TryMakeFusionHostileProjectileRect(58,500,100,85./1.45,poolBox));
    CHECK(almostEqual(poolBox.x,478.22206385066664)&&almostEqual(poolBox.y,86.26950729655172)&&
        almostEqual(poolBox.width,43.04015573333333)&&almostEqual(poolBox.height,30.798996137931034));
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(int invalid=0;invalid<8;++invalid)
    {
        FusionHostilePlanarShot output{11,22,33,44,55};
        CHECK(!TryMakeFusionHostileBasketballShot(invalid==0?nan:500,invalid==1?inf:100,
            invalid==2?inf:100,invalid==3?nan:300,output,invalid==4?0:invalid==5?-1:invalid==6?inf:invalid==7?nan:100./1.67));
        CHECK(output.x==11&&output.y==22&&output.velocityX==33&&output.velocityY==44&&output.gravityY==55);
    }
    for(int invalid=0;invalid<7;++invalid)
    {
        FusionRecipientRect output{11,22,33,44};
        CHECK(!TryMakeFusionHostileProjectileRect(invalid==0?57:58,invalid==1?nan:500,invalid==2?inf:100,
            invalid==3?0:invalid==4?-1:invalid==5?nan:invalid==6?inf:100./1.67,output));
        CHECK(output.x==11&&output.y==22&&output.width==33&&output.height==44);
    }
    FusionHostilePlanarShot invalid{11,22,33,44,inf};
    CHECK(!FusionHostilePlanarShotValid(invalid)&&!AdvanceFusionHostilePlanarShot(invalid));
    CHECK(invalid.x==11&&invalid.y==22&&invalid.velocityX==33&&invalid.velocityY==44&&invalid.gravityY==inf);
    // Source104 shot tuple is distinct from planar58/way13 and friendly3.
    for(int damage:{300,1000,2000})
        CHECK(FusionSuperCherryProjectileMetadataValid(3,damage,0,false,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionSuperCherryProjectileMetadataValid(3,300,9,true,PROJECTILE_PEA,MOTION_BACKWARDS));
    CHECK(!FusionSuperCherryProjectileMetadataValid(3,1000,9,true,PROJECTILE_PEA,MOTION_BACKWARDS));
    CHECK(FusionSuperCherryProjectileMetadataValid(3,300,0,true,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionSuperCherryProjectileMetadataValid(3,300,9,false,PROJECTILE_PEA,MOTION_BACKWARDS));
    CHECK(!FusionSuperCherryProjectileMetadataValid(3,300,9,true,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionSuperCherryProjectileMetadataValid(58,300,9,true,PROJECTILE_PEA,MOTION_BACKWARDS));
    NativeProjectileData cherry;cherry.kind=3;cherry.damage=300;cherry.specialType=9;cherry.isZombieBullet=true;
    CHECK(NativeProjectileMetadataMotion(cherry)==MOTION_BACKWARDS&&NativeProjectileMetadataValid(cherry,PROJECTILE_PEA,MOTION_BACKWARDS));
    auto pending=cherry;pending.hitLatched=true;
    CHECK(NativeProjectileMetadataValid(pending,PROJECTILE_PEA,MOTION_BACKWARDS));
    pending.isZombieBullet=false;pending.specialType=0;
    CHECK(!NativeProjectileMetadataValid(pending,PROJECTILE_PEA,MOTION_STRAIGHT));
    auto attracted=cherry;attracted.specialType=4;attracted.isZombieBullet=false;attracted.goldAttracted=true;
    CHECK(NativeProjectileMetadataValid(attracted,PROJECTILE_PEA,MOTION_THREEPEATER)); // Existing friendly GoldMagnet wrapper remains admitted.
    attracted.isZombieBullet=true;CHECK(NativeProjectileMetadataValid(attracted,PROJECTILE_PEA,MOTION_THREEPEATER));
    attracted.damage=299;CHECK(!NativeProjectileMetadataValid(attracted,PROJECTILE_PEA,MOTION_THREEPEATER));
    for(int extra=0;extra<10;++extra)
    {
        auto altered=cherry;
        switch(extra){case 0:altered.sunOnHit=5;break;case 1:altered.fireState=1;break;case 2:altered.lastTorchId=1;break;
        case 3:altered.lastTorchToken=1;break;case 4:altered.effectVariant=1;break;case 5:altered.melonEnhanced=true;break;
        case 6:altered.hitLatched=true;altered.specialType=0;break;case 7:altered.umbrellaed=true;break;case 8:altered.goldAttracted=true;break;case 9:altered.damage=299;break;}
        CHECK(!NativeProjectileMetadataValid(altered,PROJECTILE_PEA,MOTION_BACKWARDS));
    }
    // CheckPlant tests source isShort; a visible upper-row body is not an
    // interchangeable substitute for the exact source short/cell policy.
    CHECK(FusionSuperCherryPlantCollides(0,5,5,true));
    CHECK(!FusionSuperCherryPlantCollides(0,5,4,true)&&!FusionSuperCherryPlantCollides(0,6,6,true));
    CHECK(!FusionSuperCherryPlantCollides(254,0,0,true)&&!FusionSuperCherryPlantCollides(0,0,0,false));
    CHECK(!FusionSuperCherryPlantCollides(6,0,0,true)&&!FusionSuperCherryPlantCollides(65535,0,0,true));
    CHECK(FusionSuperCherryPlantShort(4)&&FusionSuperCherryPlantShort(12)&&FusionSuperCherryPlantShort(1170));
    CHECK(FusionSuperCherryPlantSingleTarget(922)&&FusionSuperCherryPlantSingleTarget(255)&&FusionSuperCherryPlantSingleTarget(3));
    CHECK(FusionSuperCherryPlantSingleTarget(243)&&FusionSuperCherryPlantSingleTarget(931)&&!FusionSuperCherryPlantSingleTarget(906));
    int reflected=73;
    CHECK(!TryPlanFusionSuperCherryRebound(931,false,false,reflected)&&reflected==73);
    CHECK(!TryPlanFusionSuperCherryRebound(30,true,false,reflected)&&reflected==73);
    CHECK(TryPlanFusionSuperCherryRebound(931,true,false,reflected)&&reflected==1000);
    CHECK(TryPlanFusionSuperCherryRebound(1131,true,true,reflected)&&reflected==2000);
    int cardinal=0;for(int col=-1;col<=1;++col)for(int row=-1;row<=1;++row)if(FusionSuperCherryCollateralCell(col,row))++cardinal;
    CHECK(cardinal==4&&!FusionSuperCherryCollateralCell(2,0)&&!FusionSuperCherryCollateralCell(0,0));
    auto plan=PlanFusionSuperCherryPlantHit(0,false,false);
    CHECK(plan.effect==FusionSuperCherryPlantEffect::Damage&&plan.amount==1000);
    for(uint16_t kind:{uint16_t(935),uint16_t(1028),uint16_t(1129),uint16_t(1159),uint16_t(1160)})
    {plan=PlanFusionSuperCherryPlantHit(kind,false,false);CHECK(plan.effect==FusionSuperCherryPlantEffect::Damage&&plan.amount==500);}
    for(uint16_t kind:{uint16_t(903),uint16_t(922),uint16_t(1003),uint16_t(1164),uint16_t(1169)})
    {plan=PlanFusionSuperCherryPlantHit(kind,true,false);CHECK(plan.effect==FusionSuperCherryPlantEffect::Heal&&plan.amount==200);}
    for(uint16_t kind:{uint16_t(12),uint16_t(17),uint16_t(921),uint16_t(1095),uint16_t(241),uint16_t(65535)})
        CHECK(PlanFusionSuperCherryPlantHit(kind,false,false).effect==FusionSuperCherryPlantEffect::Ignore);
    CHECK(PlanFusionSuperCherryPlantHit(0,true,false).effect==FusionSuperCherryPlantEffect::Ignore);
    plan=PlanFusionSuperCherryPlantHit(911,true,true); // Graph half branch precedes cover immunity.
    CHECK(plan.effect==FusionSuperCherryPlantEffect::Damage&&plan.amount==500);
#undef CHECK
    return result;
}
