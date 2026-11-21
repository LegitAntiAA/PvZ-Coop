#ifndef LAWN_FUSION_SIMULATION_H
#define LAWN_FUSION_SIMULATION_H

#include "FusionCatalog.h"
#include <vector>

namespace LawnFusion
{
// Bounded snapshot capacities, not claims about source gameplay limits.
constexpr size_t kFusionMaxPlants = 128;
constexpr size_t kFusionMaxZombies = 128;
constexpr size_t kFusionMaxProjectiles = 256;
constexpr size_t kFusionMaxSuns = 256;

// Canonical kernel projection for native Coin collection (60 pixels/world
// unit, 100 Hz); the renderer offsets this flight to its actual seed-bank UI.
constexpr float kFusionNativePixelsPerWorldUnit = 60.0f;
constexpr float kFusionSunCollectionTargetX = -8.1333333f;
constexpr float kFusionSunCollectionTargetY = 4.5f;

// Source world coordinates. The native Board adapter owns screen projection.
float FusionColumnX(int column);
float FusionRowY(int row, int rows = 5);
bool FusionSupportsPlant(FusionPlantId type);
bool FusionSupportsZombie(uint16_t type);

struct FusionPlantState
{
    uint32_t id = 0;
    FusionPlantId type = kFusionNoPlant;
    FusionPlantId firstParent = kFusionNoPlant;
    FusionPlantId secondParent = kFusionNoPlant;
    uint8_t column = 0;
    uint8_t row = 0;
    int health = 0;
    uint8_t owner = 0xff;
    uint16_t packetId = 0;
    uint8_t randomOrigin = 0;
    float animationSpeed = 1.0f;
    float animationTime = 0.0f;
    float shootAnimationTime = -1.0f;
    float attackCountdown = 0.0f;
    float produceCountdown = 0.0f;
    float pendingShot = 0.0f;
    float pendingSun = 0.0f;
};

struct FusionZombieState
{
    uint32_t id = 0;
    uint16_t type = 0;
    uint8_t row = 0;
    float x = 0.0f;
    float health = 270.0f;
    float animationSpeed = 1.0f;
    float animationTime = 0.0f;
    uint32_t attackTarget = 0;
    bool fastWalk = false;
    bool lostArm = false;
    bool headless = false;
};

struct FusionProjectileState
{
    uint32_t id = 0;
    uint16_t type = 0;
    uint8_t row = 0;
    float x = 0.0f;
    float y = 0.0f;
    int damage = 0;
    uint8_t movingWay = 0;
    uint8_t specialBulletType = 0;
    float existTime = 0.0f;
};

struct FusionSunState
{
    uint32_t id = 0;
    uint8_t row = 0;
    float x = 0.0f;
    float y = 0.0f;
    float startY = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 4.0f;
    int amount = 25;
    bool landed = false;
    bool collecting = false;
    float collectionStartDistance = 0.0f;
    float scale = 1.0f;
};

struct FusionSimulationState
{
    uint8_t columns = 10;
    uint8_t rows = 5;
    int sun = 500;
    uint32_t tick = 0;
    uint32_t nextId = 1;
    uint32_t rngState = 1;
    uint32_t killedZombies = 0;
    std::vector<FusionPlantState> plants;
    std::vector<FusionZombieState> zombies;
    std::vector<FusionProjectileState> projectiles;
    std::vector<FusionSunState> suns;
};

// These fields are supplied by the authoritative host's validated seed bank.
// The network adapter must never accept cost/owner/provenance from a client.
struct FusionPlantRequest
{
    FusionPlantId type = kFusionNoPlant;
    uint8_t column = 0;
    uint8_t row = 0;
    uint32_t expectedExistingId = 0;
    int cost = 0;
    uint8_t owner = 0xff;
    uint16_t packetId = 0;
    uint8_t randomOrigin = 0;
};

enum class FusionPlantingStatus
{
    Planted,
    Fused,
    UnsupportedPlant,
    UnsupportedResult,
    InvalidCell,
    StaleTarget,
    NoRecipe,
    InsufficientSun,
    InvalidRequest,
    Capacity
};

struct FusionPlantingResult
{
    FusionPlantingStatus status = FusionPlantingStatus::InvalidRequest;
    uint32_t plantId = 0;
    uint32_t consumedId = 0;
    FusionPlantId resultType = kFusionNoPlant;
};

// Initial native behavior slice: Peashooter, SunFlower, PeaSunFlower,
// DoubleShooter, SplitPea, GatlingPea and basic
// unarmored zombie on a grass board. Other catalog entries are rejected.
// Only the host calls Step/TryPlant/SpawnZombie/CollectSun. Clients receive a
// complete validated state, including every simulation and animation timer.
class FusionSimulation
{
public:
    bool Reset(int columns = 10, int rows = 5, int sun = 500, uint32_t seed = 1);
    const FusionSimulationState& State() const { return mState; }
    const FusionPlantState* PlantAt(int column, int row) const;
    FusionPlantingResult TryPlant(const FusionPlantRequest& request);
    uint32_t SpawnZombie(uint16_t type, int row, float x);
    bool Step(float deltaSeconds);
    bool CollectSun(uint32_t id);
    bool RemovePlant(uint32_t id);
    static bool ValidateSnapshot(const FusionSimulationState& state);
    bool ApplySnapshot(const FusionSimulationState& state);

private:
    FusionSimulationState mState;
    float RandomFloat(float minimum, float maximum);
    int RandomInt(int minimum, int exclusiveMaximum);
    bool AddSun(int row, float x, float y, int amount);
};
}

#endif
