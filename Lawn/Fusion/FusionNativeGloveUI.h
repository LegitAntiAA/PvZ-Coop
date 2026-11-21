#ifndef LAWN_FUSION_NATIVE_GLOVE_UI_H
#define LAWN_FUSION_NATIVE_GLOVE_UI_H

#include "FusionNative.h"

class Board;
class Plant;

namespace LawnFusion
{
// Local input intent only. The authoritative hand lives in gloveHolds and the
// real Plant remains active on the lawn while its preview is held.
class NativeGloveUIState
{
public:
    uint32_t epoch = 0;
    int localSlot = -1;
    bool clearing = false;
    bool suppressLocalHold = false;
    NativeGloveHold ignoredHold{};
    NativeGloveHold requestedPickup{};
};

bool HandleNativeGlovePointer(Board* board, int button, int x, int y);
bool ToggleNativeGloveFromKeyboard(Board* board);
void SyncNativeGloveCursors(Board* board);
void OnNativeGloveCursorClearing(Board* board);
// Resolves a host instance token to the actual local entity, including ghosts.
Plant* GetNativeGloveHeldPlant(Board* board, int slot);
}

#endif
