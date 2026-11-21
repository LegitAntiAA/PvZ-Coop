#include "../Lawn/Fusion/FusionExtras268Rules.h"
#include <cstdio>
#include <limits>
#include <string>
int RunFusionExtras268RulesTests(int& checks)
{
    using namespace LawnFusion;int failures=0;checks=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionExtras268RulesTests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    const auto* profile=FindFusionExtras268DefenderProfile(1129);
    CHECK(profile&&profile->maxHealth==4000&&profile->sunCost==100&&profile->rechargeTicks==3000);
    CHECK(!FindFusionExtras268DefenderProfile(1162));CHECK(IsFusionExtras268Kind(1162));CHECK(!IsFusionExtras268Kind(1151));
    int value=-1;
    CHECK(TryGetFusionMachine268Damage(0,value)&&value==0);CHECK(TryGetFusionMachine268Damage(3999,value)&&value==3999);
    CHECK(TryGetFusionMachine268Damage(4000,value)&&value==4000);CHECK(TryGetFusionMachine268Damage(16000,value)&&value==4000);
    CHECK(!TryGetFusionMachine268Damage(-1,value));
    CHECK(TryGetFusionMachine268ProtectionDamage(1,value)&&value==2);CHECK(TryGetFusionMachine268ProtectionDamage(1999,value)&&value==3998);
    CHECK(TryGetFusionMachine268ProtectionDamage(2000,value)&&value==4000);CHECK(TryGetFusionMachine268ProtectionDamage((std::numeric_limits<int>::max)(),value)&&value==4000);
    CHECK(TryGetFusionMachine268Recovery(4000,3000,value)&&value==7000);CHECK(TryGetFusionMachine268Recovery(15999,500,value)&&value==16000);
    CHECK(TryGetFusionMachine268Recovery(16000,500,value)&&value==16000);CHECK(!TryGetFusionMachine268Recovery(0,500,value));
    CHECK(!TryGetFusionMachine268Recovery(16001,0,value));CHECK(!TryGetFusionMachine268Recovery(4000,-1,value));
    CHECK(TryGetFusionMachine268Transition(0,1,value)&&value==3000);CHECK(TryGetFusionMachine268Transition(1,1,value)&&value==0);
    CHECK(TryGetFusionMachine268Transition(1,2,value)&&value==3000);CHECK(TryGetFusionMachine268Transition(2,1,value)&&value==3000);
    CHECK(!TryGetFusionMachine268Transition(0,0,value));CHECK(!TryGetFusionMachine268Transition(3,1,value));
    FusionBucketReceiverEffect effect;
    CHECK(TryGetFusionExtras268BucketEffect(3,FusionEquipmentType::Machine,4000,effect)&&effect.replacementKind==1129);
    CHECK(!TryGetFusionExtras268BucketEffect(3,FusionEquipmentType::SuperMachine,4000,effect));
    for(int type=0;type<=10;++type)CHECK(TryGetFusionExtras268BucketEffect(1129,FusionEquipmentType(type),16000,effect)&&effect.recoverHealth==500&&effect.replacementKind==65535);
    CHECK(!TryGetFusionExtras268BucketEffect(1129,FusionEquipmentType::None,4000,effect));
    CHECK(FusionExtras268SnapshotValid(1129,16000,4000,2,2));CHECK(!FusionExtras268SnapshotValid(1129,16001,4000,2,2));
    CHECK(!FusionExtras268SnapshotValid(1129,4000,16000,0,0));CHECK(!FusionExtras268SnapshotValid(1129,4000,4000,3,0));
    CHECK(!FusionExtras268SnapshotValid(1129,4000,4000,0,3));CHECK(FusionExtras268SnapshotValid(1162,300,300,0,0));
    CHECK(!FusionExtras268SnapshotValid(1162,301,300,0,0));CHECK(!FusionExtras268SnapshotValid(1162,300,300,0,1));
    CHECK(!FusionExtras268SnapshotValid(1162,300,300,1,0));CHECK(!FusionExtras268SnapshotValid(1151,4000,4000,0,0));
    CHECK(FusionExtras268PlantStateValid(1129,0,0,0,2,2,0,0,-1));CHECK(FusionExtras268PlantStateValid(1162,0,0,0,0,0,0,0,-1));
    CHECK(!FusionExtras268PlantStateValid(1162,1,0,0,0,0,0,0,-1));CHECK(!FusionExtras268PlantStateValid(1162,0,1,0,0,0,0,0,-1));
    CHECK(!FusionExtras268PlantStateValid(1129,0,0,1,0,0,0,0,-1));CHECK(!FusionExtras268PlantStateValid(1129,0,0,0,0,0,1,0,-1));
    CHECK(!FusionExtras268PlantStateValid(1162,0,0,0,0,0,0,1,-1));CHECK(!FusionExtras268PlantStateValid(1162,0,0,0,0,0,0,0,0));
    CHECK(std::string(FusionMachine268FormNode(0,1))=="Wallnut_body/yuan1_1");
    CHECK(std::string(FusionMachine268FormNode(2,2))=="Wallnut_cracked2/yuan3_2");CHECK(!FusionMachine268FormNode(3,0));CHECK(!FusionMachine268FormNode(0,3));
#undef CHECK
    std::printf("FusionExtras268RulesTests checks=%d failures=%d\n",checks,failures);return failures;
}
