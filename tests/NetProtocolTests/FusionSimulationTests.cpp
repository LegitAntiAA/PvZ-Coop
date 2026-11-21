#include "FusionSimulationTests.h"
#include "../../Lawn/Fusion/FusionSimulation.h"
#include "../../Lawn/Fusion/FusionRecipe.h"
#include "../../Lawn/CoinMotion.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
int checks = 0;
int failures = 0;
void Check(bool success, const char* message)
{
    ++checks;
    if (!success) { ++failures; std::printf("FAIL FusionSimulation %s\n", message); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
using namespace LawnFusion;
FusionPlantRequest Request(FusionPlantId type, int column, int row, uint32_t existing = 0, int cost = 0)
{
    FusionPlantRequest request;
    request.type = type;
    request.column = static_cast<uint8_t>(column);
    request.row = static_cast<uint8_t>(row);
    request.expectedExistingId = existing;
    request.cost = cost;
    return request;
}
bool Advance(FusionSimulation& sim, int ticks, float step = 0.01f)
{
    for (int i = 0; i < ticks; ++i) if (!sim.Step(step)) return false;
    return true;
}
void ReadyShot(FusionSimulation& sim)
{
    FusionSimulationState state = sim.State();
    state.plants[0].animationSpeed = 1.0f;
    state.plants[0].attackCountdown = 0.005f;
    for (FusionZombieState& zombie : state.zombies)
    { zombie.animationSpeed = 1.0f; zombie.fastWalk = false; }
    Check(sim.ApplySnapshot(state), "prepare deterministic source event phase");
}
}

FusionSimulationTestResult RunFusionSimulationTests()
{
    checks = failures = 0;
    using namespace LawnFusion;
    const LawnCoinMotion::CollectionStep nativeRight = LawnCoinMotion::AdvanceCollection(78.0f, 84.0f, 15.0f, 0.0f);
    Check(nativeRight.x == 75.0f && nativeRight.y == 80.0f && nativeRight.distance == 105.0f,
        "native collection known63-84-105 triangle moves exactly3-4 pixels and reports pre-move distance");
    const LawnCoinMotion::CollectionStep nativeLeft = LawnCoinMotion::AdvanceCollection(-48.0f, -84.0f, 15.0f, 0.0f);
    Check(nativeLeft.x == -45.0f && nativeLeft.y == -80.0f && nativeLeft.distance == 105.0f,
        "native collection known vector preserves negative coordinate branches");
    const LawnCoinMotion::CollectionStep nativeMixed = LawnCoinMotion::AdvanceCollection(78.0f, -84.0f, 15.0f, 0.0f);
    const LawnCoinMotion::CollectionStep nativeAxis = LawnCoinMotion::AdvanceCollection(36.0f, 0.0f, 15.0f, 0.0f);
    Check(nativeMixed.x == 75.0f && nativeMixed.y == -80.0f && nativeAxis.x == 35.0f && nativeAxis.y == 0.0f,
        "native collection handles mixed quadrants and aligned axes without drift");
    Check(LawnCoinMotion::CollectionScale(0.0f, 1.0f) == 0.5f &&
        LawnCoinMotion::CollectionScale(15.0f, 1.0f) == 0.75f &&
        LawnCoinMotion::CollectionScale(100.0f, 1.0f) == 1.0f &&
        LawnCoinMotion::CollectionScale(15.0f, 0.5f) == 0.375f &&
        LawnCoinMotion::CollectionScale(100.0f, 2.0f) == 2.0f,
        "native distance envelope preserves normal small and large sun base scales");
    Check(GetFusionRecipeCount() == 410, "source normal recipe table has410 entries");
    const FusionRecipe* recipe = FindFusionRecipe(0, 1);
    Check(recipe && recipe->result == 1000, "native Peashooter plusSunFlower gives1000");
    Check(FindFusionRecipe(6, 0) && FindFusionRecipe(6, 0)->result == 1018 && !FindFusionRecipe(0, 6),
        "puff direction is preserved rather than invented reverse recipe");
    Check(FindFusionRecipe(1030, 1030) && FindFusionRecipe(1030, 1030)->result == 1032,
        "native DoubleShooter pair givesGatling");
    Check(!FindFusionRecipe(kFusionNoPlant, 0) && !FindFusionRecipe(0, kFusionNoPlant), "invalid IDs have no recipe");
    FusionSimulation sim;
    Check(sim.Reset() && sim.State().columns == 10 && sim.State().rows == 5 && sim.State().sun == 500,
        "source fresh Adventure dimensions and currency");
    Check(Near(FusionColumnX(0), -4.8f) && Near(FusionColumnX(9), 7.35f) && Near(FusionRowY(4), -4.38f),
        "source ten column geometry retains last column");
    Check(!FusionSupportsPlant(2) && !FusionSupportsPlant(65535) && !FusionSupportsZombie(2),
        "unported behavior is explicitly unsupported");
    FusionPlantRequest first = Request(0, 2, 2, 0, 100);
    first.owner = 1; first.packetId = 17; first.randomOrigin = 10;
    FusionPlantingResult planted = sim.TryPlant(first);
    Check(planted.status == FusionPlantingStatus::Planted && planted.plantId && sim.State().sun == 400,
        "validated used card spends only on successful placement");
    FusionSimulationState damaged = sim.State(); damaged.plants[0].health = 10;
    Check(sim.ApplySnapshot(damaged), "damaged source parent is valid");
    FusionPlantRequest second = Request(1, 2, 2, planted.plantId, 12);
    second.owner = 2; second.packetId = 23; second.randomOrigin = 5;
    FusionPlantingResult fused = sim.TryPlant(second);
    const FusionPlantState* result = sim.PlantAt(2, 2);
    Check(fused.status == FusionPlantingStatus::Fused && fused.consumedId == planted.plantId &&
        result && result->type == 1000 && result->health == 300 && sim.State().sun == 388,
        "fusion creates full source health and spends only supplied used packet price");
    Check(result && result->firstParent == 0 && result->secondParent == 1 && result->owner == 2 &&
        result->packetId == 23 && result->randomOrigin == 5, "actual parent order and canonical bank provenance survive fusion");
    const FusionSimulationState accepted = sim.State();
    FusionPlantingResult stale = sim.TryPlant(Request(0, 2, 2, planted.plantId, 100));
    Check(stale.status == FusionPlantingStatus::StaleTarget && sim.State().sun == accepted.sun &&
        sim.State().rngState == accepted.rngState && sim.State().nextId == accepted.nextId &&
        sim.State().plants[0].id == accepted.plants[0].id, "concurrent stale fusion cannot consume a replacement or advance RNG");
    Check(sim.TryPlant(Request(0, 10, 0)).status == FusionPlantingStatus::InvalidCell, "outside source grid rejected");
    Check(sim.TryPlant(Request(2, 0, 0)).status == FusionPlantingStatus::UnsupportedPlant, "catalog alone does not unlock an unported plant");
    Check(sim.TryPlant(Request(0, 0, 0, 0, -1)).status == FusionPlantingStatus::InvalidRequest, "negative authoritative price rejected");
    Check(sim.TryPlant(Request(0, 0, 0, 0, 389)).status == FusionPlantingStatus::InsufficientSun && sim.State().sun == 388,
        "insufficient sun leaves currency untouched");
    FusionSimulationState malformed = accepted;
    malformed.plants[0].pendingSun = std::numeric_limits<float>::quiet_NaN();
    Check(!sim.ApplySnapshot(malformed) && sim.State().rngState == accepted.rngState && sim.State().plants[0].id == accepted.plants[0].id,
        "NaN snapshot rejected transactionally");
    malformed = accepted;
    malformed.plants.push_back(malformed.plants[0]);
    malformed.plants.back().id = malformed.nextId++;
    Check(!sim.ApplySnapshot(malformed) && sim.State().plants.size() == 1, "duplicate cell cannot partially apply");
    malformed = accepted;
    FusionProjectileState duplicate;
    duplicate.id = malformed.plants[0].id; duplicate.damage = 20;
    malformed.projectiles.push_back(duplicate);
    Check(!sim.ApplySnapshot(malformed) && sim.State().projectiles.empty(), "cross-kind duplicate entity ID rejected");
    malformed = accepted; malformed.plants[0].owner = 4;
    Check(!sim.ApplySnapshot(malformed), "invalid owner rejected");
    malformed = accepted; malformed.plants[0].secondParent = kFusionNoPlant;
    Check(!sim.ApplySnapshot(malformed), "incomplete fusion parent pair rejected");
    const uint32_t oldTick = sim.State().tick;
    Check(!sim.Step(std::numeric_limits<float>::infinity()) && !sim.Step(-0.01f) && sim.State().tick == oldTick,
        "invalid time cannot step the host");

    sim.Reset();
    planted = sim.TryPlant(Request(0, 0, 0));
    FusionSimulation unsupportedSim;
    unsupportedSim.Reset();
    const auto sunflower = unsupportedSim.TryPlant(Request(1, 0, 0));
    const auto base = unsupportedSim.State();
    const auto unsupported = unsupportedSim.TryPlant(Request(1, 0, 0, sunflower.plantId, 100));
    Check(unsupported.status == FusionPlantingStatus::UnsupportedResult && unsupportedSim.State().sun == base.sun &&
        unsupportedSim.State().plants[0].id == sunflower.plantId && unsupportedSim.State().rngState == base.rngState,
        "supported parents with unported TwinFlower result fail atomically");
    sim.SpawnZombie(0, 0, FusionColumnX(7)); ReadyShot(sim);
    Check(Advance(sim, 33) && sim.State().projectiles.empty(), "Pea does not spawn before source AnimShoot event");
    Check(Advance(sim, 1) && sim.State().projectiles.size() == 1 && sim.State().projectiles[0].type == 0 &&
        sim.State().projectiles[0].damage == 20 && Near(sim.State().projectiles[0].x, FusionColumnX(0) + 0.514f + 0.06f),
        "Pea source event origin,20damage and6world units per second");
    Check(FusionSimulation::ValidateSnapshot(sim.State()), "running source Pea state satisfies wire validation");

    sim.Reset();
    planted = sim.TryPlant(Request(0, 4, 2));
    fused = sim.TryPlant(Request(0, 4, 2, planted.plantId));
    Check(fused.status == FusionPlantingStatus::Fused && fused.resultType == 1030, "first repeated pea gives DoubleShooter");
    fused = sim.TryPlant(Request(0, 4, 2, fused.plantId));
    Check(fused.status == FusionPlantingStatus::Fused && fused.resultType == 1090, "third source pea gives supported SplitPea");
    fused = sim.TryPlant(Request(0, 4, 2, fused.plantId));
    Check(fused.status == FusionPlantingStatus::Fused && fused.resultType == 1032, "fourth source pea completes tutorial Gatling chain");

    sim.Reset(); sim.TryPlant(Request(1090, 4, 0)); sim.SpawnZombie(0, 0, FusionColumnX(1)); ReadyShot(sim);
    Check(Advance(sim, 33) && sim.State().projectiles.empty(), "SplitPea does not fire before source event");
    Check(Advance(sim, 1) && sim.State().projectiles.size() == 2 &&
        sim.State().projectiles[0].type == 0 && sim.State().projectiles[0].movingWay == 9 &&
        sim.State().projectiles[0].specialBulletType == 1 &&
        Near(sim.State().projectiles[0].x, FusionColumnX(4) - 0.9703125f - 0.06f) &&
        sim.State().projectiles[1].movingWay == 0,
        "SplitPea rear and front source event uses Pea0 and negative movingWay9");
    Check(Advance(sim, 17) && sim.State().projectiles.size() == 3 &&
        sim.State().projectiles[2].movingWay == 9 && sim.State().projectiles[2].specialBulletType == 1,
        "SplitPea AnimShoot2 adds only the second rear pea");
    Check(FusionSimulation::ValidateSnapshot(sim.State()), "SplitPea source phases and projectile directions validate");

    sim.Reset();
    FusionSimulationState boundary = sim.State();
    FusionProjectileState boundaryPea;
    boundaryPea.id = boundary.nextId++;
    boundaryPea.row = 0;
    boundaryPea.damage = 20;
    boundaryPea.y = FusionRowY(0) + 0.6f;
    boundaryPea.x = FusionColumnX(9) + 2.9f;
    boundary.projectiles.push_back(boundaryPea);
    Check(sim.ApplySnapshot(boundary) && sim.Step(0.01f) && sim.State().projectiles.size() == 1,
        "ordinary pea survives past former one-unit-too-early cull boundary");
    boundary = sim.State(); boundary.projectiles[0].x = FusionColumnX(9) + 3.84f;
    Check(sim.ApplySnapshot(boundary) && sim.Step(0.01f) && sim.State().projectiles.empty(),
        "ordinary pea dies past source last-column plus3.85 boundary");
    boundaryPea.x = -9.99f;
    boundaryPea.movingWay = 9;
    boundaryPea.specialBulletType = 1;
    boundary = sim.State(); boundary.projectiles.push_back(boundaryPea);
    Check(sim.ApplySnapshot(boundary) && sim.Step(0.01f) && sim.State().projectiles.size() == 1 &&
        sim.State().projectiles[0].movingWay == 0 && sim.State().projectiles[0].specialBulletType == 2 &&
        Near(sim.State().projectiles[0].x, -10.05f), "rear pea bounces below minus10 without clamping overshoot");
    Check(sim.Step(0.01f) && Near(sim.State().projectiles[0].x, -9.99f), "bounced pea travels forward at source speed6");
    boundary = sim.State(); boundary.projectiles[0].x = -10.5f;
    Check(sim.ApplySnapshot(boundary) && sim.Step(0.01f) && sim.State().projectiles.size() == 1 &&
        Near(sim.State().projectiles[0].x, -10.44f), "special2 survives left-edge overshoot while returning toward field");
    boundary = sim.State(); boundary.projectiles[0].x = FusionColumnX(9) + 2.84f;
    Check(sim.ApplySnapshot(boundary) && sim.Step(0.01f) && sim.State().projectiles.empty(),
        "bounced pea uses special source right boundary plus2.85");
    boundaryPea.x = 0.0f; boundaryPea.movingWay = 0; boundaryPea.specialBulletType = 0;
    boundaryPea.existTime = 19.9f;
    boundary = sim.State(); boundary.projectiles.push_back(boundaryPea);
    Check(sim.ApplySnapshot(boundary) && sim.Step(0.1f) && sim.State().projectiles.size() == 1 &&
        Near(sim.State().projectiles[0].existTime, 20.0f), "source projectile remains alive at exactly twenty seconds");
    Check(sim.Step(0.01f) && sim.State().projectiles.empty(), "source projectile expires strictly after twenty seconds");
    boundaryPea.existTime = 0.0f; boundaryPea.movingWay = 9;
    boundary = sim.State(); boundary.projectiles.push_back(boundaryPea);
    Check(!sim.ApplySnapshot(boundary), "rear direction without Split_left state is rejected");
    boundary.projectiles[0].specialBulletType = 1;
    boundary.projectiles[0].existTime = std::numeric_limits<float>::quiet_NaN();
    Check(!sim.ApplySnapshot(boundary), "nonfinite projectile lifetime is rejected transactionally");

    sim.Reset(); sim.TryPlant(Request(1030, 0, 0)); sim.SpawnZombie(0, 0, FusionColumnX(8)); ReadyShot(sim);
    Check(Advance(sim, 28) && sim.State().projectiles.size() == 1, "DoubleShooter first source event");
    Check(Advance(sim, 27) && sim.State().projectiles.size() == 2, "DoubleShooter second source event without extra burst timer");
    sim.Reset(); sim.TryPlant(Request(1032, 0, 0)); sim.SpawnZombie(0, 0, FusionColumnX(8)); ReadyShot(sim);
    Check(Advance(sim, 36) && sim.State().projectiles.size() == 1, "Gatling first source event");
    Check(Advance(sim, 16) && sim.State().projectiles.size() == 2, "Gatling second source event");
    Check(Advance(sim, 17) && sim.State().projectiles.size() == 3, "Gatling third source event");
    Check(Advance(sim, 17) && sim.State().projectiles.size() == 4, "Gatling fourth source event");

    sim.Reset(); sim.TryPlant(Request(0, 0, 0)); sim.SpawnZombie(0, 1, FusionColumnX(7)); ReadyShot(sim);
    Check(Advance(sim, 100) && sim.State().projectiles.empty(), "different row never triggers ordinary Pea shot");
    sim.Reset(); sim.TryPlant(Request(1000, 0, 0)); sim.SpawnZombie(0, 0, FusionColumnX(1)); ReadyShot(sim);
    Check(Advance(sim, 50) && sim.State().zombies.size() == 1 && Near(sim.State().zombies[0].health, 170.0f) &&
        sim.State().zombies[0].lostArm && sim.State().suns.size() == 1 && sim.State().suns[0].amount == 5,
        "PeaSun source projectile deals100 and drops5sun on hit");
    FusionSimulationState headless = sim.State();
    headless.zombies[0].health = 70.0f; headless.zombies[0].headless = true;
    headless.zombies[0].attackTarget = 0;
    Check(sim.ApplySnapshot(headless) && Advance(sim, 100) && Near(sim.State().zombies[0].health, 43.0f),
        "headless source zombie drains27HP per second");
    Check(Advance(sim, 170) && sim.State().zombies.empty() && sim.State().killedZombies == 1,
        "headless decay finishes as a single kill");

    sim.Reset(); sim.TryPlant(Request(1, 0, 0)); sim.SpawnZombie(0, 0, FusionColumnX(0) + 0.5f);
    FusionSimulationState eating = sim.State(); eating.zombies[0].animationSpeed = 1.0f; eating.zombies[0].fastWalk = false;
    Check(sim.ApplySnapshot(eating), "source contact fixture accepted");
    Check(Advance(sim, 33) && sim.State().plants[0].health == 300, "no bite before0.33333334 clip event");
    Check(Advance(sim, 1) && sim.State().plants[0].health == 250, "first source bite costs50health");
    Check(Advance(sim, 75) && sim.State().plants[0].health == 250, "bite events are not a uniform guessed cadence");
    Check(Advance(sim, 2) && sim.State().plants[0].health == 200, "second source bite at1.1seconds");

    sim.Reset(); sim.SpawnZombie(0, 0, 0.0f);
    FusionSimulationState walk = sim.State(); walk.zombies[0].animationSpeed = 1.0f; walk.zombies[0].fastWalk = false;
    Check(sim.ApplySnapshot(walk) && Advance(sim, 10, 1.0f/24.0f) &&
        Near(sim.State().zombies[0].x, (-0.9499999f + 0.81666666f) * 0.31f),
        "walk follows source root-Hermite key rather than Classic constant velocity");

    sim.Reset(); sim.TryPlant(Request(1, 0, 0));
    FusionSimulationState produce = sim.State(); produce.plants[0].produceCountdown = 0.005f;
    Check(sim.ApplySnapshot(produce) && Advance(sim, 1) && sim.State().suns.empty() && Near(sim.State().plants[0].pendingSun, 0.5f),
        "SunFlower production has original half-second invoke delay");
    Check(Advance(sim, 52) && sim.State().suns.size() == 1 && sim.State().suns[0].amount == 25 &&
        sim.State().plants[0].produceCountdown > 22.0f && sim.State().plants[0].produceCountdown < 27.0f,
        "SunFlower source production25 and interval jitter");
    if (sim.State().suns.empty()) return {checks, failures};
    const uint32_t sunId = sim.State().suns[0].id;
    const int beforeCollect = sim.State().sun;
    Check(sim.CollectSun(sunId) && !sim.CollectSun(sunId) && sim.State().sun == beforeCollect,
        "pickup begins once and does not credit before source HUD flight");
    Check(Advance(sim, 150) && sim.State().sun == beforeCollect + 25 && sim.State().suns.empty(),
        "sun credits once after source collection animation");
    Check(FusionSimulation::ValidateSnapshot(sim.State()), "post-collection snapshot remains valid");

    sim.Reset();
    FusionSimulationState nativeSuns = sim.State();
    FusionSunState normalSun;
    normalSun.id = nativeSuns.nextId++;
    normalSun.row = 0;
    normalSun.x = -8.0f;
    normalSun.y = normalSun.startY = kFusionSunCollectionTargetY;
    normalSun.landed = true;
    nativeSuns.suns.push_back(normalSun);
    FusionSunState littleSun = normalSun;
    littleSun.id = nativeSuns.nextId++;
    littleSun.amount = 5;
    littleSun.scale = 0.5f;
    nativeSuns.suns.push_back(littleSun);
    Check(sim.ApplySnapshot(nativeSuns), "canonical native threshold sun fixture accepted");
    Check(sim.CollectSun(normalSun.id) && sim.CollectSun(littleSun.id) && !sim.CollectSun(littleSun.id) &&
        sim.State().sun == 500 && Near(sim.State().suns[0].collectionStartDistance, 8.0f/60.0f),
        "pickup records initial world distance and pending25plus5 currency stays uncredited");
    Check(sim.Step(0.01f) && sim.State().sun == 500 && sim.State().suns.size() == 2 &&
        Near(sim.State().suns[0].scale, 0.5f) && Near(sim.State().suns[1].scale, 0.25f),
        "native exact8pixel pre-move distance does not score even after entering threshold");
    Check(sim.Step(0.01f) && sim.State().sun == 530 && sim.State().suns.empty(),
        "next native tick credits Fusion25and5 exactly once without ClassicSmallSun15 substitution");
    Check(!sim.CollectSun(normalSun.id) && !sim.CollectSun(littleSun.id) && Advance(sim, 20) && sim.State().sun == 530,
        "consumed sun IDs and repeated requests cannot create extra currency");

    sim.Reset();
    nativeSuns = sim.State();
    normalSun.id = nativeSuns.nextId++;
    normalSun.x = 0.0f; normalSun.y = normalSun.startY = 0.0f;
    normalSun.collecting = false; normalSun.collectionStartDistance = 0.0f; normalSun.scale = 1.0f;
    nativeSuns.suns.push_back(normalSun);
    Check(sim.ApplySnapshot(nativeSuns) && sim.CollectSun(normalSun.id), "far native collection fixture begins");
    FusionSimulation splitTicks;
    Check(splitTicks.ApplySnapshot(sim.State()) && sim.Step(0.1f) && Advance(splitTicks, 10) &&
        sim.State().sun == splitTicks.State().sun && sim.State().suns.size() == 1 &&
        splitTicks.State().suns.size() == 1 && Near(sim.State().suns[0].x, splitTicks.State().suns[0].x) &&
        Near(sim.State().suns[0].y, splitTicks.State().suns[0].y) &&
        Near(sim.State().suns[0].scale, splitTicks.State().suns[0].scale),
        "native ten tick substep matches ten100Hz updates within snapshot float tolerance");
    FusionSimulation lateJoinSun;
    Check(lateJoinSun.ApplySnapshot(sim.State()) &&
        lateJoinSun.State().suns[0].collectionStartDistance == sim.State().suns[0].collectionStartDistance &&
        Advance(sim, 150) && Advance(lateJoinSun, 150) && sim.State().sun == 525 &&
        lateJoinSun.State().sun == 525 && sim.State().suns.empty() && lateJoinSun.State().suns.empty(),
        "restored collection phase preserves initial-distance metadata and single final credit");
    nativeSuns.suns[0].collecting = true;
    nativeSuns.suns[0].collectionStartDistance = std::numeric_limits<float>::quiet_NaN();
    Check(!sim.ApplySnapshot(nativeSuns), "nonfinite collection initial distance rejects snapshot transactionally");
    nativeSuns.suns[0].collecting = false;
    nativeSuns.suns[0].collectionStartDistance = 1.0f;
    Check(!sim.ApplySnapshot(nativeSuns), "uncollected sun cannot carry fake flight-start distance");
    nativeSuns.suns[0].collecting = true;
    nativeSuns.suns[0].collectionStartDistance = 0.0f;
    Check(!sim.ApplySnapshot(nativeSuns), "far collecting sun cannot claim an already completed flight");
    nativeSuns.suns[0].collectionStartDistance = 1.0f;
    Check(!sim.ApplySnapshot(nativeSuns), "collecting sun start distance cannot be shorter than its remaining flight");
    nativeSuns.suns[0].x = kFusionSunCollectionTargetX;
    nativeSuns.suns[0].y = kFusionSunCollectionTargetY;
    nativeSuns.suns[0].collectionStartDistance = 0.0f;
    Check(sim.ApplySnapshot(nativeSuns), "zero-distance collection is valid at the native bank target");
    return {checks, failures};
}
