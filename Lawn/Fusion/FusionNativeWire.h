#ifndef LAWN_FUSION_NATIVE_WIRE_H
#define LAWN_FUSION_NATIVE_WIRE_H

#include "FusionNative.h"
#include "FusionCannon268Rules.h"
#include "FusionNativeProjectileMetadata.h"
#include <cmath>

namespace LawnNet { class NetBuffer; }

namespace LawnFusion
{
constexpr uint16_t kNativeFusionSnapshotVersion=25;
struct NativeWheat269Snapshot {bool isFromWheat=false;float wheatTime=0;};
struct NativeRollingActor269Snapshot:NativeRollingActor269State {int health=4000;}; // Actual native HP mirror, including source negative HP.
// Pure admission of actual native raw fields; no timer or actor is duplicated.
inline bool NativeEquipmentRawClockValid(const NativeEquipmentData& d,int age,int fullLifetime,uint16_t prefixLifetime)
{
    return age>=0&&fullLifetime>=0&&fullLifetime<=age&&d.phaseStartAge>=0&&d.phaseStartAge<=age&&
        prefixLifetime==(fullLifetime>65535?65535:fullLifetime);
}
inline bool NativeProjectileRawRotationValid(const NativeProjectileData& d,ProjectileType type,ProjectileMotion motion,float rotation,float rotationSpeed,
    const NativeStar269ProjectileState* star=nullptr)
{
    if(!std::isfinite(rotation)||!std::isfinite(rotationSpeed)||std::abs(rotation)>100000.f||std::abs(rotationSpeed)>100000.f)return false;
    if(star)return NativeProjectileMetadataValid(d,type,motion,star)&&rotationSpeed>=0&&rotationSpeed<=6.283186f;
    if(IsFusionCannon268Way14(d.kind))return d.specialType==14&&type==PROJECTILE_COBBIG&&motion==MOTION_LOBBED&&
        FusionCannon268RotationValid(rotation)&&rotationSpeed==0;
    if(d.kind==41||d.kind==42)return type==PROJECTILE_STAR&&
        motion==(d.kind==42&&d.specialType==6?MOTION_HOMING:MOTION_STAR)&&
        NativeProjectileMetadataValid(d,type,motion)&&rotationSpeed>=0&&rotationSpeed<=6.283186f;
    if(d.kind==94)return type==PROJECTILE_PEA&&((d.specialType==16&&motion==MOTION_HOMING)||(d.specialType==2&&motion==MOTION_STAR))&&std::abs(rotationSpeed)<=6.283186f;
    return true;
}
// Only metadata for the same native snapshot records. Position, integer health,
// sun balance and live packet cooldowns remain in NETMSG_SNAPSHOT's native body.
template <typename T> struct NativeSnapshotRecord
{
    uint16_t id = 0;
    T data;
};

struct NativeSnapshotFrame
{
    uint32_t epoch = 0;
    uint8_t background = BACKGROUND_1_DAY;
    bool selectionCommitted = false;
    FusionBankState selection;
    std::vector<NativeSnapshotRecord<NativePacketData>> packets;
    // One-shot death flags accompany the same native plant generation.
    std::vector<NativeSnapshotRecord<NativePlantData>> plants;
    // Source phase/hot state and last Torch identity accompany the same native actor.
    std::vector<NativeSnapshotRecord<NativeProjectileData>> projectiles;
    // Subset of the native Zombie records; exact real counters, no extra clock.
    std::vector<NativeSnapshotRecord<NativeZombieColdData>> coldZombies;
    std::vector<NativeSnapshotRecord<NativeZombieButterData>> butterZombies;
    std::vector<NativeSnapshotRecord<NativeZombieActionData>> zombieActions;
    std::vector<NativeSnapshotRecord<NativeCoinData>> coins;
    std::vector<NativeSnapshotRecord<NativeEquipmentData>> equipmentCoins;
    std::vector<NativeSnapshotRecord<NativeEquipmentPlantState>> equipmentPlants;
    // Full-generation identity of each real raw Zombie, independent of source NPC type.
    std::vector<NativeSnapshotRecord<uint32_t>> zombieIdentities;
    int money = 3000;
    uint16_t temporaryLightLevel = 0;
    std::array<NativeGloveHold, kFusionBankPlayers> gloveHolds{};
    int gloveCooldownTicks = 0;
    // Source Garlic phase/captured rows; clients adopt, never tick this state.
    std::vector<NativeSnapshotRecord<GarlicActionState>> garlicZombies;
    // Source identity/float truth for existing native Zombie generations only.
    std::vector<NativeSnapshotRecord<NativeSourceZombieData>> sourceZombies;
    std::vector<NativeSnapshotRecord<NativeStar269PlantState>> star269Plants;
    std::vector<NativeSnapshotRecord<NativeStar269ProjectileState>> star269Projectiles;
    std::vector<NativeSnapshotRecord<NativeDoom269PlantState>> doom269Plants;
    std::vector<NativeSnapshotRecord<NativeCompanion269State>> companions269;
    std::vector<NativeSnapshotRecord<NativeCompanions269PlantState>> companionPlants269;
    std::vector<NativeSnapshotRecord<NativeWorld269PlantState>> worldPlants269;
    std::vector<NativeSnapshotRecord<NativeWorld269BeamState>> worldBeams269;
    std::vector<NativeSnapshotRecord<NativeDroppedCard269Data>> droppedCards269;
    std::vector<NativeSnapshotRecord<NativeRollingActor269Snapshot>> rollingPlants269;
    std::vector<NativeSnapshotRecord<NativeFertilize269Data>> fertilizeCoins269;
    NativeWorldRemainder269BoardState worldRemainder269Board;
    bool goldHypnoSummoned=false;
    std::vector<NativeSnapshotRecord<NativeWorldRemainder269AuxState>> worldRemainder269Aux;
    std::vector<NativeSnapshotRecord<NativeWorldRemainder269ShotState>> worldRemainder269Shots;
    // Source flag/time on the same real plant; host-only lastTick is never serialized.
    std::vector<NativeSnapshotRecord<NativeWheat269Snapshot>> wheatPlants269;
};

bool ValidateNativeSnapshotFrame(const NativeSnapshotFrame& frame,
    const std::vector<FusionBankEligibleCard>& eligible);
// Host capture only: a destroyed Unity-style target becomes a null reference
// on the outgoing presentation copy. Encode/decode never repair untrusted data.
void ProjectNativeStar269LiveTargets(NativeSnapshotFrame& frame);
// Encode replaces the complete native body only on success. Decode commits
// neither the input cursor nor either output on failure. The original body is
// length bounded and checksummed before its existing incremental decoder runs.
bool EncodeNativeSnapshot(const NativeSnapshotFrame& frame,
    const std::vector<FusionBankEligibleCard>& eligible, LawnNet::NetBuffer& nativeBody);
bool DecodeNativeSnapshot(LawnNet::NetBuffer& wire,
    const std::vector<FusionBankEligibleCard>& eligible, uint32_t expectedEpoch,
    NativeSnapshotFrame& frame, LawnNet::NetBuffer& nativeBody);
}

#endif
