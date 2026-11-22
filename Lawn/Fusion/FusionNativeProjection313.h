#pragma once
class Board;class Zombie;
namespace LawnFusion
{
// Logical source grid/overlap projection. Collider303's actual model-root
// 60*mScale basis is separate and must not use these pixel-per-cell units.
bool NativeSourceRowLandY313(const Board*,float sourceX,int row,float& sourceY);
bool NativeSourceBounds313(const Board*,float& minX,float& maxX,float& maxY);
// Native ground shadow, explicitly without airborne altitude displacement.
// Does not sample source models or change the real native actor.
bool NativeSourceGroundAnchor313(const Zombie*,float& boardX,float& boardY);
bool NativeBoardPointToSource313(const Board*,int row,float nativeGroundY,
    float boardX,float boardY,float& sourceX,float& sourceY);
bool NativeSourcePointToBoard313(const Board*,int row,float nativeGroundY,
    float sourceX,float sourceY,float& boardX,float& boardY);
// Semantic axis is source Zombie.Awake's Shadow.transform. Other names are
// genuine source model paths. Returns the same pose as Draw, including air Y.
bool NativeSourcePoint313(const Zombie*,const char* semanticOrPath,float& sourceX,float& sourceY);
// NORMAL clock carrier only. Adjusts actual pos/rowY/altitude atomically;
// native Board.GetPosYBasedOnRow remains the row/roof substrate.
bool SetNativeSourceAxis313(Zombie*,float sourceX,float sourceY);
}
