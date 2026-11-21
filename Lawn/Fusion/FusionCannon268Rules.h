#ifndef LAWN_FUSION_CANNON_268_RULES_H
#define LAWN_FUSION_CANNON_268_RULES_H
#include "FusionShooterTiming.h"
#include "FusionLobberRules.h"
#include <cstdint>
namespace LawnFusion
{
enum class FusionCannon268Phase : uint8_t { Cooldown, Charging, Ready, Firing };
struct FusionCannon268Profile
{
    uint16_t kind, projectileKind;
    int health, damage, sunCost;
    float rechargeSeconds, attackIntervalSeconds;
    bool manual;
};
const FusionCannon268Profile* FindFusionCannon268Profile(uint16_t kind);
const FusionShooterTimingProfile* FindFusionCannon268Timing(uint16_t kind,bool charge=false);
// Manual/independent controllers retain their entire source clip on one native counter.
bool TryGetFusionCannon268ShotCounter(const FusionShooterTimingProfile&,float speed,int& counter);
bool TryGetFusionCannon268EventAtCounter(const FusionShooterTimingProfile&,float speed,int counter,size_t event,bool& due);
bool FusionCannon268ProjectileMetadataValid(uint16_t kind,int damage,int sunOnHit,uint8_t way,uint8_t variant,bool enhanced,ProjectileType type,ProjectileMotion motion);
bool FusionCannon268StateValid(uint16_t kind,uint8_t mode,int remaining,int pending,uint16_t rage,uint16_t bag);
const FusionLobberProfile* FindFusionCannon268LobberProfile(uint16_t kind,bool alternate=false);
const FusionLobberProfile* FindFusionCannon268ProjectileProfile(uint16_t kind);
const FusionLobberProjectileTraits* FindFusionCannon268ProjectileTraits(uint16_t kind);
const FusionShooterTimingProfile* FindFusionCannon268LobberTiming(uint16_t kind,const char* state="shoot");
const FusionShooterTimingProfile* FindFusionCannon268SubTiming();
int FusionCannon268SplitCount(uint16_t kind,uint8_t status);
// Source effect2 freezes then damages every hostile current body+first armor.
bool TryGetFusionCannon268DoomDamage(int base,float body,int firstArmor,int& damage);
bool IsFusionCannon268Way14(uint16_t kind);
constexpr float kFusionCannon268UpRotation=1.5707964f;
constexpr float kFusionCannon268DownRotation=-1.5707964f;
bool FusionCannon268RotationValid(float rotation);
double FusionCannon268LandingProbe(uint16_t projectileKind);
// Called before the existing physics step: source FixedUpdate selects velocity
// and tests its head Transform before Unity integrates the Rigidbody.
bool TryStepFusionCannon268(uint16_t kind,int ageTicks,int rows,double pixelsY,
    float targetX,float targetY,float& rootX,float& rootY,float& velocityY,float& rotation,bool& hitLand);
bool FusionCannon268AreaTouches(double centerX,double centerY,double x,double y,double width,double height,double pixelsY);
}
#endif
