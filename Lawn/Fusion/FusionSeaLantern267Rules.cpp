#include "FusionSeaLantern267Rules.h"
namespace LawnFusion
{
bool FusionSeaLantern267StateValid(uint16_t kind,uint32_t count,uint16_t rage,uint16_t bag,
    uint8_t mode,int production,int pending)
{
    return kind==1078&&!count&&!rage&&!bag&&mode<=1&&!pending&&
        (mode?production==0:production>0&&production<=kFusionSeaLantern267GrowthTicks);
}
int FusionSeaLantern267LightRadius(uint8_t mode){return mode<=1?int(mode)+1:0;}
}
