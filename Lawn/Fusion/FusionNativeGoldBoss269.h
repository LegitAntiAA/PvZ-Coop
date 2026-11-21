#ifndef LAWN_FUSION_NATIVE_GOLD_BOSS269_H
#define LAWN_FUSION_NATIVE_GOLD_BOSS269_H
#include "FusionGoldBoss269Rules.h"
#include "../Net/NetCommon.h"
class Board;class Zombie;class Plant;class Projectile;
namespace LawnFusion
{
struct NativeGoldBoss269State
{
    int attributeCount=0;uint8_t status=4,firstStage=0,secondStage=0,awardLevel=0;
    uint8_t aimedEvent=0;LawnNet::NetZombieAnimState aimedHead;
    int lastTick=-1,lastAnimationTick=-1;bool rewardCommitted=false,deathVisualCommitted=false,pointsCommitted=false;
    // Above guards are host-only; the two real Reanimations own phase clocks.
};
// Root stores state against the existing actual Zombie DataArray generation.
NativeGoldBoss269State* EditNativeGoldBoss269State(Zombie*,bool create=false);
const NativeGoldBoss269State* GetNativeGoldBoss269State(const Zombie*);
// Admission must cover actual223 art, source clip/event mappings, payload3
// both ways,203 shield-fall conversion,death item34 and exact award callbacks.
bool EnsureNativeGoldBoss269Dependencies(Board*);
// Hooks are exact missing native-boundary leaves; failure never means success.
bool CanApplyNativeGoldBoss269ShieldFall(const Zombie*);
void ApplyNativeGoldBoss269ShieldFall(Zombie*); // Full reverse-list up-to30 conversion→203×5HP,sourceDie2.
void RefreshNativeGoldBoss269ArmorVisuals(Zombie*);
// Actual source MonoBehaviour Transform fields, sampled by native source art.
// Axis is not inferred from the vanilla attack/body Rect centre.
bool GetNativeGoldBoss269SourcePoint(const Zombie*,const char* field,float&,float&);
void SetNativeGoldBoss269Charred(Zombie*);
// Source Die2 must run source death/item/reward callbacks once and native
// removal, without adding vanilla newspaper DropLoot on top of source awards.
void RetireNativeGoldBoss269(Zombie*,int sourceReason);
// Source GetTargetPlant filters exact PlantTag.flyingPlant/caltropPlant and1095.
bool NativeGoldBoss269TargetPlantAllowed(const Plant*);
void TriggerNativeGoldBoss269Animation(Zombie*,bool aimed);
Projectile* CreateNativeGoldBoss269Cherry(Zombie*,const FusionGoldBoss269Shot&,Plant* actualTarget);
bool CanCreateNativeGoldBoss269Award(const Zombie*,const FusionGoldBoss269Award&);
bool CreateNativeGoldBoss269Award(Zombie*,const FusionGoldBoss269Award&);
// Actual GameAPP.itemPrefab[34] at source shadow +Y.6, sound43; NOT particle34.
bool CreateNativeGoldBoss269DeathVisual(Zombie*);
// Exact shared CrashEntity(false); ordinary native chewing is NOT equivalent.
bool ApplyNativeGoldBoss269CrashContact(Zombie*,Plant*);
bool IsNativeGoldBoss269(const Zombie*);
bool InitializeNativeGoldBoss269(Zombie*);
bool UpdateNativeGoldBoss269(Zombie*);
bool NativeGoldBoss269Damage(Zombie*,int alreadyResolvedBaseDamage,int sourceType);
bool NativeGoldBoss269BodyDamage(Zombie*); // true blocks the body override outside6 only.
bool NativeGoldBoss269RejectStatus(const Zombie*,bool mindControl,bool knockback,bool freezeOrButter);
void NativeGoldBoss269LosePaperFinished(Zombie*); // Actual source status6 event only.
bool NativeGoldBoss269AnimShoot(Zombie*,bool aimed,bool beforeDying,bool isChangingRow,bool towardsRight=false);
Plant* FindNativeGoldBoss269Target(Zombie*);
bool ApplyNativeGoldBoss269Post(Zombie*,int awardLevel,int sourceRound,int sourceDifficulty);
bool NativeGoldBoss269Dying(Zombie*); // Once before metadata erasure, real reward only.
bool NativeGoldBoss269StateValid(const NativeGoldBoss269State&);
bool NativeGoldPaper269StateValid(uint16_t kind,const NativeGoldBoss269State&);
}
#endif
