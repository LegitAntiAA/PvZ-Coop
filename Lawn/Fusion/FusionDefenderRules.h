#ifndef LAWN_FUSION_DEFENDER_RULES_H
#define LAWN_FUSION_DEFENDER_RULES_H

#include <cstddef>
#include <cstdint>

namespace LawnFusion
{
enum class FusionDefenderFamily { WallNut, TallNut, Pot, Pumpkin };
enum class FusionDefenderLayer { Body, Under, Pumpkin, Flying };
enum class FusionDefenderElement { Freeze, Fire };
enum class FusionDefenderBite { None, ReflectHalf, Cold, Fire, ReflectFull };
enum FusionDefenderPending : uint32_t
{
    kDefenderPendingNone = 0,
    kDefenderPendingEquipment = 1,
    kDefenderPendingGarlic = 2,
    kDefenderPendingUmbrella = 4,
    kDefenderPendingStar = 16,
    kDefenderPendingZombieCherry = 32
};
struct FusionDefenderProfile
{
    uint16_t kind;
    FusionDefenderFamily family;
    FusionDefenderLayer layer;
    int maxHealth, sunCost, rechargeTicks;
    FusionDefenderBite bite;
    const char* damageNodes[3]; // Actual source root children0/1/2; null for pots.
    uint32_t pendingDependencies;
};
struct FusionDefenderBiteEffect
{
    int shieldlessDamage = 0;
    int chilledTicks = 0;
    int coldLevels = 0;
    bool warm = false;
};
struct FusionDefenderBucketEffect
{
    uint16_t replacementKind = 0xffff;
    int recoverHealth = 0;
};

const FusionDefenderProfile* FindFusionDefenderProfile(uint16_t kind);
const FusionDefenderProfile* FusionDefenderProfileAt(std::size_t index);
std::size_t FusionDefenderProfileCount();
bool FusionDefenderHasDamageStages(uint16_t kind);
// Source WallNut.ReplaceSprite uses strict thirds; exact boundaries retain prior.
bool TryGetFusionDefenderDamageStage(int health, int maximum, uint8_t previous, uint8_t& stage);
bool TryGetFusionDefenderBiteEffect(uint16_t kind, int actualBiteDamage, FusionDefenderBiteEffect& effect);
bool FusionDefenderBlocksPotDamage(uint16_t kind, bool carrying, int sourceDamageType);
int FusionDefenderElementRecovery(uint16_t kind, FusionDefenderElement element);
bool TryGetFusionDefenderBucketEffect(uint16_t kind, int sourceBucketType, FusionDefenderBucketEffect& effect);

float FusionDefenderLobberInterval(uint16_t bodyKind, uint16_t potKind, float original);
int FusionDefenderCornDenominator(uint16_t bodyKind, uint16_t potKind, int original);
bool FusionDefenderMelonEnhanced(uint16_t bodyKind, uint16_t potKind);
}
#endif
