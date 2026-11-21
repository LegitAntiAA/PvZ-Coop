#ifndef LAWN_FUSION_COMPANIONS269_RULES_H
#define LAWN_FUSION_COMPANIONS269_RULES_H
#include "FusionShooterTiming.h"
#include <cstdint>
namespace LawnFusion
{
enum class FusionCompanion269Family:uint8_t {LittleFly,UltimateLittleFly,CherryLittleFly};
struct FusionCompanions269Profile
{uint16_t kind;int childSlots;FusionCompanion269Family childFamily;float initialAttributeSeconds,attributeSeconds;};
const FusionCompanions269Profile* FindFusionCompanions269Profile(uint16_t kind);
const FusionShooterTimingProfile* FindFusionCompanions269Timing(uint16_t kind);
const FusionShooterTimingProfile* FindFusionCompanion269Animation(FusionCompanion269Family family);
bool FusionCompanion269CanTarget(FusionCompanion269Family family,int status,bool controlled,bool smallVariant);
bool FusionCompanion269RayTarget(int status,bool controlled,bool smallVariant);
bool TryStepFusionCompanion269Shoot(float before,float delta,bool onTarget,float& after,bool& trigger);
struct FusionCompanion269Point {float x=0,y=0;};
bool TryMoveFusionCompanion269(FusionCompanion269Point current,FusionCompanion269Point target,float delta,FusionCompanion269Point& after);
bool TryRotateFusionCompanion269(float current,float target,float delta,float& after);
bool FusionCompanion269RayTouches(FusionCompanion269Point origin,float x,float y,float width,float height,float* distance=nullptr);
int FusionCompanions269AttributeTicks(float seconds);
int FusionCompanion269Damage(bool ultimateTravel25);
int FusionSuperSun269IncomingDamage(bool commonTravel5);
int FusionSuperSun269SummonCost(bool advancedTravel4);
}
#endif
