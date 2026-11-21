#ifndef LAWN_FUSION_MONEY_RULES_H
#define LAWN_FUSION_MONEY_RULES_H
#include "../../ConstEnums.h"
#include <cstdint>
namespace LawnFusion
{
constexpr int kFusionMoneyMaximum = 99999;
constexpr int kFusionMoneyInitial = 3000; // Source Board.ctor0x180326400, field+D8.
bool FusionMoneyValueValid(int value);
bool FusionMoneyCoinMetadataValid(CoinType type, int value);
bool TryGetFusionMoneyItem(uint16_t sourceItem, CoinType& type, int& value);
bool TryGetFusionMarigoldReward(int sourceDraw, uint16_t& sourceItem, int& value);
bool TryCreditFusionMoney(int before, int value, int& after);
}
#endif
