#pragma once
#include "FusionNativeCharred311.h"
#include <tuple>
#include <limits>
namespace LawnFusion
{
// Four genuine finite source models. Actual once-native-R owns elapsed time;
// this table only maps its normalized phase to the source sampler.
struct SourceAnimated317Spec
{
    const char* package;const char* prefab;const char* state;
    float clipSeconds,stateSpeed,holdSeconds,rootScale,drawMultiplier;
    uint16_t nodes,sprites;bool shadowAnchor;
    constexpr float PlaySeconds()const{return clipSeconds/stateSpeed;}
    constexpr float LifeSeconds()const{return PlaySeconds()+holdSeconds;}
};
inline const SourceAnimated317Spec* FindSourceAnimated317Spec(uint8_t variant)
{
    static constexpr SourceAnimated317Spec rows[]={
        {"charred311-1","Resources/zombies/charred/Zombie_Charred.prefab","Cherred",.7f,.25f,.2f,.31f,1,22,21,true},
        {"charred313-2","Resources/zombies/charred/gargantuar/Gargantuar_charred.prefab","Ruins",1.9166666f,.8f,.2f,.85f,1,23,22,true},
        {"source-animated317-3","Resources/particle/anim/water/WaterSplashPrefab.prefab","idle",.8333333f,1,0,.3f,4.f/3.f,5,4,false},
        {"source-animated317-4","Resources/particle/prefabs/MindControl.prefab","mindControl",.8333333f,1,0,1,1,21,20,false}};
    return variant>=1&&variant<=4?&rows[variant-1]:nullptr;
}
inline bool SourceAnimated317SampleSeconds(uint8_t variant,float normalized,float& output)
{
    const auto* spec=FindSourceAnimated317Spec(variant);
    if(!spec||!std::isfinite(normalized)||normalized<0||normalized>1)return false;
    const float seconds=normalized*spec->LifeSeconds();output=seconds<spec->PlaySeconds()?seconds:spec->PlaySeconds();return true;
}
inline auto SourceAnimated317SeenKey(const SourceCharred311Birth& v)
{return std::tuple{v.ownerNetID,v.ownerToken,v.variant,v.eventSerial};}
inline bool TryNextSourceAnimated317Serial(uint32_t current,uint32_t& output)
{
    if(current==(std::numeric_limits<uint32_t>::max)())return false;
    output=current+1;return true;
}
bool EnsureNativeSourceAnimated317(Board*,uint8_t variant,size_t wholePeak=1,std::string* error=nullptr);
// HOST only, AFTER its real controller callback consumed the semantic action.
// Input worldX/Y are genuine Board pixels supplied by the source projection.
// The owner is read only to build the immutable event, then may be retired.
// FX3/4 allocate a monotonic context serial, without a gameplay RNG/clock.
bool EmitNativeSourceAnimated317(Zombie*,uint8_t variant,float worldX,float worldY,int order,std::string* error=nullptr);
}
