#ifndef LAWN_FUSION_RECIPE_H
#define LAWN_FUSION_RECIPE_H

#include "FusionCatalog.h"

namespace LawnFusion
{
struct FusionRecipe
{
    FusionPlantId existing;
    FusionPlantId used;
    FusionPlantId result;
};

// Original directional MixData.data[existing, used] entries. A recipe alone
// does not authorize placement: terrain, layers and level limits also apply.
const FusionRecipe* FindFusionRecipe(FusionPlantId existing, FusionPlantId used);
const FusionRecipe* GetFusionRecipes();
size_t GetFusionRecipeCount();
}

#endif
