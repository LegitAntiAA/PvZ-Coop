#include "FusionNativeAdventureProgress273.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeRemainingLeaves269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <cmath>

namespace LawnFusion
{
bool PreflightNativeFusionAdventureLesson284Effects(Board* board,int level,std::string* error)
{
    if(error)error->clear();
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    if(!board||!board->mApp||board->mApp->mBoard!=board||board->mApp->NetIsClient()||!IsNativeFusionBoard(board))
        return reject("Source lesson requires its current authoritative native Board.");
    if(level==15)return true; // Six plants/Cones reuse their existing accepted effect families.
    if(level!=13)return reject("Unknown source lesson effect contract.");
    return CanCreateNativeRemaining269Fertilize(board,2,error)&&EnsureNativeSourceEffect269(board,16,error)&&
        (LawnNet::NetSourceSound284Ready(board->mApp,66)||reject("Genuine source Sound66/prize unavailable."));
}
bool EmitNativeFusionAdventureLesson284Fertilize(Plant* plant,std::string* error)
{
    if(error)error->clear();const auto reject=[&](const char* why){if(error)*error=why;return false;};
    auto* board=plant?plant->mBoard:nullptr;const auto* identity=GetNativePlantData(plant);
    if(!board||!identity||!plant->mApp||plant->mApp->mBoard!=board||plant->mApp->NetIsClient()||
        plant->mNetGhost||plant->mDead||!plant->mIsOnBoard||!CanApplyNativeFusionGameplay(board)||
        board->mPlants.DataArrayTryToGet(identity->instanceToken)!=plant)
        return reject("Source Fertilize grant plant generation unavailable.");
    float x=0,y=0;
    if(!CanCreateNativeRemaining269Fertilize(board,1,error)||!EnsureNativeSourceEffect269(board,16,error)||
        !LawnNet::NetSourceSound284Ready(plant->mApp,66)||!GetNativeSourcePlantNodePoint(plant,"",x,y)||!std::isfinite(x+y))
        return reject("Source Fertilize grant resource or actual plant ROOT unavailable.");
    // TutorUpdate13 instantiates the real Ferilize at transform.position,
    // identity rotation/Board parent, then plays66(.5,1). The existing Coin
    // factory owns its source velocity/RNG, lifetime, native pickup and FNS.
    if(!CreateNativeRemaining269Fertilize(plant,x,y))return reject("Source Fertilize native Coin factory failed.");
    if(!LawnNet::NetPlaySourceSound284(plant->mApp,66,.5f))return reject("Source Sound66 failed after item grant.");
    return true;
}
}
