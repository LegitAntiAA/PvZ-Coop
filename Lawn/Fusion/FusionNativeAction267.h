#ifndef LAWN_FUSION_NATIVE_ACTION267_H
#define LAWN_FUSION_NATIVE_ACTION267_H
#include "FusionAction267Rules.h"
class Plant;
class Board;
class Zombie;
namespace LawnFusion
{
bool InitializeNativeAction267(Plant*);
bool UpdateNativeAction267(Plant*);
bool UpdateNativeAction267Phase(Plant*);
bool ApplyNativeAction267Special(Plant*);
bool DispatchNativeAction267Events(Plant*);
bool HandleNativeAction267Crush(Plant*);
bool GetNativeAction267VisualPose(const Plant*,const char*&,float&);
bool TryHandleNativeDefender267Damage(Plant*,int& damage,int sourceType);
void NativeAction267HealthChanged(Plant*);
// Root owns the canonical source damage admission/base continuation.
void ApplyNativeSourcePlantDamage(Plant*,int damage,int sourceType);
}
#endif
