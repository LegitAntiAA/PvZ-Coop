#include "FusionMoneyTests.h"
#include "../../Lawn/Fusion/FusionMoneyRules.h"
#include "../../Lawn/Fusion/FusionProducerRules.h"
#include "../../Lawn/Fusion/FusionLobberRules.h"
#include "../../Lawn/Fusion/FusionDefenderRules.h"
#include "../../Lawn/Fusion/FusionCatalog.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionMoneyTestResult result;
void Check(bool value, int line)
{ ++result.mChecks; if (!value) { ++result.mFailures; std::printf("    Money check failed line=%d\n",line); } }
#define CHECK(value) Check(!!(value),__LINE__)
bool Near(double a, double b) { return std::abs(a-b) < 1e-7; }
}
FusionMoneyTestResult RunFusionMoneyTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.NativeMoneySourceRewardsAndSharedFamilies\n");
    CHECK(kFusionMoneyInitial == 3000 && kFusionMoneyMaximum == 99999);
    struct Item { uint16_t id; CoinType type; int value; };
    for (const auto sample : {Item{34,COIN_SILVER,100},Item{35,COIN_GOLD,300},Item{36,COIN_DIAMOND,1000},
        Item{38,COIN_SILVER,25},Item{39,COIN_GOLD,100}})
    {
        CoinType type=COIN_SUN; int value=-1;
        CHECK(TryGetFusionMoneyItem(sample.id,type,value) && type==sample.type && value==sample.value);
        CHECK(FusionMoneyValueValid(value) && FusionMoneyCoinMetadataValid(type,value));
        int after=-1;
        CHECK(TryCreditFusionMoney(3000,value,after) && after==3000+value);
        CHECK(TryCreditFusionMoney(99998,value,after) && after==99999);
        CHECK(TryCreditFusionMoney(99999,value,after) && after==99999);
    }
    for (uint16_t unknown : {uint16_t(0),uint16_t(13),uint16_t(37),uint16_t(65535)})
    {
        CoinType type=COIN_SUN; int value=77;
        CHECK(!TryGetFusionMoneyItem(unknown,type,value) && type==COIN_SUN && value==77);
    }
    CHECK(!FusionMoneyCoinMetadataValid(COIN_GOLD,25));
    CHECK(!FusionMoneyCoinMetadataValid(COIN_SILVER,300));
    CHECK(!FusionMoneyCoinMetadataValid(COIN_DIAMOND,100));
    CHECK(!FusionMoneyCoinMetadataValid(COIN_SUN,100));
    for (int value : {-1,0,1,5,26,99999,(std::numeric_limits<int>::max)()})
    {
        int after=77;
        CHECK(!TryCreditFusionMoney(3000,value,after) && after==77);
    }
    for (int wallet : {-1,100000,(std::numeric_limits<int>::max)()})
    { int after=77; CHECK(!TryCreditFusionMoney(wallet,25,after) && after==77); }
    struct Reward { int draw; uint16_t item; int value; };
    for (const auto sample : {Reward{0,35,300},Reward{19,35,300},Reward{20,34,100},Reward{94,34,100},Reward{95,36,1000},Reward{99,36,1000}})
    { uint16_t item=0; int value=0; CHECK(TryGetFusionMarigoldReward(sample.draw,item,value) && item==sample.item && value==sample.value); }
    for (int draw : {-1,100,(std::numeric_limits<int>::max)()})
    { uint16_t item=77; int value=77; CHECK(!TryGetFusionMarigoldReward(draw,item,value) && item==77 && value==77); }
    int silver=0,gold=0,diamond=0,total=0;
    for (int draw=0; draw<100; ++draw)
    {
        uint16_t item=0; int value=0;
        CHECK(TryGetFusionMarigoldReward(draw,item,value));
        silver += item==34; gold += item==35; diamond += item==36; total += value;
    }
    CHECK(silver==75 && gold==20 && diamond==5 && total==18500); // Exact source intervals, not a statistical claim.

    for (uint16_t kind : {uint16_t(31),uint16_t(1140)})
    {
        const auto* profile=FindFusionProducerProfile(kind); const auto* definition=FusionFindPlant(kind);
        CHECK(profile && profile->mode==FusionProducerMode::Marigold && FusionProducerProfileValid(*profile));
        if (!profile) continue;
        CHECK(profile->periodicCoinCount==(kind==31?1:2) && profile->intervalSeconds==(kind==31?25:20));
        CHECK(definition && definition->hasStats && definition->maxHealth==300 && definition->productionIntervalSeconds==profile->intervalSeconds);
        CHECK(FusionProducerGateOpen(*profile,true,false,0) && FusionProducerGateOpen(*profile,true,true,0));
        CHECK(!FusionProducerGateOpen(*profile,false,false,1));
        struct Initial { float seconds; int ticks; };
        for (const auto sample : {Initial{4,400},Initial{4.01f,402},Initial{4.125f,413},Initial{7,700}})
        { int ticks=77; CHECK(TryGetFusionProducerInitialTicks(*profile,sample.seconds,ticks) && ticks==sample.ticks); }
        int ticks=77;
        CHECK(TryGetFusionProducerCycleTicks(*profile,-2.0f,ticks) && ticks==(kind==31?2300:1800));
        CHECK(TryGetFusionProducerCycleTicks(*profile,2.0f,ticks) && ticks==(kind==31?2700:2200));
        CHECK(TryGetFusionProducerCycleTicks(*profile,.1f,ticks) && ticks==(kind==31?2511:2011));
        CHECK(FusionProducerMaximumTicks(kind)==(kind==31?2700:2200));
        ticks=77; CHECK(!TryGetFusionProducerCycleTicks(*profile,2,ticks) && ticks==77); // Integer jitter belongs to the older Sun callbacks.
        CHECK(!TryGetFusionProducerCycleTicks(*profile,2.01f,ticks) && ticks==77);
        CHECK(!TryGetFusionProducerCycleTicks(*profile,(std::numeric_limits<float>::quiet_NaN)(),ticks) && ticks==77);
        CHECK(!TryGetFusionProducerInitialTicks(*profile,7.01f,ticks) && ticks==77);
    }
    struct OldProducer { uint16_t kind; int maximum; };
    for (const auto sample : {OldProducer{1,2701},OldProducer{1000,2701},OldProducer{1006,1701},OldProducer{1033,2201}})
    {
        const auto* profile=FindFusionProducerProfile(sample.kind); int ticks=77;
        CHECK(profile && profile->mode==FusionProducerMode::Sun && FusionProducerMaximumTicks(sample.kind)==sample.maximum);
        CHECK(profile && TryGetFusionProducerInitialTicks(*profile,4,ticks) && ticks==401);
        CHECK(profile && TryGetFusionProducerCycleTicks(*profile,2,ticks) && ticks==sample.maximum);
    }
    CHECK(FusionProducerMaximumTicks(65535)==0);

    struct Shot { uint16_t plant,payload; ProjectileType type; int damage; double mx,my; bool alternate; };
    for (const auto sample : {Shot{1134,74,PROJECTILE_CABBAGE,60,-.1617,1.2408,false},Shot{1138,76,PROJECTILE_KERNEL,40,.2211,1.6401,false},
        Shot{1138,78,PROJECTILE_BUTTER,60,.2211,1.6401,true},Shot{1141,81,PROJECTILE_MELON,100,.2013,1.6071,false}})
    {
        const auto* profile=FindFusionLobberProfile(sample.plant,sample.alternate);
        const auto* traits=FindFusionLobberProjectileTraits(sample.payload);
        CHECK(profile && profile->projectileKind==sample.payload && profile->damage==sample.damage && profile->nativeType==sample.type);
        CHECK(profile && Near(profile->muzzleX,sample.mx) && Near(profile->muzzleY,sample.my));
        CHECK(traits && traits->fixedPierceLimit==1 && traits->moneyItem==(sample.payload==78?38:0));
        CHECK(FusionLobberProjectileMetadataValid(sample.payload,sample.damage,0,sample.type));
        CHECK(!FusionLobberProjectileMetadataValid(sample.payload,sample.damage+1,0,sample.type));
        CHECK(!FusionLobberProjectileMetadataValid(sample.payload,sample.damage,5,sample.type));
        CHECK(!FusionLobberProjectileMetadataValid(sample.payload,sample.damage,0,PROJECTILE_PEA));
        FusionLobberRect rect;
        CHECK(TryMakeFusionLobberRect(sample.payload,400,300,rect) && rect.width>0 && rect.height>0);
    }
    for (const auto sample : {Shot{1139,77,PROJECTILE_KERNEL,60,.2211,1.6401,false},
        Shot{1139,79,PROJECTILE_BUTTER,80,.2211,1.6401,true},Shot{1142,82,PROJECTILE_MELON,120,.2013,1.6071,false}})
    {
        const auto* profile=FindFusionLobberProfile(sample.plant,sample.alternate);
        const auto* traits=FindFusionLobberProjectileTraits(sample.payload);
        CHECK(profile && profile->projectileKind==sample.payload && profile->damage==sample.damage && profile->nativeType==sample.type);
        CHECK(profile && Near(profile->muzzleX,sample.mx) && Near(profile->muzzleY,sample.my));
        CHECK(traits && traits->fixedPierceLimit==1 && traits->sourceRotation &&
            traits->butter==(sample.payload==79) && traits->splash==(sample.payload==82));
        CHECK(FusionLobberProjectileMetadataValid(sample.payload,sample.damage,0,sample.type));
        CHECK(!FusionLobberProjectileMetadataValid(sample.payload,sample.damage+1,0,sample.type));
        CHECK(!FusionLobberProjectileMetadataValid(sample.payload,sample.damage,5,sample.type));
        CHECK(!FusionLobberProjectileMetadataValid(sample.payload,sample.damage,0,PROJECTILE_PEA));
    }
    CHECK(!FindFusionLobberProfile(65535) && !FindFusionLobberProjectileTraits(65535));
    CHECK(FusionLobberCornDenominator(1138)==4 && FusionDefenderCornDenominator(1138,1114,4)==2);
    CHECK(FusionDefenderLobberInterval(1134,1112,3)==2 && FusionDefenderMelonEnhanced(1141,1125));
    for (int count : {1,3,25,100})
    {
        int damage=77;
        CHECK(TryGetFusionLobberSplashDamage(80,count,damage,true) && damage==53);
        CHECK(TryGetFusionLobberSplashDamage(100,count,damage,true) && damage==66);
        CHECK(TryGetFusionLobberSplashDamage(120,count,damage,true) && damage==80);
    }
    int damage=77;
    CHECK(!TryGetFusionLobberSplashDamage(100,0,damage,true) && damage==77);
    CHECK(TryGetFusionLobberSplashDamage(80,25,damage) && damage==25); // Existing unenhanced N formula remains.
    const auto* cabbage=FindFusionShooterTimingProfile(26);
    if (cabbage)
    {
        auto initial=*cabbage; initial.attackIntervalSeconds=2;
        int ticks=77;
        CHECK(TryGetFusionShooterInitialTicks(initial,.5f,ticks) && ticks==101);
        CHECK(TryGetFusionLobberCycleTicks(26,0,ticks,2) && ticks==201);
        CHECK(TryGetFusionLobberCycleTicks(26,0,ticks) && ticks==301);
        ticks=77; CHECK(!TryGetFusionLobberCycleTicks(26,0,ticks,-1) && ticks==77);
    }
    else CHECK(false);
    std::printf("[ %s ] Fusion.NativeMoneySourceRewardsAndSharedFamilies (%d checks, %d failures)\n",result.mFailures?"  FAILED  ":"       OK",result.mChecks,result.mFailures);
    return result;
}
