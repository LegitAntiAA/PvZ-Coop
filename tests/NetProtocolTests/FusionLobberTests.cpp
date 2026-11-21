#include "FusionLobberTests.h"
#include "../../Lawn/Fusion/FusionLobberRules.h"
#include "../../Lawn/Fusion/FusionColdRules.h"
#include "../../Lawn/Fusion/FusionCatalog.h"
#include "../../Lawn/Fusion/FusionRecipe.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Fusion/FusionButterRules.h"
#include "../../Lawn/Fusion/FusionRollingProjectileRules.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
namespace
{
FusionLobberTestResult result;
void Check(bool value, int line)
{ ++result.mChecks; if (!value) { ++result.mFailures; std::printf("    Lobber check failed line=%d\n",line); } }
#define CHECK(value) Check(!!(value),__LINE__)
bool Near(double a, double b, double tolerance = 1e-7) { return std::abs(a-b) <= tolerance; }
}
FusionLobberTestResult RunFusionLobberTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.NativeLobberPredictionPhysicsAndImpactPolicy\n");
    struct Profile { uint16_t kind, payload; ProjectileType type; int damage; };
    for (const auto row : {Profile{26,52,PROJECTILE_CABBAGE,40},Profile{32,59,PROJECTILE_MELON,80},Profile{1116,60,PROJECTILE_WINTERMELON,80}})
    {
        const auto* profile = FindFusionLobberProfile(row.kind);
        CHECK(profile && profile->projectileKind == row.payload && profile->nativeType == row.type && profile->damage == row.damage);
        CHECK(FindFusionLobberProjectileProfile(row.payload) == profile);
        const auto* source = FusionFindPlant(row.kind);
        CHECK(source && source->hasStats && source->damage == row.damage && source->maxHealth == 300 && source->attackIntervalSeconds == 3);
        CHECK(FusionLobberProjectileMetadataValid(row.payload,row.damage,0,row.type));
        CHECK(!FusionLobberProjectileMetadataValid(row.payload,row.damage+1,0,row.type));
        CHECK(!FusionLobberProjectileMetadataValid(row.payload,row.damage,5,row.type));
        CHECK(!FusionLobberProjectileMetadataValid(row.payload,row.damage,0,PROJECTILE_PEA));
        int ticks = 77;
        CHECK(TryGetFusionLobberCycleTicks(row.kind,-.1f,ticks) && ticks == 291);
        CHECK(TryGetFusionLobberCycleTicks(row.kind,0,ticks) && ticks == 301);
        CHECK(TryGetFusionLobberCycleTicks(row.kind,.1f,ticks) && ticks == 310); // Source float3.1 rounds below exact3.1.
        ticks = 77; CHECK(!TryGetFusionLobberCycleTicks(row.kind,.1001f,ticks) && ticks == 77);
    }
    const auto* forward = FindFusionRecipe(10,32); const auto* reverse = FindFusionRecipe(32,10);
    CHECK(forward && reverse && forward->result == 1116 && reverse->result == 1116);
    CHECK(!FindFusionLobberProfile(27) && !FindFusionLobberProjectileProfile(58));
    CHECK(!FusionLobberProjectileMetadataValid(0,20,0,PROJECTILE_PEA));
    int unchanged = 77;
    CHECK(!TryGetFusionLobberCycleTicks(0,0,unchanged) && unchanged == 77);
    CHECK(!TryGetFusionLobberCycleTicks(26,(std::numeric_limits<float>::quiet_NaN)(),unchanged) && unchanged == 77);

    FusionLobberLaunch launch;
    const FusionLobberPoint a{100,120}, b0{500,180}, b1{490,180};
    CHECK(TryMakeFusionLobberLaunch(a,b0,b1,.4,nullptr,launch));
    CHECK(Near(launch.velocityX,2.35) && Near(launch.accelerationY,.08811377245508982));
    CHECK(Near(launch.velocityY,-6.208532934131736));
    float x = 100, height = 0, velocity = static_cast<float>(launch.velocityY);
    for (int tick = 0; tick < 150; ++tick)
        if (!AdvanceFusionLobberMotion(static_cast<float>(launch.velocityX),static_cast<float>(launch.accelerationY),x,height,velocity))
        { CHECK(false); break; }
    CHECK(Near(x,452.5,.01) && Near(120+height,180,.01)); // Predict the measured leftward velocity, not a stationary target.
    FusionLobberLaunch lost;
    CHECK(TryMakeFusionLobberLaunch(a,b0,b0,.4,nullptr,lost) && Near(lost.velocityX,2.6666666666666665));
    FusionLobberRect close{110,150,40,90}; FusionLobberLaunch nearLaunch;
    CHECK(TryMakeFusionLobberLaunch(a,b0,b1,.4,&close,nearLaunch));
    CHECK(Near(nearLaunch.spawn.x,110) && Near(nearLaunch.spawn.y,155.9880239520958));
    CHECK(nearLaunch.velocityX == launch.velocityX && nearLaunch.velocityY == launch.velocityY); // Near-spawn is AFTER solving from A.
    close.x = kFusionLobberPixelsX;
    CHECK(TryMakeFusionLobberLaunch({0,120},b0,b1,.4,&close,nearLaunch) && nearLaunch.spawn.x == 0); // Strict distance1, no override at equality.
    nearLaunch.velocityX = 77;
    CHECK(!TryMakeFusionLobberLaunch(a,b0,b1,0,nullptr,nearLaunch) && nearLaunch.velocityX == 77);
    CHECK(!TryMakeFusionLobberLaunch(a,b0,b1,-.1,nullptr,nearLaunch) && nearLaunch.velocityX == 77);
    CHECK(!TryMakeFusionLobberLaunch({(std::numeric_limits<double>::quiet_NaN)(),1},b0,b1,.4,nullptr,nearLaunch) && nearLaunch.velocityX == 77);
    x = 1; height = 2; velocity = 3;
    CHECK(!AdvanceFusionLobberMotion(4,-1,x,height,velocity) && x == 1 && height == 2 && velocity == 3);
    const float plane = static_cast<float>(-.2*kFusionLobberPixelsY);
    CHECK(!FusionLobberGroundHit(plane,1));
    CHECK(FusionLobberGroundHit(std::nextafter(plane,(std::numeric_limits<float>::infinity)()),1));
    CHECK(!FusionLobberGroundHit(0,0) && !FusionLobberGroundHit(0,-1));
    CHECK(!FusionLobberExpired(-10,2000,-10,10.2) && !FusionLobberExpired(10.2,2000,-10,10.2));
    CHECK(FusionLobberExpired(-10.00001,1,-10,10.2) && FusionLobberExpired(10.20001,1,-10,10.2));
    CHECK(FusionLobberExpired(0,2001,-10,10.2));
    CHECK(FusionLobberCanAcquire(0,true,false,false,0,1,9.2));
    CHECK(!FusionLobberCanAcquire(0,true,false,false,0,0,9.2) && !FusionLobberCanAcquire(0,true,false,false,0,9.2,9.2));
    for (int status : {3,9,10,12,13,16,20,21,22,23,24}) CHECK(!FusionLobberCanAcquire(status,true,false,false,0,1,9.2));
    CHECK(!FusionLobberCanAcquire(0,false,false,false,0,1,9.2) && !FusionLobberCanAcquire(0,true,true,false,0,1,9.2));
    CHECK(!FusionLobberCanAcquire(0,true,false,true,0,1,9.2));
    CHECK(FusionLobberSplashCanAffect(0,-1,false) && FusionLobberSplashCanAffect(0,1,false));
    CHECK(!FusionLobberSplashCanAffect(0,2,false) && !FusionLobberSplashCanAffect(0,0,true));
    CHECK(FusionLobberAreaTouchesRect(0,0,{80,0,0,0}) && !FusionLobberAreaTouchesRect(0,0,{80.001,0,0,0}));
    const auto area = FusionLobberSplashCenter(200,200);
    CHECK(area.x == 200 && Near(area.y,229.94011976047904)); // Source rootY-.5, not a collider pivot.
    const FusionLobberRect below{200,200+1.6*kFusionLobberPixelsY,0,0};
    const FusionLobberRect above{200,200-1.1*kFusionLobberPixelsY,0,0};
    CHECK(!FusionLobberAreaTouchesRect(200,200,below) && FusionLobberAreaTouchesRect(area.x,area.y,below));
    CHECK(FusionLobberAreaTouchesRect(200,200,above) && !FusionLobberAreaTouchesRect(area.x,area.y,above));
    const FusionLobberRect lowerBody{180,200+1.7*kFusionLobberPixelsY,40,115};
    CHECK(!FusionLobberAreaTouchesRect(200,200,lowerBody) && FusionLobberAreaTouchesRect(area.x,area.y,lowerBody));
    FusionLobberRect bounds;
    CHECK(TryMakeFusionLobberRect(52,100,100,bounds) && Near(bounds.width,43.04015573333334));
    CHECK(bounds.x < 100 && bounds.y < 100 && bounds.width > 40); // Source collider stays at root; area offset is separate.
    bounds.width = 77; CHECK(!TryMakeFusionLobberRect(58,100,100,bounds) && bounds.width == 77);
    struct Splash { int n, expected; };
    for (const auto row : {Splash{1,26},Splash{2,26},Splash{24,26},Splash{25,25},Splash{1000,1}})
    { int damage = 0; CHECK(TryGetFusionLobberSplashDamage(80,row.n,damage) && damage == row.expected); }
    unchanged = 77; CHECK(!TryGetFusionLobberSplashDamage(80,0,unchanged) && unchanged == 77);

    // Different source ordering: primary only sees PRE-existing freeze, then
    // adds50; a secondary adds5 first and can freeze/multiply the current hit.
    FusionColdState initial{50,100,0,0}, slowed, frozen;
    int damage = 0;
    CHECK(TryGetFusionLobberColdDamage(80,initial.frozenTicks>0,damage) && damage == 80);
    CHECK(TrySetFusionColdSlow(initial,slowed) && TryAddFusionColdLevels(slowed,50,frozen));
    CHECK(frozen.level == 0 && frozen.threshold == 200 && frozen.frozenTicks == 400 && frozen.chilledTicks == 1400);
    initial = {95,100,0,0};
    CHECK(TryAddFusionColdLevels(initial,5,frozen) && TryGetFusionLobberColdDamage(26,frozen.frozenTicks>0,damage) && damage == 104);
    CHECK(TrySetFusionColdSlow(frozen,slowed) && slowed.chilledTicks == 1000 && slowed.frozenTicks == 400);
    CHECK(TryGetFusionLobberColdDamage(80,false,damage) && damage == 80); // Chilled alone never multiplies.
    CHECK(TryGetFusionLobberColdDamage(80,true,damage) && damage == 320);
    CHECK(FusionColdAppliesAfterShield(60,false) && FusionColdAppliesAfterShield(60,true));
    unchanged = 77; CHECK(!TryGetFusionLobberColdDamage((std::numeric_limits<int>::max)(),true,unchanged) && unchanged == 77);

    // Audited Corn alternatives use one RNG result only after target admission.
    for (uint16_t kind : {uint16_t(28),uint16_t(1113),uint16_t(1121)})
    {
        const auto* normal = FindFusionLobberProfile(kind); const auto* alternate = FindFusionLobberProfile(kind,true);
        CHECK(normal && alternate && !normal->alternate && alternate->alternate && normal->kind == alternate->kind);
        CHECK(FusionLobberCornDenominator(kind) == 4);
        bool chosen = false; CHECK(TryChooseFusionLobberAlternate(kind,0,chosen) && chosen);
        for (int draw : {1,2,3}) CHECK(TryChooseFusionLobberAlternate(kind,draw,chosen) && !chosen);
        chosen = true; CHECK(!TryChooseFusionLobberAlternate(kind,-1,chosen) && chosen);
        CHECK(!TryChooseFusionLobberAlternate(kind,4,chosen) && chosen);
    }
    bool chosen = true; CHECK(!TryChooseFusionLobberAlternate(26,0,chosen) && chosen);
    CHECK(FindFusionLobberProfile(28,true)->projectileKind == 55 && FindFusionLobberProfile(1113,true)->damage == 70 && FindFusionLobberProfile(1121,true)->damage == 120);
    CHECK(!FindFusionLobberProfile(26,true));
    const auto* cornNormal = FindFusionShooterTimingProfile(1113,"shoot");
    const auto* cornButter = FindFusionShooterTimingProfile(1113,"shoot2");
    CHECK(cornNormal && cornButter && cornNormal->speedParameterActive && !cornButter->speedParameterActive);
    if (cornNormal && cornButter)
    {
        int ticks = 0;
        CHECK(TryGetFusionShooterShotCounter(*cornNormal,.9f,ticks) && ticks == 37);
        CHECK(TryGetFusionShooterShotCounter(*cornNormal,1.1f,ticks) && ticks == 30);
        CHECK(TryGetFusionShooterShotCounter(*cornButter,.9f,ticks) && ticks == 33);
        CHECK(TryGetFusionShooterShotCounter(*cornButter,1.1f,ticks) && ticks == 33);
        CHECK(FusionShooterDuration(*cornButter,.9f) == 1 && FusionShooterDuration(*cornButter,1.1f) == 1);
    }
    CHECK(TryGetFusionLobberCycleTicks(1111,-.1f,unchanged) && unchanged == 491);
    CHECK(TryGetFusionLobberCycleTicks(1111,0,unchanged) && unchanged == 501);
    CHECK(TryGetFusionLobberCycleTicks(1111,.1f,unchanged) && unchanged == 510);
    CHECK(FindFusionLobberProfile(1124)->muzzleY == 1.4652); // Distinct from ordinary Melon1.6071.
    for (uint16_t payload : {uint16_t(53),uint16_t(54),uint16_t(55),uint16_t(56),uint16_t(57),uint16_t(66),uint16_t(67),uint16_t(70)})
    {
        const auto* row = FindFusionLobberProjectileProfile(payload); const auto* traits = FindFusionLobberProjectileTraits(payload);
        CHECK(row && traits);
        if (!row || !traits) continue;
        CHECK(FusionLobberProjectileMetadataValid(payload,row->damage,payload==53?5:0,row->nativeType));
        CHECK(!FusionLobberProjectileMetadataValid(payload,row->damage+1,payload==53?5:0,row->nativeType));
        CHECK(!FusionLobberProjectileMetadataValid(payload,row->damage,payload==53?0:5,row->nativeType));
        CHECK(!FusionLobberProjectileMetadataValid(payload,row->damage,payload==53?5:0,PROJECTILE_PEA));
        CHECK(traits->sourceRotation);
    }
    struct Limit { int wallet, expected; };
    for (auto row : {Limit{0,2},Limit{199,2},Limit{200,2},Limit{399,2},Limit{400,3},Limit{800,5},Limit{1000,6},Limit{2147483647,6}})
        CHECK(TryGetFusionLobberPierceLimit(53,row.wallet,unchanged) && unchanged == row.expected);
    unchanged = 77; CHECK(!TryGetFusionLobberPierceLimit(53,-1,unchanged) && unchanged == 77);
    CHECK(!TryGetFusionLobberPierceLimit(58,500,unchanged) && unchanged == 77);
    CHECK(TryGetFusionLobberPierceLimit(56,0,unchanged) && unchanged == 3);
    CHECK(TryGetFusionLobberPierceLimit(57,1000,unchanged) && unchanged == 3);
    CHECK(TryGetFusionLobberPierceLimit(70,1000,unchanged) && unchanged == 2);
    float vx = 4, vy = 8;
    CHECK(TryApplyFusionLobberBounce(56,1,vx,vy) && vx == 2 && vy == -4);
    vx = 4; vy = -8; CHECK(TryApplyFusionLobberBounce(57,2,vx,vy) && vx == 4 && vy == -8); // Rising remains unchanged.
    vx = 4; vy = 8; CHECK(TryApplyFusionLobberBounce(57,3,vx,vy) && vx == 4 && vy == 8); // Third callback precedes retirement without bounce.
    vx = 5; vy = 10; CHECK(TryApplyFusionLobberBounce(53,1,vx,vy) && vx == 2 && vy == -5);
    vx = 5; vy = 10; CHECK(TryApplyFusionLobberBounce(53,3,vx,vy) && vx == 5 && vy == -10);
    vx = 4; vy = 8; CHECK(TryApplyFusionLobberBounce(70,2,vx,vy) && vx == 2 && vy == -4);
    vx = 77; vy = 88; CHECK(!TryApplyFusionLobberBounce(56,4,vx,vy) && vx == 77 && vy == 88);
    CHECK(!TryApplyFusionLobberBounce(54,1,vx,vy) && vx == 77 && vy == 88);
    CHECK(TryMakeFusionLobberRect(54,100,100,bounds) && Near(bounds.width,26.85140906666667));
    CHECK(TryMakeFusionLobberRect(56,100,100,bounds) && Near(bounds.width,36.05546026666667));
    CHECK(TryGetFusionLobberSplashDamage(100,24,unchanged) && unchanged == 33);
    CHECK(TryGetFusionLobberSplashDamage(100,25,unchanged) && unchanged == 32);
    CHECK(TryGetFusionButterSplashTicks(1,unchanged) && unchanged == 400);
    CHECK(TryGetFusionButterSplashTicks(2,unchanged) && unchanged == 200);
    CHECK(TryGetFusionButterSplashTicks(3,unchanged) && unchanged == 100);
    CHECK(TryGetFusionButterSplashTicks(4,unchanged) && unchanged == 100);
    CHECK(TryGetFusionButterSplashTicks(5,unchanged) && unchanged == 50);
    // Same source trajectory/collider/splash projected onto the real Pool
    // row pitch. The default argument retains every accepted Day golden.
    constexpr double poolPixelsY=85.0/1.45;
    FusionLobberLaunch dayProjection,poolProjection;
    CHECK(TryMakeFusionLobberLaunch({0,0},{80,0},{80,0},.4,nullptr,dayProjection));
    CHECK(TryMakeFusionLobberLaunch({0,0},{80,0},{80,0},.4,nullptr,poolProjection,poolPixelsY));
    CHECK(Near(poolProjection.velocityX,dayProjection.velocityX));
    CHECK(Near(poolProjection.velocityY/dayProjection.velocityY,poolPixelsY/kFusionLobberPixelsY));
    CHECK(Near(poolProjection.accelerationY/dayProjection.accelerationY,poolPixelsY/kFusionLobberPixelsY));
    FusionLobberRect dayBox,poolBox;
    CHECK(TryMakeFusionLobberRect(59,100,100,dayBox));
    CHECK(TryMakeFusionLobberRect(59,100,100,poolBox,poolPixelsY));
    CHECK(Near(poolBox.width,dayBox.width)&&Near(poolBox.height/dayBox.height,poolPixelsY/kFusionLobberPixelsY));
    const auto poolSplash=FusionLobberSplashCenter(100,100,poolPixelsY);
    CHECK(Near(poolSplash.y,100+.5*poolPixelsY));
    CHECK(FusionLobberAreaTouchesRect(0,0,{0,1.5*poolPixelsY,0,0},poolPixelsY));
    CHECK(!FusionLobberAreaTouchesRect(0,0,{0,1.5*poolPixelsY+.001,0,0},poolPixelsY));
    CHECK(FusionLobberAreaTouchesRect(0,0,{0,1.5*poolPixelsY+.001,0,0}));
    const float poolGround=float(-.2*poolPixelsY);
    CHECK(!FusionLobberGroundHit(poolGround,1,poolPixelsY));
    CHECK(FusionLobberGroundHit(std::nextafter(poolGround,(std::numeric_limits<float>::infinity)()),1,poolPixelsY));
    poolProjection.velocityX=77;
    CHECK(!TryMakeFusionLobberLaunch({0,0},{80,0},{80,0},.4,nullptr,poolProjection,0)&&poolProjection.velocityX==77);
    poolBox.width=77;CHECK(!TryMakeFusionLobberRect(59,0,0,poolBox,0)&&poolBox.width==77);

    FusionRollingHitHistory history;
    CHECK(RememberFusionRollingHit(history,0x10001,0x20001) && !RememberFusionRollingHit(history,0x10001,0x20001));
    CHECK(RememberFusionRollingHit(history,0x10001,0x30001) && FusionProjectileHitCount(history,0x10001) == 2); // Recycled target slot, fresh generation.
    CHECK(RememberFusionRollingHit(history,0x20001,0x20001) && FusionProjectileHitCount(history,0x20001) == 1); // Recycled projectile slot has separate history.
    std::printf("[ %s ] Fusion.NativeLobberPredictionPhysicsAndImpactPolicy checks=%d failures=%d\n",
        result.mFailures ? " FAILED " : "       OK",result.mChecks,result.mFailures);
    return result;
}
