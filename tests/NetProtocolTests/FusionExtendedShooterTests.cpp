#include "FusionExtendedShooterTests.h"
#include "../../Lawn/Fusion/FusionLobberRules.h"
#include "../../Lawn/Fusion/FusionShooterBatchRules.h"
#include "../../Lawn/Fusion/FusionChomperRules.h"
#include "../../Lawn/Fusion/FusionRollingProjectileRules.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

FusionExtendedShooterTestResult RunFusionExtendedShooterTests()
{
    using namespace LawnFusion;
    FusionExtendedShooterTestResult result{};
    auto check=[&](bool okay,int line){++result.mChecks;if(!okay){++result.mFailures;std::printf("FusionExtendedShooterTests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    struct Row { uint16_t kind,payload;int damage,deadline; };
    constexpr Row lobbers[]={{1117,61,20,35},{1118,63,40,33},{1119,64,80,35},
        {1149,86,160,35},{1126,71,140,46},{1135,75,80,33}};
    for(const auto& row:lobbers)
    {
        const auto* p=FindFusionLobberProfile(row.kind);const auto* t=FindFusionExtendedLobberTimingProfile(row.kind);
        CHECK(p&&p->projectileKind==row.payload&&p->damage==row.damage);
        int counter=0;CHECK(t&&TryGetFusionShooterShotCounter(*t,1,counter)&&counter==row.deadline);
        CHECK(p&&FusionLobberProjectileMetadataValid(row.payload,row.damage,0,p->nativeType));
    }
    CHECK(FindFusionLobberProfile(1117,true)->projectileKind==62);
    CHECK(FindFusionExtendedLobberTimingProfile(1117,"shoot2"));
    CHECK(!FindFusionExtendedLobberTimingProfile(1118,"shoot2"));
    CHECK(FindFusionExtendedLobberTimingProfile(1135,"supershoot"));
    CHECK(!FindFusionLobberProjectileTraits(63)->sunReward);
    int damage=123;CHECK(TryGetFusionPoisonLobberDamage(61,20,5,false,damage)&&damage==120);
    CHECK(TryGetFusionPoisonLobberDamage(64,26,5,true,damage)&&damage==51);
    damage=123;CHECK(!TryGetFusionPoisonLobberDamage(64,80,-1,false,damage)&&damage==123);
    CHECK(!TryGetFusionPoisonLobberDamage(64,80,(std::numeric_limits<int>::max)(),false,damage)&&damage==123);
    CHECK(FusionLobberProjectileMetadataValid(75,160,0,PROJECTILE_CABBAGE));
    CHECK(!FusionLobberProjectileMetadataValid(75,1000000,0,PROJECTILE_CABBAGE));
    float vx=5,vy=4;CHECK(TryApplyFusionLobberBounce(63,4,vx,vy)&&vx==5&&vy==-4);
    CHECK(FindFusionLobberProfile(26)->damage==40&&FindFusionLobberProjectileTraits(52)->fixedPierceLimit==1);
    for(uint16_t kind:{1079,1081,1005,901,907,933})CHECK(IsFusionShooterBatchKind(kind));
    CHECK(FindFusionShooterBatchTimingProfile(901)->eventClipSeconds.size()==4);
    CHECK(FindFusionShooterBatchTimingProfile(907)->eventClipSeconds.size()==4);
    int ticks=0;CHECK(TryGetFusionShooterBatchCycleTicks(1081,0,ticks)&&ticks==301);
    CHECK(FusionShooterBatchStateValid(907,15,0,0,0));CHECK(!FusionShooterBatchStateValid(907,16,0,0,0));
    CHECK(FusionShooterBatchStateValid(933,0,0,0,1));CHECK(!FusionShooterBatchStateValid(933,0,0,1,1));
    CHECK(FusionShooterBatchProjectileMetadataValid(40,40,0,6,PROJECTILE_STAR,MOTION_STAR));
    CHECK(!FusionShooterBatchProjectileMetadataValid(40,40,0,0,PROJECTILE_STAR,MOTION_STAR));
    CHECK(FusionShooterBatchProjectileMetadataValid(100,300,0,18,PROJECTILE_SPIKE,MOTION_STAR));
    CHECK(!FusionShooterBatchProjectileMetadataValid(100,300,0,18,PROJECTILE_SPIKE,MOTION_STRAIGHT));
    FusionRecipientRect rect;CHECK(TryMakeFusionShooterBatchRotatedRect(100,0,0,0,rect));
    CHECK(std::abs(rect.width-3.68*.2*(72./1.35))<1e-8);
    CHECK(FusionShooterBatchRotatedProjectileTouchesRect(100,0,0,0,{-1,-1,2,2}));
    CHECK(!FusionShooterBatchRotatedProjectileTouchesRect(100,0,0,0,{80,80,2,2}));
    FusionRollingRect block;CHECK(TryMakeFusionRollingRect(4,0,0,block));
    CHECK(std::abs(block.width-.537*(72./1.35))<1e-8);
    CHECK(TryMakeFusionRollingRect(5,0,0,block)&&std::abs(block.height-.6255*(100./1.67))<1e-8);
    CHECK(TryMakeFusionRollingRect(6,0,0,block) && std::abs(block.width-.537*(72./1.35))<1e-8 &&
        std::abs(block.height-.6255*(100./1.67))<1e-8);
    CHECK(!TryMakeFusionRollingRect(7,0,0,block));
    float threshold=0;CHECK(TryGetFusionHypnoFumeThreshold(100,100,0,0,0,threshold)&&threshold==1);
    CHECK(TryGetFusionHypnoFumeThreshold(1,100,0,0,0,threshold)&&threshold==.75f);
    FusionChomperTiming timing;CHECK(TryGetFusionChomperTiming(1015,1,timing)&&timing.bite==50&&timing.chew==92&&timing.swallow==125);
    CHECK(FindFusionChomperProfile(1011)->digestTicks==4000);
    CHECK(FusionShooterBatchStateValid(1011,0,223,0,1));
    CHECK(!FusionShooterBatchStateValid(1011,0,224,0,1));
    CHECK(!FusionShooterBatchStateValid(1011,0,3,0,0));
    CHECK(FindFusionFumeProfile(1159)&&FindFusionFumeProfile(1160));
    CHECK(FusionFumeCanHit(0,true,false,false,7*kFusionBasePixelsX));
    CHECK(!FusionFumeCanHit(0,true,false,false,7*kFusionBasePixelsX+.001));
#undef CHECK
    return result;
}
