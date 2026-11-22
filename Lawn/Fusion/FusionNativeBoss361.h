#pragma once
#include "FusionBoss361Rules.h"
#include "FusionNativeSourceGraph313.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
struct NativeBoss361State
{
 FusionBoss361View view;
 uint32_t localUID=0;
 int tick=-1;
 bool initialized=false,failed=false,deathStarted=false,deathEnded=false;
};
enum class NativeBoss361Operation:uint8_t
{
 Initialize,Decision,AlignRv,SpawnChild,BungiTriple,Stomp,RvCrushAndCards,
 PutBall,FootSound,MachineSound2,DieExplode,ResetDebuffs,ClearFreeze,
 GrantBodyTools,DeathBegin,DeathEnd
};
struct NativeBoss361Facts
{
 FusionBoss361Input input;
 float sourceHealth=60000;
 float mainAnimatorSpeed=1; // Actual source animation factor, distinct from freezeSpeed gate.
 int maximumHealth=60000;
 bool boardBoss2=false;
};
struct NativeBoss361Action
{
 FusionBoss361View view;
 FusionBoss361Decision decision=FusionBoss361Decision::None;
 uint16_t childKind=0xffff;
 int payload=0;
};
struct NativeBoss361Hooks
{
 NativeBoss361State* (*stateFor)(Zombie*)=nullptr;
 const NativeSourceGraph313Hooks* graph=nullptr;
 bool (*world)(const Board*,uint16_t)=nullptr;
 // Genuine model, 3 native clocks (Boss/Boss-RV/BossDriver), 16 PS bindings,
 // full source child closure, balls32/33, Bungee backlink, trophies and tools.
 // Must not report ready for only model/graph availability.
 bool (*ensure)(Board*,uint16_t,std::string*)=nullptr;
 bool (*facts)(Zombie*,NativeBoss361Facts&)=nullptr;
 bool (*definedSourcePrefabs)(Board*,std::span<uint16_t>,uint16_t&)=nullptr;
 // Whole mandatory callback peak BEFORE any RNG/destructive operation. For
 // SpawnChild the full possible pool is supplied through source counters;
 // never narrow source RNG to only currently supported kinds.
 bool (*preflight)(Zombie*,NativeBoss361Operation,const NativeBoss361Action&,std::string*)=nullptr;
 // Thin bindings to actual native factories/Plant.Crashed/droppedCard.Fall,
 // reliable FX/audio, Boss room tool policy and native source trophy flow.
 // SpawnChild uses action.childKind/row and source spawnPosition.x; BungiTriple
 // is three source45 births at row0/x9.9, columns1/2/3 with owner backlink.
 bool (*apply)(Zombie*,NativeBoss361Operation,const NativeBoss361Action&)=nullptr;
 int (*randomInt)(Board*,int inclusive,int exclusive)=nullptr;
 void (*failed)(Board*,const char*)=nullptr;
};
bool CanSpawnNativeBoss361(Board*,uint16_t,const NativeBoss361Hooks&,std::string* error=nullptr);
bool InitializeNativeBoss361(Zombie*,NativeBoss361State&,const NativeBoss361Hooks&);
bool UpdateNativeBoss361(Zombie*,NativeBoss361State&,const NativeBoss361Hooks&);
bool NativeBoss361Parameters(Zombie*,uint8_t layer,FusionSourceGraph313Parameters&,const NativeBoss361Hooks&);
bool PreflightNativeBoss361Event(Zombie*,FusionSourceGraph313Event,const NativeBoss361Hooks&,std::string* error=nullptr);
bool DispatchNativeBoss361Event(Zombie*,FusionSourceGraph313Event,int,const NativeBoss361Hooks&);
// resolved damage has already passed the common source GetDamage exactly once.
// Boss Charred maps to common damageType4; it never creates ordinary charred art.
bool ApplyNativeBoss361BodyDamage(Zombie*,int resolved,const NativeBoss361Hooks&);
bool BeginNativeBoss361Death(Zombie*,const NativeBoss361Hooks&);
}
