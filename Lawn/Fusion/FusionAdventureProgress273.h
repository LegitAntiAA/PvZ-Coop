#pragma once
#include <cstdint>

namespace LawnFusion
{
constexpr uint16_t kFusionAdventureLevels273=45;
constexpr uint16_t kFusionAdventureRewardTicks273=425; // Source1.25s tween +3s GoBack delay.
enum class FusionAdventureMessage273:uint8_t
{
    None,Welcome,NewPlants,Resources,PlantSunflower,FusePeaSunflower,BuildGatling,Ready,
    GloveUnlocked,GloveMove,MoveTwinFlower,GloveReady,
    PresentIntro,UsePresent,PresentReady,CherryPaperWarning,PresentDefenseReady,
    EquipmentIntro,BucketNutWarning,UseBucket,IronPlants,EquipmentReady,
    PuffIntro,PuffStack,PuffMix,PuffReady
};
enum class FusionAdventureMissionAction274:uint8_t
{None,Prepare4,Prepare7,Spawn7EnemyAndCard,Prepare8,Prepare10,Mix10};

// Host observations of the exact bound mission actors, not any matching type.
// beforeDying/status1 differ from actual destruction in the source lessons.
struct FusionAdventureLessonInput274
{
    bool hasMissionPlant=false;
    int missionPlantColumn=0;
    bool hasMissionZombie=false;
    bool missionZombieBeforeDying=false,missionZombieDying=false;
    bool hasBucketPlant1020or1029=false;
};

// Persistent source profile. Bit(level-1) is advLevelCompleted[level].
// Neither Classic progress nor the retired native50 profile belongs here.
struct FusionAdventureProfile273
{
    uint64_t completedLevels=0;
    uint32_t cycle=1;
    uint16_t frontierLevel=1;
};
struct FusionAdventureTutorial273
{
    uint16_t timerTicks=0,messageTicks=0;
    uint8_t stage=0;
    FusionAdventureMessage273 message=FusionAdventureMessage273::None;
    bool active=false;
};
// Room view, explicitly serialized by the owner. No native pointers/array IDs.
// trophyToken is the HOST Coin generation, also carried by authenticated input.
struct FusionAdventureRun273
{
    uint32_t cycle=0;
    uint16_t level=0;
    FusionAdventureTutorial273 tutorial;
    uint16_t trophyNetId=0,rewardTicks=0;
    uint32_t trophyToken=0;
    float trophySpawnX=0.0f,trophySpawnY=0.0f; // Native projected/clamped source death anchor; permits allocation retry.
    bool replay=false,over=false,rewardCollected=false,transitionReady=false,transitionTaken=false;
};
struct FusionAdventureTutorialEffects273
{
    bool messageChanged=false,grantMissionCards=false,completed=false;
    uint16_t nextWaveTicks=0;
    FusionAdventureMissionAction274 missionAction=FusionAdventureMissionAction274::None;
};
struct FusionAdventureTransition273
{
    bool returnToSelector=false;
    uint16_t nextLevel=0;
    uint32_t nextCycle=0;
};

bool FusionAdventureProfileValid273(const FusionAdventureProfile273&);
bool FusionAdventureRunValid273(const FusionAdventureRun273&);
bool FusionAdventureLevelCompleted273(const FusionAdventureProfile273&,int level);
bool CanEnterFusionAdventureLevel273(const FusionAdventureProfile273&,int level);
bool FusionAdventureBaseUnlocked273(const FusionAdventureProfile273&,uint16_t kind);
bool BeginFusionAdventureRun273(const FusionAdventureProfile273&,int level,bool disableTutorial,FusionAdventureRun273&);
// One source switch case per call, including a large dt. Inputs describe the
// actual living native plants, independent of owner/cell/carrier SeedType.
bool StepFusionAdventureTutorial273(FusionAdventureRun273&,uint32_t dtTicks,
    bool hasSunflower1,bool hasPeaSunflower1000,bool hasGatling1032,FusionAdventureTutorialEffects273&);
// Source4/7/8/10. Execute missionAction against the real board before committing a
// candidate run; failed allocation must not advance the stage. The older bool
// overload rejects active lessons, so an unchanged source1 adapter fails closed.
bool StepFusionAdventureTutorial273(FusionAdventureRun273&,uint32_t dtTicks,
    const FusionAdventureLessonInput274&,FusionAdventureTutorialEffects273&);
bool FusionAdventureCardsIgnoreExpiry273(const FusionAdventureRun273&);
uint16_t FusionAdventureMessageDuration273(FusionAdventureMessage273);
// Call AFTER a non-mindcontrolled enemy death decrements the source enemy count.
// Sets over before allocation; no progress or WIN is committed by this event.
bool TryArmFusionAdventureReward273(FusionAdventureRun273&,int wave,int maximumWave,int enemyCount,bool isIZ);
bool BindFusionAdventureTrophy273(FusionAdventureRun273&,uint16_t netId,uint32_t hostToken);
bool IsFusionAdventureTrophy273(const FusionAdventureRun273&,uint16_t netId,uint32_t hostToken);
// Host only at the adapter. Pure rule changes flags/frontier exactly once on a
// matching trophy CLICK, never on spawn, last kill, animation or next-level load.
bool TryCollectFusionAdventureReward273(FusionAdventureProfile273&,FusionAdventureRun273&,uint16_t netId,uint32_t hostToken);
bool StepFusionAdventureReward273(FusionAdventureRun273&,uint32_t dtTicks);
bool TakeFusionAdventureTransition273(const FusionAdventureProfile273&,FusionAdventureRun273&,FusionAdventureTransition273&);
}
