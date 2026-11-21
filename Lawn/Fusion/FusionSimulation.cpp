#include "FusionSimulation.h"
#include "FusionRecipe.h"
#include "../CoinMotion.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace LawnFusion
{
namespace
{
// Original controller events and prefab collider/anchor data. Positions are
// relative to Shadow after CreatePlant/CreatesZombie.SetTransform, in world
// units. Shot positions are sampled at the actual event key, not prefab idle.
struct ShotEvent { float time, x, y; uint8_t movingWay = 0, specialBulletType = 0; };
const ShotEvent kPeaShots[] = {{0.6666667f, 0.514f, 0.6003f}};
const ShotEvent kPeaSunShots[] = {{0.6666667f, 0.66212f, 0.94346f}};
const ShotEvent kDoubleShots[] = {{0.53333336f, 0.514f, 0.6003f}, {1.0666667f, 0.514f, 0.6003f}};
const ShotEvent kGatlingShots[] = {
    {0.33333334f, 0.5444760134f, 0.6710248827f}, {0.5f, 0.5444760134f, 0.6710248827f},
    {0.6666667f, 0.5444760134f, 0.6710248827f}, {0.8333333f, 0.5444760134f, 0.6710248827f}};
const ShotEvent kSplitShots[] = {
    {0.6666667f, -0.9703125f, 0.600305796f, 9, 1},
    {0.6666667f, 0.514f, 0.6003f},
    {1.0f, -0.9703125f, 0.600305796f, 9, 1}};
struct PlantPilot
{
    FusionPlantId type;
    float centerX, centerY, halfX, halfY;
    float idleSpeed, shootSpeed, shootDuration;
    const ShotEvent* shots;
    size_t shotCount;
};
const PlantPilot kPlants[] = {
    {0, 0.2166503009f, 0.53080435f, 0.2886173745f, 0.66678104f, 1.3f, 2.0f, 1.0f, kPeaShots, 1},
    {1, 0.2122068168f, 0.493447977f, 0.283125678f, 0.5968095045f, 1.0f, 0.0f, 0.0f, nullptr, 0},
    {1000, 0.176851255f, 0.573810141f, 0.2557799295f, 0.6807320695f, 1.0f, 2.0f, 1.0f, kPeaSunShots, 1},
    {1030, 0.2161925549f, 0.50882072f, 0.2890751205f, 0.64479741f, 1.3f, 2.0f, 1.3333334f, kDoubleShots, 2},
    {1032, 0.146056969f, 0.59491501f, 0.2772969125f, 0.6654681195f, 1.3f, 1.0f, 1.0f, kGatlingShots, 4},
    {1090, 0.2166503009f, 0.53080435f, 0.2886173745f, 0.66678104f, 1.3f, 2.0f, 1.3333334f, kSplitShots, 3}
};
const PlantPilot* Pilot(FusionPlantId type)
{
    for (const PlantPilot& pilot : kPlants) if (pilot.type == type) return &pilot;
    return nullptr;
}

// Complete root-X Hermite keys from the original walk/walk2 clips. The native
// port explicitly applies the delta used by Zombie.PositionUpdate. Original
// Unity automatic-root-motion interaction still needs runtime comparison.
struct RootKey { float time, value, inSlope, outSlope; };
const RootKey kWalk[] = {
    {0.0f, -0.81666666f, 0.0f, -0.7999996f},
    {0.16666667f, -0.9499999f, -1.5999992f, -1.5999992f},
    {0.33333334f, -1.39f, -2.3400002f, -2.3400002f},
    {0.5f, -1.7299999f, -1.57f, -1.5699999f},
    {0.6666667f, -1.9133333f, -0.3599997f, -0.35999972f},
    {0.8333333f, -1.9433333f, 4.0829256e-08f, INFINITY},
    {1.0f, -1.9433333f, 0.0f, 0.0f},
    {1.1666666f, -2.4266665f, -2.9500003f, -2.95f},
    {1.3333334f, -2.9266667f, -2.1599998f, -2.1599998f},
    {1.5f, -3.1466665f, -1.5299997f, -1.5299996f},
    {1.6666666f, -3.4366665f, 2.6702745e-07f, 0.0f}
};
const RootKey kWalk2[] = {
    {0.0f, 3.1064515f, 0.0f, -1.9102385f},
    {0.38333333f, 2.3741934f, -1.7032251f, -1.7032256f},
    {0.6666667f, 1.9709678f, -9.8540255e-08f, 0.0f},
    {0.81666666f, 2.0225806f, -2.9481981e-08f, 0.0f},
    {0.96666664f, 1.9612902f, -0.865276f, -0.8652759f},
    {1.1f, 1.6935483f, -2.5161293f, -2.5161285f},
    {1.3333334f, 1.0387096f, -2.1977112f, -2.1977108f},
    {1.6166667f, 0.55806446f, -1.6963955f, 0.0f}
};
constexpr float kWalkDuration = 1.9166666f;
constexpr float kBiteDuration = 1.35f;
constexpr float kZombieCenterX = (1.248332f - 1.57f) * 0.31f;
constexpr float kZombieCenterY = (7.1714263f - 4.38f) * 0.31f;
constexpr float kZombieHalfX = 2.6062398f * 0.31f * 0.5f;
constexpr float kZombieHalfY = 6.3662634f * 0.31f * 0.5f;

float SampleRoot(const RootKey* keys, size_t count, float time)
{
    if (time <= keys[0].time) return keys[0].value;
    for (size_t i = 1; i < count; ++i)
    {
        const RootKey& a = keys[i - 1];
        const RootKey& b = keys[i];
        if (time > b.time) continue;
        if (!std::isfinite(a.outSlope) || !std::isfinite(b.inSlope)) return a.value;
        const float span = b.time - a.time;
        const float t = (time - a.time) / span;
        return (2*t*t*t - 3*t*t + 1)*a.value + (t*t*t - 2*t*t + t)*span*a.outSlope +
            (-2*t*t*t + 3*t*t)*b.value + (t*t*t - t*t)*span*b.inSlope;
    }
    return keys[count - 1].value;
}

float RootTravel(bool fast, float time)
{
    const RootKey* keys = fast ? kWalk2 : kWalk;
    const size_t count = fast ? sizeof(kWalk2)/sizeof(kWalk2[0]) : sizeof(kWalk)/sizeof(kWalk[0]);
    const float cycles = std::floor(time / kWalkDuration);
    return cycles * (keys[count - 1].value - keys[0].value) +
        SampleRoot(keys, count, std::fmod(time, kWalkDuration)) - keys[0].value;
}

bool InRange(float value, float low, float high)
{
    return std::isfinite(value) && value >= low && value <= high;
}
bool ValidOwner(uint8_t owner) { return owner < 4 || owner == 0xff; }
bool CanAllocate(const FusionSimulationState& state)
{
    return state.nextId != 0 && state.nextId < std::numeric_limits<uint32_t>::max();
}
bool EventCrossed(float oldTime, float newTime, float event, float duration)
{
    return (oldTime < event && newTime >= event) ||
        (newTime >= duration && newTime - duration >= event);
}
}

float FusionColumnX(int column) { return column * 1.35f - 4.8f; }
float FusionRowY(int row, int rows) { return 2.3f - row * (rows == 6 ? 1.45f : 1.67f); }
bool FusionSupportsPlant(FusionPlantId type) { return Pilot(type) != nullptr; }
bool FusionSupportsZombie(uint16_t type) { return type == 0; }

bool FusionSimulation::Reset(int columns, int rows, int sun, uint32_t seed)
{
    if (columns < 1 || columns > 32 || rows < 1 || rows > 10 || sun < 0) return false;
    FusionSimulationState fresh;
    fresh.columns = static_cast<uint8_t>(columns);
    fresh.rows = static_cast<uint8_t>(rows);
    fresh.sun = sun;
    fresh.rngState = seed ? seed : 1;
    mState = std::move(fresh);
    return true;
}

float FusionSimulation::RandomFloat(float minimum, float maximum)
{
    uint32_t value = mState.rngState;
    value ^= value << 13; value ^= value >> 17; value ^= value << 5;
    mState.rngState = value;
    return minimum + (maximum - minimum) * ((value >> 8) / 16777216.0f);
}
int FusionSimulation::RandomInt(int minimum, int exclusiveMaximum)
{
    return minimum + static_cast<int>(RandomFloat(0.0f, 1.0f) * (exclusiveMaximum - minimum));
}

const FusionPlantState* FusionSimulation::PlantAt(int column, int row) const
{
    for (const FusionPlantState& plant : mState.plants)
        if (plant.column == column && plant.row == row) return &plant;
    return nullptr;
}

FusionPlantingResult FusionSimulation::TryPlant(const FusionPlantRequest& request)
{
    FusionPlantingResult result;
    if (request.cost < 0 || !ValidOwner(request.owner) || request.randomOrigin > 10) return result;
    if (!FusionSupportsPlant(request.type)) { result.status = FusionPlantingStatus::UnsupportedPlant; return result; }
    if (request.column >= mState.columns || request.row >= mState.rows) { result.status = FusionPlantingStatus::InvalidCell; return result; }
    const FusionPlantState* existing = PlantAt(request.column, request.row);
    const uint32_t existingId = existing ? existing->id : 0;
    if (request.expectedExistingId != existingId) { result.status = FusionPlantingStatus::StaleTarget; return result; }
    FusionPlantId type = request.type;
    FusionPlantId firstParent = kFusionNoPlant;
    if (existing)
    {
        const FusionRecipe* recipe = FindFusionRecipe(existing->type, request.type);
        if (!recipe) { result.status = FusionPlantingStatus::NoRecipe; return result; }
        type = recipe->result;
        firstParent = existing->type;
        if (!FusionSupportsPlant(type)) { result.status = FusionPlantingStatus::UnsupportedResult; return result; }
    }
    if (request.cost > mState.sun) { result.status = FusionPlantingStatus::InsufficientSun; return result; }
    if (!CanAllocate(mState) || (!existing && mState.plants.size() >= kFusionMaxPlants))
    { result.status = FusionPlantingStatus::Capacity; return result; }
    const FusionPlantDefinition* definition = FusionFindPlant(type);
    if (!definition || !definition->hasStats) { result.status = FusionPlantingStatus::UnsupportedResult; return result; }
    FusionPlantState created;
    created.id = mState.nextId;
    created.type = type;
    created.column = request.column;
    created.row = request.row;
    created.health = definition->maxHealth;
    created.owner = request.owner;
    created.packetId = request.packetId;
    created.randomOrigin = request.randomOrigin;
    created.firstParent = firstParent;
    created.secondParent = existing ? request.type : kFusionNoPlant;
    // Every failure above is checked before RNG, parent, currency or ID mutation.
    created.animationSpeed = RandomFloat(0.9f, 1.1f);
    if (definition->attackIntervalSeconds != 0.0f)
        created.attackCountdown = RandomFloat(0.2f, 0.8f) * definition->attackIntervalSeconds;
    if (definition->productionIntervalSeconds != 0.0f) created.produceCountdown = RandomFloat(4.0f, 7.0f);
    if (existing)
    {
        for (FusionPlantState& plant : mState.plants) if (plant.id == existingId) { plant = created; break; }
    }
    else mState.plants.push_back(created);
    ++mState.nextId;
    mState.sun -= request.cost;
    result.status = existingId ? FusionPlantingStatus::Fused : FusionPlantingStatus::Planted;
    result.plantId = created.id;
    result.consumedId = existingId;
    result.resultType = type;
    return result;
}

uint32_t FusionSimulation::SpawnZombie(uint16_t type, int row, float x)
{
    if (!FusionSupportsZombie(type) || row < 0 || row >= mState.rows || !InRange(x, -10.0f, FusionColumnX(mState.columns - 1) + 4.0f) ||
        mState.zombies.size() >= kFusionMaxZombies || !CanAllocate(mState)) return 0;
    FusionZombieState zombie;
    zombie.id = mState.nextId++;
    zombie.type = type;
    zombie.row = static_cast<uint8_t>(row);
    zombie.x = x; // Ground/Shadow X, matching source SetTransform.
    zombie.animationSpeed = RandomFloat(0.9f, 1.8f);
    zombie.fastWalk = zombie.animationSpeed > 1.35f;
    mState.zombies.push_back(zombie);
    return zombie.id;
}

bool FusionSimulation::AddSun(int row, float x, float y, int amount)
{
    if (mState.suns.size() >= kFusionMaxSuns || !CanAllocate(mState)) return false;
    FusionSunState sun;
    sun.id = mState.nextId++;
    sun.row = static_cast<uint8_t>(row);
    sun.x = x; sun.y = sun.startY = y;
    sun.amount = amount;
    sun.scale = amount == 5 ? 0.5f : 1.0f;
    sun.velocityX = RandomFloat(-1.5f, 1.5f);
    mState.suns.push_back(sun);
    return true;
}

bool FusionSimulation::CollectSun(uint32_t id)
{
    for (FusionSunState& sun : mState.suns) if (sun.id == id)
    {
        if (sun.collecting) return false;
        const float dx = kFusionSunCollectionTargetX - sun.x;
        const float dy = kFusionSunCollectionTargetY - sun.y;
        sun.collectionStartDistance = std::sqrt(dx*dx + dy*dy);
        sun.collecting = true;
        return true;
    }
    return false;
}

bool FusionSimulation::RemovePlant(uint32_t id)
{
    const auto plant = std::find_if(mState.plants.begin(), mState.plants.end(), [id](const FusionPlantState& p) { return p.id == id; });
    if (plant == mState.plants.end()) return false;
    mState.plants.erase(plant);
    for (FusionZombieState& zombie : mState.zombies) if (zombie.attackTarget == id)
    { zombie.attackTarget = 0; zombie.animationTime = 0.0f; }
    return true;
}

bool FusionSimulation::Step(float delta)
{
    if (!InRange(delta, 0.000001f, 0.1f) || mState.tick == std::numeric_limits<uint32_t>::max()) return false;
    // At a bounded storage/ID frontier, roll back rather than consume a shot or
    // sun that could not be represented in the authoritative snapshot.
    FusionSimulationState before = mState;
    const auto fail = [&]() { mState = std::move(before); return false; };
    for (FusionPlantState& plant : mState.plants)
    {
        const FusionPlantDefinition& definition = *FusionFindPlant(plant.type);
        const PlantPilot& pilot = *Pilot(plant.type);
        plant.animationTime = std::fmod(plant.animationTime + delta * plant.animationSpeed * pilot.idleSpeed, 2.0f);
        if (definition.attackIntervalSeconds > 0.0f)
        {
            plant.attackCountdown -= delta;
            if (plant.attackCountdown < 0.0f)
            {
                plant.attackCountdown = definition.attackIntervalSeconds * RandomFloat(0.95f, 1.05f);
                const float x = FusionColumnX(plant.column);
                const float vision = FusionColumnX(mState.columns - 1) + 1.85f;
                for (const FusionZombieState& zombie : mState.zombies)
                    if (zombie.health > 0.0f && zombie.row == plant.row &&
                        (plant.type == 1090 || zombie.x > x) && zombie.x < vision)
                    { plant.shootAnimationTime = 0.0f; break; }
            }
        }
        if (plant.shootAnimationTime >= 0.0f)
        {
            const float oldTime = plant.shootAnimationTime;
            plant.shootAnimationTime += delta * plant.animationSpeed * pilot.shootSpeed;
            plant.pendingShot = 0.0f;
            for (size_t event = 0; event < pilot.shotCount; ++event)
            {
                const ShotEvent& shot = pilot.shots[event];
                if (oldTime < shot.time && plant.shootAnimationTime >= shot.time)
                {
                    if (mState.projectiles.size() >= kFusionMaxProjectiles || !CanAllocate(mState)) return fail();
                    FusionProjectileState projectile;
                    projectile.id = mState.nextId++;
                    projectile.type = plant.type == 1000 ? 8 : 0;
                    projectile.row = plant.row;
                    projectile.x = FusionColumnX(plant.column) + shot.x;
                    projectile.y = FusionRowY(plant.row, mState.rows) + shot.y;
                    projectile.damage = definition.damage;
                    projectile.movingWay = shot.movingWay;
                    projectile.specialBulletType = shot.specialBulletType;
                    mState.projectiles.push_back(projectile);
                }
                else if (shot.time > plant.shootAnimationTime && plant.pendingShot == 0.0f)
                    plant.pendingShot = (shot.time - plant.shootAnimationTime) / (plant.animationSpeed * pilot.shootSpeed);
            }
            if (plant.shootAnimationTime >= pilot.shootDuration) plant.shootAnimationTime = -1.0f;
        }
        if (plant.pendingSun > 0.0f)
        {
            plant.pendingSun -= delta;
            if (plant.pendingSun <= 0.0f)
            {
                plant.pendingSun = 0.0f;
                if (!AddSun(plant.row, FusionColumnX(plant.column), FusionRowY(plant.row, mState.rows) + 1.0f, 25)) return fail();
            }
        }
        if (definition.productionIntervalSeconds > 0.0f)
        {
            plant.produceCountdown -= delta;
            if (plant.produceCountdown < 0.0f)
            { plant.produceCountdown = definition.productionIntervalSeconds + RandomInt(-2, 3); plant.pendingSun = 0.5f; }
        }
    }
    for (FusionZombieState& zombie : mState.zombies)
    {
        if (zombie.headless) zombie.health -= delta * 27.0f;
        if (zombie.health < 1.0f) continue;
        FusionPlantState* target = nullptr;
        if (!zombie.headless)
            for (FusionPlantState& plant : mState.plants)
            {
                if (plant.row != zombie.row || plant.health <= 0) continue;
                const PlantPilot& box = *Pilot(plant.type);
                const float px = FusionColumnX(plant.column) + box.centerX;
                if (std::fabs((zombie.x + kZombieCenterX) - px) <= box.halfX + kZombieHalfX &&
                    std::fabs(kZombieCenterY - box.centerY) <= box.halfY + kZombieHalfY)
                    if (!target || plant.column > target->column) target = &plant;
            }
        if (target)
        {
            if (zombie.attackTarget != target->id) zombie.animationTime = 0.0f;
            zombie.attackTarget = target->id;
            const float oldTime = zombie.animationTime;
            const float newTime = oldTime + delta * zombie.animationSpeed;
            if (EventCrossed(oldTime, newTime, 0.33333334f, kBiteDuration)) target->health -= 50;
            if (EventCrossed(oldTime, newTime, 1.1f, kBiteDuration) && target->health > 0) target->health -= 50;
            zombie.animationTime = std::fmod(newTime, kBiteDuration);
        }
        else
        {
            if (zombie.attackTarget) zombie.animationTime = 0.0f;
            zombie.attackTarget = 0;
            const float newTime = zombie.animationTime + delta * zombie.animationSpeed * (zombie.fastWalk ? 0.5f : 0.4f);
            zombie.x += (RootTravel(zombie.fastWalk, newTime) - RootTravel(zombie.fastWalk, zombie.animationTime)) * 0.31f;
            zombie.animationTime = std::fmod(newTime, kWalkDuration);
        }
    }
    for (FusionProjectileState& projectile : mState.projectiles)
    {
        projectile.existTime += delta;
        if (projectile.existTime > 20.0f) { projectile.damage = 0; continue; }
        const float oldX = projectile.x;
        const bool backward = projectile.movingWay == 9;
        const float nextX = oldX + (backward ? -6.0f : 6.0f) * delta;
        const float leftX = std::min(oldX, nextX), rightX = std::max(oldX, nextX);
        const float halfX = projectile.type == 8 ? 0.2f : 0.240849f * 1.7f * 0.5f;
        const float halfY = projectile.type == 8 ? 0.2f : 0.23931149f * 1.7f * 0.5f;
        FusionZombieState* target = nullptr;
        for (FusionZombieState& zombie : mState.zombies)
            if (zombie.row == projectile.row && zombie.health > 0.0f &&
                zombie.x + kZombieCenterX + kZombieHalfX >= leftX - halfX &&
                zombie.x + kZombieCenterX - kZombieHalfX <= rightX + halfX &&
                std::fabs(projectile.y - (FusionRowY(zombie.row, mState.rows) + kZombieCenterY)) <= halfY + kZombieHalfY)
                if (!target || (backward ? zombie.x > target->x : zombie.x < target->x)) target = &zombie;
        if (target)
        {
            projectile.x = backward ? std::min(oldX, target->x + kZombieCenterX + kZombieHalfX + halfX) :
                std::max(oldX, target->x + kZombieCenterX - kZombieHalfX - halfX);
            target->health -= projectile.damage;
            if (target->health < 180.0f) target->lostArm = true;
            if (target->health < 90.0f)
            {
                target->headless = true;
                if (target->attackTarget) target->animationTime = 0.0f;
                target->attackTarget = 0;
            }
            if (projectile.type == 8 && !AddSun(projectile.row, projectile.x, projectile.y, 5)) return fail();
            projectile.damage = 0;
        }
        else projectile.x = nextX;
        // Bullet.FixedUpdate bounces Split_left at the source boardMinX,
        // without clamping the overshoot. Ordinary bullets have a wider cull.
        if (projectile.specialBulletType == 1 && projectile.x < -10.0f)
        {
            projectile.movingWay = 0;
            projectile.specialBulletType = 2;
        }
        else if ((!projectile.specialBulletType && projectile.x < -12.0f) ||
            projectile.x > FusionColumnX(mState.columns - 1) + (projectile.specialBulletType ? 2.85f : 3.85f) ||
            projectile.y < FusionRowY(mState.rows - 1, mState.rows) - 0.6f || projectile.y > 6.5f)
            projectile.damage = 0;
    }
    for (FusionSunState& sun : mState.suns)
    {
        if (sun.collecting)
        {
            float nativeX = sun.x * kFusionNativePixelsPerWorldUnit;
            float nativeY = sun.y * kFusionNativePixelsPerWorldUnit;
            float ticks = delta * 100.0f;
            while (ticks > 0.0f)
            {
                const float fraction = std::min(1.0f, ticks);
                const LawnCoinMotion::CollectionStep motion = LawnCoinMotion::AdvanceCollection(nativeX, nativeY,
                    kFusionSunCollectionTargetX * kFusionNativePixelsPerWorldUnit,
                    kFusionSunCollectionTargetY * kFusionNativePixelsPerWorldUnit);
                if (fraction == 1.0f) { nativeX = motion.x; nativeY = motion.y; }
                else { nativeX += (motion.x-nativeX)*fraction; nativeY += (motion.y-nativeY)*fraction; }
                sun.scale = LawnCoinMotion::CollectionScale(motion.distance, sun.amount == 5 ? 0.5f : 1.0f);
                if (motion.distance < 8.0f && sun.amount <= std::numeric_limits<int>::max() - mState.sun)
                { mState.sun += sun.amount; sun.amount = 0; break; }
                ticks -= fraction;
            }
            sun.x = nativeX / kFusionNativePixelsPerWorldUnit;
            sun.y = nativeY / kFusionNativePixelsPerWorldUnit;
        }
        else if (!sun.landed)
        {
            sun.velocityY -= delta * 12.0f;
            sun.x += sun.velocityX * delta;
            sun.y += sun.velocityY * delta;
            if (sun.y < sun.startY - 0.5f) sun.landed = true;
        }
    }
    mState.plants.erase(std::remove_if(mState.plants.begin(), mState.plants.end(), [](const FusionPlantState& p) { return p.health <= 0; }), mState.plants.end());
    for (const FusionZombieState& zombie : mState.zombies) if (zombie.health < 1.0f) ++mState.killedZombies;
    mState.zombies.erase(std::remove_if(mState.zombies.begin(), mState.zombies.end(), [](const FusionZombieState& z) { return z.health < 1.0f || z.x < -10.0f; }), mState.zombies.end());
    mState.projectiles.erase(std::remove_if(mState.projectiles.begin(), mState.projectiles.end(), [](const FusionProjectileState& p) { return p.damage <= 0; }), mState.projectiles.end());
    mState.suns.erase(std::remove_if(mState.suns.begin(), mState.suns.end(), [](const FusionSunState& s) { return s.amount == 0; }), mState.suns.end());
    for (FusionZombieState& zombie : mState.zombies)
        if (zombie.attackTarget && !std::any_of(mState.plants.begin(), mState.plants.end(), [&](const FusionPlantState& p) { return p.id == zombie.attackTarget; }))
        { zombie.attackTarget = 0; zombie.animationTime = 0.0f; }
    ++mState.tick;
    return true;
}

bool FusionSimulation::ValidateSnapshot(const FusionSimulationState& state)
{
    if (state.columns < 1 || state.columns > 32 || state.rows < 1 || state.rows > 10 || state.sun < 0 || !state.rngState || !state.nextId ||
        state.plants.size() > kFusionMaxPlants || state.zombies.size() > kFusionMaxZombies ||
        state.projectiles.size() > kFusionMaxProjectiles || state.suns.size() > kFusionMaxSuns) return false;
    std::set<uint32_t> ids;
    std::set<unsigned> cells;
    const auto validId = [&](uint32_t id) { return id && id < state.nextId && ids.insert(id).second; };
    for (const FusionPlantState& plant : state.plants)
    {
        const FusionPlantDefinition* definition = FusionFindPlant(plant.type);
        if (!Pilot(plant.type) || !definition || !definition->hasStats || !validId(plant.id) || plant.column >= state.columns || plant.row >= state.rows ||
            !cells.insert(plant.column * 256U + plant.row).second || plant.health < 1 || plant.health > definition->maxHealth ||
            !ValidOwner(plant.owner) || plant.randomOrigin > 10 || !InRange(plant.animationSpeed, 0.9f, 1.1f) ||
            !InRange(plant.animationTime, 0.0f, 2.0f) || !InRange(plant.shootAnimationTime, -1.0f, 1.333334f) ||
            !InRange(plant.attackCountdown, 0.0f, 10.0f) || !InRange(plant.produceCountdown, 0.0f, 30.0f) ||
            !InRange(plant.pendingShot, 0.0f, 2.0f) || !InRange(plant.pendingSun, 0.0f, 0.5f)) return false;
        if (plant.firstParent == kFusionNoPlant || plant.secondParent == kFusionNoPlant)
        { if (plant.firstParent != plant.secondParent) return false; }
        else
        {
            const FusionRecipe* recipe = FindFusionRecipe(plant.firstParent, plant.secondParent);
            if (!recipe || recipe->result != plant.type) return false;
        }
    }
    const float rightEdge = FusionColumnX(state.columns - 1) + 5.0f;
    for (const FusionZombieState& zombie : state.zombies)
    {
        if (!FusionSupportsZombie(zombie.type) || !validId(zombie.id) || zombie.row >= state.rows || !InRange(zombie.x, -10.0f, rightEdge) ||
            !InRange(zombie.health, 1.0f, 270.0f) || !InRange(zombie.animationSpeed, 0.9f, 1.8f) ||
            !InRange(zombie.animationTime, 0.0f, zombie.attackTarget ? kBiteDuration : kWalkDuration) ||
            (zombie.headless && zombie.attackTarget) || (zombie.health < 90.0f && !zombie.headless) ||
            (zombie.health < 180.0f && !zombie.lostArm)) return false;
        if (zombie.attackTarget && !std::any_of(state.plants.begin(), state.plants.end(), [&](const FusionPlantState& p) { return p.id == zombie.attackTarget && p.row == zombie.row; })) return false;
    }
    for (const FusionProjectileState& projectile : state.projectiles)
        if (!validId(projectile.id) || (projectile.type != 0 && projectile.type != 8) || projectile.row >= state.rows ||
            !InRange(projectile.x, -12.0f, rightEdge) || !InRange(projectile.y, -20.0f, 10.0f) ||
            (projectile.type == 0 ? projectile.damage != 20 : projectile.damage != 100) ||
            !InRange(projectile.existTime, 0.0f, 20.0f) || projectile.specialBulletType > 2 ||
            (projectile.specialBulletType == 1 ? projectile.movingWay != 9 : projectile.movingWay != 0) ||
            (projectile.type == 8 && projectile.specialBulletType != 0)) return false;
    for (const FusionSunState& sun : state.suns)
    {
        if (!validId(sun.id) || sun.row >= state.rows || (sun.amount != 5 && sun.amount != 25) ||
            !InRange(sun.x, -100.0f, 100.0f) || !InRange(sun.y, -40.0f, 20.0f) || !InRange(sun.startY, -20.0f, 10.0f) ||
            !InRange(sun.velocityX, -1.5f, 1.5f) || !InRange(sun.velocityY, -20.0f, 4.0f) ||
            !InRange(sun.collectionStartDistance, 0.0f, 256.0f) || (!sun.collecting && sun.collectionStartDistance != 0.0f) ||
            !InRange(sun.scale, sun.amount == 5 ? 0.25f : 0.5f, sun.amount == 5 ? 0.5f : 1.0f)) return false;
        if (sun.collecting && std::hypot(sun.x - kFusionSunCollectionTargetX,
            sun.y - kFusionSunCollectionTargetY) > sun.collectionStartDistance + 0.001f) return false;
    }
    return true;
}

bool FusionSimulation::ApplySnapshot(const FusionSimulationState& state)
{
    if (!ValidateSnapshot(state)) return false;
    mState = state;
    return true;
}
}
