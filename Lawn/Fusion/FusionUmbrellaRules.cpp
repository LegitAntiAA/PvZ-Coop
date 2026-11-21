#include "FusionUmbrellaRules.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr FusionUmbrellaPoseEventSpec events[] = {
    {FusionUmbrellaPoseEvent::BlockStart,0.0},
    {FusionUmbrellaPoseEvent::BlockOver,.75},
    {FusionUmbrellaPoseEvent::AllBlockOver,.75}};
bool PoseValid(const FusionUmbrellaPoseState& pose)
{ return pose.plantGeneration || (!pose.blocking && !pose.sortAtRoot); }
}
bool FusionUmbrellaCanIntercept(int sourceKind, int sourceWay, bool zombieBullet, bool hitLatched,
    int bulletRow, uint16_t attackedKind, int attackedColumn, int attackedRow,
    int umbrellaColumn, int umbrellaRow, bool awake, float sourceAnimatorSpeed)
{
    if (sourceKind < 0 || sourceKind >= 65535 || sourceKind == 3 || sourceWay != 13 || !zombieBullet || hitLatched ||
        attackedKind == 254 || attackedKind == 65535 || bulletRow != attackedRow || !awake ||
        !std::isfinite(sourceAnimatorSpeed) || sourceAnimatorSpeed == 0 ||
        attackedColumn < 0 || attackedRow < 0 || umbrellaColumn < 0 || umbrellaRow < 0) return false;
    const auto dx = int64_t(umbrellaColumn)-attackedColumn, dy = int64_t(umbrellaRow)-attackedRow;
    return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}
bool FusionUmbrellaBlockStateValid(const FusionUmbrellaBlockState& state)
{
    if (!state.projectileGeneration)
        return !state.umbrellaGeneration && state.blockedAtTick == -1 && !state.harmless && !state.pending;
    return state.umbrellaGeneration && state.blockedAtTick >= 0 && state.harmless;
}
bool TryBeginFusionUmbrellaBlock(uint32_t projectileGeneration, uint32_t umbrellaGeneration,
    int boardTick, FusionUmbrellaBlockState& state, FusionUmbrellaPoseState& pose, bool& startPose)
{
    if (!projectileGeneration || !umbrellaGeneration || boardTick < 0 || !FusionUmbrellaBlockStateValid(state) ||
        state.projectileGeneration || !PoseValid(pose) ||
        (pose.plantGeneration && pose.plantGeneration != umbrellaGeneration)) return false;
    const bool start = !pose.blocking;
    state = {projectileGeneration,umbrellaGeneration,boardTick,true,true};
    pose.plantGeneration = umbrellaGeneration;
    pose.blocking = true;
    startPose = start;
    return true;
}
FusionUmbrellaAdvance AdvanceFusionUmbrellaBlock(FusionUmbrellaBlockState& state, int boardTick,
    bool projectileAlive, bool umbrellaAlive, float& velocityX, float& velocityY)
{
    if (!FusionUmbrellaBlockStateValid(state) || boardTick < 0) return FusionUmbrellaAdvance::Invalid;
    if (!state.pending) return FusionUmbrellaAdvance::Unchanged;
    if (!projectileAlive || !umbrellaAlive)
    { state.pending = false; return FusionUmbrellaAdvance::Cancelled; }
    if (int64_t(boardTick)-state.blockedAtTick < kFusionUmbrellaDelayTicks) return FusionUmbrellaAdvance::Unchanged;
    const float x = velocityX*2.0f, y = -velocityY;
    if (!std::isfinite(velocityX) || !std::isfinite(velocityY) || !std::isfinite(x)) return FusionUmbrellaAdvance::Invalid;
    velocityX = x; velocityY = y;
    state.pending = false; // The harmless hit latch deliberately survives.
    return FusionUmbrellaAdvance::Deflected;
}
const FusionUmbrellaPoseEventSpec* GetFusionUmbrellaPoseEvents(std::size_t& count)
{ count = sizeof(events)/sizeof(events[0]); return events; }
bool TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent event, float animationSpeed, int& tick)
{
    if (!std::isfinite(animationSpeed) || animationSpeed < .9f || animationSpeed > 1.1f) return false;
    for (const auto& spec : events) if (spec.event == event)
    {
        tick = static_cast<int>(std::ceil(spec.clipSeconds/(static_cast<double>(1.2f)*animationSpeed)*100));
        return true;
    }
    return false;
}
bool ApplyFusionUmbrellaPoseEvent(FusionUmbrellaPoseState& pose, uint32_t plantGeneration,
    FusionUmbrellaPoseEvent event)
{
    if (!plantGeneration || pose.plantGeneration != plantGeneration || !PoseValid(pose)) return false;
    switch (event)
    {
    case FusionUmbrellaPoseEvent::BlockStart: pose.sortAtRoot = true; return true;
    case FusionUmbrellaPoseEvent::BlockOver: pose.sortAtRoot = false; return true;
    case FusionUmbrellaPoseEvent::AllBlockOver: pose.blocking = false; return true;
    default: return false;
    }
}
}
