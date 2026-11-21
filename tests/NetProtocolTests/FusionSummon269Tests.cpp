#include "FusionSummon269Tests.h"
#include "../../Lawn/Fusion/FusionSummon269Rules.h"
#include "../../Lawn/Fusion/FusionUmbrellaBatchRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
namespace
{
FusionSummon269TestResult result;
void Check(bool value,int line){++result.mChecks;if(!value){++result.mFailures;std::printf("    Summon269 check failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
}
FusionSummon269TestResult RunFusionSummon269Tests()
{
    using namespace LawnFusion;result={};
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    const FusionSummon269Choice sentinel{65000,FusionSummon269Post::PogoStartHealth60,false,false,73};
    for(int draw=0;draw<4;++draw)
    {
        constexpr uint16_t ordinary[]={15,109,18,104},strong[]={205,204,201,203};
        FusionSummon269Choice c=sentinel;
        CHECK(TryChooseFusionHypno269(900,false,false,false,false,draw,c)&&c.kind==ordinary[draw]);
        CHECK((c.post==FusionSummon269Post::BreakSecondArmor)==(draw==3));
        CHECK(TryChooseFusionHypno269(900,false,false,true,false,draw,c)&&c.kind==strong[draw]);
        CHECK(TryChooseFusionHypno269(900,true,false,false,false,draw,c)&&c.kind==14);
        CHECK(TryChooseFusionHypno269(900,true,false,true,false,draw,c)&&c.kind==200);
        CHECK(TryChooseFusionHypno269(917,false,false,false,false,draw,c)&&c.kind==104);
        CHECK(TryChooseFusionHypno269(917,true,true,false,false,draw,c)&&c.kind==214);
        CHECK(TryChooseFusionHypno269(917,false,false,true,false,draw,c)&&c.kind==203);
        CHECK(TryChooseFusionHypno269(917,true,true,true,true,draw,c)&&c.kind==105);
    }
    for(int draw:{-1,4,999}){auto c=sentinel;CHECK(!TryChooseFusionHypno269(900,false,false,false,false,draw,c)&&c==sentinel);}
    for(uint8_t item=0;item<11;++item)for(int draw=0;draw<3;++draw)
    {
        FusionSummon269Choice c;CHECK(TryChooseFusionMagnet269(item,draw,false,c));CHECK(FindFusionSummon269NpcProfile(c.kind));
        c=sentinel;const bool unsupported=item==3||item==6;
        CHECK(TryChooseFusionMagnet269(item,draw,true,c)!=unsupported);
        if(unsupported)CHECK(c==sentinel);else CHECK(c.kind==(item<3?13:item==4||item==5||item==8||item==9?14:item==10?11:12));
    }
    FusionSummon269Choice c;CHECK(TryChooseFusionMachine269(false,301,c)&&c.kind==211&&c.healthOverride==100);
    CHECK(TryChooseFusionMachine269(true,1,c)&&c.kind==200&&c.healthOverride==0);
    CHECK(TryChooseFusionNut269(false,false,c)&&c.kind==105);CHECK(TryChooseFusionNut269(true,true,c)&&c.kind==13);
    CHECK(TryChooseFusionIceHypno269(false,c)&&c.kind==111&&!c.withEffect);
    float interval=99;CHECK(TryGetFusionSummon269Interval(900,false,false,interval)&&interval==30);
    CHECK(TryGetFusionSummon269Interval(900,true,false,interval)&&interval==10);
    CHECK(TryGetFusionSummon269Interval(917,true,false,interval)&&interval==2.5f);
    CHECK(TryGetFusionSummon269Interval(1147,false,false,interval)&&interval==45);
    for(int rows:{5,6})for(int row=0;row<rows;++row)
    {
        std::array<int,3> selected{};int count=-1;CHECK(TryGetFusionHypnoDoom269Rows(row,rows,true,selected,count)&&count==3);
        CHECK(selected[0]==row&&selected[1]==(row?row-1:0)&&selected[2]==(row==rows-1?row:row+1));
        CHECK(TryGetFusionHypnoDoom269Rows(row,rows,false,selected,count)&&count==0);
    }
    FusionGoldHypno269Plan bosses;
    CHECK(TryPlanFusionGoldHypno269(0,false,0,bosses)&&bosses.count==2&&bosses.bosses[0]==212&&bosses.bosses[1]==223);
    CHECK(TryPlanFusionGoldHypno269(12,false,0,bosses)&&bosses.bosses[1]==224);
    constexpr uint16_t randomBosses[]={218,220,212,222,221,219,223};
    for(int i=0;i<7;++i){CHECK(TryPlanFusionGoldHypno269(9,false,i,bosses)&&bosses.count==1&&bosses.bosses[0]==randomBosses[i]);}
    const auto saved=bosses;CHECK(!TryPlanFusionGoldHypno269(0,true,0,bosses)&&bosses==saved);
    uint16_t minion=99;CHECK(!TryGetFusionGoldHypno269Minion(212,minion)&&minion==99);
    CHECK(TryGetFusionGoldHypno269Minion(224,minion)&&minion==209);
    FusionUmbrella269Plan p;
    CHECK(TryPlanFusionUmbrella269(1148,24,0,100,0,.25f,.5f,false,false,true,false,p)&&p.kind==42&&p.retireOriginal);
    CHECK(TryPlanFusionUmbrella269(1148,25,0,100,0,.25f,.5f,false,false,true,false,p)&&p.kind==41);
    CHECK(TryPlanFusionUmbrella269(1148,50,0,100,0,.25f,.5f,false,false,true,false,p)&&p.action==FusionUmbrella269Action::Garlic);
    CHECK(TryPlanFusionUmbrella269(1148,50,0,100,0,.25f,.5f,false,false,true,true,p)&&p.kind==41&&p.retireOriginal);
    CHECK(TryPlanFusionUmbrella269(916,49,0,100,0,.25f,.5f,true,false,true,false,p)&&p.damage==1000000&&p.coins==3&&!p.retireOriginal);
    CHECK(TryPlanFusionUmbrella269(923,50,0,100,0,.25f,.5f,false,true,true,false,p)&&p.coins==1&&p.retireOriginal);
    const auto savedPlan=p;CHECK(!TryPlanFusionUmbrella269(916,nan,0,100,0,.25f,.5f,false,false,true,false,p)&&p==savedPlan);
    int coin=0,count=0;CHECK(TryGetFusionMoney269Reward(41,coin,count)&&coin==36&&count==1);
    CHECK(TryGetFusionMoney269Reward(42,coin,count)&&coin==36&&count==3);
    float hp=99;CHECK(TryStepFusionMoney269Health(41,200,200,15,.01f,hp)&&hp==200);
    CHECK(TryStepFusionMoney269Health(41,200,200,std::nextafter(15.f,16.f),.01f,hp)&&hp==199.8f);
    CHECK(!TryStepFusionMoney269Health(0,200,200,16,.01f,hp));
    uint16_t payload=0;int damage=0;CHECK(TryGetFusionHypno269Projectile(1026,20,5,payload,damage)&&payload==14&&damage==20);
    CHECK(TryGetFusionHypno269Projectile(924,20,5,payload,damage)&&payload==14&&damage==25);
    float threshold=99;CHECK(!TryGetFusionPuffLove269Threshold(61,0,100,0,false,threshold)&&threshold==99);
    CHECK(TryGetFusionPuffLove269Threshold(0,0,100,0,false,threshold)&&threshold==.75f);
    CHECK(TryGetFusionPuffLove269Threshold(60,0,100,0,false,threshold)&&threshold==1);
    CHECK(TryGetFusionPuffLove269Threshold(81,0,100,0,true,threshold)&&threshold==std::sqrt(std::sqrt(std::sqrt(.81f))));
    int status=99;float clock=99;CHECK(TryUseFusionUltimateHypno269Item(0,5,status,clock)&&status==4&&clock==35);
    CHECK(TryUseFusionUltimateHypno269Item(4,35,status,clock)&&status==5&&clock==65);
    CHECK(TryUseFusionUltimateHypno269Item(5,65,status,clock)&&status==5&&clock==95);
    CHECK(!TryUseFusionUltimateHypno269Item(2,65,status,clock)&&status==5&&clock==95);
    CHECK(TryUseFusionUltimateHypno269Item(0,-1,status,clock)&&status==4&&clock==29);
    // MC is computed after the single AoE hit. A dead negative float health
    // preserves source sqrt-NaN comparison: consume the draw, never control.
    CHECK(TryGetFusionPuffLove269Threshold(-1,0,100,0,false,threshold)&&std::isnan(threshold));
    CHECK(TryGetFusionPuffLove269Threshold(100,100,100,100,true,threshold)&&threshold==1);
    CHECK(TryGetFusionPuffLove269Threshold(0,20,100,100,false,threshold)&&threshold==.75f);
    CHECK(!TryGetFusionPuffLove269Threshold(1,0,0,0,false,threshold));
    for(uint16_t kind:{uint16_t(924),uint16_t(1026)})
    {
        const auto* timing=FindFusionHypno269TimingProfile(kind);
        CHECK(timing&&FusionShooterTimingProfileValid(*timing)&&timing->stateSpeed==2&&timing->speedParameterActive);
        CHECK(timing&&timing->eventClipSeconds.size()==1&&timing->eventClipSeconds[0]==double(.75f));
        CHECK(timing&&timing->attackIntervalSeconds==(kind==924?2:3));
        CHECK(FusionHypno269PlantStateValid(kind,0,0,kind==924?-100:0,0,-1,1,0));
        CHECK(!FusionHypno269PlantStateValid(kind,1,0,0,0,-1,1,0));
    }
    CHECK(!FindFusionHypno269TimingProfile(1026,4));
    CHECK(!FindFusionHypno269TimingProfile(924,"idle"));
    for(int charge:{0,4,5})
    {
        const auto* timing=FindFusionHypno269TimingProfile(924,charge);
        const int expected=charge==0?1:charge==4?2:4;
        CHECK(timing&&int(timing->eventClipSeconds.size())==expected);
        for(float speed:{.9f,1.f,1.1f})
        {
            int initial=0,observed=0;CHECK(TryGetFusionShooterShotCounter(*timing,speed,initial));
            for(int counter=initial-1;counter>=0;--counter){int due=0;CHECK(TryGetFusionShooterEventsAtCounter(*timing,speed,counter,due));observed+=due;}
            CHECK(observed==expected);
            CHECK(FusionHypno269PlantStateValid(924,0,charge,-100,12,FusionShooterDuration(*timing,speed),speed,4));
            CHECK(!FusionHypno269PlantStateValid(924,0,charge,-100,12,FusionShooterDuration(*timing,speed)+.02f,speed,4));
        }
    }
    CHECK(!FusionHypno269PlantStateValid(924,4,4,-99,0,-1,1,0));
    CHECK(!FusionHypno269PlantStateValid(1026,0,0,-100,0,-1,1,0));
    CHECK(FusionPuffLove269CanHit(0,true,false,false));
    CHECK(!FusionPuffLove269CanHit(0,false,false,false)&&!FusionPuffLove269CanHit(0,true,true,false));
    CHECK(!FusionPuffLove269CanHit(0,true,false,true));
    for(int excluded:{3,7,9,10,12,13,16,20,21})CHECK(!FusionPuffLove269CanHit(excluded,true,false,false));
    for(int excluded:{1,7,10,12,16,20,21,22,23,24})CHECK(!FusionPuffLove269CanSplash(excluded,0,false,false));
    for(int admitted:{0,3,9,13})for(int row:{-1,0,1})CHECK(FusionPuffLove269CanSplash(admitted,row,false,false));
    CHECK(!FusionPuffLove269CanSplash(0,2,false,false)&&!FusionPuffLove269CanSplash(0,-2,false,false));
    CHECK(FusionPuffLove269CircleTouchesRect(0,0,1.5,-.1,.1,.2));
    CHECK(!FusionPuffLove269CircleTouchesRect(0,0,1.50001,-.1,.1,.2));
    CHECK(FusionPuffLove269CircleTouchesRect(0,0,1,1,.1,.1));
    CHECK(!FusionPuffLove269CircleTouchesRect(0,0,1.1,1.1,.1,.1));
    CHECK(FusionHypno269ProjectileMetadataValid(14,80,0)&&FusionHypno269ProjectileMetadataValid(14,100,0));
    CHECK(!FusionHypno269ProjectileMetadataValid(14,79,0)&&!FusionHypno269ProjectileMetadataValid(14,80,6));
    CHECK(IsFusionSummon269BigKind(14)&&!IsFusionSummon269BigKind(15)&&!IsFusionSummon269BigKind(211)&&IsFusionSummon269BigKind(224));
    for(uint16_t kind:{uint16_t(916),uint16_t(923),uint16_t(1148)})
    {
        CHECK(IsFusionUmbrellaBatchKind(kind)&&FusionUmbrellaBatchPlantStateValid(kind,0,750,0,4));
        CHECK(!FusionUmbrellaBatchPlantStateValid(kind,0,751,0,4));
        CHECK(FusionUmbrellaBatchMoneySkillFee(kind)==(kind==1148?3000:1000));
        CHECK(FusionUmbrellaBatchMaximumDuration(kind,1)==1.02f);
    }
    std::printf("[ %s ] Fusion.Summon269 rules checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
