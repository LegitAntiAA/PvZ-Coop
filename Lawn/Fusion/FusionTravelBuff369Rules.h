#pragma once
#include "FusionTravelBuff360Rules.h"
#include <array>
#include <cstdint>

namespace LawnFusion {
struct FusionTravelPlantAdvanced369 {uint16_t kind;uint8_t first,second;};
// TravelMgr.cctor: source dictionary, including its deliberate shared22.
inline constexpr std::array<FusionTravelPlantAdvanced369,12> kFusionTravelPlantAdvanced369={{{900,0,1},{907,2,3},{905,4,5},{906,6,7},{919,15,31},{908,17,18},{936,21,22},{916,22,23},{926,34,35},{928,38,39},{932,41,42},{937,46,47}}};
bool GetFusionTravelPlantAdvanced369(uint16_t,FusionTravelPlantAdvanced369&);
// Plant.AdjustDamage462C70 adds modifiers before ONE catalog multiply/truncate.
bool TryFusionTravelPlantAttack369(int catalogDamage,bool advanced10,bool connectedAdvanced33,int&);
// Plant.GetDamage464C30: positive1..3 ->1; >=4 truncates half.
bool TryFusionTravelConnectedDamage369(int raw,bool connectedAdvanced33,int&);
// Board.GetSun317B30 multiplies the ENTIRE sum by r; advanced25 only cap.
bool TryFusionTravelSun369(int before,int count,int multiplier,bool advanced25,int&);
// CoinMoney.MoveToPosition4560F0 tests money BEFORE adding the coin. It does
// not convert overflow from the coin which first crosses the money cap.
bool TryFusionTravelMoney369(int before,int value,bool advanced22,int& money,int& sun);
// Actual timers advance once on their existing native owner. No new clock.
int FusionTravelCardStep369(bool advanced27);
int FusionTravelHammerStep369(bool advanced26);
float FusionTravelImitaterSeconds369(uint16_t kind,float ordinary,bool advanced32,bool playing);
int FusionTravelLight369(bool advanced20);
// Exact source operation order: catalog/additive truncation first, then the
// SnowGatlingPuff-only1.5 modifier. It is never a general projectile multiplier.
bool TryFusionTravelProjectileDamage369(int unmodified,uint16_t projectileKind,bool advanced10,bool snowGatling17,int&);
bool FusionTravelProjectilePair369(int unmodified,int actual,uint16_t projectileKind,bool allow10,bool allow17,uint8_t fire=0);
// Repeated source Emerald relays apply the SAME F32 multiplier/truncation to
// both captured channels. Exact reverse operations, never an epsilon/range.
bool FusionTravelRelayedProjectilePair369(int unmodified,int actual,uint16_t projectileKind,bool allow10,bool allow17,bool allow24);
}
