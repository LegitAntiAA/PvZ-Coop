#include "FusionRoof350Tests.h"
#include "../../Lawn/Fusion/FusionRoof350Rules.h"
#include "../../Lawn/Fusion/FusionCampaign272Rules.h"
#include "../../Lawn/Fusion/FusionNative.h"
#include <cmath>
#include <cstdio>
#include <limits>

FusionRoof350TestResult RunFusionRoof350Tests()
{
    using namespace LawnFusion;FusionRoof350TestResult r;
#define CHECK(v) do{++r.mChecks;if(!(v)){++r.mFailures;std::printf("Roof350 failed line%d: %s\n",__LINE__,#v);}}while(0)
    const double first=LAWN_XMIN+kNativeFusionCellPitchX*.5;
    const double breakX=first+(1.5+4.8)*kFusionBasePixelsX;
    CHECK(first==76&&std::abs(breakX-412)<1e-10); // Actual current engine constants, not the old assumed80px origin.
    CHECK(std::abs(FusionRoof350Offset(first,first)-81.24827586206897)<1e-9);
    CHECK(std::abs(FusionRoof350Offset(first,first+4*72)-11.60689655172414)<1e-9);
    CHECK(std::abs(FusionRoof350Offset(first,breakX))<1e-9&&FusionRoof350Offset(first,breakX+100)==0);
    CHECK(std::abs((FusionRoof350Offset(first,first)-FusionRoof350Offset(first,first+1))-.2418103448275862)<1e-10);
    for(int col=0;col<10;++col)for(int row=0;row<5;++row)
    {
        const double x=first+col*kNativeFusionCellPitchX;
        const double sx=-4.8+col*1.35;
        const double land=FusionRoof350SourceLandY(sx,row);
        const double top=FusionRoof350RowTop(first,x,row);
        CHECK(std::isfinite(land)&&std::isfinite(top));
        CHECK(std::abs(top-(LAWN_YMIN-10+(2.3-land)*(85./1.45)))<1e-9);
        const int y=int(top);
        for(int dy:{0,1,42,84})
        {CHECK(FusionRoof350PixelToRow(first,x,y+dy)==row);CHECK(FusionRoof350PixelToRow(first,x,y+dy,true)==row);}
        CHECK(FusionRoof350PixelToRow(first,x,y-1)==row-1);
        CHECK(FusionRoof350PixelToRow(first,x,y+85)==(row==4?-1:row+1));
        // Row widths are a continuous sloped projection; source cells remain
        // 1.35 units apart and85px tall, with no repeated strip geometry.
        CHECK(std::abs(FusionRoof350RowTop(first,x,row+1)-top-85)<1e-10);
    }
    for(int x=LAWN_XMIN;x<LAWN_XMIN+10*kNativeFusionCellPitchX;++x)
    {
        const double top=FusionRoof350RowTop(first,x,0);const int y=int(top);
        CHECK(FusionRoof350PixelToRow(first,x,y-1)==-1&&FusionRoof350PixelToRow(first,x,y-1,true)==0);
        CHECK(FusionRoof350PixelToRow(first,x,y+425)==-1&&FusionRoof350PixelToRow(first,x,y+425,true)==4);
        for(int row=0;row<5;++row)CHECK(FusionRoof350PixelToRow(first,x,y+row*85+42)==row);
    }
    const auto nan=(std::numeric_limits<double>::quiet_NaN)();
    CHECK(std::isnan(FusionRoof350Offset(first,nan)));
    CHECK(FusionRoof350PixelToRow(first,nan,200,true)==-1&&FusionRoof350PixelToRow(first,100,nan,true)==-1);
    CHECK(FusionRoof350PixelToRow(first,100,-1e100,true)==0&&FusionRoof350PixelToRow(first,100,1e100,true)==4);
    int total=0;
    for(int level=1;level<=45;++level)
    {
        const auto* source=FindFusionSourceAdventure272(level);CHECK(source!=nullptr);if(!source)continue;
        if(level<37){CHECK(!source->roofPreplantColumns);continue;}
        CHECK(source->scene==4&&source->rows==5&&source->columns==10);
        CHECK(source->roofPreplantColumns==(level==37?5:3));
        total+=source->roofPreplantColumns*source->rows;
    }
    CHECK(total==145); // One25-Pot opening and eight15-Pot openings.
    // A true carrier shadow center need not be native posX+40. A facing or
    // body-offset change must select its floor at the real point, not shift the
    // incoming source phase/clock to compensate for a wrong ground query.
    for(double shadowOffset:{11.,23.,35.,52.,80.})
    {
        const double pos=200,shadow=pos+shadowOffset,next=pos+7;
        CHECK(std::abs(FusionRoof350RowTop(first,shadow+next-pos,2)-FusionRoof350RowTop(first,next+shadowOffset,2))<1e-10);
        CHECK(std::abs(FusionRoof350RowTop(first,shadow,2)-FusionRoof350RowTop(first,pos+40,2))>1);
    }
#undef CHECK
    return r;
}
