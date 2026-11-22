#pragma once
#include "FusionSourceObstacle313.h"
#include <string>
class Zombie;class Board;
namespace LawnFusion
{
// Coordinates come from the SAME actual source model ballPosition/land sampler.
// Room geometry owns this read-only projection; no shadow/muzzle approximation.
bool MakeNativeBossBall366Request(Zombie*,uint8_t variant,int row,float muzzleBoardX,float muzzleBoardY,
 float targetGroundBoardY,bool fastBoardMode,int rowLayerNativeOrder,FusionSourceObstacle313Spawn&,std::string* error=nullptr);
bool EmitNativeBossBall366(Zombie*,const FusionSourceObstacle313Spawn&,float muzzleBoardX,float muzzleBoardY,std::string* error=nullptr);
bool RetireNativeSourceZombieBalls366(Board*); // Source DieEvent filters actual obstacleType0/1: Machine4 +Boss6.
}
