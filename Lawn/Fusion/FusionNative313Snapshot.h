#pragma once
#include "FusionSourceGraph313.h"
#include "FusionWaterZombie313Rules.h"
#include "FusionExtendedZombie313Rules.h"
#include "FusionNativeHeavyZombie313B.h"
#include "FusionHeavyGold313BView.h"
#include <array>
#include <cmath>
namespace LawnFusion
{
struct NativeSourceGraph313Snapshot
{
    uint8_t layerCount=0;
    std::array<FusionSourceGraph313View,3> layers{};
    std::array<LawnNet::NetZombieAnimState,2> auxiliary{};
};
struct NativeHeavyZombie313Snapshot
{FusionHeavyZombie313BView actor;FusionHeavyGold313BView gold;};
inline bool Native313AnimationZero(const LawnNet::NetZombieAnimState& v)
{return !v.mPresent&&!v.mDead&&!v.mFrameStart&&!v.mFrameCount&&v.mAnimTime==0&&v.mAnimRate==0&&!v.mLoopType&&!v.mLoopCount;}
inline bool Native313AnimationViewValid(const LawnNet::NetZombieAnimState& v)
{return v.mPresent&&!v.mDead&&v.mFrameCount&&uint32_t(v.mFrameStart)+v.mFrameCount<=65535&&
    std::isfinite(v.mAnimTime)&&v.mAnimTime>=0&&v.mAnimTime<=1&&std::isfinite(v.mAnimRate)&&std::abs(v.mAnimRate)<=10000&&v.mLoopType<=5;}
inline bool Native313GraphLayerZero(const FusionSourceGraph313View& v)
{return !v.state&&!v.outgoing&&v.outgoingSeconds==0&&v.fadeElapsed==0&&v.fadeDuration==0&&!v.fadeNormalized;}
inline bool Native313GraphViewValid(uint16_t kind,const NativeSourceGraph313Snapshot& v)
{
    const auto* graph=FindFusionSourceGraph313(kind);
    if(!graph||v.layerCount<1||v.layerCount>3||v.layerCount!=graph->layerCount)return false;
    for(uint8_t layer=0;layer<3;++layer)
        if(layer<v.layerCount?!FusionSourceGraph313ViewValid(*graph,layer,v.layers[layer]):!Native313GraphLayerZero(v.layers[layer]))return false;
    for(uint8_t aux=0;aux<2;++aux)
        if(aux+1<v.layerCount?!Native313AnimationViewValid(v.auxiliary[aux]):!Native313AnimationZero(v.auxiliary[aux]))return false;
    return true;
}
inline bool Native313WaterViewValid(uint16_t kind,const FusionWaterZombie313View& v)
{return FusionWaterZombie313Kind(kind)&&FusionWaterZombie313WireViewValid(kind,v)&&
    (kind==20?Native313AnimationZero(v.auxiliary):Native313AnimationViewValid(v.auxiliary))&&
    (FusionWaterShooter343Kind(kind)?Native313AnimationViewValid(v.shootClock343):Native313AnimationZero(v.shootClock343));}
inline bool Native313ExtendedViewValid(uint16_t kind,const FusionExtendedZombie313View& v)
{return FusionExtendedZombie313ViewValid(kind,v)&&(FusionExtendedZombie313OwnBodyDamage(kind)||v.bodyStage==0);}
inline bool Native313HeavyViewValid(uint16_t kind,const NativeHeavyZombie313Snapshot& v)
{return ValidateFusionHeavyZombie313BView(kind,v.actor)&&ValidateFusionHeavyGold313BView(kind,v.gold);}

// Unit-data constructor only: no actor/R/cache/resource access or claimed raw
// track equivalence. Root replaces auxiliary metadata with captured real R.
inline bool MakeNative313GraphFixture(uint16_t kind,NativeSourceGraph313Snapshot& out)
{
    const auto* graph=FindFusionSourceGraph313(kind);if(!graph||!graph->layerCount||graph->layerCount>3)return false;
    NativeSourceGraph313Snapshot v;v.layerCount=graph->layerCount;
    for(uint8_t layer=0;layer<v.layerCount;++layer){v.layers[layer].state=graph->layers[layer].defaultState;v.layers[layer].outgoing=v.layers[layer].state;
        if(layer){auto& r=v.auxiliary[layer-1];r.mPresent=true;r.mFrameCount=1;}}
    if(!Native313GraphViewValid(kind,v))return false;out=v;return true;
}
}
