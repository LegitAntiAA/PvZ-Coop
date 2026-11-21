#ifndef LAWN_FUSION_FIRE_RULES_H
#define LAWN_FUSION_FIRE_RULES_H

#include "../../ConstEnums.h"
#include "FusionColdRules.h"
#include <cstdint>

namespace LawnFusion
{
enum class FusionTorchwoodAction { None, YellowPea, MeltSnow };
enum class FusionTorchConversionAction
{
    Yellow, Orange, Red, Cherry, Puff, Sun, ThreeYellow, Snow, RemoveHeat, ExtremeSnow
};
struct FusionTorchConversion
{
    FusionTorchConversionAction action = FusionTorchConversionAction::Yellow;
    uint16_t kind = 0;
    int damage = 0;
    int sunOnHit = 0;
    uint8_t fireState = 0;
    bool replace = false;
    bool stamp = true;
    bool smallProjectile = false;
    int emittedCount = 1;
    bool operator==(const FusionTorchConversion&) const = default;
};
// One verified source contact. The caller commits the returned plan only after
// native capacity/resource admission; invalid input preserves output.
bool TryResolveFusionTorchConversion(uint16_t torchKind, uint16_t kind, int damage,
    int sunOnHit, uint8_t fireState, uint8_t specialType, bool sameRow,
    bool zombieBullet, ProjectileMotion motion, uint16_t lastTorchId,
    uint32_t lastTorchToken, uint16_t torchId, uint32_t torchToken,
    FusionTorchConversion& output,bool smallProjectile = false);
bool FusionFireProjectileMetadataValid(uint16_t kind, int damage, int sunOnHit, uint8_t fireState,
    uint16_t lastTorchId, uint32_t lastTorchToken, ProjectileType nativeType,bool smallProjectile = false);
FusionTorchwoodAction ResolveFusionTorchwoodAction(uint16_t kind, uint8_t fireState, bool sameRow,
    bool zombieBullet, ProjectileMotion motion, uint16_t lastTorchId, uint32_t lastTorchToken,
    uint16_t torchId, uint32_t torchToken, uint8_t specialType = 0);
bool TryGetFusionYellowFireDamage(int damage, uint8_t fireState, int& result);
bool TryGetFusionFireSplashDamage(int damage, int targetCount, int& result);
bool TryGetFusionFireSplashDamage(uint16_t projectileKind, int damage, int targetCount, int& result);
bool FusionFireAllowsSputter(int sourceStatus, bool blockedSecondArmor);
struct FusionFireRect { double x,y,width,height; };
bool FusionFireAreaTouchesRect(double centerX, double centerY, const FusionFireRect& rect);
bool TryWarmFusionColdState(const FusionColdState& before, FusionColdState& after);
}
#endif
