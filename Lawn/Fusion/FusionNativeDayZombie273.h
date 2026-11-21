#ifndef LAWN_FUSION_NATIVE_DAY_ZOMBIE273_H
#define LAWN_FUSION_NATIVE_DAY_ZOMBIE273_H
#include "FusionDayZombie273Rules.h"
#include "../Net/NetCommon.h"
#include <cmath>
class Board; class Zombie; class Plant; class Projectile; class Coin;

namespace LawnFusion
{
struct NativeProjectileData;
struct NativeDayZombie273State
{
    float shootCountdown=1.5f;
    uint8_t status=0,armorStage=0,shootEvent=0;
    bool shadowEnabled=true;
    LawnNet::NetZombieAnimState head; // Snapshot of actual attached native Reanimation.
    bool dollFalling=false; // Host-only recursion guard; DO NOT encode/hash.
    int poleEventFrame=-1; // Host-only consumed body span, not a phase clock.
    FusionZombieClipPosition poleEvent{};
    bool poleEventValid=false;
    bool headDropPending=false; // Host-only bounded Coin allocation retry.
    uint32_t headDropDamageFlags=0; // Do not encode/hash either retry field.
};
// Root supplies one sparse map in NativeContext keyed by actual DataArray UID,
// with prune/reset/adopt/hash/validation beside sourceZombies. No local actor list.
NativeDayZombie273State* EditNativeDayZombie273(Zombie*,bool create=false);
const NativeDayZombie273State* GetNativeDayZombie273(const Zombie*);
// The host controller owns its live Reanimation, not a copied wire head clock.
// Capture fills head only in the outbound copy; drawing reads the live resource.
inline bool NativeDayZombie273ControllerValid(uint16_t kind,const NativeDayZombie273State& s)
{
    if(!FindFusionDayZombie273Profile(kind)||s.armorStage>2||s.shootEvent>2||!std::isfinite(s.shootCountdown)||
        s.shootCountdown<-.01f||s.shootCountdown>1.6f)return false;
    if(kind==3){if(s.status!=0&&s.status!=2&&s.status!=3)return false;if(s.shadowEnabled!=(s.status!=3))return false;}
    else if(s.status||!s.shadowEnabled)return false;
    if(!FusionDayZombie273Doll(kind)&&kind!=106&&kind!=107&&s.armorStage)return false;
    return FusionDayZombie273Shooter(kind)||(!s.shootEvent&&s.shootCountdown==1.5f);
}
inline bool NativeDayZombie273StateValid(uint16_t kind,const NativeDayZombie273State& s)
{
    if(!NativeDayZombie273ControllerValid(kind,s))return false;
    const auto& h=s.head;
    if(!std::isfinite(h.mAnimTime)||h.mAnimTime<0||h.mAnimTime>1||!std::isfinite(h.mAnimRate)||
        std::abs(h.mAnimRate)>10000||h.mLoopType>5||!LawnNet::NetZombieAnimStateFitsFrames(h,65535))return false;
    if(!FusionDayZombie273Shooter(kind)&&!FusionDayZombie273Doll(kind)&&h.mPresent)return false;
    if(!h.mPresent&&(s.shootEvent||h.mDead||h.mFrameStart||h.mFrameCount||h.mAnimTime||h.mAnimRate||h.mLoopType||h.mLoopCount))return false;
    return true;
}
struct NativeDayZombie273Hooks
{
    // Genuine source tags/Shadow; no native-health/type guesses.
    bool (*plantTraits)(const Plant*,FusionDayZombie273PlantTraits&)=nullptr;
    // Source Pole.OnTriggerStay2D calls GetLadder (slot23), not contact damage.
    void (*poleContact)(Zombie*,Plant*)=nullptr;
    // Sound50/51 and, ONLY after a blocked JumpFail landing, source Star FX:
    // GameAPP.particlePrefab[23]=Particle/Prefabs/Star at Shadow+(0,1.75). Native FX
    // publication/creation belongs to the existing reliable cosmetic route.
    void (*poleEvent)(Zombie*,uint8_t event)=nullptr;
    // Source Doll: MoveSpawnedZombieToTargetRow, Cone.FirstArmorFall, Die(2).
    // Prepare must reserve all resources; finish must not reject after birth.
    bool (*prepareDollFall)(Zombie*)=nullptr;
    void (*finishDollFall)(Zombie* parent,Zombie* child)=nullptr;
    // Actual shared source damage0/4 plus KnockBack(value,reason1). Native
    // weapon flags are not the source damage enum; no allegiance substitution.
    bool (*projectileHitZombie)(Projectile*,Zombie*,int sourceType,int damage,float sourceKnockback)=nullptr;
};
bool IsNativeDayZombie273(const Zombie*);
bool InitializeNativeDayZombie273(Zombie*);
// Presentation allocation only; valid on a validated ghost during adoption.
bool EnsureNativeDayZombie273ShooterHead(Zombie*);
bool UpdateNativeDayZombie273Animation(Zombie*);
bool StartNativeDayZombie273Walk(Zombie*,int blend=0);
bool MoveNativeDayZombie273(Zombie*);
// Call once inside existing host Zombie.Update, outside the native immobilized
// action gate. Head/body clocks still advance only through actual Reanimations.
bool UpdateNativeDayZombie273(Zombie*,const NativeDayZombie273Hooks&);
bool GetNativeDayZombie273Status(const Zombie*,int&);
bool GetNativeDayZombie273Presentation(const Zombie*,const char*&,float& phase);
bool GetNativeDayZombie273HeadPose(const Zombie*,const char*&,float& phase);
bool NativeDayZombie273CanUseSharedBite(const Zombie*);
// First inside native TakeHelmDamage; true consumes handling (also on ghosts).
bool ApplyNativeDayZombie273Armor(Zombie*,int damage,int& overkill,const NativeDayZombie273Hooks&);
// Actual source106 DropArmor /106+108 LoseHeadEvent instantiate Items/Bucket
// at Shadow+(0,1). Hook the source callback, not ordinary armor HP depletion.
Coin* DropNativeDayZombie273Bucket(Zombie*);
bool PrepareNativeDayZombie273HeadLoss(Zombie*,uint32_t nativeDamageFlags);
// True holds source health admission while its genuine Bucket birth is pending.
bool UpdateNativeDayZombie273PendingHeadLoss(Zombie*);
Projectile* CreateNativeDayZombie273Shot(Zombie*);
bool IsNativeDayZombie273HostileProjectile(const Projectile*);
// Root dispatches this before stock projectile movement/collision. Friendly or
// reflected shots use the existing source0/11 ordinary projectile mechanisms.
bool UpdateNativeDayZombie273HostileProjectile(Projectile*,const NativeDayZombie273Hooks&);
// True requires exact tuple AND all unrelated metadata fields clear; Root must
// explicitly add the tuple to its existing validator before creation/adoption.
bool NativeDayZombie273ProjectileMetadataValid(const NativeProjectileData&,ProjectileType,ProjectileMotion);
void NativeDayZombie273Removing(Zombie*);
}
#endif
