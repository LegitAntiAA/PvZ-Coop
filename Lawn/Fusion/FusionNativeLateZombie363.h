#pragma once
#include "FusionLateZombie363Rules.h"
#include "FusionSourceDeathBudget319.h"
#include <string>
class Zombie;
namespace LawnFusion {
// Root aggregate retirement planner adds this mandatory child peak once, before
// ANY parent commits. Native source factory readiness covers transitive closure.
bool PlanNativeLate363Death(Zombie*,int,FusionSourceDeathBudget319&,std::string* error=nullptr);
bool CanRetireNativeLate363(Zombie*,int,std::string* error=nullptr);
bool BeforeRetireNativeLate363(Zombie*,int);
}
