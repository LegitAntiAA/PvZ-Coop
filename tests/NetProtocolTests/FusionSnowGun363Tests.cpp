#include "../../Lawn/Fusion/FusionSnowGun363Rules.h"
#include "../../Lawn/Fusion/FusionColdRules.h"
#include <cmath>
#include <limits>
#include <cstdio>
using namespace LawnFusion;
int RunFusionSnowGun363Tests(int& checks)
{
 int failures=0;
 const auto check=[&](bool valid,int line){++checks;if(!valid){++failures;std::printf("SnowGun363 line%d\n",line);}};
#define C(x) check((x),__LINE__)
 // SnowGun.AnimShoot370CD0: source101,75,way17 and a horizontal
 // zero-vy ballistic drop. Negative source target delta maps down on Board.
 for(bool hostile:{false,true}){(void)hostile;C(FusionSnowBall363Metadata(101,75,17,PROJECTILE_PEA,MOTION_LOBBED));}
 C(!FusionSnowBall363Metadata(100,75,17,PROJECTILE_PEA,MOTION_LOBBED));
 C(!FusionSnowBall363Metadata(101,60,17,PROJECTILE_PEA,MOTION_LOBBED));
 C(!FusionSnowBall363Metadata(101,75,13,PROJECTILE_PEA,MOTION_LOBBED));
 C(!FusionSnowBall363Metadata(101,75,17,PROJECTILE_SNOWPEA,MOTION_LOBBED));
 C(!FusionSnowBall363Metadata(101,75,17,PROJECTILE_PEA,MOTION_STRAIGHT));
 FusionHostilePlanarShot shot;C(TryPlanFusionSnowGun363Shot(600,180,300,280,60,shot));
 const double seconds=std::sqrt(200./(double(kFusionSnowGun363Gravity)*60));
 C(shot.x==600&&shot.y==180&&shot.velocityY==0&&std::abs(shot.velocityX+3/seconds)<1e-9);
 C(std::abs(shot.gravityY-double(kFusionSnowGun363Gravity)*60*.0001)<1e-9);
 C(AdvanceFusionHostilePlanarShot(shot));
 C(std::abs(shot.y-180-shot.gravityY)<1e-8&&std::abs(shot.velocityY-shot.gravityY)<1e-8);
 FusionHostilePlanarShot untouched{11,22,33,44,55};const auto original=untouched;
 for(double bad:{0.,-1.,std::numeric_limits<double>::quiet_NaN()})
 {C(!TryPlanFusionSnowGun363Shot(600,180,300,280,bad,untouched));C(untouched.x==original.x&&untouched.velocityX==original.velocityX);}
 C(!TryPlanFusionSnowGun363Shot(600,180,300,180,60,untouched));
 C(!TryPlanFusionSnowGun363Shot(std::numeric_limits<double>::infinity(),180,300,280,60,untouched));
 int cells=0;for(int r=-2;r<=2;++r)for(int c=-2;c<=2;++c){const bool hit=FusionSnowBall363AreaCell(c,r);C(hit==(std::abs(c)<=1&&std::abs(r)<=1));cells+=hit;}C(cells==9);
 C(FusionSnowBall363PlantAffected(false,false,false));C(FusionSnowBall363PlantAffected(false,true,true));
 C(!FusionSnowBall363PlantAffected(false,false,true));C(!FusionSnowBall363PlantAffected(true,true,false));
 FusionColdState before{90,100,0,0},after;int damage=0;
 C(TryPrepareFusionColdHit(101,before,75,after,damage));C(after.level==0&&after.threshold==200&&after.frozenTicks==400&&damage==300);
 C(TryPrepareFusionColdHit(101,{0,100,0,0},75,after,damage));C(after.level==10&&after.frozenTicks==0&&damage==75);
 C(!FusionColdAppliesAfterShield(101,false)&&FusionColdAppliesAfterShield(101,true));
 const auto* g=FindFusionSnowGun363Graph(49);C(g&&g->layerCount==2&&g->stateCount==6);C(!FindFusionSnowGun363Graph(48));
 if(g){C(g->states[1].speed==2.5f&&g->states[2].speed==1.8f&&g->states[4].nullMotion&&g->states[4].duration==0);
  C(g->states[5].markerCount==1&&g->states[5].markers[0].seconds==.16666667f&&g->states[5].markers[0].event==FusionSourceGraph313Event::Shoot);
  C(g->states[1].edgeCount==1&&g->states[1].edges[0].duration==.25f&&g->states[1].edges[0].conditions[1].parameter==FusionSourceGraph313Param::Wait);
  double a=0,b=0,c=0;C(SourceRootDeltaSnowGun363(49,1,{0,0},{0,1},a));C(SourceRootDeltaSnowGun363(49,1,{0,1},{0,2},b));C(SourceRootDeltaSnowGun363(49,1,{0,0},{0,2},c));C(std::abs(a+b-c)<1e-9&&c<0);}
#undef C
 return failures;
}
