#include "../../Lawn/Fusion/FusionWaterZombie313Rules.h"
#include "../../Lawn/Fusion/FusionNative313Snapshot.h"
#include "../../Lawn/Fusion/FusionHostileProjectileRules.h"
#include <limits>
#include <cstdio>
using namespace LawnFusion;
int RunFusionWaterShoot343Tests(int& checks)
{
 int failures=0;auto check=[&](bool yes,int line){++checks;if(!yes){++failures;std::printf("WaterShoot343 line%d failed\n",line);}};
#define test(expression) check((expression),__LINE__)
 for(uint16_t kind:{113,214})
 {
  test(FusionWaterZombie313Kind(kind)&&FusionWaterZombie313SourceInWater(kind));
  FusionWaterZombie313View v;test(InitializeFusionWaterZombie313View(kind,v));
  test(v.phase==FusionWaterZombie313Phase::DolphinRide&&v.sourceStatus==8&&v.auxiliaryActive);
  const auto* g=FindFusionSourceGraph313(kind);test(g&&g->layerCount==3&&g->layers[2].additive);
  if(!g)continue;test(v.shoot343.state==g->layers[2].defaultState&&v.shoot343.outgoing==v.shoot343.state);
  test(FusionWaterZombie313ViewValid(kind,v));test(!FusionWaterZombie313WireViewValid(kind,v));
  v.auxiliary.mPresent=true;v.auxiliary.mFrameCount=1;v.shootClock343.mPresent=true;v.shootClock343.mFrameCount=1;
  test(Native313WaterViewValid(kind,v));
  const auto good=v;v.shoot343.state=g->layers[0].defaultState;test(!Native313WaterViewValid(kind,v));v=good;
  v.shootClock343.mAnimTime=std::numeric_limits<float>::quiet_NaN();test(!Native313WaterViewValid(kind,v));v=good;
  v.shootClock343.mDead=true;test(!Native313WaterViewValid(kind,v));v=good;
  v.shootClock343.mFrameCount=0;test(!Native313WaterViewValid(kind,v));v=good;
  v.gatlingReady343=true;test(Native313WaterViewValid(kind,v)==(kind==214));v=good;
  v.shoot343.fadeDuration=.25f;v.shoot343.fadeElapsed=.26f;test(!Native313WaterViewValid(kind,v));
  FusionWaterZombie313Clip own,base;
  for(auto phase:{FusionWaterZombie313Phase::DolphinRide,FusionWaterZombie313Phase::DolphinJump,FusionWaterZombie313Phase::DolphinSwim,FusionWaterZombie313Phase::DolphinEat,FusionWaterZombie313Phase::DolphinDeath})
  {test(TryGetFusionWaterZombie313Clip(kind,phase,own)&&TryGetFusionWaterZombie313Clip(27,phase,base));
   test(own.seconds==base.seconds&&own.multiplier==base.multiplier&&own.loop==base.loop&&own.speedParameter==base.speedParameter);
   double od=0,bd=0;test(TryGetFusionWaterZombie313RootDelta(kind,phase,{0,0},{0,own.seconds},od)&&TryGetFusionWaterZombie313RootDelta(27,phase,{0,0},{0,base.seconds},bd)&&od==bd);}
  unsigned shoot=255;for(uint8_t i=0;i<g->stateCount;++i)if(FusionSourceGraph313OwnsState(*g,2,i)&&!g->states[i].nullMotion)shoot=i;
  test(shoot!=255);if(shoot!=255){const auto& s=g->states[shoot];test(s.speed==(kind==113?1.3f:.5f));test(s.markerCount==(kind==113?1:4));
   for(uint8_t i=0;i<s.markerCount;++i)test(s.markers[i].event==FusionSourceGraph313Event::Shoot);}
  const uint8_t water[]={0,0,1,1,0,0};test(FusionWaterZombie313WorldValid(kind,6,true,water,2,false));
  test(!FusionWaterZombie313WorldValid(kind,6,true,water,0,false));test(FusionWaterZombie313WorldValid(kind,6,true,water,0,true));
 }
 for(uint16_t kind:{16,17,19,20,27})
 {FusionWaterZombie313View v;test(InitializeFusionWaterZombie313View(kind,v));test(FusionWaterZombie313ViewValid(kind,v));
  v.shoot343.state=1;test(!FusionWaterZombie313ViewValid(kind,v));v.shoot343={};v.shootClock343.mAnimTime=.1f;test(!FusionWaterZombie313ViewValid(kind,v));}
 test(FusionSuperCherryProjectileMetadataValid(3,300,0,true,PROJECTILE_PEA,MOTION_STRAIGHT));
 test(FusionSuperCherryProjectileMetadataValid(3,300,9,true,PROJECTILE_PEA,MOTION_BACKWARDS));
 test(!FusionSuperCherryProjectileMetadataValid(3,60,0,true,PROJECTILE_PEA,MOTION_STRAIGHT));
 test(!FusionSuperCherryProjectileMetadataValid(3,300,0,true,PROJECTILE_PEA,MOTION_BACKWARDS));
 test(!FusionSuperCherryProjectileMetadataValid(3,300,13,true,PROJECTILE_PEA,MOTION_LOBBED));
 for(int damage:{300,1000,2000})test(FusionSuperCherryProjectileMetadataValid(3,damage,0,false,PROJECTILE_PEA,MOTION_STRAIGHT));
 return failures;
#undef test
}
