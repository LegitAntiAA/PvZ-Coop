#ifndef LAWN_FUSION_LOBBER_RULES_H
#define LAWN_FUSION_LOBBER_RULES_H
#include "../../ConstEnums.h"
#include <cstdint>
namespace LawnFusion
{
struct FusionShooterTimingProfile;
constexpr double kFusionLobberPixelsX = 72.0 / 1.35;
constexpr double kFusionLobberPixelsY = 100.0 / 1.67;
constexpr double kFusionLobberFlightSeconds = 1.5;
constexpr double kFusionLobberGravity = 14.715;
struct FusionLobberProfile
{
    uint16_t kind, projectileKind;
    ProjectileType nativeType;
    int damage;
    double muzzleX, muzzleY;
    bool alternate = false;
};
const FusionLobberProfile* FindFusionLobberProfile(uint16_t kind);
const FusionLobberProfile* FindFusionLobberProfile(uint16_t kind, bool alternate);
const FusionLobberProfile* FindFusionLobberProjectileProfile(uint16_t kind);
const FusionShooterTimingProfile* FindFusionExtendedLobberTimingProfile(uint16_t kind, const char* state = "shoot");
enum class FusionLobberBounce { None, HalfFirstTwo, SunCabbage, Half };
struct FusionLobberProjectileTraits
{
    uint16_t kind;
    int fixedPierceLimit; //0 snapshots clamp(wallet/200+1,2,6) at emission.
    FusionLobberBounce bounce;
    bool splash, cold, butter, sunReward, sourceRotation;
    double boxWidth, boxHeight, boxOffsetX, boxOffsetY; // Source root box before uniform scale .4.
    uint16_t moneyItem = 0; // Source CoinMoney item, emitted per hit/ground before damage.
};
const FusionLobberProjectileTraits* FindFusionLobberProjectileTraits(uint16_t kind);
int FusionLobberCornDenominator(uint16_t kind);
bool TryChooseFusionLobberAlternate(uint16_t kind, int sourceDraw, bool& alternate);
bool TryGetFusionLobberPierceLimit(uint16_t projectileKind, int wallet, int& limit);
bool TryApplyFusionLobberBounce(uint16_t projectileKind, int admittedHits, float& velocityX, float& downwardVelocity);
bool FusionLobberProjectileMetadataValid(uint16_t kind, int damage, int sunOnHit, ProjectileType nativeType);
bool TryGetFusionLobberCycleTicks(uint16_t kind, float additiveJitter, int& ticks, float liveInterval = 0);
struct FusionLobberPoint { double x = 0, y = 0; };
struct FusionLobberRect { double x = 0, y = 0, width = 0, height = 0; };
struct FusionLobberLaunch
{
    FusionLobberPoint spawn;
    double velocityX = 0, velocityY = 0, accelerationY = 0; // Native pixels/tick, pixels/tick².
};
// Inputs are native Board coordinates (Y down). Solve from the live original
// muzzle, then apply the source near-spawn override without recomputing velocity.
bool TryMakeFusionLobberLaunch(const FusionLobberPoint& muzzle, const FusionLobberPoint& sample,
    const FusionLobberPoint& current, double elapsedSeconds, const FusionLobberRect* currentCollider,
    FusionLobberLaunch& launch,double pixelsY = kFusionLobberPixelsY,double flightSeconds=kFusionLobberFlightSeconds);
// Same canonical solver with the actual native target velocity (Board pixels/s).
bool TryMakeFusionLobberVelocityLaunch(const FusionLobberPoint& muzzle,const FusionLobberPoint& current,
    const FusionLobberPoint& velocity,double flightSeconds,const FusionLobberRect* currentCollider,
    FusionLobberLaunch& launch,double pixelsY=kFusionLobberPixelsY);
bool AdvanceFusionLobberMotion(float velocityX, float accelerationY, float& x, float& height, float& velocityY);
bool FusionLobberGroundHit(float heightFromShadow, float velocityY,double pixelsY = kFusionLobberPixelsY);
bool FusionLobberExpired(double sourceX, int ageTicks, double minimumX, double maximumX);
bool FusionLobberCanAcquire(int sourceStatus, bool sameRow, bool mindControlled, bool beforeDying,
    double plantShadowX, double zombieShadowX, double visionMaximum);
bool FusionLobberSplashCanAffect(int sourceStatus, int rowDifference, bool mindControlled);
bool TryMakeFusionLobberRect(uint16_t kind, double centerX, double centerY, FusionLobberRect& rect,double pixelsY = kFusionLobberPixelsY);
// Source Melon.OverlapCircleAll uses rootY-.5, independent of its collider/FX.
FusionLobberPoint FusionLobberSplashCenter(double rootX, double rootY,double pixelsY = kFusionLobberPixelsY);
bool FusionLobberAreaTouchesRect(double centerX, double centerY, const FusionLobberRect& rect,double pixelsY = kFusionLobberPixelsY);
bool TryGetFusionLobberSplashDamage(int baseDamage, int targetCount, int& damage, bool enhanced = false);
bool TryGetFusionLobberColdDamage(int baseDamage, bool frozen, int& damage);
bool TryGetFusionPoisonLobberDamage(uint16_t kind,int baseDamage,int poisonLevel,bool secondary,int& damage);
}
#endif
