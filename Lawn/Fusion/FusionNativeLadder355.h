#pragma once
class Zombie;class Plant;
namespace LawnFusion
{
// Source Pole GetLadder slot23; never deals contact damage or allocates actors.
bool ContactNativeDayLadder355(Zombie*,Plant*);
bool MoveNativeDayLadder355(Zombie*);
void RecoverNativeDayLadder355(Zombie*);
// Host-only: skip stock lane-Y correction while the actual source branch owns Y.
bool NativeDayLadderPositionOwned355(const Zombie*);
}
