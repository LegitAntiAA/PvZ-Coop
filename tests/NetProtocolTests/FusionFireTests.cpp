#include "FusionFireTests.h"
#include "../../Lawn/Fusion/FusionFireRules.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace
{
FusionFireTestResult result;
void Check(bool value,int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Fire check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
}

FusionFireTestResult RunFusionFireTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.SourceTorchwoodAndWarm\n");
    using Action=FusionTorchwoodAction;
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_STRAIGHT,0,0,9,0x10001)==Action::YellowPea);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_BACKWARDS,0,0,9,0x10001)==Action::YellowPea);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_THREEPEATER,0,0,9,0x10001,3)==Action::YellowPea);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_THREEPEATER,0,0,9,0x10001,4)==Action::YellowPea);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_THREEPEATER,0,0,9,0x10001)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_THREEPEATER,0,0,9,0x10001,5)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(15,0,true,false,MOTION_THREEPEATER,0,0,9,0x10001,3)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,0,false,false,MOTION_THREEPEATER,0,0,9,0x10001,3)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(15,0,true,false,MOTION_STRAIGHT,0,0,9,0x10001)==Action::MeltSnow);
    for (uint16_t kind:{uint16_t(1),uint16_t(2),uint16_t(7),uint16_t(8),uint16_t(9),uint16_t(10),uint16_t(18)})
        CHECK(ResolveFusionTorchwoodAction(kind,0,true,false,MOTION_STRAIGHT,0,0,9,0x10001)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,1,true,false,MOTION_STRAIGHT,0,0,9,0x10001)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_STRAIGHT,9,0x10001,9,0x10001)==Action::None); // Same actor moved.
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_STRAIGHT,9,0x10001,9,0x20001)==Action::YellowPea); // Recycled NetId, new actor.
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_STRAIGHT,9,0x10001,10,0x10002)==Action::YellowPea);
    CHECK(ResolveFusionTorchwoodAction(0,0,false,false,MOTION_STRAIGHT,0,0,9,0x10001)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,true,MOTION_STRAIGHT,0,0,9,0x10001)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_LOBBED,0,0,9,0x10001)==Action::None);
    CHECK(ResolveFusionTorchwoodAction(0,0,true,false,MOTION_STRAIGHT,9,0,10,0x10001)==Action::None);

    // KelpTorch470180: actual way2 contact is row-independent only for1056.
    FusionTorchConversion kelp;
    for(uint16_t kind:{uint16_t(0),uint16_t(10),uint16_t(15)})
    {
        CHECK(TryResolveFusionTorchConversion(1056,kind,20,0,0,2,false,false,MOTION_STAR,0,0,9,0x10001,kelp));
        CHECK(kind==15?(kelp.kind==0&&kelp.replace&&!kelp.fireState):
            (kelp.kind==kind&&!kelp.replace&&kelp.damage==40&&kelp.fireState==1));
    }
    const auto kelpBefore=kelp;
    CHECK(!TryResolveFusionTorchConversion(18,0,20,0,0,2,false,false,MOTION_STAR,0,0,9,0x10001,kelp)&&kelp==kelpBefore);
    CHECK(!TryResolveFusionTorchConversion(1056,0,20,0,0,0,false,false,MOTION_STRAIGHT,0,0,9,0x10001,kelp)&&kelp==kelpBefore);
    CHECK(!TryResolveFusionTorchConversion(1056,0,20,0,0,2,false,true,MOTION_STAR,0,0,9,0x10001,kelp)&&kelp==kelpBefore);
    CHECK(!TryResolveFusionTorchConversion(1056,0,20,0,0,2,false,false,MOTION_STAR,9,0x10001,9,0x10001,kelp)&&kelp==kelpBefore);
    CHECK(!TryResolveFusionTorchConversion(1056,0,20,0,0,0,true,false,MOTION_STAR,0,0,9,0x10001,kelp)&&kelp==kelpBefore);

    int damage=99;
    CHECK(TryGetFusionYellowFireDamage(20,0,damage) && damage==40);
    CHECK(TryGetFusionYellowFireDamage(30,0,damage) && damage==50); // Addition, not doubling; outside bounded metadata.
    CHECK(!FusionFireProjectileMetadataValid(0,50,0,1,9,0x10001,PROJECTILE_FIREBALL));
    damage=99;
    CHECK(!TryGetFusionYellowFireDamage(40,1,damage) && damage==99);
    CHECK(!TryGetFusionYellowFireDamage((std::numeric_limits<int>::max)()-19,0,damage) && damage==99);
    CHECK(TryGetFusionYellowFireDamage((std::numeric_limits<int>::max)()-20,0,damage) && damage==(std::numeric_limits<int>::max)());

    struct Ordinary { uint16_t kind; int damage,sun; ProjectileType type; };
    for (const auto value:{Ordinary{0,20,0,PROJECTILE_PEA},{1,60,0,PROJECTILE_PEA},{2,20,0,PROJECTILE_PEA},
        {8,100,0,PROJECTILE_PEA},{8,100,5,PROJECTILE_PEA},{9,20,0,PROJECTILE_PUFF},
        {10,20,0,PROJECTILE_PEA},{15,20,0,PROJECTILE_PEA},{18,20,0,PROJECTILE_PEA}})
    {
        CHECK(FusionFireProjectileMetadataValid(value.kind,value.damage,value.sun,0,0,0,value.type));
        CHECK(!FusionFireProjectileMetadataValid(value.kind,value.damage+1,value.sun,0,0,0,value.type));
        CHECK(!FusionFireProjectileMetadataValid(value.kind,value.damage,value.sun,0,1,0,value.type));
        if (value.kind!=0&&value.kind!=15) CHECK(!FusionFireProjectileMetadataValid(value.kind,value.damage,value.sun,0,9,0x10001,value.type));
        else CHECK(FusionFireProjectileMetadataValid(value.kind,value.damage,value.sun,0,9,0x10001,value.type));
    }
    CHECK(FusionFireProjectileMetadataValid(0,20,0,0,9,0x10001,PROJECTILE_PEA)); // Fresh melted Pea.
    CHECK(FusionFireProjectileMetadataValid(0,40,0,1,9,0x10001,PROJECTILE_FIREBALL));
    CHECK(FusionFireProjectileMetadataValid(0,40,0,1,0,0,PROJECTILE_FIREBALL)); // Hot Split return cleared stamp.
    CHECK(!FusionFireProjectileMetadataValid(0,40,0,1,0,1,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(0,40,0,1,9,0,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(0,40,0,1,9,0x10001,PROJECTILE_PEA));
    CHECK(!FusionFireProjectileMetadataValid(0,20,0,1,9,0x10001,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(0,40,5,1,9,0x10001,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(1,40,0,1,9,0x10001,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(0,40,0,2,9,0x10001,PROJECTILE_FIREBALL));
    CHECK(!FusionFireProjectileMetadataValid(65535,20,0,0,0,0,PROJECTILE_PEA));

    for (const auto vector:{std::pair{1,13},std::pair{2,13},std::pair{3,13},std::pair{4,10},std::pair{40,1},std::pair{100,1}})
        CHECK(TryGetFusionFireSplashDamage(40,vector.first,damage) && damage==vector.second);
    damage=99;
    CHECK(!TryGetFusionFireSplashDamage(40,0,damage) && damage==99);
    CHECK(!TryGetFusionFireSplashDamage(-1,1,damage) && damage==99);
    CHECK(TryGetFusionFireSplashDamage(1,100,damage) && damage==0); // Exact source cap can be0 for tiny damage.
    CHECK(TryGetFusionFireSplashDamage((std::numeric_limits<int>::max)(),1,damage) && damage==715827904);
    for (int status:{7,10,12,20,22,23,24}) CHECK(!FusionFireAllowsSputter(status,false));
    CHECK(FusionFireAllowsSputter(0,false));
    CHECK(FusionFireAllowsSputter(1,false)); // Primary lethal damage is not itself a splash exclusion.
    CHECK(!FusionFireAllowsSputter(0,true)); // Evaluate live Door/Ladder AFTER primary armor damage.

    const double radiusX=72.0/1.35,radiusY=100.0/1.67;
    CHECK(FusionFireAreaTouchesRect(0,0,{radiusX,0,0,0}));
    CHECK(!FusionFireAreaTouchesRect(0,0,{radiusX+0.000001,0,0,0}));
    CHECK(FusionFireAreaTouchesRect(0,0,{0,radiusY,0,0}));
    CHECK(!FusionFireAreaTouchesRect(0,0,{0,radiusY+0.000001,0,0}));
    CHECK(FusionFireAreaTouchesRect(0,0,{radiusX-1,-10,20,20})); // Collider intersects though its center is outside.
    CHECK(!FusionFireAreaTouchesRect(0,0,{radiusX,radiusY,1,1}));
    CHECK(!FusionFireAreaTouchesRect(0,0,{0,0,-1,1}));
    CHECK(!FusionFireAreaTouchesRect(std::numeric_limits<double>::quiet_NaN(),0,{0,0,1,1}));
    FusionColdState warmed;
    for (int threshold:{100,200,300,400})
    {
        CHECK(TryWarmFusionColdState({900,threshold,1400,400},warmed));
        CHECK(warmed.level==0 && warmed.threshold==threshold && warmed.chilledTicks==0 && warmed.frozenTicks==0);
    }
    warmed={7,300,8,9};
    CHECK(!TryWarmFusionColdState({0,150,0,0},warmed) && warmed.level==7 && warmed.threshold==300);
    std::printf("[ %s ] Fusion.SourceTorchwoodAndWarm checks=%d failures=%d\n",
        result.mFailures ? "  FAILED  " : "       OK ",result.mChecks,result.mFailures);
    return result;
}
