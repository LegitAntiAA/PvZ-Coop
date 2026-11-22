#pragma once
#include <cstdint>
#include <cmath>
namespace LawnFusion
{
enum class FusionFogLight357:uint8_t {None,Small,Lantern};
inline FusionFogLight357 FusionFogLightKind357(uint16_t kind)
{
    switch(kind)
    {
    case 18:case 902:case 909:case 932:case 1052:case 1053:case 1055:case 1056:case 1059:case 1061:
    case 1079:case 1080:case 1081:case 1100:case 1104:case 1178:case 1182:return FusionFogLight357::Small;
    case 20:case 937:case 1078:case 1087:case 1185:return FusionFogLight357::Lantern;
    default:return FusionFogLight357::None;
    }
}
inline bool FusionFogLightRange357(FusionFogLight357 light,int plantCol,int plantRow,int fogCol,int fogRow)
{
    if(plantCol<0||plantCol>=10||plantRow<0||plantRow>=6||fogCol<0||fogCol>11||fogRow<0||fogRow>=6)return false;
    const int dx=std::abs(plantCol-fogCol),dy=std::abs(plantRow-fogRow);
    return light==FusionFogLight357::Small?((dy==0&&dx<2)||(dx==0&&dy<2)):
        light==FusionFogLight357::Lantern&&((dy<=1&&dx<4)||(dy==2&&dx<3));
}
// Resampling native fog-tile centers onto the actual Fusion lawn grid. No
// clamp: an off-lawn tile cannot acquire a boundary plant's light spuriously.
inline int FusionFogSampleCell357(float center,float firstCenter,float pitch)
{
    if(!std::isfinite(center)||!std::isfinite(firstCenter)||!std::isfinite(pitch)||pitch<=0)return -1000;
    const float cell=std::floor((center-firstCenter)/pitch+.5f);
    return std::isfinite(cell)&&cell>-1000&&cell<1000?int(cell):-1000;
}
}
