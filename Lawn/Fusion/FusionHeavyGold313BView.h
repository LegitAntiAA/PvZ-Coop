#pragma once
#include <cstdint>
namespace LawnFusion
{
// Only persistent source appearance/status; native graph clocks wire separately.
// No awards, RNG progress, event cursors, local generations or native handles.
struct FusionHeavyGold313BView
{uint8_t status=4,firstStage=0,secondStage=0;bool operator==(const FusionHeavyGold313BView&)const=default;};
inline bool ValidateFusionHeavyGold313BView(uint16_t kind,const FusionHeavyGold313BView& v)
{if(kind!=203&&kind!=223)return v==FusionHeavyGold313BView{};return(v.status==1||v.status==4||v.status==5||v.status==6)&&v.firstStage<=2&&v.secondStage<=2&&(kind!=203||v.firstStage==0);}
}
