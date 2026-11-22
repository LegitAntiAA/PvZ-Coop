#include "FusionNativeDrownWeapon313.h"
#include "FusionWaterZombie313Rules.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeProjection313.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Ready(const NativeDrownWeapon313Hooks& h)
{return h.plantTraits&&h.plantShadow&&h.prepare&&h.spawn&&h.touches&&h.groundY&&h.sound&&h.randomInt;}
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
 if(!z||!d||d->kind!=20||!Ready(h)||z->IsDeadOrDying()||z->mNetGhost||z->mApp->NetIsClient()||
    z->mMindControlled||!CanApplyNativeFusionGameplay(z->mBoard))return false;
 float x=0,y=0;PlantIdentity target;
 if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)||!Select(z,h,target))return false;
 FusionSourceObstacle313Spawn request;request.ownerId=z->mNetId;request.ownerToken=d->instanceToken;request.row=z->mRow;
 request.centerX=x-float(2*kFusionBasePixelsX);request.centerY=y-float(3.1*GetNativeFusionPixelsY(z->mBoard));
 request.targetPlantId=target.id;request.targetPlantToken=target.uid;
 float groundX=0;if(!NativeSourceGroundAnchor313(z,groundX,request.groundY))return false;
 // DrownWeapon.Start3764E0: bullet{row}, child0 sortingOrder=-10.
 request.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,z->mRow,-10);
 double dx=0,dy=0,vx=0,vy=0;
 if(target.uid)
 {
  auto* p=z->mBoard->mPlants.DataArrayTryToGet(target.uid);float tx=0,ty=0;
  if(!p||p->mNetId!=target.id||!h.plantShadow(p,tx,ty))return false;
  dx=std::abs(request.centerX-tx)/kFusionBasePixelsX;dy=std::abs(request.centerY-ty)/GetNativeFusionPixelsY(z->mBoard);
 }
 if(!TryMakeFusionDrown313Velocity(dx,dy,target.uid!=0,vx,vy))return false;
 std::string error;if(!h.prepare(z->mBoard,request,&error))return false;
 const int sound=h.randomInt(z->mBoard,3,5);if(sound<3||sound>=5)return false;h.sound(z,uint16_t(sound),.5f);
 auto* p=h.spawn(z->mBoard,request);if(!p)return false;
 const double py=GetNativeFusionPixelsY(p->mBoard);
 p->mPosX=request.centerX-20;p->mPosY=request.centerY-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 p->mVelX=float(vx*kFusionBasePixelsX*.01);p->mVelY=float(-vy*py*.01);p->mVelZ=0;p->mAccZ=float(10*py*.0001);
 p->mRotation=-17.f*3.14159265358979323846f/180.f;p->mRotationSpeed=0;
 return true;
}
bool UpdateNativeDrownWeapon313(Projectile* p,FusionSourceObstacle313View& d,const NativeDrownWeapon313Hooks& h)
{
 if(d.kind!=FusionSourceObstacle313Kind::DrownWeapon)return false;
 if(!Host(p))return true;if(!Ready(h)||!FusionSourceObstacle313ViewValid(d)){p->Die();return true;}
 if(p->mProjectileAge>300){p->Die();return true;} // Source normal existTime>3.
 const double py=GetNativeFusionPixelsY(p->mBoard);auto* target=Target(p->mBoard,d);
 if(!d.landed)
 {
  bool hold=false;float tx=0,ty=0;
  if(target&&target->mRow==p->mRow)
  {if(!h.plantShadow(target,tx,ty)){p->Die();return true;}hold=p->mPosY+20>=ty;}
  if(!hold)
  {
   if(!std::isfinite(p->mPosX+p->mVelX)||!std::isfinite(p->mPosY+p->mVelY)){p->Die();return true;}
   // Source order: p+=v*dt, then vy-=g*dt, then rotate from NEW vy.
   p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mVelY+=p->mAccZ;
   const double sourceVX=p->mVelX/(kFusionBasePixelsX*.01),sourceVY=-p->mVelY/(py*.01);
   p->mRotation=float(-std::atan2(sourceVY,-sourceVX));p->mX=int(p->mPosX);p->mY=int(p->mPosY);
  }
  float ground=0;if(!h.groundY(p,ground)||!std::isfinite(ground)){p->Die();return true;}
  if(p->mPosY+20>ground)d.landed=true;
 }
 if(d.landed||d.hit||!target||target->mRow!=p->mRow)return true;
 bool invincible=false,pumpkin=false,flying=false,pot=false;
 if(!h.plantTraits(target,invincible,pumpkin,flying,pot)){p->Die();return true;}
 // OnTrigger tests only flying; target eligibility was the source launch scan.
 if(flying||!h.touches(p,target))return true;
 const auto* data=GetNativePlantData(target);int damage=0;
 if(!data||!TryGetFusionDrown313Damage(target->mPlantHealth,data->kind,false,false,false,damage)){p->Die();return true;}
 d.hit=true; // Consume before real recursive protector/death callbacks.
 auto* b=p->mBoard;auto* app=p->mApp;const auto uid=b->mProjectiles.DataArrayGetID(p);
 ApplyNativeSourcePlantDamage(target,damage,0);
 if(app->mBoard==b)if(auto* actual=b->mProjectiles.DataArrayTryToGet(uid);actual&&!actual->mDead)actual->Die();
 return true;
}
}
