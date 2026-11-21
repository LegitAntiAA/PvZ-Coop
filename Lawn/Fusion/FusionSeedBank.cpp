#include "FusionSeedBank.h"
#include "../Net/NetCommon.h"
#include "../Net/NetGameplayPolicy.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
static_assert(kFusionBankPlayers == LawnNet::NET_MAX_PLAYERS, "Fusion uses the existing co-op slots");
constexpr uint8_t kPlayerMask = (1U << kFusionBankPlayers) - 1;

bool Active(const FusionBankState& state, int slot)
{
    return slot >= 0 && slot < static_cast<int>(kFusionBankPlayers) && (state.activeMask & (1U << slot)) != 0;
}

bool Identity(FusionPlantId kind, bool extra, const FusionBankEligibleCard& card)
{
    return card.kind == kind && card.isExtra == extra;
}

bool Identity(const FusionBankPacket& first, const FusionBankPacket& second)
{
    return first.kind == second.kind && first.isExtra == second.isExtra;
}

const FusionBankEligibleCard* Find(const std::vector<FusionBankEligibleCard>& eligible, FusionPlantId kind, bool extra)
{
    for (const auto& card : eligible) if (Identity(kind, extra, card)) return &card;
    return nullptr;
}

bool EligibleValid(const std::vector<FusionBankEligibleCard>& eligible)
{
    if (eligible.empty()) return false;
    for (size_t i = 0; i < eligible.size(); ++i)
    {
        const auto& card = eligible[i];
        if (card.kind == kFusionNoPlant || !std::isfinite(card.rechargeSeconds) || card.rechargeSeconds < 0.0f)
            return false;
        if(card.baseCost<0)
        {FusionBankEligibleCard source;if(!TryMakeFusionEligibleCard(card.kind,card.isExtra,source)||source.baseCost!=card.baseCost)return false;}
        for (size_t j = 0; j < i; ++j) if (Identity(card.kind, card.isExtra, eligible[j])) return false;
    }
    return true;
}

bool Quotas(size_t target, uint8_t mask, std::array<uint8_t, kFusionBankPlayers>& result)
{
    if (!mask || (mask & ~kPlayerMask) != 0 || target > kFusionBankCapacity) return false;
    bool active[LawnNet::NET_MAX_PLAYERS] = {};
    int quotas[LawnNet::NET_MAX_PLAYERS] = {};
    for (size_t i = 0; i < kFusionBankPlayers; ++i) active[i] = (mask & (1U << i)) != 0;
    if (!LawnNet::NetComputeSeedQuotas(static_cast<int>(target), 0, active, quotas)) return false;
    for (size_t i = 0; i < kFusionBankPlayers; ++i) result[i] = static_cast<uint8_t>(quotas[i]);
    return true;
}

size_t Count(const FusionBankState& state, int slot)
{
    size_t count = 0;
    for (const auto& packet : state.packets) if (packet.owner == slot) ++count;
    return count;
}

void Resolve(FusionBankPacket& packet, const FusionBankEligibleCard& card)
{
    packet.kind = card.kind;
    packet.isExtra = card.isExtra;
    packet.mysteryPending = false;
    packet.price = FusionBankPacketCost(card.baseCost, packet.randomOrigin);
    packet.rechargeSeconds = card.rechargeSeconds;
    packet.remainingRechargeSeconds = 0.0f;
}

bool Same(const FusionBankState& first, const FusionBankState& second)
{
    if (first.epoch != second.epoch || first.revision != second.revision || first.phase != second.phase ||
        first.difficulty != second.difficulty || first.activeMask != second.activeMask || first.readyMask != second.readyMask ||
        first.mysteryMask != second.mysteryMask || first.targetCount != second.targetCount || first.quotas != second.quotas ||
        first.nextPacketId != second.nextPacketId || first.packets.size() != second.packets.size()) return false;
    for (size_t i = 0; i < first.packets.size(); ++i)
    {
        const auto& a = first.packets[i]; const auto& b = second.packets[i];
        if (a.id != b.id || !Identity(a, b) || a.owner != b.owner || a.randomOrigin != b.randomOrigin ||
            a.mysteryPending != b.mysteryPending || a.price != b.price || a.rechargeSeconds != b.rechargeSeconds ||
            a.remainingRechargeSeconds != b.remainingRechargeSeconds) return false;
    }
    return true;
}
}

int FusionBankPacketCost(int baseCost,int randomOrigin)
{return baseCost<0?baseCost:LawnNet::DiscountedPacketCost(baseCost,randomOrigin,0);}

bool TryMakeFusionEligibleCard(FusionPlantId kind, bool extra, FusionBankEligibleCard& output)
{
    const auto* definition = FusionFindPlant(kind);
    if (!definition || !definition->hasStats ||
        !std::isfinite(definition->rechargeSeconds) || definition->rechargeSeconds < 0 ||
        static_cast<double>(definition->rechargeSeconds)*100.0 > (std::numeric_limits<int>::max)()) return false;
    int cost = definition->sunCost;
    if (extra)
    {
        // CardUI.Awake0x18042DD70: exact source kinds6/19, not all Puff tags.
        if (kind == 6 || kind == 19) cost = 25;
        else
        {
            if (cost > (std::numeric_limits<int>::max)()/2 || cost < (std::numeric_limits<int>::min)()/2) return false;
            cost *= 2;
        }
    }
    output = {kind, extra, cost, definition->rechargeSeconds};
    return true;
}

bool TryMakeFusionEnvironmentEligibleCard280(FusionPlantId kind, bool extra, bool night, bool shooting, FusionBankEligibleCard& output)
{
    FusionBankEligibleCard card;
    if(!TryMakeFusionEligibleCard(kind,extra,card))return false;
    if(!night&&!shooting&&((kind>=6&&kind<=11)||kind==19||kind==25))
    {
        const int surcharge=extra?150:75;
        if(card.baseCost>(std::numeric_limits<int>::max)()-surcharge)return false;
        card.baseCost+=surcharge;
    }
    output=card;return true;
}

bool FusionSeedBank::HasContext(uint32_t epoch, FusionBankPhase phase) const
{
    return epoch != 0 && epoch == mState.epoch && mState.phase == phase &&
        mState.revision < (std::numeric_limits<uint32_t>::max)();
}

bool FusionSeedBank::CanChoose(uint32_t epoch, int slot) const
{
    return HasContext(epoch, FusionBankPhase::Choosing) && Active(mState, slot);
}

void FusionSeedBank::Commit(FusionBankState& state)
{
    ++state.revision;
    std::swap(mState, state);
}

bool FusionSeedBank::Open(uint32_t epoch, int difficulty, const std::vector<FusionBankEligibleCard>& eligible, uint8_t activeMask, size_t capacity)
{
    if (!epoch || epoch <= mState.epoch || difficulty < 1 || difficulty > 10 || !EligibleValid(eligible) ||
        !capacity || capacity > kFusionBankCapacity) return false;
    FusionBankState state;
    state.epoch = epoch;
    state.revision = 1;
    state.phase = FusionBankPhase::Choosing;
    state.difficulty = static_cast<uint8_t>(difficulty);
    state.activeMask = activeMask;
    state.targetCount = static_cast<uint8_t>((std::min)(capacity, eligible.size()));
    if (!Quotas(state.targetCount, activeMask, state.quotas)) return false;
    mEligible = eligible;
    mCapacity = capacity;
    mState = state;
    return true;
}

bool FusionSeedBank::Choose(uint32_t epoch, int slot, FusionPlantId kind, bool isExtra, uint16_t* packetId)
{
    if (!CanChoose(epoch, slot) || (mState.mysteryMask & (1U << slot)) || Count(mState, slot) >= mState.quotas[slot] ||
        mState.nextPacketId > (std::numeric_limits<uint16_t>::max)()) return false;
    const auto* card = Find(mEligible, kind, isExtra);
    if (!card) return false;
    for (const auto& packet : mState.packets) if (!packet.mysteryPending && packet.kind == kind && packet.isExtra == isExtra) return false;
    FusionBankState state = mState;
    FusionBankPacket packet;
    packet.id = static_cast<uint16_t>(state.nextPacketId++);
    packet.owner = static_cast<uint8_t>(slot);
    Resolve(packet, *card);
    state.packets.push_back(packet);
    state.readyMask = 0;
    Commit(state);
    if (packetId) *packetId = packet.id;
    return true;
}

bool FusionSeedBank::Unchoose(uint32_t epoch, int slot, uint16_t packetId)
{
    if (!CanChoose(epoch, slot) || (mState.mysteryMask & (1U << slot))) return false;
    FusionBankState state = mState;
    auto packet = std::find_if(state.packets.begin(), state.packets.end(), [=](const FusionBankPacket& card) { return card.id == packetId && card.owner == slot; });
    if (packet == state.packets.end()) return false;
    state.packets.erase(packet);
    state.readyMask = 0;
    Commit(state);
    return true;
}

bool FusionSeedBank::ToggleMystery(uint32_t epoch, int slot, bool enabled)
{
    if (!CanChoose(epoch, slot) || mState.quotas[slot] == 0 || ((mState.mysteryMask & (1U << slot)) != 0) == enabled) return false;
    if (enabled && mState.nextPacketId + mState.quotas[slot] > static_cast<uint32_t>((std::numeric_limits<uint16_t>::max)()) + 1) return false;
    FusionBankState state = mState;
    state.packets.erase(std::remove_if(state.packets.begin(), state.packets.end(), [=](const FusionBankPacket& card) { return card.owner == slot; }), state.packets.end());
    if (enabled)
    {
        state.mysteryMask |= static_cast<uint8_t>(1U << slot);
        for (int i = 0; i < state.quotas[slot]; ++i)
        {
            FusionBankPacket packet;
            packet.id = static_cast<uint16_t>(state.nextPacketId++);
            packet.owner = static_cast<uint8_t>(slot);
            packet.randomOrigin = state.difficulty;
            packet.mysteryPending = true;
            state.packets.push_back(packet);
        }
    }
    else state.mysteryMask &= static_cast<uint8_t>(~(1U << slot));
    state.readyMask = 0;
    Commit(state);
    return true;
}

bool FusionSeedBank::SetReady(uint32_t epoch, int slot, bool ready)
{
    if (!CanChoose(epoch, slot) || (ready && Count(mState, slot) != mState.quotas[slot])) return false;
    if (((mState.readyMask & (1U << slot)) != 0) == ready) return true;
    FusionBankState state = mState;
    if (ready) state.readyMask |= static_cast<uint8_t>(1U << slot);
    else state.readyMask &= static_cast<uint8_t>(~(1U << slot));
    Commit(state);
    return true;
}

bool FusionSeedBank::CommitIntro(uint32_t epoch)
{
    if (!HasContext(epoch, FusionBankPhase::Choosing) || mState.readyMask != mState.activeMask || mState.packets.size() != mState.targetCount) return false;
    FusionBankState state = mState;
    state.phase = FusionBankPhase::Intro;
    Commit(state);
    return true;
}

bool FusionSeedBank::BeginPlaying(uint32_t epoch)
{
    if (!HasContext(epoch, FusionBankPhase::Intro)) return false;
    FusionBankState state = mState;
    state.phase = FusionBankPhase::Playing;
    Commit(state);
    return true;
}

bool FusionSeedBank::Reveal(uint32_t epoch, const RandomIndex& randomIndex)
{
    if (!HasContext(epoch, FusionBankPhase::Playing)) return false;
    bool pending = false;
    for (const auto& packet : mState.packets) pending |= packet.mysteryPending;
    if (!pending) return true;
    if (!randomIndex) return false;
    FusionBankState state = mState;
    try
    {
        for (size_t slot = 0; slot < kFusionBankPlayers; ++slot)
        {
            std::vector<size_t> pool(mEligible.size());
            for (size_t i = 0; i < pool.size(); ++i) pool[i] = i;
            for (auto& packet : state.packets)
            {
                if (packet.owner != slot || !packet.mysteryPending) continue;
                size_t index = 0;
                if (pool.empty() || !randomIndex(pool.size(), index) || index >= pool.size()) return false;
                Resolve(packet, mEligible[pool[index]]);
                pool.erase(pool.begin() + index);
            }
        }
    }
    catch (...) { return false; }
    Commit(state);
    return true;
}

const FusionBankPacket* FusionSeedBank::PeekReadyPacket(uint32_t epoch, int slot, uint16_t packetId) const
{
    if (!HasContext(epoch, FusionBankPhase::Playing) || !Active(mState, slot)) return nullptr;
    for (const auto& packet : mState.packets) if (packet.mysteryPending) return nullptr;
    for (const auto& packet : mState.packets)
        if (packet.id == packetId && packet.owner == slot && !packet.mysteryPending && packet.remainingRechargeSeconds == 0.0f) return &packet;
    return nullptr;
}

bool FusionSeedBank::CommitPacketUse(uint32_t epoch, int slot, uint16_t packetId)
{
    if (!PeekReadyPacket(epoch, slot, packetId)) return false;
    FusionBankState state = mState;
    for (auto& packet : state.packets) if (packet.id == packetId) packet.remainingRechargeSeconds = packet.rechargeSeconds;
    Commit(state);
    return true;
}

bool FusionSeedBank::AdvanceRecharge(uint32_t epoch, float deltaSeconds)
{
    if (!HasContext(epoch, FusionBankPhase::Playing) || !std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) return false;
    FusionBankState state = mState;
    bool changed = false;
    for (auto& packet : state.packets)
    {
        const float remaining = (std::max)(0.0f, packet.remainingRechargeSeconds - deltaSeconds);
        changed |= remaining != packet.remainingRechargeSeconds;
        packet.remainingRechargeSeconds = remaining;
    }
    if (changed) Commit(state);
    return true;
}

bool FusionSeedBank::ReassignOwners(uint32_t epoch, uint8_t activeMask)
{
    if (!HasContext(epoch, FusionBankPhase::Playing)) return false;
    FusionBankState state = mState;
    if (!Quotas(state.targetCount, activeMask, state.quotas)) return false;
    state.activeMask = activeMask;
    state.readyMask = 0;
    state.mysteryMask &= activeMask;
    std::array<uint8_t, kFusionBankPlayers> remaining = state.quotas;
    std::vector<size_t> reassign;
    for (size_t i = 0; i < state.packets.size(); ++i)
    {
        const int owner = state.packets[i].owner;
        if (Active(state, owner) && remaining[owner]) --remaining[owner];
        else reassign.push_back(i);
    }
    for (size_t index : reassign)
    {
        size_t owner = 0;
        for (size_t i = 1; i < remaining.size(); ++i) if (remaining[i] > remaining[owner]) owner = i;
        if (!remaining[owner]) return false;
        state.packets[index].owner = static_cast<uint8_t>(owner);
        --remaining[owner];
    }
    for (size_t i = 0; i < kFusionBankPlayers; ++i) if (!state.quotas[i]) state.mysteryMask &= static_cast<uint8_t>(~(1U << i));
    Commit(state);
    return true;
}

bool FusionSeedBank::ReconcilePlayers(uint32_t epoch, uint8_t activeMask)
{
    if ((!HasContext(epoch, FusionBankPhase::Choosing) && !HasContext(epoch, FusionBankPhase::Playing)) ||
        !activeMask || (activeMask & ~mState.activeMask) != 0) return false;
    if (activeMask == mState.activeMask) return true;
    if (mState.phase == FusionBankPhase::Playing)
    {
        FusionSeedBank candidate = *this;
        if (!candidate.ReassignOwners(epoch, activeMask) ||
            !ValidateSnapshot(candidate.mState, mEligible, mCapacity)) return false;
        std::swap(mState, candidate.mState);
        return true;
    }

    FusionBankState state = mState;
    if (!Quotas(state.targetCount, activeMask, state.quotas)) return false;
    state.activeMask = activeMask;
    state.readyMask = 0;
    state.mysteryMask &= activeMask;
    state.packets.erase(std::remove_if(state.packets.begin(), state.packets.end(),
        [&](const FusionBankPacket& packet) { return !Active(state, packet.owner); }), state.packets.end());

    uint32_t missing = 0;
    for (size_t slot = 0; slot < kFusionBankPlayers; ++slot)
    {
        const size_t count = Count(state, static_cast<int>(slot));
        if (count > state.quotas[slot]) return false;
        if (state.mysteryMask & (1U << slot)) missing += static_cast<uint32_t>(state.quotas[slot] - count);
    }
    if (state.nextPacketId + missing > static_cast<uint32_t>((std::numeric_limits<uint16_t>::max)()) + 1) return false;
    for (size_t slot = 0; slot < kFusionBankPlayers; ++slot)
    {
        if (!(state.mysteryMask & (1U << slot))) continue;
        for (size_t count = Count(state, static_cast<int>(slot)); count < state.quotas[slot]; ++count)
        {
            FusionBankPacket packet;
            packet.id = static_cast<uint16_t>(state.nextPacketId++);
            packet.owner = static_cast<uint8_t>(slot);
            packet.randomOrigin = state.difficulty;
            packet.mysteryPending = true;
            state.packets.push_back(packet);
        }
    }
    if (!ValidateSnapshot(state, mEligible, mCapacity)) return false;
    Commit(state);
    return true;
}

bool FusionSeedBank::ValidateSnapshot(const FusionBankState& state, const std::vector<FusionBankEligibleCard>& eligible, size_t capacity)
{
    if (!capacity || capacity > kFusionBankCapacity || !EligibleValid(eligible) || !state.epoch || !state.revision || state.difficulty < 1 || state.difficulty > 10 ||
        state.phase < FusionBankPhase::Choosing || state.phase > FusionBankPhase::Playing ||
        state.targetCount != (std::min)(capacity, eligible.size()) || state.packets.size() > state.targetCount ||
        (state.readyMask & ~state.activeMask) != 0 || (state.mysteryMask & ~state.activeMask) != 0 ||
        state.nextPacketId < 1 || state.nextPacketId > static_cast<uint32_t>((std::numeric_limits<uint16_t>::max)()) + 1) return false;
    std::array<uint8_t, kFusionBankPlayers> quotas{};
    if (!Quotas(state.targetCount, state.activeMask, quotas) || quotas != state.quotas) return false;
    if (state.phase != FusionBankPhase::Choosing && state.packets.size() != state.targetCount) return false;
    if (state.phase == FusionBankPhase::Intro && state.readyMask != state.activeMask) return false;
    for (size_t slot = 0; slot < kFusionBankPlayers; ++slot)
    {
        const size_t count = Count(state, static_cast<int>(slot));
        if (count > quotas[slot] || (state.phase != FusionBankPhase::Choosing && count != quotas[slot]) ||
            ((state.readyMask & (1U << slot)) && count != quotas[slot]) ||
            ((state.mysteryMask & (1U << slot)) && (!quotas[slot] || (state.phase != FusionBankPhase::Playing && count != quotas[slot])))) return false;
    }
    bool pendingMystery = false, revealedMystery = false;
    for (size_t i = 0; i < state.packets.size(); ++i)
    {
        const auto& packet = state.packets[i];
        if (!packet.id || packet.id >= state.nextPacketId || !Active(state, packet.owner) ||
            (packet.randomOrigin != 0 && packet.randomOrigin != state.difficulty) ||
            !std::isfinite(packet.rechargeSeconds) || !std::isfinite(packet.remainingRechargeSeconds) ||
            packet.remainingRechargeSeconds < 0.0f || packet.remainingRechargeSeconds > packet.rechargeSeconds) return false;
        for (size_t j = 0; j < i; ++j)
        {
            const auto& previous = state.packets[j];
            if (packet.id == previous.id || (!packet.randomOrigin && !previous.randomOrigin && Identity(packet, previous))) return false;
        }
        const bool mystery = (state.mysteryMask & (1U << packet.owner)) != 0;
        if (state.phase != FusionBankPhase::Playing && (packet.mysteryPending != mystery || (packet.randomOrigin != 0) != mystery || packet.remainingRechargeSeconds != 0.0f)) return false;
        if (packet.mysteryPending)
        {
            pendingMystery = true;
            if (!packet.randomOrigin || packet.kind != kFusionNoPlant || packet.isExtra || packet.price != 0 ||
                packet.rechargeSeconds != 0.0f || packet.remainingRechargeSeconds != 0.0f) return false;
        }
        else
        {
            revealedMystery |= packet.randomOrigin != 0;
            const auto* card = Find(eligible, packet.kind, packet.isExtra);
            if (!card || packet.price != FusionBankPacketCost(card->baseCost, packet.randomOrigin) || packet.rechargeSeconds != card->rechargeSeconds) return false;
        }
    }
    return !pendingMystery || !revealedMystery;
}

bool FusionSeedBank::ApplySnapshot(const FusionBankState& state)
{
    if (state.epoch != mState.epoch || state.revision < mState.revision || state.phase < mState.phase ||
        state.difficulty != mState.difficulty || state.nextPacketId < mState.nextPacketId || !ValidateSnapshot(state, mEligible, mCapacity) ||
        (state.revision == mState.revision && !Same(state, mState))) return false;
    if (mState.phase >= FusionBankPhase::Intro)
    {
        if (mState.packets.size() != state.packets.size()) return false;
        for (const auto& old : mState.packets)
        {
            const auto found = std::find_if(state.packets.begin(), state.packets.end(), [&](const FusionBankPacket& packet) { return packet.id == old.id; });
            if (found == state.packets.end() || found->randomOrigin != old.randomOrigin ||
                (!old.mysteryPending && (found->mysteryPending || !Identity(*found, old)))) return false;
        }
        if (state.phase == FusionBankPhase::Intro && state.activeMask != mState.activeMask) return false;
    }
    mState = state;
    return true;
}
}
