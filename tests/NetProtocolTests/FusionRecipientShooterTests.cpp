#include "FusionRecipientShooterTests.h"
#include "../../Lawn/Fusion/FusionRecipientShooterRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionRecipientShooterTestResult result;
void Check(bool value,int line)
{++result.mChecks;if(!value){++result.mFailures;std::printf("    RecipientShooter failure line=%d\n",line);}}
#define CHECK(x) Check(!!(x),__LINE__)
bool Near(double a,double b,double tolerance=1e-6){return std::abs(a-b)<tolerance;}
}
FusionRecipientShooterTestResult RunFusionRecipientShooterTests()
{
    using namespace LawnFusion;
    result={};std::printf("[ RUN      ] Fusion.SourceCactusStarRecipients\n");
    const auto* cactus=FindFusionRecipientTimingProfile(21,false);
    const auto* upper=FindFusionRecipientTimingProfile(21,true);
    const auto* star=FindFusionRecipientTimingProfile(23,false);
    CHECK(cactus&&upper&&star);
    CHECK(FindFusionRecipientTimingProfile(21,"shoot1")==cactus);
    CHECK(FindFusionRecipientTimingProfile(21,"shoot2")==upper);
    CHECK(FindFusionRecipientTimingProfile(23,"shoot")==star);
    CHECK(!FindFusionRecipientTimingProfile(23,true));
    CHECK(!FindFusionRecipientTimingProfile(21,"shoot"));
    CHECK(!FindFusionRecipientTimingProfile(1088,false)&&!FindFusionRecipientTimingProfile(1089,false));
    int counter=77;
    CHECK(TryGetFusionShooterShotCounter(*cactus,1,counter)&&counter==33);
    CHECK(TryGetFusionShooterShotCounter(*upper,1,counter)&&counter==28);
    CHECK(TryGetFusionShooterShotCounter(*star,1,counter)&&counter==39);
    CHECK(cactus->eventClipSeconds[0]==0.3166666626930237);
    CHECK(upper->eventClipSeconds[0]==0.2666666805744171);
    CHECK(star->eventClipSeconds[0]==0.4166666567325592);
    CHECK(star->stateSpeed==1.100000023841858&&star->clipDurationSeconds==0.6666666865348816);
    for(const auto* profile:{cactus,upper,star})for(float speed:{.9f,1.f,1.1f})
    {
        CHECK(FusionShooterTimingProfileValid(*profile));
        CHECK(TryGetFusionShooterShotCounter(*profile,speed,counter));
        int events=0;for(int remaining=counter-1;remaining>=0;--remaining)
        {bool due=false;CHECK(TryGetFusionShooterEventAtCounter(*profile,speed,remaining,0,due));events+=due?1:0;}
        CHECK(events==1); // One callback creates five Star projectiles, not five timeline events.
    }
    CHECK(TryGetFusionCactusCycleTicks(0,counter)&&counter==151);
    CHECK(TryGetFusionCactusCycleTicks(-.1f,counter)&&counter==140);
    CHECK(TryGetFusionCactusCycleTicks(.1f,counter)&&counter==161);
    CHECK(!TryGetFusionCactusCycleTicks(.11f,counter)&&counter==161);
    CHECK(!TryGetFusionCactusCycleTicks((std::numeric_limits<float>::quiet_NaN)(),counter)&&counter==161);

    FusionRecipientEmissionSpec e;
    CHECK(FusionRecipientEmissionCount(21)==1&&FusionRecipientEmissionCount(23)==5&&FusionRecipientEmissionCount(1089)==0);
    CHECK(TryGetFusionRecipientEmission(21,0,e)&&e.projectileKind==37&&e.sourceMovingWay==0);
    CHECK(Near(e.muzzleX,.8943000924)&&Near(e.muzzleY,.6405000175)); // Actual shoot1 curve, not resting muzzle.
    CHECK(Near(e.velocityX,3.2)&&e.velocityY==0);
    constexpr double vx[]={0,0,-3.2,2.7712812921102037,2.7712812921102037};
    constexpr double vy[]={-3.592814371257485,3.592814371257485,0,-1.7964071856287425,1.7964071856287425};
    constexpr int ways[]={2,2,9,2,2};
    for(std::size_t i=0;i<5;++i)
    {
        CHECK(TryGetFusionRecipientEmission(23,i,e)&&e.projectileKind==39&&e.sourceMovingWay==ways[i]);
        CHECK(Near(e.velocityX,vx[i])&&Near(e.velocityY,vy[i]));
        CHECK(FusionRecipientProjectileVelocityValid(39,static_cast<uint8_t>(ways[i]),e.velocityX,e.velocityY));
    }
    const auto saved=e;CHECK(!TryGetFusionRecipientEmission(23,5,e)&&e.velocityX==saved.velocityX&&e.muzzleX==saved.muzzleX);
    CHECK(!TryGetFusionRecipientEmission(65535,0,e)&&e.muzzleX==saved.muzzleX);
    CHECK(!FusionRecipientProjectileVelocityValid(39,9,3.2,0));
    CHECK(!FusionRecipientProjectileVelocityValid(39,2,-3.2,0));
    CHECK(!FusionRecipientProjectileVelocityValid(37,0,3.33,0));

    CHECK(FusionCactusCanAcquire(0,true,false,false,200,201,760));
    CHECK(!FusionCactusCanAcquire(0,true,false,false,200,200,760));
    CHECK(!FusionCactusCanAcquire(0,true,false,false,200,760,760));
    CHECK(!FusionCactusCanAcquire(0,false,false,false,200,201,760));
    CHECK(!FusionCactusCanAcquire(0,true,true,false,200,201,760));
    CHECK(!FusionCactusCanAcquire(10,true,false,false,200,201,760)); // Air NPC branch explicitly deferred.
    double distance=77;
    CHECK(TryGetFusionStarAcquisitionDistance(0,false,0,0,{319,-180.64,2,2},distance)&&distance>6.68&&distance<6.73);
    CHECK(!TryGetFusionStarAcquisitionDistance(0,false,0,0,{319,-208.5,2,2},distance)); //30deg muzzle is not the2:1 acquisition ray.
    CHECK(TryGetFusionStarAcquisitionDistance(1,false,0,0,{319,178.64,2,2},distance));
    CHECK(TryGetFusionStarAcquisitionDistance(2,false,0,0,{-1,-101,2,2},distance));
    CHECK(TryGetFusionStarAcquisitionDistance(3,false,0,0,{-1,99,2,2},distance));
    CHECK(TryGetFusionStarAcquisitionDistance(4,true,0,0,{-101,-1,2,2},distance));
    distance=77;CHECK(!TryGetFusionStarAcquisitionDistance(4,false,0,0,{-101,-1,2,2},distance)&&distance==77);
    CHECK(!TryGetFusionStarAcquisitionDistance(5,true,0,0,{-101,-1,2,2},distance)&&distance==77);

    CHECK(FusionRecipientProjectileMetadataValid(37,20,0,0,PROJECTILE_SPIKE,MOTION_STRAIGHT));
    CHECK(FusionRecipientProjectileMetadataValid(37,20,0,8,PROJECTILE_SPIKE,MOTION_STRAIGHT));
    CHECK(FusionRecipientProjectileMetadataValid(39,20,0,2,PROJECTILE_STAR,MOTION_STAR));
    CHECK(FusionRecipientProjectileMetadataValid(39,20,0,9,PROJECTILE_STAR,MOTION_STAR));
    for(uint8_t way:{0,1,3,4,5,8,255})CHECK(!FusionRecipientProjectileMetadataValid(39,20,0,way,PROJECTILE_STAR,MOTION_STAR));
    CHECK(!FusionRecipientProjectileMetadataValid(39,20,0,2,PROJECTILE_PEA,MOTION_STAR));
    CHECK(!FusionRecipientProjectileMetadataValid(39,20,0,2,PROJECTILE_STAR,MOTION_STRAIGHT));
    CHECK(!FusionRecipientProjectileMetadataValid(39,40,0,2,PROJECTILE_STAR,MOTION_STAR));
    CHECK(!FusionRecipientProjectileMetadataValid(37,20,5,0,PROJECTILE_SPIKE,MOTION_STRAIGHT));
    CHECK(FusionRecipientProjectileCanHit(39,2,0,false,false,false,270));
    CHECK(!FusionRecipientProjectileCanHit(39,9,0,false,false,false,270));
    CHECK(FusionRecipientProjectileCanHit(39,9,0,true,false,false,270));
    CHECK(!FusionRecipientProjectileCanHit(37,0,0,false,false,false,270));
    CHECK(!FusionRecipientProjectileCanHit(37,8,0,true,false,false,270));
    CHECK(!FusionRecipientProjectileCanHit(39,2,0,false,true,false,270));
    CHECK(!FusionRecipientProjectileCanHit(39,2,0,false,false,true,270));
    CHECK(FusionRecipientDamageFlags(39,9)==1U&&FusionRecipientDamageFlags(39,2)==0U&&FusionRecipientDamageFlags(37,0)==0U);

    FusionRecipientRect r;
    CHECK(TryMakeFusionRecipientProjectileRect(37,100,100,3.2,0,r));
    CHECK(Near(r.width,24.33389701333333)&&Near(r.height,16.668299461077847));
    CHECK(Near(r.x+r.width*.5,98.55318285866667));
    CHECK(FusionRecipientProjectileTouchesRect(37,100,100,3.2,0,{98,98,4,4}));
    CHECK(!FusionRecipientProjectileTouchesRect(37,100,100,3.2,0,{130,98,4,4}));
    CHECK(TryMakeFusionRecipientProjectileRect(39,200,200,vx[3],vy[3],r));
    CHECK(FusionRecipientProjectileTouchesRect(39,200,200,vx[3],vy[3],{199,199,2,2}));
    CHECK(!FusionRecipientProjectileTouchesRect(39,200,200,vx[3],vy[3],{r.x+.01,r.y+.01,.01,.01})); // AABB corner is outside the rotated source box.
    const auto old=r;CHECK(!TryMakeFusionRecipientProjectileRect(65535,200,200,1,0,r)&&r.x==old.x&&r.width==old.width);
    CHECK(!FusionRecipientProjectileTouchesRect(39,200,200,0,0,{199,199,2,2}));
    std::printf("[ %s ] Fusion.SourceCactusStarRecipients checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
