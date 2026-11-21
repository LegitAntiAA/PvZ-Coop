#ifndef LAWN_FUSION_NATIVE_MONEY_ACTION_H
#define LAWN_FUSION_NATIVE_MONEY_ACTION_H
class Board;
class Plant;
namespace LawnFusion
{
bool TryNativeMoneyPlantAction(Plant* plant,int slot);
// Read-only common eligibility, price and cooldown check for local hover.
// Client ghosts are valid preview targets; only TryNativeMoneyPlantAction mutates.
bool IsNativeMoneyTargetApplicable(const Plant* plant,int slot);
bool HandleNativeMoneyPointer(Board* board,int button,int x,int y);
bool ToggleNativeMoneyFromKeyboard(Board* board);
}
#endif
