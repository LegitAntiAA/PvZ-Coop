#include "FusionNativeUmbrella.h"
#include "FusionNative.h"
#include "FusionUmbrellaBatchRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
bool Current(const GameObject* object)
{
    return object && object->mApp && object->mBoard && object->mApp->mBoard == object->mBoard &&
        IsNativeFusionBoard(object->mBoard);
}
bool Host(const GameObject* object)
{ return Current(object) && !object->mNetGhost && !object->mApp->NetIsClient(); }
bool Playing(const Board* board)
{
    const auto* context = GetNativeContext(board);
    return context && context->selection.State().phase == FusionBankPhase::Playing &&
        board->mApp->mGameScene == SCENE_PLAYING;
}
uint32_t Generation(const Projectile* projectile)
{
    if (!Current(projectile)) return 0;
    const auto id = projectile->mBoard->mProjectiles.DataArrayGetID(const_cast<Projectile*>(projectile));
    return projectile->mBoard->mProjectiles.DataArrayTryToGet(id) == projectile ? id : 0;
}
uint32_t Generation(const Plant* plant)
{
    if (!Current(plant)) return 0;
    const auto id = plant->mBoard->mPlants.DataArrayGetID(const_cast<Plant*>(plant));
    return plant->mBoard->mPlants.DataArrayTryToGet(id) == plant ? id : 0;
}
bool Live(const Plant* plant)
{ return Current(plant) && !plant->mDead && plant->mIsOnBoard && plant->mPlantHealth > 0 && Generation(plant); }
bool Umbrella(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return Live(plant) && data && IsFusionUmbrellaBatchKind(data->kind);
}
bool KnownAxis(NativeUmbrellaVerticalAxis axis)
{ return axis == NativeUmbrellaVerticalAxis::PlanarY || axis == NativeUmbrellaVerticalAxis::LobZ; }
bool Incoming(Projectile* projectile, Plant* attacked, const NativeUmbrellaProjectileView* view)
{
    const auto* target = GetNativePlantData(attacked);
    return Host(projectile) && Host(attacked) && !projectile->mDead && Live(attacked) && target && view &&
        !projectile->mBoard->mPaused && Playing(projectile->mBoard) &&
        attacked->mBoard == projectile->mBoard && view->generation && view->generation == Generation(projectile) &&
        view->bulletRow == projectile->mRow && KnownAxis(view->verticalAxis) &&
        FusionFindBullet(view->sourceKind) && FusionUmbrellaCanIntercept(view->sourceKind,view->sourceWay,
            view->isZombieBullet,view->hitLatched,view->bulletRow,target->kind,
            attacked->mPlantCol,attacked->mRow,attacked->mPlantCol,attacked->mRow,true,1.0f);
}
bool Eligible(Plant* umbrella, Plant* attacked, const NativeUmbrellaProjectileView& view,
    NativeUmbrellaEligibility eligibility)
{
    float animatorSpeed = 0;
    const auto* target = GetNativePlantData(attacked);
    const auto* data = GetNativePlantData(umbrella);
    return eligibility && Host(umbrella) && Umbrella(umbrella) && umbrella->mBoard == attacked->mBoard &&
        data->instanceToken == Generation(umbrella) && eligibility(umbrella,animatorSpeed) &&
        FusionUmbrellaCanIntercept(view.sourceKind,view.sourceWay,view.isZombieBullet,view.hitLatched,
            view.bulletRow,target->kind,attacked->mPlantCol,attacked->mRow,umbrella->mPlantCol,
            umbrella->mRow,!umbrella->mIsAsleep,animatorSpeed);
}
FusionUmbrellaAdvance Cancel(FusionUmbrellaBlockState& state)
{
    float x = 0, y = 0;
    return AdvanceFusionUmbrellaBlock(state,0,false,false,x,y);
}
}
Plant* FindNativeUmbrellaForTarget(Projectile* projectile, Plant* attacked,
    const NativeUmbrellaProjectileView* view, NativeUmbrellaEligibility eligibility)
{
    if (!Incoming(projectile,attacked,view) || !eligibility) return nullptr;
    auto* board = attacked->mBoard;
    // Native deterministic cell/layer order; equivalence of Unity's first
    // choice when several identical umbrellas overlap is not asserted.
    for (int row = (std::max)(0,attacked->mRow-1); row <= (std::min)(GetNativeFusionRowCount(board)-1,attacked->mRow+1); ++row)
        for (int column = (std::max)(0,attacked->mPlantCol-1); column <= (std::min)(board->GetColumnCount()-1,attacked->mPlantCol+1); ++column)
        {
            PlantsOnLawn layers{}; board->GetPlantsOnLawn(column,row,&layers);
            Plant* candidates[] = {layers.mNormalPlant,layers.mUnderPlant,layers.mPumpkinPlant};
            for (auto* plant : candidates) if (Eligible(plant,attacked,*view,eligibility)) return plant;
        }
    return nullptr;
}
bool TryNativeUmbrellaIntercept(Projectile* projectile, Plant* attacked, Plant* umbrella,
    const NativeUmbrellaProjectileView* view, NativeUmbrellaEligibility eligibility,
    FusionUmbrellaBlockState& state, FusionUmbrellaPoseState& pose, bool& startPose)
{
    if (!Incoming(projectile,attacked,view) || !Eligible(umbrella,attacked,*view,eligibility)) return false;
    const float vertical = view->verticalAxis == NativeUmbrellaVerticalAxis::LobZ ? projectile->mVelZ : projectile->mVelY;
    if (!std::isfinite(projectile->mVelX) || !std::isfinite(vertical)) return false;
    if (!TryBeginFusionUmbrellaBlock(view->generation,Generation(umbrella),projectile->mBoard->mMainCounter,state,pose,startPose)) return false;
    LawnNet::NetTrace("Fusion native Umbrella block plant=%u projectile=%u generation=%u kind=%u way=13 harmless=1 pending=%d startPose=%d tick=%d",
        umbrella->mNetId,projectile->mNetId,view->generation,view->sourceKind,kFusionUmbrellaDelayTicks,startPose?1:0,projectile->mBoard->mMainCounter);
    return true;
}
FusionUmbrellaAdvance AdvanceNativeUmbrellaDeflection(Projectile* projectile,
    FusionUmbrellaBlockState& state, NativeUmbrellaVerticalAxis axis)
{
    if (!FusionUmbrellaBlockStateValid(state)) return FusionUmbrellaAdvance::Invalid;
    if (!state.pending) return FusionUmbrellaAdvance::Unchanged;
    if (!Host(projectile) || projectile->mDead || Generation(projectile) != state.projectileGeneration ||
        !Playing(projectile->mBoard)) return Cancel(state);
    if (!KnownAxis(axis)) return FusionUmbrellaAdvance::Invalid;
    auto* owner = projectile->mBoard->mPlants.DataArrayTryToGet(state.umbrellaGeneration);
    if (!Host(owner) || !Umbrella(owner) || Generation(owner) != state.umbrellaGeneration) return Cancel(state);
    if (projectile->mBoard->mPaused) return FusionUmbrellaAdvance::Unchanged;
    float& vertical = axis == NativeUmbrellaVerticalAxis::LobZ ? projectile->mVelZ : projectile->mVelY;
    const auto result = AdvanceFusionUmbrellaBlock(state,projectile->mBoard->mMainCounter,true,true,projectile->mVelX,vertical);
    if (result == FusionUmbrellaAdvance::Deflected)
        LawnNet::NetTrace("Fusion native Umbrella deflect plant=%u projectile=%u generation=%u vx=%.9g vertical=%.9g axis=%d tick=%d",
            owner->mNetId,projectile->mNetId,state.projectileGeneration,projectile->mVelX,vertical,static_cast<int>(axis),projectile->mBoard->mMainCounter);
    return result;
}
bool IsNativeUmbrellaHarmless(const Projectile* projectile, const FusionUmbrellaBlockState& state)
{
    return FusionUmbrellaBlockStateValid(state) && state.harmless && Current(projectile) &&
        !projectile->mDead && Generation(projectile) == state.projectileGeneration;
}
bool ApplyNativeUmbrellaPoseEvent(Plant* umbrella, FusionUmbrellaPoseState& pose,
    FusionUmbrellaPoseEvent event)
{
    return Umbrella(umbrella) && ApplyFusionUmbrellaPoseEvent(pose,Generation(umbrella),event);
}
}
