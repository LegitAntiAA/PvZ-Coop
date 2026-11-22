#pragma once
#include "FusionHeavyZombie313BRules.h"
namespace LawnFusion
{
// Source Start lambda3510A0 filters only column. Duplicate row entries remain
// weighted by the number of source plants; stealability is a later Catch rule.
bool BuildFusionBossBungee366RowPool(std::span<const FusionHeavy313BPlant>,int column,int rowCount,std::vector<int>&);
}
