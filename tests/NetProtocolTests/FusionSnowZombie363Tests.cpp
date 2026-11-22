#include "../../Lawn/Fusion/FusionSnowZombie363Rules.h"
#include "../../Lawn/Fusion/FusionExtendedZombie313Rules.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace LawnFusion;
int RunFusionSnowZombie363Tests(int& checks)
{
 int failures=0;auto check=[&](bool v,int line){++checks;if(!v){++failures;std::printf("Snow363 line%d failed\n",line);}};
#define test(x) check((x),__LINE__)
 test(std::bit_cast<uint32_t>(kFusionSnow363MinimumTurn)==0x4153851f);
 test(std::bit_cast<uint32_t>(kFusionSnow363MaximumTurn)==0x41b570a4);
 uint8_t stage=99;
 test(TryGetFusionSnow363ArmorStage(8888,8888,stage)&&stage==0);
 test(TryGetFusionSnow363ArmorStage(5925,8888,stage)&&stage==1);
 test(TryGetFusionSnow363ArmorStage(5926,8888,stage)&&stage==0);
 test(TryGetFusionSnow363ArmorStage(2963,8888,stage)&&stage==1);
 test(TryGetFusionSnow363ArmorStage(2962,8888,stage)&&stage==0); //359560 else restores original at equality.
 test(TryGetFusionSnow363ArmorStage(2961,8888,stage)&&stage==2);
 test(TryGetFusionSnow363ArmorStage(8888,8888,stage)&&stage==0); //Real restore, not a damage latch.
 stage=99;test(!TryGetFusionSnow363ArmorStage(0,8888,stage)&&stage==99);
 test(!TryGetFusionSnow363ArmorStage(8889,8888,stage)&&stage==99);
 for(uint16_t kind:{47,48})
 {
  const auto* p=FindFusionExtendedZombie313Profile(kind);test(p&&p->kind==kind&&p->maxHealth==(kind==47?1350:8888));
  test(FusionExtendedZombie313BaseLimbCallbacks(kind)&&!FusionExtendedZombie313OwnBodyDamage(kind));
  FusionExtendedZombie313View v;test(FusionExtendedZombie313ViewValid(kind,v));v.rootFlip=true;test(FusionExtendedZombie313ViewValid(kind,v));v.firstArmorStage=2;test(FusionExtendedZombie313ViewValid(kind,v)==(kind==48));v.firstArmorStage=0;v.secondArmorStage343=1;test(!FusionExtendedZombie313ViewValid(kind,v));
  const auto* g=FindFusionSnow363Graph(kind);test(g&&g->layerCount==1&&g->stateCount==4);if(!g)continue;
  int eats=0,destroy=0;
  for(uint8_t i=0;i<g->stateCount;++i)
  {const auto& s=g->states[i];test(s.speedParameter==1);
   if(!std::strcmp(s.name,"walk")){test(s.speed==.6f);double delta=0;test(SourceRootDeltaSnow363(kind,i,{0,0},{1,0},delta)&&delta<0);}
   if(!std::strcmp(s.name,"eat"))test(s.speed==1.5f);
   for(uint8_t n=0;n<s.markerCount;++n){if(s.markers[n].event==FusionSourceGraph313Event::Bite)++eats;if(s.markers[n].event==FusionSourceGraph313Event::Destroy)++destroy;}}
  test(eats==2&&destroy==1);
 }
 test(!FindFusionSnow363Graph(49));
#undef test
 return failures;
}
