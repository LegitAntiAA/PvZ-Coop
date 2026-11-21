#ifndef LAWN_FUSION_UMBRELLA_RULES_H
#define LAWN_FUSION_UMBRELLA_RULES_H

#include <cstddef>
#include <cstdint>

namespace LawnFusion
{
constexpr int kFusionUmbrellaDelayTicks = 5; // ceil(float32(.02f*2.5f)*100).
struct FusionUmbrellaBlockState
{
    uint32_t projectileGeneration = 0;
    uint32_t umbrellaGeneration = 0;
    int blockedAtTick = -1;
    bool harmless = false;
    bool pending = false;
};
struct FusionUmbrellaPoseState
{
    uint32_t plantGeneration = 0;
    bool blocking = false;
    bool sortAtRoot = false;
};
enum class FusionUmbrellaPoseEvent { BlockStart, BlockOver, AllBlockOver };
struct FusionUmbrellaPoseEventSpec { FusionUmbrellaPoseEvent event; double clipSeconds; };
enum class FusionUmbrellaAdvance { Unchanged, Deflected, Cancelled, Invalid };

bool FusionUmbrellaCanIntercept(int sourceKind, int sourceWay, bool zombieBullet, bool hitLatched,
    int bulletRow, uint16_t attackedKind, int attackedColumn, int attackedRow,
    int umbrellaColumn, int umbrellaRow, bool awake, float sourceAnimatorSpeed);
bool FusionUmbrellaBlockStateValid(const FusionUmbrellaBlockState& state);
// State belongs to one complete native generation. Recycled slots require a
// fresh caller-owned state; neither function acquires actors or advances time.
bool TryBeginFusionUmbrellaBlock(uint32_t projectileGeneration, uint32_t umbrellaGeneration,
    int boardTick, FusionUmbrellaBlockState& state, FusionUmbrellaPoseState& pose, bool& startPose);
FusionUmbrellaAdvance AdvanceFusionUmbrellaBlock(FusionUmbrellaBlockState& state, int boardTick,
    bool projectileAlive, bool umbrellaAlive, float& velocityX, float& velocityY);
const FusionUmbrellaPoseEventSpec* GetFusionUmbrellaPoseEvents(std::size_t& count);
bool TryGetFusionUmbrellaPoseEventTick(FusionUmbrellaPoseEvent event, float animationSpeed, int& tick);
bool ApplyFusionUmbrellaPoseEvent(FusionUmbrellaPoseState& pose, uint32_t plantGeneration,
    FusionUmbrellaPoseEvent event);
}

#endif
