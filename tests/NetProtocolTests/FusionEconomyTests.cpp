#include "FusionEconomyTests.h"
#include "../../Lawn/Fusion/FusionEconomyRules.h"
#include "../../Lawn/Fusion/FusionProducerRules.h"
#include "../../ConstEnums.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

FusionEconomyTestResult RunFusionEconomyTests()
{
    FusionEconomyTestResult result{};using namespace LawnFusion;
#define CHECK(...) do{++result.mChecks;if(!(__VA_ARGS__)){++result.mFailures;std::printf("Economy:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    CHECK(kFusionSunShroomGrowthTicks==12001&&kFusionGoldSunflowerInitialTicks==501&&
        kFusionGoldSunflowerCycleTicks==1001&&kFusionGoldSunflowerMaximumCharges==200);
    for(uint16_t kind:{1031,1184,1136,1137,1188})
    {
        CHECK(IsFusionEconomyBatchKind(kind));CHECK(FusionEconomyPlantStateValid(kind,0,0,0,0));
        CHECK(!FusionEconomyPlantStateValid(kind,0,1,0,0));
        CHECK(!FusionEconomyPlantStateValid(kind,0,0,1,0));
        CHECK(!FusionEconomyPlantStateValid(kind,0,0,0,2));
        if(kind!=1188)CHECK(!FusionEconomyPlantStateValid(kind,1,0,0,0));
        if(kind!=1031&&kind!=1188)CHECK(!FusionEconomyPlantStateValid(kind,0,0,0,1));
    }
    CHECK(FusionEconomyPlantStateValid(1031,0,0,0,1));
    for(uint32_t charges:{0u,1u,199u,200u})for(uint8_t mode:{uint8_t(0),uint8_t(1)})
    {
        CHECK(FusionEconomyPlantStateValid(1188,charges,0,0,mode));
        // The coroutine's launch clock lives in pendingProductionTicks. The
        // unrelated bag field must never gain a second interpretation.
        CHECK(!FusionEconomyPlantStateValid(1188,charges,0,1,mode));
        CHECK(!FusionEconomyPlantStateValid(1188,charges,0,10,mode));
    }
    CHECK(!FusionEconomyPlantStateValid(1188,201,0,0,0));
    CHECK(!FusionEconomyPlantStateValid(1188,0xffffffffu,0,0,1));
    for(uint16_t kind:{0,1,1030,1187,1189,65535})
    {CHECK(!IsFusionEconomyBatchKind(kind));CHECK(!FusionEconomyPlantStateValid(kind,0,0,0,0));}
    struct Upgrade {uint16_t before,after;int fee;};
    const Upgrade upgrades[]={{1134,1135,1000},{1138,1139,2000},{1141,1142,3000},
        {1136,1137,2000},{1143,1144,1000},{1145,1146,1000},{1187,1188,3000}};
    for(const auto row:upgrades)
    {
        uint16_t after=123;int fee=456;
        CHECK(TryGetFusionMoneyUpgrade(row.before,0,after,fee)&&after==row.after&&fee==row.fee);
        FusionMoneyActionQuote quote;
        CHECK(TryGetFusionMoneyActionQuote(row.before,0,quote)&&quote.upgrade&&quote.resultKind==row.after&&quote.fee==row.fee);
        CHECK(IsFusionMoneyUpgradeAncestry(row.before,65535,row.after));
        CHECK(!IsFusionMoneyUpgradeAncestry(row.before,0,row.after));
        CHECK(!IsFusionMoneyUpgradeAncestry(row.after,65535,row.before));
        CHECK(!IsFusionMoneyUpgradeAncestry(65534,65535,row.after));
        for(int count:{1,2,4})
        {
            const int expected=row.fee*((row.before==1136||row.before==1187)?(1<<count):1);
            CHECK(TryGetFusionMoneyUpgrade(row.before,count,after,fee)&&after==row.after&&fee==expected);
        }
        after=123;fee=456;
        CHECK(!TryGetFusionMoneyUpgrade(row.before,-1,after,fee)&&after==123&&fee==456);
    }
    // Both exponential fees stop before signedI32 overflow, atomically.
    for(const Upgrade row:{Upgrade{1136,1137,2000},Upgrade{1187,1188,3000}})
    {
        uint16_t after=123;int fee=456;
        const int last=row.before==1136?20:19;
        CHECK(TryGetFusionMoneyUpgrade(row.before,last,after,fee)&&after==row.after&&fee==row.fee*(1<<last));
        after=123;fee=456;
        CHECK(!TryGetFusionMoneyUpgrade(row.before,last+1,after,fee)&&after==123&&fee==456);
        CHECK(!TryGetFusionMoneyUpgrade(row.before,(std::numeric_limits<int>::max)(),after,fee)&&after==123&&fee==456);
    }
    for(uint16_t kind:{0,31,1135,1137,1188,65535})
    {uint16_t after=123;int fee=456;CHECK(!TryGetFusionMoneyUpgrade(kind,0,after,fee)&&after==123&&fee==456);}
    for(const Upgrade row:{Upgrade{1135,0,1000},Upgrade{1146,0,1000},Upgrade{1188,0,1000},
        Upgrade{916,0,1000},Upgrade{923,0,1000},Upgrade{934,0,1000},
        Upgrade{1139,0,3000},Upgrade{1142,0,3000},Upgrade{1148,0,3000},Upgrade{1144,0,2000}})
    {int fee=456;CHECK(TryGetFusionMoneySkillFee(row.before,fee)&&fee==row.fee);}
    for(uint16_t kind:{0,1,1136,1137,1145,1187,65535})
    {int fee=456;CHECK(!TryGetFusionMoneySkillFee(kind,fee)&&fee==456);}
    // A price fallback must not admit ordinary plants to ReinforcePlant.
    // In particular an ordinary Sunflower is not the3000 Silver->Gold upgrade.
    for(uint16_t kind:{0,1,31,1137,65534,65535})
    {
        FusionMoneyActionQuote quote{123,456,true};
        CHECK(!TryGetFusionMoneyActionQuote(kind,0,quote)&&quote.resultKind==123&&quote.fee==456&&quote.upgrade);
    }
    for(const Upgrade row:{Upgrade{1135,1135,1000},Upgrade{1144,1144,2000},Upgrade{1139,1139,3000}})
    {
        FusionMoneyActionQuote quote;
        CHECK(TryGetFusionMoneyActionQuote(row.before,0,quote)&&!quote.upgrade&&quote.resultKind==row.after&&quote.fee==row.fee);
    }
    {
        FusionMoneyActionQuote quote{123,456,true};
        CHECK(!TryGetFusionMoneyActionQuote(1187,20,quote)&&quote.resultKind==123&&quote.fee==456&&quote.upgrade);
        CHECK(!TryGetFusionMoneyActionQuote(1135,-1,quote)&&quote.resultKind==123&&quote.fee==456&&quote.upgrade);
        CHECK(TryGetFusionMoneyActionQuote(1187,1,quote)&&quote.upgrade&&quote.resultKind==1188&&quote.fee==6000);
    }
    for(int fee:{-1,0,1,499,500,999,1000,2000,3000,4000,19999,20000,20500,(std::numeric_limits<int>::max)()})
    {
        const int expected=fee<=0?0:fee/500<40?fee/500:40;
        CHECK(FusionMoneySpendSunCount(fee,false)==expected);CHECK(FusionMoneySpendSunCount(fee,true)==0);
    }
    for(uint16_t kind:{1031,1184,1136,1137})
    {
        const auto* profile=FindFusionProducerProfile(kind);CHECK(profile&&FusionProducerProfileValid(*profile));if(!profile)continue;
        CHECK(profile->intervalSeconds==25&&profile->periodicCoinCount==1);
        CHECK(profile->mode==(kind==1031?FusionProducerMode::Sun:FusionProducerMode::MoneyPot));
        int ticks=-1;CHECK(TryGetFusionProducerInitialTicks(*profile,4,ticks)&&ticks==(kind==1031?401:400));
        CHECK(TryGetFusionProducerInitialTicks(*profile,7,ticks)&&ticks==(kind==1031?701:700));
        CHECK(FusionProducerMaximumTicks(kind)==(kind==1031?2701:2750));
        if(kind==1031)
        {CHECK(TryGetFusionProducerCycleTicks(*profile,-2,ticks)&&ticks==2301);CHECK(TryGetFusionProducerCycleTicks(*profile,2,ticks)&&ticks==2701);}
        else
        {
            CHECK(TryGetFusionProducerCycleTicks(*profile,.9f,ticks)&&ticks==2250);
            CHECK(TryGetFusionProducerCycleTicks(*profile,1.1f,ticks)&&ticks==2750);
            for(float invalid:{.899f,1.101f,(std::numeric_limits<float>::quiet_NaN)()})
            {ticks=123;CHECK(!TryGetFusionProducerCycleTicks(*profile,invalid,ticks)&&ticks==123);}
            CHECK(FusionProducerGateOpen(*profile,true,false,0)&&FusionProducerGateOpen(*profile,true,true,0));
        }
        CHECK(!FusionProducerGateOpen(*profile,false,false,1));
    }
    struct Sun {uint16_t item;int value;CoinType type;};
    for(const Sun row:{Sun{0,25,COIN_SUN},Sun{1,50,COIN_LARGESUN},Sun{2,15,COIN_SMALLSUN},Sun{13,5,COIN_SMALLSUN}})
    {
        int value=123;CHECK(TryGetFusionSourceSunValue(row.item,value)&&value==row.value);
        CHECK(FusionSunCoinMetadataValid(row.type,row.value));
        for(CoinType type:{COIN_SILVER,COIN_GOLD,COIN_DIAMOND})CHECK(!FusionSunCoinMetadataValid(type,row.value));
    }
    for(uint16_t item:{3,12,14,65535}){int value=123;CHECK(!TryGetFusionSourceSunValue(item,value)&&value==123);}
    for(CoinType type:{COIN_SMALLSUN,COIN_SUN,COIN_LARGESUN})for(int value:{-1,0,14,16,24,26,49,51,100})
        CHECK(!FusionSunCoinMetadataValid(type,value));
    CHECK(!FusionSunCoinMetadataValid(COIN_SUN,15)&&!FusionSunCoinMetadataValid(COIN_SMALLSUN,50)&&
        !FusionSunCoinMetadataValid(COIN_LARGESUN,25));
#undef CHECK
    std::printf("Fusion.Economy checks=%d failures=%d\n",result.mChecks,result.mFailures);return result;
}
