#ifndef LAWN_FUSION_NATIVE_ACTION266_H
#define LAWN_FUSION_NATIVE_ACTION266_H
#include "FusionAction266Rules.h"
class Plant;
class Zombie;
namespace LawnFusion
{
bool InitializeNativeAction266(Plant* plant);
bool UpdateNativeAction266(Plant* plant);
bool UpdateNativeAction266Phase(Plant* plant);
bool ApplyNativeAction266Special(Plant* plant);
bool TryActivateNativeAction266Skill(Plant* plant);
bool GetNativeAction266VisualPose(const Plant* plant,const char*& state,float& elapsed);
int GetNativeAction266LightContribution(const Plant* receiver);
void NativeAction266HealthChanged(Plant* plant);
void NativeAction266PlantRemoving(Plant* plant,int sourceReason);
void NativeAction266ChomperSwallowStarted(Plant* plant);
bool NativeAction266GloomAcquire(Plant* plant,Zombie* zombie);
void ApplyNativeAction266GloomAttack(Plant* plant);
void ApplyNativeAction266Jalaed(Zombie* zombie);
// Existing native Squish invokes source Crashed; rocks survive this transition.
bool HandleNativeAction266Crush(Plant* plant);
}
#endif
