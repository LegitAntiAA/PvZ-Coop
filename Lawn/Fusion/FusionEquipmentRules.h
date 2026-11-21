#ifndef LAWN_FUSION_EQUIPMENT_RULES_H
#define LAWN_FUSION_EQUIPMENT_RULES_H
#include <cstddef>
#include <cstdint>
#include <vector>
namespace LawnFusion
{
enum class FusionEquipmentType : uint8_t
{Bucket,Helmet,Jackbox,Pickaxe,Machine,SuperMachine,Jumper,Ladder,IronHead,RedIronHead,Door,None=255};
enum class FusionEquipmentFamily : uint8_t {Capture,Pumpkin,Defender,Producer};
struct FusionEquipmentProfile
{
    uint16_t kind;FusionEquipmentFamily family;
    int maximumHealth,attackDamage,sunCost,rechargeTicks;
    float initialAttributeSeconds,attributeSeconds;
};
struct FusionMagnetCaptureProfile
{float radius=4,cooldownSeconds=15,initialSeconds=15;int rowRange=2;bool wholeWorld=false,waterOnly=false,dropHeldOnDeath=true,lightBurst=false;};
struct FusionBucketReceiverEffect
{uint16_t replacementKind=65535;int recoverHealth=0,sunValue=0;};
enum class FusionEquipmentCaptureCommit : uint8_t {Helmet,Door,Ladder,Jackbox,Pogo,Pickaxe,IronHead,MachineObject};
struct FusionEquipmentCapturePlan
{FusionEquipmentType type=FusionEquipmentType::None;FusionEquipmentCaptureCommit commit=FusionEquipmentCaptureCommit::Helmet;bool preserveReturnedBucket=false;};
struct FusionMagnetGraphNode {uint16_t kind;int column,row;bool live=true;};
bool FusionEquipmentTypeValid(int type);
const FusionEquipmentProfile* FindFusionEquipmentProfile(uint16_t kind);
std::size_t FusionEquipmentProfileCount();
const FusionEquipmentProfile* FusionEquipmentProfileAt(std::size_t index);
// Source light0 changes radius/CD, retaining the previous rowRange.
bool TryGetFusionMagnetCaptureProfile(uint16_t kind,int light,bool seaGrown,int previousRowRange,FusionMagnetCaptureProfile& out);
bool FusionEquipmentMagnetMember(uint16_t kind);
bool FusionEquipmentGraphAdjacent(int columnA,int rowA,int columnB,int rowB);
bool TryGetFusionMagnetComponentSizes(const FusionMagnetGraphNode* nodes,std::size_t count,std::vector<uint32_t>& sizes);
// Slots0/1/2 are first/second/unique armor. Controller retains source search order.
bool TryGetFusionEquipmentArmorCapture(int sourceSlot,int sourceType,FusionEquipmentCapturePlan& out);
bool TryGetFusionEquipmentPrefabIndex(int type,int& index);
bool FusionMagnetPumpkinMetalTarget(int firstArmor,int secondArmor,int uniqueArmor,int sourceZombieKind);
// IMUL -> float32 -> MULSS speed -> CVTTSS2SI -> integer Water*2.
bool TryGetFusionMagnetPumpkinDamage(int baseDamage,int magnetCount,float sourceSpeed,bool water,int& damage);
bool TryGetFusionBucketReceiverEffect(uint16_t kind,int type,int health,int maximum,FusionBucketReceiverEffect& out);
bool TryGetFusionBucketDeathRecovery(int type,int superMachineCount,int machineCount,uint16_t& receiverKind,int& amount);
bool TryGetFusionBucketLifetime(float seconds,bool disappear,bool tutor,bool cursorHeld,bool alreadyFlashing,bool& startFlash,bool& expire);
// RNG belongs to the caller; supplied draw is Random.Range(0,10).
bool TryGetFusionSunMagnetWeightedItem(int draw,int& itemKind,FusionEquipmentType& type);
bool TryGetFusionSunMagnetPeriodicSeconds(int integerJitter,float& seconds);
constexpr float kFusionEquipmentPullSeconds=.3f;
constexpr float kFusionSunMagnetInitialAttributeSeconds=10;
constexpr float kFusionSunMagnetAttributeSeconds=30;
constexpr float kFusionSunMagnetCallbackSeconds=.5f;
constexpr float kFusionSunMagnetPeriodicInitialMinimum=4;
constexpr float kFusionSunMagnetPeriodicInitialMaximum=7;
}
#endif
