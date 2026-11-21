#include "FusionRollingActors269Rules.h"
#include "FusionNativeRollingActors269.h"
#include <cmath>
#include <algorithm>
#include <limits>
namespace LawnFusion
{
namespace
{
// eve-pool-proof.json: actual LoadPlant nonnull slots minus source WheatLimit.
constexpr uint16_t eveCandidates[]={1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1011,1012,1013,1014,1015,1016,1017,1020,1023,1024,1025,1026,1027,1028,1029,1030,1032,1033,1034,1037,1038,1039,1040,1041,1042,1043,1045,1046,1047,1048,1052,1053,1054,1055,1057,1058,1059,1060,1061,1062,1063,1064,1070,1071,1072,1073,1074,1075,1079,1080,1081,1084,1085,1090,1093,1094,1095,1096,1100,1102,1103,1104,1105,1106,1107,1108,1111,1113,1115,1116,1117,1118,1119,1120,1121,1122,1123,1124,1126,1127,1128,1129,1131,1132,1134,1135,1138,1139,1140,1141,1142,1143,1144,1145,1146,1147,1148,1149,1150,1154,1155,1157,1158,1159,1160,1161,1162,1163,1166,1171,1172,1173,1174,1176,1177,1178,1179,1180,1181,1182,1185,1186,1187,1188,1189,1191,1192};
static_assert(sizeof(eveCandidates)/sizeof(eveCandidates[0])==138);
// BigWallNut.Start484D40/Stay484A50; actual Round clip root0->8/2s,
// rootScale.5, box3.58x4.17; both real prefabs use this one class.
constexpr FusionRollingActor269Profile profiles[]={
    {251,600,4000,4.f,1.79f,2.085f,-.88f},
    {255,600,4000,4.f,1.79f,2.085f,-.88f}};
}
const FusionRollingActor269Profile* FindFusionRollingActor269Profile(uint16_t kind)
{for(const auto& v:profiles)if(v.kind==kind)return &v;return nullptr;}
uint16_t FusionRollingActor269PhysicalKind(uint16_t factory)
{return factory==251||factory==255?255:65535;}
bool FusionRollingActor269Contact(int row,int targetRow,int status,bool controlled)
{return row>=0&&row==targetRow&&!controlled&&status>=0&&status<=25&&status!=10&&status!=12&&(status<20||status>24);}
bool FusionRollingActor269Enter(int row,int targetRow,int status,bool controlled,int kind)
{return FusionRollingActor269Contact(row,targetRow,status,controlled)&&kind!=44&&kind!=46;}
bool FusionRollingActor269Touches(float x,float y,float left,float bottom,float width,float height)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(left)||!std::isfinite(bottom)||!std::isfinite(width)||!std::isfinite(height)||width<0||height<0)return false;
    constexpr float halfWidth=1.79f*.5f,halfHeight=2.085f*.5f;
    return x-halfWidth<=left+width&&x+halfWidth>=left&&y-halfHeight<=bottom+height&&y+halfHeight>=bottom;
}
bool FusionRollingActor269Expired(float x,float maxX)
{return std::isfinite(x)&&std::isfinite(maxX)&&x>maxX+1.f;}
int FusionRollingActor269DamageSum(int before,int damage)
{
    // C# int addition is unchecked. Avoid signed C++ overflow in its history.
    const int64_t sum=int64_t(before)+damage;
    if(sum>(std::numeric_limits<int32_t>::max)())return int(sum-(int64_t(1)<<32));
    if(sum<(std::numeric_limits<int32_t>::min)())return int(sum+(int64_t(1)<<32));
    return int(sum);
}
std::span<const uint16_t> FusionEve269Candidates(){return eveCandidates;}
bool FusionEve269Candidate(uint16_t kind){const auto pool=FusionEve269Candidates();return std::binary_search(pool.begin(),pool.end(),kind);}
float FusionWheat269InitialTime(bool special){return special?30.f:0.f;}
bool TryStepFusionWheat269(float before,float delta,bool enemies,bool active,float& after,bool& transform)
{
    if(!std::isfinite(before)||before<0||before>32||!std::isfinite(delta)||delta<0||delta>1)return false;
    after=before;transform=false;if(enemies&&active){after+=delta;transform=after>31.f;}return true;
}
bool NativeRollingActor269StateValid(const NativeRollingActor269State& v)
{return FindFusionRollingActor269Profile(v.factoryKind)&&v.kind==FusionRollingActor269PhysicalKind(v.factoryKind)&&v.instanceToken&&v.bornTick>=0&&
    std::isfinite(v.originX)&&std::isfinite(v.originY)&&std::isfinite(v.rootOffsetX)&&std::isfinite(v.rootOffsetY)&&
    std::abs(v.originX)<=4096&&std::abs(v.originY)<=4096&&std::abs(v.rootOffsetX)<=1024&&std::abs(v.rootOffsetY)<=1024;}
bool NativeWheat269StateValid(const NativeWheat269State& v)
{return v.isFromWheat&&std::isfinite(v.wheatTime)&&v.wheatTime>=0&&v.wheatTime<=32;}
}
