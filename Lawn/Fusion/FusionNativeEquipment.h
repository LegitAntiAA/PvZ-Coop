#ifndef LAWN_FUSION_NATIVE_EQUIPMENT_H
#define LAWN_FUSION_NATIVE_EQUIPMENT_H
#include "FusionEquipmentRules.h"
#include <cmath>
#include <cstdint>
#include <array>
class Board;
class Coin;
class Plant;
namespace LawnFusion
{
enum class NativeEquipmentPhase : uint8_t {Falling,World,Pulling,Attached,Cursor};
// Metadata extends one real Board.Coin. Coin owns position, velocity, age and
// the disappearance counter; clients adopt the host, never run item RNG.
struct NativeEquipmentData
{
    FusionEquipmentType type=FusionEquipmentType::None;
    FusionEquipmentType factoryType=FusionEquipmentType::None;
    NativeEquipmentPhase phase=NativeEquipmentPhase::Falling;
    uint32_t instanceToken=0;
    uint16_t sourcePlantId=0;
    uint32_t sourcePlantToken=0;
    uint8_t claimSlot=255;
    // pullStart is a Board-space start during Pulling, then a parent-local
    // point in Attached/World-with-source. Cursor preserves it until PutDown.
    float anchorX=0,anchorY=0,pullStartX=0,pullStartY=0,pullEndX=0,pullEndY=0;
    float gravity=0;
    int phaseStartAge=0;
    bool disappear=true,flashing=false;
    bool weapon=false; // Source CherryMagnet removes the Bucket component and launches this same native Coin.
    std::array<float,4> attachmentBasis{1,0,0,1};
};
inline bool NativeEquipmentDataValid(const NativeEquipmentData& d)
{
    if(!FusionEquipmentTypeValid(int(d.type))||!FusionEquipmentTypeValid(int(d.factoryType))||
        (d.type!=d.factoryType&&!(d.factoryType==FusionEquipmentType::Door&&d.type==FusionEquipmentType::Bucket))||
        uint8_t(d.phase)>uint8_t(NativeEquipmentPhase::Cursor)||!d.instanceToken||
        ((d.sourcePlantId==0)!=(d.sourcePlantToken==0))||d.phaseStartAge<0||d.gravity!=(d.weapon?0.f:9.8f))return false;
    if(d.phase==NativeEquipmentPhase::Cursor?d.claimSlot>=4:d.claimSlot!=255)return false;
    if(d.weapon&&(d.phase!=NativeEquipmentPhase::Attached||d.disappear||d.claimSlot!=255))return false;
    if((d.phase==NativeEquipmentPhase::Pulling||d.phase==NativeEquipmentPhase::Attached)&&!d.sourcePlantId)return false;
    const float coordinates[]={d.anchorX,d.anchorY,d.pullStartX,d.pullStartY,d.pullEndX,d.pullEndY};
    for(float value:coordinates)if(!std::isfinite(value)||std::abs(value)>4096)return false;
    for(float value:d.attachmentBasis)if(!std::isfinite(value)||std::abs(value)>4096)return false;
    const double determinant=double(d.attachmentBasis[0])*d.attachmentBasis[3]-double(d.attachmentBasis[1])*d.attachmentBasis[2];
    if(std::abs(determinant)<=1e-12)return false;
    return true;
}
NativeEquipmentData* GetNativeEquipmentData(Coin* coin);
const NativeEquipmentData* GetNativeEquipmentData(const Coin* coin);
Coin* ResolveNativeEquipment(Board* board,uint16_t id,uint32_t token);
Coin* GetNativeEquipmentHeldBy(const Plant* source);
// All API XY values are Board-space visual centres (Coin.mPosX/Y +20),
// matching native payout/projectile adapters; never raw Coin body origins.
// Capacity, actual source package and full identities are checked before
// allocation. Null means no item and no source Zombie mutation is permitted.
bool CanCreateNativeEquipment(Board* board,FusionEquipmentType type);
bool CanCreateNativeEquipmentVisual(Board* board,FusionEquipmentType factoryType);
void ClearNativeEquipmentLease(Board* board,uint16_t id,uint32_t token);
Coin* CreateNativeEquipment(Board* board,FusionEquipmentType type,float x,float y,bool disappear=true);
Coin* CreateNativeEquipmentForCapture(Plant* source,FusionEquipmentType type,
    float startX,float startY,float endX,float endY);
bool BeginNativeEquipmentPull(Coin* coin,Plant* source,float endX,float endY);
// Source PutDown uses the stored anchor, independent of current cursor XY.
bool PutDownNativeEquipment(Coin* coin);
bool DropNativeEquipmentFromPlant(Coin* coin);
// Receiver consumption is Destroy(item), distinct from Bucket.Die expiry and
// its MachineNut heal callback. Both operations are single-commit.
bool ConsumeNativeEquipment(Coin* coin);
void RetireNativeEquipment(Coin* coin,bool sourceDieCallback);
// Source SetPlant(Default): prepare/bind the fresh actor before retiring old.
// Root owns co-op provenance; a Bucket upgrade is not a fusion recipe.
Plant* TryReplaceNativeEquipmentReceiver(Plant* old,uint16_t kind);
bool TryGetNativeEquipmentHoldAnchor(const Plant* plant,float& x,float& y);
bool TryNativeEquipmentGraphSink(Plant* plant,FusionEquipmentType type,Coin* item);
// Root integration hooks: typed equipment must never reach ScoreCoin,
// sun/money auto-collect or ordinary Coin input/presentation.
bool UpdateNativeEquipmentCoin(Coin* coin);
bool IsNativeEquipmentCoin(const Coin* coin);
}
#endif
