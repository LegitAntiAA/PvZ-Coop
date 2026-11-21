#include "FusionBatchActionTests.h"
#include "../../Lawn/Fusion/FusionBatchActionRules.h"
#include "../../Lawn/Fusion/FusionColdRules.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionBatchActionTestResult result;
void Check(bool ok,int line)
{
    ++result.mChecks;
    if (!ok) { ++result.mFailures; std::printf("    BatchAction failure line=%d\n",line); }
}
#define CHECK(x) Check(!!(x),__LINE__)
}
FusionBatchActionTestResult RunFusionBatchActionTests()
{
    using namespace LawnFusion;
    result={}; std::printf("[ RUN      ] Fusion.SourceBatchInstantPanicRanged\n");
    for (uint16_t kind:{8,1022,11,16,13,1057,17,1155,1180,14,1058,9,1024,1025}) CHECK(FindFusionBatchActionProfile(kind));
    for (uint16_t kind:{0,1,2,7,10,1000,65535}) CHECK(!FindFusionBatchActionProfile(kind));
    CHECK(FusionBatchDamageFlags(17)==1U && FusionBatchDamageFlags(1155)==1U);
    const auto* iceCaltrop=FindFusionCaltropStrikeProfile(1180);
    CHECK(iceCaltrop && iceCaltrop->damage==20 && iceCaltrop->coldLevelsAfterDamage==10 &&
        !iceCaltrop->warmAfterDamage && !iceCaltrop->rock && FusionBatchDamageFlags(1180)==1U);
    CHECK(FindFusionCaltropStrikeProfile(1155) && !FindFusionCaltropStrikeProfile(1155)->warmAfterDamage);
    for (uint16_t kind:{1060,1074,1075})
    {
        CHECK(FindFusionCaltropStrikeProfile(kind) && FindFusionCaltropStrikeProfile(kind)->rock);
        CHECK(FindFusionBatchActionProfile(kind) && FindFusionBatchTimingProfile(kind));
        CHECK(FusionBatchDamageFlags(kind)==1U);
    }
    CHECK(FindFusionCaltropStrikeProfile(1060)->damage==20 && FindFusionCaltropStrikeProfile(1060)->coldLevelsAfterDamage==0);
    CHECK(FindFusionCaltropStrikeProfile(1074)->damage==20 && FindFusionCaltropStrikeProfile(1074)->coldLevelsAfterDamage==5);
    CHECK(FindFusionCaltropStrikeProfile(1075)->damage==40 && FindFusionCaltropStrikeProfile(1075)->warmAfterDamage);
    CHECK(!FindFusionCaltropStrikeProfile(10) && !FindFusionCaltropStrikeProfile(65535));
    const auto* iceAttack=FindFusionBatchTimingProfile(1180);
    CHECK(iceAttack && iceAttack->attackIntervalSeconds==1 && iceAttack->eventClipSeconds.size()==1 &&
        iceAttack->eventClipSeconds[0]==static_cast<double>(.33333334f) && iceAttack->stateSpeed==1 &&
        iceAttack->clipDurationSeconds==static_cast<double>(.6666667f) && iceAttack->speedParameterActive);
    int iceCounter=0,events=0;
    CHECK(iceAttack && TryGetFusionShooterShotCounter(*iceAttack,.9f,iceCounter) && iceCounter==39);
    CHECK(iceAttack && TryGetFusionShooterShotCounter(*iceAttack,1,iceCounter) && iceCounter==35);
    CHECK(iceAttack && TryGetFusionShooterShotCounter(*iceAttack,1.1f,iceCounter) && iceCounter==32);
    CHECK(iceAttack && TryGetFusionShooterEventsAtCounter(*iceAttack,1,1,events) && events==1);
    CHECK(iceAttack && TryGetFusionShooterEventsAtCounter(*iceAttack,1,2,events) && events==0);
    // Source IceCaltrop calls TakeDamage5(20) before AddfreezeLevel10. Unlike
    // SnowPea it never multiplies that damage, even if the target was frozen.
    FusionColdState cold{90,100,1400,0},slowed,after;
    CHECK(iceCaltrop && iceCaltrop->damage==20 && TrySetFusionColdSlow(cold,slowed) && slowed.chilledTicks==1000);
    CHECK(TryAddFusionColdLevels(slowed,iceCaltrop->coldLevelsAfterDamage,after) &&
        after.level==0 && after.threshold==200 && after.frozenTicks==400 && after.chilledTicks==1400);
    cold={95,100,1400,123};
    CHECK(iceCaltrop->damage==20 && TrySetFusionColdSlow(cold,slowed) &&
        TryAddFusionColdLevels(slowed,iceCaltrop->coldLevelsAfterDamage,after) &&
        after.level==105 && after.threshold==100 && after.frozenTicks==123 && after.chilledTicks==1000);
    CHECK(FusionBatchDamageFlags(13)==18U && FusionBatchDamageFlags(1057)==18U);
    CHECK(FusionBatchDamageFlags(1025)==2U && FusionBatchDamageFlags(9)==0U);
    int ticks=77;
    CHECK(TryGetFusionBatchInstantTicks(11,1,ticks) && ticks==96);
    CHECK(TryGetFusionBatchInstantTicks(16,.9f,ticks) && ticks==84);
    CHECK(TryGetFusionBatchInstantTicks(16,1.1f,ticks) && ticks==84); // No Speed parameter.
    CHECK(!TryGetFusionBatchInstantTicks(13,1,ticks) && ticks==84);
    CHECK(!TryGetFusionBatchInstantTicks(11,0,ticks) && ticks==84);
    CHECK(!TryGetFusionBatchInstantTicks(11,(std::numeric_limits<float>::quiet_NaN)(),ticks) && ticks==84);
    CHECK(TryGetFusionCaltropCycleTicks(0,ticks) && ticks==100);
    CHECK(TryGetFusionCaltropCycleTicks(-.1f,ticks) && ticks==90);
    const int saved=ticks;
    CHECK(!TryGetFusionCaltropCycleTicks(-.11f,ticks) && ticks==saved);

    uint8_t mask=0; bool control=false;
    for (uint8_t color=0;color<7;++color)
    {
        uint8_t next=255;
        CHECK(TryAdvanceFusionPuffColors(mask,color,next,control) && next==(mask|(1u<<color)) && control==(color==6));
        mask=next;
        CHECK(TryAdvanceFusionPuffColors(mask,color,next,control) && next==mask && control==(color==6));
    }
    uint8_t untouched=81; control=true;
    CHECK(!TryAdvanceFusionPuffColors(128,0,untouched,control) && untouched==81 && control);
    CHECK(!TryAdvanceFusionPuffColors(0,7,untouched,control) && untouched==81 && control);
    std::array<int,3> rows{77,77,77}; size_t count=77;
    CHECK((TryGetFusionBatchEntryRows(1058,2,5,rows,count) && count==3 && rows==std::array<int,3>{1,3,2}));
    CHECK(TryGetFusionBatchEntryRows(1058,0,5,rows,count) && count==2 && rows[0]==1 && rows[1]==0);
    CHECK(TryGetFusionBatchEntryRows(1058,4,5,rows,count) && count==2 && rows[0]==3 && rows[1]==4);
    CHECK(TryGetFusionBatchEntryRows(1058,0,1,rows,count) && count==1 && rows[0]==0);
    const auto oldRows=rows;
    CHECK(!TryGetFusionBatchEntryRows(14,0,5,rows,count) && rows==oldRows && count==1);
    CHECK(!TryGetFusionBatchEntryRows(1058,5,5,rows,count) && rows==oldRows && count==1);
    for (int color=0;color<7;++color) CHECK(FusionBatchProjectileMetadataValid(13,20,0,0,static_cast<uint8_t>(color),0));
    CHECK(!FusionBatchProjectileMetadataValid(13,20,0,0,7,0));
    CHECK(!FusionBatchProjectileMetadataValid(13,40,0,0,0,0));
    CHECK(!FusionBatchProjectileMetadataValid(13,20,5,0,0,0));
    CHECK(!FusionBatchProjectileMetadataValid(13,20,0,1,0,0));
    CHECK(!FusionBatchProjectileMetadataValid(13,20,0,0,0,3));
    CHECK(FusionBatchProjectileMetadataValid(9,20,0,0,0,0)); // Long-range Puff has no75tick deadline.
    CHECK(!FusionBatchProjectileMetadataValid(9,20,0,0,0,3));
    for (uint8_t phase:{kFusionThreeUpper,kFusionThreeLower})
    {
        CHECK(FusionBatchProjectileMetadataValid(0,20,0,phase,0,2));
        CHECK(!FusionBatchProjectileMetadataValid(0,20,0,phase,1,2));
        CHECK(!FusionBatchProjectileMetadataValid(0,20,0,phase,0,0));
    }
    CHECK(!FusionBatchProjectileMetadataValid(0,20,0,0,0,2));
    float dy=77;
    CHECK(TryGetFusionThreeVerticalStep(kFusionThreeUpper,0,dy) && std::abs(dy+7*100.f/1.67f*.01f)<1e-5f);
    CHECK(TryGetFusionThreeVerticalStep(kFusionThreeLower,0,dy) && dy>0);
    CHECK(TryGetFusionThreeVerticalStep(kFusionThreeLower,45,dy) && dy<0); // The source curve crosses0 before.5; do not clamp its sign.
    CHECK(TryGetFusionThreeVerticalStep(kFusionThreeLower,50,dy) && dy==0);
    dy=77; CHECK(!TryGetFusionThreeVerticalStep(1,0,dy) && dy==77);
    CHECK(!TryGetFusionThreeVerticalStep(kFusionThreeLower,-1,dy) && dy==77);

    CHECK(FusionBatchShooterCanAcquire(14,0,-1,false,false,1,10));
    CHECK(FusionBatchShooterCanAcquire(14,0,1,false,false,1,10));
    CHECK(!FusionBatchShooterCanAcquire(14,0,2,false,false,1,10));
    CHECK(!FusionBatchShooterCanAcquire(9,0,1,false,false,1,10));
    CHECK(FusionBatchShooterCanAcquire(9,0,0,false,false,1,10));
    CHECK(!FusionBatchShooterCanAcquire(9,0,0,false,false,0,10));
    CHECK(!FusionBatchShooterCanAcquire(9,0,0,false,false,10,10));
    CHECK(!FusionBatchShooterCanAcquire(9,0,0,true,false,1,10));
    CHECK(!FusionBatchShooterCanAcquire(9,0,0,false,true,1,10));
    CHECK(!FusionBatchShooterCanAcquire(9,-1,0,false,false,1,10));
    FusionBatchArea area;
    CHECK(TryMakeFusionBatchArea(17,100,200,true,area));
    CHECK(FusionBatchAreaTouchesRect(area,100,200,0,0));
    CHECK(!FusionBatchAreaTouchesRect(area,200,200,1,1));
    const auto plainCaltropArea=area;
    CHECK(TryMakeFusionBatchArea(1180,100,200,true,area) && area.x==plainCaltropArea.x &&
        area.y==plainCaltropArea.y && area.width==plainCaltropArea.width && area.height==plainCaltropArea.height);
    CHECK(TryMakeFusionBatchArea(13,100,200,false,area) && area.width>150 && area.height>175);
    CHECK(TryMakeFusionBatchArea(13,100,200,true,area) && area.width<54 && area.height>175);
    CHECK(TryMakeFusionBatchArea(9,100,200,false,area));
    const auto old=area;
    CHECK(!TryMakeFusionBatchArea(14,100,200,false,area) && area.x==old.x && area.y==old.y);
    CHECK(FusionDoomTouchesRect(0,0,0,0,0,0));
    CHECK(FusionDoomTouchesRect(0,0,5*72/1.35,0,0,0));
    CHECK(!FusionDoomTouchesRect(0,0,5.01*72/1.35,0,0,0));
    CHECK(!FusionDoomTouchesRect(0,0,0,0,-1,1));
    const auto* close=FindFusionBatchTimingProfile(1025,true);
    const auto* far=FindFusionBatchTimingProfile(1025,false);
    CHECK(close && far && close->eventClipSeconds.size()==1 && far->eventClipSeconds.size()==2);
    CHECK(close->projectileKind==65535 && far->projectileKind==9 && close->stateSpeed==1 && far->stateSpeed==2);
    CHECK(!FindFusionBatchTimingProfile(1057));
    for (uint16_t kind:{1036,1038,1040,1042,1043,1044,1045,1046,1171,1054,1062,1063,1064}) CHECK(FindFusionBatchActionProfile(kind));
    CHECK(!FindFusionBatchActionProfile(1041) && !FindFusionBatchActionProfile(1172)); // Explicit NPC/poison blockers, not aliases.
    CHECK(FusionBatchPlantStateValid(1042,14,0,0,0));
    CHECK(!FusionBatchPlantStateValid(1042,15,0,0,0));
    CHECK(!FusionBatchPlantStateValid(1042,0,1,0,0));
    CHECK(!FusionBatchPlantStateValid(1042,0,0,1,0));
    CHECK(!FusionBatchPlantStateValid(1042,0,0,0,1));
    CHECK(!FusionBatchPlantStateValid(1038,1,0,0,0));
    CHECK(FusionBatchPlantStateValid(1038,0,0,0,0));
    CHECK(TryGetFusionBatchInstantTicks(1040,1,ticks) && ticks==96);
    CHECK(TryGetFusionCaltropCycleTicks(1062,0,ticks) && ticks==100);
    CHECK(TryGetFusionCaltropCycleTicks(1063,0,ticks) && ticks==150);
    CHECK(TryGetFusionCaltropCycleTicks(1064,0,ticks) && ticks==150);
    const int cycleSentinel=ticks;
    CHECK(!TryGetFusionCaltropCycleTicks(1043,0,ticks) && ticks==cycleSentinel);
    const auto* threeSpike=FindFusionBatchTimingProfile(1064);
    CHECK(threeSpike && threeSpike->eventClipSeconds[0]==.5 && threeSpike->clipDurationSeconds==1 && threeSpike->attackIntervalSeconds==1.5f);
    CHECK(FindFusionCaltropStrikeProfile(1064)->damage==5 && FusionBatchDamageFlags(1063)==1U);
    float interval=77;
    CHECK(TryGetFusionScaredyDoomInterval(1,interval) && interval==1.399999976158142f);
    CHECK(TryGetFusionScaredyDoomInterval(13,interval) && interval==.19999980926513672f && interval!=.2f);
    CHECK(TryGetFusionScaredyDoomInterval(14,interval) && interval==.2f);
    uint32_t shots=88; bool blast=true;
    CHECK(TryAdvanceFusionScaredyDoom(13,true,shots,blast) && shots==0 && !blast); // The below-.2 float is not exact.2.
    CHECK(TryAdvanceFusionScaredyDoom(13,false,shots,blast) && shots==14 && !blast);
    CHECK(TryAdvanceFusionScaredyDoom(14,true,shots,blast) && shots==14 && blast);
    interval=77; shots=88;
    CHECK(!TryGetFusionScaredyDoomInterval(15,interval) && interval==77);
    CHECK(!TryAdvanceFusionScaredyDoom(15,true,shots,blast) && shots==88 && blast);
    const auto* doomScared=FindFusionBatchTimingProfile(1042);
    CHECK(doomScared && !doomScared->speedParameterActive && doomScared->projectileKind==22);
    int slowCounter=0,fastCounter=0;
    CHECK(TryGetFusionShooterShotCounter(*doomScared,.9f,slowCounter) && TryGetFusionShooterShotCounter(*doomScared,1.1f,fastCounter) && slowCounter==32 && fastCounter==32);
    const auto* manual=FindFusionBatchTimingProfile(1043);
    CHECK(manual && manual->attackIntervalSeconds==60 && manual->clipDurationSeconds==static_cast<double>(1.55f));
    const auto* forward=FindFusionBatchTimingProfile(1046);
    CHECK(forward && forward->projectileKind==24 && forward->eventClipSeconds.size()==4 && forward->eventClipSeconds[3]==static_cast<double>(.8333333f));
    CHECK(FusionBatchShooterCanAcquire(1046,0,0,false,false,10000,1)); // Source override has no vision upper bound.
    CHECK(!FusionBatchShooterCanAcquire(1046,0,0,false,false,0,10000));
    CHECK(!FusionBatchShooterCanAcquire(1046,0,1,false,false,1,10000));
    FusionScaredyFearTiming fear;
    CHECK(TryGetFusionScaredyFearTiming(1038,1,fear) && fear.lowering==100 && fear.event==84 && fear.raising==102);
    CHECK(TryGetFusionScaredyFearTiming(1038,.9f,fear) && fear.lowering==112 && fear.event==93 && fear.raising==113);
    const auto savedFear=fear;
    CHECK(!TryGetFusionScaredyFearTiming(9,1,fear) && fear.event==savedFear.event);
    FusionBatchArea freezeArea;
    CHECK(TryMakeFusionBatchArea(1038,0,0,true,freezeArea) && std::abs(freezeArea.width-3*kFusionBasePixelsX)<1e-9 && std::abs(freezeArea.y+2.5*kFusionBasePixelsY)<1e-9);
    CHECK(FusionBatchCircleTouchesRect(0,0,1,kFusionBasePixelsX,0,0,0));
    CHECK(!FusionBatchCircleTouchesRect(0,0,1,kFusionBasePixelsX+.001,0,0,0));
    CHECK(TryMakeFusionDoomPuffTrigger(0,0,0,freezeArea) && std::abs(freezeArea.width-.861872295*kFusionBasePixelsX)<1e-9);
    CHECK(TryMakeFusionDoomPuffTrigger(1,0,0,freezeArea) && std::abs(freezeArea.height-.7765878*kFusionBasePixelsY)<1e-9);
    const auto savedArea=freezeArea;
    CHECK(!TryMakeFusionDoomPuffTrigger(2,0,0,freezeArea) && freezeArea.x==savedArea.x);
    std::printf("[ %s ] Fusion.SourceBatchInstantPanicRanged checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
