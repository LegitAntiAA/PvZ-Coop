#ifndef LAWN_FUSION_FIRE_ACTION_BATCH_RULES_H
#define LAWN_FUSION_FIRE_ACTION_BATCH_RULES_H
#include <cstdint>

namespace LawnFusion
{
enum class FusionFireActionFamily { Light, Blover, Torch, FireGround, Caltrop, Obsidian, CherryRunner, BloverShell };
struct FusionFireActionProfile
{
    uint16_t kind;
    FusionFireActionFamily family;
    int maximumHealth;
    bool enabled;
};
const FusionFireActionProfile* FindFusionFireActionProfile(uint16_t kind);
bool IsFusionFireActionSupported(uint16_t kind);
bool FusionFireActionInvincible(uint16_t kind);
bool FusionFireTorchKind(uint16_t kind);
bool FusionFireActionProjectileMetadataValid(uint16_t kind,int damage,int sunOnHit,
    uint8_t specialType,uint8_t fireState,int nativeType,int nativeMotion);
float FusionFireActionMaximumDuration(uint16_t kind,float speed);
// Integer native100Hz quantization of source events, using the promoted source
// F32 key and actual Speed parameter. Output is unchanged on invalid input.
bool TryGetFusionFireActionTicks(double sourceSeconds, float speed, int& ticks);
// Conversion count increments even if the51st Squash world placement fails;
// Squash resets on success, whereas TorchSpike resets before its101st attempt.
bool TryAdvanceFusionTorchCount(uint16_t kind, int before, bool spawnSucceeded,
    int& after, bool& attemptSpawn);
bool FusionFireLightCellAffected(int lightColumn, int lightRow, int column, int row);
bool TryGetFusionFireKnockBack(double axisX, double sourceRightBoundary,
    double distance, double& result);
bool FusionFireCircleTouchesRect(double centerX, double centerY, double sourceRadius,
    double x, double y, double width, double height);
int FusionFireCherryNutHealOverlapCount(double centerX,double centerY,int blastRow,
    double cellCenterX,double cellCenterY,int plantRow,double sourceRadius);
bool TryStepFusionSquashProjectile(float positionZ, float velocityZ, float acceleration,
    float groundZ, float& nextZ, float& nextVelocity, bool& bounced);
}
#endif
