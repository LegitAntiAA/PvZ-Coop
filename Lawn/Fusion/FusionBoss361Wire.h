#pragma once
#include "FusionNativeBossRoom366.h"
#include <cmath>
namespace LawnFusion
{
// Canonical fields only. Native local UID/tick/once-only event cursors stay host-private.
template<class IO>void FieldsBoss361(IO& io,FusionBoss361View& v)
{
 io.U16(v.kind);uint8_t phase=uint8_t(v.phase);io.U8(phase);v.phase=FusionBoss361Phase(phase);
 io.U8(v.sourceStatus);io.U8(v.targetRow);io.U8(v.targetColumn);io.U8(v.ballType);io.U8(v.bodyStage);io.U8(v.eyeMask);
 io.I32(v.summonCount);io.I32(v.spawnCount);io.I32(v.summonBallCount);io.F32(v.summonTime);io.F32(v.ballCountdown);
 io.U8(v.bodyHistoryMask);io.F32(v.rvOffsetX);io.F32(v.rvOffsetY);io.Bool(v.rvAligned);io.F32(v.eyeBlackSeconds);
 io.Bool(v.ballBusy);io.Bool(v.colliderEnabled);io.Bool(v.rvVisible);io.Bool(v.dying);
}
template<class IO>void FieldsBossRoom366(IO& io,NativeBossRoom366& v)
{
 io.Bool(v.enabled);io.Bool(v.boss2);io.Bool(v.fastBoard);io.Bool(v.hammerGranted);io.Bool(v.gloveGranted);
 io.Bool(v.fakeTrophyNext);io.Bool(v.won);io.Bool(v.trophyFake);io.Bool(v.trophyClaimed);
 io.U16(v.hammerCooldownTicks);io.U16(v.trophyId);io.U32(v.trophyToken);io.U32(v.trophyAnimationBaseAge);io.I32(v.enemyCount);io.F32(v.trophyStartX);io.F32(v.trophyStartY);
}
inline bool NativeBossRoom366Valid(const NativeBossRoom366& v)
{return v.hammerCooldownTicks<=6000&&v.enemyCount>=0&&v.enemyCount<=1000000&&bool(v.trophyId)==bool(v.trophyToken)&&
 (!v.trophyClaimed||v.trophyId)&&std::isfinite(v.trophyStartX)&&std::isfinite(v.trophyStartY)&&std::abs(v.trophyStartX)<=4096&&std::abs(v.trophyStartY)<=4096;}
}
