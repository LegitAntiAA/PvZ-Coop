#pragma once
#include "FusionBoss361Rules.h"
#include <string>
#include <cstdint>
#include <memory>
class Board;class Zombie;class Coin;class Plant;class LawnApp;
namespace Sexy{class Graphics;}
namespace LawnFusion
{
struct FusionCommand;
class NativeBossRoomVisuals366
{
public:
 NativeBossRoomVisuals366();~NativeBossRoomVisuals366();
 bool Ensure(LawnApp*,std::string&);bool Draw(const Coin*,Sexy::Graphics*)const;
private:struct Impl;std::unique_ptr<Impl> impl;
};
// One source Board room state, not an Adventure profile or another actor array.
struct NativeBossRoom366
{
 bool enabled=false,boss2=false,fastBoard=false,hammerGranted=false,gloveGranted=false;
 bool fakeTrophyNext=false,won=false,trophyFake=false,trophyClaimed=false;
 uint16_t hammerCooldownTicks=6000,trophyId=0;
 uint32_t trophyToken=0,trophyAnimationBaseAge=0;
 int32_t enemyCount=0;
 float trophyStartX=0,trophyStartY=0;
 // HOST ONLY, never replicated or interpreted as an animation clock.
 int lastTick=-1;
 bool spawnPending375=false,strongerStart375=false;
};
NativeBossRoom366* EditNativeBossRoom366(Board*);
const NativeBossRoom366* GetNativeBossRoom366(const Board*);
bool EnsureNativeBossRoom366(Board*,std::string* error=nullptr);
// Fresh native room setup, also legal during chooser/intro. The existing
// source mode adapter supplies its bank/economy descriptor. Both authored
// isBoss/isBoss2 modes start source44; isBoss2 applies source SetZombieHealth(5).
// Native source actor creation is deferred until the real Playing boundary.
bool BeginNativeBossRoom366(Board*,bool sourceIsBoss2=false,std::string* error=nullptr);
bool GrantNativeBossTools366(Board*,int sourceDamageBand);
void UpdateNativeBossRoom366(Board*);
bool CanCreateNativeBossTrophy366(Board*,std::string* error=nullptr);
bool CreateNativeBossTrophy366(Zombie*);
bool IsNativeBossTrophy366(const Coin*);
bool DrawNativeBossTrophy366(const Coin*,Sexy::Graphics*);
bool UpdateNativeBossTrophy366(Coin*);
bool HandleNativeBossRoom366Command(Board*,int authenticatedSlot,const FusionCommand&);
bool HandleNativeBossRoom366Pointer(Board*,int button,int boardX,int boardY);
bool ToggleNativeBossHammer366(Board*);
void DrawNativeBossTools366(Board*,Sexy::Graphics*);
bool NativeBossRoomGloveAllowed366(const Board*);
bool NativeSourcePlantInvincible366(const Plant*);
bool PreflightNativeBossCrush366(Zombie*,const FusionBoss361View&,bool rv,std::string* error=nullptr);
bool ApplyNativeBossCrush366(Zombie*,const FusionBoss361View&,bool rv);
// Actual source Plant.Crashed, also usable by other source crushing callers.
bool ApplyNativeSourcePlantCrashed366(Plant*,Zombie* sourceBoss=nullptr);
}
