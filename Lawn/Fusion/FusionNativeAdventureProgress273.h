#pragma once
#include "FusionAdventureProgress273.h"
class Board;class Coin;
namespace LawnFusion
{
// Root owns the NativeContext member/save/snapshot codec. Each adapter receives
// that one room state by reference; clients never step it or mutate the profile.
// Native lessons1/4 only. Active7/8 fail closed. Prepare4 currently supports
// empty cells or audited removal-only kinds0/1/2/3/1000/1030/1032/1033; other
// occupants refuse before mutation until their source callback budgets/order
// are closed. Root keeps public/campaign admission separate from this adapter.
bool UpdateNativeFusionAdventureTutorial273(Board*,FusionAdventureRun273&,FusionAdventureTutorialEffects273&);
void PresentNativeFusionAdventureTutorial273(Board*,const FusionAdventureRun273&);
// Real non-friendly Zombie::Die, after source enemy-count decrement. Root must
// suppress stock DropAward/check-level-win for source Adventure separately.
bool NativeFusionAdventureEnemyDied273(Board*,FusionAdventureRun273&,int sourceWave,int maximumWave,
    int sourceEnemyCount,bool isIZ,float nativeRootX,float nativeRootY);
bool EnsureNativeFusionAdventureTrophy273(Board*,FusionAdventureRun273&);
bool IsNativeFusionAdventureTrophy273(const Coin*,const FusionAdventureRun273&);
// Coin::Collect BEFORE vanilla mIsBeingCollected/profile/FadeOutLevel changes.
// Return true consumes a recognized trophy even for a rejected/repeated click.
// committed=true is the ONE host event at which Root persists/broadcasts profile.
bool CollectNativeFusionAdventureTrophy273(Coin*,int authenticatedSlot,FusionAdventureProfile273&,
    FusionAdventureRun273&,bool& committed);
// BEFORE Coin::Update (including mCoinAge++) and ScoreCoin/auto-collect: this
// trophy may only complete through a real authenticated player collection.
bool UpdateNativeFusionAdventureTrophy273(Coin*,const FusionAdventureRun273&);
// Host Board gameplay tick, once per native100Hz update; does NOT dispatch the
// next board. Root takes TakeFusionAdventureTransition273 exactly once afterward.
bool UpdateNativeFusionAdventureReward273(Board*,FusionAdventureRun273&);
}
