#pragma once
#include <array>
#include <cmath>
namespace LawnFusion
{
// IceExplodeControl.Start309FF0 saves the original SpriteRenderer.color.
// FixedUpdate309FA0 subtracts .04 alpha per genuine 20ms physics step.
// Tod emitter age is the existing native 10ms clock; no visual timer is stored.
inline bool SourceIceExplosionColor366(int nativeEmitterAge,const std::array<float,4>& original,std::array<float,4>& out)
{
 if(nativeEmitterAge<0)return false;
 for(float value:original)if(!std::isfinite(value))return false;
 auto next=original;next[3]-=.04f*float(nativeEmitterAge/2);out=next;return true;
}
}
