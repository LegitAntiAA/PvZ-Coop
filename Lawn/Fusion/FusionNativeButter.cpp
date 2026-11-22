#include "FusionNativeButter.h"
#include "FusionButterRules.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
bool ApplyNativeFusionButter(Zombie* zombie, int ticks, bool showSprite)
{
    if (!zombie || !zombie->mApp || !zombie->mBoard || zombie->mApp->mBoard != zombie->mBoard ||
        zombie->mApp->NetIsClient() || zombie->mNetGhost || zombie->mDead ||
        !IsNativeFusionBoard(zombie->mBoard) || !NativeFusionBackgroundValid(static_cast<uint8_t>(zombie->mBoard->mBackground)) ||
        !IsNativeFusionGroundZombie(zombie) || zombie->IsDeadOrDying() || zombie->mBodyHealth <= 0 ||
        zombie->mMindControlled) return false;
    bool source313=false;
    if(NativeZombie313StatusTarget(zombie,FusionExtendedEffect313::Butter,source313)){if(!source313)return false;}
    else if(!zombie->mHasHead||!zombie->CanBeFrozen()||zombie->IsTangleKelpTarget()||zombie->IsBobsledTeamWithSled()||zombie->IsFlying())return false;

    const auto* before = GetNativeZombieButterData(zombie);
    const int beforeTicks = zombie->mButteredCounter;
    // An untracked native butter effect was visible; an expired record is not.
    const bool beforeVisible = beforeTicks > 0 && (!before || before->visible);
    bool visible = false;
    if (!TryPrepareFusionButter(beforeTicks, beforeVisible, ticks, visible,showSprite)) return false;
    auto* data = GetOrCreateNativeZombieButterData(zombie); // Validates the allocated full generation.
    if (!data) return false;

    zombie->ApplyButter(ticks); // Existing native counter, animation and sound handling.
    data->ticks = zombie->mButteredCounter; // Snapshot mirror only; no second ticking.
    data->visible = visible;
    return true;
}

bool ShouldDrawNativeButter(const Zombie* zombie)
{
    if (!zombie || !IsNativeFusionBoard(zombie->mBoard)) return true;
    const auto* data = GetNativeZombieButterData(zombie);
    return !data || data->visible;
}
}
