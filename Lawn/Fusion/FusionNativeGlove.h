#ifndef LAWN_FUSION_NATIVE_GLOVE_H
#define LAWN_FUSION_NATIVE_GLOVE_H

class Board;
class Plant;
namespace LawnFusion
{
struct FusionCommand;
bool HandleNativeGloveCommand(Board* board, int slot, const FusionCommand& command);
void UpdateNativeGloveState(Board* board);
void CancelNativeGlove(Board* board, int slot);
bool IsNativeGlovePlantReserved(const Board* board, const Plant* plant, int exceptSlot);
}

#endif
