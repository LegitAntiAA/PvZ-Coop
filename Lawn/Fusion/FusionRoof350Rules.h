#pragma once
#include "FusionBasePlantRules.h"

namespace LawnFusion
{
// Mouse.GetLandY1803C31E0, GetBoxX1803C30A0 and GetBoxY1803C30C0.
// Native flat-roof origin/85px row pitch are the existing PvZ projection;
// only the source height delta is converted through its actual1.45 row units.
double FusionRoof350SourceLandY(double sourceX,int row);
double FusionRoof350Offset(double firstColumnCenterX,double boardX);
double FusionRoof350RowTop(double firstColumnCenterX,double boardX,int row);
int FusionRoof350PixelToRow(double firstColumnCenterX,double boardX,double boardY,bool keepOnBoard=false);
}
