#include "FusionDefenderTests.h"
#include "../../Lawn/Fusion/FusionDefenderRules.h"
#include "../../Lawn/Fusion/FusionCatalog.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace
{
FusionDefenderTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Defender check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
}

FusionDefenderTestResult RunFusionDefenderTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.SourceDefenderLayersThirdsBitesAndSupports\n");
    // Direct source CSV and loader vectors, not Classic enum ordinals.
    struct Source { uint16_t kind; int health, cost, recharge; FusionDefenderLayer layer; };
    const Source source[] = {
        {1029,8000,100,5000,FusionDefenderLayer::Body}, {1048,4000,150,3000,FusionDefenderLayer::Body},
        {1189,4000,125,3000,FusionDefenderLayer::Body}, {1027,8000,125,5000,FusionDefenderLayer::Body},
        {1028,16000,175,5000,FusionDefenderLayer::Body}, {1039,8000,200,5000,FusionDefenderLayer::Body},
        {1073,8000,250,5000,FusionDefenderLayer::Body}, {27,300,25,750,FusionDefenderLayer::Under},
        {1112,300,100,750,FusionDefenderLayer::Under}, {1114,300,100,750,FusionDefenderLayer::Under},
        {1125,300,100,750,FusionDefenderLayer::Under}, {1130,300,50,750,FusionDefenderLayer::Under},
        {1133,300,100,750,FusionDefenderLayer::Under}, {24,4000,125,3000,FusionDefenderLayer::Pumpkin},
        {1088,4000,225,3000,FusionDefenderLayer::Pumpkin}, {1089,4000,250,3000,FusionDefenderLayer::Pumpkin},
        {1164,4000,275,3000,FusionDefenderLayer::Pumpkin}, {1091,4000,225,3000,FusionDefenderLayer::Pumpkin},
        {1087,4000,150,3000,FusionDefenderLayer::Pumpkin}, {1190,4000,150,3000,FusionDefenderLayer::Pumpkin},
        {1136,300,0,750,FusionDefenderLayer::Under}, {1137,300,9999,9000,FusionDefenderLayer::Under},
        {1184,300,25,750,FusionDefenderLayer::Under},
        {1145,600,75,750,FusionDefenderLayer::Body}, {1146,900,125,1500,FusionDefenderLayer::Body},
        {931,16000,450,5000,FusionDefenderLayer::Body}, {913,32000,350,9000,FusionDefenderLayer::Body},
        {925,32000,350,9000,FusionDefenderLayer::Body}, {936,300,9999,5000,FusionDefenderLayer::Under},
        {1129,4000,100,3000,FusionDefenderLayer::Body},
        {1092,4000,225,3000,FusionDefenderLayer::Pumpkin}, {1097,8000,275,3000,FusionDefenderLayer::Pumpkin},
        {1098,4000,275,3000,FusionDefenderLayer::Pumpkin}, {1099,4000,275,3000,FusionDefenderLayer::Pumpkin},
        {935,8000,325,5000,FusionDefenderLayer::Pumpkin}
    };
    CHECK(FusionDefenderProfileCount() == 35);
    for (const auto row : source)
    {
        const auto* profile = FindFusionDefenderProfile(row.kind);
        const auto* catalog = FusionFindPlant(row.kind);
        CHECK(profile && catalog && catalog->hasStats);
        if (!profile || !catalog) continue;
        CHECK(profile->maxHealth == row.health && profile->sunCost == row.cost && profile->rechargeTicks == row.recharge);
        CHECK(profile->layer == row.layer && catalog->maxHealth == row.health && catalog->sunCost == row.cost);
        CHECK(catalog->rechargeSeconds*100 == row.recharge);
        CHECK(FusionDefenderHasDamageStages(row.kind) == (row.layer != FusionDefenderLayer::Under));
        int count = 0;
        for (std::size_t i = 0; i < FusionDefenderProfileCount(); ++i)
            if (FusionDefenderProfileAt(i)->kind == row.kind) ++count;
        CHECK(count == 1);
    }
    CHECK(!FusionDefenderProfileAt(35) && !FusionDefenderProfileAt((std::numeric_limits<std::size_t>::max)()));
    for (const uint16_t kind : {uint16_t(0),uint16_t(3),uint16_t(255),uint16_t(1026),uint16_t(1188),uint16_t(0xffff)})
        CHECK(!FindFusionDefenderProfile(kind));
    CHECK(std::strcmp(FindFusionDefenderProfile(1027)->damageNodes[0],"tnf1") == 0);
    CHECK(std::strcmp(FindFusionDefenderProfile(1088)->damageNodes[2],"pumpkin_damage2") == 0);

    struct Stage { int health, maximum; uint8_t previous, expected; };
    for (const auto row : {Stage{4000,4000,2,0}, Stage{2667,4000,2,0}, Stage{2666,4000,0,0},
        Stage{2666,4000,2,2},Stage{2665,4000,0,1}, Stage{1334,4000,2,1}, Stage{1333,4000,1,1},
        Stage{1333,4000,2,2},Stage{1332,4000,0,2}, Stage{0,4000,0,2},Stage{-1,4000,1,2},
        Stage{5333,8000,2,2},Stage{2666,8000,1,1},Stage{10666,16000,0,0},Stage{5333,16000,1,1},
        Stage{1200000000,2000000000,0,1}})
    {
        uint8_t stage = 99;
        CHECK(TryGetFusionDefenderDamageStage(row.health,row.maximum,row.previous,stage) && stage == row.expected);
    }
    uint8_t unchangedStage = 99;
    CHECK(!TryGetFusionDefenderDamageStage(4001,4000,0,unchangedStage) && unchangedStage == 99);
    CHECK(!TryGetFusionDefenderDamageStage(1,0,0,unchangedStage) && unchangedStage == 99);
    CHECK(!TryGetFusionDefenderDamageStage(1,4000,3,unchangedStage) && unchangedStage == 99);

    FusionDefenderBiteEffect bite;
    CHECK(TryGetFusionDefenderBiteEffect(1048,4,bite) && bite.shieldlessDamage == 2 && !bite.warm && !bite.coldLevels);
    CHECK(TryGetFusionDefenderBiteEffect(1048,15,bite) && bite.shieldlessDamage == 7);
    CHECK(TryGetFusionDefenderBiteEffect(1048,1,bite) && bite.shieldlessDamage == 0);
    struct Reflection { int bite, full, half; };
    for (const auto row : {Reflection{1,1,0},Reflection{2,2,1},Reflection{15,15,7},Reflection{16,16,8},
        Reflection{2147483647,2147483647,1073741823}})
    {
        CHECK(TryGetFusionDefenderBiteEffect(1088,row.bite,bite) && bite.shieldlessDamage == row.full &&
            !bite.chilledTicks && !bite.coldLevels && !bite.warm);
        CHECK(TryGetFusionDefenderBiteEffect(1048,row.bite,bite) && bite.shieldlessDamage == row.half &&
            !bite.chilledTicks && !bite.coldLevels && !bite.warm);
    }
    for (const uint16_t kind : {uint16_t(1088),uint16_t(1048)})
        for (const int damage : {0,-1,(std::numeric_limits<int>::min)()})
        {
            bite.shieldlessDamage = 77; bite.chilledTicks = 11; bite.coldLevels = 12; bite.warm = true;
            CHECK(!TryGetFusionDefenderBiteEffect(kind,damage,bite) && bite.shieldlessDamage == 77 &&
                bite.chilledTicks == 11 && bite.coldLevels == 12 && bite.warm);
        }
    CHECK(TryGetFusionDefenderBiteEffect(1073,4,bite) && bite.shieldlessDamage == 20 && bite.warm && !bite.chilledTicks);
    for (const uint16_t kind : {uint16_t(1039),uint16_t(1189)})
        CHECK(TryGetFusionDefenderBiteEffect(kind,4,bite) && bite.chilledTicks == 1000 && bite.coldLevels == 10 && !bite.shieldlessDamage && !bite.warm);
    CHECK(TryGetFusionDefenderBiteEffect(1029,4,bite) && !bite.chilledTicks && !bite.coldLevels && !bite.shieldlessDamage && !bite.warm);
    bite.shieldlessDamage = 77;
    CHECK(!TryGetFusionDefenderBiteEffect(1048,0,bite) && bite.shieldlessDamage == 77);
    CHECK(!TryGetFusionDefenderBiteEffect(3,4,bite) && bite.shieldlessDamage == 77);

    for (const auto row : source)
    {
        CHECK(!FusionDefenderBlocksPotDamage(row.kind,true,0));
        CHECK(!FusionDefenderBlocksPotDamage(row.kind,false,1));
        CHECK(FusionDefenderBlocksPotDamage(row.kind,true,1) == (row.layer == FusionDefenderLayer::Under));
        CHECK(FusionDefenderBlocksPotDamage(row.kind,true,2) == (row.layer == FusionDefenderLayer::Under));
        CHECK(!FusionDefenderBlocksPotDamage(row.kind,true,-1) && !FusionDefenderBlocksPotDamage(row.kind,true,3));
    }
    CHECK(FusionDefenderElementRecovery(1039,FusionDefenderElement::Freeze) == 1000);
    CHECK(FusionDefenderElementRecovery(1189,FusionDefenderElement::Freeze) == 0); // Actual CreateFreeze, not assumed ice-family healing.
    CHECK(FusionDefenderElementRecovery(1073,FusionDefenderElement::Fire) == 3000);
    CHECK(FusionDefenderElementRecovery(1039,FusionDefenderElement::Fire) == 0);
    CHECK(FusionDefenderElementRecovery(1073,FusionDefenderElement::Freeze) == 0);

    FusionDefenderBucketEffect bucket;
    for (int type = 0; type <= 10; ++type)
    {
        CHECK(TryGetFusionDefenderBucketEffect(1028,type,bucket) && bucket.recoverHealth == 500 && bucket.replacementKind == 0xffff);
        CHECK(TryGetFusionDefenderBucketEffect(1029,type,bucket) && bucket.recoverHealth == 500 && bucket.replacementKind == 0xffff);
        CHECK(TryGetFusionDefenderBucketEffect(1027,type,bucket) == (type == 1));
    }
    CHECK(TryGetFusionDefenderBucketEffect(1027,1,bucket) && bucket.replacementKind == 1028 && !bucket.recoverHealth);
    bucket.replacementKind = 99; bucket.recoverHealth = 77;
    CHECK(!TryGetFusionDefenderBucketEffect(1028,-1,bucket) && bucket.replacementKind == 99 && bucket.recoverHealth == 77);
    CHECK(!TryGetFusionDefenderBucketEffect(1028,11,bucket) && bucket.replacementKind == 99 && bucket.recoverHealth == 77);
    CHECK(!TryGetFusionDefenderBucketEffect(1048,1,bucket) && bucket.replacementKind == 99 && bucket.recoverHealth == 77);

    for (const uint16_t kind : {uint16_t(26),uint16_t(1113),uint16_t(1124),uint16_t(1134)})
    {
        CHECK(FusionDefenderLobberInterval(kind,1112,3.0f) == 2.0f);
        CHECK(FusionDefenderLobberInterval(kind,0xffff,3.0f) == 3.0f); // Move off resets from source original, not a cached delta.
    }
    CHECK(FusionDefenderLobberInterval(1111,1112,5.0f) == 3.33f);
    CHECK(FusionDefenderLobberInterval(1111,1114,5.0f) == 5.0f);
    CHECK(FusionDefenderLobberInterval(28,1112,3.0f) == 3.0f);
    CHECK(FusionDefenderLobberInterval(32,1112,3.0f) == 3.0f);
    for (const uint16_t kind : {uint16_t(28),uint16_t(1113),uint16_t(1121),uint16_t(1138)})
    {
        CHECK(FusionDefenderCornDenominator(kind,1114,4) == 2);
        CHECK(FusionDefenderCornDenominator(kind,0xffff,4) == 4);
    }
    CHECK(FusionDefenderCornDenominator(26,1114,4) == 4);
    for (const uint16_t kind : {uint16_t(32),uint16_t(1116),uint16_t(1121),uint16_t(1124),uint16_t(1141)})
    {
        CHECK(FusionDefenderMelonEnhanced(kind,1125));
        CHECK(!FusionDefenderMelonEnhanced(kind,0xffff));
    }
    CHECK(!FusionDefenderMelonEnhanced(26,1125) && !FusionDefenderMelonEnhanced(28,1125));
    CHECK(FindFusionDefenderProfile(1130)->pendingDependencies == kDefenderPendingNone);
    CHECK(FindFusionDefenderProfile(1133)->pendingDependencies & kDefenderPendingUmbrella);
    CHECK(FindFusionDefenderProfile(1088)->bite == FusionDefenderBite::ReflectFull &&
        FindFusionDefenderProfile(1088)->pendingDependencies == kDefenderPendingNone);
    CHECK(FindFusionDefenderProfile(1089)->pendingDependencies & kDefenderPendingStar);
    CHECK(FindFusionDefenderProfile(1164)->pendingDependencies & kDefenderPendingZombieCherry);
    std::printf("[ %s ] Fusion.SourceDefenderLayersThirdsBitesAndSupports checks=%d failures=%d\n",
        result.mFailures ? "FAILED" : "OK",result.mChecks,result.mFailures);
    return result;
}
