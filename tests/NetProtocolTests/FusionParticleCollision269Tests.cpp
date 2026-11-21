#include "FusionParticleCollision269Tests.h"
#include "../../Lawn/Fusion/FusionParticleCollision269Rules.h"
#include <cmath>
#include <limits>
int RunFusionParticleCollision269Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    auto check=[&](bool value){++checks;if(!value)++failures;};
    const FusionParticleCollision269Profile p{.5f,1,.3f,0,10000};
    FusionParticleCollision269State s{110,4,6,10,201};bool hit=false,killed=false;
    check(StepFusionParticleCollision269(p,80,60,100,3,false,s,hit,killed)&&hit&&!killed&&s.y==97&&s.velocityX==2&&s.velocityY==-3&&s.age==70);
    s={110,4,6,10,201};check(StepFusionParticleCollision269(p,80,60,100,3,true,s,hit,killed)&&hit&&s.velocityY==0&&s.velocityX==2);
    s={90,4,6,10,201};check(StepFusionParticleCollision269(p,80,60,100,3,false,s,hit,killed)&&!hit&&s.age==10&&s.velocityY==6);
    s={110,4,-6,10,201};check(StepFusionParticleCollision269(p,80,60,100,3,false,s,hit,killed)&&!hit&&s.y==110);
    s={110,4,6,150,201};check(StepFusionParticleCollision269(p,80,60,100,3,false,s,hit,killed)&&killed&&s.age==201);
    auto loss=p;loss.lifetimeLoss=.5f;s={110,0,0,0,201};check(StepFusionParticleCollision269(loss,80,60,100,0,false,s,hit,killed)&&s.age==100);
    auto speed=p;speed.maximumSpeed=1;s={110,4,6,0,201};check(StepFusionParticleCollision269(speed,80,60,100,0,false,s,hit,killed)&&killed);
    speed=p;speed.minimumSpeed=1;s={110,0,0,0,201};check(StepFusionParticleCollision269(speed,80,60,100,0,false,s,hit,killed)&&killed);
    auto bad=p;bad.damping=std::numeric_limits<float>::quiet_NaN();s={110,4,6,10,201};
    check(!StepFusionParticleCollision269(bad,80,60,100,3,false,s,hit,killed)&&s.age==10&&s.velocityX==4);
    check(!StepFusionParticleCollision269(p,0,60,100,3,false,s,hit,killed));
    check(!StepFusionParticleCollision269(p,80,60,100,-1,false,s,hit,killed));
    bad=p;bad.bounce=1.1f;check(!FusionParticleCollision269ProfileValid(bad));
    bad=p;bad.lifetimeLoss=-.1f;check(!FusionParticleCollision269ProfileValid(bad));
    bad=p;bad.minimumSpeed=10001;check(!FusionParticleCollision269ProfileValid(bad));
    return failures;
}
