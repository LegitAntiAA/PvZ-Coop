#include "FusionNativeGroundShooter343.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeNextZombie343State.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeGarlic.h"
#include "FusionNative.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion {namespace {
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
uint32_t UID(Zombie* z){return z->mBoard->mZombies.DataArrayGetID(z);}
bool Host(const Zombie* z){return z&&FusionGroundShooter343Kind(Kind(z))&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard);}
bool Reject(const char* why,std::string* error){if(error)*error=why;return false;}
const char* Muzzle(uint16_t kind)
{return kind==114?"Shoot":kind==118?"Zombie_football_head/HeadPos/GatlingPea_head/paokou3_mid/Shoot":"Zombie_balloon_head/Zombie_head/Shoot";}
bool Plan(Zombie* z,FusionGround343Shot& plan,float& x,float& y)
{
 auto* d=GetNativeSourceZombieData(z);const auto* s=GetNativeExtendedZombie313(z);if(!d||!s)return false;
 const auto* row=GetNativeZombieGarlicData(z);bool active=false;
 if(!GetNativeSourceZombie269NodeActivity(z,Muzzle(d->kind),active))return false;
 if(!TryPlanFusionGround343Shot(d->kind,s->sourceStatus,d->beforeDying,row&&row->phase==GarlicActionPhase::Moving,s->rootFlip,z->mMindControlled,active,plan))return false;
 return !plan.emit||GetNativeSourceZombie269NodePoint(z,Muzzle(d->kind),x,y);
}
NativeProjectileData Metadata(const FusionGround343Shot& plan)
{NativeProjectileData d;d.kind=plan.bullet;d.damage=plan.damage;d.specialType=plan.way;d.isZombieBullet=plan.hostile;return d;}
void Fail(Zombie* z,const char* reason)
{if(auto* s=EditNativeExtendedZombie313(z))s->failed=true;LawnNet::NetTrace("Fusion GroundShoot343 stopped kind=%u uid=%u reason=%s",unsigned(Kind(z)),UID(z),reason);}
}
bool EnsureNativeGroundShooter343Resources(Board* b,uint16_t kind,std::string* error)
{
 if(!b||!FusionGroundShooter343Kind(kind)||!IsNativeSourceWorld313(b))return false;
 if(!EnsureNativeSourceZombieVisuals(b,kind,error)||!CanQueryNativeSourceZombieRoot273(b,kind)||!FindFusionGround343Graph(kind)||
    !EnsureNativeProjectileVisuals(b,kind==118?0:11)||!LawnNet::NetSourceSound284Ready(b->mApp,3)||!LawnNet::NetSourceSound284Ready(b->mApp,4))return Reject("Ground343 actual model/graph/projectile/sound unavailable",error);
 if(kind==119&&(!LawnNet::NetSourceSound284Ready(b->mApp,77)||!LawnNet::NetSourceSound284Ready(b->mApp,86)))return false;
 return true;
}
bool InitializeNativeGroundShooter343(Zombie* z)
{
 if(!Host(z))return false;auto* c=GetNativeContext(z->mBoard);if(!c)return false;float t=0;if(!TryGetFusionGround343Attribute(Kind(z),true,t))return false;
 auto& s=c->nextZombies343[UID(z)];s={};s.initialized=true;s.attributeCountdown=t;return true;
}
bool UpdateNativeGroundShooter343Attribute(Zombie* z)
{
 if(!Host(z)||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;auto* c=GetNativeContext(z->mBoard);auto* graph=EditNativeSourceGraph313(z);const auto* d=GetNativeSourceZombieData(z);
 if(!c||!graph||!d)return false;auto it=c->nextZombies343.find(UID(z));if(it==c->nextZombies343.end()||!it->second.initialized||it->second.failed)return false;auto& s=it->second;
 if(s.attributeTick==z->mBoard->mMainCounter)return true;s.attributeTick=z->mBoard->mMainCounter;
 if(s.attributeCountdown>0){s.attributeCountdown-=.01f;if(s.attributeCountdown<=0){s.attributeCountdown=0;
   // Source118 leaves its expired countdown at zero once beforeDying. Its
   // already-running outgoing shoot callbacks remain independently valid.
   if(Kind(z)!=118||!d->beforeDying){if(!TryGetFusionGround343Attribute(Kind(z),false,s.attributeCountdown))return false;graph->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Shoot);}}}
 return true;
}
bool CanShootNativeGround343(Zombie* z,std::string* error)
{
 if(!Host(z))return false;FusionGround343Shot plan;float x=0,y=0;if(!Plan(z,plan,x,y))return Reject("Ground343 actual muzzle/phase cannot be sampled",error);if(!plan.emit)return true;
 const auto d=Metadata(plan);const auto motion=plan.way==9?MOTION_BACKWARDS:MOTION_STRAIGHT;
 if(!std::isfinite(x+y)||!NativeProjectileMetadataValid(d,PROJECTILE_PEA,motion)||!z->mBoard->mProjectiles.mBlock||z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize)return Reject("Ground343 exact projectile tuple/oneP unavailable",error);
 return EnsureNativeProjectileVisuals(z->mBoard,d.kind)&&LawnNet::NetSourceSound284Ready(z->mApp,3)&&LawnNet::NetSourceSound284Ready(z->mApp,4);
}
bool ShootNativeGround343(Zombie* z)
{
 if(!CanShootNativeGround343(z))return false;FusionGround343Shot plan;float x=0,y=0;if(!Plan(z,plan,x,y))return false;if(!plan.emit)return true;
 x+=plan.sourceOffsetX*float(kFusionBasePixelsX);auto* p=z->mBoard->AddProjectile(int(x-20),int(y-20),z->mRenderOrder-1,z->mRow,PROJECTILE_PEA);if(!p)return false;
 const auto d=Metadata(plan);p->mMotionType=plan.way==9?MOTION_BACKWARDS:MOTION_STRAIGHT;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY=y;
 p->mVelX=float((plan.way==9?-1:1)*.06*kFusionBasePixelsX);p->mVelY=p->mVelZ=p->mAccZ=0;p->mRotation=p->mRotationSpeed=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 if(!AttachNativeProjectile(z->mBoard,p,d)){p->Die();return false;}return LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(RandRangeInt(3,4)),.5f);
}
bool ApplyNativeGround343FirstArmor(Zombie* z,int damage,int& remaining)
{
 if(!Host(z)||(Kind(z)!=114&&Kind(z)!=118)||damage<0)return false;auto* s=EditNativeExtendedZombie313(z);if(!s)return false;
 const int hp=z->mHelmHealth;if(hp<=0){remaining=damage;return true;}
 if(damage>=hp&&Kind(z)==114&&!PlayNativeSourceZombieArmor284(z,true)){Fail(z,"Prepared source114 LoseBucket refused");remaining=0;return true;}
 if(damage>=hp){remaining=damage-hp;z->mHelmHealth=0;z->mHelmType=HELMTYPE_NONE;if(Kind(z)==118)z->mHelmMaxHealth=0;}
 else{z->mHelmHealth=hp-damage;s->firstArmorStage=FusionGround343ArmorStage(Kind(z),z->mHelmHealth,z->mHelmMaxHealth,s->firstArmorStage);remaining=0;}
 return true;
}
bool ApplyNativeGround343SecondArmor(Zombie* z,int damage,int& remaining)
{
 if(!Host(z)||Kind(z)!=114||damage<0)return false;auto* s=EditNativeExtendedZombie313(z);auto* graph=EditNativeSourceGraph313(z);if(!s||!graph)return false;
 const int hp=z->mShieldHealth;if(hp<=0){remaining=damage;return true;}
 if(damage>=hp){if(!PlayNativeSourceZombieArmor284(z,false)){Fail(z,"Prepared source114 LoseDoor refused");remaining=0;return true;}
  z->mShieldHealth=0;z->mShieldType=SHIELDTYPE_NONE;z->mHasObject=false;remaining=damage-hp;
  graph->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::LosePaper);}
 else{z->mShieldHealth=hp-damage;s->secondArmorStage343=FusionGround343ArmorStage(114,z->mShieldHealth,z->mShieldMaxHealth,s->secondArmorStage343);remaining=0;}
 return true;
}
}
