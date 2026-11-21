#include "FusionRemainingPlants269Rules.h"
#include <limits>
#include <initializer_list>
namespace LawnFusion
{
namespace
{
constexpr double present[]={double(1.25f)},imitate[]={1.5},puff[]={double(.41666666f),double(.6666667f)},grab[]={.5};
const FusionShooterTimingProfile timings[]={
    {250,1,present,1,double(1.2666667f),true,65535,"idle"},
    {245,1,imitate,1,double(1.5166667f),false,65535,"explode"},
    {1175,1.5,puff,1,double(.9166667f),false,9,"shoot"},
    {1175,1.5,grab,double(2.1f),double(1.6666666f),true,65535,"Attack"}};
}
bool IsFusionRemaining269Kind(uint16_t k)
{for(uint16_t v:{241,242,243,244,245,248,249,250,254,256,900,938,1056,1151,1174,1175})if(k==v)return true;return false;}
bool FusionRemaining269ControllerKind(uint16_t k)
{return k==241||k==244||k==245||k==250||k==254||k==1175;}
const FusionShooterTimingProfile* FindFusionRemaining269Timing(uint16_t k,bool hand)
{if(k==1175)return &timings[hand?3:2];for(const auto& t:timings)if(t.kind==k)return &t;return nullptr;}
bool FusionRemaining269StateValid(uint16_t k,uint8_t mode,uint16_t bag,uint16_t rage,uint32_t shots,int ticks,int pending)
{
    if(!FusionRemaining269ControllerKind(k)||bag||rage||ticks<0||pending<0||shots>uint32_t((std::numeric_limits<int>::max)()))return false;
    if(k==1175)return mode<=3&&ticks<=300&&pending<=100;
    if(k==245)return mode<=1&&ticks<=150&&!pending&&!shots;
    if(k==244)return !mode&&ticks<=200&&!pending&&!shots;
    if(k==250)return !mode&&!ticks&&!pending&&shots<=1;
    return !mode&&!ticks&&!pending;
}
bool TryChooseFusionPresentZombie269(bool water,int draw,uint16_t& kind)
{if(draw<0||draw>2)return false;constexpr uint16_t types[]={0,2,4};kind=water?110:types[draw];return true;}
bool TryGetFusionMixBomb269Recipe(uint16_t center,uint16_t& left,uint16_t& right,uint16_t& result)
{
    switch(center)
    {case 5:left=right=5;result=1174;return true;case 14:left=right=1030;result=1161;return true;
     case 19:left=right=15;result=1175;return true;case 25:left=1098;right=1164;result=1169;return true;
     case 28:left=right=32;result=1132;return true;default:return false;}
}
bool FusionRemaining269Imitable(uint16_t k,bool flying,bool pumpkin,bool pot,bool lily)
{return k!=245&&!flying&&!pumpkin&&!pot&&!lily;}
bool TryAdvanceFusionHamburger269(uint8_t stage,uint16_t donor,uint8_t& next,int& health,float& interval)
{constexpr uint16_t donors[]={21,32,1116};if(stage>2||donor!=donors[stage])return false;next=uint8_t(stage+1);health=1000*(next+1);interval=next==3?2.f:2.5f;return true;}
bool TryCapFusionSuperMachine269Damage(int maximum,int incoming,int& applied)
{if(maximum<1||incoming<0)return false;applied=incoming>maximum?maximum:incoming;return true;}
bool FusionBigChomper269Boss(uint16_t k)
{return k==43||k==44||k==46||k==212||(k>=218&&k<=224);}
bool FusionBigChomper269AreaTarget(int status,bool row,bool friendly,bool dying,uint16_t kind)
{if(!row||friendly||dying||FusionBigChomper269Boss(kind))return false;switch(status){case 7:case 10:case 12:case 16:case 20:case 21:case 22:case 23:case 24:return false;default:return status>=0;}}
uint16_t FusionRemaining269SnowGatlingPayload(bool bean){return bean?97:15;}
}
