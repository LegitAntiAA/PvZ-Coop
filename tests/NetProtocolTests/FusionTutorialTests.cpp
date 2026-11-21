#include "FusionTutorialTests.h"
#include "../../Lawn/Fusion/FusionTutorial.h"
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
    if (!success) { ++failures; std::printf("FAIL FusionTutorial %s\n", message); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
}

FusionTutorialTestResult RunFusionTutorialTests()
{
    checks = failures = 0;
    using namespace LawnFusion;
    FusionTutorial tutorial;
    FusionSimulationState simulation;
    FusionTutorialEvents events;
    tutorial.ResetAdventure1(false);
    Check(tutorial.Step(100.0f, simulation, events) && !tutorial.State().active && !events.messageChanged,
        "disabled tutorial does not create messages or cards");
    tutorial.ResetAdventure1();
    Check(tutorial.Step(0.0f, simulation, events) && tutorial.State().sourceProgress == 1 &&
        tutorial.State().message == FusionTutorialMessage::Welcome && Near(tutorial.State().timer, 3.0f),
        "first source update shows welcome and waits three seconds");
    Check(tutorial.Step(2.0f, simulation, events) && tutorial.State().sourceProgress == 1 &&
        Near(tutorial.State().timer, 1.0f) && !events.messageChanged, "clock does not instantly skip welcome");
    Check(tutorial.Step(1.0f, simulation, events) && tutorial.State().sourceProgress == 2 &&
        tutorial.State().message == FusionTutorialMessage::NewPlants && Near(tutorial.State().timer, 4.0f),
        "source second introduction waits four seconds");
    Check(tutorial.Step(100.0f, simulation, events) && tutorial.State().sourceProgress == 3 &&
        tutorial.State().message == FusionTutorialMessage::Resources && Near(tutorial.State().timer, 3.0f),
        "large frame advances one original switch case only");
    Check(tutorial.Step(3.0f, simulation, events) && tutorial.State().sourceProgress == 4 &&
        tutorial.State().message == FusionTutorialMessage::PlantSunflower &&
        Near(tutorial.State().messageRemaining, 60.0f), "sunflower prompt has original sixty second display");
    Check(tutorial.Step(61.0f, simulation, events) && tutorial.State().sourceProgress == 4 &&
        Near(tutorial.State().messageRemaining, 0.0f), "expired message cannot bypass plant condition");
    FusionPlantState plant;
    plant.type = 0;
    simulation.plants.push_back(plant);
    Check(tutorial.Step(0.01f, simulation, events) && tutorial.State().sourceProgress == 4,
        "ordinary pea does not satisfy sunflower condition");
    simulation.plants[0].type = 1;
    simulation.plants[0].owner = 3;
    simulation.plants[0].column = 9;
    simulation.plants[0].row = 4;
    Check(tutorial.Step(0.01f, simulation, events) && tutorial.State().sourceProgress == 5 &&
        tutorial.State().message == FusionTutorialMessage::FusePeaSunflower && !events.grantMissionCards,
        "source checks any sunflower independent of player or cell");
    Check(tutorial.Step(0.01f, simulation, events) && tutorial.State().sourceProgress == 5,
        "sunflower alone does not satisfy fusion condition");
    simulation.plants[0].type = 1000;
    Check(tutorial.Step(0.01f, simulation, events) && tutorial.State().sourceProgress == 6 &&
        tutorial.State().message == FusionTutorialMessage::BuildGatling &&
        Near(tutorial.State().messageRemaining, 600.0f) && events.grantMissionCards,
        "pea sunflower milestone grants cards and original six hundred second prompt");
    for (size_t i = 0; i < events.missionCards.size(); ++i)
        Check(events.missionCards[i].type == 0 && events.missionCards[i].cost == 0 &&
            events.missionCards[i].column == 4 + i && events.missionCards[i].row == 2,
            "mission effect contains free source pea card at exact source cell");
    FusionTutorial lateJoin;
    Check(lateJoin.ApplySnapshot(tutorial.State()), "late join restores progress and clocks");
    Check(lateJoin.Step(0.01f, simulation, events) && !events.grantMissionCards &&
        lateJoin.State().sourceProgress == 6, "snapshot application cannot grant mission cards again");
    Check(tutorial.Step(0.01f, simulation, events) && !events.grantMissionCards,
        "source milestone grants only once on next host update");
    simulation.plants[0].type = 1030;
    Check(tutorial.Step(0.01f, simulation, events) && tutorial.State().active && !events.completed,
        "double shooter does not complete Gatling condition");
    simulation.plants[0].type = 1090;
    Check(tutorial.Step(0.01f, simulation, events) && tutorial.State().active && !events.completed,
        "split pea does not complete Gatling condition");
    simulation.plants[0].type = 1032;
    Check(tutorial.Step(0.01f, simulation, events) && !tutorial.State().active && events.completed &&
        tutorial.State().message == FusionTutorialMessage::Ready && Near(events.nextWaveCountdown, 5.0f) &&
        Near(tutorial.State().messageRemaining, 5.0f), "Gatling clears tutorial immediately with five second wave countdown");
    Check(tutorial.Step(1.0f, simulation, events) && !events.completed && !events.grantMissionCards &&
        Near(tutorial.State().messageRemaining, 4.0f), "finished tutorial emits no duplicate completion");
    const FusionTutorialState before = tutorial.State();
    events.completed = true;
    Check(!tutorial.Step(-1.0f, simulation, events) && tutorial.State().timer == before.timer && events.completed,
        "invalid negative delta preserves state and caller effects");
    Check(!tutorial.Step(std::numeric_limits<float>::quiet_NaN(), simulation, events) &&
        tutorial.State().messageRemaining == before.messageRemaining, "nonfinite delta rolls back");
    FusionTutorialState invalid = before;
    invalid.sourceProgress = 7;
    Check(!tutorial.ApplySnapshot(invalid) && tutorial.State().sourceProgress == before.sourceProgress,
        "invalid source progress rejects snapshot without mutation");
    invalid = before;
    invalid.timer = std::numeric_limits<float>::infinity();
    Check(!tutorial.ApplySnapshot(invalid), "nonfinite source clock is rejected");
    invalid = before;
    invalid.message = FusionTutorialMessage::Welcome;
    Check(!tutorial.ApplySnapshot(invalid), "completed phase requires original final message");
    invalid = before;
    invalid.messageRemaining = 5.01f;
    Check(!tutorial.ApplySnapshot(invalid), "message clock cannot exceed source duration");
    invalid = before;
    invalid.message = static_cast<FusionTutorialMessage>(255);
    Check(!tutorial.ApplySnapshot(invalid), "unknown message enum is rejected");

    FusionDroppedCards cards;
    Check(cards.Spawn(1000, 0, 0, FusionColumnX(4), FusionRowY(2), 1.0f, 2.5f),
        "source mission pea spawns with externally allocated world ID");
    Check(!cards.Spawn(1000, 0, 0, 0.0f, 0.0f, 0.0f, 2.0f) &&
        !cards.Spawn(1001, 0, 0, 0.0f, 0.0f, 2.1f, 2.0f),
        "duplicate card ID or motion outside original random range is rejected");
    Check(cards.Step(0.1f, true) && Near(cards.State().cards[0].velocityY, 1.52f) &&
        Near(cards.State().cards[0].x, FusionColumnX(4) + 0.1f) &&
        Near(cards.State().cards[0].y, FusionRowY(2) + 0.152f), "source card uses gravity9.8 and updated velocity");
    Check(cards.PickUp(1000, 2) && !cards.PickUp(1000, 1) && !cards.PickUp(1000, 2),
        "one authenticated player claims world card and concurrent pickup loses");
    const FusionDroppedCardState held = cards.State().cards[0];
    Check(cards.Step(30.0f, false) && Near(cards.State().cards[0].existTime, held.existTime) &&
        Near(cards.State().cards[0].x, held.x) && cards.State().cards[0].landed,
        "held source card freezes motion and expiry clock");
    FusionDroppedCards joinedCards;
    Check(joinedCards.ApplySnapshot(cards.State()) && joinedCards.State().cards[0].heldSlot == 2,
        "late join restores exact world-card claim and clocks");
    Check(!cards.PutDown(1000, 1) && cards.PutDown(1000, 2) && cards.Step(0.1f, true) &&
        Near(cards.State().cards[0].x, held.x) && cards.State().cards[0].landed,
        "cancel by owner releases card while source permanently-landed state survives");
    Check(cards.Step(16.0f, true) && cards.State().cards.size() == 1 &&
        cards.State().cards[0].existTime > 16.0f && !cards.State().cards[0].flashing,
        "tutorial suppresses expiry and flashing while elapsed lifetime accumulates");
    Check(cards.Step(0.0f, false) && cards.State().cards.empty(),
        "leftover unheld card expires when tutorial suppression ends");

    cards.Spawn(1002, 0, 0, 0.0f, 0.0f, 0.0f, 2.0f);
    Check(cards.Step(0.61f, true) && !cards.State().cards[0].landed && cards.Step(0.0f, true) &&
        cards.State().cards[0].landed, "source card landing reads lifetime before increment and requires greater-than0.6");
    Check(cards.Step(9.5f, false) && cards.State().cards[0].flashing &&
        Near(cards.State().cards[0].flashTime, 9.5f), "unheld card begins flashing after source ten second threshold");
    cards.Reset();
    const float maxCardX = FusionColumnX(9) + 1.85f - 2.0f;
    cards.Spawn(1003, 0, 0, maxCardX + 0.1f, 0.0f, 2.0f, 2.0f);
    Check(cards.Step(0.01f, true) && Near(cards.State().cards[0].x, maxCardX) &&
        Near(cards.State().cards[0].velocityX, -1.0f), "source right-edge clamp sets card horizontal velocity minus1");
    cards.Reset();
    cards.Spawn(1004, 0, 0, 0.0f, -10.0f, 0.0f, 2.0f);
    Check(cards.Step(0.01f, true) && cards.State().cards[0].landed &&
        Near(cards.State().cards[0].y, FusionRowY(4) - 0.1f), "source bottom clamp is boardMinY plus0.5");

    FusionSimulation world;
    cards.Reset();
    cards.Spawn(1010, 0, 0, 0.0f, 0.0f, 0.0f, 2.0f);
    cards.Spawn(1011, 0, 0, 1.0f, 0.0f, 0.0f, 2.0f);
    Check(cards.PickUp(1010, 0) && !cards.PickUp(1011, 0), "one player cannot hold two world cards");
    FusionPlantingResult placement = cards.TryPlant(1010, 1, 4, 2, 0, world);
    Check(placement.status == FusionPlantingStatus::InvalidRequest && cards.State().cards.size() == 2 && world.State().plants.empty(),
        "wrong authenticated slot cannot plant another player's claimed card");
    placement = cards.TryPlant(1010, 0, 4, 2, 123, world);
    Check(placement.status == FusionPlantingStatus::StaleTarget && cards.State().cards.size() == 2 && world.State().sun == 500,
        "stale world-card placement does not consume card or currency");
    placement = cards.TryPlant(1010, 0, 4, 2, 0, world);
    Check(placement.status == FusionPlantingStatus::Planted && cards.State().cards.size() == 1 &&
        world.State().sun == 500 && world.State().plants[0].owner == 0 && world.State().plants[0].packetId == 0,
        "successful free world card is consumed once without bank cooldown or sun cost");
    Check(cards.TryPlant(1010, 0, 4, 2, placement.plantId, world).status == FusionPlantingStatus::InvalidRequest &&
        world.State().plants[0].id == placement.plantId, "replayed consumed card cannot mutate its replacement");
    Check(cards.PickUp(1011, 1), "second player can claim next world card");
    const FusionPlantingResult worldFusion = cards.TryPlant(1011, 1, 4, 2, placement.plantId, world);
    Check(worldFusion.status == FusionPlantingStatus::Fused && worldFusion.resultType == 1030 &&
        world.State().sun == 500 && cards.State().cards.empty(), "free source card uses ordinary directional fusion fallback atomically");
    cards.Spawn(1020, 0, 0, 0.0f, 0.0f, 0.0f, 2.0f);
    cards.PickUp(1020, 3);
    cards.ReleaseSlot(3);
    Check(cards.State().cards[0].heldSlot == 0xff && cards.State().cards[0].landed,
        "disconnect releases claim without deleting source world card");
    FusionDroppedCardsState badCards = cards.State();
    badCards.cards.push_back(badCards.cards[0]);
    Check(!cards.ApplySnapshot(badCards) && cards.State().cards.size() == 1, "duplicate world-card snapshot ID rolls back");
    badCards = cards.State();
    badCards.cards[0].heldSlot = 4;
    Check(!cards.ApplySnapshot(badCards), "out-of-range world-card claim rejects snapshot");
    badCards = cards.State();
    badCards.cards[0].velocityY = std::numeric_limits<float>::quiet_NaN();
    Check(!cards.ApplySnapshot(badCards), "nonfinite world-card movement rejects snapshot");
    return {checks, failures};
}
