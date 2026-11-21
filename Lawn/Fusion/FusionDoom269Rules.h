#ifndef LAWN_FUSION_DOOM269_RULES_H
#define LAWN_FUSION_DOOM269_RULES_H
#include "FusionShooterTiming.h"
#include <cstdint>

namespace LawnFusion
{
// Source bodies: work/fusion/batch40-269/assault-primary.json.
constexpr float kFusionIceDoom269Range=4.f;
constexpr float kFusionIceDoom269PulseDelay=.5f;
constexpr float kFusionIceDoom269PulseRadius=1.5f;
constexpr int kFusionIceDoom269FreezeLevel=5;
const FusionShooterTimingProfile* FindFusionDoom269Timing(uint16_t kind);
bool FusionDoom269LandTarget(int sourceStatus,bool controlled);
bool FusionSuperDoom269FearTarget(int sourceStatus,bool controlled);
int FusionIceDoom269PulseDelayTicks();
bool TryMakeFusionIceDoom269Deadline(int nativeTick,int& deadline);
bool FusionSuperDoom269StateValid(float interval,int ordinal,int power);
// Generic native self-transition plan. Inputs are existing Plant counters after
// shooting/lock decrement but before DoSpecial decrement. It never ticks them.
struct FusionSelfTransition269Counters
{int incoming=0,lock=0,outgoing=0;bool queued=false;};
struct FusionSelfTransition269Plan
{FusionSelfTransition269Counters counters{};bool begin=false,outgoingDue=false;};
bool TryPlanFusionSelfTransition269(const FusionSelfTransition269Counters&,bool trigger,
    bool tickAdvanced,int initialIncoming,int incomingEventCounter,int lockTicks,FusionSelfTransition269Plan&);

struct FusionSuperDoom269Shot
{
    float interval=0, recoverySeconds=0;
    int ordinal=0, damage=0;
    bool enhanced=false, overrideRecovery=false;
};
// Input ordinal counts shots since the last enhanced shot, rather than all
// lifetime shots. The native controller retains the authoritative clocks.
bool TryPrepareFusionSuperDoom269Shot(float interval,int ordinal,int damage,
    bool basicTravel3,bool advancedTravel2,FusionSuperDoom269Shot& result);

struct FusionIceDoom269Damage
{
    int power=0;
    bool absorbed=false, doom=false;
};
bool TryPrepareFusionIceDoom269Damage(int power,int damage,int damageType,
    bool expertTravel11,FusionIceDoom269Damage& result);
int FusionIceDoom269DeathDamage(bool expertTravel10);
}
#endif
