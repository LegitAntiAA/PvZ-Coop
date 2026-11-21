#ifndef LAWN_FUSION_TUTORIAL_H
#define LAWN_FUSION_TUTORIAL_H

#include "FusionSimulation.h"
#include <array>

namespace LawnFusion
{
enum class FusionTutorialMessage : uint8_t
{
    None,
    Welcome,
    NewPlants,
    Resources,
    PlantSunflower,
    FusePeaSunflower,
    BuildGatling,
    Ready
};

struct FusionTutorialState
{
    bool active = false;
    uint8_t sourceProgress = 0;
    float timer = 0.0f;
    FusionTutorialMessage message = FusionTutorialMessage::None;
    float messageRemaining = 0.0f;
};

struct FusionMissionCard
{
    FusionPlantId type = 0;
    uint8_t column = 0;
    uint8_t row = 0;
    int cost = 0;
};

// One-call host effects, not a second persistent event queue. The room adapter
// allocates world-card IDs and snapshots cards alongside this controller.
struct FusionTutorialEvents
{
    bool messageChanged = false;
    bool grantMissionCards = false;
    std::array<FusionMissionCard, 3> missionCards{};
    bool completed = false;
    float nextWaveCountdown = 0.0f;
};

// Source TutorManager.TutorUpdate1 (0x18034A0D0). Only the host steps;
// clients apply the complete clock/progress snapshot without firing effects.
class FusionTutorial
{
public:
    void ResetAdventure1(bool enabled = true);
    const FusionTutorialState& State() const { return mState; }
    bool Step(float deltaSeconds, const FusionSimulationState& simulation, FusionTutorialEvents& events);
    static bool ValidateSnapshot(const FusionTutorialState& state);
    bool ApplySnapshot(const FusionTutorialState& state);

private:
    FusionTutorialState mState;
};

constexpr size_t kFusionMaxDroppedCards = 128;

struct FusionDroppedCardState
{
    uint32_t id = 0;
    FusionPlantId type = kFusionNoPlant;
    int cost = 0;
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 2.0f;
    float existTime = 0.0f;
    float flashTime = 0.0f;
    bool landed = false;
    bool flashing = false;
    uint8_t heldSlot = 0xff;
};

struct FusionDroppedCardsState
{
    std::vector<FusionDroppedCardState> cards;
};

// Source DroppedCard movingWay0, for implemented non-puff plants. The room
// allocator supplies world-card IDs in its separate namespace; this class
// never allocates or copies Simulation IDs. Spawn velocities are host RNG
// draws from source ranges [-2,2] and [2,3]. Slots come from authentication.
class FusionDroppedCards
{
public:
    void Reset();
    const FusionDroppedCardsState& State() const { return mState; }
    bool Spawn(uint32_t roomCardId, FusionPlantId type, int cost, float x, float y, float velocityX, float velocityY);
    bool PickUp(uint32_t id, uint8_t authenticatedSlot);
    bool PutDown(uint32_t id, uint8_t authenticatedSlot);
    void ReleaseSlot(uint8_t authenticatedSlot);
    FusionPlantingResult TryPlant(uint32_t id, uint8_t authenticatedSlot, int column, int row,
        uint32_t expectedExistingId, FusionSimulation& simulation);
    bool Step(float deltaSeconds, bool isTutorial, int columns = 10, int rows = 5);
    static bool ValidateSnapshot(const FusionDroppedCardsState& state);
    bool ApplySnapshot(const FusionDroppedCardsState& state);

private:
    FusionDroppedCardsState mState;
};
}

#endif
