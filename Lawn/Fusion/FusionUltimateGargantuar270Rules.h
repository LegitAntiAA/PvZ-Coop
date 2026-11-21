#ifndef LAWN_FUSION_ULTIMATE_GARGANTUAR270_RULES_H
#define LAWN_FUSION_ULTIMATE_GARGANTUAR270_RULES_H
#include "FusionZombieRules.h"
#include <cstdint>
namespace LawnFusion
{
constexpr uint16_t kFusionUltimateGarg270Kind=212,kFusionUltimateGarg270ImpKind=213;
constexpr float kFusionUltimateGarg270WalkDuration=3.5833333f;
constexpr float kFusionUltimateGarg270ActionDuration=2.8333333f;
enum class FusionUltimateGarg270Effect:uint8_t {Cold,Freeze,Butter,MindControl,KnockBack};
enum class FusionUltimateGarg270PlantHit:uint8_t {Ignore,Crush,Damage500,Damage1000};
const FusionZombieProfile* FindFusionUltimateGarg270Profile(uint16_t kind);
bool CapFusionUltimateGarg270Damage(int damage,int& capped);
bool ScaleFusionUltimateGarg270Status(float seconds,FusionUltimateGarg270Effect,float& scaled);
bool FusionUltimateGarg270MustThrow(float health,int maximum,bool carrying,bool friendly,float shadowSourceX);
uint8_t FusionUltimateGarg270BodyStage(float health,int maximum,uint8_t previous);
FusionUltimateGarg270PlantHit PlanFusionUltimateGarg270PlantHit(uint16_t kind,bool bigNut,bool nut,int level);
bool FusionUltimateGarg270ZombieAllowed(int sourceStatus,bool sameRow,bool oppositeSide);
bool TryGetFusionUltimateGarg270RootDelta(FusionZombieClipPosition before,FusionZombieClipPosition after,bool friendly,double pixelsPerUnit,double& delta);
bool CountFusionUltimateGarg270Events(FusionZombieClipPosition before,FusionZombieClipPosition after,float eventClipSeconds,uint32_t& count);
// Semi-implicit source SuperBall.Update: gravity first, then Translate(v*dt).
bool StepFusionUltimateBall270(float x,float y,float vx,float vy,float dt,float landY,float& nextX,float& nextY,float& nextVy,bool& landed);
}
#endif
