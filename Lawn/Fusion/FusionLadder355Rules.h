#pragma once
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
// Source Zombie.ladderTime/lastLadder. Physics is host-only; clients adopt raw X/Y.
struct FusionLadder355State
{
    float seconds=0;
    int lastColumn=-1,lastRow=-1,lastNativeAge=-1;
    bool recovering=false;
};
inline bool FusionLadder355Valid(const FusionLadder355State& s)
{
    const bool noCell=s.lastColumn==-1&&s.lastRow==-1;
    return std::isfinite(s.seconds)&&s.seconds>=0&&s.seconds<=1&&s.lastNativeAge>=-1&&
        (noCell?(s.seconds==0&&!s.recovering):s.lastColumn>=0&&s.lastColumn<10&&s.lastRow>=0&&s.lastRow<6);
}
inline bool BeginFusionLadder355(bool present,int column,int row,FusionLadder355State& s)
{
    if(!present||column<0||column>=10||row<0||row>=6||!FusionLadder355Valid(s))return false;
    if(s.seconds==0&&(s.lastColumn!=column||s.lastRow!=row))
    {s.seconds=1;s.lastColumn=column;s.lastRow=row;s.recovering=true;}
    return true;
}
// True means the source ladder branch owned this native update. Repeated calls
// in one actual Zombie.Update consume no second translation or timer step.
inline bool StepFusionLadder355(FusionLadder355State& s,int nativeAge,float dt,bool& translate)
{
    if(!FusionLadder355Valid(s)||nativeAge<0||!std::isfinite(dt)||dt<0)return false;
    if(s.lastNativeAge==nativeAge){translate=false;return true;}
    if(s.seconds<=0){translate=false;return false;}
    s.seconds=(std::max)(0.f,s.seconds-dt);s.lastNativeAge=nativeAge;translate=true;return true;
}
}
