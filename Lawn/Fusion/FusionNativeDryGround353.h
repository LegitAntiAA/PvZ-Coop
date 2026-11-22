#pragma once
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "../Board.h"
namespace LawnFusion
{
inline bool FusionDryRow353(int rows,int row,bool normal)
{return (rows==5||rows==6)&&row>=0&&row<rows&&normal;}
// World validation retains epoch, exact imported terrain, TD and grid-item
// policy. A dry controller is not restricted by the board's total row count.
inline bool NativeSourceDryRow353(const Board* b,int row)
{
    return IsNativeSourceWorld313(b)&&row>=0&&row<GetNativeFusionRowCount(b)&&
        FusionDryRow353(GetNativeFusionRowCount(b),row,b->mPlantRow[row]==PLANTROW_NORMAL);
}
inline float NativeSourceRowPitch353(const Board* b)
{return float(GetNativeFusionPixelsY(b)*((GetNativeFusionRowCount(b)==6||const_cast<Board*>(b)->StageHasRoof())?1.45:1.67));}
}
