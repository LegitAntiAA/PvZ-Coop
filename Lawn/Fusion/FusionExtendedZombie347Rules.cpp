#include "FusionExtendedZombie347Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr FusionZombieProfile profiles[]={
 {202,270,7500,0,50},{204,3000,12000,0,100},
 {205,3000,0,0,50,true,false,true},{206,500,0,0,50,true,false,true},
 {210,1350,0,0,50,true,false,true},{218,8000,24000,0,50},
 {219,12000,0,0,50,true,false,true},{220,8000,24000,0,200},
 {221,12000,0,0,50,true,false,true},{224,32000,0,0,50,true,false,true}};
}
const FusionZombieProfile* FindFusionExtendedZombie347Profile(uint16_t k)
{for(const auto& p:profiles)if(p.kind==k)return &p;return nullptr;}
bool FusionExtendedZombie347Kind(uint16_t k){return FindFusionExtendedZombie347Profile(k)!=nullptr;}
std::span<const uint16_t> FusionExtended347Dependencies(uint16_t k)
{static constexpr uint16_t a[]={206},b[]={202},c[]={204},d[]={205},e[]={30,209};switch(k){case 205:return a;case 218:return b;case 220:return c;case 221:return d;case 224:return e;default:return {};}}
bool TryGetFusionExtended347Family(uint16_t k,FusionExtended347Family& result)
{
 FusionExtended347Family v;
 switch(k){case 202:case 204:case 218:case 220:v=FusionExtended347Family::Football;break;
 case 205:case 206:v=FusionExtended347Family::Airship;break;case 210:case 219:v=FusionExtended347Family::Machine;break;
 case 221:v=FusionExtended347Family::Spider;break;case 224:v=FusionExtended347Family::Jackbox;break;default:return false;}
 result=v;return true;
}
bool FusionExtended347OwnBody(uint16_t k){return k==205||k==206||k==210||k==219||k==221||k==224;}
bool FusionExtended347BaseLimbs(uint16_t k){return k==202||k==204||k==218||k==220;}
bool FusionExtended347ViewValid(uint16_t k,const FusionExtendedZombie313View& v)
{
 if(!FusionExtendedZombie347Kind(k)||v.bodyStage>2||v.firstArmorStage>2||v.weapon||v.machineType||v.uniqueType||
   v.carriedImpRemoved||v.ballRemoved||v.fallen||!v.visible||v.adjustingImp||v.impVerticalSpeed!=1||v.impMaximumVx!= -3||
   !std::isfinite(v.uniqueSpeed)||v.uniqueSpeed<=0||v.uniqueSpeed>4096||
   !std::isfinite(v.attributeCountdown)||v.attributeCountdown<0||v.attributeCountdown>1000000||
   !std::isfinite(v.throwTime)||v.throwTime< -1||v.throwTime>18||v.attackDamage<0||v.attackDamage>1000000)return false;
 if(v.sourceStatus!=0&&v.sourceStatus!=1&&((k==205||k==206)?v.sourceStatus!=10:k==221?v.sourceStatus!=10&&v.sourceStatus!=25:true))return false;
 if(k!=205&&k!=206&&k!=218&&v.bombPresent)return false;
 if(k!=202&&k!=218&&(v.throwTime||v.isThrow))return false;
 if(k!=218&&k!=219&&k!=220&&k!=221&&k!=224&&v.attributeCountdown)return false;
 if(k!=220&&v.attackDamage)return false;
 if(k==220&&v.attackDamage<200)return false;
 if(k!=210&&k!=219&&v.uniqueSpeed!=1)return false;
 if(k!=202&&k!=204&&k!=218&&k!=220&&v.firstArmorStage)return false;
 return !FusionExtended347OwnBody(k)||!v.beforeDying;
}
bool TryPlanFusionExtended347Effect(uint16_t k,int status,FusionExtendedEffect313 effect,float seconds,FusionExtendedEffectAction313& out,float& value)
{
 if(!FusionExtendedZombie347Kind(k)||status<0||!std::isfinite(seconds)||uint8_t(effect)>uint8_t(FusionExtendedEffect313::MindControl))return false;
 auto action=FusionExtendedEffectAction313::Normal;
 if((k==218||k==219||k==220||k==221||k==224)&&effect==FusionExtendedEffect313::MindControl)action=FusionExtendedEffectAction313::Suppress;
 if((k==220||k==221)&&effect==FusionExtendedEffect313::Knockback)action=FusionExtendedEffectAction313::Suppress;
 if((k==210||k==219)&&(effect==FusionExtendedEffect313::Butter||effect==FusionExtendedEffect313::Poison||effect==FusionExtendedEffect313::Garlic))action=FusionExtendedEffectAction313::Suppress;
 if((k==221||k==224)&&effect==FusionExtendedEffect313::Butter)action=FusionExtendedEffectAction313::Suppress;
 if(k==224&&effect==FusionExtendedEffect313::Freeze)action=FusionExtendedEffectAction313::Suppress;
 if(k==221&&effect==FusionExtendedEffect313::Freeze&&status!=0)action=FusionExtendedEffectAction313::Suppress;
 if((k==205||k==206)&&effect==FusionExtendedEffect313::Freeze){action=FusionExtendedEffectAction313::ColdInstead;seconds+=10;}
 out=action;value=seconds;return true;
}
bool TryPlanFusionExtended347Damage(uint16_t k,int status,bool controlled,int raw,float countdown,FusionExtended347DamagePlan& out)
{
 if(!FusionExtendedZombie347Kind(k)||raw<0||!std::isfinite(countdown)||countdown<0)return false;
 FusionExtended347DamagePlan p;p.raw=raw;p.attributeCountdown=countdown;
 if((k==206&&controlled)||(k==221&&status==25))p.ignored=true;
 if(k==220||k==224)p.raw=(std::min)(raw,5000);
 if(k==218||k==219||k==221)p.maximumResolved=5000;
 if(k==219){if(countdown>0)p.ignored=true;else p.attributeCountdown=.2f;}
 out=p;return true;
}
bool TryPlanFusionExtended347Body(uint16_t k,float hp,int maximum,int damage,uint8_t previous,int status,FusionExtendedBody313Plan& out)
{
 if(k==205)return TryPlanFusionExtendedBody313(26,hp,maximum,damage,previous,0,status,false,0,out);
 if(k==210||k==219)return TryPlanFusionExtendedBody313(40,hp,maximum,damage,previous,0,status,false,0,out);
 if(!FusionExtended347OwnBody(k)||!std::isfinite(hp)||maximum<=0||hp>maximum||damage<0||previous>2)return false;
 FusionExtendedBody313Plan p;p.health=hp-float(damage);p.bodyStage=previous;p.sourceStatus=status;
 if(!std::isfinite(p.health))return false;
 if(k==221&&status==0&&p.health<float((maximum*2)/3)){p.health=float((maximum*2)/3);p.sourceStatus=25;p.triggerThrow=true;}
 if(k!=206)
 {
  const bool floating=k==205;const float third=floating?float(maximum)/3:float(maximum/3);
  const float upper=floating?float(maximum*2)/3:float((maximum*2)/3);
  if(p.health<third)p.bodyStage=2;
  else if(p.health<=upper)p.bodyStage=1;
  else if(k==224)p.bodyStage=0;
 }
 if(k==224)p.bodyStage=(p.health>float(maximum/3)&&p.health<=float((maximum*2)/3))?1:p.health<float(maximum)/3.f?2:0;
 out=p;return true;
}
uint8_t FusionExtended347ArmorStage(uint16_t k,int hp,int maximum,uint8_t previous)
{
 if((k!=202&&k!=204&&k!=218&&k!=220)||hp<=0||maximum<=0||previous>2)return previous;
 if(k==220)return hp<maximum/3?2:hp>maximum/3&&hp<=(maximum*2)/3?1:0;
 const float third=k==202?float(maximum)/3:float(maximum/3),upper=k==202?float(maximum*2)/3:float((maximum*2)/3);
 if(float(hp)<third)return 2;
 if(float(hp)<upper&&(k==218||k==220||previous<1))return 1;
 return previous;
}
bool PlanFusionExtended347Births(uint16_t k,int status,int level,int rows,int columns,bool controlled,float axisX,std::vector<FusionExtended347Birth>& out)
{
 if(!FusionExtendedZombie347Kind(k)||(rows!=5&&rows!=6)||columns!=10||!std::isfinite(axisX))return false;
 std::vector<FusionExtended347Birth> births;
 if(k==218&&!controlled)for(int row=0;row<rows;++row)births.push_back({202,row,-1,9.9f,false,false});
 else if(k==221&&status==10)for(int row=0;row<rows;++row)births.push_back({205,row,-1,9.9f,false,false});
 else if(k==224&&(level==0||level==1))for(int col=4;col<columns;++col)for(int row=0;row<rows;++row)births.push_back({uint16_t(level==0?30:209),row,col,0,false,level==0});
 out.swap(births);return true;
}
bool TryPlanFusionExtended347Attribute(uint16_t k,int status,float speed,int attack,int hp,int maximum,FusionExtended347AttributePlan& out)
{
 if(!FusionExtendedZombie347Kind(k)||!std::isfinite(speed)||speed<=0||attack<0||hp<0||maximum<hp)return false;
 FusionExtended347AttributePlan p;p.originSpeed=speed;p.attackDamage=attack;p.firstArmorHealth=hp;
 if(k==220){p.seconds=3;p.attackDamage=int((std::min)(int64_t(1000000),int64_t(attack)*2));p.originSpeed=speed<3?speed+.3f:3;if(hp>0)p.firstArmorHealth=int((std::min)(int64_t(maximum),int64_t(hp)+6000));}
 else if(k==221){p.seconds=3;p.shoot=status==10;}
 else if(k==224){p.seconds=8;p.pop=true;}
 else if(k!=218&&k!=219)return false;
 out=p;return true;
}
bool TryNormalizeFusionExtended347Death(uint16_t k,int reason,int& out)
{if(!FusionExtendedZombie347Kind(k)||reason<0)return false;out=(k==210||k==219||k==221||k==224)?(reason==1?1:2):(k==205||k==206)&&reason==0?2:reason;return true;}
bool FusionSpider347HitsGround(float y,float land){return std::isfinite(y)&&std::isfinite(land)&&y<=land;}
}
