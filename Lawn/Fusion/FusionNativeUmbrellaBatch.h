#ifndef LAWN_FUSION_NATIVE_UMBRELLA_BATCH_H
#define LAWN_FUSION_NATIVE_UMBRELLA_BATCH_H
#include "FusionUmbrellaBatchRules.h"
class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
bool IsNativeUmbrellaBatch(const Plant*);
bool InitializeNativeUmbrellaBatch(Plant*);
bool UpdateNativeUmbrellaBatch(Plant*);
bool UpdateNativeUmbrellaBatchPhase(Plant*);
bool ApplyNativeUmbrellaBatchSpecial(Plant*);
bool GetNativeUmbrellaBatchVisualPose(const Plant*,const char*&,float&);
bool CanActivateNativeUmbrellaBatchSkill(const Plant*);
bool TryActivateNativeUmbrellaBatchSkill(Plant*);
bool TryInterceptNativeUmbrella(Projectile*,Plant* attacked);
void AdvanceNativeUmbrellaProjectiles(Projectile*);
bool ApplyNativeUmbrellaProjectileImpact(Projectile*,Zombie*);
void NativeUmbrellaBatchRemoving(Plant*);
bool TryHandleNativeUmbrella267Damage(Plant*,int& damage,int sourceType);
bool TryEnhanceNativeUmbrellaProjectile(Projectile*,Plant* actualTarget);
int GetNativeUmbrella267LightContribution(const Plant*);
// Source ConvertZombie virtual override. Uses actual target HP/armor and
// source factory41/42; returns false if a replacement could not be admitted.
bool TryConvertNativeUmbrella269(Plant*,Zombie*,float goldRatio=.1f,float silverRatio=.25f);
}
#endif
