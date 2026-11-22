#include "FusionNative.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
int GetNativeFusionRowCount(const Board* b)
{ return b?NativeFusionRowsForBackground(static_cast<uint8_t>(b->mBackground)):kNativeFusionRows; }
double GetNativeFusionPixelsY(const Board* b)
{ return b&&(GetNativeFusionRowCount(b)==6||const_cast<Board*>(b)->StageHasRoof())?85./1.45:100./1.67; }
float GetNativeFusionRoofRowY350(const Board* b,float x,int row)
{return float(FusionRoof350RowTop(const_cast<Board*>(b)->GetCellCenterX(0,row),x,row));}
int GetNativeFusionPixelToRow350(const Board* b,int x,int y,bool keep)
{
    if(!b)return -1;
    if(!const_cast<Board*>(b)->StageHasRoof())return NativeFusionPixelToRow(uint8_t(b->mBackground),y,keep);
    const int actualX=keep?(std::max)(LAWN_XMIN,(std::min)(LAWN_XMIN+b->GetColumnCount()*b->GetCellPitchX()-1,x)):x;
    return FusionRoof350PixelToRow(const_cast<Board*>(b)->GetCellCenterX(0,0),actualX,y,keep);
}
bool GetNativeFusionRoofZombieRowY350(const Zombie* z,float nextX,int row,float& out)
{
    const auto* b=z?z->mBoard:nullptr;
    if(!b||!IsNativeFusionBoard(b)||!const_cast<Board*>(b)->StageHasRoof()||
       !GetNativeSourceZombieData(z)||!z->mApp||!z->mApp->ReanimationTryToGet(z->mBodyReanimID)||
       !std::isfinite(nextX)||row<0||row>=GetNativeFusionRowCount(b))return false;
    float shadowX=0,shadowY=0;
    // Native GetShadowCenter/GetDrawPos do not call GetPosYBasedOnRow. Query
    // the real carrier/facing/scale/body-offset basis, never assume +40.
    if(!const_cast<Zombie*>(z)->GetShadowCenter(shadowX,shadowY))return false;
    float value=GetNativeFusionRoofRowY350(b,shadowX+nextX-z->mPosX,row)-30;
    if(z->mZombieType==ZOMBIE_BALLOON)value-=30;
    else if(z->mZombieType==ZOMBIE_POGO)value-=16;
    if(!std::isfinite(value))return false;out=value;return true;
}
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
