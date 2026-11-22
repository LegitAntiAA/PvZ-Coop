#include "FusionNextZombie343Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion { namespace {
constexpr FusionZombieProfile profiles[]={
{47,1350,0,0,50},
{48,8888,8888,0,50},
{49,720,0,0,50,false,false,true},
{112,1350,0,0,50},
{113,720,0,0,50},
{114,1350,1100,1100,50},
{115,500,0,0,50},
{116,1500,0,0,50},
{117,500,0,0,50},
{118,270,1400,0,50},
{119,1350,500,0,50},
{200,3000,0,0,500,true,true,true},
{201,12000,0,0,500,true,false,true},
{207,3000,0,0,50},
{209,720,4400,0,50},
{211,3000,0,0,50,true,false,true},
{214,1500,0,0,50},
{216,1350,4400,0,50},
{217,3000,0,0,50},
{222,32000,0,0,50,true,false,true},
};}
const FusionZombieProfile* FindFusionNextZombie343Profile(uint16_t k){for(const auto& p:profiles)if(p.kind==k)return &p;return nullptr;}
bool TryGetFusionNextZombie343BaseKind(uint16_t k,uint16_t& out){switch(k){
case 47:out=0;return true;
case 48:out=2;return true;
case 49:out=0;return true;
case 112:out=31;return true;
case 113:out=27;return true;
case 114:out=8;return true;
case 115:out=0;return true;
case 116:out=0;return true;
case 117:out=0;return true;
case 118:out=0;return true;
case 119:out=29;return true;
case 200:out=14;return true;
case 201:out=18;return true;
case 207:out=10;return true;
case 209:out=34;return true;
case 211:out=14;return true;
case 214:out=27;return true;
case 216:out=31;return true;
case 217:out=6;return true;
case 222:out=18;return true;
default:return false;}}

bool TryPlanFusionCatapult343Shot(uint16_t k,uint8_t ammo,FusionCatapult343Shot& out)
{
 if((k!=31&&k!=112)||!ammo||ammo>25)return false;
 FusionCatapult343Shot p;p.projectile=k==112&&ammo<=20?3:58;p.remaining=ammo-1;
 p.walk2=!p.remaining;p.cherryStage=k==112&&p.remaining<=20;
 p.hiddenRootChildren=p.remaining<=4?uint8_t((1u<<(std::min)(4,5-int(p.remaining)))-1u):0;
 // Source root3..6 change one by one, then roots7 AND8 at remaining20.
 p.damagedRootRenderers=k!=112||p.remaining>24?0:p.remaining>20?uint8_t((1u<<(25-p.remaining))-1u):63;
 out=p;return true;
}
uint8_t FusionDrownCatapult343ArmorStage(int hp,int maximum,uint8_t previous)
{if(maximum<=0||hp<=0)return previous;const int third=maximum/3,twoThird=int((int64_t(maximum)*2)/3);return hp<third?2:hp>third&&hp<=twoThird?1:previous;}
bool TryApplyFusionLandSubmarine343Drain(float health,int maximum,bool controlled,float dt,float& out)
{if(!std::isfinite(health)||maximum<=0||!std::isfinite(dt)||dt<0||dt>1)return false;const float value=controlled?health:health-dt*.1f*float(maximum);if(!std::isfinite(value))return false;out=value;return true;}
bool TryPlanFusionNext343Death(uint16_t kind,int reason,bool controlled,bool lostJumper,int status,FusionNext343DeathPlan& out)
{
 if((kind!=201&&kind!=222&&kind!=209)||reason<0||reason>2||status<0||status>24)return false;
 FusionNext343DeathPlan p;
 if(kind==209)
 {if(reason==0&&!lostJumper&&status!=20){p.explode=true;p.count=1;p.children[0]={30,0,controlled};}}
 else
 {p.driverEffect=true;if(!controlled){p.count=3;
  p.children[0]={uint16_t(kind==201?16:201),-1,false};
  p.children[1]={uint16_t(kind==201?18:201),1,false};
  p.children[2]={uint16_t(kind==201?10:207),0,false};}}
 out=p;return true;
}
}
