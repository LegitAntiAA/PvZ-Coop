#ifndef LAWN_FUSION_NATIVE_LASER_CONNECTIONS269_H
#define LAWN_FUSION_NATIVE_LASER_CONNECTIONS269_H
class Board;class Plant;
namespace LawnFusion
{
// Source Lawnf.ConnectPlant(column): disconnect the entire actual column,
// then match adjacent same-kind rows from top to bottom, without chains.
// Event-driven, host authoritative; never called by a per-tick scan.
bool RebuildNativeLaserConnections269Column(Board*,int column);
// Call after source actor registration/init, removal (mDead already set), or
// completed native glove move. Applies to any source kind in that column.
void NativeLaserConnections269PlantPlaced(Plant*);
void NativeLaserConnections269PlantRemoving(Plant*);
void NativeLaserConnections269PlantMoved(Plant*,int previousColumn);
// Source external ball centre, not the parent's fightball node or editor pos.
// Same mutual full-generation FNS references on host and client; no new actor.
bool GetNativeLaserConnections269BallPoint(const Plant*,float& x,float& y,int& sortingRow);
}
#endif
