#include "FusionNativeTravelUltimate370.h"
#include "FusionNativeTravelConsumers369.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeFinish363.h"
#include "FusionNativeZombies.h"
#include "FusionChomper268Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion {
namespace {
bool Has(const Plant* p,unsigned bit){return p&&GetNativePlantData(p)&&HasNativeTravelUltimate369(p->mBoard,bit);}
bool Chomper(const Plant* p){const auto*d=GetNativePlantData(p);return d&&(d->kind==903||d->kind==921);}
int Scale(int n,int multiplier){return int((std::min)(int64_t((std::numeric_limits<int>::max)()),int64_t((std::max)(0,n))*multiplier));}
}
bool PrepareNativeTravelUltimate370(Board* b,uint8_t id,std::string* error)
{
 if(!b||!b->mApp||b->mApp->mBoard!=b||!IsNativeFusionBoard(b))
 {if(error)*error="Ultimate370 requires the current native Fusion Board";return false;}
 switch(id){
 case 0:case 1:case 2:case 4:case 5:case 12:case 13:case 14:case 15:case 16:case 17:case 20:case 21:return true;
 case 6:return EnsureNativePlantVisuals(b,1054,error);
 case 7:
  if(!EnsureNativePlantVisuals(b,1054,error))return false;
  // Resource/callback readiness only: never perform an attack or draw RNG here.
  for(int row=0;row<GetNativeFusionRowCount(b);++row)
   if(!CanApplyNativeSourceFireLine363(b,row,false,error))return false;
  return true;
 // Passive source awards have no synchronous effect at UpgradeUltimatePlantData.
 // Keep both genuine bits in the app ledger; consumer gaps are reported separately.
 case 3:case 23:return true;
 default:if(error)*error="Ultimate370 does not own this source ID";return false;
 }
}
void PlanNativeTravelUltimate370Bite(Plant* p,Zombie* z,FusionStrongChomperBite& result)
{
 if(!Chomper(p)||!z)return;
 const auto* profile=FindFusionStrongChomperProfile(GetNativePlantData(p)->kind);if(!profile)return;
 const int attack=GetNativeTravelPlantAttack369(p,profile->attackDamage);
 const float threshold=Has(p,0)?float(p->mPlantHealth):float(attack)*2;
 const float health=GetNativeFusionZombieHealth(z)+float((std::max)(0,z->mHelmHealth));
 // 4BDAD0: the source sums body and first armor only; shield is handled by NormalAll1.
 const bool swallow=health<=threshold;
 result.sourceDamage=swallow?1000000:attack;
 result.recovery=swallow?p->mPlantMaxHealth:attack;
}
int NativeTravelUltimate370Recovery(const Plant* p,int amount)
{return Chomper(p)&&Has(p,1)?Scale(amount,3):amount;}
int NativeTravelUltimate370ChomperCap(const Plant* p,int damage)
{return Chomper(p)&&Has(p,1)?(std::min)(damage,500):damage;}
int NativeTravelUltimate370CherryDamage(const Board* b,int ordinary)
{return HasNativeTravelUltimate369(b,2)?Scale(ordinary,3):ordinary;}
int NativeTravelUltimate370FumeDamage(const Plant* p,int ordinary)
{ordinary=GetNativeTravelPlantAttack369(p,ordinary);return Has(p,4)?Scale(ordinary,3):ordinary;}
int NativeTravelUltimate370FumeCold(const Plant* p,int ordinary)
{return Has(p,5)?30:ordinary;}
int NativeTravelUltimate370TorchLimit(const Plant* p){return Has(p,6)?10:20;}
bool NativeTravelUltimate370TorchFailure(Plant* p)
{
 if(!p||!Has(p,7))return false;
 // 4C1EC0 only starts the real friendly fire line after its failed right-cell search.
 if(!ApplyNativeSourceFireLine363(p->mBoard,p->mRow,false))return false;
 return true;
}
bool NativeTravelUltimate370ExtraMelon(const Plant* p,int sourceDraw)
{return sourceDraw<42||Has(p,12);}
void NativeTravelUltimate370MelonAfterShot(Plant* p)
{auto*d=GetNativePlantData(p);if(d&&d->kind==914&&Has(p,13))d->ultimateMelonPot370=true;}
bool NativeTravelUltimate370MelonEnhanced(const Plant* p)
{const auto*d=GetNativePlantData(p);return d&&d->kind==914&&d->ultimateMelonPot370;}
int NativeTravelUltimate370CannonDamage(const Board* b,uint16_t kind,int damage)
{return kind==84&&HasNativeTravelUltimate369(b,14)?Scale(damage,4):damage;}
int NativeTravelUltimate370CannonReload(const Plant* p,int ordinary)
{const auto*d=GetNativePlantData(p);return d&&d->kind==915&&Has(p,15)?1201:ordinary;}
int NativeTravelUltimate370ElementRecovery(const Plant* p,bool fire,int ordinary)
{
 const auto*d=GetNativePlantData(p);
 return d&&(d->kind==913||d->kind==925)&&Has(p,16)?(fire?p->mPlantMaxHealth:10000):ordinary;
}
bool RedirectNativeTravelUltimate370Damage(Plant* p,int damage,int sourceType)
{
 const auto*d=GetNativePlantData(p);
 if(!d||damage<=0||sourceType==3||d->kind==913||d->kind==925||!Has(p,17))return false;
 auto*b=p->mBoard;
 // 4655D0's source cell traversal includes the recipient's own column.
 for(int col=p->mPlantCol-1;col<=p->mPlantCol+1;++col)
 {
  Plant* target=nullptr;
  while(b->IteratePlants(target))
  {
   const auto*t=GetNativePlantData(target);
   if(target==p||!t||target->mDead||!target->mIsOnBoard||target->mPlantHealth<=0||target->mRow!=p->mRow||
      target->mPlantCol!=col||(t->kind!=913&&t->kind!=925))continue;
   const auto uid=b->mPlants.DataArrayGetID(target);
   ApplyNativeSourcePlantDamage(target,damage,0);
   if(auto*current=b->mPlants.DataArrayTryToGet(uid))current->mEatenFlashCountdown=20;
   return true;
  }
 }
 return false;
}
bool CaptureNativeTravelSteel370(Plant* p,NativeProjectileData& data)
{
 const auto*d=GetNativePlantData(p);
 if(!d||d->kind!=927||data.kind!=93||!Has(p,20)||p->mPlantMaxHealth<=0)return false;
 const int third=p->mPlantMaxHealth/3;
 float factor=1.f;
 if(p->mPlantHealth<third&&third>0)factor+=3.f*(1.f-float(p->mPlantHealth)/float(third));
 data.sourceUnmodifiedDamage369=int(120.f*factor);
 data.sourceSteelAttack370=GetNativeTravelPlantAttack369(p,120);
 data.sourceSteelFactor370=factor;data.sourceSteelHits370=0;
 data.damage=int((float(data.sourceSteelAttack370)*3.f)*factor);return true;
}
void NativeTravelSteel370AfterHit(NativeProjectileData& data)
{
 if(!data.sourceSteelAttack370)return;
 data.sourceUnmodifiedDamage369=int(float(data.sourceUnmodifiedDamage369)*.5f);
 if(data.sourceSteelHits370<12)++data.sourceSteelHits370;
 // At12 damage and baseline are already0; later actual hits still execute all
 // gameplay callbacks. Only this inert proof counter saturates.
}
float NativeTravelUltimate370SteelKnockback(const Board* b)
{return HasNativeTravelUltimate369(b,20)?.15f:.05f;}
int NativeTravelUltimate370TurretIncoming(const Plant* p,int damage)
{
 const auto*d=GetNativePlantData(p);
 if(!d||d->kind!=927||!Has(p,21)||p->mPlantMaxHealth<=0)return damage;
 if(p->mPlantHealth<100&&damage<100)return 0;
 return int(float(p->mPlantHealth)/float(p->mPlantMaxHealth)*float(damage));
}
}
