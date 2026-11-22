#include "FusionNativeLateZombie363.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeNextZombie343State.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <span>
namespace LawnFusion {namespace {
bool Plan(Zombie* z,int reason,FusionLate363Death& p,NativeJackbox300Explosion& e)
{
 const auto* d=GetNativeSourceZombieData(z);auto* c=GetNativeContext(z?z->mBoard:nullptr);
 if(!z||!d||!c||z->mDead||z->mNetGhost||z->mApp->NetIsClient()||!IsNativeSourceWorld313(z->mBoard))return false;
 const auto* h=GetNativeHeavyZombie313B(z);if(d->kind==209&&!h)return false;
 const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);const auto i=c->nextZombies343.find(uid);
 if(i!=c->nextZombies343.end()&&i->second.failed)return false;
 if(!TryPlanFusionLate363Death(d->kind,reason,z->mMindControlled,h&&h->lostJumper,h?h->status:0,i!=c->nextZombies343.end()&&i->second.deathConsumed,p))return false;
 float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
 e.ownerId=z->mNetId;e.ownerToken=d->instanceToken;e.sourceKind=d->kind;e.row=z->mRow;e.fromWave=z->mFromWave;e.renderOrder=z->mRenderOrder;e.controlled=z->mMindControlled;
 e.shadowX=e.centerX=x;e.shadowY=y;e.centerY=y-float((p.effect36?.6:.7)*GetNativeFusionPixelsY(z->mBoard));
 const auto* row=GetNativeZombieGarlicData(z);e.targetRow=row&&row->phase==GarlicActionPhase::Moving?row->targetRow:-1;
 return true;
}
}
bool PlanNativeLate363Death(Zombie* z,int reason,FusionSourceDeathBudget319& out,std::string* error)
{
 FusionLate363Death p;NativeJackbox300Explosion e;if(!Plan(z,reason,p,e))return false;
 if(p.explosion&&!CanEmitNativeSourceZombieExplosion343(z->mBoard,e,error,2))return false;
 if(p.effect36&&(!EnsureNativeSourceEffect269(z->mBoard,36,error)||!LawnNet::NetSourceSound284Ready(z->mApp,43)))return false;
 FusionSourceDeathBudget319 peak;
 if(p.count&&!GetNativeSourceZombieBirthBudget319(z->mBoard,std::span(p.children.data(),p.count),peak,error))return false;
 out=peak;return true;
}
bool CanRetireNativeLate363(Zombie* z,int reason,std::string* error)
{FusionSourceDeathBudget319 peak;return PlanNativeLate363Death(z,reason,peak,error)&&CanAllocateNativeSourceBudget319(z->mBoard,peak,error);}
bool BeforeRetireNativeLate363(Zombie* z,int reason)
{
 if(!CanRetireNativeLate363(z,reason))return false;FusionLate363Death p;NativeJackbox300Explosion e;if(!Plan(z,reason,p,e))return false;
 auto* b=z->mBoard;auto* app=z->mApp;auto* c=GetNativeContext(b);const uint32_t epoch=c->epoch,uid=b->mZombies.DataArrayGetID(z);
 const auto failure=[&](const char* why){if(app->mBoard==b&&GetNativeContext(b)->epoch==epoch){GetNativeContext(b)->nextZombies343[uid].failed=true;if(auto* current=b->mZombies.DataArrayTryToGet(uid))if(auto* h=EditNativeHeavyZombie313B(current))h->failed=true;LawnNet::NetTrace("SourceLate363 callback stopped uid=%u kind=%u reason=%s",uid,e.sourceKind,why);}return false;};
 c->nextZombies343[uid].deathConsumed=true; // BEFORE synchronous AoE or any child entry callback.
 if(p.explosion&&!EmitNativeSourceZombieExplosion343(b,e,2))return failure("prepared explosion failed after latch");
 if(p.effect36){LawnNet::NetPlaySourceSound284(app,43,.5f);SpawnNativeSourceEffect269(b,36,e.centerX,e.centerY,e.renderOrder,true);}
 for(uint8_t n=0;n<p.count;++n)
 {
  if(app->mBoard!=b||GetNativeContext(b)->epoch!=epoch)return true;
  // Original source re-reads Shadow after Explode, while its Unity component
  // may be pending destruction. Preserve its captured pose if native retired.
  z=b->mZombies.DataArrayTryToGet(uid);float x=e.shadowX,y=e.shadowY;if(z&&!z->mDead)GetNativeSourceZombie269NodePoint(z,"Shadow",x,y);
  NativeSourceZombieSpawnRequest request;request.kind=p.children[n];request.row=e.row;request.fromWave=e.fromWave;request.positioned=true;
  request.shadowX=x+p.offsets[n]*float(kFusionBasePixelsX);request.friendly=e.sourceKind==209&&e.controlled;
  auto* child=SpawnNativeSourceZombie(b,request);if(!child)return failure("prepared child batch partially emitted");
  if(e.sourceKind==209&&e.targetRow!=-1&&!GetNativeJackbox309Hooks().moveChildToTargetRow(child,e.targetRow))return failure("prepared targetRow continuation failed");
 }
 return true;
}
}
