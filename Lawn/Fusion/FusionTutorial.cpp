#include "FusionTutorial.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace LawnFusion
{
namespace
{
bool HasPlant(const FusionSimulationState& simulation, FusionPlantId type)
{
    for (const FusionPlantState& plant : simulation.plants)
        if (plant.type == type) return true;
    return false;
}

float MessageDuration(FusionTutorialMessage message)
{
    switch (message)
    {
    case FusionTutorialMessage::None: return 0.0f;
    case FusionTutorialMessage::Welcome: return 3.0f;
    case FusionTutorialMessage::NewPlants: return 4.0f;
    case FusionTutorialMessage::Resources: return 3.0f;
    case FusionTutorialMessage::PlantSunflower:
    case FusionTutorialMessage::FusePeaSunflower: return 60.0f;
    case FusionTutorialMessage::BuildGatling: return 600.0f;
    case FusionTutorialMessage::Ready: return 5.0f;
    }
    return -1.0f;
}

void ShowMessage(FusionTutorialState& state, FusionTutorialEvents& events, FusionTutorialMessage message)
{
    state.message = message;
    state.messageRemaining = MessageDuration(message);
    events.messageChanged = true;
}
}

void FusionTutorial::ResetAdventure1(bool enabled)
{
    mState = FusionTutorialState{};
    mState.active = enabled;
}

bool FusionTutorial::Step(float deltaSeconds, const FusionSimulationState& simulation, FusionTutorialEvents& events)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) return false;
    FusionTutorialState next = mState;
    FusionTutorialEvents effects;
    next.messageRemaining = std::max(0.0f, next.messageRemaining - deltaSeconds);
    if (next.active)
    {
        next.timer -= deltaSeconds;
        if (next.timer <= 0.0f)
        {
            // Original Update calls TutorUpdate1 once, so a large frame may
            // advance one switch case only; it never skips the three messages.
            switch (next.sourceProgress)
            {
            case 0:
                ShowMessage(next, effects, FusionTutorialMessage::Welcome);
                next.timer = 3.0f;
                ++next.sourceProgress;
                break;
            case 1:
                ShowMessage(next, effects, FusionTutorialMessage::NewPlants);
                next.timer = 4.0f;
                ++next.sourceProgress;
                break;
            case 2:
                ShowMessage(next, effects, FusionTutorialMessage::Resources);
                next.timer = 3.0f;
                ++next.sourceProgress;
                break;
            case 3:
                ShowMessage(next, effects, FusionTutorialMessage::PlantSunflower);
                ++next.sourceProgress;
                break;
            case 4:
                if (HasPlant(simulation, 1))
                {
                    ShowMessage(next, effects, FusionTutorialMessage::FusePeaSunflower);
                    ++next.sourceProgress;
                }
                break;
            case 5:
                if (HasPlant(simulation, 1000))
                {
                    ShowMessage(next, effects, FusionTutorialMessage::BuildGatling);
                    effects.grantMissionCards = true;
                    for (size_t i = 0; i < effects.missionCards.size(); ++i)
                    {
                        effects.missionCards[i].type = 0;
                        effects.missionCards[i].column = static_cast<uint8_t>(4 + i);
                        effects.missionCards[i].row = 2;
                        effects.missionCards[i].cost = 0;
                    }
                    ++next.sourceProgress;
                }
                break;
            case 6:
                if (HasPlant(simulation, 1032))
                {
                    ShowMessage(next, effects, FusionTutorialMessage::Ready);
                    next.active = false;
                    effects.completed = true;
                    effects.nextWaveCountdown = 5.0f;
                }
                break;
            }
        }
    }
    if (!ValidateSnapshot(next)) return false;
    mState = next;
    events = effects;
    return true;
}

bool FusionTutorial::ValidateSnapshot(const FusionTutorialState& state)
{
    if (state.sourceProgress > 6 || !std::isfinite(state.timer) || state.timer > 4.0f ||
        !std::isfinite(state.messageRemaining) || state.messageRemaining < 0.0f) return false;
    const float duration = MessageDuration(state.message);
    if (duration < 0.0f || state.messageRemaining > duration) return false;
    if (state.sourceProgress == 0)
        return state.timer == 0.0f && state.message == FusionTutorialMessage::None && state.messageRemaining == 0.0f;
    if (!state.active)
        return state.sourceProgress == 6 && state.message == FusionTutorialMessage::Ready && state.timer <= 0.0f;
    // The prompt is the one immediately preceding the source wait stage.
    return static_cast<uint8_t>(state.message) == state.sourceProgress &&
        (state.sourceProgress <= 3 || state.timer <= 0.0f);
}

bool FusionTutorial::ApplySnapshot(const FusionTutorialState& state)
{
    if (!ValidateSnapshot(state)) return false;
    mState = state;
    return true;
}

void FusionDroppedCards::Reset()
{
    mState = FusionDroppedCardsState{};
}

bool FusionDroppedCards::Spawn(uint32_t roomCardId, FusionPlantId type, int cost, float x, float y,
    float velocityX, float velocityY)
{
    if (!roomCardId || !FusionSupportsPlant(type) || cost < 0 || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(velocityX) || velocityX < -2.0f || velocityX > 2.0f ||
        !std::isfinite(velocityY) || velocityY < 2.0f || velocityY > 3.0f ||
        mState.cards.size() >= kFusionMaxDroppedCards) return false;
    for (const FusionDroppedCardState& card : mState.cards) if (card.id == roomCardId) return false;
    FusionDroppedCardState card;
    card.id = roomCardId;
    card.type = type;
    card.cost = cost;
    card.x = x;
    card.y = y;
    card.velocityX = velocityX;
    card.velocityY = velocityY;
    mState.cards.push_back(card);
    return true;
}

bool FusionDroppedCards::PickUp(uint32_t id, uint8_t authenticatedSlot)
{
    if (authenticatedSlot >= 4) return false;
    for (const FusionDroppedCardState& card : mState.cards)
        if (card.heldSlot == authenticatedSlot) return false;
    for (FusionDroppedCardState& card : mState.cards)
        if (card.id == id && card.heldSlot == 0xff)
        {
            card.heldSlot = authenticatedSlot;
            card.landed = true;
            return true;
        }
    return false;
}

bool FusionDroppedCards::PutDown(uint32_t id, uint8_t authenticatedSlot)
{
    if (authenticatedSlot >= 4) return false;
    for (FusionDroppedCardState& card : mState.cards)
        if (card.id == id && card.heldSlot == authenticatedSlot)
        {
            card.heldSlot = 0xff;
            return true;
        }
    return false;
}

void FusionDroppedCards::ReleaseSlot(uint8_t authenticatedSlot)
{
    if (authenticatedSlot >= 4) return;
    for (FusionDroppedCardState& card : mState.cards)
        if (card.heldSlot == authenticatedSlot) card.heldSlot = 0xff;
}

FusionPlantingResult FusionDroppedCards::TryPlant(uint32_t id, uint8_t authenticatedSlot, int column, int row,
    uint32_t expectedExistingId, FusionSimulation& simulation)
{
    if (authenticatedSlot >= 4 || column < 0 || column >= simulation.State().columns ||
        row < 0 || row >= simulation.State().rows) return FusionPlantingResult{};
    const auto card = std::find_if(mState.cards.begin(), mState.cards.end(),
        [=](const FusionDroppedCardState& value) { return value.id == id && value.heldSlot == authenticatedSlot; });
    if (card == mState.cards.end()) return FusionPlantingResult{};
    FusionPlantRequest request;
    request.type = card->type;
    request.cost = card->cost;
    request.column = static_cast<uint8_t>(column);
    request.row = static_cast<uint8_t>(row);
    request.expectedExistingId = expectedExistingId;
    request.owner = authenticatedSlot;
    // A free world card has no bank packet or mystery origin.
    request.packetId = 0;
    request.randomOrigin = 0;
    const FusionPlantingResult result = simulation.TryPlant(request);
    if (result.status == FusionPlantingStatus::Planted || result.status == FusionPlantingStatus::Fused)
        mState.cards.erase(card);
    return result;
}

bool FusionDroppedCards::Step(float deltaSeconds, bool isTutorial, int columns, int rows)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f || columns < 1 || columns > 32 || rows < 1 || rows > 10)
        return false;
    FusionDroppedCardsState next = mState;
    const float minY = FusionRowY(rows - 1, rows) - 0.6f;
    const float maxX = FusionColumnX(columns - 1) + 1.85f;
    for (FusionDroppedCardState& card : next.cards)
    {
        if (!card.landed)
        {
            card.velocityY -= 9.8f * deltaSeconds;
            card.x += card.velocityX * deltaSeconds;
            card.y += card.velocityY * deltaSeconds;
            if (card.y < minY + 0.5f)
            {
                card.y = minY + 0.5f;
                card.landed = true;
            }
            if (card.x > maxX - 2.0f)
            {
                card.x = maxX - 2.0f;
                card.velocityX = -1.0f;
            }
            // Source PositionUpdate reads existTime before Update increments it.
            if (card.existTime > 0.6f) card.landed = true;
        }
        if (card.heldSlot != 0xff) continue;
        card.existTime += deltaSeconds;
        if (isTutorial) continue;
        if (card.existTime > 10.0f && card.existTime < 15.0f) card.flashing = true;
        if (card.flashing) card.flashTime += deltaSeconds;
    }
    if (!ValidateSnapshot(next)) return false;
    if (!isTutorial)
        next.cards.erase(std::remove_if(next.cards.begin(), next.cards.end(),
            [](const FusionDroppedCardState& card) { return card.heldSlot == 0xff && card.existTime > 15.0f; }), next.cards.end());
    mState = std::move(next);
    return true;
}

bool FusionDroppedCards::ValidateSnapshot(const FusionDroppedCardsState& state)
{
    if (state.cards.size() > kFusionMaxDroppedCards) return false;
    std::set<uint32_t> ids;
    unsigned claimed = 0;
    for (const FusionDroppedCardState& card : state.cards)
    {
        if (!card.id || !ids.insert(card.id).second || !FusionSupportsPlant(card.type) || card.cost < 0 ||
            !std::isfinite(card.x) || !std::isfinite(card.y) || !std::isfinite(card.velocityX) ||
            card.velocityX < -2.0f || card.velocityX > 2.0f || !std::isfinite(card.velocityY) || card.velocityY > 3.0f ||
            !std::isfinite(card.existTime) || card.existTime < 0.0f ||
            !std::isfinite(card.flashTime) || card.flashTime < 0.0f || (!card.flashing && card.flashTime != 0.0f)) return false;
        if (card.heldSlot == 0xff) continue;
        if (card.heldSlot >= 4 || !card.landed || (claimed & (1U << card.heldSlot))) return false;
        claimed |= 1U << card.heldSlot;
    }
    return true;
}

bool FusionDroppedCards::ApplySnapshot(const FusionDroppedCardsState& state)
{
    if (!ValidateSnapshot(state)) return false;
    mState = state;
    return true;
}
}
