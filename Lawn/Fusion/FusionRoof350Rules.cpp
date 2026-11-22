#include "FusionRoof350Rules.h"
#include "../../GameConstants.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
double FusionRoof350SourceLandY(double x,int row)
{
    if(!std::isfinite(x))return (std::numeric_limits<double>::quiet_NaN)();
    return 2.3-double(row)*1.45+(x<=1.5?x*.22-.33:0);
}
double FusionRoof350Offset(double first,double x)
{
    if(!std::isfinite(first)||!std::isfinite(x))return (std::numeric_limits<double>::quiet_NaN)();
    const double sx=(x-first)/kFusionBasePixelsX-4.8;
    return (2.3-FusionRoof350SourceLandY(sx,0))*(85./1.45);
}
double FusionRoof350RowTop(double first,double x,int row)
{return LAWN_YMIN-10.+85.*row+FusionRoof350Offset(first,x);}
int FusionRoof350PixelToRow(double first,double x,double y,bool keep)
{
    const double offset=FusionRoof350RowTop(first,x,0);
    if(!std::isfinite(offset)||!std::isfinite(y))return -1;
    // GridToPixelY is the native integer tile API. Use its same pixel boundary;
    // retaining the fractional continuous floor here makes its own top pixel
    // select the preceding row at most source slope positions.
    const double row=std::floor((y-std::floor(offset))/85.);
    if(row<0)return keep?0:-1;
    if(row>=5)return keep?4:-1;
    return int(row);
}
}
