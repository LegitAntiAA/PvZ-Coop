#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeTravelBuff369.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include <limits>
namespace LawnFusion {
bool AttachNativeTravelProjectile369(Plant* source,Projectile* projectile,NativeProjectileData data)
{
 const auto* plant=GetNativePlantData(source);
 if(!source||!plant||!projectile||projectile->mBoard!=source->mBoard||source->mNetGhost||
  !source->mApp||source->mApp->NetIsClient()||projectile->mNetGhost)return false;
 const auto* def=FusionFindPlant(plant->kind);if(!def)return false;
 // Preserve the source attack coefficient after parent AdjustDamage.
 // The baseline tuple remains subject to the existing strict metadata checks.
 int multiplier=1;
 // Existing source emission rules already produced and strictly validate the
 // baseline tuple. Preserve an exact integer source attack coefficient (large
 // Doom6, connected star/light levels, etc.) after parent AdjustDamage.
 if(def->damage>0&&data.damage>0&&data.damage%def->damage==0)multiplier=data.damage/def->damage;
 const int64_t base=int64_t(def->damage)*multiplier;
 const bool fromAttack=def->hasStats&&def->damage>0&&base==data.damage;
 const bool advanced10=fromAttack&&HasNativeTravelAdvanced369(source->mBoard,10);
 const bool advanced17=fromAttack&&plant->kind==908&&data.kind==18&&HasNativeTravelAdvanced369(source->mBoard,17);
 if(!data.sourceUnmodifiedDamage369&&(advanced10||advanced17))
 {
  int attack=0;if(!TryFusionTravelProjectileDamage369(def->damage,data.kind,advanced10,advanced17,attack))return false;
  const int64_t final=int64_t(attack)*multiplier;if(final>(std::numeric_limits<int>::max)())return false;
  if(final!=data.damage){data.sourceUnmodifiedDamage369=data.damage;data.damage=int(final);}
 }
 if(!NativeTravelProjectileLedgerValid369(data,GetNativeTravelBuff369(source->mBoard))||
  !AttachNativeProjectile(source->mBoard,projectile,data))return false;
 // IceSpark.SetPenetrationTime captures Advanced17 at actual birth. Reuse the
 // existing host-only full-generation projectile limit map; clients never hit.
 if((data.kind==18||data.kind==24)&&HasNativeTravelAdvanced369(source->mBoard,17))
  GetNativeContext(source->mBoard)->lobberPierceLimits[source->mBoard->mProjectiles.DataArrayGetID(projectile)]=(std::numeric_limits<int>::max)();
 if(data.kind==87&&HasNativeTravelAdvanced369(source->mBoard,31))
  GetNativeContext(source->mBoard)->lobberPierceLimits[source->mBoard->mProjectiles.DataArrayGetID(projectile)]=1000000;
 return true;
}
}
