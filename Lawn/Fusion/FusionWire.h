#ifndef LAWN_FUSION_WIRE_H
#define LAWN_FUSION_WIRE_H

#include "FusionSimulation.h"
#include "FusionSeedBank.h"

namespace LawnNet { class NetBuffer; }

namespace LawnFusion
{
enum class FusionActivity : uint16_t { Adventure = 0, Validation = 65534 };

struct FusionStartDescriptor
{
    uint32_t epoch = 0;
    FusionActivity activity = FusionActivity::Validation;
    uint16_t level = 1;
    uint32_t seed = 1;
    uint8_t difficulty = 1;
    uint8_t sourceDifficulty = 2;
    uint8_t activeMask = 1;
    uint8_t columns = 10;
    uint8_t rows = 5;
    int initialSun = 500;
    std::vector<FusionBankEligibleCard> eligible;
};

struct FusionRoomState
{
    uint32_t epoch = 0;
    uint32_t revision = 0;
    bool paused = false;
    FusionSimulationState world;
    FusionBankState bank;
};

enum class FusionCommandKind : uint8_t
{
    Choose = 1, Unchoose, Mystery, Ready, Plant, Shovel, CollectSun,
    GlovePickup, GlovePlace, GloveCancel, ActivatePlant, MoneyPlant,
    EquipmentPickup, EquipmentUse, EquipmentCancel, CannonAim,
    DroppedCardPickup,DroppedCardPutDown,DroppedCardPlant
};

// The connection supplies the sender. Prices, ownership and RNG results are
// deliberately absent: the host derives them from its canonical seed bank.
struct FusionCommand
{
    uint32_t epoch = 0;
    uint32_t inputEpoch = 0;
    uint32_t sequence = 0;
    FusionCommandKind action = FusionCommandKind::Ready;
    FusionPlantId kind = kFusionNoPlant;
    bool isExtra = false;
    bool enabled = false;
    uint16_t packetId = 0;
    uint8_t column = 0;
    uint8_t row = 0;
    uint32_t targetId = 0;
    uint16_t sourceId = 0;
    uint32_t sourceToken = 0;
    uint32_t targetToken = 0;
    int16_t boardTargetX = 0;
    int16_t boardTargetY = 0;
};

bool ValidateFusionDescriptor(const FusionStartDescriptor& descriptor);
bool EncodeFusionStart(const FusionStartDescriptor& descriptor, LawnNet::NetBuffer& buffer);
bool DecodeFusionStart(LawnNet::NetBuffer& buffer, FusionStartDescriptor& descriptor);
bool EncodeFusionCommand(const FusionCommand& command, LawnNet::NetBuffer& buffer);
bool DecodeFusionCommand(LawnNet::NetBuffer& buffer, FusionCommand& command);
bool EncodeFusionRoom(const FusionRoomState& state, const FusionStartDescriptor& descriptor, LawnNet::NetBuffer& buffer);
bool DecodeFusionRoom(LawnNet::NetBuffer& buffer, const FusionStartDescriptor& descriptor, FusionRoomState& state);

// Field-based and versioned. Classic raw structures and byte type IDs are
// never part of this format. Both failure paths leave their outputs unchanged.
bool EncodeFusionSnapshot(const FusionSimulationState& state, LawnNet::NetBuffer& buffer);
bool DecodeFusionSnapshot(LawnNet::NetBuffer& buffer, FusionSimulationState& state);
}

#endif
