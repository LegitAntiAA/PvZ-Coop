#pragma once
#include "FusionRandomZombie360Rules.h"
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
// No second state/map/clock. Armor stage uses existing source metadata;
// summoned is a synchronous epoch/full-generation transaction before Die2.
struct NativeRandom360Hooks
{
 // Resource/allocator/actual-directfactory admission only; cannot remove a
 // source pool entry or claim a native rejection was a genuine source null.
 bool (*prepareOutcome)(Board*,uint16_t emitted,int row,bool controlled,std::string*)=nullptr;
 // Full source TravelMgr outcomeclosure and canonical cooldown/upgrade ledger.
 bool (*prepareTravelBuff)(Board*,std::string*)=nullptr;
 bool (*travelBuffCooldown)(const Board*,bool& active)=nullptr;
 bool (*unlockTravelBuff)(Zombie*,bool& awarded,std::string*)=nullptr;
 // Optional shared directSetZombie factory override; default uses the REAL
 // SpawnNativeSourceZombie request, same row/actualShadowX, withEffectfalse.
 Zombie* (*spawnOutcome)(Zombie*,uint16_t emitted,float boardShadowX)=nullptr;
};
bool IsNativeRandom360(const Zombie*);
bool EnsureNativeRandom360Resources(Board*,uint16_t,std::string* error=nullptr);
bool PreflightNativeRandom360Fall(Zombie*,const NativeRandom360Hooks&,std::string* error=nullptr);
// Root dispatches classified105/110/215 before ordinary TakeHelmDamage. false
// refuseswithoutarmor/healthmutation; Root mustnotfallbacktovanillaonrefusal.
bool ApplyNativeRandom360FirstArmorDamage(Zombie*,int resolvedDamage,int& overflow,const NativeRandom360Hooks&,std::string* error=nullptr);
// Real source LoseHeadEvent override. No ordinary ConeFall/head FX/stockdeath.
bool ApplyNativeRandom360HeadLoss(Zombie*,const NativeRandom360Hooks&,std::string* error=nullptr);
bool NativeRandom360FallInProgress(const Zombie*);
}
