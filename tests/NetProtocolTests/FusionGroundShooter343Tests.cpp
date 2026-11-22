#include "../../Lawn/Fusion/FusionGroundShooter343Rules.h"
#include "../../Lawn/Fusion/FusionExtendedZombie313Rules.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace LawnFusion;
int RunFusionGroundShooter343Tests(int& checks)
{
 int failures=0;auto check=[&](bool v,int line){++checks;if(!v){++failures;std::printf("Ground343 line%d failed\n",line);}};
#define test(x) check((x),__LINE__)
 FusionGround343Shot s;
 test(TryPlanFusionGround343Shot(114,0,false,false,false,false,true,s)&&s.emit&&s.bullet==11&&s.damage==80&&s.way==9&&s.hostile&&s.sourceOffsetX==-.4f);
 test(TryPlanFusionGround343Shot(114,0,false,false,true,true,true,s)&&s.emit&&s.way==0&&!s.hostile&&s.sourceOffsetX==.4f);
 test(TryPlanFusionGround343Shot(119,10,false,false,false,false,true,s)&&s.emit&&s.bullet==11&&s.damage==80&&s.sourceOffsetX==0);
 for(uint16_t k:{114,119})
 {test(TryPlanFusionGround343Shot(k,1,false,false,false,false,true,s)&&!s.emit);
  test(TryPlanFusionGround343Shot(k,0,true,false,false,false,true,s)&&!s.emit);
  test(TryPlanFusionGround343Shot(k,0,false,true,false,false,true,s)&&!s.emit);}
 //3566F0 has no status/beforeDying/changingRow guard; only the actual Shoot
 //Transform survives until its real parent is destroyed. Do not alias114.
 test(TryPlanFusionGround343Shot(118,1,true,true,false,false,true,s)&&s.emit&&s.bullet==0&&s.damage==20);
 test(TryPlanFusionGround343Shot(118,0,false,false,false,false,false,s)&&!s.emit);
 s.damage=123;test(!TryPlanFusionGround343Shot(117,0,false,false,false,false,true,s)&&s.damage==123);
 float seconds=0;test(TryGetFusionGround343Attribute(114,true,seconds)&&seconds==.5f);
 test(TryGetFusionGround343Attribute(114,false,seconds)&&seconds==1.5f);
 test(TryGetFusionGround343Attribute(118,true,seconds)&&seconds==.1f);
 test(TryGetFusionGround343Attribute(118,false,seconds)&&seconds==.1f);
 test(TryGetFusionGround343Attribute(119,true,seconds)&&seconds==1.5f);
 //35CDF0 /354A00 monotonic integer thirds; 356940 intentionally keeps the
 //prior image at exactly floor(max/3), unlike the114 rule.
 test(FusionGround343ArmorStage(114,733,1100,0)==0);
 test(FusionGround343ArmorStage(114,732,1100,0)==1);
 test(FusionGround343ArmorStage(114,366,1100,1)==1);
 test(FusionGround343ArmorStage(114,365,1100,0)==2);
 test(FusionGround343ArmorStage(114,1100,1100,2)==2);
 test(FusionGround343ArmorStage(118,933,1400,0)==1);
 test(FusionGround343ArmorStage(118,934,1400,0)==0);
 test(FusionGround343ArmorStage(118,467,1400,0)==1);
 test(FusionGround343ArmorStage(118,466,1400,0)==0);
 test(FusionGround343ArmorStage(118,466,1400,1)==1);
 test(FusionGround343ArmorStage(118,465,1400,1)==2);
 FusionFlightArmor313Plan fall;
 test(TryPlanFusionFlightFall313(119,false,false,500,fall)&&fall.fallTriggered&&fall.fallen&&fall.clearArmor&&!fall.die2&&fall.health==0&&fall.maximum==500&&fall.sourceStatus==0);
 test(TryPlanFusionFlightFall313(29,false,false,500,fall)&&fall.health==2200&&fall.maximum==2200);
 test(TryPlanFusionFlightFall313(119,false,true,500,fall)&&fall.die2&&fall.fallTriggered);
 test(TryPlanFusionFlightFall313(119,true,false,500,fall)&&!fall.fallTriggered&&fall.maximum==500);
 test(TryPlanFusionFlightArmor313(119,10,false,false,500,500,200,fall)&&fall.health==300&&fall.overflow==0&&!fall.fallTriggered);
 test(TryPlanFusionFlightArmor313(119,10,false,false,500,500,5000,fall)&&fall.fallTriggered&&fall.overflow==0&&fall.maximum==500);
 test(!TryPlanFusionFlightFall313(114,false,false,1100,fall));
 FusionExtendedEffectAction313 action;float duration=0;
 test(TryPlanFusionExtendedEffect313(119,10,FusionExtendedEffect313::Freeze,2,action,duration)&&action==FusionExtendedEffectAction313::ColdInstead&&duration==12);
 test(TryPlanFusionExtendedEffect313(119,0,FusionExtendedEffect313::Freeze,2,action,duration)&&action==FusionExtendedEffectAction313::Normal&&duration==2);
 for(uint16_t kind:{114,118,119})
 {
  const auto* profile=FindFusionExtendedZombie313Profile(kind);test(profile&&profile->kind==kind);
  test(FusionExtendedZombie313BaseLimbCallbacks(kind)&&!FusionExtendedZombie313OwnBodyDamage(kind));
  FusionExtendedZombie313View v;test(FusionExtendedZombie313ViewValid(kind,v));v.secondArmorStage343=1;test(FusionExtendedZombie313ViewValid(kind,v)==(kind==114));v.secondArmorStage343=3;test(!FusionExtendedZombie313ViewValid(kind,v));v={};v.fallen=true;test(FusionExtendedZombie313ViewValid(kind,v)==(kind==119));
  const auto* g=FindFusionGround343Graph(kind);test(g&&g->layerCount==(kind==119?3:2));if(!g)continue;
  int markers=0;for(uint8_t n=0;n<g->stateCount;++n)
  {const auto& clip=g->states[n];for(uint8_t i=0;i<clip.markerCount;++i)if(clip.markers[i].event==FusionSourceGraph313Event::Shoot)
    {++markers;test(clip.markers[i].seconds==(kind==114?.5f:kind==118?.13333334f:.33333334f));}
   if(!std::strcmp(clip.name,"walk")){double delta=0;test(SourceRootDeltaGround343(kind,n,{0,0},{1,0},delta)&&delta!=0);}}
  test(markers==1);
 }
 FusionExtendedZombie313View v;v.secondArmorStage343=1;test(!FusionExtendedZombie313ViewValid(29,v));test(!FusionExtendedZombie313ViewValid(202,v));
 test(!FindFusionGround343Graph(117));
#undef test
 return failures;
}
