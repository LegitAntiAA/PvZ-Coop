#ifndef LAWN_FUSION_CATALOG_H
#define LAWN_FUSION_CATALOG_H

#include <cstddef>
#include <cstdint>

namespace LawnFusion
{
using FusionPlantId = uint16_t;
constexpr FusionPlantId kFusionNoPlant = 0xffff;

// Original Fusion 2.3.1 identifiers, never Classic SeedType ordinals.
// Catalog membership describes source data, not an implemented ability or unlock.
struct FusionPlantDefinition
{
    FusionPlantId id;
    const char* enumName;
    const char* nameUtf8;
    float attackIntervalSeconds;
    float productionIntervalSeconds;
    int damage;
    int maxHealth;
    float rechargeSeconds;
    int sunCost;
    bool hasStats;
    bool hasAlmanac;
};

struct FusionEntityName
{
    uint16_t id;
    const char* enumName;
    const char* nameUtf8;
};

struct FusionZombieStats
{
    uint16_t id;
    int damage;
    int maxHealth;
    int firstArmorMaxHealth;
    int secondArmorMaxHealth;
};

// Source TextAsset fields, including original richtext and empty optional cost.
// These entries describe the almanac, independently of gameplay/unlock support.
struct FusionAlmanacEntry
{
    uint16_t id;
    const char* nameUtf8;
    const char* infoUtf8;
    const char* loreUtf8;
    const char* costUtf8;
    // SHA256 of canonical UTF8 JSON over source name/info/introduce/cost.
    const char* sourceFieldsSha256;
};

const FusionAlmanacEntry* FusionFindPlantAlmanac(int id);
const FusionAlmanacEntry* FusionPlantAlmanacAt(size_t index);
size_t FusionPlantAlmanacCount();
const FusionAlmanacEntry* FusionFindZombieAlmanac(int id);
const FusionAlmanacEntry* FusionZombieAlmanacAt(size_t index);
size_t FusionZombieAlmanacCount();

const FusionPlantDefinition* FusionFindPlant(int id);
const FusionPlantDefinition* FusionPlantAt(size_t index);
size_t FusionPlantCount();
const FusionEntityName* FusionFindZombie(int id);
const FusionEntityName* FusionZombieAt(size_t index);
size_t FusionZombieCount();
const FusionZombieStats* FusionFindZombieStats(int id);
const FusionEntityName* FusionFindBullet(int id);
const FusionEntityName* FusionBulletAt(size_t index);
size_t FusionBulletCount();
}

#endif
