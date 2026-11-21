#ifndef LAWN_FUSION_NATIVE_TOOLS268_H
#define LAWN_FUSION_NATIVE_TOOLS268_H
#include <cstdint>
class Board;class Coin;
namespace LawnFusion
{
struct FusionCommand;
Coin* GetNativeEquipmentClaimedBy(Board* board,int slot);
Coin* GetNativeFertilize269ClaimedBy(Board* board,int slot);
void NativeFertilize269CoinRemoving(Coin* coin);
bool HandleNativeTools268Pointer(Board* board,int button,int boardX,int boardY);
bool HandleNativeTools268Command(Board* board,int slot,const FusionCommand& command);
void SyncNativeTools268Cursors(Board* board);
void OnNativeTools268CursorClearing(Board* board);
// Same local/remote Board coordinates as the established co-op cursor path.
bool GetNativeEquipmentCursorPosition(Board* board,uint8_t slot,float& x,float& y);
}
#endif
