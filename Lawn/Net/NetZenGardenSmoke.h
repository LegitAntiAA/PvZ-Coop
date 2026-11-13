#pragma once

#include "../../ConstEnums.h"
#include <cstdint>

class LawnApp;
class Plant;

namespace LawnNet
{
bool NetSmokeUpdateZenGarden(LawnApp* app);
bool NetSmokeZenGardenAllowsStart(LawnApp* app, uint8_t mode, int32_t level);
void NetSmokeObserveZenOverlay(LawnApp* app, const Plant* plant, PottedPlantNeed need);
}
