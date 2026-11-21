#include "FusionMoneyRules.h"
namespace LawnFusion
{
namespace
{
struct Item { uint16_t source; CoinType type; int value; };
constexpr Item items[] = {
    {34,COIN_SILVER,100}, {35,COIN_GOLD,300}, {36,COIN_DIAMOND,1000},
    {38,COIN_SILVER,25}, {39,COIN_GOLD,100}};
}
bool FusionMoneyValueValid(int value)
{ for (const auto& item : items) if (item.value == value) return true; return false; }
bool FusionMoneyCoinMetadataValid(CoinType type, int value)
{ for (const auto& item : items) if (item.type == type && item.value == value) return true; return false; }
bool TryGetFusionMoneyItem(uint16_t source, CoinType& type, int& value)
{
    for (const auto& item : items) if (item.source == source) { type = item.type; value = item.value; return true; }
    return false;
}
bool TryGetFusionMarigoldReward(int draw, uint16_t& source, int& value)
{
    if (draw < 0 || draw >= 100) return false;
    const uint16_t item = draw < 20 ? 35 : draw > 94 ? 36 : 34;
    CoinType type;
    if (!TryGetFusionMoneyItem(item,type,value)) return false;
    source = item; return true;
}
bool TryCreditFusionMoney(int before, int value, int& after)
{
    if (before < 0 || before > kFusionMoneyMaximum || !FusionMoneyValueValid(value)) return false;
    after = before > kFusionMoneyMaximum-value ? kFusionMoneyMaximum : before+value;
    return true;
}
}
