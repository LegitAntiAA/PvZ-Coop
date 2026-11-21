#include "FusionLastPlants269Rules.h"
#include <cmath>
namespace LawnFusion
{
namespace
{
constexpr double summonEvent[]={double(.33333334f)};
const FusionShooterTimingProfile emperor={900,30,summonEvent,.5,double(.6666667f),true,0xffff,"summon"};
}
bool IsFusionLastPlants269Kind(uint16_t kind){return kind==900||kind==938||kind==1056||kind==253||kind==918;}
const FusionShooterTimingProfile* FindFusionLastPlants269Timing(uint16_t kind){return kind==900?&emperor:nullptr;}
bool FusionLastPlants269StateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode,uint8_t stage)
{
    if(rage||stage)return false;
    if(kind==900)return !shots&&bag<=5&&mode<=1;
    if(kind==938)return !shots&&!bag&&mode<=1;
    if(kind==1056)return shots<=100&&!bag&&mode<=2;
    return (kind==253||kind==918)&&!shots&&!bag&&!mode;
}
bool FusionLastPlants269ClockValid(uint16_t kind,int production,int pending,float speed,float shoot)
{
    if(!std::isfinite(speed)||speed<.9f||speed>1.1f||!std::isfinite(shoot)||production<0||pending<0)return false;
    if(kind==900)return production<=3000&&!pending&&(shoot== -1||(shoot>=0&&shoot<=FusionShooterDuration(emperor,speed)+.011f));
    if(kind==938)return production<=151&&!pending&&shoot== -1;
    if(kind==253)return production<=3101&&!pending&&shoot== -1;
    if(kind==1056)return production<=38&&pending<=50&&shoot== -1;
    if(kind==918)return !production&&!pending&&shoot== -1;
    return false;
}
bool FusionLastPlants269TorchContactAllowed(uint8_t way,bool sameRow,bool zombie,bool sameTorch)
{return !zombie&&!sameTorch&&(way==2||sameRow);}
bool FusionLastPlants269TorchPayload(uint16_t kind){return kind==0||kind==10||kind==15||kind==29;}
int FusionLastPlants269WheatTicks(bool fast){return fast?101:3101;}
bool FusionLastPlants269KelpProjectileValid(uint16_t kind,int damage,uint8_t way)
{return kind==30&&damage==40&&(way==0||way==2);}
bool FusionLastPlants269KelpSputterAllowed(int armor,int npc)
{return armor>=0&&npc>=0&&armor!=1&&armor!=3&&npc!=14;}
bool FusionLastPlants269KelpProjectileRect(double x,double y,double py,FusionRecipientRect& out)
{
    if(!std::isfinite(x+y+py)||py<=0)return false;
    const double w=.27739394*1.7*(72./1.35),h=.24820518*1.7*py;
    out={x-w*.5,y-h*.5,w,h};return true;
}
bool FusionLastPlants269KelpProjectileTouches(double x,double y,double py,const FusionRecipientRect& other)
{
    FusionRecipientRect r;if(!FusionLastPlants269KelpProjectileRect(x,y,py,r)||!std::isfinite(other.x+other.y+other.width+other.height)||other.width<0||other.height<0)return false;
    return r.x<=other.x+other.width&&r.x+r.width>=other.x&&r.y<=other.y+other.height&&r.y+r.height>=other.y;
}
}
