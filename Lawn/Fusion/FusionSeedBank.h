#ifndef LAWN_FUSION_SEED_BANK_H
#define LAWN_FUSION_SEED_BANK_H

#include "FusionCatalog.h"
#include <array>
#include <functional>
#include <vector>

namespace LawnFusion
{
constexpr size_t kFusionBankCapacity = 14;
constexpr size_t kFusionBankPlayers = 4;

enum class FusionBankPhase : uint8_t { Closed, Choosing, Intro, Playing };

// Supplied by the host's source unlock/scene/implemented-behavior policy.
// No catalog-wide default pool and no client-supplied prices are accepted.
struct FusionBankEligibleCard
{
    FusionPlantId kind = kFusionNoPlant;
    bool isExtra = false;
    int baseCost = 0;
    float rechargeSeconds = 0.0f;
    bool operator==(const FusionBankEligibleCard&) const = default;
};

// Ordinary source bank-card price/CD only; eligibility and mystery discounts
// belong to their existing callers. Failure leaves output unchanged.
bool TryMakeFusionEligibleCard(FusionPlantId kind, bool extra, FusionBankEligibleCard& output);
// Source negative costs grant sun; discounts apply only to positive purchases.
int FusionBankPacketCost(int baseCost,int randomOrigin);

struct FusionBankPacket
{
    uint16_t id = 0;
    FusionPlantId kind = kFusionNoPlant;
    bool isExtra = false;
    uint8_t owner = 0xff;
    uint8_t randomOrigin = 0;
    bool mysteryPending = false;
    int price = 0;
    float rechargeSeconds = 0.0f;
    float remainingRechargeSeconds = 0.0f;
    bool operator==(const FusionBankPacket&) const = default;
};

struct FusionBankState
{
    uint32_t epoch = 0;
    uint32_t revision = 0;
    FusionBankPhase phase = FusionBankPhase::Closed;
    uint8_t difficulty = 1; // Co-op 1..10 for mystery pricing; separate from source GameAPP.difficulty.
    uint8_t activeMask = 0;
    uint8_t readyMask = 0;
    uint8_t mysteryMask = 0;
    uint8_t targetCount = 0;
    std::array<uint8_t, kFusionBankPlayers> quotas{};
    uint32_t nextPacketId = 1; // 65536 means the u16 ID space is exhausted.
    std::vector<FusionBankPacket> packets;
    bool operator==(const FusionBankState&) const = default;
};

// Host-only mutations. The network adapter authenticates slot/epoch/phase
// before dispatch and sends State() to clients. Snapshots never contain a
// private RNG seed or a hidden mystery result.
class FusionSeedBank
{
public:
    // Fresh selection or added participants use a strictly newer epoch.
    bool Open(uint32_t epoch, int difficulty, const std::vector<FusionBankEligibleCard>& eligible, uint8_t activeMask,
        size_t capacity = kFusionBankCapacity);
    bool Choose(uint32_t epoch, int slot, FusionPlantId kind, bool isExtra, uint16_t* packetId = nullptr);
    bool Unchoose(uint32_t epoch, int slot, uint16_t packetId);
    bool ToggleMystery(uint32_t epoch, int slot, bool enabled);
    bool SetReady(uint32_t epoch, int slot, bool ready);
    bool CommitIntro(uint32_t epoch);
    bool BeginPlaying(uint32_t epoch);

    // Called only after BeginPlaying, using an isolated host RNG. The callback
    // returns an index in [0, exclusiveMaximum). Failure/exception commits none
    // of the owners' rolls. Other players' cards do not restrict any pool.
    using RandomIndex = std::function<bool(size_t exclusiveMaximum, size_t& index)>;
    bool Reveal(uint32_t epoch, const RandomIndex& randomIndex);

    const FusionBankPacket* PeekReadyPacket(uint32_t epoch, int slot, uint16_t packetId) const;
    // Called after successful world placement; commits recharge, not currency.
    bool CommitPacketUse(uint32_t epoch, int slot, uint16_t packetId);
    bool AdvanceRecharge(uint32_t epoch, float deltaSeconds);
    bool ReassignOwners(uint32_t epoch, uint8_t activeMask);
    // Disconnect-only reconciliation. Choosing retains surviving choices and
    // refills opaque mystery quotas; Playing preserves all packet identities.
    // Intro/additions reject. An unchanged valid roster is an idempotent no-op.
    bool ReconcilePlayers(uint32_t epoch, uint8_t activeMask);

    const FusionBankState& State() const { return mState; }
    static bool ValidateSnapshot(const FusionBankState& state, const std::vector<FusionBankEligibleCard>& eligible,
        size_t capacity = kFusionBankCapacity);
    // Initialize the same source eligibility via Open before applying a state.
    // Wrong epoch, old revision, phase rollback and malformed states are rejected.
    bool ApplySnapshot(const FusionBankState& state);

private:
    FusionBankState mState;
    std::vector<FusionBankEligibleCard> mEligible;
    size_t mCapacity = kFusionBankCapacity;
    bool HasContext(uint32_t epoch, FusionBankPhase phase) const;
    bool CanChoose(uint32_t epoch, int slot) const;
    void Commit(FusionBankState& state);
};
}

#endif
