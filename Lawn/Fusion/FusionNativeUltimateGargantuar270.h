#ifndef LAWN_FUSION_NATIVE_ULTIMATE_GARGANTUAR270_H
#define LAWN_FUSION_NATIVE_ULTIMATE_GARGANTUAR270_H
#include "FusionUltimateGargantuar270Rules.h"
#include "FusionNativeWorldRemainder269.h"
class Board;class Zombie;class Plant;class Projectile;
namespace LawnFusion
{
// One native Projectile carrier, with raw position/velocity/age as its only
// simulation state. Root owns a full-generation keyed map and FNS adoption.
using NativeUltimateBall270State=NativeWorldRemainder269AuxState;
// Persistent source damage appearance, not a timer or a second actor.
struct NativeUltimateGarg270State {uint8_t bodyStage=0;bool boardCounted=false;}; // Host-only lifetime guard.
NativeUltimateGarg270State* EditNativeUltimateGarg270(Zombie*,bool create=false);
const NativeUltimateGarg270State* GetNativeUltimateGarg270(const Zombie*);
NativeUltimateBall270State* EditNativeUltimateBall270(Projectile*,bool create=false);
const NativeUltimateBall270State* GetNativeUltimateBall270(const Projectile*);
bool EnsureNativeUltimateBall270Visual(Board*);
// These callbacks represent actual sourceBoard fields, not an extra timer:
// Start sets punishTime100000; factory/death maintain ultimateGargantuarCount.
void NativeUltimateGarg270BoardEvent(Zombie*,bool born);
using NativeUltimateGarg270NodeQuery=bool(*)(const Zombie*,const char*,float&,float&);
bool CanCreateNativeUltimateGarg270(Board*,int row);
bool IsNativeUltimateGarg270(const Zombie*);
bool InitializeNativeUltimateGarg270(Zombie*);
bool UpdateNativeUltimateGarg270Animation(Zombie*);
bool StartNativeUltimateGarg270Walk(Zombie*,int blendTicks);
bool MoveNativeUltimateGarg270(Zombie*);
// Replace stock UpdateZombieGargantuar and generic typed bite processing.
bool UpdateNativeUltimateGarg270(Zombie*,NativeUltimateGarg270NodeQuery);
// Cap AFTER the common source GetDamage modifiers and BEFORE armor splitting.
bool AdjustNativeUltimateGarg270IncomingDamage(Zombie*,int& damage);
bool ApplyNativeUltimateGarg270BodyDamage(Zombie*,int damage);
bool GetNativeUltimateGarg270Presentation(const Zombie*,const char*& clip,float& phase);
bool GetNativeUltimateGarg270Status(const Zombie*,int& sourceStatus);
void NativeUltimateGarg270Died(Zombie*); // Call exactly once, before source identity removal.
Projectile* CreateNativeUltimateBall270(Zombie*,NativeUltimateGarg270NodeQuery);
bool UpdateNativeUltimateBall270(Projectile*); // Early return before stock pea motion/impact.
}
#endif
