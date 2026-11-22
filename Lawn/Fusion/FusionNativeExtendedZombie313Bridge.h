#pragma once
#include "FusionNativeExtendedZombie313.h"
#include "FusionSourceBoard313.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionSourceDeathBudget319.h"
#include <span>
namespace LawnNet { struct NetZombieAnimState; }
namespace LawnFusion
{
NativeExtendedZombie313State* EditNativeExtendedZombie313(Zombie*,bool create=false);
const NativeExtendedZombie313State* GetNativeExtendedZombie313(const Zombie*);
const NativeExtendedZombie313Hooks& GetNativeExtendedZombie313Hooks();
bool EnsureNativeExtendedZombie313Resources(Board*,uint16_t,std::string* error=nullptr);
bool CanSpawnNativeExtendedZombie313(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeExtendedZombie313Bridge(Zombie*);
bool UpdateNativeExtendedZombie313Bridge(Zombie*);
// Called instead of stock NORMAL movement once AFTER the family/graph update.
// Flight uses raw native velocity/pos/altitude; ground displacement is owned by
// the segmented graph's genuine root curve, never a second animation clock.
bool MoveNativeExtendedZombie313Physics(Zombie*);
bool PrepareNativeExtendedZombie313Clocks(Zombie*,std::string* error=nullptr);
bool AdoptNativeExtendedZombie313Clocks(Zombie*,std::span<const LawnNet::NetZombieAnimState>,std::string* error=nullptr);
void RemoveNativeExtendedZombie313Bridge(Zombie*);
// Source CreateZombie root/axis coordinate before its Start, then exact axis
// placement. Used by actual SuperBall landing and Machine death descendants.
Zombie* SpawnNativeExtendedZombieAtAxis313(Board*,uint16_t,int row,float sourceX,float sourceY,bool controlled);
// Root's one shared source damage boundary (0/4/10/12), resolving source
// GetDamage once and preserving virtual armor/body/terminal callbacks.
// Root owns source-status dispatcher and animated standalone native R leases.
// These are typed source operations, not renderer data or approximate particles.
bool ReadNativeSourceStatusLevel313(const Zombie*,int&);
bool EnsureNativeSourceAnimatedEffect313(Board*,uint16_t sourceEffect,std::string* error=nullptr);
bool PlayNativeSourceAnimatedEffect313(Zombie*,uint16_t sourceEffect,float boardX,float boardY);
// One common source MC transaction: preflight DropItem/awards, source62/63,
// mind-control state/retire-count/target clearing and preserve the actual axis.
// False must precede any state/item mutation; not stock StartMindControlled.
bool ApplyNativeSourceMindControl313(Zombie*,int sourceControlLevel);
bool PrepareNativeSourceMindControl313(Board*,FusionSourceDeathBudget319& peak,std::string* error=nullptr);
}
