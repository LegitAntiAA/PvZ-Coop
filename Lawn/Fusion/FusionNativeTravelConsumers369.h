#pragma once
#include "FusionTravelBuff369Rules.h"
class Board;class Plant;
namespace LawnFusion {
bool HasNativeTravelAdvanced369(const Board*,unsigned);
bool HasNativeTravelUltimate369(const Board*,unsigned);
int GetNativeTravelPlantAttack369(const Plant*,int catalogBase);
bool NativeTravelConnected369(const Plant*);
int GetNativeTravelLight369(const Board*);
int GetNativeTravelCardStep369(const Board*);
float GetNativeTravelCardSeconds369(const Board*,uint16_t,float ordinary);
int GetNativeTravelHammerStep369(const Board*);
// Apply source balance rules to the real Board balance, host only. multiplier
// models the source r argument; ordinary native sun pickup passes1.
bool CreditNativeTravelSun369(Board*,int count,int multiplier=1);
}
