#pragma once
#include "FusionAdventureLesson346Rules.h"
#include <string>
class Board;
namespace LawnFusion
{
bool PreflightNativeFusionAdventureLesson346(Board*,int level,std::string* error=nullptr);
// Host only, exact authored operation order after complete resource/pool and
// removal-callback preflight. Failed post-mutation invariant stops this run.
bool ExecuteNativeFusionAdventureLesson346(Board*,int level,uint8_t sourceStage,
    uint32_t& missionZombieUID,std::string* error=nullptr);
void ObserveNativeFusionAdventureLesson346(Board*,FusionAdventureLessonInput274&);
}
