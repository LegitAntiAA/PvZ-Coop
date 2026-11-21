#include "FusionParticleCollision269Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
bool FusionParticleCollision269ProfileValid(const FusionParticleCollision269Profile& p)
{
    return std::isfinite(p.damping)&&p.damping>=0&&p.damping<=1&&std::isfinite(p.bounce)&&p.bounce>=0&&p.bounce<=1&&
        std::isfinite(p.lifetimeLoss)&&p.lifetimeLoss>=0&&p.lifetimeLoss<=1&&
        std::isfinite(p.minimumSpeed)&&p.minimumSpeed>=0&&std::isfinite(p.maximumSpeed)&&
        p.maximumSpeed>=p.minimumSpeed&&p.maximumSpeed<=10000;
}
bool StepFusionParticleCollision269(const FusionParticleCollision269Profile& p,float px,float py,float ground,float radius,
    bool settled,FusionParticleCollision269State& state,bool& hit,bool& killed)
{
    hit=false;killed=false;
    if(!FusionParticleCollision269ProfileValid(p)||!std::isfinite(px)||!std::isfinite(py)||px<=0||py<=0||
       !std::isfinite(ground)||!std::isfinite(radius)||radius<0||!std::isfinite(state.y)||
       !std::isfinite(state.velocityX)||!std::isfinite(state.velocityY)||state.age<0||state.duration<1||state.duration>1000000||state.age>state.duration)
        return false;
    if(state.velocityY<0||state.y+radius<ground)return true;
    auto next=state;hit=true;next.y=ground-radius;
    next.velocityX*=1-p.damping;
    next.velocityY*=-(settled?0.f:p.bounce)*(1-p.damping);
    const int loss=int(std::lround(double(state.duration-1)*p.lifetimeLoss));
    next.age=(std::min)(state.duration,state.age+loss);
    const double speed=std::hypot(double(next.velocityX)/(px*.01),double(next.velocityY)/(py*.01));
    killed=next.age>=next.duration||speed<p.minimumSpeed||speed>p.maximumSpeed;
    state=next;return true;
}
}
