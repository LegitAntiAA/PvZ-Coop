#ifndef LAWN_FUSION_NATIVE_STAR_269_H
#define LAWN_FUSION_NATIVE_STAR_269_H
#include "FusionStar269Rules.h"
#include "FusionShooterTiming.h"
#include "../../SexyAppFramework/Rect.h"
#include <array>
#include <cmath>
class Board;class Plant;class Projectile;class Zombie;
namespace LawnFusion
{
struct NativeStar269Reference {uint16_t id=0;uint32_t token=0;};
struct NativeStar269PlantState
{
    bool attracting=false;
    // Only the first profile.storageSlots entries exist in the source backing array.
    std::array<NativeStar269Reference,240> slots{};
};
struct NativeStar269ProjectileState
{
    uint32_t instanceToken=0;
    uint16_t ownerId=0,ownerSlot=0;
    uint32_t ownerToken=0;
    bool accelerated=false,colliderEnabled=true,movingToCircle=true;
    float elapsed=0,angularSpeed=-3,angle=0,trackSpeed=5;
    float targetX=0,targetY=0; // Source targetPosition, not another actor transform.
    uint16_t targetZombieId=0;
    uint32_t targetZombieToken=0;
};
inline bool NativeStar269PlantStateValid(uint16_t kind,const NativeStar269PlantState& s)
{
    const auto* profile=FindFusionStar269Profile(kind);if(!profile||!profile->storageSlots)return false;
    for(std::size_t i=0;i<s.slots.size();++i)
    {
        const auto ref=s.slots[i];if((ref.id==0)!=(ref.token==0))return false;
        if(i>=profile->storageSlots&&ref.id)return false;
        if(ref.id){if(!s.attracting)return false;for(std::size_t j=0;j<i;++j)if(s.slots[j].id==ref.id||s.slots[j].token==ref.token)return false;}
    }
    return true;
}
// Root provides Context full-generation maps. create is host-only; client snapshot
// adoption writes its validated map directly and never initializes another clock.
NativeStar269PlantState* EditNativeStar269PlantState(Plant*,bool create=false);
const NativeStar269PlantState* GetNativeStar269PlantState(const Plant*);
NativeStar269ProjectileState* EditNativeStar269ProjectileState(Projectile*,bool create=false);
const NativeStar269ProjectileState* GetNativeStar269ProjectileState(const Projectile*);
// Root owns the actual shared equipment magnetCount and Board meteorite subsystem.
int GetNativeStar269MagnetCount(const Plant*);
bool TryActivateNativeStar269Meteorite(Plant*);
// Root extends its source10 adapter to arbitrary damage, preserving source
// burn/armor/MC/terrain policy; the Star module owns only circle/row admission.
bool ApplyNativeSourceExplosionDamage(Zombie*,int damage);
// Only real typed NPC26/205/206 use this source49 object/status callback.
// Unimplemented recipient scripts return false and remain an explicit gate.
bool TryApplyNativeStar269UltimateRecipient(Zombie*,bool& objectBroken);
inline bool NativeStar269ProjectileStateValid(uint16_t kind,uint8_t way,const NativeStar269ProjectileState& s)
{
    if(!FusionStar269ProjectileClass(kind)||!s.instanceToken||((s.ownerId==0)!=(s.ownerToken==0))||
       ((s.targetZombieId==0)!=(s.targetZombieToken==0))||!std::isfinite(s.elapsed)||s.elapsed<0||s.elapsed>6.02f||
       !std::isfinite(s.angularSpeed)||s.angularSpeed< -6||s.angularSpeed> -3||!std::isfinite(s.angle)||s.angle< -50||s.angle>6.283186f||
       !std::isfinite(s.trackSpeed)||(kind==49?(s.trackSpeed!=8&&s.trackSpeed!=16):(s.trackSpeed!=5&&s.trackSpeed!=10))||
       !std::isfinite(s.targetX)||!std::isfinite(s.targetY)||std::abs(s.targetX)>40||std::abs(s.targetY)>40)return false;
    if(s.ownerId)return way==10&&s.accelerated&&!s.colliderEnabled&&s.ownerSlot<240;
    return !s.ownerSlot&&way!=10;
}
bool IsNativeStar269Plant(const Plant*);
const FusionShooterTimingProfile* GetNativeStar269Timing(const Plant*);
bool InitializeNativeStar269Plant(Plant*);
bool UpdateNativeStar269Plant(Plant*);
// Once per host native tick after native shooting counter decrement and before
// sampling animated source nodes; callers must not also advance legacy phase.
void UpdateNativeStar269PlantPhase(Plant*);
bool DispatchNativeStar269Shot(Plant*);
bool GetNativeStar269VisualPose(const Plant*,const char*& state,float& seconds);
void NativeStar269PlantRemoving(Plant*,int sourceReason);
bool NativeStar269Invulnerable(const Plant*);
// Call after AttachNativeProjectile for every proven StarBullet-derived source
// payload39..49 except47. Factory47=MagnetCactusBullet:Bullet is not a Star.
// Idempotent; existing real projectiles are retained.
bool NativeStar269ProjectileCreated(Projectile*);
void NativeStar269ProjectileRemoving(Projectile*);
bool IsNativeStar269Projectile(const Projectile*);
bool NativeStar269ProjectileColliderEnabled(const Projectile*);
bool MoveNativeStar269Projectile(Projectile*);
bool GetNativeStar269ProjectileCenter(const Projectile*,float& x,float& y);
bool GetNativeStar269ProjectileRect(const Projectile*,Sexy::Rect&);
bool HandleNativeStar269ProjectileCollision(Projectile*);
bool ApplyNativeStar269ProjectileImpact(Projectile*,Zombie*);
}
#endif
