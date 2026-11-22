#include "FusionNativePlantDisable363.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeProducer.h"
#include "FusionNativeBasePlants.h"
#include "FusionShooterTiming.h"
#include "FusionCatalog.h"
#include "../Plant.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion {namespace {
uint32_t UID(const Plant* p){return p&&p->mBoard?p->mBoard->mPlants.DataArrayGetID(const_cast<Plant*>(p)):0;}
bool World(const Plant* p){return p&&p->mApp&&p->mApp->mBoard==p->mBoard&&!p->mDead&&p->mIsOnBoard&&GetNativePlantData(p)&&IsNativeSourceWorld313(p->mBoard);}
std::array<uint32_t,8> Clocks(const Plant* p)
{return {uint32_t(p->mBodyReanimID),uint32_t(p->mHeadReanimID),uint32_t(p->mHeadReanimID2),uint32_t(p->mHeadReanimID3),uint32_t(p->mBlinkReanimID),uint32_t(p->mLightReanimID),uint32_t(p->mSleepingReanimID),uint32_t(p->mSpecialReanimID)};}
void RestoreIntervals(Plant* p)
{
 auto* d=GetNativePlantData(p);const auto* def=d?FusionFindPlant(d->kind):nullptr;if(!d||!def)return;
 // Preserve active animation/digest/special state. Reset only the existing
 // standard launch counter bound by its native initializer (no RNG call).
 if(p->mLaunchRate>0&&!IsNativeChomper(p))if(const auto* t=FindFusionShooterTimingProfile(d->kind);t&&t->attackIntervalSeconds>0)
 {int ticks=0;if(TryGetFusionShooterCycleTicks(*t,1.f,ticks))p->mLaunchCounter=ticks;}
 if(const auto* producer=FindFusionProducerProfile(d->kind))
 {int ticks=0;if(TryGetFusionProducerInitialTicks(*producer,producer->intervalSeconds,ticks)){if(d->kind==1000)d->productionTicks=ticks;else p->mLaunchCounter=ticks;d->productionTicks=ticks;}}
}
}
bool NativePlantDisabled363(const Plant* p)
{const auto* d=World(p)?GetNativePlantData(p):nullptr;return d&&!d->disable363.empty();}
bool CanDisableNativePlant363(const Plant* p)
{
 if(!World(p)||p->mNetGhost||p->mApp->NetIsClient())return false;const auto* d=GetNativePlantData(p);
 if(FusionSourcePlantDisable363Immune(d->kind))return true;
 return FusionPlantDisable363ViewValid(d->disable363)&&d->disable363.size()<65535&&p->mApp->ReanimationTryToGet(p->mBodyReanimID)!=nullptr;
}
bool DisableNativePlant363(Plant* p)
{
 if(!CanDisableNativePlant363(p))return false;auto* d=GetNativePlantData(p);if(FusionSourcePlantDisable363Immune(d->kind))return true;
 std::vector<uint16_t> next;if(!TryAddFusionPlantDisable363(d->disable363,next))return false;
 auto* c=GetNativeContext(p->mBoard);if(!c)return false;auto& state=c->plantDisables363[UID(p)];state.epoch=c->epoch;state.lastTick=p->mBoard->mMainCounter;state.clocks=Clocks(p);d->disable363=std::move(next);ApplyNativePlantDisable363Color(p);return true;
}
void ReconcileNativePlantDisables363(Board* b)
{
 auto* c=GetNativeContext(b);if(!c||!IsNativeSourceWorld313(b))return;
 for(auto i=c->plantDisables363.begin();i!=c->plantDisables363.end();)
 {auto* p=b->mPlants.DataArrayTryToGet(i->first);if(!NativePlantDisabled363(p)||i->second.epoch!=c->epoch){if(i->second.epoch==c->epoch&&World(p))ApplyNativePlantDisable363Color(p);i=c->plantDisables363.erase(i);}else{ i->second.clocks=Clocks(p);++i;}}
 Plant* p=nullptr;while(b->IteratePlants(p))if(NativePlantDisabled363(p))
 {auto& state=c->plantDisables363[UID(p)];state.epoch=c->epoch;state.clocks=Clocks(p);ApplyNativePlantDisable363Color(p);}
}
void UpdateNativePlantDisables363(Board* b)
{
 if(!b||!b->mApp||b->mApp->NetIsClient()||b->mPaused||b->mApp->mGameScene!=SCENE_PLAYING||!IsNativeSourceWorld313(b))return;
 ReconcileNativePlantDisables363(b);auto* c=GetNativeContext(b);if(!c)return;
 for(auto& [uid,state]:c->plantDisables363)
 {if(state.lastTick==b->mMainCounter)continue;state.lastTick=b->mMainCounter;auto* p=b->mPlants.DataArrayTryToGet(uid);auto* d=GetNativePlantData(p);if(!d||!NativePlantDisabled363(p))continue;
  bool active=false;if(!TickFusionPlantDisable363(d->disable363,active))continue;
  if(active){RestoreIntervals(p);ApplyNativePlantDisable363Color(p);}}
 ReconcileNativePlantDisables363(b);
}
bool NativePlantDisable363HoldsClock(const Reanimation* r)
{
 auto* b=gLawnApp?gLawnApp->mBoard:nullptr;const auto* c=GetNativeContext(b);if(!r||r->mDead||!c||!IsNativeSourceWorld313(b))return false;
 for(const auto& [uid,state]:c->plantDisables363)
 {const auto* p=b->mPlants.DataArrayTryToGet(uid);if(state.epoch!=c->epoch||!NativePlantDisabled363(p))continue;
  // A legitimate status/level callback may replace a native role between
  // Board reconciliation and the holder's update. Resolve its CURRENT full
  //R IDs here, so a fresh blink/head never advances one unheld frame.
  for(auto id:Clocks(p))if(id&&b->mApp->ReanimationTryToGet(ReanimationID(id))==r)return true;}
 return false;
}
void ApplyNativePlantDisable363Color(Plant* p)
{if(!World(p))return;const int gray=NativePlantDisabled363(p)?128:255;for(auto id:Clocks(p))if(id)if(auto* r=p->mApp->ReanimationTryToGet(ReanimationID(id)))r->mColorOverride=Sexy::Color(gray,gray,gray,r->mColorOverride.mAlpha);}
bool HandleNativeDisabledPlantAbilities363(Plant* p)
{
 if(!NativePlantDisabled363(p))return false;
 // Existing Producer class explicitly distinguishes AnimatorSpeed from
 // GameRunning/OutsideIZ; its already-scheduled Invoke still runs first.
 if(IsNativeProducer(p))UpdateNativeProducer(p);return true;
}
}
