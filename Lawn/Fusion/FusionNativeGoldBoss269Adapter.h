#ifndef LAWN_FUSION_NATIVE_GOLD_BOSS269_ADAPTER_H
#define LAWN_FUSION_NATIVE_GOLD_BOSS269_ADAPTER_H
#include "FusionNativeGoldBoss269.h"
class Reanimation;
namespace LawnFusion
{
// Root binds this ONE source Board+124 float ledger, not profile/money/vanillaScore.
bool CanCreditNativeGoldPaper269SourcePoints(Board*,float amount);
bool CreditNativeGoldPaper269SourcePoints(Board*,float amount);
bool IsNativeGoldPaper269(const Zombie*);
bool CanCreateNativeGoldPaper269(Board*,uint16_t kind);
bool InitializeNativeGoldPaper269(Zombie*);
bool UpdateNativeGoldPaper269(Zombie*);
// Before generic source health admission.223 uses actualDie2;203's native
// death-animation path continues after the source awards are committed.
bool UpdateNativeGoldPaper269Death(Zombie*);
bool UpdateNativeGoldPaper269Animation(Zombie*);
bool StartNativeGoldPaper269Walk(Zombie*,int blend=0);
bool UpdateNativeGoldPaper269Attack(Zombie*);
bool NativeGoldPaper269Damage(Zombie*,int resolvedDamage,int sourceDamageType);
bool NativeGoldPaper269BodyDamage(const Zombie*);
bool NativeGoldPaper269RejectStatus(const Zombie*,bool mindControl,bool knockback,bool freezeOrButter);
bool BeginNativeGoldPaper269Fall(Zombie*);
bool GetNativeGoldPaper269Status(const Zombie*,int&);
bool GetNativeGoldPaper269Presentation(const Zombie*,const char*&,float& normalizedPhase);
bool GetNativeGoldPaper269AddonPose(const Zombie*,bool aimed,const char*&,float& normalizedPhase);
bool GetNativeGoldPaper269Collider(const Zombie*,FusionGoldPaper269Rect&);
bool ApplyNativeGoldPaper269Post(Zombie*,int awardLevel,int round,int difficulty);
bool NativeGoldPaper269Dying(Zombie*);
void NativeGoldPaper269Removing(Zombie*);
// Source3/60/way13 is its actual common planar Cherry leaf; Root must admit
// this exact tuple and route this before the ordinary linear source3 path.
bool IsNativeGoldPaper269AimedProjectile(const Projectile*);
bool UpdateNativeGoldPaper269AimedProjectile(Projectile*);
}
#endif
