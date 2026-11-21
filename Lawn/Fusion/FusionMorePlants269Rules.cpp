#include "FusionMorePlants269Rules.h"
#include <array>
#include <algorithm>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr uint16_t s0[]={0,0,0,0,39,9};
constexpr uint16_t s1[]={0,0,0,0,39,9,37,55,28};
constexpr uint16_t s2[]={0,0,0,0,39,9,59,86,37,55,28};
constexpr uint16_t s3[]={0,0,0,0,39,9,60,15,21,24,59,86,37,55,28};
constexpr auto all=[] {std::array<uint16_t,33> a{};for(unsigned i=0;i<a.size();++i)a[i]=uint16_t(i);return a;}();
constexpr double events[]={double(.45f),double(.5f),double(.55f),double(.6f),double(.65f),double(.7f),double(.75f),double(.8f)};
constexpr double presentEvents[]={double(1.25f)};
const FusionShooterTimingProfile shots[]={
 {243,3,events,1,1,true,0xffff,"shoot"},{243,2.5f,events,1,1,true,0xffff,"shoot2"},
 {243,2.5f,events,1,1,true,0xffff,"shoot3"},{243,2,events,1,1,true,0xffff,"shoot4"}};
const FusionShooterTimingProfile present={256,0,presentEvents,1,double(1.2666667f),true,0xffff,"idle"};
}
std::span<const uint16_t> FusionHamburger269Roster(unsigned stage)
{switch(stage){case 0:return s0;case 1:return s1;case 2:return s2;case 3:return s3;default:return {};}}
bool TryChooseFusionHamburger269Shot(unsigned stage,int base,int draw,int pea,FusionHamburger269Shot& out)
{auto roster=FusionHamburger269Roster(stage);if(roster.empty()||base<0||draw<0||unsigned(draw)>=roster.size()||pea<0||pea>3)return false;
 int64_t damage=int64_t(base)+(stage==1?10:stage>=2?20:0);if(roster[draw]==0)damage+=pea==0?20:pea==1?40:pea==2?60:0;
 if(damage>(std::numeric_limits<int>::max)())return false;out={roster[draw],int(damage),uint8_t(roster[draw]==0&&pea<3?pea+1:0)};return true;}
bool FusionHamburger269EventAllowed(unsigned stage,unsigned event){return stage<=3&&event<8&&event/2<=stage;}
const FusionShooterTimingProfile* FindFusionMorePlants269Timing(uint16_t kind,unsigned stage)
{return kind==243&&stage<4?&shots[stage]:kind==256?&present:nullptr;}
bool FusionMorePlants269StateValid(uint16_t kind,uint8_t mode,uint16_t bag,uint16_t rage,uint32_t shots,int production,int pending)
{if(rage||production<0||pending<0)return false;if(kind==243)return mode==0&&bag<4&&production<=2700&&pending<=50;
 if(kind==256)return mode==0&&!bag&&!shots&&!production&&!pending;
 if(kind==248)return mode<2&&!bag&&production<=1000&&!pending;return false;}
std::span<const uint16_t> FusionPresent269AllCandidates(){return all;}
bool FusionMorePlants269SquashTarget(int status,bool sameRow,bool land){return status==7||(sameRow&&land);}
FusionSqualour269Drops PlanFusionSqualour269Drops(bool hit,unsigned count)
{return {!hit,hit&&count>2,hit?(std::min)(3U,count):0};}
}
