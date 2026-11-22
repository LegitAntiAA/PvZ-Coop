#pragma once
#include "FusionNativeExtendedZombie313.h"
#include "FusionExtendedZombie347Rules.h"
class Projectile;class Board;class LawnApp;
namespace LawnFusion
{
// All347 actors reuse extendedZombies313 + sourceGraphs313. No second owner,
// native body clock, world container or client simulation is introduced.
enum class NativeExtended347Operation:uint8_t {Initialize,Attribute,Throw,Pop,Death,Convert};
bool EnsureNativeExtended347Resources(Board*,uint16_t,std::string* error=nullptr);
bool PreflightNativeExtended347(Zombie*,NativeExtended347Operation,std::string* error=nullptr);
bool InitializeNativeExtended347Fields(Zombie*,NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&);
bool UpdateNativeExtended347Fields(Zombie*,NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&);
bool DispatchNativeExtended347Event(Zombie*,FusionSourceGraph313Event,int,const NativeExtendedZombie313Hooks&);
bool ResolveNativeExtended347Damage(Zombie*,int raw,bool fix,int& resolved);
bool ApplyNativeExtended347Charred(Zombie*,int raw,bool fix=false);
// HOST stack-only immutable source220 AttackEffect input. Actor pointers are
// re-resolved by full local generation around every common gameplay callback.
struct NativeExtended347BiteCapture
{
 LawnApp* app=nullptr;Board* board=nullptr;
 uint32_t epoch=0,ownerUID=0,ownerToken=0,targetUID=0,targetToken=0;
 int row=0,renderOrder=0;float shadowX=0,shadowY=0;
 bool invincible=false;
};
enum class NativeExtended347BitePreparation:uint8_t {NotHandled,Ready,Refused};
NativeExtended347BitePreparation PrepareNativeExtended347Bite(Zombie*,Plant*,NativeExtended347BiteCapture&);
bool ApplyNativeExtended347CapturedBite(const NativeExtended347BiteCapture&,int damage);
int NativeExtended347AttackDamage(const Zombie*,int fallback);
bool NativeExtended347Bomb(Zombie*);
bool NativeExtended347MachineDeath(Zombie*);
bool BeforeRetireNativeExtended347(Zombie*,int reason,const NativeExtendedZombie313Hooks&);
// Root calls at successful hostile factory commit, never from source Start or
// failed Init cleanup. Counter is source Board state, not an alive-NPC scan.
bool CommitNativeExtended347Factory(Zombie*);
int NativeUltimateFootballDrownCount343(const Board*);
// Root's typed actual Projectile dispatcher increments age once, then invokes
// this kind5 controller. Gravity is genuine Physics2DSettings(-9.81), no timer.
bool UpdateNativeSpiderWeapon347(Projectile*);
}
