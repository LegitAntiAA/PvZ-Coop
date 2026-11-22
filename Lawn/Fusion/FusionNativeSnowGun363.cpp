#include "FusionNativeSnowGun363.h"
#include "FusionNativePlantDisable363.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeNextZombie343State.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeWater268.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeCold.h"
#include "FusionNativeEffects269.h"
#include "FusionStar269Rules.h"
#include "FusionWaterBridge321Rules.h"
#include "FusionNative.h"
#include "FusionAnimation.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace LawnFusion { namespace {
constexpr const char* muzzle="Zombie_polevaulter_pole/Shoot";
bool Host(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return z&&d&&d->kind==49&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard);}
bool Live(const Plant* p){return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Tags(const Plant* p,bool& flying,bool& pumpkin)
{bool pot=false,lily=false;return Live(p)&&GetNativeRemaining269PlantTags(p,flying,pumpkin,pot,lily);}
NativeNextZombie343State* State(const Zombie* z)
{auto* c=GetNativeContext(z?z->mBoard:nullptr);if(!c||!z)return nullptr;const auto i=c->nextZombies343.find(z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z)));return i==c->nextZombies343.end()?nullptr:&i->second;}
bool Refuse(const char* why,std::string* e){if(e)*e=why;return false;}
bool Shot(Zombie* z,FusionHostilePlanarShot& out)
{
 const auto* s=State(z);float x=0,y=0;if(!s||!s->initialized||s->failed||!GetNativeSourceZombie269NodePoint(z,muzzle,x,y))return false;
 return TryPlanFusionSnowGun363Shot(x,y,s->snowTargetX363,s->snowTargetY363,GetNativeFusionPixelsY(z->mBoard),out);
}
bool Covered(Plant* target)
{Plant* q=nullptr;while(target->mBoard->IteratePlants(q))if(Live(q)&&q->mRow==target->mRow&&q->mPlantCol==target->mPlantCol){bool fly=false,pump=false;if(Tags(q,fly,pump)&&pump)return true;}return false;}
bool Touches(Projectile* p,const Sexy::Rect& r)
{
 FusionAnimationBoxCollider303 box;if(r.mWidth<=0||r.mHeight<=0||!GetNativeSnowBall363Box(p,box)||!box.enabled||!box.nodeActive)return false;
 FusionContactQuad321 q;for(size_t i=0;i<4;++i)q[i]={p->mPosX+20+60*box.corners[i][0],p->mPosY+20-60*box.corners[i][1]};
 const float l=float(r.mX),t=float(r.mY),rr=float(r.mX+r.mWidth),b=float(r.mY+r.mHeight);
 return FusionContactQuads321(q,{{{l,t},{rr,t},{rr,b},{l,b}}});
}
void Effect(Board* b,uint16_t kind,float x,float y,int row)
{SpawnNativeSourceEffect269(b,kind,x,y,Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row,0),true);}
bool HitPlant(Projectile* p,Plant* target)
{
 auto* b=p->mBoard;auto* app=p->mApp;const auto* d=GetNativeProjectileData(p);if(!d||!Live(target)||!EnsureNativeSnowBall363Resources(b))return false;
 const uint16_t kind=GetNativePlantData(target)->kind;const float hitX=p->mPosX+20,hitY=p->mPosY+20;
 if(kind==251||kind==255){GetNativeProjectileData(p)->hitLatched=true;Effect(b,24,hitX,hitY,p->mRow);LawnNet::NetPlaySourceSound284(app,uint16_t(RandRangeInt(0,2)),.5f);p->Die();return true;}
 const int row=target->mRow,column=target->mPlantCol;float x=0,y=0;if(!target->GetShadowCenter(x,y))return false;y-=float(.5*GetNativeFusionPixelsY(b));
 std::vector<uint32_t> refs;Plant* q=nullptr;
 while(b->IteratePlants(q))if(Live(q)&&FusionSnowBall363AreaCell(q->mPlantCol-column,q->mRow-row))
 {bool flying=false,pumpkin=false;if(!Tags(q,flying,pumpkin))return false;if(!FusionSnowBall363PlantAffected(flying,pumpkin,Covered(q)))continue;
  if(!CanDisableNativePlant363(q))return false;refs.push_back(b->mPlants.DataArrayGetID(q));}
 const uint32_t uid=b->mProjectiles.DataArrayGetID(p);GetNativeProjectileData(p)->hitLatched=true;
 for(auto id:refs){if(app->mBoard!=b)return true;q=b->mPlants.DataArrayTryToGet(id);if(!Live(q))continue;
  if(!DisableNativePlant363(q))return false;ApplyNativeSourcePlantDamage(q,75,0);
  if(app->mBoard!=b)return true;q=b->mPlants.DataArrayTryToGet(id);if(Live(q))q->mEatenFlashCountdown=20;}
 if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
 Effect(b,24,hitX,hitY,p->mRow);Effect(b,97,x,y,row);LawnNet::NetPlaySourceSound284(app,uint16_t(RandRangeInt(104,105)),.5f);p->Die();return true;
}
bool HitZombie(Projectile* p,Zombie* z)
{
 if(!EnsureNativeSnowBall363Resources(p->mBoard))return false;int damage=75;FusionColdDamageContext cold;
 if(!PrepareNativeColdHit(p,z,damage,cold))return false;
 auto* b=p->mBoard;auto* app=p->mApp;const uint32_t uid=b->mProjectiles.DataArrayGetID(p),zid=b->mZombies.DataArrayGetID(z);
 GetNativeProjectileData(p)->hitLatched=true;const float x=p->mPosX+20,y=p->mPosY+20;const int row=p->mRow;
 z->TakeDamage(damage,0U,&cold);if(app->mBoard!=b)return true;p=b->mProjectiles.DataArrayTryToGet(uid);if(!p||p->mDead)return true;
 Effect(b,24,x,y,row);z=b->mZombies.DataArrayTryToGet(zid);p->PlayImpactSound(z);p->Die();return true;
}
}
bool EnsureNativeSnowBall363Resources(Board* b,std::string* error)
{
 if(!b||!IsNativeSourceWorld313(b)||!EnsureNativeProjectileVisuals(b,101,error)||!EnsureNativeSourceEffect269(b,24,error)||!EnsureNativeSourceEffect269(b,97,error))return false;
 for(int sound:{0,1,2,3,4,68,104,105})if(!LawnNet::NetSourceSound284Ready(b->mApp,uint16_t(sound)))return Refuse("SnowBall363 genuine source sound unavailable",error);return true;
}
bool EnsureNativeSnowGun363Resources(Board* b,std::string* error)
{return b&&IsNativeSourceWorld313(b)&&EnsureNativeSourceZombieVisuals(b,49,error)&&CanQueryNativeSourceZombieRoot273(b,49)&&FindFusionSnowGun363Graph(49)&&EnsureNativeSnowBall363Resources(b,error);}
bool InitializeNativeSnowGun363(Zombie* z)
{if(!Host(z))return false;auto* c=GetNativeContext(z->mBoard);if(!c)return false;auto& s=c->nextZombies343[z->mBoard->mZombies.DataArrayGetID(z)];s={};s.initialized=true;s.attributeCountdown=.5f;return true;}
bool NativeSnowGun363Shooting(const Zombie* z){const auto* s=State(z);return s&&s->initialized&&!s->failed&&s->snowShooting363;}
bool UpdateNativeSnowGun363(Zombie* z)
{
 if(!Host(z)||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
 auto* s=State(z);auto* graph=EditNativeSourceGraph313(z);const auto* d=GetNativeSourceZombieData(z);if(!s||!graph||!s->initialized||s->failed||!d)return false;
 if(s->attributeTick==z->mBoard->mMainCounter)return true;s->attributeTick=z->mBoard->mMainCounter;
 // Source SnowGun.Update invokes base Update/Attribute BEFORE refreshing its cached target.
 if(s->attributeCountdown>0){s->attributeCountdown-=.01f;if(s->attributeCountdown<=0){s->attributeCountdown=2;if(s->snowShooting363&&!d->beforeDying)graph->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Shoot);}}
 float shadowX=0,shadowY=0,groundX=0,groundY=0,sx=0,sy=0,minX=0,right=0,top=0;
 if(!GetNativeSourceZombie269NodePoint(z,"Shadow",shadowX,shadowY)||!NativeSourceGroundAnchor313(z,groundX,groundY)||!NativeBoardPointToSource313(z->mBoard,z->mRow,groundY,shadowX,shadowY,sx,sy)||!NativeSourceBounds313(z->mBoard,minX,right,top))return false;
 s->snowShooting363=false;
 if(z->mMindControlled||d->beforeDying||sx>=right-.6f)return true;
 float shootX=0,shootY=0;if(!GetNativeSourceZombie269NodePoint(z,muzzle,shootX,shootY))return false;
 for(int column=z->mBoard->GetColumnCount()-1;column>=0;--column){Plant* p=nullptr;while(z->mBoard->IteratePlants(p))
  if(Live(p)&&p->mRow==z->mRow&&p->mPlantCol==column&&GetNativePlantData(p)->kind!=1095){bool flying=false,pumpkin=false;float x=0,y=0;if(!Tags(p,flying,pumpkin)||!p->GetShadowCenter(x,y))return false;if(!flying&&x<shootX){s->snowShooting363=true;s->snowTargetX363=x;s->snowTargetY363=y-float(.5*GetNativeFusionPixelsY(z->mBoard));return true;}}}
 return true;
}
bool CanShootNativeSnowGun363(Zombie* z,std::string* error)
{
 if(!Host(z))return false;const auto* d=GetNativeSourceZombieData(z);if(d->beforeDying)return true;FusionHostilePlanarShot shot;
 return Shot(z,shot)&&z->mBoard->mProjectiles.mBlock&&z->mBoard->mProjectiles.mSize<z->mBoard->mProjectiles.mMaxSize&&EnsureNativeSnowBall363Resources(z->mBoard,error);
}
bool ShootNativeSnowGun363(Zombie* z)
{
 if(!CanShootNativeSnowGun363(z))return false;if(GetNativeSourceZombieData(z)->beforeDying)return true;FusionHostilePlanarShot shot;if(!Shot(z,shot))return false;
 float gx=0,gy=0;if(!NativeSourceGroundAnchor313(z,gx,gy))return false;
 auto* p=z->mBoard->AddProjectile(int(shot.x-20),int(shot.y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,z->mRow,0),z->mRow,PROJECTILE_PEA);if(!p)return false;
 NativeProjectileData d;d.kind=101;d.damage=75;d.specialType=17;d.isZombieBullet=!z->mMindControlled;p->mMotionType=MOTION_LOBBED;
 if(!AttachNativeProjectile(z->mBoard,p,d)){p->Die();return false;}p->mPosX=float(shot.x-20);p->mPosY=float(shot.y-20);p->mPosZ=0;p->mVelX=float(shot.velocityX);p->mVelY=p->mVelZ=0;p->mAccZ=float(shot.gravityY);p->mShadowY=gy;p->mRotation=p->mRotationSpeed=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(RandRangeInt(3,4)),.5f);return LawnNet::NetPlaySourceSound284(z->mApp,68,.5f);
}
bool IsNativeSnowBall363(const Projectile* p)
{const auto* d=GetNativeProjectileData(p);return d&&FusionSnowBall363Metadata(d->kind,d->damage,d->specialType,p->mProjectileType,p->mMotionType)&&NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);}
bool UpdateNativeSnowBall363(Projectile* p)
{
 if(!IsNativeSnowBall363(p))return false;auto* b=p->mBoard;if(p->mDead||p->mNetGhost||!b||b->mApp->NetIsClient()||!IsNativeSourceWorld313(b))return true;
 auto* d=GetNativeProjectileData(p);if(d->hitLatched)return true;
 FusionHostilePlanarShot shot{p->mPosX+20.,p->mPosY+20.,p->mVelX,p->mVelY,p->mAccZ};if(!AdvanceFusionHostilePlanarShot(shot)){p->Die();return true;}
 p->mPosX=float(shot.x-20);p->mPosY=float(shot.y-20);p->mVelX=float(shot.velocityX);p->mVelY=float(shot.velocityY);p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 Plant* plant=nullptr;if(d->isZombieBullet)while(b->IteratePlants(plant))if(Live(plant)&&plant->mRow==p->mRow&&NativeSourcePlantColliderEnabled(plant)&&Touches(p,plant->GetPlantRect())){if(!HitPlant(p,plant))LawnNet::NetTrace("SnowBall363 plant commit refused id=%u target=%u",p->mNetId,plant->mNetId);return true;}
 Zombie* z=nullptr;while(b->IterateZombies(z))
 {NativeZombie313Semantics sem;const int status=GetNativeFusionZombieStatus(z);if(z->mDead||GetNativeFusionZombieHealth(z)<=0||z->mMindControlled!=d->isZombieBullet||z->mRow!=p->mRow||NativeWater268ZombieCaptured(z)||!FusionStar269CanHitStatus(17,status)||(GetNativeZombie313Semantics(z,sem)&&!sem.colliderEnabled)||!Touches(p,z->GetZombieRect()))continue;
  if(!HitZombie(p,z))LawnNet::NetTrace("SnowBall363 zombie commit refused id=%u target=%u",p->mNetId,z->mNetId);return true;}
 if(shot.velocityY>0&&shot.y>p->mShadowY-.5*GetNativeFusionPixelsY(b))
 {if(!EnsureNativeSnowBall363Resources(b))return true;Effect(b,24,float(shot.x),float(shot.y),p->mRow);LawnNet::NetPlaySourceSound284(b->mApp,uint16_t(RandRangeInt(0,2)),.5f);p->Die();}
 return true;
}
}
