#pragma once
#include "FusionNative.h"
#include "FusionTravelBuff360Rules.h"
#include <cmath>
namespace LawnFusion {
inline bool NativeTravelSteel370Present(const NativeProjectileData& d)
{return d.sourceSteelAttack370!=0||d.sourceSteelFactor370!=0||d.sourceSteelHits370!=0;}
inline bool NormalizeNativeTravelSteel370(const NativeProjectileData& in,NativeProjectileData& out)
{
 if(in.kind!=93||in.isZombieBullet||in.fireState||in.umbrellaed||
  (in.sourceSteelAttack370!=120&&in.sourceSteelAttack370!=144&&in.sourceSteelAttack370!=180&&in.sourceSteelAttack370!=204)||
  !std::isfinite(in.sourceSteelFactor370)||in.sourceSteelFactor370<1||in.sourceSteelFactor370>4||in.sourceSteelHits370>12)return false;
 //337700: catalog AdjustDamage -> Ultimate20 x3 -> low-HP factor -> one F32 truncation.
 int exact=int((float(in.sourceSteelAttack370)*3.f)*in.sourceSteelFactor370);
 int ordinary=int(120.f*in.sourceSteelFactor370);
 //43CFA0: every actual HitZombie truncates both immutable provenance channels.
 // Maximum birth is204*3*4=2448, hence12 halvings make the shot inert.
 for(unsigned n=0;n<in.sourceSteelHits370;++n){exact=int(float(exact)*.5f);ordinary=int(float(ordinary)*.5f);}
 if(in.damage!=exact||in.sourceUnmodifiedDamage369!=ordinary)return false;
 auto copy=in;copy.damage=ordinary;copy.sourceUnmodifiedDamage369=0;
 copy.sourceSteelAttack370=0;copy.sourceSteelFactor370=0;copy.sourceSteelHits370=0;out=copy;return true;
}
inline bool NativeTravelSteel370LedgerValid(const NativeProjectileData& in,const FusionTravelBuff360Ledger* ledger)
{
 NativeProjectileData normalized;
 if(!NormalizeNativeTravelSteel370(in,normalized)||!ledger||!FusionTravelBuff360LedgerValid(*ledger)||!HasFusionTravelUltimate360(*ledger,20))return false;
 if((in.sourceSteelAttack370==144||in.sourceSteelAttack370==204)&&!HasFusionTravelAdvanced360(*ledger,10))return false;
 if((in.sourceSteelAttack370==180||in.sourceSteelAttack370==204)&&!HasFusionTravelAdvanced360(*ledger,33))return false;
 return true;
}
}
