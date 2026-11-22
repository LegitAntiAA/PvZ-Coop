#include "../../Lawn/Fusion/FusionNextZombie343Rules.h"
#include "../../Lawn/Fusion/FusionHostileProjectileRules.h"
#include "../../Lawn/Fusion/FusionDrown343Rules.h"
#include "../../Lawn/Fusion/FusionSourceObstacle313.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <cstdio>
using namespace LawnFusion;
int RunFusionNextZombie343Tests(int& checks)
{
 int failures=0;auto check=[&](bool ok,int line){++checks;if(!ok){++failures;std::printf("FusionNextZombie343 line%d failed\n",line);}};
#define test(expression) check((expression),__LINE__)
 // InitZombieData3BA5C0: real112 stats differ from base31's850/500.
 const auto* p=FindFusionNextZombie343Profile(112);test(p&&p->maxHealth==1350&&p->biteDamage==50&&!p->firstArmor&&!p->secondArmor);
 // Source352390 tests ammo BEFORE decrement, so shots1..5 are58,6..25 are3.
 uint8_t ammo=25;unsigned basket=0,cherry=0;
 for(unsigned shot=1;shot<=25;++shot)
 {FusionCatapult343Shot s;const bool ok=TryPlanFusionCatapult343Shot(112,ammo,s);test(ok);if(!ok)break;
  test(s.projectile==(shot<=5?58:3));basket+=s.projectile==58;cherry+=s.projectile==3;
  test(s.remaining==25-shot&&s.walk2==(shot==25));test(s.cherryStage==(shot>=5));
  const uint8_t expected=shot==1?1:shot==2?3:shot==3?7:shot==4?15:63;test(s.damagedRootRenderers==expected);
  const uint8_t hidden=shot<21?0:shot==21?1:shot==22?3:shot==23?7:15;test(s.hiddenRootChildren==hidden);ammo=s.remaining;}
 test(basket==5&&cherry==20&&ammo==0);
 FusionCatapult343Shot sentinel;sentinel.projectile=999;test(!TryPlanFusionCatapult343Shot(112,0,sentinel)&&sentinel.projectile==999);
 test(!TryPlanFusionCatapult343Shot(112,26,sentinel)&&sentinel.projectile==999);test(!TryPlanFusionCatapult343Shot(216,2,sentinel)&&sentinel.projectile==999);
 FusionCatapult343Shot base;test(TryPlanFusionCatapult343Shot(31,20,base)&&base.projectile==58&&!base.cherryStage&&!base.damagedRootRenderers);
 test(FusionCatapultCherry343MetadataValid(3,60,13,true,PROJECTILE_PEA,MOTION_LOBBED));
 test(!FusionCatapultCherry343MetadataValid(3,300,13,true,PROJECTILE_PEA,MOTION_LOBBED));
 test(!FusionCatapultCherry343MetadataValid(3,60,13,false,PROJECTILE_PEA,MOTION_LOBBED));
 test(!FusionCatapultCherry343MetadataValid(3,60,9,true,PROJECTILE_PEA,MOTION_BACKWARDS));
 test(!FusionCatapultCherry343MetadataValid(58,60,13,true,PROJECTILE_BASKETBALL,MOTION_LOBBED));
 test(FusionSuperCherryProjectileMetadataValid(3,300,9,true,PROJECTILE_PEA,MOTION_BACKWARDS));
 test(!FusionSuperCherryProjectileMetadataValid(3,60,13,true,PROJECTILE_PEA,MOTION_LOBBED));
 uint16_t out=999;test(!TryGetFusionNextZombie343BaseKind(221,out)&&out==999);test(TryGetFusionNextZombie343BaseKind(200,out)&&out==14);test(TryGetFusionNextZombie343BaseKind(217,out)&&out==6);
 const auto* g=FindFusionSourceGraph343(112);test(g&&g->stateCount==10&&g->layerCount==2&&g->layers[1].additive);
 if(g){
  unsigned shoot=255,walk=255,walk1=255,rest=255;
  for(uint8_t i=0;i<g->stateCount;++i){const auto& s=g->states[i];if(!std::strcmp(s.name,"shoot"))shoot=i;if(!std::strcmp(s.name,"walk"))walk=i;if(!std::strcmp(s.name,"walk1"))walk1=i;if(!std::strcmp(s.name,"rest"))rest=i;}
  test(shoot<255&&walk<255&&walk1<255&&rest<255);
  if(shoot<255){const auto& s=g->states[shoot];test(s.markerCount==1&&s.markers[0].event==FusionSourceGraph313Event::Shoot&&std::abs(s.markers[0].seconds-1.0666667f)<1e-6f);test(s.duration==2.0166667f&&s.speed==1&&s.speedParameter==1);}
  if(rest<255&&walk1<255){FusionSourceGraph313View v;v.state=v.outgoing=uint8_t(rest);FusionSourceGraph313Parameters params;params.wait=true;
   const FusionSourceGraph313Edge* edge=nullptr;uint32_t consumed=99;
   test(SelectFusionSourceGraph313Edge(*g,0,v,{0,0},{0,0},params,false,false,edge,consumed)&&edge&&edge->target==walk1&&edge->duration==.25f&&consumed==0);}
  if(walk<255){double d=999;test(SourceRootDelta343(112,uint8_t(walk),{0,0},{0,2},d)&&d<0);
   double first=0,second=0,joined=0;test(SourceRootDelta343(112,uint8_t(walk),{0,0},{0,1},first)&&SourceRootDelta343(112,uint8_t(walk),{0,1},{1,1},second)&&SourceRootDelta343(112,uint8_t(walk),{0,0},{1,1},joined)&&std::abs(first+second-joined)<1e-8);
   d=123;test(!SourceRootDelta343(112,uint8_t(walk),{0,1},{0,0},d)&&d==123);test(!SourceRootDelta343(112,uint8_t(walk),{0,0},{0,std::numeric_limits<float>::quiet_NaN()},d)&&d==123);}
 }
 // DrownWeapon.Start3764E0: horizontal X draw is12..14, independent Y=-1/-2.
 float vx=99,vy=88;
 test(TryMakeFusionDrown343HorizontalVelocity(12,false,vx,vy)&&vx==-12&&vy==-1);
 test(TryMakeFusionDrown343HorizontalVelocity(14,true,vx,vy)&&vx==-14&&vy==-2);
 vx=99;vy=88;test(!TryMakeFusionDrown343HorizontalVelocity(11.99f,false,vx,vy)&&vx==99&&vy==88);
 test(!TryMakeFusionDrown343HorizontalVelocity(std::numeric_limits<float>::quiet_NaN(),true,vx,vy)&&vx==99&&vy==88);
 for(uint8_t variant=0;variant<4;++variant)
 {test(FusionDrown343OwnerVariant(20,variant)==(variant==0));test(FusionDrown343OwnerVariant(202,variant)==(variant==0));
  test(FusionDrown343OwnerVariant(216,variant)==(variant==1));test(FusionDrown343OwnerVariant(218,variant)==(variant==3));}
 test(!FusionDrown343OwnerVariant(221,0));
 // OnTrigger375F30: the three unconditional ways cross rows/allegiances.
 for(uint8_t way:{2,15,18})for(bool hostile:{false,true})for(bool row:{false,true})test(FusionDrown343BigBulletContact(way,hostile,row));
 test(FusionDrown343BigBulletContact(0,false,true));test(!FusionDrown343BigBulletContact(0,false,false));
 test(!FusionDrown343BigBulletContact(0,true,true));test(!FusionDrown343BigBulletContact(13,false,true));
 // Fixed375E80 material uses cos, not sin:5sec starts gain1; quarter-wave1.5sec gain1 too.
 test(FusionDrown343Brightness(100,100,true)==1);test(FusionDrown343Brightness(100,0,false)==1);
 test(std::abs(FusionDrown343FlashRemaining(100,100,false)-.3f)<1e-6f);
 test(FusionDrown343FlashRemaining(132,100,false)==0);test(FusionDrown343FlashRemaining(602,100,true)==0);
 for(int i=100;i<620;++i){const float gain=FusionDrown343Brightness(i,100,true);test(gain>=1&&gain<=4&&std::isfinite(gain));}
 // U16 survives300 genuine bullet interceptions, then301 consumes the actor.
 FusionSourceObstacle313View weapon;weapon.instanceToken=0x10001;weapon.ownerId=4;weapon.ownerToken=0x20001;weapon.variant=3;weapon.ageTicks=2700;
 weapon.hitCount=300;weapon.lastHitAge=2600;test(FusionSourceObstacle313ViewValid(weapon));
 weapon.hitCount=301;test(FusionSourceObstacle313ViewValid(weapon));weapon.hitCount=302;test(!FusionSourceObstacle313ViewValid(weapon));
 weapon.hitCount=0;weapon.drownLongFlash=true;test(FusionSourceObstacle313ViewValid(weapon));
 weapon.variant=1;test(!FusionSourceObstacle313ViewValid(weapon));weapon.variant=3;weapon.targetPlantId=9;weapon.targetPlantToken=1;test(!FusionSourceObstacle313ViewValid(weapon));
 weapon.targetPlantId=0;weapon.targetPlantToken=0;weapon.ageTicks=2599;test(!FusionSourceObstacle313ViewValid(weapon));
 //211 Update358F70 drains10% maxHP/second only for hostile; no death/part flag alias.
 float health=999;test(TryApplyFusionLandSubmarine343Drain(3000,3000,false,.01f,health)&&health==2997);
 test(TryApplyFusionLandSubmarine343Drain(3000,3000,true,.01f,health)&&health==3000);
 test(TryApplyFusionLandSubmarine343Drain(2,3000,false,.01f,health)&&std::abs(health+1)<1e-6f);
 health=777;test(!TryApplyFusionLandSubmarine343Drain(3000,0,false,.01f,health)&&health==777);
 test(!TryApplyFusionLandSubmarine343Drain(3000,3000,false,std::numeric_limits<float>::infinity(),health)&&health==777);
 test(FusionSubmarine343Kind(14)&&FusionSubmarine343Kind(200)&&FusionSubmarine343Kind(211)&&!FusionSubmarine343Kind(201));
 test(FusionDancePole343Kind(6)&&FusionDancePole343Kind(217)&&!FusionDancePole343Kind(7));
 const auto* land=FindFusionNextZombie343Profile(211);const auto* water=FindFusionNextZombie343Profile(200);
 test(land&&water&&land->maxHealth==3000&&water->maxHealth==3000&&land->biteDamage==50&&water->biteDamage==500);
 //362EC0 integer thirds + ValueBetween(low<hp<=high), NOT float-thirds:
 // hp==floor(max/3) intentionally retains the prior source image.
 test(FusionDrownCatapult343ArmorStage(2934,4400,0)==0);
 test(FusionDrownCatapult343ArmorStage(2933,4400,0)==1);
 test(FusionDrownCatapult343ArmorStage(1467,4400,0)==1);
 test(FusionDrownCatapult343ArmorStage(1466,4400,0)==0);
 test(FusionDrownCatapult343ArmorStage(1466,4400,1)==1);
 test(FusionDrownCatapult343ArmorStage(1465,4400,1)==2);
 test(FusionDrownCatapult343ArmorStage(0,0,1)==1);
 return failures;
}
#undef test
