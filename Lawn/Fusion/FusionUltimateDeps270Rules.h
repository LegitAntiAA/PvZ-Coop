#ifndef LAWN_FUSION_ULTIMATE_DEPS270_RULES_H
#define LAWN_FUSION_ULTIMATE_DEPS270_RULES_H
#include "FusionZombieRules.h"
#include <cstdint>
namespace LawnFusion
{
enum class FusionUltimateDeps270Effect:uint8_t {Cold,Freeze,Butter,Poison,Garlic,MindControl};
constexpr int kFusionMachine270OffspringCount = 5;
// A new Machine reserves its own slot as well. During death the existing
// parent is already counted in live; only its five actual children need slots.
bool FusionMachine270CanReserveOffspring(int live,int capacity,bool includeParent);
const FusionZombieProfile* FindFusionUltimateDeps270Profile(uint16_t);
bool FusionUltimateDeps270RejectEffect(uint16_t kind,int status,FusionUltimateDeps270Effect);
bool ChooseFusionMachine270Type(int sourceDraw,uint8_t& type);
uint8_t FusionMachine270DamageStage(float health,int maximum,uint8_t previous);
bool FusionMachine270CanHitProjectile(uint8_t way,bool hostile,bool sameRow);
bool TryGetFusionUltimateDeps270RootDelta(uint16_t kind,bool entering,FusionZombieClipPosition before,FusionZombieClipPosition after,bool friendly,double pixels,double& delta);
bool CountFusionUltimateDeps270Events(uint16_t kind,bool entering,FusionZombieClipPosition before,FusionZombieClipPosition after,uint32_t& count);
float FusionMachine270BallVisualSeconds(int nativeAge);
}
#endif
