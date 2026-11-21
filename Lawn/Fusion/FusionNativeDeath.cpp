#include "FusionNativeDeath.h"
#include "FusionNative.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeLastPlants269.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeLaserConnections269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativePlacement.h"
#include "FusionCherryRules.h"
#include "FusionPotatoRules.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeAction266.h"
#include "FusionNativeWater268.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeSummon269.h"
#include "FusionNativeStar269.h"
#include "FusionNativeDoom269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../GridItem.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodParticle.h"

namespace LawnFusion
{
namespace
{
bool HostCanonicalBoard(Board* board)
{
    const auto* context = GetNativeContext(board);
    return context && board->mApp && board->mApp->mBoard == board && !board->mApp->NetIsClient() &&
        board->mApp->mGameScene == SCENE_PLAYING && context->selection.State().phase == FusionBankPhase::Playing &&
        NativeFusionBackgroundValid(static_cast<uint8_t>(board->mBackground)) && board->mBoardFadeOutCounter < 0 &&
        !board->mApp->GetDialog(DIALOG_ALMANAC);
}
bool HostGameplay(Board* board)
{ return HostCanonicalBoard(board) && !board->HasLevelAwardDropped(); }
NativePlantData* HostPlant(Plant* plant, bool removing)
{
    if (!plant || !HostCanonicalBoard(plant->mBoard) || plant->mApp != plant->mBoard->mApp ||
        plant->mNetGhost || !plant->mIsOnBoard || plant->mDead != removing ||
        !plant->mBoard->IsValidGridCell(plant->mPlantCol,plant->mRow)) return nullptr;
    auto* data = GetNativePlantData(plant); // Validates the actual DataArray generation, even after mDead.
    return data && GetNativeSeedForPlant(data->kind) == plant->mSeedType ? data : nullptr;
}
bool MakeBlast(Plant* plant, uint16_t kind, bool threshold, NativeBlastSpec& spec)
{
    return TryMakeFusionDeathBlast(kind,plant->mBoard->GetCellCenterX(plant->mPlantCol,plant->mRow),
        plant->mBoard->GridToPixelY(plant->mPlantCol,plant->mRow)+50,plant->mRow,threshold,spec);
}
void BlastEffects(Board* board, const NativeBlastSpec& spec)
{
    const bool cherry = spec.policy == FusionDeathBlastPolicy::Cherry;
    const auto effect = cherry ? PARTICLE_POWIE : PARTICLE_POTATO_MINE;
    const int order = cherry ? static_cast<int>(RENDER_LAYER_TOP) :
        Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,spec.row,0);
    const float x = static_cast<float>(spec.centerX), y = static_cast<float>(spec.centerY);
    board->mApp->AddTodParticle(x,y,order,effect);
    LawnNet::NetBroadcastParticle(board->mApp,x,y,order,effect);
    if (cherry)
    {
        board->mApp->PlayFoley(FOLEY_CHERRYBOMB);
        board->mApp->PlayFoley(FOLEY_JUICY);
    }
    else board->mApp->PlaySample(Sexy::SOUND_POTATO_MINE);
    if (spec.shake) board->ShakeBoard(3,-4);
}
}

void HealNativeCherryNutFromBlast(Board* board, double centerX, double centerY, int row)
{
    NativeBlastSpec spec;
    spec.centerX = centerX; spec.centerY = centerY; spec.row = row;
    // This is a continuation, also used by source2 after its zombie-damage loop.
    // A last kill may already have awarded the level; accepted healing still runs.
    if (!HostCanonicalBoard(board) || !FusionDeathBlastValid(spec) || spec.row>=GetNativeFusionRowCount(board)) return;
    Plant* receiver = nullptr;
    while (board->IteratePlants(receiver))
    {
        auto* data = HostPlant(receiver,false);
        if (!data || data->kind != 1003 || receiver->mPlantHealth <= 0) continue;
        const int overlaps = FusionCherryNutHealOverlapCount(spec,
            board->GetCellCenterX(receiver->mPlantCol,receiver->mRow),
            board->GridToPixelY(receiver->mPlantCol,receiver->mRow)+50,receiver->mRow);
        for (int i = 0; i < overlaps; ++i)
        {
            const int before = receiver->mPlantHealth;
            int after = before;
            if (!TryApplyFusionDeathHeal(before,receiver->mPlantMaxHealth,1000,after)) break;
            receiver->mPlantHealth = after;
            // Native nut/source art consumes the real HP; no independent health or visual clock.
            LawnNet::NetTrace("Fusion native Death heal id=%u token=%u kind=1003 collider=%d before=%d after=%d max=%d tick=%d",
                receiver->mNetId,data->instanceToken,i,before,after,receiver->mPlantMaxHealth,board->mMainCounter);
        }
    }
}

void ApplyNativeFusionBlast(Board* board, const NativeBlastSpec& spec)
{
    if (CanStartNativeFusionBlast(board,spec)) ContinueNativeFusionBlast(board,spec);
}

bool CanStartNativeFusionBlast(Board* board, const NativeBlastSpec& spec)
{
    return HostGameplay(board) && FusionDeathBlastValid(spec) && spec.row<GetNativeFusionRowCount(board);
}

void ContinueNativeFusionBlast(Board* board, const NativeBlastSpec& spec)
{
    if (!HostCanonicalBoard(board) || !FusionDeathBlastValid(spec) || spec.row>=GetNativeFusionRowCount(board)) return;
    Zombie* zombie = nullptr;
    while (board->IterateZombies(zombie))
    {
        if (zombie->mDead || zombie->mNetGhost || NativeWater268ZombieCaptured(zombie) || !IsNativeFusionGroundZombie(zombie) ||
            zombie->mMindControlled || !FusionDeathBlastAffectsRow(spec,zombie->mRow)) continue;
        const auto rect = zombie->GetZombieRect();
        if (!FusionDeathBlastRectOverlaps(spec,rect.mX,rect.mY,rect.mWidth,rect.mHeight)) continue;
        if (spec.policy == FusionDeathBlastPolicy::Cherry) ApplyNativeFusionCherryDamage(zombie);
        else if (FusionPotatoCanDamage(zombie->IsFlying() ? 10 : zombie->IsDeadOrDying() ? 1 : 0,
            zombie->mRow == spec.row,zombie->mMindControlled)) ApplyNativeFusionPotatoDamage(zombie,spec.damage);
    }
    if (spec.policy == FusionDeathBlastPolicy::Cherry)
    {
        const FusionCherryArea area{spec.centerX,spec.centerY};
        GridItem* item = nullptr;
        while (board->IterateGridItems(item))
            if (!item->mDead && item->mGridItemType == GRIDITEM_LADDER &&
                board->IsValidGridCell(item->mGridX,item->mGridY) && FusionDeathBlastAffectsRow(spec,item->mGridY) &&
                FusionCherryLadderOverlaps(area,board->GetCellCenterX(item->mGridX,item->mGridY),
                    board->GridToPixelY(item->mGridX,item->mGridY)+50)) item->GridItemDie();
        HealNativeCherryNutFromBlast(board,spec.centerX,spec.centerY,spec.row);
    }
    BlastEffects(board,spec);
    LawnNet::NetTrace("Fusion native Death blast policy=%d row=%d center=%.6f,%.6f damage=%d shake=%d tick=%d awarded=%d",
        static_cast<int>(spec.policy),spec.row,spec.centerX,spec.centerY,spec.damage,spec.shake ? 1 : 0,
        board->mMainCounter,board->HasLevelAwardDropped() ? 1 : 0);
}

void NativePlantHealthChangedForDeath(Plant* plant)
{
    if (!plant || !HostGameplay(plant->mBoard)) return;
    auto* data = HostPlant(plant,false);
    if (!data || (data->kind != 1010 && data->kind != 1190)) return;
    NativeBlastSpec spec;
    if (!MakeBlast(plant,data->kind,true,spec)) return;
    int damage = 0;
    if (!TryConsumeFusionPotatoNutThreshold(plant->mPlantHealth,plant->mPlantMaxHealth,data->deathEventFlags,damage) || !damage) return;
    // Consume before the synchronous effect; lethal low500 precedes the later death1800.
    LawnNet::NetTrace("Fusion native Death threshold id=%u token=%u kind=1010 hp=%d flags=%u damage=%d tick=%d",
        plant->mNetId,data->instanceToken,plant->mPlantHealth,data->deathEventFlags,damage,plant->mBoard->mMainCounter);
    ApplyNativeFusionBlast(plant->mBoard,spec);
}

void NativePlantRemoving(Plant* plant, int sourceReason)
{
    // Identity cleanup is unconditional; source damage/release effects retain
    // the same explicit removal-reason policy as the existing death families.
    if(const auto* data=GetNativePlantData(plant);data&&data->kind==900)
        NativeLastPlants269Removing(plant,sourceReason);
    NativeSummon269PlantRemoving(plant);
    NativeSummonPlants269Removing(plant,sourceReason);
    NativeRemaining269PlantRemoving(plant,sourceReason);
    NativeFertilize269PlantRemoving(plant);
    NativeCompanions269PlantRemoving(plant,sourceReason);
    DisconnectNativeLaser269(plant);
    NativeLaserConnections269PlantRemoving(plant);
    NativeStar269PlantRemoving(plant,FusionDeathReasonHasEffects(sourceReason)?sourceReason:255);
    NativeDoom269PlantRemoving(plant,sourceReason);
    if (plant) NativeEquipmentPlantRemoving(plant,sourceReason);
    if (!FusionDeathReasonHasEffects(sourceReason) || !plant || !HostGameplay(plant->mBoard)) return;
    NativeWater268PlantRemoving(plant,sourceReason);
    NativeExtras268PlantRemoving(plant,sourceReason);
    NativeFireActionPlantRemoving(plant,sourceReason);
    NativeBatchPlantRemoving(plant,sourceReason);
    const auto* data = HostPlant(plant,true);
    const auto* profile = data ? FindFusionDeathProfile(data->kind) : nullptr;
    NativeBlastSpec spec;
    if (!profile || !MakeBlast(plant,data->kind,false,spec))
    { NativeAction266PlantRemoving(plant,sourceReason); return; }
    // Capture provenance before any nested world-factory allocation can rehash metadata.
    const auto provenance = *data;
    LawnNet::NetTrace("Fusion native Death remove id=%u token=%u kind=%u reason=%d tick=%d",
        plant->mNetId,provenance.instanceToken,provenance.kind,sourceReason,plant->mBoard->mMainCounter);
    ApplyNativeFusionBlast(plant->mBoard,spec);
    NativeAction266PlantRemoving(plant,sourceReason);
    if (!profile->createsMines) return;
    std::array<FusionDeathSpawnCell,9> cells;
    size_t count = 0;
    if (!TryGetFusionDeathSpawnCells(plant->mPlantCol,plant->mRow,plant->mBoard->GetColumnCount(),
        GetNativeFusionRowCount(plant->mBoard),cells,count)) return;
    NativePlantData incoming;
    // These are fresh source world mines, not the paid parent's card actor.
    incoming.kind = 4; incoming.owner = LawnNet::NET_SHARED_OWNER;
    for (size_t i = 0; i < count; ++i)
    {
        Plant* child = TrySpawnNativeFusionPlant(plant->mBoard,cells[i].column,cells[i].row,incoming);
        const auto* childData = GetNativePlantData(child);
        if (child && !child->mDead && childData && childData->kind == 4 && child->mSeedType == SEED_POTATOMINE)
            child->mStateCountdown = kFusionDeathMinePreparationTicks;
        LawnNet::NetTrace("Fusion native Death spawn source=%u column=%d row=%d success=%d child=%u kind=%u preparation=%d tick=%d",
            plant->mNetId,cells[i].column,cells[i].row,child ? 1 : 0,child ? child->mNetId : 0,
            childData ? childData->kind : 65535,childData && childData->kind == 4 ? child->mStateCountdown : -1,plant->mBoard->mMainCounter);
    }
}
}
