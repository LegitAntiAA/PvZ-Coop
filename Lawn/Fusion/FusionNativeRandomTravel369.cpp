#include "FusionNativeRandomTravel369.h"
#include "FusionNativeTravelBuff369.h"
#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeTravelUltimate370.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeLastPlants269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../MessageWidget.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion { namespace {
bool Fail(std::string* e,const char* why){if(e)*e=why;return false;}
bool Host(Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&!b->mApp->NetIsClient()&&IsNativeSourceWorld313(b)&&GetNativeContext(b);}
bool Live(Plant* p)
{return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Child(Board* b,const NativeCompanion269Reference& ref)
{
 if(!ref.id||!ref.token)return false;Projectile* p=nullptr;
 while(b->IterateProjectiles(p))if(!p->mDead&&p->mNetId==ref.id)
 {const auto* d=GetNativeCompanion269State(p);if(d&&d->instanceToken==ref.token)return true;}
 return false;
}
bool PreparePumpkins(Board* b,std::string* e)
{
 FusionSourceDeathBudget319 peak;Plant* p=nullptr;
 while(b->IteratePlants(p))
 {
  const auto* d=GetNativePlantData(p);if(!Live(p)||!d||(d->kind!=911&&d->kind!=922))continue;
  const auto* s=GetNativeCompanions269PlantState(p);const auto* profile=FindFusionCompanions269Profile(d->kind);
  if(!s||!profile||profile->childSlots!=3||s->children.size()!=3||
   !EnsureNativeCompanion269Visual(b,profile->childFamily,false,e))
   return Fail(e,"Travel Ultimate24 actual pumpkin companion resources/state unavailable");
  // Source Supplement creates its extra slots1/2; slot0 retains its ordinary
  // source birth/recreate schedule. Native carrier is one PEA Projectile only.
  for(size_t i=1;i<3;++i)if(!Child(b,s->children[i]))++peak.projectiles;
 }
 return CanAllocateNativeSourceBudget319(b,peak,e);
}
bool PrepareOutcome(Board* b,uint16_t kind,int row,bool,std::string* e)
{
 if(!Host(b)||row<0||row>=GetNativeFusionRowCount(b))return Fail(e,"Random369 direct source outcome world/row unavailable");
 const uint16_t one[]={kind};FusionSourceDeathBudget319 peak;
 return EnsureNativeSourceZombieClosure280(b,one,e)&&GetNativeSourceZombieBirthBudget319(b,one,peak,e)&&CanAllocateNativeSourceBudget319(b,peak,e);
}
bool PrepareAward(Board* b,FusionTravelBuff360Award a,std::string* e)
{
 if(!Host(b)||!b->mAdvice)return Fail(e,"Travel369 current host/native Advice unavailable");
 if(a.type==FusionTravelBuff360Type::Ultimate)
 {
  switch(a.index)
  {
   case 8:case 9:
    for(uint8_t type:{uint8_t(0),uint8_t(1),uint8_t(2),uint8_t(5)})
     if(!EnsureNativeWorldRemainder269Assets(b,NativeWorldRemainder269Family::Meteorite,type))return Fail(e,"Travel meteorite resource closure unavailable");
    return true;
   case 10:case 11:case 18:case 19:return true;
   case 22:return EnsureNativeProjectileVisuals(b,75,e);
   case 24:return PreparePumpkins(b,e);
   case 25:return EnsureNativeCompanion269Visual(b,FusionCompanion269Family::UltimateLittleFly,false,e);
   default:return PrepareNativeTravelUltimate370(b,a.index,e);
  }
 }
 if(a.type!=FusionTravelBuff360Type::Advanced||a.index>=48)return Fail(e,"Travel369 invalid award identity");
 FusionTravelBuff360Pool sourcePool;FusionTravelBuff360Ledger empty;
 if(!BuildFusionTravelBuff360Pool(empty,false,sourcePool)||
   std::find(sourcePool.advanced.begin(),sourcePool.advanced.end(),a.index)==sourcePool.advanced.end())
  return Fail(e,"Travel369 award not in the genuine ordinary source pool");
 if(a.index==25)
 {int next=0;return TryFusionTravelSun369(b->mSunMoney,3000,2,HasNativeTravelAdvanced369(b,25),next)||Fail(e,"Travel369 immediate source sun value invalid");}
 if(a.index==39)
 {
  // Future HypnoDoom source branch may create either real ground43 or water205.
  // Do not replace its pool with whichever outcomes happen to be implemented.
  const uint16_t roots[]={43,205};if(!EnsureNativeSourceZombieClosure280(b,roots,e))return false;
 }
 // Every affected catalog plant keeps its real native implementation. Queries
 // are read from the app ledger at their source use sites, not cached fake
 // per-plant copies. Load current real plant resources before publishing a bit.
 Plant* p=nullptr;while(b->IteratePlants(p))if(Live(p)&&!EnsureNativePlantVisuals(b,GetNativePlantData(p)->kind,e))return false;
 if(e)e->clear();return true;
}
void ApplyImmediate(Board* b,FusionTravelBuff360Award a,const FusionTravelBuff360Ledger&)
{
 if(a.type==FusionTravelBuff360Type::Advanced)
 {
  if(a.index==25)CreditNativeTravelSun369(b,3000,2);
  // Source10 AdjustDamage and20 BoxInfo light are represented by the shared
  // derived catalog attack / native field-light queries. The transaction
  // publishes their bit immediately after this callback. Already-born shots
  // retain their captured damage; no retrospective projectile mutation.
 }
 else if(a.index==24)
 {
  // Source publishes Ultimate24 before Supplement. Whole P capacity was
  // checked before the award RNG, so callbacks use the actual native factory.
  Plant* p=nullptr;while(b->IteratePlants(p))if(Live(p))
  {const auto* d=GetNativePlantData(p);if(d&&(d->kind==911||d->kind==922))TrySupplementNativeCompanions269(p);}
 }
}
bool Cooldown(const Board* b,bool& active)
{
 const auto* c=GetNativeContext(b);if(!c||!std::isfinite(c->travelBuffCooldown369)||c->travelBuffCooldown369<0||c->travelBuffCooldown369>60)return false;
 active=c->travelBuffCooldown369>0;return true;
}
}
const NativeTravelBuff360Hooks& GetNativeTravel369Hooks()
{static const NativeTravelBuff360Hooks hooks{PrepareAward,ApplyImmediate,ShowNativeTravelAward369};return hooks;}
bool PreflightNativeRandomTravel369(Board* b,std::string* e)
{const auto* ledger=GetNativeTravelBuff369(b);return ledger&&PreflightNativeTravelBuff360(b,*ledger,GetNativeTravel369Hooks(),e);}
bool UnlockNativeRandomTravel369(Zombie* z,bool& awarded,std::string* e)
{
 if(!z||!IsNativeRandom360(z)||!Host(z->mBoard))return Fail(e,"Random369 actual typed source owner unavailable");
 auto* ledger=EditNativeTravelBuff369(z->mBoard);auto* c=GetNativeContext(z->mBoard);if(!ledger||!c)return false;
 // Ordinary Adventure Board.currentSurvivalRound starts0. Travel continuation
 // is a separate future entry contract, never inferred from level/cycle.
 return UnlockNativeTravelBuff360(z->mBoard,*ledger,c->travelBuffCooldown369,0,GetNativeTravel369Hooks(),awarded,e);
}
const NativeRandom360Hooks& GetNativeRandomTravel369Hooks()
{static const NativeRandom360Hooks hooks{PrepareOutcome,PreflightNativeRandomTravel369,Cooldown,UnlockNativeRandomTravel369,nullptr};return hooks;}
void ShowNativeTravelAward369(Board* b,FusionTravelBuff360Award a,float seconds)
{
 if(!b||!b->mApp||b->mApp->mBoard!=b||!b->mAdvice||!std::isfinite(seconds)||seconds<=0)return;
 const auto text=a.type==FusionTravelBuff360Type::Advanced?
  Sexy::StrFormat(_S("Fusion: улучшение %u"),unsigned(a.index+1)):
  Sexy::StrFormat(_S("Fusion: ультимативное улучшение %u"),unsigned(a.index+1));
 b->DisplayAdvice(text,MESSAGE_STYLE_HINT_TALL_LONG,ADVICE_NONE);
 b->mAdvice->mDuration=int(seconds*100);b->mAdvice->mDisplayTime=b->mAdvice->mDuration;
 LawnNet::NetTrace("Fusion Travel369 award type=%u index=%u duration=%d",unsigned(a.type),a.index,b->mAdvice->mDuration);
}
}
