#ifndef LAWN_FUSION_SHOOTER_BATCH_RULES_H
#define LAWN_FUSION_SHOOTER_BATCH_RULES_H

#include "FusionShooterTiming.h"
#include "FusionRecipientShooterRules.h"
#include <cstdint>

namespace LawnFusion
{
// The native counters own all timelines. These fields are source sequence/state
// identities, except rageTicks which is only a snapshot of mStateCountdown.
bool FusionShooterBatchStateValid(uint16_t kind, uint32_t shotCount,
    uint16_t rageTicks, uint16_t bagCount, uint8_t mode);
bool IsFusionShooterBatchKind(uint16_t kind);
const FusionShooterTimingProfile* FindFusionShooterBatchTimingProfile(uint16_t kind);
const FusionShooterTimingProfile* FindFusionShooterBatchTimingProfile(uint16_t kind, const char* state);
const FusionShooterTimingProfile* FindFusionBigGatlingTimingProfile(uint32_t callbacks, bool loop);
float FusionShooterBatchMaximumDuration(uint16_t kind,float animationSpeed);
bool TryGetFusionBigGatlingPhaseTicks(uint8_t mode,float animationSpeed,uint32_t callbacks,int& ticks);
bool TryRephaseFusionBigGatling(uint8_t mode,float animationSpeed,uint32_t callbacks,
    std::size_t eventIndex,int& shootingCounter,int& stateCountdown,float& elapsedSeconds);
bool TryGetFusionDoomCactusAngle(float sourceElapsedSeconds,float& radians);
bool TryGetFusionShooterBatchCycleTicks(uint16_t kind, float jitter, int& ticks);
// Source Sniper increments only after a successful target check; every sixth
// actual hit is1,000,000. The stored sequence is the equivalent residue0..5.
bool TryAdvanceFusionSniperShot(uint16_t kind, uint32_t previous, uint32_t& next, int& damage);
// Source bag contains remainingFalse false entries followed by one true entry.
// The caller supplies the source RangeInt(0, remainingFalse+1) draw.
bool TryDrawFusionDoomCactusBag(uint16_t remainingFalse, int draw,
    uint16_t& nextRemainingFalse, bool& superSkill);
bool TryGetFusionGatlingMilestone(uint32_t successfulCallbacks, float& speed, float& rate);
bool FusionShooterBatchInvulnerable(uint16_t kind, int remainingRageTicks);
bool FusionSniperCanAcquire(int status, bool controlled, bool dying, double plantX,
    double zombieX, double visionX);
bool TryGetFusionIronDamage(int baseDamage, int secondArmorHealth, int& damage);
bool FusionShooterBatchProjectileMetadataValid(uint16_t kind, int damage, int sun,
    uint8_t sourceWay, ProjectileType type, ProjectileMotion motion);
bool TryMakeFusionShooterBatchProjectileRect(uint16_t kind, double centerX, double centerY,
    FusionRecipientRect& rect);
bool TryMakeFusionShooterBatchRotatedRect(uint16_t kind,double centerX,double centerY,
    double sourceRadians,FusionRecipientRect& rect);
bool FusionShooterBatchRotatedProjectileTouchesRect(uint16_t kind,double centerX,double centerY,
    double sourceRadians,const FusionRecipientRect& target);
}
#endif
