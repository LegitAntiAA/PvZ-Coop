#include "FusionExtras268Rules.h"
#include <algorithm>
#include <cstdint>
namespace LawnFusion
{
namespace
{
constexpr FusionDefenderProfile machine={1129,FusionDefenderFamily::WallNut,FusionDefenderLayer::Body,
    4000,100,3000,FusionDefenderBite::None,{"Wallnut_body","Wallnut_cracked1","Wallnut_cracked2"},kDefenderPendingNone};
constexpr const char* forms[3][3]={
    {"Wallnut_body/yuan1","Wallnut_body/yuan1_1","Wallnut_body/yuan1_2"},
    {"Wallnut_cracked1/yuan2","Wallnut_cracked1/yuan2_1","Wallnut_cracked1/yuan2_2"},
    {"Wallnut_cracked2/yuan3","Wallnut_cracked2/yuan3_1","Wallnut_cracked2/yuan3_2"}};
}
bool IsFusionExtras268Kind(uint16_t kind){return kind==1129||kind==1162;}
const FusionDefenderProfile* FindFusionExtras268DefenderProfile(uint16_t kind){return kind==1129?&machine:nullptr;}
bool FusionExtras268SnapshotValid(uint16_t kind,int health,int maximum,uint8_t stage,uint8_t mode)
{
    if(health<0)return false;
    if(kind==1129)return maximum==4000&&health<=16000&&stage<=2&&mode<=2;
    return kind==1162&&maximum==300&&health<=300&&stage==0&&mode==0;
}
bool FusionExtras268PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,
    uint8_t mode,uint8_t stage,int productionTicks,int pendingTicks,float shootSeconds)
{
    if(shots||rage||bag||productionTicks||pendingTicks||shootSeconds!=-1)return false;
    return kind==1129?(mode<=2&&stage<=2):kind==1162&&mode==0&&stage==0;
}
bool TryGetFusionMachine268Damage(int incoming,int& applied)
{if(incoming<0)return false;applied=(std::min)(incoming,4000);return true;}
bool TryGetFusionMachine268ProtectionDamage(int incoming,int& applied)
{if(incoming<0)return false;applied=int((std::min)(int64_t(incoming)*2,int64_t(4000)));return true;}
bool TryGetFusionMachine268Recovery(int health,int amount,int& after)
{if(health<=0||health>16000||amount<0)return false;after=int((std::min)(int64_t(health)+amount,int64_t(16000)));return true;}
bool TryGetFusionMachine268Transition(uint8_t before,uint8_t after,int& recovery)
{if(before>2||after<1||after>2)return false;recovery=before==after?0:3000;return true;}
bool TryGetFusionExtras268BucketEffect(uint16_t kind,FusionEquipmentType type,int health,FusionBucketReceiverEffect& out)
{
    if(!FusionEquipmentTypeValid(int(type)))return false;FusionBucketReceiverEffect e;
    if(kind==3&&type==FusionEquipmentType::Machine){if(health<=0||health>4000)return false;e.replacementKind=1129;}
    else if(kind==1129&&health>0&&health<=16000)e.recoverHealth=500;
    else return false;out=e;return true;
}
const char* FusionMachine268FormNode(uint8_t stage,uint8_t form)
{return stage<3&&form<3?forms[stage][form]:nullptr;}
}
