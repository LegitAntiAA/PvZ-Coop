#include "FusionAction267Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
constexpr double rockEvents[]={static_cast<double>(.33333334f),static_cast<double>(.7f)};
const FusionShooterTimingProfile rockClock{906,1,rockEvents,static_cast<double>(1.2f),static_cast<double>(.9f),true,0xffff,"attack"};
}
const FusionShooterTimingProfile* FindFusionAction267TimingProfile(uint16_t k){return k==906?&rockClock:nullptr;}
bool IsFusionAction267Kind(uint16_t k)
{return k==931||k==906||k==913||k==925||k==1181||k==1176||k==936||k==1077||k==1086;}
bool IsFusionObsidian267Kind(uint16_t k){return k==931||k==906||k==913||k==925;}
bool FusionAction267PlantStateValid(uint16_t k,uint32_t stored,uint16_t rage,uint16_t bag,uint8_t mode)
{
    return IsFusionAction267Kind(k)&&(k==925?stored<=1000:stored==0)&&!rage&&!bag&&
        (k==1077?mode<=2:(k==1176||k==1181||k==1086)?mode<=1:mode==0);
}
bool TryGetFusionObsidian267Damage(uint16_t k,int incoming,int type,int count,int& applied,bool& shared)
{
    if(!IsFusionObsidian267Kind(k)||incoming<0||type<0||type>3||count<0)return false;
    shared=false;applied=incoming;
    if(k==931)applied=(std::min)(incoming,1000);
    if(k==913||k==925)
    {
        applied=(std::min)(incoming,200);
        if(type!=3){if(!count)return false;applied=(std::max)(applied/count,1);shared=true;}
    }
    return true;
}
bool TryAdvanceFusionObsidian267Store(uint32_t before,int damage,uint32_t& after,bool& explode)
{
    if(before>1000||damage<0)return false;
    const uint64_t total=uint64_t(before)+uint32_t(damage);explode=total>1000;
    after=explode?0:static_cast<uint32_t>(total);return true;
}
bool TryGetFusionSolar267Cycle(int sun,float multiplier,int& ticks)
{
    if(sun<0||!std::isfinite(multiplier)||multiplier<.9f||multiplier>1.1f)return false;
    const float ratio=(std::min)(static_cast<float>(sun)/15000.0f,1.0f);
    const float seconds=(std::max)(3.0f,multiplier*15.0f*ratio);
    ticks=static_cast<int>(std::ceil(static_cast<double>(seconds)*100));return true;
}
bool TryGetFusionSeaSun267Item(uint8_t stage,uint16_t& item)
{if(stage>2)return false;item=stage==0?2:stage==1?0:1;return true;}
int FusionAction267MaximumProductionTicks(uint16_t k){return k==1077?1701:k==936?1651:0;}
float FusionAction267MaximumDuration(uint16_t k,float speed)
{if(!std::isfinite(speed)||speed<.9f||speed>1.1f)return 0;if(k==906)return FusionShooterDuration(rockClock,speed)+.02f;return k==1086?1.02f:k==1181||k==1176?1.2f/speed:k==1077?1.02f:0;}
}
