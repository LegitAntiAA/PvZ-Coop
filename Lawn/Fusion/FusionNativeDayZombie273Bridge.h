#pragma once
#include "FusionNativeDayZombie273.h"
#include <string>
namespace LawnFusion
{
// Complete-family readiness; staged profiles alone never admit a live actor.
bool CanSpawnNativeDayZombie273(Board*,uint16_t kind,std::string* error=nullptr);
const NativeDayZombie273Hooks& GetNativeDayZombie273Hooks();
// Prepare source106's direct DropArmor BEFORE a Chomper can retire it.
// A false result leaves the target and current bite locked for a later retry.
bool PrepareNativeDayZombie273ChomperArmor(Zombie*,Coin*& born,int& oldType,int& oldHealth);
void RollbackNativeDayZombie273ChomperArmor(Zombie*,Coin* born,int oldType,int oldHealth);
}
