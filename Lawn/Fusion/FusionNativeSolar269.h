#ifndef LAWN_FUSION_NATIVE_SOLAR_269_H
#define LAWN_FUSION_NATIVE_SOLAR_269_H
#include "FusionSolar269Rules.h"
class Board;class Plant;
namespace LawnFusion
{
// Root implements these against its actual source Board/Solar world. Unavailable
// means not implemented, not an invented inactive singleton.
FusionSolar269WorldBranch QueryNativeSolar269WorldBranch(const Board*);
bool CanCreateNativeSolar269SuperParticle(const Board*);
bool CreateNativeSolar269SuperParticle(Plant*,float centerX,float centerY);
// Synchronous source callback after successful preflight: actual Solar.SetDamage
// or real Board solarCountdown=min(before,.5), and original source sound66.
// UI/auth remain Root-owned.
void CommitNativeSolar269World(Plant*,FusionSolar269WorldBranch);
bool NativeSolar269TravelUltimate22(const Plant*);
bool NativeSolar269ShootingBoard(const Board*);
bool IsNativeSolar269(const Plant*);
float GetNativeSolar269LobberInterval(Plant*);
bool UpdateNativeSolar269(Plant*);
bool HandleNativeSolar269LobberEvent(Plant*);
bool CanActivateNativeSolar269Skill(const Plant*);
bool TryActivateNativeSolar269Skill(Plant*);
int GetNativeSolar269SourceStatus(const Plant*);
int GetNativeSolar269BackOrder(const Plant*);
bool GetNativeSolar269VisualPose(const Plant*,const char*& state,float& seconds);
}
#endif
