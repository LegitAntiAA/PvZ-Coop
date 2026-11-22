#include "FusionNativeGarlic.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeDefender.h"
#include "FusionDefenderRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
int Rows(Board* board){return board->StageHas6Rows()?6:5;}
bool CurrentBoard(Board* board)
{
    if (!board || !board->mApp || board->mApp->mBoard != board || !IsNativeFusionBoard(board) ||
        !NativeFusionBackgroundValid(uint8_t(board->mBackground))) return false;
    // Use real native grass/pool rows; row transitions preserve terrain compatibility.
    for (int row = 0; row < Rows(board); ++row)
        if (board->mPlantRow[row] != PLANTROW_NORMAL && board->mPlantRow[row] != PLANTROW_POOL) return false;
    return true;
}
bool MachineRow(const Zombie* z,const GarlicActionState* state=nullptr)
{const auto* d=GetNativeSourceZombieData(z);if(!state)state=GetNativeZombieGarlicData(z);return d&&state&&FusionSourceMachineRowMoving323(d->kind,*state);}
bool CurrentZombie(Zombie* zombie,const GarlicActionState* movement=nullptr)
{
    return zombie && CurrentBoard(zombie->mBoard) && zombie->mApp == zombie->mBoard->mApp &&
        !zombie->mDead && !zombie->IsDeadOrDying() && zombie->mBodyHealth > 0 && zombie->mHasHead &&
        IsNativeFusionGroundZombie(zombie) && zombie->mZombiePhase == PHASE_ZOMBIE_NORMAL &&
        zombie->mZombieHeight == HEIGHT_ZOMBIE_NORMAL && (!zombie->mMindControlled||MachineRow(zombie,movement)) &&
        !zombie->mOnHighGround && zombie->mRow >= 0 && zombie->mRow < Rows(zombie->mBoard) &&
        std::isfinite(zombie->mPosX) && std::isfinite(zombie->mPosY);
}
bool HostZombie(Zombie* zombie)
{
    bool source313=false;
    if(MachineRow(zombie))return CurrentZombie(zombie)&&!zombie->mNetGhost&&!zombie->mApp->NetIsClient()&&zombie->mApp->mGameScene==SCENE_PLAYING;
    if(NativeZombie313StatusTarget(zombie,FusionExtendedEffect313::Garlic,source313))return source313&&CurrentZombie(zombie)&&
        !zombie->mMindControlled&&zombie->mApp->mGameScene==SCENE_PLAYING;
    return CurrentZombie(zombie) && !zombie->mNetGhost && !zombie->mApp->NetIsClient() &&
        zombie->mApp->mGameScene == SCENE_PLAYING;
}
void SortingRow(Zombie* zombie, int row)
{ zombie->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,row,4); }
bool StartRow(Zombie* zombie,GarlicActionState& state,bool playSound)
{
        const int from = zombie->mRow;
        const auto compatible = [&](int row)
        {
            return row >= 0 && row < Rows(zombie->mBoard) && zombie->mBoard->RowCanHaveZombies(row) &&
                (zombie->mBoard->mPlantRow[row] == PLANTROW_POOL) == (zombie->mBoard->mPlantRow[from] == PLANTROW_POOL);
        };
        GarlicRowPlan plan;
        if (!TryPlanFusionGarlicRow(from,Rows(zombie->mBoard),compatible(from-1),compatible(from+1),state.certainRow,plan)) return false;
        zombie->StopEating();
        if(playSound)zombie->mApp->PlayFoley(FOLEY_YUCK); // Host-only sound; presentation restore is RNG-free.
        const int target = plan.randomZeroRow == plan.randomOneRow ? plan.randomZeroRow :
            (RandRangeInt(0,1) == 0 ? plan.randomZeroRow : plan.randomOneRow);
        const float targetY = zombie->GetPosYBasedOnRow(target);
        if (!TryStartFusionGarlicRow(state,Rows(zombie->mBoard),from,target,zombie->mPosY,targetY)) return false;
        zombie->SetRow(FusionGarlicLogicalRow(state,from));
        SortingRow(zombie,FusionGarlicSortingRow(state,from));
        LawnNet::NetTrace("Fusion native Garlic row zombie=%u from=%d target=%d startY=%.9g targetY=%.9g face=%u tick=%d",
            zombie->mNetId,from,target,zombie->mPosY,targetY,state.faceTicks,zombie->mBoard->mMainCounter);
        return true;
}

}
bool IsNativeGarlicPlant(const Plant* plant)
{
    if (!plant || !CurrentBoard(plant->mBoard) || plant->mApp != plant->mBoard->mApp) return false;
    const auto* data = GetNativePlantData(plant);
    return data && IsFusionGarlicKind(data->kind) && GetNativeSeedForPlant(data->kind) != SEED_NONE &&
        plant->mSeedType == GetNativeSeedForPlant(data->kind);
}
bool CanBeginNativeGarlicBite(Zombie* zombie, Plant* victim, const GarlicActionState& state)
{
    return IsNativeGarlicPlant(victim) && HostZombie(zombie) && !NativeSummon269RejectStatus(zombie) && zombie->mBoard == victim->mBoard &&
        !zombie->mBoard->mPaused && !zombie->IsImmobilizied() && !victim->mDead && !victim->mNetGhost &&
        victim->mIsOnBoard && victim->mPlantHealth > 0 && victim->mPlantCol >= 0 &&
        victim->mPlantCol < victim->mBoard->GetColumnCount() && victim->mRow == zombie->mRow &&
        ValidateFusionGarlicAction(state,Rows(zombie->mBoard)) && state.phase == GarlicActionPhase::Idle;
}
NativeGarlicBiteResult BeginNativeGarlicBite(Zombie* zombie, Plant* victim, GarlicActionState& state, int& damage)
{
    if (!IsNativeGarlicPlant(victim)) return NativeGarlicBiteResult::NotGarlic;
    if (!CanBeginNativeGarlicBite(zombie,victim,state)) return NativeGarlicBiteResult::HandledBlocked;
    const auto* data = GetNativePlantData(victim);
    if (!TryBeginFusionGarlicAction(data->kind,GetNativeDefenderPotKind(victim),Rows(zombie->mBoard),state,damage))
        return NativeGarlicBiteResult::HandledBlocked;
    LawnNet::NetTrace("Fusion native Garlic begin zombie=%u plant=%u kind=%u damage=%d certain=%d wait=%u tick=%d",
        zombie->mNetId,victim->mNetId,data->kind,damage,state.certainRow?1:0,state.waitTicks,zombie->mBoard->mMainCounter);
    return NativeGarlicBiteResult::Started;
}
bool RestoreNativeGarlicPresentation(Zombie* zombie, const GarlicActionState& state)
{
    if (!FusionGarlicActionActive(state))
    {
        if (!zombie || !zombie->mBoard || !ValidateFusionGarlicAction(state,Rows(zombie->mBoard)) || !zombie->mApp ||
            !IsNativeFusionBoard(zombie->mBoard) || !zombie->mApp->ReanimationTryToGet(zombie->mBodyReanimID)) return false;
        zombie->ShowYuckyFace(false);
        SortingRow(zombie,zombie->mRow);
        return true;
    }
    if (!CurrentZombie(zombie,&state) || !ValidateFusionGarlicAction(state,Rows(zombie->mBoard)) ||
        FusionGarlicLogicalRow(state,zombie->mRow) != zombie->mRow || !zombie->mApp->ReanimationTryToGet(zombie->mBodyReanimID)) return false;
    // The native mYuckyFace flag also stops movement/attacks. Source reaction
    // images are adopted directly so they cannot activate that Classic clock.
    zombie->ShowYuckyFace(state.faceTicks != 0);
    SortingRow(zombie,FusionGarlicSortingRow(state,zombie->mRow));
    return true;
}
bool BeginNativeGarlicEffect(Zombie* zombie,uint16_t waitTicks,bool certainRow)
{
    if (!HostZombie(zombie) || NativeSummon269RejectStatus(zombie) || zombie->mBoard->mPaused || zombie->IsImmobilizied()) return false;
    auto* state=GetOrCreateNativeZombieGarlicData(zombie);
    if (!state || !TryBeginFusionGarlicTimedAction(Rows(zombie->mBoard),waitTicks,certainRow,*state)) return false;
    zombie->StopEating();
    LawnNet::NetTrace("Fusion native Garlic skill zombie=%u wait=%u certain=%d tick=%d",zombie->mNetId,waitTicks,certainRow?1:0,zombie->mBoard->mMainCounter);
    return true;
}
bool BeginNativeGarliced(Zombie* zombie,bool certainRow)
{
    if(!HostZombie(zombie)||NativeSummon269RejectStatus(zombie)||zombie->mBoard->mPaused)return false;
    auto* state=GetOrCreateNativeZombieGarlicData(zombie);
    if(!state||!TryBeginFusionGarlicTimedAction(Rows(zombie->mBoard),1,certainRow,*state))return false;
    return StartRow(zombie,*state,false)&&RestoreNativeGarlicPresentation(zombie,*state);
}
bool UpdateNativeGarlicAction(Zombie* zombie, GarlicActionState& state, float speed)
{
    if (!HostZombie(zombie) || !std::isfinite(speed) || speed < 0 || !zombie->mApp->ReanimationTryToGet(zombie->mBodyReanimID)) return false;
    if (FusionGarlicLogicalRow(state,zombie->mRow) != zombie->mRow) return false;
    GarlicActionStep step;
    if (!TryAdvanceFusionGarlicAction(state,Rows(zombie->mBoard),zombie->mBoard->mPaused,
        zombie->IsImmobilizied()?0.0f:speed,step)) return false;
    if (step.rowStartDue)
    {if(!StartRow(zombie,state,true))return false;}
    else if (step.positionChanged)
    {
        zombie->mPosY = step.positionY; zombie->mY = static_cast<int>(step.positionY);
        if (step.rowFinished)
        {
            zombie->SetRow(step.targetRow);
            LawnNet::NetTrace("Fusion native Garlic moved zombie=%u row=%d y=%.9g face=%u tick=%d",
                zombie->mNetId,zombie->mRow,zombie->mPosY,state.faceTicks,zombie->mBoard->mMainCounter);
        }
    }
    return RestoreNativeGarlicPresentation(zombie,state);
}
bool BeginNativeSourceRowDuration313(Zombie* z,float seconds)
{
    // Source380C50/385130: same adjacent-row/terrain policy and coroutine,
    // with NO Garlic face, sound, waiting bite, or stock body-track changes.
    if(!z||!CurrentBoard(z->mBoard)||z->mDead||z->mNetGhost||z->mApp->NetIsClient()||z->mBoard->mPaused||
       !CanApplyNativeFusionGameplay(z->mBoard)||!std::isfinite(seconds)||seconds<=0||seconds>100)return false;
    auto* state=GetOrCreateNativeZombieGarlicData(z);const int rows=Rows(z->mBoard),from=z->mRow;
    if(!state||!ValidateFusionGarlicAction(*state,rows)||state->phase!=GarlicActionPhase::Idle||from<0||from>=rows)return false;
    const auto compatible=[&](int row){return row>=0&&row<rows&&
        (z->mBoard->mPlantRow[row]==PLANTROW_POOL)==(z->mBoard->mPlantRow[from]==PLANTROW_POOL);};
    GarlicRowPlan plan;if(!TryPlanFusionGarlicRow(from,rows,compatible(from-1),compatible(from+1),false,plan))return false;
    // Full validation precedes the sole actual native RNG draw.
    GarlicActionState probe=*state;
    if(!TryStartFusionSourceRowDuration313(probe,rows,from,plan.randomZeroRow,z->mPosY,z->mPosY,seconds))return false;
    const int target=plan.randomZeroRow==plan.randomOneRow?plan.randomZeroRow:
        (RandRangeInt(0,1)==0?plan.randomZeroRow:plan.randomOneRow);
    const float y=z->GetPosYBasedOnRow(target);
    if(!TryStartFusionSourceRowDuration313(*state,rows,from,target,z->mPosY,y,seconds))return false;
    z->SetRow(FusionGarlicLogicalRow(*state,from));SortingRow(z,FusionGarlicSortingRow(*state,from));return true;
}
void NativeGarlicHealthChanged(Plant* plant)
{
    if (!IsNativeGarlicPlant(plant) || plant->mDead || plant->mNetGhost || plant->mApp->NetIsClient()) return;
    auto* data = GetNativePlantData(plant);
    if (data->kind == 1130) return; //1130 retains its source pot appearance.
    uint8_t stage = data->damageStage;
    if (TryGetFusionDefenderDamageStage(plant->mPlantHealth,plant->mPlantMaxHealth,stage,stage)) data->damageStage = stage;
}
}
