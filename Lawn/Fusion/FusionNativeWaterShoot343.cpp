#include "FusionNativeWaterShoot343.h"
#include "FusionNativeNextZombie343State.h"
#include "FusionNativeSourceGraph313.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeWaterZombie313Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
#include "FusionNative.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace LawnFusion { namespace {
constexpr uint8_t Mask=4;
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
uint32_t UID(const Zombie* z){return z&&z->mBoard?z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z)):0;}
bool Typed(const Zombie* z){return z&&FusionWaterShooter343Kind(Kind(z))&&IsNativeSourceWorld313(z->mBoard);}
bool Host(const Zombie* z){return Typed(z)&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient();}
bool Reject(const char* why,std::string* error){if(error)*error=why;return false;}
NativeSourceGraph313State* State(Zombie* z)
{auto* c=GetNativeContext(z?z->mBoard:nullptr);if(!c||!Typed(z))return nullptr;auto it=c->sourceGraphs313.find(UID(z));return it==c->sourceGraphs313.end()?nullptr:&it->second;}
Reanimation* Clock(Zombie* z,uint8_t layer)
{const auto* s=GetNativeWaterZombie313State(z);return layer==2&&s&&s->shootClockUID343?z->mApp->ReanimationTryToGet(ReanimationID(s->shootClockUID343)):nullptr;}
void Failed(Board* b,const char* why)
{if(auto* c=GetNativeContext(b)){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}LawnNet::NetTrace("Fusion WaterShoot343 stopped reason=%s",why);}
bool Params(Zombie* z,FusionSourceGraph313Parameters& p)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* garlic=GetNativeZombieGarlicData(z);float factor=0;
 if(!d||!TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,
    garlic&&FusionGarlicStopsWalking(*garlic),factor))return false;
 p.speed=d->originSpeed*factor*GetNativeWater268GrapSpeedFactor(z);return std::isfinite(p.speed)&&p.speed>=0;
}
bool Allowed(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);const auto* s=GetNativeWaterZombie313State(z);const auto* garlic=GetNativeZombieGarlicData(z);
 return d&&s&&s->sourceStatus!=1&&s->sourceStatus!=9&&!d->beforeDying&&
    !(garlic&&garlic->phase==GarlicActionPhase::Moving);}
NativeProjectileData Shot(const Zombie* z)
{NativeProjectileData p;const auto* s=GetNativeWaterZombie313State(z);p.kind=Kind(z)==214?3:0;p.damage=Kind(z)==214?300:20;
 p.specialType=s&&s->facingRight?0:9;p.isZombieBullet=!z->mMindControlled;return p;}
bool ShotReady(Zombie* z,std::string* error)
{
 if(!Allowed(z))return true;float x=0,y=0;const auto d=Shot(z);const auto motion=d.specialType==9?MOTION_BACKWARDS:MOTION_STRAIGHT;
 if(!GetNativeSourceZombie269NodePoint(z,"Shoot",x,y)||!std::isfinite(x+y)||
    !NativeProjectileMetadataValid(d,PROJECTILE_PEA,motion)||!EnsureNativeProjectileVisuals(z->mBoard,d.kind)||
    !z->mBoard->mProjectiles.mBlock||z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize)
    return Reject("WaterShoot343 genuine muzzle/projectile tuple/oneP unavailable",error);
 return LawnNet::NetSourceSound284Ready(z->mApp,3)&&LawnNet::NetSourceSound284Ready(z->mApp,4);
}
bool Shoot(Zombie* z)
{
 if(!Allowed(z))return true;if(!ShotReady(z,nullptr))return false;float x=0,y=0;
 if(!GetNativeSourceZombie269NodePoint(z,"Shoot",x,y))return false;
 const auto d=Shot(z);auto* p=z->mBoard->AddProjectile(int(x-20),int(y-20),z->mRenderOrder-1,z->mRow,PROJECTILE_PEA);if(!p)return false;
 p->mMotionType=d.specialType==9?MOTION_BACKWARDS:MOTION_STRAIGHT;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mShadowY=y;
 p->mVelX=float((d.specialType==9?-1:1)*.06*kFusionBasePixelsX);p->mVelY=p->mVelZ=p->mAccZ=0;
 p->mRotation=p->mRotationSpeed=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 if(!AttachNativeProjectile(z->mBoard,p,d)){p->Die();return false;}
 return LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(RandRangeInt(3,4)),.5f);
}
const NativeSourceGraph313Hooks& Hooks()
{
 static const NativeSourceGraph313Hooks h{State,Clock,Params,
 [](Zombie* z,FusionSourceGraph313Event e,std::string* error){return e==FusionSourceGraph313Event::Shoot?ShotReady(z,error):false;},
 [](Zombie* z,FusionSourceGraph313Event e,int32_t){return e==FusionSourceGraph313Event::Shoot&&Shoot(z);},
 [](Zombie*,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double& out){out=0;return true;},
 [](Zombie*,double x){return x==0;},[](Zombie*,uint8_t,uint8_t){return false;},Failed};return h;
}
bool Allocate(Zombie* z,NativeWaterZombie313State& s)
{
 if(s.shootClockUID343)return Clock(z,2)!=nullptr;
 auto* a=z->mApp->AddReanimation(0,0,0,REANIM_ZOMBIE);if(!a)return false;
 s.shootClockUID343=uint32_t(z->mApp->ReanimationGetID(a));a->PlayReanim("anim_head1",REANIM_PLAY_ONCE_AND_HOLD,0,0);
 for(int i=0;i<a->mDefinition->mTracks.count;++i)a->mTrackInstances[i].mForceDontRender=true;
 return true; // Un-attached actual R, not a fake actor/head attachment.
}
void Copy(Reanimation* r,const LawnNet::NetZombieAnimState& v)
{r->mFrameStart=v.mFrameStart;r->mFrameCount=v.mFrameCount;r->mAnimTime=r->mLastFrameTime=v.mAnimTime;r->mAnimRate=v.mAnimRate;r->mLoopType=ReanimLoopType(v.mLoopType);r->mLoopCount=v.mLoopCount;}
}
bool EnsureNativeWaterShoot343Resources(Board* b,uint16_t kind,std::string* error)
{
 if(!FusionWaterShooter343Kind(kind)||!IsNativeSourceWorld313(b))return false;const auto* g=FindFusionSourceGraph313(kind);
 if(!g||g->layerCount!=3||!g->layers[2].additive)return Reject("WaterShoot343 actualthirdadditive sourcegraph unavailable",error);
 ReanimatorEnsureDefinitionLoaded(REANIM_ZOMBIE,false);if(!gReanimatorDefArray||unsigned(REANIM_ZOMBIE)>=gReanimatorDefCount)return false;
 const auto& tracks=gReanimatorDefArray[int(REANIM_ZOMBIE)].mTracks;if(!tracks.tracks||tracks.count<=0)return false;bool head=false;
 for(int i=0;i<tracks.count;++i){if(tracks.tracks[i].mName&&!_strnicmp(tracks.tracks[i].mName,"attacher__",10))return false;
  if(tracks.tracks[i].mName&&!_stricmp(tracks.tracks[i].mName,"anim_head1")&&tracks.tracks[i].mTransforms.count>0)head=true;}
 return head&&EnsureNativeProjectileVisuals(b,kind==214?3:0)&&LawnNet::NetSourceSound284Ready(b->mApp,3)&&LawnNet::NetSourceSound284Ready(b->mApp,4);
}
bool InitializeNativeWaterShoot343(Zombie* z,NativeWaterZombie313State& s)
{
 if(!FusionWaterShooter343Kind(Kind(z)))return true;if(!Host(z)||!EnsureNativeWaterShoot343Resources(z->mBoard,Kind(z))||!Allocate(z,s))return false;
 auto* c=GetNativeContext(z->mBoard);if(!c)return false;auto& g=c->sourceGraphs313[UID(z)];g.auxiliaryUIDs[1]=s.shootClockUID343;
 auto& next=c->nextZombies343[UID(z)];next={};next.initialized=true;
 next.attributeCountdown=Kind(z)==113?.75f:z->mMindControlled?.5f:3.f;
 s.gatlingReady343=Kind(z)==214&&z->mMindControlled;
 if(!InitializeNativeSourceGraph313Masked(z,g,Hooks(),Mask))return false;s.shoot343=g.layers[2];return true;
}
bool UpdateNativeWaterShoot343(Zombie* z,NativeWaterZombie313State& s)
{
 if(!FusionWaterShooter343Kind(Kind(z)))return true;if(!Host(z)||z->mBoard->mPaused)return true;
 auto* c=GetNativeContext(z->mBoard);auto* g=State(z);if(!c||!g)return false;auto it=c->nextZombies343.find(UID(z));
 if(it==c->nextZombies343.end()||!it->second.initialized||it->second.failed)return false;auto& ai=it->second;
 if(ai.attributeTick!=z->mBoard->mMainCounter)
 {ai.attributeTick=z->mBoard->mMainCounter;
  if(ai.attributeCountdown>0){ai.attributeCountdown-=.01f;if(ai.attributeCountdown<=0){ai.attributeCountdown=Kind(z)==113?.75f:1.5f;
   if(Kind(z)==214)s.gatlingReady343=true;
   if(s.sourceStatus==0||s.sourceStatus==8)g->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Shoot);}}}
 if(!UpdateNativeSourceGraph313Masked(z,*g,Hooks(),Mask)){ai.failed=true;return false;}s.shoot343=g->layers[2];return true;
}
bool CaptureNativeWaterShoot343(const Zombie* z,const NativeWaterZombie313State& s,FusionWaterZombie313View& v)
{
 if(!FusionWaterShooter343Kind(Kind(z)))return true;const auto* g=State(const_cast<Zombie*>(z));auto* r=Clock(const_cast<Zombie*>(z),2);
 if(!g||!r||r->mDead||!g->initialized||g->failed||s.localUID!=UID(z)||r->mFrameStart<0||r->mFrameStart>65535||r->mFrameCount<=0||r->mFrameCount>65535)return false;
 v.shoot343=g->layers[2];auto& a=v.shootClock343;a.mPresent=true;a.mFrameStart=uint16_t(r->mFrameStart);a.mFrameCount=uint16_t(r->mFrameCount);
 a.mAnimTime=r->mAnimTime;a.mAnimRate=r->mAnimRate;a.mLoopType=uint8_t(r->mLoopType);a.mLoopCount=uint8_t((std::clamp)(r->mLoopCount,0,255));
 return FusionWaterZombie313WireViewValid(Kind(z),v);
}
bool AdoptNativeWaterShoot343(Zombie* z,NativeWaterZombie313State& s,const FusionWaterZombie313View& view)
{
 if(!FusionWaterShooter343Kind(Kind(z)))return true;if((!z->mNetGhost&&!z->mApp->NetIsClient())||!FusionWaterZombie313WireViewValid(Kind(z),view)||!Allocate(z,s))return false;
 auto* c=GetNativeContext(z->mBoard);auto* a=z->mApp->ReanimationTryToGet(ReanimationID(s.shootClockUID343));if(!c||!a)return false;
 Copy(a,view.shootClock343);if(!RegisterNativeSourceGraphClock313(a,true))return false;
 NativeSourceGraph313State g;g.initialized=true;g.initializedLayerMask343=Mask;g.localUID=UID(z);g.layers[2]=view.shoot343;g.auxiliaryUIDs[1]=s.shootClockUID343;c->sourceGraphs313[UID(z)]=std::move(g);return true;
}
void RemoveNativeWaterShoot343(Zombie* z,NativeWaterZombie313State& s)
{
 if(!z||!FusionWaterShooter343Kind(Kind(z)))return;ReleaseNativeSourceGraph313ClocksMasked(z,Hooks(),Mask);
 if(s.shootClockUID343)z->mApp->RemoveReanimation(ReanimationID(s.shootClockUID343));s.shootClockUID343=0;
 if(auto* c=GetNativeContext(z->mBoard)){c->sourceGraphs313.erase(UID(z));c->nextZombies343.erase(UID(z));}
}
bool GetNativeWaterShoot343Presentation(const Zombie* z,const char*& incoming,float& seconds,const char*& outgoing,float& oldSeconds,float& weight)
{const auto* g=State(const_cast<Zombie*>(z));return Typed(z)&&g&&GetNativeSourceGraph313Presentation(const_cast<Zombie*>(z),*g,2,Hooks(),incoming,seconds,outgoing,oldSeconds,weight);}
}
