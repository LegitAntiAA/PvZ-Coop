#include "FusionNativeWire.h"
#include "FusionMorePlants269Rules.h"
#include "FusionRollingActors269Rules.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionLastPlants269Rules.h"
#include "FusionRemainingPlants269Rules.h"
#include "FusionCherryShooterRules.h"
#include "FusionShooterTiming.h"
#include "FusionStraightProjectileRules.h"
#include "FusionProducerRules.h"
#include "FusionColdRules.h"
#include "FusionFireRules.h"
#include "FusionDeathRules.h"
#include "FusionChomperRules.h"
#include "FusionLobberRules.h"
#include "FusionButterRules.h"
#include "FusionSplitShooterRules.h"
#include "FusionRecipe.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionShooterBatchRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionDefenderRules.h"
#include "FusionEconomyRules.h"
#include "FusionPoisonRules.h"
#include "FusionAction266Rules.h"
#include "FusionAction267Rules.h"
#include "FusionUmbrellaBatchRules.h"
#include "FusionWaterPlants267Rules.h"
#include "FusionSeaLantern267Rules.h"
#include "FusionShooter267Rules.h"
#include "FusionWater268Rules.h"
#include "FusionCannon268Rules.h"
#include "FusionNativeEquipment268.h"
#include "FusionChomper268Rules.h"
#include "FusionShulk268Rules.h"
#include "FusionExtras268Rules.h"
#include "FusionSummon269Rules.h"
#include "FusionSolar269Rules.h"
#include "../Net/NetCommon.h"
#include "../Net/NetGameplayPolicy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace LawnFusion
{
namespace
{
constexpr uint32_t kMagic = 0x31534e46; // FNS1
constexpr uint16_t kVersion = kNativeFusionSnapshotVersion;
constexpr size_t kMaximumBytes = LawnNet::NET_MAX_FRAME - 8;

struct Writer
{
    LawnNet::NetBuffer& b;
    bool valid = true;
    void U8(uint8_t& v) { b.WriteU8(v); }
    void U16(uint16_t& v) { b.WriteU16(v); }
    void U32(uint32_t& v) { b.WriteU32(v); }
    void I32(int& v) { b.WriteI32(v); }
    void F32(float& v) { b.WriteF32(v); }
    void Bool(bool& v) { b.WriteU8(v ? 1 : 0); }
};
struct Reader
{
    LawnNet::NetBuffer& b;
    bool valid = true;
    void U8(uint8_t& v) { if (valid) valid = b.ReadU8(&v); }
    void U16(uint16_t& v) { if (valid) valid = b.ReadU16(&v); }
    void U32(uint32_t& v) { if (valid) valid = b.ReadU32(&v); }
    void I32(int& v) { if (valid) valid = b.ReadI32(&v); }
    void F32(float& v) { if (valid) valid = b.ReadF32(&v); }
    void Bool(bool& v) { uint8_t n = 0; U8(n); if (n > 1) valid = false; if (valid) v = n != 0; }
};

template <typename IO> void DayFields273(IO& io,NativeDayZombie273State& state)
{
    io.F32(state.shootCountdown);io.U8(state.status);io.U8(state.armorStage);
    io.U8(state.shootEvent);io.Bool(state.shadowEnabled);
}
void Fields(Writer& io,NativeDayZombie273State& state)
{
    DayFields273(io,state);
    if(!LawnNet::NetEncodeZombieAnimState(state.head,io.b))io.valid=false;
}
void Fields(Reader& io,NativeDayZombie273State& state)
{
    DayFields273(io,state);
    if(io.valid)io.valid=LawnNet::NetDecodeZombieAnimState(io.b,state.head);
}
template <typename IO> void Fields(IO& io, FusionBankState& v)
{
    io.U32(v.epoch); io.U32(v.revision);
    uint8_t phase = (uint8_t)v.phase; io.U8(phase); v.phase = (FusionBankPhase)phase;
    io.U8(v.difficulty); io.U8(v.activeMask); io.U8(v.readyMask); io.U8(v.mysteryMask); io.U8(v.targetCount);
    for (auto& quota : v.quotas) io.U8(quota);
    io.U32(v.nextPacketId);
}
template <typename IO> void Fields(IO& io, FusionBankPacket& v)
{
    io.U16(v.id); io.U16(v.kind); io.Bool(v.isExtra); io.U8(v.owner); io.U8(v.randomOrigin);
    io.Bool(v.mysteryPending); io.I32(v.price); io.F32(v.rechargeSeconds); io.F32(v.remainingRechargeSeconds);
}
template <typename IO> void Fields(IO& io, NativePacketData& v)
{
    io.U16(v.selectionId); io.U16(v.kind); io.Bool(v.isExtra); io.U8(v.owner); io.U8(v.randomOrigin); io.I32(v.price); io.F32(v.rechargeSeconds);
}
template <typename IO> void Fields(IO& io, NativePlantData& v)
{
    io.U16(v.kind); io.U16(v.firstParent); io.U16(v.secondParent); io.U16(v.packetId);
    io.U8(v.owner); io.U8(v.randomOrigin); io.I32(v.productionTicks); io.I32(v.pendingProductionTicks);
    io.U8(v.producerRewardFlags); io.U8(v.deathEventFlags);
    io.U8(v.damageStage);
    io.F32(v.idleSeconds); io.F32(v.shootSeconds); io.F32(v.animationSpeed);
    io.U32(v.instanceToken);
    io.U32(v.batchShotCount); io.U16(v.batchRageTicks); io.U16(v.batchBagCount); io.U8(v.batchMode);
    io.U16(v.waterTargetId); io.U32(v.waterTargetToken);
}
template <typename IO> void Fields(IO& io, NativeProjectileData& v)
{
    io.U16(v.kind); io.I32(v.damage); io.I32(v.sunOnHit); io.U8(v.specialType);
    io.U8(v.fireState); io.U16(v.lastTorchId); io.U32(v.lastTorchToken);
    io.Bool(v.melonEnhanced); io.U8(v.effectVariant);
    io.Bool(v.isZombieBullet); io.Bool(v.hitLatched); io.Bool(v.umbrellaed);
    io.Bool(v.goldAttracted);
}

template <typename IO> void Fields(IO& io, NativeZombieColdData& v)
{ io.I32(v.level); io.I32(v.threshold); io.I32(v.chilledTicks); io.I32(v.frozenTicks); }
template <typename IO> void Fields(IO& io, NativeZombieButterData& v)
{ io.I32(v.ticks); io.Bool(v.visible); }
template <typename IO> void Fields(IO& io, NativeZombieActionData& v)
{
    io.U8(v.colorMask); io.F32(v.scale); io.Bool(v.doomOnDeath); io.Bool(v.jalaedOnDeath);
    io.I32(v.poisonLevel); io.I32(v.poisonTicks); io.I32(v.poisonDamageTicks);
    io.I32(v.waterGrapTicks); io.I32(v.waterGrapCount);
}

template <typename IO> void Fields(IO& io, NativeSourceZombieData& v)
{
    io.U16(v.kind); io.U32(v.instanceToken); io.F32(v.originSpeed); io.F32(v.health);
    uint8_t entity = static_cast<uint8_t>(v.targetEntity); io.U8(entity);
    v.targetEntity = static_cast<NativeZombieTargetEntity>(entity);
    io.U16(v.targetId); io.U32(v.targetGeneration);
    io.U16(v.sourceParentId);io.U32(v.sourceParentToken);io.Bool(v.dieWithParent);io.U8(v.firstArmorStage);
    io.U8(v.paperShootTicks);io.U8(v.paperShootEvent);
    auto& head=v.paperHead;
    io.Bool(head.mPresent);io.Bool(head.mDead);io.U16(head.mFrameStart);io.U16(head.mFrameCount);
    io.F32(head.mAnimTime);io.F32(head.mAnimRate);io.U8(head.mLoopType);io.U8(head.mLoopCount);
    io.Bool(v.vehicleStopped);
}

void Fields(Writer& io,NativeStar269PlantState& v)
{
    io.Bool(v.attracting);uint16_t count=0;
    for(const auto& ref:v.slots)if(ref.id)++count;
    io.U16(count);
    for(size_t i=0;i<v.slots.size();++i)if(v.slots[i].id)
    {auto slot=static_cast<uint8_t>(i);io.U8(slot);io.U16(v.slots[i].id);io.U32(v.slots[i].token);}
}
void Fields(Reader& io,NativeStar269PlantState& v)
{
    io.Bool(v.attracting);uint16_t count=0;io.U16(count);
    if(count>v.slots.size()){io.valid=false;return;}
    std::array<bool,240> used{};
    for(unsigned i=0;i<count&&io.valid;++i)
    {
        uint8_t slot=0;io.U8(slot);
        if(slot>=v.slots.size()||used[slot]){io.valid=false;return;}
        used[slot]=true;io.U16(v.slots[slot].id);io.U32(v.slots[slot].token);
        if(!v.slots[slot].id||!v.slots[slot].token)io.valid=false;
    }
}
template <typename IO> void Fields(IO& io,NativeStar269ProjectileState& v)
{
    io.U32(v.instanceToken);io.U16(v.ownerId);io.U16(v.ownerSlot);io.U32(v.ownerToken);
    io.Bool(v.accelerated);io.Bool(v.colliderEnabled);io.Bool(v.movingToCircle);
    io.F32(v.elapsed);io.F32(v.angularSpeed);io.F32(v.angle);io.F32(v.trackSpeed);
    io.F32(v.targetX);io.F32(v.targetY);io.U16(v.targetZombieId);io.U32(v.targetZombieToken);
}
template <typename IO> void Fields(IO& io,NativeDoom269PlantState& v)
{io.F32(v.interval);io.I32(v.ordinal);io.I32(v.power);io.Bool(v.queuedTrigger);}

template <typename IO> void Fields(IO& io,NativeCompanion269State& v)
{
    uint8_t family=static_cast<uint8_t>(v.family);io.U8(family);v.family=static_cast<FusionCompanion269Family>(family);
    io.U32(v.instanceToken);io.U16(v.parentId);io.U32(v.parentToken);io.U16(v.targetId);io.U32(v.targetToken);
    io.F32(v.offsetX);io.F32(v.offsetY);io.F32(v.positionOffset);io.F32(v.shootCountdown);
    io.Bool(v.onTarget);io.Bool(v.smallVariant);io.Bool(v.shotEmitted);io.I32(v.ageTicks);io.I32(v.shootStartAge);
}
template <typename IO> void Fields(IO& io,FusionWorld269Reference& v){io.U16(v.id);io.U32(v.token);}
template <typename IO> void Fields(IO& io,NativeDroppedCard269Data& v)
{io.U16(v.kind);io.I32(v.cost);io.U32(v.instanceToken);io.U8(v.heldSlot);io.Bool(v.landed);io.Bool(v.flashing);}
template <typename IO> void Fields(IO& io,NativeFertilize269Data& v)
{
    io.U32(v.instanceToken);io.U16(v.targetId);io.U32(v.targetToken);
    uint8_t phase=uint8_t(v.phase);io.U8(phase);v.phase=NativeFertilize269Phase(phase);
    io.U8(v.heldSlot);io.I32(v.phaseStartAge);io.F32(v.anchorX);io.F32(v.anchorY);
}
template <typename IO> void Fields(IO& io,NativeWorldRemainder269Pending& v)
{io.I32(v.deadline);io.U8(v.type);}
void Fields(Writer& io,NativeWorldRemainder269BoardState& v)
{
    io.Bool(v.initialized);io.I32(v.passiveDeadline);io.I32(v.activeDeadline);io.I32(v.ultimateDeadline);
    auto count=uint16_t(v.pending.size());io.U16(count);for(auto& p:v.pending)Fields(io,p);
}
void Fields(Reader& io,NativeWorldRemainder269BoardState& v)
{
    io.Bool(v.initialized);io.I32(v.passiveDeadline);io.I32(v.activeDeadline);io.I32(v.ultimateDeadline);
    uint16_t count=0;io.U16(count);if(count>1024){io.valid=false;return;}
    v.pending.resize(count);for(auto& p:v.pending)Fields(io,p);
}
template <typename IO> void Fields(IO& io,NativeWorldRemainder269Reference& v)
{io.U16(v.id);io.U32(v.token);}
template <typename IO> void Fields(IO& io,NativeWorldRemainder269AuxState& v)
{
    uint8_t family=uint8_t(v.family);io.U8(family);v.family=NativeWorldRemainder269Family(family);
    io.U32(v.instanceToken);Fields(io,v.parent);io.F32(v.targetX);io.F32(v.targetY);io.I32(v.parentDamage);
    io.I32(v.ageTicks);io.I32(v.nextShotAge);io.U8(v.shots);io.U8(v.type);io.Bool(v.ready);io.Bool(v.sun);io.Bool(v.impacted);
}
template <typename IO> void Fields(IO& io,NativeWorldRemainder269ShotState& v)
{Fields(io,v.parent);io.I32(v.ageTicks);}

template <typename IO> void Fields(IO& io,NativeWheat269Snapshot& v)
{io.Bool(v.isFromWheat);io.F32(v.wheatTime);}

template <typename IO> void Fields(IO& io,NativeRollingActor269Snapshot& v)
{
    io.U16(v.kind);io.U16(v.factoryKind);io.U32(v.instanceToken);io.I32(v.bornTick);
    io.F32(v.originX);io.F32(v.originY);io.F32(v.rootOffsetX);io.F32(v.rootOffsetY);io.I32(v.health);
}
template <typename IO> void Fields(IO& io,NativeWorld269PlantState& v)
{
    for(auto& ref:v.zombies)Fields(io,ref);for(auto& ref:v.plants)Fields(io,ref);
    Fields(io,v.receiver);Fields(io,v.connectedPlant);
    io.I32(v.energy);io.I32(v.maximumEnergy);io.I32(v.contributors);io.Bool(v.connected);io.Bool(v.main);io.I32(v.explosionDeadline);
}
template <typename IO> void Fields(IO& io,NativeWorld269BeamState& v)
{
    io.U32(v.instanceToken);io.U16(v.ownerId);io.U32(v.ownerToken);
    io.F32(v.startX);io.F32(v.startY);io.F32(v.endX);io.F32(v.endY);io.F32(v.width);io.I32(v.sortingRow);io.I32(v.ageTicks);
}
void Fields(Writer& io,NativeCompanions269PlantState& v)
{
    auto count=static_cast<uint16_t>(v.children.size());io.U16(count);
    for(auto& child:v.children){io.U16(child.id);io.U32(child.token);}
}
void Fields(Reader& io,NativeCompanions269PlantState& v)
{
    uint16_t count=0;io.U16(count);if(count>1024){io.valid=false;return;}
    v.children.resize(count);
    for(auto& child:v.children){io.U16(child.id);io.U32(child.token);}
}

template <typename IO> void Fields(IO& io, GarlicActionState& v)
{
    uint8_t phase = static_cast<uint8_t>(v.phase); io.U8(phase); v.phase = static_cast<GarlicActionPhase>(phase);
    io.Bool(v.certainRow); io.U16(v.waitTicks); io.U16(v.moveElapsedTicks); io.U16(v.faceTicks);
    uint8_t origin = static_cast<uint8_t>(v.originRow), target = static_cast<uint8_t>(v.targetRow);
    io.U8(origin); io.U8(target);
    // Decode the maximum native terrain first; frame validation applies its
    // actual Day5/Pool6 row and terrain compatibility rules atomically.
    v.originRow = origin == 255 ? -1 : origin < 6 ? static_cast<int8_t>(origin) : -2;
    v.targetRow = target == 255 ? -1 : target < 6 ? static_cast<int8_t>(target) : -2;
    io.F32(v.startY); io.F32(v.targetY);
}
template <typename IO> void Fields(IO& io, NativeCoinData& v)
{ io.I32(v.value); io.Bool(v.money); }
template <typename IO> void Fields(IO& io, NativeEquipmentData& v)
{
    uint8_t type=static_cast<uint8_t>(v.type),factory=static_cast<uint8_t>(v.factoryType),phase=static_cast<uint8_t>(v.phase);
    io.U8(type);io.U8(factory);io.U8(phase);
    v.type=static_cast<FusionEquipmentType>(type);v.factoryType=static_cast<FusionEquipmentType>(factory);v.phase=static_cast<NativeEquipmentPhase>(phase);
    io.U32(v.instanceToken);io.U16(v.sourcePlantId);io.U32(v.sourcePlantToken);io.U8(v.claimSlot);
    io.F32(v.anchorX);io.F32(v.anchorY);io.F32(v.pullStartX);io.F32(v.pullStartY);io.F32(v.pullEndX);io.F32(v.pullEndY);
    io.F32(v.gravity);io.I32(v.phaseStartAge);io.Bool(v.disappear);io.Bool(v.flashing);
    io.Bool(v.weapon);
    for(auto& element:v.attachmentBasis)io.F32(element);
}
template <typename IO> void Fields(IO& io, NativeEquipmentPlantState& v)
{
    io.U32(v.seaCallbacks);io.U16(v.componentSize);io.U16(v.targetItemId);io.U32(v.targetItemToken);
    uint8_t type=static_cast<uint8_t>(v.lastCapturedType);io.U8(type);v.lastCapturedType=static_cast<FusionEquipmentType>(type);
    io.U8(v.retainedRowRange);io.Bool(v.seaGrown);
    uint8_t draw=static_cast<uint8_t>(v.pendingItemDraw);io.U8(draw);v.pendingItemDraw=draw==255?-1:static_cast<int8_t>(draw);
}
template <typename IO> void Fields(IO& io, int& v) { io.I32(v); }
template <typename IO> void Fields(IO& io, uint32_t& v) { io.U32(v); }
template <typename IO> void GloveFields(IO& io, NativeSnapshotFrame& frame)
{
    for (auto& hold : frame.gloveHolds) { io.U16(hold.plantId); io.U32(hold.instanceToken); }
    io.I32(frame.gloveCooldownTicks);
}
template <typename IO, typename T> void Fields(IO& io, NativeSnapshotRecord<T>& v)
{ io.U16(v.id); Fields(io, v.data); }
template <typename T> void Vector(Writer& io, std::vector<T>& v)
{
    uint16_t count = (uint16_t)v.size(); io.U16(count);
    for (auto& item : v) Fields(io, item);
}
template <typename T> void Vector(Reader& io, std::vector<T>& v, size_t maximum)
{
    uint16_t count = 0; io.U16(count);
    if (!io.valid || count > maximum) { io.valid = false; return; }
    v.resize(count); for (auto& item : v) Fields(io, item);
}
template <typename T> bool UniqueIds(const std::vector<NativeSnapshotRecord<T>>& v, size_t maximum)
{
    if (v.size() > maximum) return false;
    std::set<uint16_t> seen;
    for (const auto& item : v) if (!item.id || !seen.insert(item.id).second) return false;
    return true;
}
bool Equipment268PlantValid(const NativePlantData& v)
{
    const auto* p=FindFusionEquipmentProfile(v.kind);if(!p||v.batchShotCount||v.batchRageTicks||v.batchBagCount)return false;
    const auto mode=v.batchMode;
    if(v.kind==1102)
    {if(mode!=kEquipmentBlow&&mode!=kEquipmentBlowLoop)return false;return v.productionTicks==0&&v.pendingProductionTicks==0&&v.shootSeconds>=0&&
        (mode==kEquipmentBlow?v.shootSeconds<1.2666667f/v.animationSpeed:v.shootSeconds<=10);}
    if(p->family==FusionEquipmentFamily::Capture)
    {
        if(v.productionTicks||v.pendingProductionTicks)return false;
        if(mode==kEquipmentIdle||mode==kEquipmentHolding)return v.shootSeconds==-1;
        if(mode==kEquipmentAttract)return v.shootSeconds>=0&&v.shootSeconds<(v.kind==1101?.6333333f:.68333334f)/v.animationSpeed;
        return v.kind==1101&&mode==kEquipmentGrow&&v.shootSeconds>=0&&v.shootSeconds<1.f/v.animationSpeed;
    }
    if(mode||v.shootSeconds!=-1)return false;
    if(v.kind!=1154)return !v.productionTicks&&!v.pendingProductionTicks;
    return FindFusionProducerProfile(v.kind)&&v.productionTicks<=FusionProducerMaximumTicks(v.kind)&&v.pendingProductionTicks<=kFusionProducerDelayTicks;
}
bool Water268PlantValid(const NativePlantData& v)
{
    if(!FusionWater268PlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode))return false;
    const bool capture=v.kind==15||v.kind==1049||v.kind==1156||v.kind==1050||v.kind==1051||v.kind==1066;
    if(capture)
    {
        if(!FusionWater268CaptureStateValid(v.kind,v.batchMode,v.productionTicks,v.pendingProductionTicks,v.waterTargetId,v.waterTargetToken,v.animationSpeed))return false;
        if(v.kind==15||v.kind==1049||v.kind==1156)return v.shootSeconds==-1;
        if(v.kind==1050){int ticks=0;return TryGetFusionWater268SearchTicks(v.animationSpeed,ticks)&&
            (v.shootSeconds==-1||(v.shootSeconds>=0&&v.shootSeconds<=ticks*.01f));}
    }
    else
    {
        if(v.waterTargetId||v.waterTargetToken||v.pendingProductionTicks)return false;
        if(v.kind==252)
        {const int maximum=int(std::ceil(double(12.433333f)*double(.8304291f)/(4*v.animationSpeed)*100));if(v.productionTicks>maximum)return false;}
        else if(v.kind==1076||v.kind==1083)
        {
            if(v.batchMode==2)
            {const int grow=int(std::ceil(100./(v.kind==1076?1:v.animationSpeed)));return v.batchBagCount==1&&v.productionTicks<=grow&&v.shootSeconds>=0&&v.shootSeconds<=grow*.01f;}
            if(v.productionTicks>(v.batchBagCount?0:v.kind==1076?12001:6001))return false;
        }
        else if(v.productionTicks)return false;
    }
    const auto* timing=FindFusionWater268Timing(v.kind,(v.kind==1076||v.kind==1083)&&v.batchBagCount>0,v.batchMode==1);
    return timing&&(v.shootSeconds==-1||(v.shootSeconds>=0&&v.shootSeconds<FusionShooterDuration(*timing,v.animationSpeed)));
}
bool Cannon268PlantValid(const NativePlantData& v)
{
    const auto* p=FindFusionCannon268Profile(v.kind);
    if(!p||!FusionCannon268StateValid(v.kind,v.batchMode,v.productionTicks,v.pendingProductionTicks,v.batchRageTicks,v.batchBagCount))return false;
    if(p->manual)
    {
        if(v.batchMode==0)return v.shootSeconds==-1;
        if(v.batchMode==2&&v.shootSeconds==-1)return true;
        if(v.shootSeconds<0)return false;
        const auto* timing=FindFusionCannon268Timing(v.kind,v.batchMode==1||v.batchMode==2);
        int ticks=0;return timing&&TryGetFusionCannon268ShotCounter(*timing,v.animationSpeed,ticks)&&v.shootSeconds<=ticks*.01f;
    }
    if(v.shootSeconds==-1)return v.batchMode==0;
    if(v.shootSeconds<0)return false;
    const char* state=v.batchMode?(v.kind==1139?"shoot3":"shoot2"):"shoot";
    const auto* timing=FindFusionCannon268LobberTiming(v.kind,state);
    if(v.kind==1139&&!v.batchMode)
    {const auto* alternate=FindFusionCannon268LobberTiming(v.kind,"shoot2");return v.shootSeconds<(std::max)(FusionShooterDuration(*timing,v.animationSpeed),FusionShooterDuration(*alternate,v.animationSpeed));}
    return timing&&v.shootSeconds<FusionShooterDuration(*timing,v.animationSpeed);
}
bool SupportedPlant(uint16_t kind) { return GetNativeSeedForPlant(kind) != SEED_NONE; }
bool ValidNativeAncestry(const NativePlantData& data)
{
    // A recipe can produce a base kind (e.g. Puff + Scaredy produces Puff).
    // Only parentless plants use the direct acquisition rule.
    if (data.firstParent == kFusionNoPlant && data.secondParent == kFusionNoPlant &&
        (IsNativeBasePlantKind(data.kind) || data.kind == 1020 || data.kind == 1096 || data.kind == 1161)) return true;
    if (data.kind == 1028 && data.firstParent == 1027 && data.secondParent == kFusionNoPlant) return true;
    if (IsFusionMoneyUpgradeAncestry(data.firstParent,data.secondParent,data.kind)) return true;
    if(data.secondParent==kFusionNoPlant&&((data.firstParent==935&&(data.kind==911||data.kind==922))||
        (data.firstParent==1082&&data.kind==912)))return true;
    if (!IsNativeBasePlantKind(data.kind) &&
        data.firstParent == kFusionNoPlant && data.secondParent == kFusionNoPlant &&
        (data.owner == LawnNet::NET_SHARED_OWNER||data.owner<kFusionBankPlayers) && !data.packetId && !data.randomOrigin) return true;
    const auto* recipe = FindFusionRecipe(data.firstParent, data.secondParent);
    return recipe && recipe->result == data.kind;
}
}

bool ValidateNativeSnapshotFrame(const NativeSnapshotFrame& frame, const std::vector<FusionBankEligibleCard>& eligible)
{
    if (!NativeFusionBackgroundValid(frame.background) || !frame.epoch || frame.epoch != frame.selection.epoch || frame.gloveCooldownTicks < 0 || frame.gloveCooldownTicks > 1000 ||
        !FusionSeedBank::ValidateSnapshot(frame.selection, eligible, kNativeFusionBankCapacity) ||
        !UniqueIds(frame.packets, 32) || !UniqueIds(frame.plants, 1024) ||
        !UniqueIds(frame.projectiles, 1024) || !UniqueIds(frame.coldZombies, 1024) ||
        !UniqueIds(frame.butterZombies, 1024) || !UniqueIds(frame.zombieActions, 1024) ||
        !UniqueIds(frame.coins, 1024) || !UniqueIds(frame.garlicZombies, 1024) ||
        !UniqueIds(frame.sourceZombies, 1024) || !UniqueIds(frame.star269Plants,1024) ||
        !UniqueIds(frame.star269Projectiles,1024) || !UniqueIds(frame.doom269Plants,1024) || !UniqueIds(frame.equipmentCoins,1024) ||
        !UniqueIds(frame.equipmentPlants,1024) || !UniqueIds(frame.zombieIdentities,1024) ||
        !UniqueIds(frame.companions269,1024) || !UniqueIds(frame.companionPlants269,1024) ||
        !UniqueIds(frame.worldPlants269,1024)||!UniqueIds(frame.worldBeams269,1024)||!UniqueIds(frame.droppedCards269,128)||
        !UniqueIds(frame.rollingPlants269,1024)||!UniqueIds(frame.fertilizeCoins269,1024)||
        !UniqueIds(frame.worldRemainder269Aux,1024)||!UniqueIds(frame.worldRemainder269Shots,1024)||!UniqueIds(frame.wheatPlants269,1024)||
        !UniqueIds(frame.dayZombies273,1024)||!NativeWorldRemainder269BoardStateValid(frame.worldRemainder269Board)||
        !FusionAdventureSessionValid273(frame.adventure)||frame.money < 0 || frame.money > 99999) return false;
    if(frame.adventure.enabled)
    {
        const int sourceBackground=(frame.adventure.run.level-1)/9;
        if(frame.background!=sourceBackground)return false;
    }
    for(const auto& record:frame.dayZombies273)
    {
        const auto source=std::find_if(frame.sourceZombies.begin(),frame.sourceZombies.end(),
            [&](const auto& value){return value.id==record.id;});
        if(source==frame.sourceZombies.end()||!NativeDayZombie273StateValid(source->data.kind,record.data))return false;
    }
    for(const auto& source:frame.sourceZombies)
        if(FindFusionDayZombie273Profile(source.data.kind)&&std::none_of(frame.dayZombies273.begin(),frame.dayZombies273.end(),
            [&](const auto& value){return value.id==source.id;}))return false;
    const bool committedPhase = frame.selection.phase == FusionBankPhase::Intro || frame.selection.phase == FusionBankPhase::Playing;
    if (frame.selectionCommitted != committedPhase) return false;
    // Source duck variants require the actual Pool/Fog background.
    if(NativeFusionRowsForBackground(frame.background)!=6)
        for(const auto& source:frame.sourceZombies)if(const auto* p=FindFusionZombieProfile(source.data.kind);p&&p->waterOnly)return false;
    if (frame.packets.size() != frame.selection.packets.size()) return false;
    std::set<uint16_t> selectionIds;
    for (const auto& record : frame.packets)
    {
        const auto& v = record.data;
        if (v.owner >= kFusionBankPlayers || v.randomOrigin > 10 || !std::isfinite(v.rechargeSeconds)) return false;
        const auto selected = std::find_if(frame.selection.packets.begin(), frame.selection.packets.end(), [&](const auto& p) { return p.id == v.selectionId; });
        if (!v.selectionId || !selectionIds.insert(v.selectionId).second || selected == frame.selection.packets.end() || selected->kind != v.kind || selected->isExtra != v.isExtra ||
            selected->owner != v.owner || selected->randomOrigin != v.randomOrigin || selected->price != v.price || selected->rechargeSeconds != v.rechargeSeconds) return false;
        if (v.kind == kFusionNoPlant)
        {
            if (!v.randomOrigin || v.isExtra || v.price || v.rechargeSeconds != 0 || frame.selection.phase == FusionBankPhase::Playing) return false;
            continue;
        }
        const auto card = std::find_if(eligible.begin(), eligible.end(), [&](const auto& c) { return c.kind == v.kind && c.isExtra == v.isExtra; });
        if (!SupportedPlant(v.kind) || card == eligible.end() || v.price != FusionBankPacketCost(card->baseCost, v.randomOrigin) || v.rechargeSeconds != card->rechargeSeconds) return false;
    }
    std::set<uint32_t> instanceTokens;
    for (const auto& record : frame.plants)
    {
        const auto& v = record.data;
        const bool validOwner = v.owner < kFusionBankPlayers || (v.owner == LawnNet::NET_SHARED_OWNER && v.packetId == 0);
        if (!v.instanceToken || !instanceTokens.insert(v.instanceToken).second ||
            !SupportedPlant(v.kind) || !validOwner || v.randomOrigin > 10 || (!v.packetId && v.randomOrigin) ||
            (v.productionTicks < 0 && !(v.kind==924 && v.productionTicks==-100)) || v.productionTicks > 1000000 || v.pendingProductionTicks < 0 || v.pendingProductionTicks > 1000000 ||
            !std::isfinite(v.idleSeconds) || v.idleSeconds < 0 || v.idleSeconds > 10000000 ||
            !std::isfinite(v.shootSeconds) || v.shootSeconds < -1 || v.shootSeconds > 10000000 ||
            !FusionNativeAnimationSpeedValid(v.kind, v.animationSpeed) || !ValidNativeAncestry(v)) return false;
        if (v.kind == 1006 ? (v.producerRewardFlags & ~kFusionSunNutRewardMask) != 0 : v.producerRewardFlags != 0) return false;
        if (!FusionDeathFlagsValid(v.kind, v.deathEventFlags)) return false;
        if (v.kind == 1012 || v.kind==1129 || v.kind==1147 || v.kind==1164 || FindFusionStrongChomperProfile(v.kind) || FusionDefenderHasDamageStages(v.kind) || IsFusionAction266Rock(v.kind) || v.kind == 29 ? v.damageStage > 2 : v.damageStage != 0) return false;
        const bool waterCapture=v.kind==1175||v.kind==15||v.kind==1049||v.kind==1156||v.kind==1050||v.kind==1051||v.kind==1056||v.kind==1066;
        if(!waterCapture&&(v.waterTargetId||v.waterTargetToken))return false;
        if(IsFusionLastPlants269Kind(v.kind))
        {if(!FusionLastPlants269StateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode,v.damageStage)||
            !FusionLastPlants269ClockValid(v.kind,v.productionTicks,v.pendingProductionTicks,v.animationSpeed,v.shootSeconds)||
            (v.kind==1056&&!FusionWater268CaptureStateValid(v.kind,v.batchMode,v.productionTicks,v.pendingProductionTicks,v.waterTargetId,v.waterTargetToken,v.animationSpeed)))return false;continue;}
        if(v.kind==909||v.kind==926||v.kind==918)
        {if(!FusionWorldRemainder269PlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode,v.damageStage)||
            !FusionWorldRemainder269ClockValid(v.kind,v.productionTicks,v.pendingProductionTicks,v.animationSpeed,v.shootSeconds))return false;continue;}
        if(v.kind==255)
        {if(v.batchMode||v.batchBagCount||v.batchShotCount||v.batchRageTicks||v.productionTicks||v.pendingProductionTicks||v.shootSeconds!=-1.f)return false;continue;}
        if(v.kind==243||v.kind==248||v.kind==256)
        {if(!FusionMorePlants269StateValid(v.kind,v.batchMode,v.batchBagCount,v.batchRageTicks,v.batchShotCount,v.productionTicks,v.pendingProductionTicks)||
            (v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed)))return false;continue;}
        if(FindFusionRemainingLeaves269Timing(v.kind)||v.kind==34||v.kind==937)
        {if(v.productionTicks||v.pendingProductionTicks||v.batchMode||v.batchBagCount||v.batchRageTicks||
            (v.shootSeconds>=0&&(v.kind==937||v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed))))return false;continue;}
        if(FusionRemaining269ControllerKind(v.kind))
        {if(!FusionRemaining269StateValid(v.kind,v.batchMode,v.batchBagCount,v.batchRageTicks,v.batchShotCount,v.productionTicks,v.pendingProductionTicks)||
            (v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed)))return false;continue;}
        if(IsFusionSummonPlants269Kind(v.kind))
        {
            if(!FusionSummonPlants269StateValid(v.kind,v.batchMode,v.batchBagCount,v.batchRageTicks,
                v.productionTicks,v.pendingProductionTicks,v.batchShotCount)||
                (v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed)))return false;
            continue;
        }
        if(FindFusionCompanions269Profile(v.kind)&&v.kind!=1169)
        {
            if(v.productionTicks||v.pendingProductionTicks||v.batchMode||v.batchBagCount||v.batchRageTicks||
                (v.kind==1178?v.batchShotCount>31:v.kind!=912&&v.batchShotCount!=0)||
                (v.kind==912&&v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed))||
                (v.kind!=912&&v.shootSeconds!=-1.f))return false;
            continue;
        }
        if(v.kind==924||v.kind==1026)
        {
            if(v.batchRageTicks||v.pendingProductionTicks||
                !FusionHypno269PlantStateValid(v.kind,v.batchMode,v.batchBagCount,v.productionTicks,
                    v.idleSeconds,v.shootSeconds,v.animationSpeed,v.batchShotCount))return false;
            continue;
        }
        if(v.kind==934)
        {
            if(v.productionTicks||v.pendingProductionTicks||
                !FusionSolar269PlantStateValid(v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode)||
                (v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed)))return false;
            continue;
        }
        if(const auto* star=FindFusionStar269Profile(v.kind))
        {
            if(v.productionTicks||v.pendingProductionTicks>(v.kind==1104?253:0)||v.batchRageTicks||v.batchBagCount||v.batchMode>(star->storageSlots?1:0)||
                (v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed)))return false;
            continue;
        }
        if(v.kind==910||v.kind==930)
        {
            if(v.productionTicks||v.pendingProductionTicks||v.batchRageTicks||v.batchBagCount||v.batchMode||
                (v.shootSeconds>=0&&v.shootSeconds>=FusionShooterMaximumDuration(v.kind,v.animationSpeed)))return false;
            continue;
        }
        if(IsFusionExtras268Kind(v.kind))
        {if(!FusionExtras268PlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode,v.damageStage,v.productionTicks,v.pendingProductionTicks,v.shootSeconds))return false;continue;}
        if(FindFusionStrongChomperProfile(v.kind))
        {if(!FusionChomper268StateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode,v.damageStage)||
            !FusionChomper268ClockStateValid(v.kind,v.productionTicks,v.pendingProductionTicks,v.animationSpeed,v.shootSeconds,v.batchMode))return false;continue;}
        if(IsFusionShulk268Kind(v.kind))
        {if(!FusionShulk268StateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode)||
            !FusionShulk268ClockStateValid(v.kind,v.productionTicks,v.pendingProductionTicks,v.animationSpeed,v.shootSeconds,v.batchMode))return false;continue;}
        if(FindFusionEquipmentProfile(v.kind)){if(!Equipment268PlantValid(v))return false;continue;}
        if(IsFusionWater268Kind(v.kind)){if(!Water268PlantValid(v))return false;continue;}
        if(FindFusionCannon268Profile(v.kind)){if(!Cannon268PlantValid(v))return false;continue;}
        if (v.kind==1078 ?
            !FusionSeaLantern267StateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode,v.productionTicks,v.pendingProductionTicks) :
            v.kind==12 ? !FusionWater267PlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode) :
            IsFusionAction267Kind(v.kind) ? !FusionAction267PlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode) :
            IsFusionUmbrellaBatchKind(v.kind) ? !FusionUmbrellaBatchPlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode) :
            IsFusionEconomyBatchKind(v.kind) ?
            !FusionEconomyPlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode) :
            IsFusionAction266Kind(v.kind) ?
            !FusionAction266PlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode) :
            IsFusionShooter267Kind(v.kind) || IsFusionShooterBatchKind(v.kind) || v.kind==1011 || v.kind==1135 ?
            !FusionShooterBatchStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode) :
            !FusionBatchPlantStateValid(v.kind,v.batchShotCount,v.batchRageTicks,v.batchBagCount,v.batchMode)) return false;
        if (const auto* producer = FindFusionProducerProfile(v.kind))
        {
            const int maximumDeadline = FusionProducerMaximumTicks(producer->kind);
            if (maximumDeadline <= 0 ||
                v.productionTicks > maximumDeadline || v.pendingProductionTicks > kFusionProducerDelayTicks) return false;
        }
        const float fireDuration=FusionFireActionMaximumDuration(v.kind,v.animationSpeed);
        if (fireDuration>0 && v.shootSeconds>=fireDuration) return false;
        const float actionDuration=FusionAction266MaximumDuration(v.kind,v.animationSpeed);
        if (actionDuration>0 && v.shootSeconds>=actionDuration) return false;
        const float action267Duration=FusionAction267MaximumDuration(v.kind,v.animationSpeed);
        const float umbrellaDuration=FusionUmbrellaBatchMaximumDuration(v.kind,v.animationSpeed);
        if((action267Duration>0&&v.shootSeconds>=action267Duration)||(umbrellaDuration>0&&v.shootSeconds>=umbrellaDuration))return false;
        if(IsFusionAction267Kind(v.kind)&&(v.productionTicks>FusionAction267MaximumProductionTicks(v.kind)||v.pendingProductionTicks>50))return false;
        if(v.kind==12&&(v.productionTicks||v.pendingProductionTicks||v.shootSeconds!=-1.f))return false;
        if(v.kind==1078&&((!v.batchMode&&v.shootSeconds!=-1.f)||v.shootSeconds>=1.f/v.animationSpeed))return false;
        if (v.kind==1188&&(v.productionTicks>kFusionGoldSunflowerCycleTicks||v.pendingProductionTicks>10||
            ((v.batchMode!=0)!=(v.pendingProductionTicks!=0)))) return false;
        const auto* chomper=FindFusionChomperProfile(v.kind);
        if (const auto* profile = chomper?nullptr:FindFusionShooterTimingProfile(v.kind))
        {
            if (v.productionTicks || v.pendingProductionTicks ||
                (v.shootSeconds != -1.0f && (v.shootSeconds < 0 || v.shootSeconds >= FusionShooterMaximumDuration(v.kind,v.animationSpeed)))) return false;
        }
        else if (chomper)
        {
            if (v.productionTicks || v.pendingProductionTicks ||
                (v.kind == 5 ? v.shootSeconds != -1.0f :
                    (v.shootSeconds != -1.0f && (v.shootSeconds < 0 || v.shootSeconds > chomper->digestTicks * .01f)))) return false;
        }
    }
    std::set<uint32_t> zombieTokens;
    for(const auto& record:frame.plants)
    {
        if(const auto* profile=FindFusionStar269Profile(record.data.kind);profile&&profile->storageSlots)
        {
            const auto state=std::find_if(frame.star269Plants.begin(),frame.star269Plants.end(),[&](const auto& p){return p.id==record.id;});
            if(state==frame.star269Plants.end()||state->data.attracting!=(record.data.batchMode==1))return false;
        }
        if((record.data.kind==910||record.data.kind==930)&&
            std::none_of(frame.doom269Plants.begin(),frame.doom269Plants.end(),[&](const auto& p){return p.id==record.id;}))return false;
    }
    for(const auto& identity:frame.zombieIdentities)
        if(!identity.data||!zombieTokens.insert(identity.data).second)return false;
    for(const auto& source:frame.sourceZombies)if(!frame.zombieIdentities.empty())
    {
        const auto found=std::find_if(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[&](const auto& z){return z.id==source.id;});
        if(found==frame.zombieIdentities.end()||found->data!=source.data.instanceToken)return false;
    }
    std::set<uint16_t> captureTargets;
    for(const auto& record:frame.plants)
    {
        const auto& v=record.data;
        if(!v.waterTargetId)continue;
        if(v.kind!=1175&&(v.batchMode==1||v.batchMode==2)&&!captureTargets.insert(v.waterTargetId).second)return false;
        const auto target=std::find_if(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[&](const auto& z){return z.id==v.waterTargetId;});
        if(target==frame.zombieIdentities.end()||target->data!=v.waterTargetToken)return false;
    }
    std::set<uint32_t> equipmentTokens;std::set<uint8_t> equipmentClaims;std::set<std::pair<uint16_t,uint32_t>> equipmentSources;
    for(const auto& record:frame.equipmentCoins)
    {
        const auto& v=record.data;
        if(!NativeEquipmentDataValid(v)||!equipmentTokens.insert(v.instanceToken).second||
            std::any_of(frame.coins.begin(),frame.coins.end(),[&](const auto& coin){return coin.id==record.id;}))return false;
        if(v.phase==NativeEquipmentPhase::Cursor&&
            (frame.selection.phase!=FusionBankPhase::Playing||!(frame.selection.activeMask&(1U<<v.claimSlot))||!equipmentClaims.insert(v.claimSlot).second))return false;
        if(v.sourcePlantId)
        {
            const auto source=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==v.sourcePlantId&&p.data.instanceToken==v.sourcePlantToken;});
            const auto* profile=source==frame.plants.end()?nullptr:FindFusionEquipmentProfile(source->data.kind);
            if(source==frame.plants.end()||
                (v.weapon?source->data.kind!=1166:!profile||profile->family!=FusionEquipmentFamily::Capture)||
                !equipmentSources.insert({v.sourcePlantId,v.sourcePlantToken}).second)return false;
        }
    }
    for(const auto& record:frame.equipmentPlants)
    {
        const auto plant=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.id;});
        if(plant==frame.plants.end()||!NativeEquipmentPlantStateValid(plant->data.kind,record.data))return false;
        const auto& v=record.data;
        if(v.targetItemId)
        {
            const auto item=std::find_if(frame.equipmentCoins.begin(),frame.equipmentCoins.end(),[&](const auto& c){return c.id==v.targetItemId&&c.data.instanceToken==v.targetItemToken;});
            if(item==frame.equipmentCoins.end()||item->data.sourcePlantId!=record.id||item->data.sourcePlantToken!=plant->data.instanceToken)return false;
        }
    }
    std::set<uint16_t> heldPlants;
    for (size_t slot = 0; slot < frame.gloveHolds.size(); ++slot)
    {
        const auto& hold = frame.gloveHolds[slot];
        if (!hold.plantId && !hold.instanceToken) continue;
        if (!hold.plantId || !hold.instanceToken || !(frame.selection.activeMask & (1U << slot)) ||
            frame.selection.phase != FusionBankPhase::Playing || !heldPlants.insert(hold.plantId).second) return false;
        const auto plant = std::find_if(frame.plants.begin(), frame.plants.end(),
            [&](const auto& record) { return record.id == hold.plantId && record.data.instanceToken == hold.instanceToken; });
        if (plant == frame.plants.end()) return false;
    }
    for (const auto& record : frame.projectiles)
    {
        const auto& v = record.data;
        const auto state=std::find_if(frame.star269Projectiles.begin(),frame.star269Projectiles.end(),[&](const auto& s){return s.id==record.id;});
        const auto* star=state==frame.star269Projectiles.end()?nullptr:&state->data;
        const auto motion=v.goldAttracted&&(v.specialType==4||v.specialType==5)?MOTION_THREEPEATER:
            star?(v.specialType==6?MOTION_HOMING:MOTION_STAR):NativeProjectileMetadataMotion(v);
        if (!NativeProjectileMetadataValid(v,NativeProjectileMetadataType(v),motion,star)) return false;
        if((v.kind==43||v.kind==44||v.kind==45||v.kind==48||v.kind==49)&&!star)return false;
    }
    for (const auto& record : frame.coldZombies)
    {
        const auto& v = record.data;
        if (!FusionColdStateValid(v.level, v.threshold, v.chilledTicks, v.frozenTicks)) return false;
    }
    for (const auto& record : frame.butterZombies)
        if (!FusionButterStateValid(record.data.ticks,record.data.visible)) return false;
    for (const auto& record : frame.zombieActions)
        if (record.data.colorMask > 127 || !std::isfinite(record.data.scale) ||
            record.data.scale <= 0 || record.data.scale > 8 ||
            !FusionPoisonStateValid({record.data.poisonLevel,record.data.poisonTicks,record.data.poisonDamageTicks})||
            record.data.waterGrapTicks<0||record.data.waterGrapCount<0||
            static_cast<int64_t>(record.data.waterGrapTicks)>static_cast<int64_t>(record.data.waterGrapCount)*200) return false;
    for (const auto& record : frame.garlicZombies)
        if (frame.selection.phase != FusionBankPhase::Playing || !NativeGarlicActionStateValid(record.data,NativeFusionRowsForBackground(frame.background))) return false;
    std::set<uint32_t> sourceZombieTokens;
    for (const auto& record : frame.sourceZombies)
    {
        if (!NativeSourceZombieDataValid(record.data,(std::numeric_limits<int>::max)()) ||
            !sourceZombieTokens.insert(record.data.instanceToken).second ||
            (record.data.targetEntity == NativeZombieTargetEntity::Zombie && record.data.targetId == record.id)) return false;
        if(record.data.sourceParentId&&std::none_of(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.data.sourceParentId&&p.data.instanceToken==record.data.sourceParentToken;}))return false;
    }
    for(const auto& record:frame.star269Plants)
    {
        const auto plant=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.id;});
        if(plant==frame.plants.end()||!NativeStar269PlantStateValid(plant->data.kind,record.data))return false;
        for(size_t slot=0;slot<record.data.slots.size();++slot)if(const auto ref=record.data.slots[slot];ref.id)
        {
            const auto child=std::find_if(frame.star269Projectiles.begin(),frame.star269Projectiles.end(),[&](const auto& q){return q.id==ref.id;});
            if(child==frame.star269Projectiles.end()||child->data.instanceToken!=ref.token||child->data.ownerId!=record.id||
                child->data.ownerToken!=plant->data.instanceToken||child->data.ownerSlot!=slot)return false;
        }
    }
    std::set<uint32_t> starTokens;
    for(const auto& record:frame.star269Projectiles)
    {
        const auto projectile=std::find_if(frame.projectiles.begin(),frame.projectiles.end(),[&](const auto& q){return q.id==record.id;});
        if(projectile==frame.projectiles.end()||!NativeStar269ProjectileStateValid(projectile->data.kind,projectile->data.specialType,record.data)||
            !starTokens.insert(record.data.instanceToken).second)return false;
        if(record.data.ownerId)
        {
            const auto parent=std::find_if(frame.star269Plants.begin(),frame.star269Plants.end(),[&](const auto& p){return p.id==record.data.ownerId;});
            if(parent==frame.star269Plants.end()||record.data.ownerSlot>=parent->data.slots.size())return false;
            const auto ref=parent->data.slots[record.data.ownerSlot];if(ref.id!=record.id||ref.token!=record.data.instanceToken)return false;
        }
        if(record.data.targetZombieId&&std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[&](const auto& z){return z.id==record.data.targetZombieId&&z.data==record.data.targetZombieToken;}))return false;
    }
    for(const auto& record:frame.doom269Plants)
    {
        const auto plant=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.id;});
        if(plant==frame.plants.end()||!NativeDoom269PlantStateValid(plant->data.kind,record.data))return false;
    }
    for(const auto& record:frame.wheatPlants269)
    {
        const auto plant=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.id;});
        NativeWheat269State state;state.isFromWheat=record.data.isFromWheat;state.wheatTime=record.data.wheatTime;
        if(plant==frame.plants.end()||!FusionEve269Candidate(plant->data.kind)||!NativeWheat269StateValid(state))return false;
    }
    std::set<uint32_t> companionTokens;
    std::set<uint32_t> cardTokens;std::set<uint8_t> cardSlots;
    std::set<uint32_t> fertilizeTokens;std::set<uint8_t> fertilizeSlots;
    for(const auto& record:frame.fertilizeCoins269)
    {
        const auto& d=record.data;
        if(!NativeFertilize269DataValid(d)||!fertilizeTokens.insert(d.instanceToken).second||
            std::any_of(frame.coins.begin(),frame.coins.end(),[&](const auto& c){return c.id==record.id;})||
            std::any_of(frame.equipmentCoins.begin(),frame.equipmentCoins.end(),[&](const auto& c){return c.id==record.id;})||
            std::any_of(frame.droppedCards269.begin(),frame.droppedCards269.end(),[&](const auto& c){return c.id==record.id;}))return false;
        if(d.heldSlot<4&&(!(frame.selection.activeMask&(1U<<d.heldSlot))||!fertilizeSlots.insert(d.heldSlot).second||
            equipmentClaims.contains(d.heldSlot)||frame.gloveHolds[d.heldSlot].plantId))return false;
        if(d.targetId&&std::none_of(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==d.targetId&&p.data.instanceToken==d.targetToken;}))return false;
    }
    for(const auto& record:frame.droppedCards269)
    {
        const auto& d=record.data;
        if(!NativeDroppedCard269DataValid(d)||!SupportedPlant(d.kind)||!cardTokens.insert(d.instanceToken).second||
            std::any_of(frame.coins.begin(),frame.coins.end(),[&](const auto& c){return c.id==record.id;})||
            std::any_of(frame.equipmentCoins.begin(),frame.equipmentCoins.end(),[&](const auto& c){return c.id==record.id;}))return false;
        if(d.heldSlot<4&&(!(frame.selection.activeMask&(1U<<d.heldSlot))||!cardSlots.insert(d.heldSlot).second||
            equipmentClaims.contains(d.heldSlot)||fertilizeSlots.contains(d.heldSlot)||frame.gloveHolds[d.heldSlot].plantId))return false;
    }
    for(const auto& record:frame.rollingPlants269)
        if(!NativeRollingActor269StateValid(record.data)||std::none_of(frame.plants.begin(),frame.plants.end(),[&](const auto& p)
            {return p.id==record.id&&p.data.kind==255&&p.data.instanceToken==record.data.instanceToken;}))return false;
    for(const auto& record:frame.plants)if(record.data.kind==255&&std::none_of(frame.rollingPlants269.begin(),frame.rollingPlants269.end(),[&](const auto& p){return p.id==record.id;}))return false;
    for(const auto& record:frame.worldPlants269)
    {
        const auto plant=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.id;});
        if(plant==frame.plants.end()||!NativeWorld269PlantStateValid(plant->data.kind,record.data))return false;
        const auto validPlant=[&](FusionWorld269Reference ref){return !ref.id||std::any_of(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==ref.id&&p.data.instanceToken==ref.token;});};
        for(auto ref:record.data.plants)if(!validPlant(ref))return false;
        if(!validPlant(record.data.receiver)||!validPlant(record.data.connectedPlant))return false;
        for(auto ref:record.data.zombies)if(ref.id&&std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[&](const auto& z){return z.id==ref.id&&z.data==ref.token;}))return false;
    }
    for(const auto& record:frame.worldBeams269)
    {
        if(!NativeWorld269BeamStateValid(record.data)||!companionTokens.insert(record.data.instanceToken).second||
            std::any_of(frame.projectiles.begin(),frame.projectiles.end(),[&](const auto& p){return p.id==record.id;})||
            std::any_of(frame.companions269.begin(),frame.companions269.end(),[&](const auto& p){return p.id==record.id;}))return false;
    }
    for(const auto& record:frame.companions269)
    {
        const auto& v=record.data;
        if(!NativeCompanion269StateValid(v)||!companionTokens.insert(v.instanceToken).second||
            std::any_of(frame.projectiles.begin(),frame.projectiles.end(),[&](const auto& p){return p.id==record.id;}))return false;
        const auto parent=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==v.parentId&&p.data.instanceToken==v.parentToken;});
        if(parent==frame.plants.end()||!FindFusionCompanions269Profile(parent->data.kind))return false;
        const auto slots=std::find_if(frame.companionPlants269.begin(),frame.companionPlants269.end(),[&](const auto& p){return p.id==v.parentId;});
        if(slots==frame.companionPlants269.end()||std::none_of(slots->data.children.begin(),slots->data.children.end(),[&](const auto& c){return c.id==record.id&&c.token==v.instanceToken;}))return false;
        if(v.targetId&&std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[&](const auto& z){return z.id==v.targetId&&z.data==v.targetToken;}))return false;
    }
    for(const auto& record:frame.companionPlants269)
    {
        const auto parent=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==record.id;});
        if(parent==frame.plants.end()||!NativeCompanions269PlantStateValid(parent->data.kind,record.data))return false;
        for(const auto ref:record.data.children)if(ref.id&&std::none_of(frame.companions269.begin(),frame.companions269.end(),[&](const auto& c)
            {return c.id==ref.id&&c.data.instanceToken==ref.token&&c.data.parentId==record.id&&c.data.parentToken==parent->data.instanceToken;}))return false;
    }
    const auto validWorldRemainderOwner=[&](NativeWorldRemainder269Reference ref)
    {
        if(!ref.id)return true;
        const auto live=std::find_if(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==ref.id;});
        // A destroyed managed owner survives in source references. If its UID
        // is live in this frame, only the original full generation may bind.
        return live==frame.plants.end()||(live->data.kind==926&&live->data.instanceToken==ref.token);
    };
    for(const auto& record:frame.worldRemainder269Aux)
    {
        const auto& d=record.data;
        if(!NativeWorldRemainder269AuxStateValid(d)||starTokens.contains(d.instanceToken)||!companionTokens.insert(d.instanceToken).second||
            !validWorldRemainderOwner(d.parent)||
            std::any_of(frame.projectiles.begin(),frame.projectiles.end(),[&](const auto& p){return p.id==record.id;})||
            std::any_of(frame.companions269.begin(),frame.companions269.end(),[&](const auto& p){return p.id==record.id;})||
            std::any_of(frame.worldBeams269.begin(),frame.worldBeams269.end(),[&](const auto& p){return p.id==record.id;}))return false;
    }
    for(const auto& record:frame.worldRemainder269Shots)
    {
        if(!NativeWorldRemainder269ShotStateValid(record.data)||!validWorldRemainderOwner(record.data.parent)||
            std::none_of(frame.projectiles.begin(),frame.projectiles.end(),[&](const auto& p){return p.id==record.id&&p.data.kind==91;}))return false;
    }
    for(const auto& record:frame.projectiles)if(record.data.kind==91&&
        std::none_of(frame.worldRemainder269Shots.begin(),frame.worldRemainder269Shots.end(),[&](const auto& p){return p.id==record.id;}))return false;
    for (const auto& record : frame.coins)
    {
        const auto& coin = record.data;
        if (coin.money ? (coin.value != 25 && coin.value != 100 && coin.value != 300 && coin.value != 1000) :
            (coin.value != 5 && coin.value != 15 && coin.value != 25 && coin.value != 50)) return false;
    }
    return true;
}

void ProjectNativeStar269LiveTargets(NativeSnapshotFrame& frame)
{
    for(auto& record:frame.star269Projectiles)
        if(record.data.targetZombieId&&std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),
            [&](const auto& target){return target.id==record.data.targetZombieId&&target.data==record.data.targetZombieToken;}))
        {record.data.targetZombieId=0;record.data.targetZombieToken=0;}
    for(auto& record:frame.companions269)
        if(record.data.targetId&&std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),
            [&](const auto& target){return target.id==record.data.targetId&&target.data==record.data.targetToken;}))
        {record.data.targetId=0;record.data.targetToken=0;}
    const auto plantAlive=[&](FusionWorld269Reference ref){return !ref.id||std::any_of(frame.plants.begin(),frame.plants.end(),[&](const auto& p){return p.id==ref.id&&p.data.instanceToken==ref.token;});};
    for(auto& record:frame.worldPlants269)
    {
        auto& state=record.data;
        for(auto& ref:state.zombies)if(ref.id&&std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[&](const auto& z){return z.id==ref.id&&z.data==ref.token;}))ref={};
        for(auto& ref:state.plants)if(!plantAlive(ref))ref={};
        if(!plantAlive(state.receiver))state.receiver={};
        if(!plantAlive(state.connectedPlant))
        {state.connectedPlant={};state.connected=state.main=false;state.zombies[4]=state.zombies[5]={};}
    }
}

bool EncodeNativeSnapshot(const NativeSnapshotFrame& frame, const std::vector<FusionBankEligibleCard>& eligible, LawnNet::NetBuffer& nativeBody)
{
    if (nativeBody.mReadPos || nativeBody.Size() > kMaximumBytes || !ValidateNativeSnapshotFrame(frame, eligible)) return false;
    auto copy = frame;
    LawnNet::NetBuffer metadata;
    Writer writer{metadata}; writer.U32(copy.epoch); writer.U8(copy.background); writer.Bool(copy.selectionCommitted);
    Fields(writer, copy.selection); Vector(writer, copy.selection.packets);
    Vector(writer, copy.packets); Vector(writer, copy.plants); Vector(writer, copy.projectiles); Vector(writer, copy.coldZombies); Vector(writer, copy.butterZombies); Vector(writer, copy.zombieActions); Vector(writer, copy.coins);
    GloveFields(writer, copy);
    writer.I32(copy.money);
    writer.U16(copy.temporaryLightLevel);
    if(!EncodeFusionAdventureSession273(copy.adventure,metadata))return false;
    Vector(writer, copy.garlicZombies);
    Vector(writer, copy.sourceZombies);
    Vector(writer, copy.equipmentCoins);Vector(writer, copy.equipmentPlants);Vector(writer,copy.zombieIdentities);
    Vector(writer,copy.star269Plants);Vector(writer,copy.star269Projectiles);Vector(writer,copy.doom269Plants);
    Vector(writer,copy.companions269);Vector(writer,copy.companionPlants269);
    Vector(writer,copy.worldPlants269);Vector(writer,copy.worldBeams269);
    Vector(writer,copy.droppedCards269);Vector(writer,copy.rollingPlants269);
    Vector(writer,copy.fertilizeCoins269);
    Fields(writer,copy.worldRemainder269Board);writer.Bool(copy.goldHypnoSummoned);
    Vector(writer,copy.worldRemainder269Aux);Vector(writer,copy.worldRemainder269Shots);
    Vector(writer,copy.wheatPlants269);
    Vector(writer,copy.dayZombies273);
    if(!writer.valid)return false;
    constexpr size_t headerBytes = 14;
    if (metadata.Size() > 65535 || headerBytes + metadata.Size() > kMaximumBytes - nativeBody.Size()) return false;
    LawnNet::NetBuffer encoded;
    encoded.WriteU32(kMagic); encoded.WriteU16(kVersion); encoded.WriteU16((uint16_t)metadata.Size());
    encoded.WriteU16((uint16_t)nativeBody.Size()); encoded.WriteU32(LawnNet::NetFnv1a32(nativeBody.Data(), nativeBody.Size()));
    encoded.WriteBytes(metadata.Data(), metadata.Size()); encoded.WriteBytes(nativeBody.Data(), nativeBody.Size());
    nativeBody = std::move(encoded);
    return true;
}

bool DecodeNativeSnapshot(LawnNet::NetBuffer& wire, const std::vector<FusionBankEligibleCard>& eligible, uint32_t expectedEpoch, NativeSnapshotFrame& frame, LawnNet::NetBuffer& nativeBody)
{
    auto candidate = wire;
    uint32_t magic = 0, hash = 0; uint16_t version = 0, metadataBytes = 0, bodyBytes = 0;
    if (candidate.Remaining() > kMaximumBytes || !candidate.ReadU32(&magic) || magic != kMagic ||
        !candidate.ReadU16(&version) || version != kVersion || !candidate.ReadU16(&metadataBytes) ||
        !candidate.ReadU16(&bodyBytes) || !candidate.ReadU32(&hash) ||
        candidate.Remaining() != (size_t)metadataBytes + bodyBytes) return false;
    LawnNet::NetBuffer metadata;
    metadata.mData.resize(metadataBytes);
    if (!candidate.ReadBytes(metadata.mData.data(), metadataBytes)) return false;
    NativeSnapshotFrame decoded;
    Reader reader{metadata}; reader.U32(decoded.epoch); reader.U8(decoded.background); reader.Bool(decoded.selectionCommitted);
    Fields(reader, decoded.selection); Vector(reader, decoded.selection.packets, kFusionBankCapacity);
    Vector(reader, decoded.packets, 32); Vector(reader, decoded.plants, 1024); Vector(reader, decoded.projectiles, 1024); Vector(reader, decoded.coldZombies, 1024); Vector(reader, decoded.butterZombies, 1024); Vector(reader, decoded.zombieActions, 1024); Vector(reader, decoded.coins, 1024);
    GloveFields(reader, decoded);
    reader.I32(decoded.money);
    reader.U16(decoded.temporaryLightLevel);
    if(reader.valid)reader.valid=DecodeFusionAdventureSession273(metadata,decoded.adventure);
    Vector(reader, decoded.garlicZombies, 1024);
    Vector(reader, decoded.sourceZombies, 1024);
    Vector(reader, decoded.equipmentCoins,1024);Vector(reader, decoded.equipmentPlants,1024);Vector(reader,decoded.zombieIdentities,1024);
    Vector(reader,decoded.star269Plants,1024);Vector(reader,decoded.star269Projectiles,1024);Vector(reader,decoded.doom269Plants,1024);
    Vector(reader,decoded.companions269,1024);Vector(reader,decoded.companionPlants269,1024);
    Vector(reader,decoded.worldPlants269,1024);Vector(reader,decoded.worldBeams269,1024);
    Vector(reader,decoded.droppedCards269,128);Vector(reader,decoded.rollingPlants269,1024);
    Vector(reader,decoded.fertilizeCoins269,1024);
    Fields(reader,decoded.worldRemainder269Board);reader.Bool(decoded.goldHypnoSummoned);
    Vector(reader,decoded.worldRemainder269Aux,1024);Vector(reader,decoded.worldRemainder269Shots,1024);
    Vector(reader,decoded.wheatPlants269,1024);
    Vector(reader,decoded.dayZombies273,1024);
    if (!reader.valid || metadata.Remaining() || decoded.epoch != expectedEpoch || !ValidateNativeSnapshotFrame(decoded, eligible)) return false;
    LawnNet::NetBuffer body;
    body.mData.resize(bodyBytes);
    if (!candidate.ReadBytes(body.mData.data(), bodyBytes) || LawnNet::NetFnv1a32(body.Data(), body.Size()) != hash) return false;
    frame = std::move(decoded); nativeBody = std::move(body); wire.mReadPos = candidate.mReadPos;
    return true;
}
}
