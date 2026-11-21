#ifndef LAWN_FUSION_CHOMPER268_RULES_H
#define LAWN_FUSION_CHOMPER268_RULES_H
#include <cstdint>
namespace LawnFusion
{
// Normal Fusion branch. Travel overrides remain a separate source dependency.
struct FusionStrongChomperProfile
{
    uint16_t kind;
    int baseHealth, attackDamage;
};
struct FusionStrongChomperBite
{
    int sourceDamage = 0, recovery = 0, rechargeTicks = 0;
    uint16_t nextShotKind = 0;
};
const FusionStrongChomperProfile* FindFusionStrongChomperProfile(uint16_t kind);
// randomShot is drawn only for the heavy SuperChomper branch, in [4,6).
bool TryPlanFusionStrongChomperBite(uint16_t kind, float bodyHealth,
    bool firstArmorObject, int firstArmorHealth, bool charged, int randomShot,
    FusionStrongChomperBite& result);
bool TryGetFusionStrongChomperShot(uint16_t kind, uint16_t storedShot,
    uint16_t& projectile, uint8_t& movingWay, int& damage);
bool TryCapFusionStrongChomperDamage(uint16_t kind, int incoming, int& damage);
bool TryDecayFusionStrongChomperHealth(uint16_t kind, int health, int& result);
bool TryCapFusionStrongChomperHealth(uint16_t kind, int health, int& result);
bool TryAdvanceFusionStrongChomperStage(uint16_t kind, int health,
    uint8_t previous, uint8_t& stage);
bool FusionStrongChomperTarget(int status,int rowDelta,bool controlled,bool beforeDying,bool colliderEnabled);
bool TryGetFusionStrongChomperBiteTicks(float speed,int& ticks);
bool FusionStrongChomperEventDue(float speed,int remaining,bool shoot);
bool FusionChomper268StateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode,uint8_t damageStage);
bool FusionChomper268ClockStateValid(uint16_t kind,int chargeTicks,int decayTicks,float speed,float shootSeconds,uint8_t mode);
}
#endif
