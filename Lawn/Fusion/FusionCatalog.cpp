#include "FusionCatalog.h"

namespace LawnFusion
{
namespace
{
#include "../../assets/fusion/catalog/FusionCatalogData.inc"

template <typename T, size_t N>
const T* FindById(const T (&entries)[N], int id)
{
    if (id < 0 || id >= kFusionNoPlant)
        return nullptr;
    size_t first = 0;
    size_t last = N;
    while (first < last)
    {
        const size_t middle = first + (last - first) / 2;
        if (entries[middle].id < id)
            first = middle + 1;
        else
            last = middle;
    }
    return first < N && entries[first].id == id ? &entries[first] : nullptr;
}
}

const FusionAlmanacEntry* FusionFindPlantAlmanac(int id) { return FindById(kFusionPlantAlmanac, id); }
const FusionAlmanacEntry* FusionPlantAlmanacAt(size_t index) { return index < FusionPlantAlmanacCount() ? &kFusionPlantAlmanac[index] : nullptr; }
size_t FusionPlantAlmanacCount() { return sizeof(kFusionPlantAlmanac) / sizeof(kFusionPlantAlmanac[0]); }
const FusionAlmanacEntry* FusionFindZombieAlmanac(int id) { return FindById(kFusionZombieAlmanac, id); }
const FusionAlmanacEntry* FusionZombieAlmanacAt(size_t index) { return index < FusionZombieAlmanacCount() ? &kFusionZombieAlmanac[index] : nullptr; }
size_t FusionZombieAlmanacCount() { return sizeof(kFusionZombieAlmanac) / sizeof(kFusionZombieAlmanac[0]); }

const FusionPlantDefinition* FusionFindPlant(int id) { return FindById(kFusionPlants, id); }
const FusionPlantDefinition* FusionPlantAt(size_t index) { return index < FusionPlantCount() ? &kFusionPlants[index] : nullptr; }
size_t FusionPlantCount() { return sizeof(kFusionPlants) / sizeof(kFusionPlants[0]); }
const FusionEntityName* FusionFindZombie(int id) { return FindById(kFusionZombies, id); }
const FusionEntityName* FusionZombieAt(size_t index) { return index < FusionZombieCount() ? &kFusionZombies[index] : nullptr; }
size_t FusionZombieCount() { return sizeof(kFusionZombies) / sizeof(kFusionZombies[0]); }
const FusionZombieStats* FusionFindZombieStats(int id) { return FindById(kFusionZombieStats, id); }
const FusionEntityName* FusionFindBullet(int id) { return FindById(kFusionBullets, id); }
const FusionEntityName* FusionBulletAt(size_t index) { return index < FusionBulletCount() ? &kFusionBullets[index] : nullptr; }
size_t FusionBulletCount() { return sizeof(kFusionBullets) / sizeof(kFusionBullets[0]); }
}
