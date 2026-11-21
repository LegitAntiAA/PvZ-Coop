#include "FusionCatalogTests.h"
#include "../../Lawn/Fusion/FusionCatalog.h"
#include "../../Lawn/Fusion/FusionRecipe.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace
{
FusionCatalogTestResult result;
void Check(bool condition, int line)
{
    ++result.mChecks;
    if (!condition) { ++result.mFailures; std::printf("    Fusion catalog check failed line=%d\n", line); }
}
#define CHECK(condition) Check(!!(condition), __LINE__)
}

FusionCatalogTestResult RunFusionCatalogTests()
{
    using namespace LawnFusion;
    result = {};
    std::printf("[ RUN      ] Fusion.SourceCatalogAndDirectionalRecipes\n");
    CHECK(FusionPlantCount() == 286);
    CHECK(FusionZombieCount() == 94);
    CHECK(FusionBulletCount() == 100);
    CHECK(FusionPlantAlmanacCount() == 281);
    CHECK(FusionZombieAlmanacCount() == 88);
    size_t emptyPlantCost = 0;
    for (size_t i = 0; i < FusionPlantAlmanacCount(); ++i)
    {
        const auto* entry = FusionPlantAlmanacAt(i);
        CHECK(entry && FusionFindPlantAlmanac(entry->id) == entry);
        CHECK(i == 0 || FusionPlantAlmanacAt(i - 1)->id < entry->id);
        CHECK(entry && entry->nameUtf8 && *entry->nameUtf8);
        CHECK(entry && entry->infoUtf8 && *entry->infoUtf8 && entry->loreUtf8 && entry->costUtf8);
        CHECK(entry && entry->sourceFieldsSha256 && std::strlen(entry->sourceFieldsSha256) == 64);
        CHECK(entry && FusionFindPlant(entry->id) && FusionFindPlant(entry->id)->hasAlmanac);
        if (entry && !*entry->costUtf8) ++emptyPlantCost;
    }
    CHECK(emptyPlantCost == 228);
    // Exact sparse membership of the original LawnStrings plant table.
    for (int id = 0; id <= 1193; ++id)
    {
        const bool present = id <= 34 || (id >= 241 && id <= 245) ||
            (id >= 248 && id <= 256) || (id >= 900 && id <= 938) ||
            (id >= 1000 && id <= 1192);
        CHECK((FusionFindPlantAlmanac(id) != nullptr) == present);
    }
    const int zombieAlmanacIds[] = {
        0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
        30,31,32,33,34,35,36,37,38,39,40,43,45,47,48,49,100,101,102,103,104,105,106,107,
        108,109,110,111,112,113,114,115,116,117,118,200,201,202,203,204,205,206,207,208,
        209,210,212,213,214,215,216,217,218,219,220,221,222,223,224};
    CHECK(sizeof(zombieAlmanacIds) / sizeof(zombieAlmanacIds[0]) == FusionZombieAlmanacCount());
    size_t emptyZombieLore = 0;
    for (size_t i = 0; i < FusionZombieAlmanacCount(); ++i)
    {
        const auto* entry = FusionZombieAlmanacAt(i);
        CHECK(entry && entry->id == zombieAlmanacIds[i]);
        CHECK(entry && FusionFindZombieAlmanac(entry->id) == entry);
        CHECK(entry && entry->nameUtf8 && *entry->nameUtf8 && entry->infoUtf8 && *entry->infoUtf8);
        CHECK(entry && entry->loreUtf8 && entry->costUtf8 && !*entry->costUtf8);
        CHECK(entry && entry->sourceFieldsSha256 && std::strlen(entry->sourceFieldsSha256) == 64);
        CHECK(entry && FusionFindZombieStats(entry->id));
        if (entry && !*entry->loreUtf8) ++emptyZombieLore;
    }
    CHECK(emptyZombieLore == 1);
    for (int id : {-1, 65535, 65536})
    {
        CHECK(!FusionFindPlantAlmanac(id));
        CHECK(!FusionFindZombieAlmanac(id));
    }
    CHECK(!FusionFindZombieAlmanac(1));
    CHECK(!FusionFindZombieAlmanac(41));
    CHECK(!FusionFindZombieAlmanac(211));
    CHECK(!FusionPlantAlmanacAt(FusionPlantAlmanacCount()));
    CHECK(!FusionZombieAlmanacAt(FusionZombieAlmanacCount()));
    CHECK(!FusionPlantAlmanacAt(static_cast<size_t>(-1)));
    CHECK(!FusionZombieAlmanacAt(static_cast<size_t>(-1)));
    const auto* sourcePeaSun = FusionFindPlantAlmanac(1000);
    CHECK(sourcePeaSun && std::strcmp(sourcePeaSun->nameUtf8, "\350\261\214\350\261\206\345\220\221\346\227\245\350\221\265") == 0);
    CHECK(sourcePeaSun && std::strstr(sourcePeaSun->infoUtf8, "<color=red>100/5"));
    CHECK(sourcePeaSun && std::strstr(sourcePeaSun->infoUtf8, "\n<color=#3D1400>"));
    CHECK(sourcePeaSun && std::strncmp(sourcePeaSun->loreUtf8, "<color=#3D1400>", 15) == 0);
    CHECK(sourcePeaSun && !*sourcePeaSun->costUtf8);
    const auto* shulk = FusionFindPlantAlmanac(33);
    const auto* onion = FusionFindPlantAlmanac(34);
    CHECK(shulk && std::strcmp(shulk->nameUtf8, "\351\222\242\345\217\266\350\215\211") == 0);
    CHECK(onion && std::strcmp(onion->nameUtf8, "\351\227\252\347\224\265\346\264\213\350\221\261") == 0);
    const auto* sourcePea = FusionFindPlantAlmanac(0);
    CHECK(sourcePea && std::strcmp(sourcePea->costUtf8,
        "\350\212\261\350\264\271\357\274\232<color=red>100</color>\012\345\206\267\345\215\264\346\227\266\351\227\264\357\274\232<color=red>7.5\347\247\222</color>") == 0);
    const auto* sourceZombie = FusionFindZombieAlmanac(0);
    CHECK(sourceZombie && std::strstr(sourceZombie->infoUtf8, "<color=red>270</color>"));
    CHECK(sourcePea && std::strcmp(sourcePea->sourceFieldsSha256, "1097a357eebe5c59277f1096848071e1c862bf77928cc66272f057ac431e43bb") == 0);
    CHECK(shulk && std::strcmp(shulk->sourceFieldsSha256, "7cca83d62e326ce7cf0b01c88e98b8516420b877c67f957ce1ee667e565efeab") == 0);
    CHECK(onion && std::strcmp(onion->sourceFieldsSha256, "c8c3fec880c1d6672080eb31ba20763cf44cde4c729990386ad6cdee58286a21") == 0);
    CHECK(sourcePeaSun && std::strcmp(sourcePeaSun->sourceFieldsSha256, "0197b9214100a309c547bc10289be049e43f81c68d40301c72628661a1f067fd") == 0);
    CHECK(sourceZombie && std::strcmp(sourceZombie->sourceFieldsSha256, "baba5f9d2c1947022174a796e46ebd29771148fc271bd675df7afa7e3fcffa8d") == 0);
    CHECK(FusionFindZombieAlmanac(224) && std::strcmp(FusionFindZombieAlmanac(224)->sourceFieldsSha256,
        "298e08a366bc92966c4a3d81d1db91d2e00194dcff67b71a4fa9ae8d1cb055c4") == 0);
    for (size_t i = 0; i < FusionPlantCount(); ++i)
    {
        const auto* plant = FusionPlantAt(i);
        CHECK(plant && FusionFindPlant(plant->id) == plant);
        CHECK(plant && plant->enumName && *plant->enumName);
        CHECK(plant && (FusionFindPlantAlmanac(plant->id) != nullptr) == plant->hasAlmanac);
        CHECK(i == 0 || FusionPlantAt(i - 1)->id < plant->id);
        if (plant->hasStats)
        {
            CHECK(std::isfinite(plant->attackIntervalSeconds));
            CHECK(std::isfinite(plant->productionIntervalSeconds));
            CHECK(std::isfinite(plant->rechargeSeconds));
        }
    }
    for (size_t i = 0; i < FusionZombieCount(); ++i)
    {
        const auto* zombie = FusionZombieAt(i);
        CHECK(zombie && FusionFindZombie(zombie->id) == zombie);
        CHECK(i == 0 || FusionZombieAt(i - 1)->id < zombie->id);
    }
    for (size_t i = 0; i < FusionBulletCount(); ++i)
    {
        const auto* bullet = FusionBulletAt(i);
        CHECK(bullet && FusionFindBullet(bullet->id) == bullet);
        CHECK(i == 0 || FusionBulletAt(i - 1)->id < bullet->id);
    }
    CHECK(!FusionPlantAt(FusionPlantCount()));
    CHECK(!FusionZombieAt(FusionZombieCount()));
    CHECK(!FusionBulletAt(FusionBulletCount()));
    CHECK(!FusionFindPlant(-1));
    CHECK(!FusionFindPlant(65535));
    CHECK(!FusionFindPlant(65536));
    const auto* pea = FusionFindPlant(0);
    CHECK(pea && pea->hasStats && pea->sunCost == 100 && pea->maxHealth == 300 && pea->damage == 20);
    CHECK(pea && pea->attackIntervalSeconds == 1.5f && pea->rechargeSeconds == 7.5f);
    const auto* peaSun = FusionFindPlant(1000);
    CHECK(peaSun && peaSun->id == 1000 && peaSun->hasStats && peaSun->sunCost == 150 && peaSun->damage == 100);
    CHECK(peaSun && peaSun->attackIntervalSeconds == 5.0f && peaSun->productionIntervalSeconds == 25.0f);
    CHECK(FusionFindPlant(1192) && FusionFindPlant(1192)->id == 1192);
    for (int unknownStats : {257, 258, 800})
        CHECK(FusionFindPlant(unknownStats) && !FusionFindPlant(unknownStats)->hasStats);

    CHECK(GetFusionRecipeCount() == 410);
    const auto* recipes = GetFusionRecipes();
    for (size_t i = 0; i < GetFusionRecipeCount(); ++i)
    {
        const auto& recipe = recipes[i];
        CHECK(FusionFindPlant(recipe.existing) && FusionFindPlant(recipe.used) && FusionFindPlant(recipe.result));
        CHECK(FindFusionRecipe(recipe.existing, recipe.used) == &recipe);
        if (i)
            CHECK(recipes[i - 1].existing < recipe.existing ||
                (recipes[i - 1].existing == recipe.existing && recipes[i - 1].used < recipe.used));
    }
    const auto* basic = FindFusionRecipe(0, 1);
    CHECK(basic && basic->result == 1000);
    const auto* asymmetric = FindFusionRecipe(6, 0);
    CHECK(asymmetric && asymmetric->result == 1018);
    CHECK(!FindFusionRecipe(0, 6));
    CHECK(!FindFusionRecipe(kFusionNoPlant, 0));
    CHECK(!FindFusionRecipe(0, kFusionNoPlant));
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.Catalog\n" : "[       OK ] Fusion.Catalog\n");
    return result;
}
