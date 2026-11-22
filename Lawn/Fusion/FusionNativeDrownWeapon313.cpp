#include "FusionNativeDrownWeapon313.h"
#include "FusionWaterZombie313Rules.h"
#include "FusionDrown343Rules.h"
#include "FusionNativeObstacle313.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeProjection313.h"
#include "FusionBasePlantRules.h"
#include "../Net/NetCommon.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <array>

namespace LawnFusion
{
namespace
{
bool Ready(const NativeDrownWeapon313Hooks& h)
{return h.plantTraits&&h.plantShadow&&h.prepare&&h.spawn&&h.touches&&h.groundY&&h.sound&&h.randomInt&&h.randomFloat&&h.strongerBoard&&h.touchesBullet&&h.bigExplosionReady&&h.bigExplosion;}
bool Host(const Projectile* p)
{return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&!p->mDead&&!p->mNetGhost&&
 !p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard)&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
Plant* Target(Board* b,const FusionSourceObstacle313View& d)
{
 if(!d.targetPlantId)return nullptr;
 auto* p=b->mPlants.DataArrayTryToGet(d.targetPlantToken);const auto* data=GetNativePlantData(p);
 return p&&!p->mDead&&p->mIsOnBoard&&p->mNetId==d.targetPlantId&&data&&data->instanceToken==d.targetPlantToken?p:nullptr;
}
struct PlantIdentity{uint32_t uid=0;uint16_t id=0;int column=0;};
bool Select(Zombie* z,const NativeDrownWeapon313Hooks& h,PlantIdentity& out)
{
 float ownerX=0,ownerY=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",ownerX,ownerY))return false;
 std::vector<PlantIdentity> candidates;Plant* p=nullptr;
 while(z->mBoard->IteratePlants(p))
 {
  if(p->mDead||!p->mIsOnBoard||p->mRow!=z->mRow)continue;
  const auto* data=GetNativePlantData(p);bool invincible=false,pumpkin=false,flying=false,pot=false;
  if(!data||!h.plantTraits(p,invincible,pumpkin,flying,pot))return false;
  float x=0,y=0;if(!h.plantShadow(p,x,y))return false;
  if(!FusionDrown313PlantEligible(invincible,pumpkin,flying,pot)||!(x+6*kFusionBasePixelsX<ownerX))continue;
  PlantIdentity item{z->mBoard->mPlants.DataArrayGetID(p),p->mNetId,p->mPlantCol};if(!item.uid||!item.id)return false;
  // Actual source priority-first foreach, then descending column comparator.
  // Ordinary capability has TravelDebuff11=false; no random target substitution.
  if(data->kind==1148||data->kind==916){out=item;return true;}
  candidates.push_back(item);
 }
 if(candidates.empty()){out={};return true;}
 std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.column>b.column;});
 out=candidates.front();return true;
}
}
bool ThrowNativeDrownWeapon313(Zombie* z,const NativeDrownWeapon313Hooks& h)
{
 const auto* d=GetNativeSourceZombieData(z);
 if(!z||!d||(d->kind!=20&&d->kind!=202&&d->kind!=218)||!Ready(h)||z->mNetGhost||z->mApp->NetIsClient()||
    !CanApplyNativeFusionGameplay(z->mBoard))return false;
 if(z->mMindControlled||d->beforeDying||z->IsDeadOrDying())return true;
 float x=0,y=0;PlantIdentity target;
 if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)||!Select(z,h,target))return false;
 FusionSourceObstacle313Spawn request;request.ownerId=z->mNetId;request.ownerToken=d->instanceToken;request.row=z->mRow;
 request.centerX=x-float(2*kFusionBasePixelsX);request.centerY=y-float(3.1*GetNativeFusionPixelsY(z->mBoard));
 if(d->kind==218){request.variant=3;if(!GetNativeSourceZombie269NodePoint(z,"Shoot",request.centerX,request.centerY))return false;}
 request.targetPlantId=request.variant?0:target.id;request.targetPlantToken=request.variant?0:target.uid;
 float groundX=0;if(!NativeSourceGroundAnchor313(z,groundX,request.groundY))return false;
 // DrownWeapon.Start3764E0: bullet{row}, child0 sortingOrder=-10.
 request.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,z->mRow,request.variant?0:-10);
 double dx=0,dy=0,vx=0,vy=0;
 if(target.uid&&!request.variant)
 {
  auto* p=z->mBoard->mPlants.DataArrayTryToGet(target.uid);float tx=0,ty=0;
  if(!p||p->mNetId!=target.id||!h.plantShadow(p,tx,ty))return false;
  dx=std::abs(request.centerX-tx)/kFusionBasePixelsX;dy=std::abs(request.centerY-ty)/GetNativeFusionPixelsY(z->mBoard);
 }
 if(!request.variant&&!TryMakeFusionDrown313Velocity(dx,dy,target.uid!=0,vx,vy))return false;
 std::string error;if(!h.prepare(z->mBoard,request,&error))return false;
 const int sound=h.randomInt(z->mBoard,3,5);if(sound<3||sound>=5)return false;h.sound(z,uint16_t(sound),.5f);
 auto* p=h.spawn(z->mBoard,request);if(!p)return false;
 const double py=GetNativeFusionPixelsY(p->mBoard);
 p->mPosX=request.centerX-20;p->mPosY=request.centerY-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 p->mVelX=float(vx*kFusionBasePixelsX*.01);p->mVelY=float(-vy*py*.01);p->mVelZ=0;p->mAccZ=float(10*py*.0001);
 p->mRotation=-17.f*3.14159265358979323846f/180.f;p->mRotationSpeed=0;
 return true;
}
bool CanSpawnNativeDrownVolley343(Zombie* z,float x,float y,const NativeDrownWeapon313Hooks& h,std::string* error)
{
 const auto* d=GetNativeSourceZombieData(z);if(!z||!d||d->kind!=216||!Ready(h)||z->mNetGhost||z->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(z->mBoard))return false;
 if(z->mMindControlled)return true; // Source still consumes ammo/sound, no births.
 auto* b=z->mBoard;if(!b->mProjectiles.mBlock||uint64_t(b->mProjectiles.mSize)+14>b->mProjectiles.mMaxSize||!std::isfinite(x)||!std::isfinite(y))return false;
 if(std::abs(x)+.5f*kFusionBasePixelsX>4096||std::abs(y)+.3f*GetNativeFusionPixelsY(b)>4096)return false;
 FusionSourceObstacle313Spawn q;q.ownerId=z->mNetId;q.ownerToken=d->instanceToken;q.row=z->mRow;q.variant=1;q.centerX=x;q.centerY=y;
 float gx=0;if(!NativeSourceGroundAnchor313(z,gx,q.groundY))return false;
 q.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,z->mRow,0);return h.prepare(b,q,error);
}
bool SpawnNativeDrownVolley343(Zombie* z,float x,float y,const NativeDrownWeapon313Hooks& h)
{
 if(!CanSpawnNativeDrownVolley343(z,x,y,h))return false;if(z->mMindControlled)return true;
 const auto* d=GetNativeSourceZombieData(z);FusionSourceObstacle313Spawn q;q.ownerId=z->mNetId;q.ownerToken=d->instanceToken;q.row=z->mRow;q.variant=1;
 float gx=0;if(!NativeSourceGroundAnchor313(z,gx,q.groundY))return false;q.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,z->mRow,0);
 std::array<std::array<float,2>,14> offsets{};
 for(auto& offset:offsets){offset[0]=h.randomFloat(z->mBoard,-.5f,.5f);offset[1]=h.randomFloat(z->mBoard,-.3f,.3f);if(!std::isfinite(offset[0]+offset[1])||std::abs(offset[0])>.5f||std::abs(offset[1])>.3f)return false;}
 for(const auto& offset:offsets)
 {q.centerX=x+float(offset[0]*kFusionBasePixelsX);q.centerY=y-float(offset[1]*GetNativeFusionPixelsY(z->mBoard));auto* p=h.spawn(z->mBoard,q);if(!p)return false;
  p->mPosX=q.centerX-20;p->mPosY=q.centerY-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);p->mVelX=0;p->mVelY=float(-4*GetNativeFusionPixelsY(z->mBoard)*.01);p->mVelZ=0;p->mAccZ=float(10*GetNativeFusionPixelsY(z->mBoard)*.0001);p->mRotation=p->mRotationSpeed=0;}
 return true; // Partial failure is latched by the real graph event owner.
}

namespace
{
NativeJackbox300Explosion BigDrownExplosion(Projectile* p,const FusionSourceObstacle313View& d)
{
 NativeJackbox300Explosion e;e.ownerId=d.ownerId;e.ownerToken=d.ownerToken;
 e.sourceKind=218; // ONLY218 may birth variant3; verified by the real factory.
 e.row=p->mRow;e.renderOrder=p->mRenderOrder;e.centerX=e.shadowX=p->mPosX+20;e.centerY=e.shadowY=p->mPosY+20;return e;
}
bool WasEntered(const NativeSourceObstacle313Data& s,uint8_t type,uint32_t id)
{return std::any_of(s.entered.begin(),s.entered.end(),[&](const auto& c){return c.entity==type&&c.generation==id;});}
bool Refuse(Projectile* p,const char* reason)
{
 if(auto* state=EditNativeSourceObstacle313(p);state&&!state->failed)
 {state->failed=true;LawnNet::NetTrace("Fusion Drown343 stopped id=%u token=%u age=%d reason=%s",p->mNetId,state->instanceToken,p->mProjectileAge,reason);}
 return true;
}
}
bool UpdateNativeDrownWeapon313(Projectile* p,FusionSourceObstacle313View& d,const NativeDrownWeapon313Hooks& h)
{
 if(d.kind!=FusionSourceObstacle313Kind::DrownWeapon)return false;
 if(!Host(p))return true;
 // raw age/motion mirrors are captured later; strict wire checks belong there.
 if(!Ready(h)||!FusionSourceObstacle313ControllerValid(d)){p->Die();return true;}
 const bool horizontal=(d.variant&Drown343Horizontal)!=0,big=(d.variant&Drown343Big)!=0;
 auto* b=p->mBoard;auto* app=p->mApp;const uint32_t uid=b->mProjectiles.DataArrayGetID(p);
 const double py=GetNativeFusionPixelsY(b);
 if(horizontal&&p->mProjectileAge==1)
 {
  // Unity Start occurs AFTER the entire14-body AnimShoot jitter loop. Native
  // age1 owns this one deferred draw; ghosts/hydration never replay Start.
  float vx=0,vy=0;
  if(!h.randomFloat||!TryMakeFusionDrown343HorizontalVelocity(h.randomFloat(b,12,14),big,vx,vy))return Refuse(p,"horizontal Start RNG outside source range");
  p->mVelX=float(vx*kFusionBasePixelsX*.01);p->mVelY=float(-vy*py*.01);p->mAccZ=float(10*py*.0001);
 }
 if(p->mProjectileAge>(big?3000:300))
 {
  if(!big){p->Die();return true;}
  const auto e=BigDrownExplosion(p,d);std::string error;
  if(!h.bigExplosionReady||!h.bigExplosion||!h.bigExplosionReady(b,e,&error))return Refuse(p,error.empty()?"big explosion preflight refused":error.c_str());
  d.exploded=true;p->Die(); // Source virtual Die, then standalone ZombieExplode.
  if(!h.bigExplosion(b,e))LawnNet::NetTrace("Fusion Drown343 consumed explosion refused owner=%u token=%u",e.ownerId,e.ownerToken);return true;
 }
 if(big&&p->mProjectileAge>2500&&FusionDrown343FlashRemaining(p->mProjectileAge,d.lastHitAge,d.drownLongFlash)==0)
 {d.drownLongFlash=true;d.lastHitAge=uint32_t(p->mProjectileAge);}
 auto* target=Target(b,d);
 if(!d.landed)
 {
  bool hold=false;float tx=0,ty=0;
  if(target&&target->mRow==p->mRow){if(!h.plantShadow(target,tx,ty)){p->Die();return true;}hold=p->mPosY+20>=ty;}
  if(!hold)
  {
   if(!std::isfinite(p->mPosX+p->mVelX)||!std::isfinite(p->mPosY+p->mVelY)){p->Die();return true;}
   p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mVelY+=p->mAccZ;
   const double vx=p->mVelX/(kFusionBasePixelsX*.01),vy=-p->mVelY/(py*.01);
   p->mRotation=float(-std::atan2(vy,-vx));p->mX=int(p->mPosX);p->mY=int(p->mPosY);
  }
  float ground=0;if(!h.groundY(p,ground)||!std::isfinite(ground)){p->Die();return true;}
  if(p->mPosY+20>ground)d.landed=true;
 }
 if(!horizontal)
 {
  if(d.landed||d.hit||!target||target->mRow!=p->mRow)return true;
  bool invincible=false,pumpkin=false,flying=false,pot=false;
  if(!h.plantTraits(target,invincible,pumpkin,flying,pot)){p->Die();return true;}
  if(flying||!h.touches(p,target))return true;
  const auto* data=GetNativePlantData(target);int damage=0;
  const bool stronger=h.strongerBoard&&h.strongerBoard(b);
  if(!data||!TryGetFusionDrown313Damage(target->mPlantHealth,data->kind,false,false,stronger,damage)){p->Die();return true;}
  d.hit=true;ApplyNativeSourcePlantDamage(target,damage,0);
  if(app->mBoard==b)if(auto* actual=b->mProjectiles.DataArrayTryToGet(uid);actual&&!actual->mDead)actual->Die();return true;
 }
 auto* state=EditNativeSourceObstacle313(p);if(!state)return Refuse(p,"horizontal own state missing");
 std::vector<NativeSourceObstacle313Contact> observed;
 std::vector<uint32_t> plants,bullets;Plant* plant=nullptr;
 if(!d.landed&&!d.hit)while(b->mPlants.IterateNext(plant))
 {
  if(plant->mDead||!plant->mIsOnBoard||plant->mRow!=p->mRow||!NativeSourcePlantColliderEnabled(plant)||!h.touches(p,plant))continue;
  bool invincible=false,pumpkin=false,flying=false,pot=false;if(!h.plantTraits(plant,invincible,pumpkin,flying,pot))return Refuse(p,"plant traits unavailable before contact commit");if(flying)continue;
  const uint32_t id=b->mPlants.DataArrayGetID(plant);observed.push_back({id,1});if(!WasEntered(*state,1,id))plants.push_back(id);
 }
 Projectile* shot=nullptr;
 if(big&&d.landed&&h.touchesBullet)while(b->mProjectiles.IterateNext(shot))
 {
  const auto* q=GetNativeProjectileData(shot);
  if(shot==p||shot->mDead||!q||!FusionDrown343BigBulletContact(q->specialType,q->isZombieBullet,shot->mRow==p->mRow)||!h.touchesBullet(p,shot))continue;
  const uint32_t id=b->mProjectiles.DataArrayGetID(shot);observed.push_back({id,3});if(!WasEntered(*state,3,id))bullets.push_back(id);
 }
 state->entered=std::move(observed); // Real Unity enter set, full generations.
 for(auto id:plants)
 {
  auto* victim=b->mPlants.DataArrayTryToGet(id);const auto* data=GetNativePlantData(victim);int damage=1000000;
  if(!victim||victim->mDead||!data)continue;
  if(!big&&!TryGetFusionDrown313Damage(victim->mPlantHealth,data->kind,false,false,h.strongerBoard&&h.strongerBoard(b),damage))return Refuse(p,"horizontal damage preflight refused");
  if(!big)d.hit=true;ApplyNativeSourcePlantDamage(victim,damage,0);
  if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
  if(!big){p->Die();return true;}
 }
 for(auto id:bullets)
 {
  auto* victim=b->mProjectiles.DataArrayTryToGet(id);if(!victim||victim->mDead)continue;victim->Die();
  if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
  auto* live=EditNativeSourceObstacle313(p);if(!live)return true;
  ++live->hitCount;live->drownLongFlash=false;live->lastHitAge=uint32_t(p->mProjectileAge);
  if(live->hitCount>300){p->Die();return true;}
 }
 return true;
}
}
