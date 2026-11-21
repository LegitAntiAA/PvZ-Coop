#ifndef LAWN_FUSION_NATIVE_MORE_PLANTS269_BACKEND_H
#define LAWN_FUSION_NATIVE_MORE_PLANTS269_BACKEND_H
#include "FusionMorePlants269BackendRules.h"
class Plant;class Projectile;
namespace LawnFusion
{
// Before the ordinary movement chain; handled lobber-class way0 uses the real
// planar body and its existing typed Lobber hit/collider helpers, no fake lob.
bool IsNativeMore269Projectile(const Projectile*);
bool MoveNativeMore269PlanarLobber(Projectile*);
bool GetNativeMore269SquashVisualPose(const Plant*,const char*&,float&);
}
#endif
