#ifndef LAWN_FUSION_NATIVE_H
#define LAWN_FUSION_NATIVE_H

#include "FusionSeedBank.h"
#include "FusionRollingProjectileRules.h"
#include "FusionGarlicRules.h"
#include "FusionNativeZombies.h"
#include "FusionUmbrellaRules.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeStar269.h"
#include "FusionNativeDoom269.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeRollingActors269.h"
#include "FusionNativeRemainingLeaves269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionAdventureSession273.h"
#include "FusionAdventureScheduler273.h"
#include "FusionNativeDayZombie273.h"
#include "FusionNativeNightZombie281.h"
#include "FusionNativeJackbox300.h"
#include "FusionNativeSourceGraph313.h"
#include "FusionNativeWaterZombie313.h"
#include "FusionNativeExtendedZombie313.h"
#include "FusionNativeHeavyZombie313B.h"
#include "FusionSourceBoard313.h"
#include "FusionNativeObstacle313.h"
#include "../../ConstEnums.h"
#include <cstdint>
#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class Board;
class Plant;
class Projectile;
class Zombie;
class Coin;
class SeedPacket;
class GameButton;
namespace LawnNet { class NetJackbox309Audio; }

namespace LawnFusion
{
constexpr int kNativeFusionColumns = 10;
constexpr int kNativeFusionRows = 5;
constexpr int kNativeFusionCellPitchX = 72;
inline int NativeFusionPoolEntryRight(int columns)
{return 680+(columns-9)*kNativeFusionCellPitchX;}
inline bool NativeFusionBackgroundValid(uint8_t background)
{ return background==BACKGROUND_1_DAY||background==BACKGROUND_2_NIGHT||background==BACKGROUND_3_POOL||
    background==BACKGROUND_4_FOG||background==BACKGROUND_5_ROOF||background==BACKGROUND_6_BOSS; }
inline int NativeFusionRowsForBackground(uint8_t background)
{ return background==BACKGROUND_3_POOL||background==BACKGROUND_4_FOG?6:5; }
inline bool NativeFusionGroundTerrainAllowed(uint8_t background,int row,bool inPool)
{
    return NativeFusionBackgroundValid(background)&&row>=0&&row<NativeFusionRowsForBackground(background)&&
        (!inPool||(NativeFusionRowsForBackground(background)==6&&(row==2||row==3)));
}
inline int NativeFusionPixelToRow(uint8_t background,int pixelY,bool keepOnBoard=false)
{
    if(!NativeFusionBackgroundValid(background))return -1;
    const int rows=NativeFusionRowsForBackground(background),pitch=rows==6?85:100;
    if(pixelY<LAWN_YMIN)return keepOnBoard?0:-1;
    if(pixelY>=LAWN_YMIN+rows*pitch)return keepOnBoard?rows-1:-1;
    return (pixelY-LAWN_YMIN)/pitch;
}
inline bool NativeGarlicRawFlagsValid(uint8_t background,int row,uint8_t flags)
{
    if(!NativeFusionBackgroundValid(background)||row<0||row>=NativeFusionRowsForBackground(background)||
        !(flags&8)||(flags&(4|128)))return false;
    return !(flags&2)||(NativeFusionRowsForBackground(background)==6&&(row==2||row==3));
}
int GetNativeFusionRowCount(const Board* board);
double GetNativeFusionPixelsY(const Board* board);
bool ConfigureNativeFusionBackground(Board* board,uint8_t background);
// DAY grass: Board::GridToPixelY(row) minus Zombie::GetPosYBasedOnRow's30.
inline float NativeGarlicRowY(int row,int rows=5) { return static_cast<float>(LAWN_YMIN-30+(rows==6?85:100)*row); }
inline bool NativeGarlicActionStateValid(const GarlicActionState& state,int rows=5)
{
    if ((rows!=5&&rows!=6)||!ValidateFusionGarlicAction(state,rows)) return false;
    if (state.phase != GarlicActionPhase::Moving) return true;
    GarlicRowPlan plan;
    const auto water=[=](int row){return rows==6&&(row==2||row==3);};
    const bool lower=state.originRow>0&&water(state.originRow-1)==water(state.originRow);
    const bool upper=state.originRow+1<rows&&water(state.originRow+1)==water(state.originRow);
    return state.startY == NativeGarlicRowY(state.originRow,rows) && state.targetY == NativeGarlicRowY(state.targetRow,rows) &&
        TryPlanFusionGarlicRow(state.originRow,rows,lower,upper,state.certainRow,plan) &&
        (state.targetRow == plan.randomZeroRow || state.targetRow == plan.randomOneRow);
}
// Keep the stock bank, shovel and Menu layout. Source Unity's bank permits14.
constexpr size_t kNativeFusionBankCapacity = 10;
// Explicit implementation choices; sparse source IDs never index native arrays.
SeedType GetNativeSeedForPlant(FusionPlantId kind);
bool IsNativeBasePlantKind(FusionPlantId kind);
void ApplyNativeSourceRealDamage(Zombie* zombie, int damage);
bool CanApplyNativeFusionGameplay(Board* board);
class NativeVisuals;
class NativeZombieVisuals;
class NativeSourceCharred311;
class NativeCapturedPlants314;
class NativeGloveUIState;
struct FusionCommand;
struct FusionSplitEmissionSpec;
// Metadata extends the real Board entities. This context owns no second world,
// entity arrays, sun balance, collision loop or plant cooldown simulation.
struct NativePlantData
{
    FusionPlantId kind = kFusionNoPlant;
    FusionPlantId firstParent = kFusionNoPlant;
    FusionPlantId secondParent = kFusionNoPlant;
    uint16_t packetId = 0;
    uint8_t owner = 0xff;
    uint8_t randomOrigin = 0;
    int productionTicks = 0;
    int pendingProductionTicks = 0;
    uint8_t producerRewardFlags = 0; // Source SunNut one-shot threshold rewards, bits0/1.
    uint8_t deathEventFlags = 0; // Source PotatoNut consumed HP bands, bits0/1.
    uint8_t damageStage = 0; // Source appearance retained at no-write HP boundaries.
    float idleSeconds = 0.0f;
    float shootSeconds = -1.0f;
    float animationSpeed = 1.0f;
    uint32_t instanceToken = 0; // Opaque host DataArray generation, not a local slot.
    uint32_t batchShotCount = 0; // Source successful-shot sequence, not an animation clock.
    uint16_t batchRageTicks = 0; // Snapshot mirror of the real Plant rage counter.
    uint16_t batchBagCount = 0; // Remaining indistinguishable false draws in the source bag.
    uint8_t batchMode = 0;
    uint16_t waterTargetId = 0;
    uint32_t waterTargetToken = 0;
};

struct NativeProjectileData
{
    uint16_t kind = 0;
    int damage = 20;
    int sunOnHit = 0;
    uint8_t specialType = 0; // Source ordinary0, outbound Split_left1, returned Split2.
    uint8_t fireState = 0; // Source ordinary0, yellow1, orange2, red3; kind remains unchanged.
    uint16_t lastTorchId = 0;
    uint32_t lastTorchToken = 0; // Actor identity survives movement and retirement.
    bool melonEnhanced = false; // Source pot trait captured at emission.
    uint8_t effectVariant = 0; // Source random-colour Puff13, otherwise zero.
    bool isZombieBullet = false; // Source allegiance, independent of movingWay.
    bool hitLatched = false; // Source hostile contact/umbrella latch; never a local prediction.
    bool umbrellaed = false; // Source one-shot friendly Umbrella reinforcement flag.
    bool goldAttracted = false; // Source GoldMagnet one-shot capture; the native projectile remains the actor.
};

struct NativePacketData
{
    uint16_t selectionId = 0;
    FusionPlantId kind = kFusionNoPlant;
    bool isExtra = false;
    uint8_t owner = 0xff;
    uint8_t randomOrigin = 0;
    int price = 0;
    float rechargeSeconds = 0.0f;
};

struct NativeZombieColdData
{
    int level = 0;
    int threshold = 100;
    int chilledTicks = 0; // Snapshot mirror of the real Zombie counter.
    int frozenTicks = 0;  // Never advanced as another timer.
};

struct NativeZombieButterData
{
    int ticks = 0; // Mirror of the existing Zombie counter, never ticked here.
    bool visible = false; // Source may overwrite a visible effect with.5s.
};

struct NativeZombieActionData
{
    uint8_t colorMask = 0; // Seven source Puff_randomColor effects, keyed by full generation.
    float scale = 1.0f; // Mirror of the native mScaleZombie property, not a second transform.
    bool doomOnDeath = false; // Source isDoom tag, consumed once by the real death path.
    bool jalaedOnDeath = false; // Source fire tag; poison may clear it before death.
    int poisonLevel = 0;
    int poisonTicks = 0;
    int poisonDamageTicks = 100;
    int waterGrapTicks = 0;
    int waterGrapCount = 0;
};

struct NativeCoinData
{
    int value = 25;
    bool money = false;
    NativeCoinData() = default;
    NativeCoinData(int amount, bool isMoney = false) : value(amount), money(isMoney) {}
};

// Host-only capture for the real Plant's next throw event. Clients receive
// the resulting native projectile; they do not predict or launch from this.
struct NativeLobberAim
{
    uint32_t targetGeneration = 0;
    int sampleTick = 0;
    float targetCenterX = 0, targetTopY = 0;
    bool targetPlant = false; // Source Thrower may aim at actual CabbageUmbrella.
};

struct NativeGloveHold
{
    uint16_t plantId = 0;
    uint32_t instanceToken = 0;
};

struct NativeContext
{
    NativeContext();
    ~NativeContext();
    uint32_t epoch = 0;
    uint32_t seed = 1;
    int level = 1;
    int money = 3000; // Source Board::.ctor +0xD8; separate from native SUN/profile coins.
    std::vector<int> temporaryLightDeadlines; // Host-only expiries on the real Board tick.
    uint16_t temporaryLightLevel = 0; // Source additive room-light leases; clients only adopt.
    bool validation = true;
    FusionAdventureSession273 adventure;
    FusionAdventureScheduler273State adventureScheduler; // Host-private; never serialized.
    int adventureTick = -1;
    int adventureEnemyCount = 0;
    std::unordered_set<uint32_t> adventureEnemies; // Host generations, retired exactly once on death/MC.
    uint32_t adventureMissionPlantUID = 0; // Host-private full Plant generation for source Tutor4/7; never FNS.
    uint32_t adventureMissionZombieUID = 0; // Host-private full Zombie generation for source Tutor7/8; never FNS.
    int adventureSkySunTicks = 750;
    bool adventureTeardown = false;
    bool selectionCommitted = false;
    bool restartChooserRequested = false;
    uint8_t pendingRosterMask = 0; // Host-only: preserve sealed Intro until private reveal.
    uint8_t eve269ResourceAudit = 0; // Host-local resource cache only:0unknown/1pass/2failed; not FNS or a clock.

    // Classic's raw-saved nine-column arrays retain their original layout.
    GridSquareType lastColumnTerrain[6];
    int lastColumnOffsets[6][2] = {};
    std::unique_ptr<GameButton> fieldButton;
    bool fieldButtonPressed = false;
    bool fieldButtonReleasePending = false;

    // Selection-only controller supplies the existing SeedChooserScreen.
    // Once committed, real SeedPacket instances own the live cooldowns.
    FusionSeedBank selection;
    std::vector<FusionBankEligibleCard> eligible;
    std::unordered_map<uint16_t, NativePacketData> packets;
    std::unordered_map<uint32_t, NativePlantData> plants;
    std::unordered_map<uint32_t, NativeLobberAim> lobberAims;
    std::unordered_map<uint32_t, uint32_t> lobberPlantTargets; // Full projectile generation -> full target Plant generation.
    std::unordered_map<uint32_t, int> lobberPierceLimits; // Host-only emission snapshot.
    std::unordered_map<uint32_t, NativeProjectileData> projectiles;
    std::unordered_map<uint32_t, NativeSuperCherryStart> superCherryStarts; // Host-only native projectile-age continuations.
    std::unordered_map<uint32_t, NativeStar269PlantState> star269Plants;
    std::unordered_map<uint32_t, NativeStar269ProjectileState> star269Projectiles;
    std::unordered_map<uint32_t, NativeDoom269PlantState> doom269Plants;
    std::unordered_map<uint32_t, NativeCompanion269State> companions269;
    std::unordered_map<uint32_t, NativeCompanions269PlantState> companionPlants269;
    std::unordered_map<uint32_t, NativeWorld269PlantState> worldPlants269;
    std::unordered_map<uint32_t, NativeWorld269BeamState> worldBeams269;
    NativeWorldRemainder269BoardState worldRemainder269Board;
    bool goldHypnoSummoned=false;
    std::unordered_map<uint32_t, NativeWorldRemainder269AuxState> worldRemainder269Aux;
    std::unordered_map<uint32_t, NativeWorldRemainder269ShotState> worldRemainder269Shots;
    std::unordered_map<uint32_t, std::vector<NativeDoom269Cloud>> doom269Clouds; // Host-only native-tick deadlines.
    std::unordered_map<uint32_t, FusionUmbrellaBlockState> umbrellaBlocks;
    std::unordered_map<uint32_t, FusionUmbrellaPoseState> umbrellaPoses;
    std::unordered_map<uint32_t, NativeZombieColdData> coldZombies;
    std::unordered_map<uint32_t, NativeZombieButterData> butterZombies;
    std::unordered_map<uint32_t, NativeZombieActionData> zombieActions;
    std::unordered_map<uint32_t, uint32_t> zombieInstanceTokens; // Local full generation -> opaque host generation.
    std::unordered_map<uint32_t, GarlicActionState> garlicZombies;
    std::unordered_map<uint32_t, NativeSourceZombieData> sourceZombies;
    std::unordered_map<uint32_t, uint64_t> sourceZombieBirthOrdinals; // HOST actual birth order, not sparse IDs.
    uint64_t sourceZombieBirthSequence=0;
    std::unordered_set<uint32_t> sourceGraphClocks313; // LOCAL full R generations; no source clock is duplicated.
    std::unordered_set<uint32_t> sourceZombieInitializations313; // HOST synchronous factory scope only.
    std::unordered_set<uint32_t> sourceDeathPoints313; // HOST once-only source Die points per LOCAL generation.
    std::unordered_map<uint32_t, NativeSourceGraph313State> sourceGraphs313;
    std::unordered_map<uint32_t, NativeWaterZombie313State> waterZombies313;
    std::unordered_map<uint32_t, NativeExtendedZombie313State> extendedZombies313;
    std::unordered_map<uint32_t, NativeHeavyZombie313BState> heavyZombies313B;
    float sourcePoints=500.f; // Source Board.thePoints124, distinct from native sun and Fusion money.
    FusionExtendedBoard313State extendedBoard313;
    uint32_t sourceAnimatedSerial317=0; // HOST cosmetic birth identity; never a simulation clock.
    std::unordered_map<uint32_t, NativeSourceObstacle313Data> sourceObstacles313;
    std::unique_ptr<NativeSourceObstacle313Visuals> sourceObstacleVisuals313;
    std::unordered_map<uint32_t, NativeDayZombie273State> dayZombies273;
    std::unordered_map<uint32_t, NativeNightZombie281State> nightZombies281;
    std::unordered_map<uint32_t, NativeJackbox300State> jackboxZombies300;
    std::unique_ptr<LawnNet::NetJackbox309Audio> jackboxAudio309;
    std::unique_ptr<NativeSourceCharred311> sourceCharred311;
    std::unique_ptr<NativeCapturedPlants314> capturedPlants314;
    std::unordered_map<uint32_t, NativeSourceZombieCursor> sourceZombieCursors; // Host-only consumed native animation spans.
    FusionRollingHitHistory rollingHits; // Host-only full-generation impact history.
    std::unordered_map<uint32_t,std::unordered_set<uint32_t>> fireContacts; // Current native contact episodes only.
    std::array<NativeGloveHold, kFusionBankPlayers> gloveHolds{};
    int gloveCooldownTicks = 0;
    int gloveTick = -1; // Cache of Board.mMainCounter; not another simulation clock.
    std::unique_ptr<NativeGloveUIState> gloveUI;
    std::unordered_map<uint32_t, NativeCoinData> coinValues;
    std::unordered_map<uint32_t, NativeEquipmentData> equipmentCoins;
    std::unordered_map<uint32_t, NativeDroppedCard269Data> droppedCards269;
    std::unordered_map<uint32_t, NativeFertilize269Data> fertilizeCoins269;
    std::unordered_map<uint32_t, NativeRollingActor269State> rollingPlants269;
    std::unordered_map<uint32_t, NativeWheat269State> wheatPlants269;
    std::unordered_map<uint32_t, NativeEquipmentPlantState> equipmentPlants;
    std::unique_ptr<class NativeSourceEffects269> sourceEffects269;
    std::unique_ptr<NativeVisuals> visuals;
    std::unique_ptr<NativeZombieVisuals> zombieVisuals;
};

bool InitializeNativeBoardContext(Board* board, uint32_t epoch, uint32_t seed,
    int level, int difficulty, uint8_t activeMask);
void SyncNativeSelectionPackets(Board* board);
bool HandleNativeFusionCommand(Board* board, int slot, const FusionCommand& command);
bool AdvanceNativeSelection(Board* board);
bool BeginNativePlaying(Board* board);
bool ReconcileNativePlayers(Board* board, uint8_t activeMask);
void UpdateNativeFieldButton(Board* board);
bool NativeFieldToggleAvailable277(const Board* board);
bool HandleNativeFieldPointer(Board* board, bool down, int button, int x, int y);
void PruneNativeMetadata(Board* board);

bool IsNativeFusionBoard(const Board* board);
NativeContext* GetNativeContext(Board* board);
const NativeContext* GetNativeContext(const Board* board);
NativePlantData* GetNativePlantData(Plant* plant);
const NativePlantData* GetNativePlantData(const Plant* plant);
NativeProjectileData* GetNativeProjectileData(Projectile* projectile);
const NativeProjectileData* GetNativeProjectileData(const Projectile* projectile);
NativeZombieColdData* GetNativeZombieColdData(Zombie* zombie);
NativeSourceZombieData* GetNativeSourceZombieData(Zombie* zombie);
const NativeSourceZombieData* GetNativeSourceZombieData(const Zombie* zombie);
bool AttachNativeSourceZombieData(Board* board, Zombie* zombie, const NativeSourceZombieData& data);
// Existing native NORMAL controls plus explicitly typed source ground families.
bool IsNativeFusionGroundZombie(const Zombie* zombie);
int GetNativeFusionZombieKind(const Zombie* zombie);
int GetNativeFusionZombieStatus(const Zombie* zombie);
float GetNativeFusionZombieHealth(const Zombie* zombie);
const NativeZombieColdData* GetNativeZombieColdData(const Zombie* zombie);
NativeZombieColdData* GetOrCreateNativeZombieColdData(Zombie* zombie, bool includeRetiring = false);
NativeZombieButterData* GetNativeZombieButterData(Zombie* zombie);
const NativeZombieButterData* GetNativeZombieButterData(const Zombie* zombie);
NativeZombieButterData* GetOrCreateNativeZombieButterData(Zombie* zombie);
bool AttachNativeZombieButterData(Board* board, Zombie* zombie, const NativeZombieButterData& data);
bool AttachNativeZombieColdData(Board* board, Zombie* zombie, const NativeZombieColdData& data);
const NativePacketData* GetNativePacketData(const SeedPacket* packet);
bool AttachNativePlant(Board* board, Plant* plant, const NativePlantData& data);
bool AttachNativeProjectile(Board* board, Projectile* projectile, const NativeProjectileData& data);
// Allocation only. The caller publishes its typed auxiliary metadata before
// returning; no BulletID, position container or independent clock is created.
Projectile* CreateNativeAuxiliary269Carrier(Board* board,int row,float centerX,float centerY);
bool AttachNativeCoinValue(Board* board, Coin* coin, int value, bool money = false);
int GetNativeCoinValue(const Coin* coin, int fallback);
bool IsNativeMoneyCoin(const Coin* coin);
bool ClaimNativeMoneyCoinValue(Coin* coin, int& value);
NativeZombieActionData* GetNativeZombieActionData(Zombie* zombie);
const NativeZombieActionData* GetNativeZombieActionData(const Zombie* zombie);
NativeZombieActionData* GetOrCreateNativeZombieActionData(Zombie* zombie);
bool AttachNativeZombieActionData(Board* board, Zombie* zombie, const NativeZombieActionData& data);
GarlicActionState* GetNativeZombieGarlicData(Zombie* zombie);
const GarlicActionState* GetNativeZombieGarlicData(const Zombie* zombie);
GarlicActionState* GetOrCreateNativeZombieGarlicData(Zombie* zombie);
bool AttachNativeZombieGarlicData(Board* board, Zombie* zombie, const GarlicActionState& data);
void PruneNativeZombieGarlicData(Board* board);

// Hooks execute within the existing native entity lifecycle, never alongside
// a separate FusionSimulation::Step or independent renderer world.
void NativePlantInitialized(Plant* plant);
void NativePlantHealthChanged(Plant* plant);
void UpdateNativePlant(Plant* plant);
void NativeProjectileCreated(Plant* source, Projectile* projectile, const FusionSplitEmissionSpec* fusionEmission = nullptr);
int GetNativeProjectileDamage(const Projectile* projectile, int fallback);
void NativeProjectileHit(Projectile* projectile, bool hitZombie);
// Source-normal Cherry rules currently cover the real native basic zombie.
bool ApplyNativeFusionCherryDamage(Zombie* zombie);
bool IsNativeFusionPotato(const Plant* plant);
bool NativeFusionPotatoHasTrigger(Plant* plant);
bool ApplyNativeFusionPotatoDamage(Zombie* zombie, int damage = 1800);
bool ApplyNativeSourceExplosionDamage(Zombie* zombie,int damage);
void ApplyNativeSourceSquashDamage(Zombie* zombie,int damage);
}

#endif
