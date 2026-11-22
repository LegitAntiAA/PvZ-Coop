#pragma once
#include "FusionNativeDayZombie273.h"
#include <string>
namespace LawnFusion
{
// Complete-family readiness; staged profiles alone never admit a live actor.
bool CanSpawnNativeDayZombie273(Board*,uint16_t kind,std::string* error=nullptr);
const NativeDayZombie273Hooks& GetNativeDayZombie273Hooks();
// Shared genuine ground-controller/dependency world, not the wave roster.
bool CanSpawnNativeGrassZombie284(Board*,uint16_t kind,std::string* error=nullptr);
// Shared genuine FX11: Dance child ROOT+.5, Jackson child SHADOW+.7.
bool PlayNativeNightChildEffect291(Zombie* parent,Zombie* child,bool dancePole);
// Source6's accepted JumpOver is retried only before the first real birth.
void RetryNativeDancePole284(Zombie*);
// Prepare source106's direct DropArmor BEFORE a Chomper can retire it.
// A false result leaves the target and current bite locked for a later retry.
bool PrepareNativeDayZombie273ChomperArmor(Zombie*,Coin*& born,int& oldType,int& oldHealth);
void RollbackNativeDayZombie273ChomperArmor(Zombie*,Coin* born,int oldType,int oldHealth);
}
