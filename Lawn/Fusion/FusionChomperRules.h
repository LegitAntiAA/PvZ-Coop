#ifndef LAWN_FUSION_CHOMPER_RULES_H
#define LAWN_FUSION_CHOMPER_RULES_H

#include "FusionBasePlantRules.h"
#include "FusionDeathRules.h"
#include <cstdint>

namespace LawnFusion
{
struct FusionChomperProfile
{
    uint16_t kind;
    int maxHealth, digestTicks;
    double biteSeconds, chewSeconds, biteDuration;
    double swallowDuration, swallowStateSpeed, chewDuration, idleStateSpeed;
    int recovery, sunDelayTicks, sunCount;
    bool cherryBite;
    const char* digestVisualState = "chew";
    const char* swallowVisualState = "swallow";
    const char* shootingVisualState = nullptr;
};

const FusionChomperProfile* FindFusionChomperProfile(uint16_t kind);
// 100 Hz native phase adaptation. Kind5 retains its accepted decimal-double
// timings (notably ToChew85 at speed1); Sun/Cherry share those phases. Nut's
// distinct event/clip values are the promoted source float32 values.
bool TryGetFusionChomperTiming(uint16_t kind, float speed, FusionChomperTiming& result);
bool TryGetFusionChomperChewTicks(uint16_t kind, float speed, int& ticks);
bool TryRecoverFusionChomper(uint16_t kind, int health, int maxHealth, int& recoveredHealth);
bool TryAdvanceFusionChomperDamageStage(uint16_t kind, int health,
    uint8_t previousStage, uint8_t& stage);
// The actual native mLaunchCounter is reused only during Sun1014 SWALLOWING.
// Consume before attempting the whole Coin batch: capacity failure must not
// retry or duplicate a reward. Removal/leaving the phase cancels the callback.
bool AdvanceFusionChomperSunReward(uint16_t kind, bool callbackActive,
    int& remainingTicks, int& coinCount);
// Source CherryChomper root+(1.5,.5), relative to its actual Shadow(+1.58,+.34).
// Supplying a native shadow is the explicit native-art anchor adaptation.
bool TryMakeFusionChomperBlast(uint16_t kind, double shadowX, double shadowY,
    int row, NativeBlastSpec& spec);
}
#endif
