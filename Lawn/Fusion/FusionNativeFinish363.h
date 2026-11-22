#pragma once
#include "FusionSourceGraph313.h"
#include "FusionSourceDeathBudget319.h"
#include <string>
class Zombie;class Board;class Projectile;class Plant;
namespace LawnFusion {
struct NativeSourceObstacle313Data;
constexpr bool FusionFinish363Kind(uint16_t k){return k==115||k==116||k==117;}
const FusionSourceGraph313* FindFusionFinish363Graph(uint16_t);
bool SourceRootDeltaFinish363(uint16_t,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
bool EnsureNativeFinish363Resources(Board*,uint16_t,std::string* error=nullptr);
bool InitializeNativeFinish363(Zombie*);
bool UpdateNativeFinish363(Zombie*);
bool CanApplyNativeSourceFireLine363(Board*,int row,bool hostile,std::string* error=nullptr);
bool ApplyNativeSourceFireLine363(Board*,int row,bool hostile);
bool InitializeNativeSquashItem363(Projectile*,uint8_t column,bool fire);
bool UpdateNativeSquashItem363(Projectile*);
bool NativeSquashItem363Pose(const Projectile*,const NativeSourceObstacle313Data&,const char*& state,float& samplerSeconds);
}
