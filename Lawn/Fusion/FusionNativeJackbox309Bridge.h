#pragma once
#include "FusionNativeJackbox300.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
// Shared Lawnf.ZombieExplode4017D0. A standalone validated source object may
// outlive its owner; retain the actual birth sourceKind/identity, never alias24.
bool CanEmitNativeSourceZombieExplosion343(Board*,const NativeJackbox300Explosion&,std::string* error=nullptr,int sourcePlantDamageType=0);
bool EmitNativeSourceZombieExplosion343(Board*,const NativeJackbox300Explosion&,int sourcePlantDamageType=0);
NativeJackbox300State* EditNativeJackbox309(Zombie*,bool create=false);
const NativeJackbox300State* GetNativeJackbox309(const Zombie*);
bool CanSpawnNativeJackbox309(Board*,uint16_t kind,std::string* error=nullptr);
bool InitializeNativeJackbox309(Zombie*);
const NativeJackbox300Hooks& GetNativeJackbox309Hooks();
bool UpdateNativeJackbox309(Zombie*);
bool UpdateNativeJackbox309Animation(Zombie*);
bool StartNativeJackbox309Walk(Zombie*);
bool MoveNativeJackbox309(Zombie*);
bool NativeJackbox309RowMoving(const Zombie*);
// Root calls before stock body/terminal changes. Refused means no destructive
// continuation; OwnerRetired means the captured full generation is gone.
NativeJackbox300DeathResult BeginNativeJackbox309Death(Zombie*,int sourceReason);
bool NativeJackbox309AudioStatus(Zombie*,bool pause);
bool NativeJackbox309LoseHead(Zombie*);
bool NativeJackbox309FirstArmorFall(Zombie*);
bool ApplyNativeJackbox309Armor(Zombie*,int damage,int& remaining);
// Typed return consumes this GetDamage path. Filtered damage is source status11.
bool FilterNativeJackbox309Damage(const Zombie*,int incoming,int& filtered);
bool CanDisarmNativeJackbox309(const Zombie*);
bool CommitNativeJackbox309Disarm(Zombie*);
void NativeJackbox309Removing(Zombie*);
bool GetNativeJackbox309Presentation(const Zombie*,const char*&,float&);
// Root-owned common Charred bridge: source D2 body<=damage -> genuine SetCarred
// +Die2; otherwise source DmgType12. Never substitute native18U damage.
bool CanApplyNativeJackbox309Charred(Zombie*,int damage,std::string* error=nullptr);
bool ApplyNativeJackbox309Charred(Zombie*,int damage);
}
