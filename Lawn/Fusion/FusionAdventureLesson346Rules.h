#pragma once
#include "FusionAdventureProgress273.h"
#include <span>
namespace LawnFusion
{
struct FusionAdventureSession273;
enum class FusionLesson346OpKind:uint8_t {Clear,Plant,Zombie,Money,ClampDifficulty};
struct FusionLesson346Op
{
    FusionLesson346OpKind operation;
    uint16_t kind=0;
    int column=0,row=0,value=0;
    float sourceX=0;
};
enum class FusionLesson346Condition:uint8_t {Always,MissionZombieDestroyed,NoEnemies,GoldCabbage};
struct FusionLesson346Step
{
    FusionAdventureMessage273 message;
    uint16_t timerTicks,messageTicks;
    FusionLesson346Condition condition=FusionLesson346Condition::Always;
    bool complete=false;
    std::span<const FusionLesson346Op> operations;
};
std::span<const FusionLesson346Step> FusionAdventureLesson346Steps(int level);
bool FusionAdventureLesson346Supported(int level);
bool FusionAdventureLesson346TutorialValid(const FusionAdventureTutorial273&,int level);
uint16_t FusionAdventureLesson346MessageDuration(FusionAdventureMessage273);
bool StepFusionAdventureLesson346(FusionAdventureRun273&,uint32_t ticks,
    const FusionAdventureLessonInput274&,FusionAdventureTutorialEffects273&);
// Generation difficulty/waves remain immutable. Source42 changes the runtime
// global only AFTER successful case4 (canonical next-case stage5).
uint8_t FusionAdventureRuntimeDifficulty346(const FusionAdventureSession273&);
}
