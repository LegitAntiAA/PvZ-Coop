#include "FusionPlantDisable363Rules.h"
#include <algorithm>
#include <limits>
namespace LawnFusion
{
bool FusionPlantDisable363ViewValid(std::span<const uint16_t> v)
{return v.size()<=65535&&std::is_sorted(v.begin(),v.end())&&std::all_of(v.begin(),v.end(),[](uint16_t n){return n>0&&n<=kFusionSnowBallDisable363Ticks;});}
bool TryAddFusionPlantDisable363(std::span<const uint16_t> before,std::vector<uint16_t>& out)
{if(!FusionPlantDisable363ViewValid(before)||before.size()==65535)return false;std::vector<uint16_t> next(before.begin(),before.end());next.push_back(kFusionSnowBallDisable363Ticks);out=std::move(next);return true;}
bool TickFusionPlantDisable363(std::vector<uint16_t>& v,bool& active)
{if(!FusionPlantDisable363ViewValid(v))return false;const bool was=!v.empty();for(auto& t:v)--t;v.erase(v.begin(),std::find_if(v.begin(),v.end(),[](uint16_t n){return n!=0;}));active=was&&v.empty();return true;}
// Verbatim predicates from TypeMgr40D140/40D290. These are source element
// tags, not a whitelist of plants the new gameplay operation may target.
bool FusionSourcePlantFire363(uint16_t k)
{
 if(k<0x463){if(k<0x387)return (uint32_t(k)-0x10U&0xfffffffdU)==0||k==0x386;if(k==0x3a4)return true;
 switch(k){case 0x419:case 0x41c:case 0x41d:case 0x41e:case 0x41f:case 0x420:case 0x421:case 0x423:case 0x425:case 0x426:case 0x42d:case 0x42f:case 0x431:case 0x433:return true;default:return k==0x462;}}
 if(k<0x48c)return k==0x47d||uint32_t(k)-0x483U<2||k==0x48b;
 if(k>0x495)return uint32_t(k)-0x49aU<2||k==0x49e;
 return k==0x48f||k==0x495;
}
bool FusionSourcePlantIce363(uint16_t k)
{
 if(k<0x417){if(k>0x38c){if(k<0x398)return k==0x38e||k==0x397;return uint32_t(k)-0x40aU<8||k==0x416;}if(k<0xf2)return k==10||k==0xf1;return (uint32_t(k)-0x388U&0xfffffffbU)==0;}
 if(k>0x453){if(k>0x48d)return (uint32_t(k)-0x49cU&0xfffffffdU)==0||k==0x4a5;return k==0x463||k==0x48d;}
 if(k>0x430)return k==0x432||uint32_t(k)-0x451U<3;
 return (uint32_t(k)-0x42cU&0xfffffffbU)==0;
}
bool FusionSourcePlantDisable363Immune(uint16_t k)
{return k==251||k==255||k==1179||FusionSourcePlantIce363(k)||FusionSourcePlantFire363(k);}
}
