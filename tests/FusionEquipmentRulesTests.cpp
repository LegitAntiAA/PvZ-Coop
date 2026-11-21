#include "../Lawn/Fusion/FusionEquipmentRules.h"
#include <array>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <vector>
// Isolated, not registered/executed by TASK268. Root may call this after267.
int RunFusionEquipmentRulesTests(int& checks)
{
    using namespace LawnFusion;int failures=0;checks=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionEquipmentRulesTests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    CHECK(FusionEquipmentProfileCount()==15); // Original13 plus genuine CherryMagnet1166/BigPumpkin1169.
    for(std::size_t i=0;i<FusionEquipmentProfileCount();++i)
    {const auto* p=FusionEquipmentProfileAt(i);CHECK(p&&FindFusionEquipmentProfile(p->kind)==p&&p->maximumHealth>0);}
    CHECK(!FusionEquipmentProfileAt(15));CHECK(!FindFusionEquipmentProfile(1103));
    CHECK(FindFusionEquipmentProfile(1166)&&FindFusionEquipmentProfile(1166)->family==FusionEquipmentFamily::Capture);
    CHECK(FindFusionEquipmentProfile(1169)&&FindFusionEquipmentProfile(1169)->family==FusionEquipmentFamily::Pumpkin&&FindFusionEquipmentProfile(1169)->maximumHealth==12000);
    for(int type=0;type<=10;++type)CHECK(FusionEquipmentTypeValid(type));
    CHECK(!FusionEquipmentTypeValid(-1));CHECK(!FusionEquipmentTypeValid(11));CHECK(!FusionEquipmentTypeValid(255));
    FusionMagnetCaptureProfile capture;
    CHECK(TryGetFusionMagnetCaptureProfile(25,0,false,2,capture)&&capture.radius==4&&capture.cooldownSeconds==15&&capture.initialSeconds==0);
    CHECK(TryGetFusionMagnetCaptureProfile(1100,1,false,2,capture)&&capture.radius==5&&capture.cooldownSeconds==11&&capture.rowRange==3&&!capture.lightBurst);
    CHECK(TryGetFusionMagnetCaptureProfile(1100,2,false,3,capture)&&capture.radius==6&&capture.cooldownSeconds==7.5f&&capture.rowRange==4&&capture.lightBurst);
    CHECK(TryGetFusionMagnetCaptureProfile(1100,3,false,4,capture)&&capture.radius==7&&capture.cooldownSeconds==5&&capture.rowRange==5);
    CHECK(TryGetFusionMagnetCaptureProfile(1100,0,false,5,capture)&&capture.radius==2&&capture.cooldownSeconds==15&&capture.rowRange==5&&!capture.lightBurst);
    CHECK(TryGetFusionMagnetCaptureProfile(1101,0,true,2,capture)&&capture.waterOnly&&capture.cooldownSeconds==10);
    CHECK(TryGetFusionMagnetCaptureProfile(1102,0,false,2,capture)&&capture.wholeWorld&&capture.initialSeconds==.5f&&capture.cooldownSeconds==1&&!capture.dropHeldOnDeath);
    CHECK(!TryGetFusionMagnetCaptureProfile(1092,0,false,2,capture));CHECK(!TryGetFusionMagnetCaptureProfile(1100,-1,false,2,capture));
    for(uint16_t kind:{25,246,247,909,911,920,922,924,935,1092,1093,1094,1095,1096,1097,1098,1099,1100,1101,1102,1103,1104,1154,1157,1158,1159,1160,1166,1169})CHECK(FusionEquipmentMagnetMember(kind));
    for(uint16_t kind:{26,245,248,910,921,923,1027,1028,1029,1091,1105,1155,1156,1161,1170})CHECK(!FusionEquipmentMagnetMember(kind));
    CHECK(FusionEquipmentGraphAdjacent(0,0,1,1));CHECK(!FusionEquipmentGraphAdjacent(0,0,2,1));
    CHECK(!FusionEquipmentGraphAdjacent((std::numeric_limits<int>::min)(),0,(std::numeric_limits<int>::max)(),0));
    const FusionMagnetGraphNode graph[]={{25,0,0},{1092,1,1},{1154,2,2},{1027,3,3},{1101,4,4},{25,3,3,false}};
    std::vector<uint32_t> sizes;CHECK(TryGetFusionMagnetComponentSizes(graph,6,sizes));
    CHECK((sizes==std::vector<uint32_t>{3,3,3,0,1,0}));
    const FusionMagnetGraphNode moved[]={{25,0,0},{1092,3,1},{1154,2,2}};
    CHECK(TryGetFusionMagnetComponentSizes(moved,3,sizes));CHECK((sizes==std::vector<uint32_t>{1,2,2}));
    const auto previous=sizes;CHECK(!TryGetFusionMagnetComponentSizes(nullptr,1,sizes)&&sizes==previous);
    FusionEquipmentCapturePlan plan;
    CHECK(TryGetFusionEquipmentArmorCapture(0,2,plan)&&plan.type==FusionEquipmentType::Bucket);
    CHECK(TryGetFusionEquipmentArmorCapture(0,4,plan)&&plan.type==FusionEquipmentType::Helmet);
    CHECK(TryGetFusionEquipmentArmorCapture(0,7,plan)&&plan.preserveReturnedBucket&&plan.type==FusionEquipmentType::None);
    CHECK(TryGetFusionEquipmentArmorCapture(1,1,plan)&&plan.commit==FusionEquipmentCaptureCommit::Door&&plan.type==FusionEquipmentType::Door);
    CHECK(TryGetFusionEquipmentArmorCapture(1,3,plan)&&plan.commit==FusionEquipmentCaptureCommit::Ladder);
    CHECK(TryGetFusionEquipmentArmorCapture(2,1,plan)&&plan.commit==FusionEquipmentCaptureCommit::Jackbox);
    CHECK(TryGetFusionEquipmentArmorCapture(2,2,plan)&&plan.commit==FusionEquipmentCaptureCommit::Pogo);
    CHECK(TryGetFusionEquipmentArmorCapture(2,5,plan)&&plan.type==FusionEquipmentType::RedIronHead);
    CHECK(!TryGetFusionEquipmentArmorCapture(0,1,plan));CHECK(!TryGetFusionEquipmentArmorCapture(3,2,plan));
    int prefab=777;const int expectedPrefabs[]={4,6,7,8,-1,-1,17,42,30,31,5};
    for(int type=0;type<=10;++type)
    {prefab=777;const bool valid=TryGetFusionEquipmentPrefabIndex(type,prefab);CHECK(valid==(expectedPrefabs[type]>=0));CHECK(prefab==(valid?expectedPrefabs[type]:777));}
    CHECK(FusionMagnetPumpkinMetalTarget(2,0,0,0));CHECK(FusionMagnetPumpkinMetalTarget(0,0,3,0));
    CHECK(FusionMagnetPumpkinMetalTarget(0,0,0,108));CHECK(!FusionMagnetPumpkinMetalTarget(0,0,0,119));
    CHECK(!FusionMagnetPumpkinMetalTarget(0,0,0,15));CHECK(!FusionMagnetPumpkinMetalTarget(0,0,4,0));
    int damage=777;
    CHECK(TryGetFusionMagnetPumpkinDamage(20,1,.075f,false,damage)&&damage==1);
    CHECK(TryGetFusionMagnetPumpkinDamage(20,1,.075f,true,damage)&&damage==2); // Doubling before truncation would incorrectly give3.
    CHECK(TryGetFusionMagnetPumpkinDamage(20,3,1.5f,true,damage)&&damage==180);
    CHECK(TryGetFusionMagnetPumpkinDamage(0,3,1.5f,true,damage)&&damage==0);
    damage=777;CHECK(!TryGetFusionMagnetPumpkinDamage(20,0,1,false,damage)&&damage==777);
    CHECK(!TryGetFusionMagnetPumpkinDamage((std::numeric_limits<int>::max)(),2,1,false,damage));
    CHECK(!TryGetFusionMagnetPumpkinDamage(20,1,(std::numeric_limits<float>::quiet_NaN)(),false,damage));
    FusionBucketReceiverEffect effect;
    CHECK(TryGetFusionBucketReceiverEffect(1027,1,8000,8000,effect)&&effect.replacementKind==1028);
    CHECK(!TryGetFusionBucketReceiverEffect(1027,0,7000,8000,effect));
    for(int type=0;type<=10;++type)
    {
        CHECK(TryGetFusionBucketReceiverEffect(1028,type,16000,16000,effect)&&effect.recoverHealth==500&&effect.replacementKind==65535);
        CHECK(TryGetFusionBucketReceiverEffect(1154,type,300,300,effect)&&effect.sunValue==25);
        CHECK(TryGetFusionBucketReceiverEffect(1097,type,8000,8000,effect)&&effect.recoverHealth==500);
    }
    for(const auto expected:{std::array<int,2>{0,1097},std::array<int,2>{2,1098},std::array<int,2>{3,1099}})
        CHECK(TryGetFusionBucketReceiverEffect(1092,expected[0],4000,4000,effect)&&effect.replacementKind==expected[1]);
    CHECK(!TryGetFusionBucketReceiverEffect(1092,1,4000,4000,effect));
    CHECK(TryGetFusionBucketReceiverEffect(1092,1,3999,4000,effect)&&effect.recoverHealth==500);
    CHECK(!TryGetFusionBucketReceiverEffect(1092,11,3999,4000,effect));CHECK(!TryGetFusionBucketReceiverEffect(1092,1,0,4000,effect));
    uint16_t receiver=0;int recover=0;
    CHECK(TryGetFusionBucketDeathRecovery(0,3,4,receiver,recover)&&receiver==1151&&recover==2666);
    CHECK(TryGetFusionBucketDeathRecovery(5,0,3,receiver,recover)&&receiver==1129&&recover==333);
    CHECK(!TryGetFusionBucketDeathRecovery(2,3,4,receiver,recover));
    bool flash=false,expire=false;
    CHECK(TryGetFusionBucketLifetime(4,true,false,false,false,flash,expire)&&!flash&&!expire);
    CHECK(TryGetFusionBucketLifetime(8,true,false,false,false,flash,expire)&&flash&&!expire);
    CHECK(TryGetFusionBucketLifetime(8.01f,true,false,true,false,flash,expire)&&flash&&!expire);
    CHECK(TryGetFusionBucketLifetime(8.01f,true,false,false,true,flash,expire)&&!flash&&expire);
    CHECK(TryGetFusionBucketLifetime(9,true,true,false,false,flash,expire)&&!flash&&!expire);
    std::array<int,11> weighted{};int item=0;FusionEquipmentType type=FusionEquipmentType::None;
    for(int draw=0;draw<10;++draw){CHECK(TryGetFusionSunMagnetWeightedItem(draw,item,type));++weighted[int(type)];}
    CHECK(weighted[0]==3&&weighted[1]==2&&weighted[3]==2&&weighted[2]==2&&weighted[4]==1);
    CHECK(item==41&&type==FusionEquipmentType::Machine);CHECK(!TryGetFusionSunMagnetWeightedItem(10,item,type));
    float seconds=0;CHECK(TryGetFusionSunMagnetPeriodicSeconds(-2,seconds)&&seconds==23);
    CHECK(TryGetFusionSunMagnetPeriodicSeconds(2,seconds)&&seconds==27);CHECK(!TryGetFusionSunMagnetPeriodicSeconds(3,seconds));
    CHECK(kFusionSunMagnetInitialAttributeSeconds==10&&kFusionSunMagnetAttributeSeconds==30&&kFusionSunMagnetCallbackSeconds==.5f);
#undef CHECK
    std::printf("FusionEquipmentRulesTests checks=%d failures=%d\n",checks,failures);return failures;
}
