#ifndef LAWN_FUSION_UMBRELLA_BATCH_RULES_H
#define LAWN_FUSION_UMBRELLA_BATCH_RULES_H
#include <cstdint>
namespace LawnFusion
{
bool IsFusionUmbrellaBatchKind(uint16_t);
bool FusionUmbrellaBatchPlantStateValid(uint16_t,uint32_t shots,uint16_t rage,uint16_t energy,uint8_t mode);
float FusionUmbrellaBatchMaximumDuration(uint16_t,float speed);
int FusionUmbrellaBatchMoneySkillFee(uint16_t);
int FusionUmbrellaSelfDamage267(int targets);
bool TryApplyFusionLanternUmbrella267Damage(int incoming,uint16_t energy,int& applied,uint16_t& after);
bool TryGetFusionUmbrella267Reinforcement(int incoming,bool travel,int& after);
bool TryMakeFusionUmbrella267Retarget(double x,double y,double targetX,double targetY,double pixelsY,
    float& velocityX,float& velocityY,float& gravity);
}
#endif
