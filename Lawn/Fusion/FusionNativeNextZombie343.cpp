#include "FusionNativeNextZombie343.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionNativeDrownWeapon313.h"
#include "FusionNativeDrown313Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNative.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include <cmath>
namespace LawnFusion
{
bool ShootNativeCherryCatapult343(Zombie* z,NativeHeavyZombie313BState& s,Plant* p,float x,float y,const NativeHeavyZombie313BHooks& h)
{
 const auto* d=GetNativeSourceZombieData(z);FusionCatapult343Shot plan;
 if(!z||!d||d->kind!=112||z->mNetGhost||z->mApp->NetIsClient()||!h.graphStateFor||!h.sound||
    !TryPlanFusionCatapult343Shot(112,s.ammo,plan))return false;
 auto* g=h.graphStateFor(z);if(!g)return false;
 if(!TryMakeNativeHostileCatapult343(z->mBoard,p,z->mRow,x,y,plan.projectile))return false;
 s.ammo=plan.remaining;s.targetPlantUID=0;
 // Source bool shoot2 is true after five shots. Stored in the existing graph
 // bool Wait slot for THIS graph only; no canonical/second clock field.
 g->parameters.wait=plan.cherryStage;
 if(plan.walk2)g->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Walk2);
 h.sound(z,101);return true;
}
bool ShootNativeDrownCatapult343(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{
 const auto* d=GetNativeSourceZombieData(z);float x=0,y=0;
 if(!d||d->kind!=216||!s.ammo||s.ammo>2||!h.sound||!GetNativeSourceZombie269NodePoint(z,"Shoot",x,y))return false;
 const auto& weapon=GetNativeDrownWeapon313Hooks();
 if(!CanSpawnNativeDrownVolley343(z,x,y,weapon))return false;
 --s.ammo; // Source decrements even on MC, before spawning its entire volley.
 if(!SpawnNativeDrownVolley343(z,x,y,weapon))return false;
 h.sound(z,101);if(!s.ammo)s.catapultFlags343|=4;return true;
}
bool ApplyNativeDrownCatapult343Body(Zombie* z,NativeHeavyZombie313BState& s,int damage,const NativeHeavyZombie313BHooks& h)
{
 auto* d=GetNativeSourceZombieData(z);if(!d||d->kind!=216||damage<0||!h.sourceDie||!std::isfinite(d->health-float(damage)))return false;
 d->health-=damage;z->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);
 if(d->health<float(z->mBodyMaxHealth/2))s.catapultFlags343|=1;
 if(d->health<=0)h.sourceDie(z,2);return true;
}
}
