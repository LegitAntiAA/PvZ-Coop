#pragma once
#include "FusionSourceGraph313.h"
#include <cstdint>
#include <span>
namespace LawnFusion
{
enum class FusionBoss361Phase:uint8_t {Enter,Idle,Spawn,Foot,Rv,HeadEnter,HeadIdle,HeadLeave,Bungi};
struct FusionBoss361View
{
 uint16_t kind=44;
 FusionBoss361Phase phase=FusionBoss361Phase::Enter;
 uint8_t sourceStatus=21,targetRow=0,targetColumn=0,ballType=0,bodyStage=0,eyeMask=0;
 int32_t summonCount=0,spawnCount=8,summonBallCount=0;
 float summonTime=5,ballCountdown=3;
 uint8_t bodyHistoryMask=0; // Source middle/low writes are not identical on a skipped band.
 float rvOffsetX=0,rvOffsetY=0;bool rvAligned=false;
 float eyeBlackSeconds=0; // Source ColoredEye coroutine, native update delta only.
 bool ballBusy=false,colliderEnabled=false,rvVisible=false,dying=false;
};
const FusionSourceGraph313* FindFusionBoss361Graph(uint16_t);
bool FusionBoss361Kind(uint16_t);
bool FusionBoss361ViewValid(const FusionBoss361View&);
// TypeMgr.WaterZombie40DDB0 / IsAirZombie40D030 and AllowUnique37A030.
bool FusionBoss361AllowChild(uint16_t kind,bool boardBoss2);
bool FusionBoss361BuildChildPool(bool boardBoss2,std::span<const uint16_t> genuineDefinedPrefabs,
                               std::span<uint16_t> output,uint16_t& count);
enum class FusionBoss361Decision:uint8_t {None,HeadEnter,Spawn,Stomp,Attack,Bungi,Rv};
struct FusionBoss361Plan
{
 FusionBoss361View next;
 FusionBoss361Decision decision=FusionBoss361Decision::None;
 uint8_t randomRowUpper=0,randomColumnUpper=0;
 bool randomBallType=false;
};
// Read-only inputs come from the actual source-ordered plant/card registries.
struct FusionBoss361Input
{
 bool gameActive=true;
 float freezeSpeed=1;
 int firstCrushPlantRow=-1; // First allocated tag0 plant with column>=6, not nearest target.
 bool droppedCard16=false,droppedCard10=false;
};
bool PlanFusionBoss361Tick(const FusionBoss361View&,const FusionBoss361Input&,float delta,FusionBoss361Plan&);
bool PlanFusionBoss361Skill(const FusionBoss361View&,float sourceHealth,int maximumHealth,FusionBoss361Plan&);
bool ResolveFusionBoss361Plan(const FusionBoss361Plan&,int rowDraw,int columnDraw,int ballDraw,
                            FusionBoss361View&,uint64_t& mainTriggers,uint64_t& driverTriggers);
// Exact integer thirds and source min<HP<=max. No write at equality to lower
// boundary or above the middle interval; previous art survives healing.
bool PlanFusionBoss361BodyStage(uint16_t kind,int maximum,float health,uint8_t previous,uint8_t& stage);
bool FusionBoss361StompCell(int targetRow,int row,int column,bool plantTag0);
bool FusionBoss361RvCell(int targetRow,int targetColumn,int row,int column);
// Boss_head* / Boss_idle / Boss_rv source behaviour callbacks. RNG is requested
// only by HeadAttackExit; native caller preflights before that draw.
bool ApplyFusionBoss361Behaviour(FusionBoss361View&,FusionSourceGraph313Event,int randomSpawnCount=4);
}
