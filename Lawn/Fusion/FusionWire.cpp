#include "FusionWire.h"
#include "../Net/NetCommon.h"
#include <cmath>
#include <set>

namespace LawnFusion
{
namespace
{
constexpr uint32_t kMagic = 0x31535546; // FUS1, little endian
constexpr uint16_t kSchema = 2; // Native sun collection replaces the Unity-speed field.

struct Reader
{
    LawnNet::NetBuffer& buffer;
    bool valid = true;
    void U8(uint8_t& value) { if (valid) valid = buffer.ReadU8(&value); }
    void U16(uint16_t& value) { if (valid) valid = buffer.ReadU16(&value); }
    void U32(uint32_t& value) { if (valid) valid = buffer.ReadU32(&value); }
    void I16(int16_t& value) { if (valid) valid = buffer.ReadI16(&value); }
    void I32(int& value) { if (valid) valid = buffer.ReadI32(&value); }
    void F32(float& value) { if (valid) valid = buffer.ReadF32(&value); }
    void Bool(bool& value)
    {
        uint8_t wire = 0;
        U8(wire);
        if (wire > 1) valid = false;
        if (valid) value = wire != 0;
    }
};

struct Writer
{
    LawnNet::NetBuffer& buffer;
    void U8(uint8_t& value) { buffer.WriteU8(value); }
    void U16(uint16_t& value) { buffer.WriteU16(value); }
    void U32(uint32_t& value) { buffer.WriteU32(value); }
    void I16(int16_t& value) { buffer.WriteI16(value); }
    void I32(int& value) { buffer.WriteI32(value); }
    void F32(float& value) { buffer.WriteF32(value); }
    void Bool(bool& value) { buffer.WriteU8(value ? 1 : 0); }
};

template <typename IO> void Fields(IO& io, FusionSimulationState& state)
{
    io.U8(state.columns); io.U8(state.rows); io.I32(state.sun);
    io.U32(state.tick); io.U32(state.nextId); io.U32(state.rngState); io.U32(state.killedZombies);
}

template <typename IO> void Fields(IO& io, FusionPlantState& plant)
{
    io.U32(plant.id); io.U16(plant.type); io.U16(plant.firstParent); io.U16(plant.secondParent);
    io.U8(plant.column); io.U8(plant.row); io.I32(plant.health);
    io.U8(plant.owner); io.U16(plant.packetId); io.U8(plant.randomOrigin);
    io.F32(plant.animationSpeed); io.F32(plant.animationTime); io.F32(plant.shootAnimationTime);
    io.F32(plant.attackCountdown); io.F32(plant.produceCountdown);
    io.F32(plant.pendingShot); io.F32(plant.pendingSun);
}

template <typename IO> void Fields(IO& io, FusionZombieState& zombie)
{
    io.U32(zombie.id); io.U16(zombie.type); io.U8(zombie.row);
    io.F32(zombie.x); io.F32(zombie.health); io.F32(zombie.animationSpeed); io.F32(zombie.animationTime);
    io.U32(zombie.attackTarget); io.Bool(zombie.fastWalk); io.Bool(zombie.lostArm); io.Bool(zombie.headless);
}

template <typename IO> void Fields(IO& io, FusionProjectileState& projectile)
{
    io.U32(projectile.id); io.U16(projectile.type); io.U8(projectile.row);
    io.F32(projectile.x); io.F32(projectile.y); io.I32(projectile.damage);
    io.U8(projectile.movingWay); io.U8(projectile.specialBulletType); io.F32(projectile.existTime);
}

template <typename IO> void Fields(IO& io, FusionSunState& sun)
{
    io.U32(sun.id); io.U8(sun.row); io.F32(sun.x); io.F32(sun.y); io.F32(sun.startY);
    io.F32(sun.velocityX); io.F32(sun.velocityY); io.I32(sun.amount);
    io.Bool(sun.landed); io.Bool(sun.collecting); io.F32(sun.collectionStartDistance); io.F32(sun.scale);
}

template <typename IO> void Fields(IO& io, FusionBankEligibleCard& card)
{
    io.U16(card.kind); io.Bool(card.isExtra); io.I32(card.baseCost); io.F32(card.rechargeSeconds);
}

template <typename IO> void Fields(IO& io, FusionBankPacket& packet)
{
    io.U16(packet.id); io.U16(packet.kind); io.Bool(packet.isExtra); io.U8(packet.owner);
    io.U8(packet.randomOrigin); io.Bool(packet.mysteryPending); io.I32(packet.price);
    io.F32(packet.rechargeSeconds); io.F32(packet.remainingRechargeSeconds);
}

template <typename IO> void Fields(IO& io, FusionBankState& bank)
{
    uint8_t phase = (uint8_t)bank.phase;
    io.U32(bank.epoch); io.U32(bank.revision); io.U8(phase); bank.phase = (FusionBankPhase)phase;
    io.U8(bank.difficulty); io.U8(bank.activeMask); io.U8(bank.readyMask); io.U8(bank.mysteryMask);
    io.U8(bank.targetCount);
    for (auto& quota : bank.quotas) io.U8(quota);
    io.U32(bank.nextPacketId);
}

template <typename IO> void Fields(IO& io, FusionStartDescriptor& start)
{
    uint16_t activity = (uint16_t)start.activity;
    io.U32(start.epoch); io.U16(activity); start.activity = (FusionActivity)activity;
    io.U16(start.level); io.U32(start.seed); io.U8(start.difficulty); io.U8(start.sourceDifficulty);
    io.U8(start.activeMask); io.U8(start.columns); io.U8(start.rows); io.I32(start.initialSun);
}

bool AppendBounded(LawnNet::NetBuffer& destination, const LawnNet::NetBuffer& source)
{
    const size_t limit = LawnNet::NET_MAX_FRAME - 8;
    if (destination.Size() > limit || source.Size() > limit - destination.Size()) return false;
    destination.WriteBytes(source.Data(), source.Size());
    return true;
}

bool Header(LawnNet::NetBuffer& buffer, uint32_t expected)
{
    uint32_t magic = 0;
    uint16_t schema = 0;
    return buffer.Remaining() <= LawnNet::NET_MAX_FRAME - 8 && buffer.ReadU32(&magic) && magic == expected &&
        buffer.ReadU16(&schema) && schema == kSchema;
}

template <typename T> void WriteVector(Writer& writer, std::vector<T>& values)
{
    uint16_t count = (uint16_t)values.size();
    writer.U16(count);
    for (auto& value : values) Fields(writer, value);
}

template <typename T> void ReadVector(Reader& reader, std::vector<T>& values, size_t limit)
{
    uint16_t count = 0;
    reader.U16(count);
    if (!reader.valid || count > limit) { reader.valid = false; return; }
    values.resize(count);
    for (auto& value : values) Fields(reader, value);
}
}

bool EncodeFusionSnapshot(const FusionSimulationState& state, LawnNet::NetBuffer& buffer)
{
    if (!FusionSimulation::ValidateSnapshot(state)) return false;
    FusionSimulationState copy = state;
    LawnNet::NetBuffer candidate;
    candidate.WriteU32(kMagic);
    candidate.WriteU16(kSchema);
    Writer writer{candidate};
    Fields(writer, copy);
    WriteVector(writer, copy.plants); WriteVector(writer, copy.zombies);
    WriteVector(writer, copy.projectiles); WriteVector(writer, copy.suns);
    const size_t limit = LawnNet::NET_MAX_FRAME - 8;
    if (buffer.Size() > limit || candidate.Size() > limit - buffer.Size()) return false;
    buffer.WriteBytes(candidate.Data(), candidate.Size());
    return true;
}

bool DecodeFusionSnapshot(LawnNet::NetBuffer& buffer, FusionSimulationState& state)
{
    if (buffer.Remaining() > LawnNet::NET_MAX_FRAME - 8) return false;
    LawnNet::NetBuffer candidate = buffer;
    uint32_t magic = 0;
    uint16_t schema = 0;
    if (!candidate.ReadU32(&magic) || magic != kMagic || !candidate.ReadU16(&schema) || schema != kSchema) return false;
    FusionSimulationState decoded;
    Reader reader{candidate};
    Fields(reader, decoded);
    ReadVector(reader, decoded.plants, kFusionMaxPlants);
    ReadVector(reader, decoded.zombies, kFusionMaxZombies);
    ReadVector(reader, decoded.projectiles, kFusionMaxProjectiles);
    ReadVector(reader, decoded.suns, kFusionMaxSuns);
    if (!reader.valid || candidate.Remaining() || !FusionSimulation::ValidateSnapshot(decoded)) return false;
    state = std::move(decoded);
    buffer.mReadPos = candidate.mReadPos;
    return true;
}

bool ValidateFusionDescriptor(const FusionStartDescriptor& start)
{
    if (!start.epoch || !start.seed || start.level < 1 || start.level > 45 ||
        (start.activity != FusionActivity::Adventure && start.activity != FusionActivity::Validation) ||
        (start.activity == FusionActivity::Validation && start.level != 1) ||
        start.difficulty < 1 || start.difficulty > 10 || start.sourceDifficulty > 4 ||
        !start.activeMask || (start.activeMask & 0xf0) || !(start.activeMask & 1) ||
        start.columns < 1 || start.columns > 32 || start.rows < 1 || start.rows > 10 || start.initialSun < 0 ||
        start.eligible.empty() || start.eligible.size() > 1024) return false;
    std::set<std::pair<FusionPlantId, bool>> identities;
    for (const auto& card : start.eligible)
    {
        FusionBankEligibleCard expected;
        if (!TryMakeFusionEligibleCard(card.kind, card.isExtra, expected) ||
            !identities.emplace(card.kind, card.isExtra).second) return false;
        if (card.baseCost != expected.baseCost || !std::isfinite(card.rechargeSeconds) ||
            std::fabs(card.rechargeSeconds - expected.rechargeSeconds) > 0.0001f) return false;
    }
    return true;
}

bool EncodeFusionStart(const FusionStartDescriptor& descriptor, LawnNet::NetBuffer& buffer)
{
    if (!ValidateFusionDescriptor(descriptor)) return false;
    auto copy = descriptor;
    LawnNet::NetBuffer candidate;
    candidate.WriteU32(0x31545346); candidate.WriteU16(kSchema); // FST1
    Writer writer{candidate};
    Fields(writer, copy); WriteVector(writer, copy.eligible);
    return AppendBounded(buffer, candidate);
}

bool DecodeFusionStart(LawnNet::NetBuffer& buffer, FusionStartDescriptor& descriptor)
{
    auto candidate = buffer;
    if (!Header(candidate, 0x31545346)) return false;
    FusionStartDescriptor decoded;
    Reader reader{candidate};
    Fields(reader, decoded); ReadVector(reader, decoded.eligible, 1024);
    if (!reader.valid || candidate.Remaining() || !ValidateFusionDescriptor(decoded)) return false;
    descriptor = std::move(decoded); buffer.mReadPos = candidate.mReadPos;
    return true;
}

namespace
{
bool ValidCommand(const FusionCommand& command)
{
    if (!command.epoch || !command.inputEpoch || !command.sequence) return false;
    if(command.action!=FusionCommandKind::CannonAim&&command.action!=FusionCommandKind::BossHammer366&&(command.boardTargetX||command.boardTargetY))return false;
    const bool newTool=command.action==FusionCommandKind::EquipmentPickup||command.action==FusionCommandKind::EquipmentUse||
        command.action==FusionCommandKind::EquipmentCancel||command.action==FusionCommandKind::CannonAim||
        command.action==FusionCommandKind::AdventureRewardCollect||command.action==FusionCommandKind::BossHammer366||command.action==FusionCommandKind::BossTrophy366;
    if(newTool&&(command.kind!=kFusionNoPlant||command.isExtra||command.enabled||command.packetId||
        command.targetId||command.targetToken||(command.action!=FusionCommandKind::EquipmentUse&&(command.column||command.row))))return false;
    switch (command.action)
    {
    case FusionCommandKind::DroppedCardPickup:case FusionCommandKind::DroppedCardPutDown:case FusionCommandKind::DroppedCardPlant:
        return FusionFindPlant(command.kind)&&command.sourceId&&command.sourceToken&&!command.packetId&&!command.isExtra&&!command.enabled&&
            command.column<32&&command.row<10&&command.targetId<=65535&&((command.targetId!=0)==(command.targetToken!=0))&&
            (command.action==FusionCommandKind::DroppedCardPlant||(!command.column&&!command.row&&!command.targetId));
    case FusionCommandKind::Choose:
        return FusionFindPlant(command.kind) && FusionFindPlant(command.kind)->hasStats;
    case FusionCommandKind::Unchoose: return command.packetId != 0;
    case FusionCommandKind::Mystery: case FusionCommandKind::Ready: return true;
    case FusionCommandKind::Plant:
        // The reference simulation retains fullU32 IDs without native tokens.
        // Native host admission separately requires the exact nonzero tuple.
        return command.packetId && command.column < 32 && command.row < 10 &&
            (!command.targetToken || command.targetId);
    case FusionCommandKind::Shovel: case FusionCommandKind::CollectSun: return command.targetId != 0;
    case FusionCommandKind::GlovePickup: case FusionCommandKind::ActivatePlant: case FusionCommandKind::MoneyPlant:
    case FusionCommandKind::AdventureRewardCollect:
        return command.sourceId && command.sourceToken;
    case FusionCommandKind::GlovePlace:
        return command.sourceId && command.sourceToken && command.column < 32 && command.row < 10 &&
            command.targetId <= 65535 && ((command.targetId != 0) == (command.targetToken != 0));
    case FusionCommandKind::GloveCancel: return (command.sourceId != 0) == (command.sourceToken != 0);
    case FusionCommandKind::EquipmentPickup: return command.sourceId&&command.sourceToken;
    case FusionCommandKind::EquipmentUse:
        return command.sourceId&&command.sourceToken&&command.column<32&&command.row<10;
    case FusionCommandKind::EquipmentCancel: return (command.sourceId!=0)==(command.sourceToken!=0);
    case FusionCommandKind::BossTrophy366:return command.sourceId&&command.sourceToken;
    case FusionCommandKind::BossHammer366:return !command.sourceId&&!command.sourceToken&&command.boardTargetX>=0&&command.boardTargetX<=4095&&command.boardTargetY>=0&&command.boardTargetY<=4095;
    case FusionCommandKind::CannonAim:
        return command.sourceId&&command.sourceToken&&command.boardTargetX>=0&&command.boardTargetX<=4095&&
            command.boardTargetY>=0&&command.boardTargetY<=4095;
    default: return false;
    }
}

template <typename IO> void CommandFields(IO& io, FusionCommand& command)
{
    uint8_t action = (uint8_t)command.action;
    io.U32(command.epoch); io.U32(command.inputEpoch); io.U32(command.sequence);
    io.U8(action); command.action = (FusionCommandKind)action;
    switch (command.action)
    {
    case FusionCommandKind::DroppedCardPickup:case FusionCommandKind::DroppedCardPutDown:case FusionCommandKind::DroppedCardPlant:
        io.U16(command.kind);io.U16(command.sourceId);io.U32(command.sourceToken);io.U8(command.column);io.U8(command.row);
        io.U32(command.targetId);io.U32(command.targetToken);break;
    case FusionCommandKind::Choose: io.U16(command.kind); io.Bool(command.isExtra); break;
    case FusionCommandKind::Unchoose: io.U16(command.packetId); break;
    case FusionCommandKind::Mystery: case FusionCommandKind::Ready: io.Bool(command.enabled); break;
    case FusionCommandKind::Plant:
        io.U16(command.packetId); io.U8(command.column); io.U8(command.row);
        io.U32(command.targetId); io.U32(command.targetToken); break;
    case FusionCommandKind::Shovel: case FusionCommandKind::CollectSun: io.U32(command.targetId); break;
    case FusionCommandKind::GlovePickup: case FusionCommandKind::GloveCancel: case FusionCommandKind::ActivatePlant: case FusionCommandKind::MoneyPlant:
    case FusionCommandKind::EquipmentPickup: case FusionCommandKind::EquipmentCancel: case FusionCommandKind::AdventureRewardCollect:
        io.U16(command.sourceId); io.U32(command.sourceToken); break;
    case FusionCommandKind::EquipmentUse:
        io.U16(command.sourceId); io.U32(command.sourceToken); io.U8(command.column); io.U8(command.row); break;
    case FusionCommandKind::BossTrophy366:io.U16(command.sourceId);io.U32(command.sourceToken);break;
    case FusionCommandKind::BossHammer366:io.I16(command.boardTargetX);io.I16(command.boardTargetY);break;
    case FusionCommandKind::CannonAim:
        io.U16(command.sourceId); io.U32(command.sourceToken); io.I16(command.boardTargetX); io.I16(command.boardTargetY); break;
    case FusionCommandKind::GlovePlace:
        io.U16(command.sourceId); io.U32(command.sourceToken); io.U8(command.column); io.U8(command.row);
        io.U32(command.targetId); io.U32(command.targetToken); break;
    default: break;
    }
}

bool ValidRoom(const FusionRoomState& state, const FusionStartDescriptor& descriptor)
{
    return ValidateFusionDescriptor(descriptor) && state.epoch == descriptor.epoch && state.revision &&
        state.bank.epoch == descriptor.epoch && state.bank.difficulty == descriptor.difficulty &&
        !(state.bank.activeMask & ~descriptor.activeMask) && (state.bank.activeMask & 1) &&
        state.world.columns == descriptor.columns && state.world.rows == descriptor.rows &&
        FusionSeedBank::ValidateSnapshot(state.bank, descriptor.eligible) && FusionSimulation::ValidateSnapshot(state.world);
}
}

bool EncodeFusionCommand(const FusionCommand& command, LawnNet::NetBuffer& buffer)
{
    if (!ValidCommand(command)) return false;
    auto copy = command;
    LawnNet::NetBuffer candidate;
    candidate.WriteU32(0x314d4346); candidate.WriteU16(kSchema); // FCM1
    Writer writer{candidate}; CommandFields(writer, copy);
    return AppendBounded(buffer, candidate);
}

bool DecodeFusionCommand(LawnNet::NetBuffer& buffer, FusionCommand& command)
{
    auto candidate = buffer;
    if (!Header(candidate, 0x314d4346)) return false;
    FusionCommand decoded;
    Reader reader{candidate}; CommandFields(reader, decoded);
    if (!reader.valid || candidate.Remaining() || !ValidCommand(decoded)) return false;
    command = decoded; buffer.mReadPos = candidate.mReadPos;
    return true;
}

bool EncodeFusionRoom(const FusionRoomState& state, const FusionStartDescriptor& descriptor, LawnNet::NetBuffer& buffer)
{
    if (!ValidRoom(state, descriptor)) return false;
    LawnNet::NetBuffer world;
    if (!EncodeFusionSnapshot(state.world, world)) return false;
    auto copy = state;
    LawnNet::NetBuffer candidate;
    candidate.WriteU32(0x314d5246); candidate.WriteU16(kSchema); // FRM1
    Writer writer{candidate};
    writer.U32(copy.epoch); writer.U32(copy.revision); writer.Bool(copy.paused);
    Fields(writer, copy.bank); WriteVector(writer, copy.bank.packets);
    candidate.WriteU16((uint16_t)world.Size()); candidate.WriteBytes(world.Data(), world.Size());
    return AppendBounded(buffer, candidate);
}

bool DecodeFusionRoom(LawnNet::NetBuffer& buffer, const FusionStartDescriptor& descriptor, FusionRoomState& state)
{
    auto candidate = buffer;
    if (!Header(candidate, 0x314d5246)) return false;
    FusionRoomState decoded;
    Reader reader{candidate};
    reader.U32(decoded.epoch); reader.U32(decoded.revision); reader.Bool(decoded.paused);
    Fields(reader, decoded.bank); ReadVector(reader, decoded.bank.packets, kFusionBankCapacity);
    uint16_t worldBytes = 0; reader.U16(worldBytes);
    if (!reader.valid || candidate.Remaining() != worldBytes) return false;
    LawnNet::NetBuffer world;
    world.mData.resize(worldBytes);
    if (!candidate.ReadBytes(world.mData.data(), worldBytes) || !DecodeFusionSnapshot(world, decoded.world) ||
        !ValidRoom(decoded, descriptor)) return false;
    state = std::move(decoded); buffer.mReadPos = candidate.mReadPos;
    return true;
}
}
