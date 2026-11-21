#ifndef LAWN_FUSION_EXTRAS268_RULES_H
#define LAWN_FUSION_EXTRAS268_RULES_H
#include "FusionDefenderRules.h"
#include "FusionEquipmentRules.h"
#include <cstdint>
namespace LawnFusion
{
constexpr int kFusionMachine268BaseHealth=4000,kFusionMachine268MaximumHealth=16000;
bool IsFusionExtras268Kind(uint16_t kind);
const FusionDefenderProfile* FindFusionExtras268DefenderProfile(uint16_t kind);
bool FusionExtras268SnapshotValid(uint16_t kind,int health,int maximum,uint8_t stage,uint8_t mode);
bool FusionExtras268PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,
    uint8_t mode,uint8_t stage,int productionTicks,int pendingTicks,float shootSeconds);
bool TryGetFusionMachine268Damage(int incoming,int& applied);
bool TryGetFusionMachine268ProtectionDamage(int incoming,int& applied);
bool TryGetFusionMachine268Recovery(int health,int amount,int& after);
bool TryGetFusionMachine268Transition(uint8_t before,uint8_t after,int& recovery);
bool TryGetFusionExtras268BucketEffect(uint16_t kind,FusionEquipmentType type,int health,FusionBucketReceiverEffect& effect);
// Actual child paths, indexed by WallNut health stage and machineType0/1/2.
const char* FusionMachine268FormNode(uint8_t stage,uint8_t form);
}
#endif
