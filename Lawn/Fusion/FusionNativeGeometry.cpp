#include "FusionNative.h"
#include "../Board.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
int GetNativeFusionRowCount(const Board* b)
{ return b?NativeFusionRowsForBackground(static_cast<uint8_t>(b->mBackground)):kNativeFusionRows; }
double GetNativeFusionPixelsY(const Board* b)
{ return b&&(GetNativeFusionRowCount(b)==6||const_cast<Board*>(b)->StageHasRoof())?85./1.45:100./1.67; }
bool ConfigureNativeFusionBackground(Board* b,uint8_t background)
{
    if(!b||!IsNativeFusionBoard(b)||!NativeFusionBackgroundValid(background))return false;
    if(static_cast<uint8_t>(b->mBackground)==background)return true;
    b->mBackground=static_cast<BackgroundType>(background);
    const int rows=GetNativeFusionRowCount(b);
    for(int row=0;row<MAX_GRID_SIZE_Y;++row)
    {
        const bool water=rows==6&&(row==2||row==3);
        b->mPlantRow[row]=water?PLANTROW_POOL:PLANTROW_NORMAL;
        for(int col=0;col<b->GetColumnCount();++col)
            b->GetGridSquareTypeAt(col,row)=row>=rows?GRIDSQUARE_NONE:water?GRIDSQUARE_POOL:GRIDSQUARE_GRASS;
    }
    b->LoadBackgroundImages();
    return true;
}
}
