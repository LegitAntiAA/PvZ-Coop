#ifndef LAWN_FUSION_ACTION267_RULES_H
#define LAWN_FUSION_ACTION267_RULES_H
#include <cstdint>
#include "FusionShooterTiming.h"
namespace LawnFusion
{
bool IsFusionAction267Kind(uint16_t kind);
bool IsFusionObsidian267Kind(uint16_t kind);
bool FusionAction267PlantStateValid(uint16_t kind,uint32_t stored,uint16_t rage,uint16_t bag,uint8_t mode);
bool TryGetFusionObsidian267Damage(uint16_t kind,int incoming,int sourceType,int recipients,int& applied,bool& shared);
bool TryAdvanceFusionObsidian267Store(uint32_t before,int incoming,uint32_t& after,bool& explode);
bool TryGetFusionSolar267Cycle(int sun,float multiplier,int& ticks);
bool TryGetFusionSeaSun267Item(uint8_t stage,uint16_t& item);
int FusionAction267MaximumProductionTicks(uint16_t kind);
float FusionAction267MaximumDuration(uint16_t kind,float speed);
const FusionShooterTimingProfile* FindFusionAction267TimingProfile(uint16_t kind);
}
#endif
