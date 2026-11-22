#ifndef LAWN_FUSION_NATIVE_ZOMBIES_H
#define LAWN_FUSION_NATIVE_ZOMBIES_H

#include "FusionZombieRules.h"
#include "../Net/NetCommon.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

class Board;
class Zombie;
class Plant;

namespace LawnFusion
{
enum class NativeZombieTargetEntity : uint8_t { None=0, Plant=1, Zombie=2 };

// Persistent source identity/float truth, keyed by the real local DataArray UID.
// Received tokens remain opaque host identities; clients do not resolve targets.
struct NativeSourceZombieData
{
    uint16_t kind=0xffff;
    uint32_t instanceToken=0;
    float originSpeed=1.0f;
    float health=270.0f;
    NativeZombieTargetEntity targetEntity=NativeZombieTargetEntity::None;
    uint16_t targetId=0;
    uint32_t targetGeneration=0;
    uint16_t sourceParentId=0;
    uint32_t sourceParentToken=0;
    bool dieWithParent=false;
    uint8_t firstArmorStage=0; // Persistent appearance: first9/111/109, second5/15/104.
    // Day273 status/armor/head variants live in their separately validated
    // full-UID state; they must not overload these legacy Paper/vehicle fields.
    uint8_t paperShootTicks=0,paperShootEvent=0;
    LawnNet::NetZombieAnimState paperHead; // Actual attached native Reanimation, never a client clock.
    bool vehicleStopped=false; // Actual source18 beforeDying/collider-off presentation.
};

// Host-only consumed positions of the existing body Reanimation. No timer,
// independently advanced phase, actor array or animation evaluator is owned here.
struct NativeSourceZombieCursor
{
    int motionFrameStart=-1,attackFrameStart=-1;
    FusionZombieClipPosition motion{},attack{};
    bool motionValid=false,attackValid=false;
};

inline bool NativeSourceZombieDataValid(const NativeSourceZombieData& data,int maximumHealth)
{
    const auto& head=data.paperHead;
    if(data.paperShootTicks>75||data.paperShootEvent>2||
        !std::isfinite(head.mAnimTime)||head.mAnimTime<0||head.mAnimTime>1||
        !std::isfinite(head.mAnimRate)||std::abs(head.mAnimRate)>10000||head.mLoopType>5||
        !LawnNet::NetZombieAnimStateFitsFrames(head,65535))return false;
    if(!head.mPresent&&(head.mDead||head.mFrameStart||head.mFrameCount||head.mAnimTime||
        head.mAnimRate||head.mLoopType||head.mLoopCount))return false;
    if(data.kind!=104&&(data.paperShootTicks||data.paperShootEvent||head.mPresent))return false;
    if(data.kind!=18&&data.vehicleStopped)return false;
    if(((data.kind==9||data.kind==111||data.kind==109||data.kind==5||data.kind==15||data.kind==104)?data.firstArmorStage>2:data.firstArmorStage!=0)||!FindFusionZombieProfile(data.kind)||!data.instanceToken||
        !std::isfinite(data.originSpeed)||data.originSpeed<kFusionZombieMinimumOriginSpeed||
        data.originSpeed>kFusionZombieMaximumOriginSpeed||!FusionZombieHealthFitsMaximum(data.health,maximumHealth)||
        bool(data.sourceParentId)!=bool(data.sourceParentToken)||(data.dieWithParent&&!data.sourceParentId))return false;
    if(data.targetEntity==NativeZombieTargetEntity::None)return !data.targetId&&!data.targetGeneration;
    return (data.targetEntity==NativeZombieTargetEntity::Plant||data.targetEntity==NativeZombieTargetEntity::Zombie)&&data.targetId&&data.targetGeneration;
}

// The float source pre-update check owns death admission. A zero integer cast
// after headless bleed must not prematurely activate native death callbacks.
inline int NativeSourceZombieLivenessHealth(float health)
{
    if(!std::isfinite(health)||static_cast<double>(health)>(std::numeric_limits<int>::max)())return 0;
    return health<=1.0f?1:static_cast<int>(std::ceil(static_cast<double>(health)));
}

// Explicit opt-in only. No wave/catalog/global native factory is redirected.
// Existing AddZombieInRow initialization and co-op/endless policy are retained.
// True only during the synchronous native carrier initialization of a typed
// source birth, before its source identity can be attached to that carrier.
bool IsNativeSourceZombieBirth(const Board* board);
// Whole-batch peak capacity for the admitted ordinary Day carriers. No dead
// resource is credited before EffectSystem actually returns it to its pool.
bool CanAllocateNativeSourceDayBatch274(Board* board,std::span<const uint16_t> kinds,std::string* error=nullptr);
Zombie* SpawnNativeSourceZombie(Board* board,uint16_t kind,int row,int fromWave,
    int sourcePreset=kFusionZombieSourcePreset);
struct NativeSourceZombieSpawnRequest
{
    uint16_t kind=0xffff;
    int row=0,fromWave=0,sourcePreset=kFusionZombieSourcePreset;
    bool positioned=false,friendly=false,withEffect=false,dieWithParent=false;
    float shadowX=0;
    uint16_t parentId=0;
    uint32_t parentToken=0;
};
Zombie* SpawnNativeSourceZombie(Board* board,const NativeSourceZombieSpawnRequest& request);
Zombie* TrySpawnNativeAdventureWaveVariant(Board* board, int nativeType, int row, int wave, bool allowVehicle);
int GetNativeSourceZombieCarrier(uint16_t kind);
bool IsNativeSourceZombie(const Zombie* zombie);
bool ApplyNativeSourceZombie111Armor(Zombie*,int damage,int& remaining);
bool EmitNativeSpecialZombie269VehicleFx(Zombie*,uint16_t sourceParticle,float x,float y);
bool GetNativeSpecialZombie269PlantTraits(const Plant*,bool& isCaltrop,bool& isNut,int& sourceLevel);
void RefreshNativeSpecialZombie269PaperArmor(Zombie*);

// Root calls this before UpdatePlaying's positive-health assertion. True means
// a typed source death was admitted; caller skips the rest of that live update.
bool UpdateNativeSourceZombieHealth(Zombie* zombie);
// True means typed source subtraction was handled, including authority refusal;
// caller must not also execute the native mBodyHealth -= damage statement.
bool ApplyNativeSourceZombieBodyDamage(Zombie* zombie,int damage);
int GetNativeSourceZombieBiteDamage(const Zombie* zombie,int nativeDamage);
// Root replaces only the typed1022 raw integer-halving block with this call.
// False rejects atomically; do not fall back to raw mutation for a typed actor.
// Same admitted BeSmall: float HP, integer max/armor and existing native scale.
bool TryShrinkNativeSourceZombie(Zombie* zombie);

// Return true only for typed actors; old native NORMAL test controls fall through.
bool UpdateNativeSourceZombieAnimation(Zombie* zombie);
bool StartNativeSourceZombieWalk(Zombie* zombie,int blendTime);
bool UpdateNativeSourceZombieWalking(Zombie* zombie);
bool UpdateNativeSourceZombieAttack(Zombie* zombie);

// Read-only source state plus existing normalized native body phase. Renderer
// maps this to its actual source clip duration; it must not apply speed twice.
bool GetNativeSourceZombiePresentation(const Zombie* zombie,const char*& stateName,float& normalizedPhase);
}

#endif
