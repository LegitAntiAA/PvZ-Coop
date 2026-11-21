#ifndef LAWN_FUSION_ROLLING_ACTORS269_RULES_H
#define LAWN_FUSION_ROLLING_ACTORS269_RULES_H
#include <cstdint>
#include <span>
namespace LawnFusion
{
struct FusionRollingActor269Profile
{uint16_t kind;int damage,maxHealth;float speed,colliderWidth,colliderHeight,shadowY;};
const FusionRollingActor269Profile* FindFusionRollingActor269Profile(uint16_t kind);
uint16_t FusionRollingActor269PhysicalKind(uint16_t factoryKind);
bool FusionRollingActor269Contact(int sourceRow,int targetRow,int status,bool controlled);
bool FusionRollingActor269Enter(int sourceRow,int targetRow,int status,bool controlled,int zombieKind);
bool FusionRollingActor269Touches(float x,float y,float left,float bottom,float width,float height);
bool FusionRollingActor269Expired(float shadowX,float boardMaxX);
int FusionRollingActor269DamageSum(int before,int damage);
// Source Wheat.Start sets30 only in WheatProtection/SuperRandom; ordinary0.
// Actual nonnull source prefab slots minus Board.WheatLimit; never native-filter this uniform pool.
std::span<const uint16_t> FusionEve269Candidates();
bool FusionEve269Candidate(uint16_t kind);
float FusionWheat269InitialTime(bool specialMode);
bool TryStepFusionWheat269(float before,float delta,bool enemyPresent,bool animatorActive,float& after,bool& transform);
}
#endif
