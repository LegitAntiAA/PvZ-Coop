#pragma once
#include "FusionSourceGraph313.h"
namespace LawnFusion
{
// Genuine root local-X Hermite/step integral incl original root scale, exactly
// one loop displacement per crossed loop. Caller applies reflection/Board basis.
bool SourceRootDelta313(uint16_t,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
