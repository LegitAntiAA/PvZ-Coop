#ifndef LAWN_FUSION_NATIVE_DEFENDER_H
#define LAWN_FUSION_NATIVE_DEFENDER_H

#include "FusionDefenderRules.h"
class Board;
class Plant;
class Zombie;

namespace LawnFusion
{
bool IsNativeDefender(const Plant* plant);
void InitializeNativeDefender(Plant* plant);
void NativeDefenderHealthChanged(Plant* plant);
// Read the actual native under-slot. No cached support actor/bonus or second clock.
uint16_t GetNativeDefenderPotKind(Plant* plant);
float GetNativeDefenderLobberInterval(Plant* plant, float original);
int GetNativeDefenderCornDenominator(Plant* plant, int original);
// Capture this at projectile emission, never re-read the pot at impact.
bool GetNativeDefenderMelonEnhanced(Plant* plant);
void NativeDefenderBeforeBite(Zombie* biter, Plant* victim, int actualBiteDamage);
// Caller maps a known source damage type0/1/2; default/unknown native damage stays native.
bool NativeDefenderBlocksDamage(Plant* plant, int sourceDamageType);
void NativeDefenderElementPulse(Board* board, FusionDefenderElement element, int row = -1);
bool CanUseNativeDefenderBucket(const Plant* plant, int sourceBucketType, FusionDefenderBucketEffect& effect);
// Root owns authentic item lease/token checks and consumption. A1027 upgrade
// requires a fresh prepared1028 actor in the same cell before this commit;
// this helper allocates/debits nothing and never invents a fusion recipe.
bool ApplyNativeDefenderBucket(Plant* plant, int sourceBucketType, Plant* preparedReplacement = nullptr);
}
#endif
