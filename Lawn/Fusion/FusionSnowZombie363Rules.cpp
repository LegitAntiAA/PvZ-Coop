#include "FusionSnowZombie363Rules.h"
namespace LawnFusion
{
bool TryGetFusionSnow363ArmorStage(int hp,int maximum,uint8_t& out)
{
 if(maximum<=0||hp<=0||hp>maximum)return false;
 const int low=maximum/3,high=int(int64_t(maximum)*2/3);
 //359560 is not a monotonic latch. Above high AND exactly low both take
 //the source's final else, restoring the original armor renderer/children.
 out=hp<low?2:hp>low&&hp<=high?1:0;return true;
}
}
