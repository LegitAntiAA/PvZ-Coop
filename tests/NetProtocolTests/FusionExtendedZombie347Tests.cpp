#include "../../Lawn/Fusion/FusionExtendedZombie347Rules.h"
#include <cstdio>
#include <limits>
#include <cmath>
#include <utility>
using namespace LawnFusion;
int RunFusionExtendedZombie347Tests(int& totalChecks)
{
 int checks=0,failures=0;auto check=[&](bool ok,const char* m){++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL347 %s\n",m);}};
 for(uint16_t k:{202,204,205,206,210,218,219,220,221,224})
 {
  check(FusionExtendedZombie347Kind(k)&&FindFusionExtendedZombie347Profile(k)->kind==k,"typed profile preserves sourcekind");
  FusionExtended347Family family;check(TryGetFusionExtended347Family(k,family),"family present");
  FusionExtendedZombie313View v;if(k==220)v.attackDamage=200;check(FusionExtended347ViewValid(k,v),"fresh canonical defaults valid");
  for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),-1.f}){auto x=v;x.attributeCountdown=bad;check(!FusionExtended347ViewValid(k,x),"bad source countdown refused");}
  auto x=v;x.throwTime=19;check(!FusionExtended347ViewValid(k,x),"source throw range bounded");
  int n=77;check(TryNormalizeFusionExtended347Death(k,0,n),"typed death normalization");check((k==210||k==219||k==221||k==224||k==205||k==206)?n==2:n==0,"source normalization values");
  check(TryNormalizeFusionExtended347Death(k,1,n)&&n==1,"real reason1 retained");
 }
 check(!FusionExtendedZombie347Kind(26)&&!FusionExtendedZombie347Kind(0),"oldfamilies not silently reclassified");
 check(FindFusionExtendedZombie347Profile(202)->firstArmor==7500&&FindFusionExtendedZombie347Profile(220)->biteDamage==200,"genuine football stats");
 check(FindFusionExtendedZombie347Profile(221)->kind==221&&FindFusionExtendedZombie347Profile(221)->maxHealth==12000,"Spider not Kirov26 alias");
 for(int rows:{5,6})for(int choice:{0,1,2})
 {
  std::vector<FusionExtended347Birth> b;check(PlanFusionExtended347Births(224,0,choice,rows,10,false,5,b),"source224 gridloop plan");
  check(b.size()==size_t(choice==2?0:rows*6),"all columns4..9/allrows retained");
  for(const auto& x:b)check(x.row>=0&&x.row<rows&&x.column>=4&&x.column<10&&!x.controlled&&x.kind==(choice?209:30)&&x.popImmediately==(choice==0),"exact childsource tuple");
 }
 for(int rows:{5,6})
 {
  std::vector<FusionExtended347Birth> b;check(PlanFusionExtended347Births(218,0,0,rows,10,false,5,b)&&b.size()==size_t(rows),"218 attempts EVERY actual row202");
  check(PlanFusionExtended347Births(218,0,0,rows,10,true,5,b)&&b.empty(),"218 controlled attribute nochildren");
  check(PlanFusionExtended347Births(221,10,0,rows,10,false,5,b)&&b.size()==size_t(rows),"221flying spawns205 allrows");
  check(PlanFusionExtended347Births(221,0,0,rows,10,false,5,b)&&b.empty(),"221grounded doesnotspawnairfleet");
 }
 FusionExtended347DamagePlan damage;
 check(TryPlanFusionExtended347Damage(220,0,false,10000,0,damage)&&damage.raw==5000&&damage.maximumResolved==0x7fffffff,"220 raw cap BEFORE baseJalaed/protection");
 check(TryPlanFusionExtended347Damage(218,0,false,10000,0,damage)&&damage.raw==10000&&damage.maximumResolved==5000,"218 cap AFTER commonGetDamage");
 check(TryPlanFusionExtended347Damage(221,25,false,10000,0,damage)&&damage.ignored,"Spider transform invulnerable");
 check(TryPlanFusionExtended347Damage(206,10,true,100,0,damage)&&damage.ignored,"controlledBombThrower damage no-op");
 check(TryPlanFusionExtended347Damage(219,0,false,100,.1f,damage)&&damage.ignored,"219 actualsource damagecooldown honored");
 check(TryPlanFusionExtended347Damage(219,0,false,100,0,damage)&&!damage.ignored&&damage.attributeCountdown==.2f,"219 consumes genuine .2s countdown once");
 FusionExtendedBody313Plan body;
 check(TryPlanFusionExtended347Body(221,12000,12000,100000,0,0,body)&&body.health==8000&&body.sourceStatus==25&&body.triggerThrow,"Spider firstthreshold clampsHP and raises; nooverkill bypass");
 check(TryPlanFusionExtended347Body(221,8000,12000,5000,1,10,body)&&body.health==3000&&body.sourceStatus==10&&!body.triggerThrow,"Spider flyingbody damage doesn't retriggerraise");
 check(TryPlanFusionExtended347Body(206,500,500,250,0,10,body)&&body.health==250&&body.bodyStage==0,"BombThrower ownsubtraction noKirovdamagedsprite stages");
 check(TryPlanFusionExtended347Body(224,5000,32000,1,0,0,body)&&body.bodyStage==2,"224lowerthirdbody");
 check(TryPlanFusionExtended347Body(224,32000,32000,1,2,0,body)&&body.bodyStage==0,"224nextDamage afterheal restores highHP appearance");
 for(const auto& sample: {std::pair<int,uint8_t>{7999,2},{8000,0},{8001,1},{16000,1},{16001,0},{24000,0}})
  check(FusionExtended347ArmorStage(220,sample.first,24000,2)==sample.second,"220 actual ValueBetween boundary/restoration");
 for(const auto& sample: {std::pair<float,uint8_t>{10666.f,2},{10666.5f,1},{21333.f,1},{21334.f,0}})
  check(TryPlanFusionExtended347Body(224,sample.first,32000,0,0,0,body)&&body.bodyStage==sample.second,"224 exact mixed integer and floating thirds");
 FusionExtended347AttributePlan attribute;
 check(TryPlanFusionExtended347Attribute(220,0,1,200,1000,24000,attribute)&&attribute.attackDamage==400&&attribute.firstArmorHealth==7000&&std::abs(attribute.originSpeed-1.3f)<1e-6&&attribute.seconds==3,"220genuine3s growthandarmorrepair");
 check(TryPlanFusionExtended347Attribute(220,0,2.9f,900000,0,24000,attribute)&&attribute.attackDamage==1000000&&attribute.firstArmorHealth==0&&attribute.originSpeed>3,"220 one-stepovershoot thennextcap; lostarmor not resurrected");
 check(TryPlanFusionExtended347Attribute(224,0,1,0,0,0,attribute)&&attribute.pop&&attribute.seconds==8,"224source8sattribute");
 check(TryPlanFusionExtended347Attribute(221,10,1,0,0,0,attribute)&&attribute.shoot&&attribute.seconds==3,"Spider3sfire onlyflying");
 for(uint16_t k:{205,206}){FusionExtendedEffectAction313 action;float seconds=0;check(TryPlanFusionExtended347Effect(k,10,FusionExtendedEffect313::Freeze,4,action,seconds)&&action==FusionExtendedEffectAction313::ColdInstead&&seconds==14,"KirovderivedFreeze4=>Cold14");}
 for(uint16_t k:{218,219,220,221,224}){FusionExtendedEffectAction313 action;float seconds=0;check(TryPlanFusionExtended347Effect(k,0,FusionExtendedEffect313::MindControl,1,action,seconds)&&action==FusionExtendedEffectAction313::Suppress,"genuine ultimateMC no-op");}
 check(FusionSpider347HitsGround(1,1)&&FusionSpider347HitsGround(.9f,1)&&!FusionSpider347HitsGround(1.1f,1),"Spiderground boundary fromactualposition");
 check(!FusionSpider347HitsGround(std::numeric_limits<float>::quiet_NaN(),1),"Spiderinvalidposition refused");
 std::printf("TASK347 checks=%d failures=%d\n",checks,failures);totalChecks+=checks;return failures;
}
