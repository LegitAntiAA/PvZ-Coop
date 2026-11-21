#ifndef LAWN_FUSION_ECONOMY_RULES_H
#define LAWN_FUSION_ECONOMY_RULES_H
#include <cstdint>
namespace LawnFusion
{
constexpr int kFusionSunShroomGrowthTicks=12001;
constexpr int kFusionGoldSunflowerInitialTicks=501;
constexpr int kFusionGoldSunflowerCycleTicks=1001;
constexpr uint32_t kFusionGoldSunflowerMaximumCharges=200;
bool IsFusionEconomyBatchKind(uint16_t kind);
bool FusionEconomyPlantStateValid(uint16_t kind,uint32_t count,uint16_t rage,uint16_t bag,uint8_t mode);
// Source Money dictionary and fees. Count includes the corresponding ultimate
// forms; Travel's separate fee cap is intentionally not part of this baseline.
bool TryGetFusionMoneyUpgrade(uint16_t source,int existingGoldCount,uint16_t& result,int& fee);
bool TryGetFusionMoneySkillFee(uint16_t kind,int& fee);
struct FusionMoneyActionQuote
{
    uint16_t resultKind=0xffff;
    int fee=0;
    bool upgrade=false;
};
// Only ReinforcePlant's actual source whitelist participates. GetSuperSkillCost's
// default1000 is not permission to activate the tool on ordinary plants.
bool TryGetFusionMoneyActionQuote(uint16_t kind,int existingGoldCount,FusionMoneyActionQuote& quote);
bool IsFusionMoneyUpgradeAncestry(uint16_t first,uint16_t second,uint16_t result);
int FusionMoneySpendSunCount(int fee,bool isIZ);
}
#endif
