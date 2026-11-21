#ifndef LAWN_FUSION_BATCH_ACTION_RULES_H
#define LAWN_FUSION_BATCH_ACTION_RULES_H

#include "FusionShooterTiming.h"
#include <array>
#include <cstdint>

namespace LawnFusion
{
enum class FusionBatchActionFamily { Hypno, Doom, Jalapeno, Squash, Caltrop, Scaredy, AdaptiveFume, Threepeater, Contact, ManualFume, ForwardFume, Gloom };
struct FusionBatchActionProfile
{
    uint16_t kind;
    FusionBatchActionFamily family;
    int damage;
    bool fireAppearance;
    double muzzleXFromShadow = 0;
    double muzzleYFromShadow = 0;
};
const FusionBatchActionProfile* FindFusionBatchActionProfile(uint16_t kind);
bool FusionBatchPlantStateValid(uint16_t kind,uint32_t shotCount,uint16_t rageTicks,uint16_t bagCount,uint8_t mode);
struct FusionScaredyFearTiming { int lowering=0, event=0, raising=0; };
bool TryGetFusionScaredyFearTiming(uint16_t kind,float animationSpeed,FusionScaredyFearTiming& result);
struct FusionCaltropStrikeProfile
{
    uint16_t kind;
    int damage;
    int coldLevelsAfterDamage;
    bool warmAfterDamage;
    bool rock;
};
// Source caltrop and rock strikes share the native area-damage path.
const FusionCaltropStrikeProfile* FindFusionCaltropStrikeProfile(uint16_t kind);
unsigned int FusionBatchDamageFlags(uint16_t kind);
const FusionShooterTimingProfile* FindFusionBatchTimingProfile(uint16_t kind, bool closeRange = false);
bool TryGetFusionBatchInstantTicks(uint16_t kind, float animationSpeed, int& ticks);
bool TryGetFusionCaltropCycleTicks(float jitterSeconds, int& ticks);
bool TryGetFusionCaltropCycleTicks(uint16_t kind,float jitterSeconds,int& ticks);
bool TryGetFusionScaredyDoomInterval(uint32_t successfulShots,float& interval);
bool TryAdvanceFusionScaredyDoom(uint32_t before,bool scared,uint32_t& after,bool& explode);
bool TryAdvanceFusionPuffColors(uint8_t before, uint8_t color, uint8_t& after, bool& hypnotize);
bool TryGetFusionBatchEntryRows(uint16_t kind, int row, int rows, std::array<int,3>& result, size_t& count);
// Source rows belong to the requested lane from creation, while the real
// projectile travels diagonally out of the middle head for its first.5s.
constexpr uint8_t kFusionThreeUpper = 3;
constexpr uint8_t kFusionThreeLower = 4;
bool FusionBatchProjectileMetadataValid(uint16_t kind, int damage, int sunOnHit,
    uint8_t specialType, uint8_t effectVariant, int nativeMotion);
bool TryGetFusionThreeVerticalStep(uint8_t specialType, int ageTicks, float& pixels);
bool FusionBatchShooterCanAcquire(uint16_t kind, int sourceStatus, int rowDelta,
    bool mindControlled, bool beforeDying, double deltaX, double visionDeltaX);
struct FusionBatchArea { double x = 0, y = 0, width = 0, height = 0; };
bool TryMakeFusionBatchArea(uint16_t kind, double shadowX, double shadowY, bool attack,
    FusionBatchArea& result);
bool FusionBatchAreaTouchesRect(const FusionBatchArea& area, double x, double y, double width, double height);
bool FusionDoomTouchesRect(double centerX, double centerY, double x, double y, double width, double height);
bool FusionBatchCircleTouchesRect(double centerX,double centerY,double radius,
    double x,double y,double width,double height);
bool TryMakeFusionDoomPuffTrigger(size_t collider,double shadowX,double shadowY,FusionBatchArea& result);
}
#endif
