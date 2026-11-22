#pragma once
#include "FusionSourceDeathBudget319.h"
#include <string>
#include <span>
class Zombie;class Board;
namespace LawnFusion
{
bool CanRetireNativeSourceZombie313(Zombie*,int sourceReason,std::string* error=nullptr);
bool RetireNativeSourceZombie313(Zombie*,int sourceReason);
// Root's stock Die/PlayDeath hooks use this to avoid entering the same source
// virtual dispatch twice. Scope matches Board/epoch/full local generation.
bool IsNativeSourceRetiring313(const Zombie*);
// Source BaseDie restores garlicSpeed=1 before DieEvent. HOST full-generation
// scope covers that callback; the once-only points marker covers its later
// dying graph. Row continuation metadata is deliberately retained.
bool NativeSourceDeathIgnoresGarlic313(const Zombie*);
bool PlanNativeSourceRetireBatch319(Board*,std::span<const uint32_t> fullUIDs,int sourceReason,FusionSourceDeathBudget319& out,std::string* error=nullptr);
bool CanRetireNativeSourceZombieBatch313(Board*,std::span<const uint32_t> fullUIDs,int sourceReason,std::string* error=nullptr);
bool CanRetireNativeSourceZombieBatch313(Board*,std::span<const uint32_t> fullUIDs,int sourceReason,const FusionSourceDeathBudget319& additionalBirthBudget,std::string* error=nullptr);
// Capacity arithmetic is shared by death+birth transactions. This loads actual
// clock-carrier definitions and validates their relevant nonempty track spans;
// no speculative RNG, native actor creation or Classic behavior changes.
bool GetNativeSourceZombieBirthBudget319(Board*,std::span<const uint16_t> kinds,FusionSourceDeathBudget319& out,std::string* error=nullptr);
bool CanAllocateNativeSourceBudget319(Board*,const FusionSourceDeathBudget319& peak,std::string* error=nullptr);
}
