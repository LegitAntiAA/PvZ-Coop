#ifndef LAWN_FUSION_WORLD_ACTORS269_RULES_H
#define LAWN_FUSION_WORLD_ACTORS269_RULES_H
#include <cstdint>
namespace LawnFusion
{
struct FusionShooterTimingProfile;
// Actual Prismflower shoot controller.937 is a separate continuous beam actor.
const FusionShooterTimingProfile* FindFusionWorldActors269Timing(uint16_t kind);
struct NativeWorld269PlantState;
struct NativeWorld269BeamState;
bool NativeWorld269PlantStateValid(uint16_t,const NativeWorld269PlantState&);
bool NativeWorld269BeamStateValid(const NativeWorld269BeamState&);
struct FusionWorld269Reference { uint16_t id=0; uint32_t token=0; };
bool FusionWorld269ReferenceValid(FusionWorld269Reference);
bool TryGetFusionPrism269Damage(int baseDamage,int contributors,int& damage);
bool TryGetFusionLaser269Damage(int baseDamage,bool connected,bool advanced47,int status,int& damage);
bool FusionLaser269CanAcquire(int sourceStatus,bool mindControlled,bool alive,int rowDelta,int light,bool connected,bool airPass);
int FusionLaser269Healing(int maximumHealth);
// Source AdjustEnergy subtracts cost. Negative cost is recovery, not spending.
bool TryAdjustFusionLaser269Energy(int before,int maximum,int cost,int& after,bool& depleted);
float FusionPrism269Alpha(int actualNativeAge);
bool FusionPrism269Expired(int actualNativeAge);
}
#endif
