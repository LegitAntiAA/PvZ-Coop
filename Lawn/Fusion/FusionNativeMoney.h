#ifndef LAWN_FUSION_NATIVE_MONEY_H
#define LAWN_FUSION_NATIVE_MONEY_H
#include <cstdint>
class Board;
class Coin;
class Plant;
namespace LawnFusion
{
// x/y is the native Board payout center; existing Coin origin is center-20.
bool EmitNativeMoneyItem(Board* board, float x, float y, uint16_t sourceItem);
// One whole callback: capacity checked before the independent reward draws.
bool EmitNativeMoneyCoins(Plant* plant, int count);
// Called after Coin.Die. True also handles client/invalid/already-claimed
// Fusion money so it can never fall into Classic's persistent-profile credit.
bool CreditNativeMoneyCoin(Coin* coin);
}
#endif
