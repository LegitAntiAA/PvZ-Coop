#ifndef LAWN_FUSION_REMAINING_LEAVES269_RULES_H
#define LAWN_FUSION_REMAINING_LEAVES269_RULES_H
#include "FusionShooterTiming.h"
#include "FusionRecipientShooterRules.h"
namespace LawnFusion
{
const FusionShooterTimingProfile* FindFusionRemainingLeaves269Timing(uint16_t kind);
bool FusionRemainingLeaves269ProjectileValid(uint16_t kind,int damage,uint8_t way,uint8_t effectVariant,int nativeType,int nativeMotion);
bool FusionRemainingLeaves269TrackTarget(int status,bool controlled,bool alive,bool collider,bool retaining);
bool FusionRemainingLeaves269CanHit(uint16_t kind,uint8_t way,int status,bool sameRow,bool controlled,bool collider);
bool FusionRemainingLeaves269Touches(uint16_t kind,double x,double y,double angle,double pixelsY,const FusionRecipientRect&);
bool TryGetFusionFertilize269Upgrade(uint16_t oldKind,bool lilyCovered,uint16_t& result);
bool TryGetFusionEndo269Damage(int uniqueTypes,int& damage);
bool FusionSword269AttackEvent(unsigned event);
}
#endif
