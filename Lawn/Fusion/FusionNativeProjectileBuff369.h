#pragma once
#include "FusionNative.h"
#include "FusionTravelBuff369Rules.h"
#include "FusionNativeTravelSteel370.h"
class Plant;class Projectile;class Board;
namespace LawnFusion {
// Structural normalization is independent of world/RNG/owner lifetime. The
// existing strict native tuple validator still validates this normalized copy.
inline bool NormalizeNativeTravelProjectile369(const NativeProjectileData& in,NativeProjectileData& out)
{
 if(NativeTravelSteel370Present(in))return NormalizeNativeTravelSteel370(in,out);
 if(!in.sourceUnmodifiedDamage369){out=in;return true;}
 if(in.isZombieBullet||!(FusionTravelProjectilePair369(in.sourceUnmodifiedDamage369,in.damage,in.kind,true,true,in.fireState)||
  in.umbrellaed&&FusionTravelRelayedProjectilePair369(in.sourceUnmodifiedDamage369,in.damage,in.kind,true,true,true)))return false;
 auto copy=in;copy.damage=in.sourceUnmodifiedDamage369;copy.sourceUnmodifiedDamage369=0;out=copy;return true;
}
inline int NativeProjectileValidationDamage369(const NativeProjectileData& in)
{NativeProjectileData copy;return NormalizeNativeTravelProjectile369(in,copy)?copy.damage:-1;}
// Root's final FNS joint validator must call this with the INCOMING app ledger,
// not the previous local Board's ledger. Current context is used only at birth.
inline bool NativeTravelProjectileLedgerValid369(const NativeProjectileData& in,const FusionTravelBuff360Ledger* ledger)
{
 if(NativeTravelSteel370Present(in))return NativeTravelSteel370LedgerValid(in,ledger);
 if(!in.sourceUnmodifiedDamage369)return true;
 return ledger&&FusionTravelBuff360LedgerValid(*ledger)&&!in.isZombieBullet&&
  (FusionTravelProjectilePair369(in.sourceUnmodifiedDamage369,in.damage,in.kind,
   HasFusionTravelAdvanced360(*ledger,10),HasFusionTravelAdvanced360(*ledger,17),in.fireState)||
   in.umbrellaed&&FusionTravelRelayedProjectilePair369(in.sourceUnmodifiedDamage369,in.damage,in.kind,
    HasFusionTravelAdvanced360(*ledger,10),HasFusionTravelAdvanced360(*ledger,17),HasFusionTravelAdvanced360(*ledger,24)));
}
// Genuine catalog attack emissions only. No generic TakeDamage scaling, no
// reapplication to already-born/converted shots and no local ghost mutation.
bool AttachNativeTravelProjectile369(Plant*,Projectile*,NativeProjectileData);
}
